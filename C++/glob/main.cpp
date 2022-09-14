#include "pch.h"

#include "log_reader.h"
#include "stream_iterator.h"
#include "misc.h"
#include "ring_buffer.h"

// output buffer size
static size_t local_buffer_size = 1024 * 64;

namespace LogReader {
    /**
    * Asyncronous output writer
    */
    class COutputBuffer : public CRingBuffer {
    private:
        FILE * m_file;

        /**
        * writer worker
        */
        virtual void OnProcessBlock( CRingBufferBlock & block ) {
            
            // The block buffer data size (buffer->SetDataSize) is not updated in the main thread after GetNextLine() call since the size is not needed there.
            // So to get a size to write wee need to calculate it here.
            size_t size = strlen( block.GetData() );
            
            if ( fwrite( block.GetData(), 1, size, m_file ) != size ) {
                fprintf( stderr, "Output file write failure" );
                fprintf( stderr, "\r\n" );
                exit( -1 );
            }
        };
    public:
        COutputBuffer( const int block_count, size_t block_size, FILE * file )
            : CRingBuffer( block_count, block_size )
            , m_file( file )
        {
        }
    };
}

/**
* Log sink for the LogReader namespace.
*/
void log( const char * s ) {
    fprintf( stderr, "%s", s );
    //exit( -1 );
}

/**
* Checked nothrow allocator.
* Checked because "the code should be absolutely foolproof and robust".
* Nothrow because "Exceptions: the use of Windows and C++ exceptions is not allowed".
*/
char * allocate_buffer( size_t size ) {
    char * buffer = new ( std::nothrow ) char[ size ];
    if ( buffer == NULL ) {
        fprintf( stderr, "Allocation failure" );
        fprintf( stderr, "\r\n" );
        exit( -1 );
    }
    return buffer;
}

/**
* Process the input_file_name with the filter and output results to output_file_name (or to stdout if output_file_name=NULL).
*/
void process( const char * filter, const char * input_file_name, const char * output_file_name ) {

    LogReader::set_log_sink( log );
    
    auto log_reader = CLogReader();

    if ( !log_reader.SetFilter( filter ) ) {
        fprintf( stderr, "Invalid filter: " );
        fprintf( stderr, filter );
        fprintf( stderr, "\r\n" );
        exit( -1 );
    }

    if ( !log_reader.Open( input_file_name ) ) {
        fprintf( stderr, "Can't open file for reading: " );
        fprintf( stderr, input_file_name );
        fprintf( stderr, "\r\n" );
        exit( -1 );
    }

    FILE * output_file = NULL;
    if ( output_file_name == NULL ) {
        output_file = stdout;
    } else {
        if ( fopen_s( &output_file, output_file_name, "wb" ) != 0 ) {
            fprintf( stderr, "Can't open file for writing: " );
            fprintf( stderr, output_file_name );
            fprintf( stderr, "\r\n" );
            exit( -1 );
        }
    }

    // CRLF token for output file
    const char * CRLF = "\r\n";
    size_t CRLF_length = strlen( CRLF );

    LogReader::COutputBuffer output = LogReader::COutputBuffer( 4, local_buffer_size, output_file );
    if ( !output.Init() ) {
        fprintf( stderr, "Can't init output buffer" );
        exit( -1 );
    }

    unsigned long long int output_line_count = 0; // found line counter for debugging
    bool bEmptyString = true; // previous string found was empty (* filter)
    bool bFound = false; // false if EOF is reached
    do {
        // get the free uninitialized buffer for writing
        // also enqueue the previous buffer, filled by GetNextLine() to be written
        auto buffer = output.GetNextBlockToProcess();

        // set data to be always zero-terminated
        // free space length passed to GetNextLine() is one character shorter to keep the buffer zero-terminated
        buffer->GetMutableData()[ buffer->GetAllocatedSize() - 1 ] = '\0';
        
        // initialize data size to zero
        buffer->SetDataSize( 0 );
        
        if ( output_line_count > 0 ) {
            // adding CRLF if it is not the first output string
            buffer->SetDataSize( CRLF_length );
            memcpy( buffer->GetMutableData(), CRLF, buffer->GetDataSize() );
        }

        // set the current data in the buffer to be zero-terminated
        buffer->GetMutableData()[ buffer->GetDataSize() ] = '\0';

        // fund the next matching line and place it into the buffer
        bFound = log_reader.GetNextLine(
            buffer->GetMutableData() + buffer->GetDataSize(), // free space is after CRLF if CRLF is added
            ( int )( buffer->GetAllocatedSize() - buffer->GetDataSize() - 1 ) // free space is one character shorter to keep it zero-terminated
        );

        if ( bFound ) {
            
            // another matching line is found

            output_line_count++;
            //debug break
            //if ( output_line_count == 0 ) {
            //    output_line_count++; output_line_count--;
            //}

            // buffer->GetDataSize() is still the same as it was before GetNextLine()
            // so if there is \0 here then it means that GetNextLine() returned empty string
            bEmptyString = ( buffer->GetData()[ buffer->GetDataSize() ] == '\0' );

            // just in case it was somehow overwritten in GetNextLine()
            buffer->GetMutableData()[ buffer->GetAllocatedSize() - 1 ] = '\0';

        } else {

            // even if false is returned by GetNextLine() there is still no guarantee that the buffer does not contain some random data
            buffer->GetMutableData()[ buffer->GetDataSize() ] = '\0';
            
            // prevent adding one more \r\n to the file end every time a file is processed with * filter
            if ( bEmptyString ) {
                buffer->SetDataSize( 0 );
                buffer->GetMutableData()[ 0 ] = '\0';
            }

        }

    } while ( bFound );

    // flush last filled buffer
    output.Uninit();

    log_reader.Close();

    // close the output except when it is the stdout
    if ( output_file_name != NULL ) {
        fclose( output_file );
    }
}

/**
* Displays usage info and exits.
*/ 
void usage( void ) {
    fprintf( stderr, "Usage:\r\ntool.exe <file name> <pattern>" );
    fprintf( stderr, "\r\n" );
    exit( -1 );
}

// test mode
//#define TEST

#ifdef TEST
#include "test.h"
#endif // TEST

int main( int argc, char * argv[] ) {

#ifndef TEST

    if ( argc < 3 ) {
        // missing mandatory parameters
        usage();
    }

    // do the work
    process( argv[ 2 ], argv[ 1 ], NULL );

#else // !TEST

    local_buffer_size = 1024 * 1024; // test algorithm idempotency with the sufficienly large output buffer size
    test();

#endif // !TEST
}
