/*

Solution for https://github.com/Ecwid/new-job/blob/master/IP-Addr-Counter.md

 */

import java.io.RandomAccessFile
import java.lang.Long.max
import java.nio.ByteBuffer
import java.nio.IntBuffer
import java.util.concurrent.ForkJoinPool
import kotlin.system.exitProcess

// the data is processed using the BLOCK_COUNT amount of threads, using BLOCK_COUNT*2 allocations by BLOCK_SIZE bytes
var BLOCK_COUNT = 64;
var BLOCK_SIZE = 4096 * 256;
var fileName = "";
var bForceGC = false;

/** 32-bit IP range as a bitmap. */
const val MAP_TOTAL_SIZE = 0x100000000 / 8;

/** Bitmap is allocated by MAP_PAGE_KEY_SIZE pages. */
const val MAP_PAGE_KEY_SIZE = 0x10000;

/**
 * The bitmap with the non-zero bit counter
 */
class CBitMap {

    private var bitMap = mutableMapOf<UInt, ByteBuffer>();
    private var counter: Int = 0;
    private val bitMasks: IntArray = intArrayOf(0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80);

    /**
     * Sets the bit specified, allocating the corresponding map page if needed.
     * - In read-only mode (bMutable=false):
     * Checks the bit specified without modifying any data structures.
     * Returns true if the bit is already set (data should not be modified) and false if bit is not set yet.
     * - In writeable mode (bMutable=true):
     * Does the page allocation and a bit setting if needed.
     * Returns true.
    */
    private fun doSet(n: UInt, bMutable: Boolean): Boolean {
        val mapPageKey: UInt = n.shr(16);
        if (!bitMap.containsKey(mapPageKey)) {
            if ( bMutable ) {
                bitMap[mapPageKey] = ByteBuffer.allocate(MAP_TOTAL_SIZE / MAP_PAGE_KEY_SIZE);
                // need to write and can write
                // ignored
            } else {
                return false; // need to write but can't
            }
        }
        val bitMapPage = bitMap[mapPageKey]!!;
        val pagedOffset = n.and(0xFFFFu);
        val byteOffset: Int = (pagedOffset / 8u).toInt();
        val bitMask: Int = bitMasks[(pagedOffset % 8u).toInt()];
        val byteVal: Byte = bitMapPage[byteOffset];
        return if ((byteVal.toInt() and bitMask) == 0) {
            if ( bMutable ) {
                counter++;
                bitMapPage.put(byteOffset, (byteVal.toInt() or bitMask).toByte());
                true; // need to write and can write
            } else {
                false; // need to write but can't
            }
        } else {
            true; // needn't write
        }
    }

    fun isSet(n: UInt): Boolean {
        return doSet(n, false);
    }

    fun set(n: UInt): Boolean {
        return doSet(n, true);
    }

    fun getCount(): Int {
        return counter;
    }
}

/**
 * A basic structure for data processing.
 */
class CDataBlock {

    /** Main flat memory buffer and its BYTE view. */
    val stringBuffer: ByteBuffer = ByteBuffer.allocate(BLOCK_SIZE);

    /** DWORD view over the memory buffer. */
    val uintBuffer: IntBuffer = stringBuffer.asIntBuffer();

    /** Size of data stored after reading from disk. */
    var stringBufferSize = 0;

    /** Count of DWORDs stored after string parsing. */
    var uintBufferSize = 0;

    /** Index of the first DWORD to be processed yet. */
    var uintBufferStart = 0;

    /** The count of strings processed via this block to be used for progress and final output. */
    var processedCount = 0L;
}

/** The main working mechanism */
class CBlockProcessor(file_: RandomAccessFile) {

    // Basic effectiveness profiling variables
    private val startTime: Long = System.currentTimeMillis();
    private var sequentialTimeStart = System.currentTimeMillis();
    private var sequentialTimeAmount: Long = 0;
    private var parallelTimeStart: Long = 0;
    private var parallelTimeAmount: Long = 0;
    private var ioTimeStart: Long = 0;
    private var ioTimeAmount: Long = 0;

    private val file: RandomAccessFile = file_;
    private val totalSize: Long = file.channel.size();

    /** Thread pool to process file i/o, bitmap modification and BLOCK_COUNT blocks concurrently. */
    private val customThreadPool = ForkJoinPool(BLOCK_COUNT + 2);

    /**
     * Two sets (Lists) of blocks.
     * Every single processing pass one block set is used to read new data from file and another block set is used to process the data read in the previous pass.
     * Next pass blocks are switched - the previously read set is processed and the previously processed set is overwritten by the new data from file.
     */
    private val blocks = mapOf(
        false to List(BLOCK_COUNT) { CDataBlock() },
        true to List(BLOCK_COUNT) { CDataBlock() }
    );

    /** The switch/indicator of block usage scenario in the current processing pass. */
    private var bTickTock = false;

    /** IP uniqueness data storage. */
    private val bitMap = CBitMap();

    /** Converts raw character bytes (strings) read from file to IP DWORDs. */
    private fun parseBlock(block: CDataBlock) {

        //println( "+ start parsing" );

        with(block) {

            /** Current character parsing position. */
            var bufferOffset = 0;
            /** Current string parsing start position. */
            var stringStartOffset = 0;
            /** Accumulated DWORD-sized IP representation. */
            var dwordValue = 0u;
            /** Accumulated byte-sized IP octet representation */
            var byteValue = 0u;
            /** True if some data is accumulated. */
            var bParsing = false;
            /** True if data accumulated is valid. */
            var bValid = true;

            /** Store byte value after '.' or EOL to the DWORD */
            fun finalizeByte() {
                dwordValue = dwordValue.shl(8);
                dwordValue += byteValue;
                byteValue = 0u;
            }

            /** Get the string currently parsed for invalid data reporting. */
            fun getCurrentString(): String {
                var s = "";
                for (i in stringStartOffset until bufferOffset) {
                    s += Char(stringBuffer[i].toInt()).toString();
                }
                return s.trim();
            }

            /** Store DWORD value in the buffer after EOL or end of block. */
            fun finalize() {

                if (bParsing) { // have new data

                    finalizeByte();
                    processedCount++;

                    if ( bValid ) { // storing invalid IPs only

                        // To minimize memory usage, the same memory buffer is used for input data (string representation of IPs)
                        // and for the output data (DWORD representation of IPs).
                        //
                        // We expect that the minimal valid IP string length is 7 bytes (i.e. '8.8.8.8') plus 1 or 2 bytes EOL,
                        // so for every valid input we have enough bytes to store 4 bytes of output.
                        // The first 4 bytes of the first string parsed are overwritten by the resulting DWORD right after string is parsed.
                        //
                        // To keep the program stable when encountering invalid input (short string) at the start of the block,
                        // we are skipping strings less than 4 bytes/characters long (with EOL counted as the string part).
                        // To simplify algorithm, the skipping is done regardless the real threat of unparsed string overrun.

                        if (bufferOffset - stringStartOffset < 4) {

                            // report that the string is too short
                            println("'${getCurrentString()}' - line is too short");

                        } else {

                            // store DWORD if string is long enough
                            uintBuffer.put(uintBufferSize, dwordValue.toInt());
                            uintBufferSize++;

                        }

                    } else { // bValid
                        println("'${getCurrentString()}' is not an IPv4 address");
                    }

                    dwordValue = 0u;
                    byteValue = 0u;
                    bParsing = false;
                    stringStartOffset = bufferOffset;
                }

                bValid = true;

            }

            // reset buffer position after file reading (just in case it is not done before)
            stringBuffer.rewind();

            while (bufferOffset < stringBufferSize) {
                when (val c = stringBuffer[bufferOffset].toUInt()) {
                    0x2Eu -> { // '.'
                        bParsing = true;
                        finalizeByte();
                    }
                    in 0x30u..0x39u -> { // 0-9
                        bParsing = true;
                        byteValue *= 10u;
                        byteValue += (c - 0x30u);
                    }
                    0x0Au, 0x0Du -> { // EOL chars
                        finalize();
                    }
                    0x09u, 0x20u, 0xA0u -> { // space chars
                        // ignore
                    }
                    else -> { // invalid character
                        bParsing = true;
                        bValid = false;
                    }
                }
                bufferOffset++;
            }

            stringBufferSize = 0;

            finalize();
        }

        //println( "- end parsing" );
    }

    /** Read data from the file to the set of blocks specified. */
    private fun readBlocks(blocks: List<CDataBlock>) {

        //println( "++ start reading" );
        ioTimeStart = System.currentTimeMillis();
        var bytesRead = 0;
        var currentBlockIndex = 0;

        while (

            // Read the block of fixed length (or less on EOF) to every buffer.
            currentBlockIndex < blocks.size &&
            run {
                with(blocks[currentBlockIndex]) {
                    stringBuffer.rewind();
                    bytesRead = file.channel.read(stringBuffer);
                    bytesRead
                }
            } > 0

        ) {

            // Since the line length is variable, it is most probable that the end of the block splits some line in two.
            // To keep block parsing self-sufficient, we should yield the non-complete line part to the next block.
            with(blocks[currentBlockIndex]) {

                // Set full block length.
                stringBufferSize = bytesRead;

                // If not EOF, decreasing block length until the last character is EOL or the block is empty.
                while (bytesRead == BLOCK_SIZE && stringBufferSize > 0 && run {
                        val c = stringBuffer[stringBufferSize - 1].toInt(); c != 0x0A && c != 0x0D
                    }) {
                    stringBufferSize--;
                }

                // Yield skipped characters to the next read operation.
                file.channel.position(file.channel.position() - bytesRead + stringBufferSize);
            }

            currentBlockIndex++;
        }

        ioTimeAmount += System.currentTimeMillis() - ioTimeStart;
        //println( "-- end reading" );
    }

    /**
     * The main multithreaded processing pass.
     * Returns false if there is no new data read from file (nothing left to process).
     */
    fun processBlocks(): Boolean {

        // Setting block purposes for the current pass.
        val blocksToParse = blocks[bTickTock]!!;
        val blocksToRead = blocks[!bTickTock]!!;
        // Inverting block purposes for the next pass.
        bTickTock = !bTickTock;

        // Start reading data from the file ASAP (it is the usual bottleneck).
        val fileReadTask = customThreadPool.submit {
            readBlocks(blocksToRead);
        };

        sequentialTimeAmount += System.currentTimeMillis() - sequentialTimeStart;
        parallelTimeStart = System.currentTimeMillis();

        // Start read-only MT processing of data.
        val parseAndReadMapTask = customThreadPool.submit {
            blocksToParse.parallelStream().parallel().forEach {

                // Convert IP representation from string to DWORD
                parseBlock(it);

                // Check IPs for uniqueness until the first unique item is found.
                // For the unique item the bitmap must be changed, and we shouldn't do this in MT mode
                with(it){
                        while (uintBufferStart < uintBufferSize) {
                            if ( !bitMap.isSet(uintBuffer[uintBufferStart].toUInt()) ) {
                                // should apply this block as new-bit-setting from the current pos
                                break;
                            }
                            uintBufferStart++;
                        }
                }
            }
        };
        // Wait for MT processing to end.
        parseAndReadMapTask.join();

        parallelTimeAmount += System.currentTimeMillis() - parallelTimeStart;
        sequentialTimeStart = System.currentTimeMillis();

        // Start map-modifying single-threaded processing of data.
        val writeMapTask = customThreadPool.submit {
            blocksToParse.forEach {
                with(it) {
                    while (uintBufferStart < uintBufferSize) {
                        bitMap.set(uintBuffer[uintBufferStart].toUInt());
                        uintBufferStart++;
                    }
                    uintBufferStart = 0;
                    uintBufferSize = 0;
                }
            }
        };
        // Wait for processing to end.
        writeMapTask.join();

        // Wait for the file read completion.
        fileReadTask.join();

        // If there are no new data in the first buffer, then all data are processed.
        return blocksToRead[0].stringBufferSize > 0;
    }

    /** Returns the count of lines processed. */
    fun getProcessedCount(): Long {
        return blocks[false]?.sumOf { it.processedCount }!! + blocks[true]?.sumOf { it.processedCount }!!;
    }

    /** Returns the count of unique IPs encountered. */
    fun getUniqueCount(): Int {
        return bitMap.getCount();
    }

    /** Returns the total wall clock time spent processing data. */
    fun getTotalTime(): Long {
        return System.currentTimeMillis() - startTime;
    }

    /** Displays current processing stats. */
    fun showStatus() {

        val processedCount = getProcessedCount();

        print(processedCount);
        print(" lines (");
        val pos = file.channel.position() * 100.0 / totalSize;
        print(pos.toInt());
        print(".");
        print(((pos % 1) * 10).toInt());
        print("%), ");
        print(bitMap.getCount());
        print(" uniq, ");
        val timePassed = getTotalTime();
        if (timePassed > 0) {
            if (timePassed > 1000) {
                print(processedCount / (timePassed / 1000) / 1000);
                print("K lps, ");
                print(file.channel.position() / (timePassed / 1000) / (1024 * 1024));
                print(" MBps, ");
            }
            print("seq ops ");
            print(sequentialTimeAmount / 1000);
            print("s (");
            print("%.0f".format(sequentialTimeAmount.toDouble() / timePassed.toDouble() * 100));
            print("%), par ops ");
            print(parallelTimeAmount / 1000);
            print("s (");
            print("%.0f".format(parallelTimeAmount.toDouble() / timePassed.toDouble() * 100));
            print("%), i/o ");
            print(ioTimeAmount / 1000);
            print("s (");
            print("%.0f".format(ioTimeAmount.toDouble() / timePassed.toDouble() * 100));
            print("%), ");
            print(Runtime.getRuntime().totalMemory() / ( 1024 * 1024));
            print("MB RAM");
        }
        println();
    }

}

/**
 * Command line parser.
 */
fun parseCommandLine(args: Array<String>): Boolean {

    fun printUsage() {
        println(
            """
            Usage:
            <program_name> --file=<file name> [--fast|--compact|--slow] [--block_size=<size in bytes>] [--block_count=<count>] [--force_gc]
        """.trimIndent()
        )
    }

    val commandLineSplitter = Regex("--([a-z_]+)(?:\\s*=\\s*([^\\s|$]*))?");
    val argsMap = mutableMapOf<String, String>();
    commandLineSplitter.findAll(args.joinToString(" ")).forEach { matchResult ->
        argsMap[matchResult.groupValues[1]] = matchResult.groupValues[2];
    }

    argsMap.forEach {
        when (it.key) {
            "file" -> { // input file
                fileName = it.value;
            }
            "fast" -> { // faster but a bit more memory-consuming
                BLOCK_COUNT = 64;
                BLOCK_SIZE = 4096 * 256;
            }
            "compact" -> { // slower but less memory-consuming
                BLOCK_COUNT = 32;
                BLOCK_SIZE = 4096;
            }
            "slow" -> { // minimal memory consumption
                BLOCK_COUNT = 1;
                BLOCK_SIZE = 4096;
                bForceGC = true;
            }
            "block_count" -> {
                BLOCK_COUNT = it.value.toInt();
            }
            "block_size" -> {
                BLOCK_SIZE = it.value.toInt();
            }
            "force_gc" -> {
                bForceGC = true;
            }
            else -> {
                println("Unknown parameter --" + it.key);
                printUsage();
                return false;
            }
        }
    }

    if (fileName.isEmpty()) {
        println("--file parameter is not specified");
        printUsage();
        return false;
    }

    return true;
}

fun main(args: Array<String>) {

    if ( !parseCommandLine(args) ) {
        exitProcess( -1 );
    }

    val f = RandomAccessFile(fileName, "r");

    val blockProcessor = CBlockProcessor(f);

    var prevStatCount = 0L;
    var prevStatTime = System.currentTimeMillis();

    var maxMemoryUsed = 0L;
    fun updateMaxMemory(){
        maxMemoryUsed = max( Runtime.getRuntime().totalMemory(), maxMemoryUsed );
    }

    // hint to GC on start
    System.gc();
    @Suppress("DEPRECATION")
    System.runFinalization();

    // Run processor passes until no more data to process.
    while (blockProcessor.processBlocks()) {

        // Display stats from time to time.
        val processedCount = blockProcessor.getProcessedCount();
        if (processedCount - prevStatCount > 5000000 || System.currentTimeMillis() - prevStatTime > 5000 ) {

            updateMaxMemory();

            prevStatCount = processedCount;
            prevStatTime = System.currentTimeMillis();
            blockProcessor.showStatus();

            // Hint to garbage collect if we are very much in need of minimizing memory consumption.
            if ( bForceGC ) {
                System.gc();
                @Suppress("DEPRECATION")
                System.runFinalization();
            }

        }
    };

    // Display final dynamic stats.
    blockProcessor.showStatus();

    updateMaxMemory();

    println();
    println("=======================");
    print(blockProcessor.getUniqueCount());
    println(" unique IPs");
    println();
    print(f.channel.position() / (1024 * 1024));
    print(" MB, ");
    print(blockProcessor.getProcessedCount());
    print(" lines, ");
    print(maxMemoryUsed / (1024 * 1024));
    print("MB RAM max, ");
    print(blockProcessor.getTotalTime() / 1000);
    println(" sec");
}