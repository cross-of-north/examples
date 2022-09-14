#include "pch.h"

#include "input_stream.h"

#include "misc.h"

namespace LogReader {


	// count of buffers used for asynchronous file reading.
	static int CAsyncInputStream_buffer_count = 2;

	// file page size
	static DWORD CAsyncInputStream_page_size = 0x10000000;

	CAsyncInputStreamBlock::CAsyncInputStreamBlock( CRingBuffer & ring_buffer )
		: CRingBufferBlock( 0, ring_buffer )
	{

	}

	CAsyncInputStreamBlock::~CAsyncInputStreamBlock() {

	}

	CRingBufferBlock * CAsyncInputStream::CreateBlock( void ) {
		return new ( std::nothrow ) CAsyncInputStreamBlock( *this );
	}

	DWORD CAsyncInputStream::GetPageSize( void ) {
		return CAsyncInputStream_page_size;
	}

	CAsyncInputStream::CAsyncInputStream( const size_t page_prefix_size )
		: CRingBuffer( CAsyncInputStream_buffer_count, 0 )
		, m_page_prefix_size( page_prefix_size )
	{
		SYSTEM_INFO SysInfo;
		// Get the system allocation granularity.
		::GetSystemInfo( &SysInfo );
		m_dwSysGran = SysInfo.dwAllocationGranularity;

	}

	CAsyncInputStream::~CAsyncInputStream() {
	}

	bool CAsyncInputStream::IsOpened( void ) const {
		return m_file != INVALID_HANDLE_VALUE;
	}

	bool CAsyncInputStream::Open( const char * filename ) {
		if ( filename == NULL ) {
			log( "NULL filename\n" );
			return false;
		}
		if ( IsOpened() ) {
			log( "File is already opened\n" );
			return false;
		}
		if ( !Init() ) {
			return false;
		}
		m_file = ::CreateFileA( 
			filename, 
			GENERIC_READ, 
			FILE_SHARE_READ, 
			NULL, 
			OPEN_EXISTING, 
			FILE_FLAG_SEQUENTIAL_SCAN,
			NULL
		);
		if ( m_file == INVALID_HANDLE_VALUE ) {
			log( "Can't open file\n" );
			m_file = NULL;
			return false;
		}
		m_current_file_offset = 0;

		if ( ::GetFileSizeEx( m_file, &m_file_size ) == 0 ) {
			log( "Error mapping file\n" );
			DWORD dw = GetLastError();
			// debug
			dw++; dw--;
			return false;
		}

		return true;
	}

	void CAsyncInputStream::Close() {
		Uninit(); // wait for workers to finish and free buffers
		CAsyncInputStreamBlock * block = static_cast < CAsyncInputStreamBlock * > ( GetFirstBlock() );
		while ( block != NULL ) {
			CloseBlock( *block );
			block = static_cast < CAsyncInputStreamBlock * > ( block->GetNext() );
		}
		if ( m_file != INVALID_HANDLE_VALUE ) {
			::CloseHandle( m_file );
			m_file = INVALID_HANDLE_VALUE;
		}
	}

	void CAsyncInputStream::CloseBlock( CAsyncInputStreamBlock & block ) {
		if ( block.m_lpMapAddress != NULL ) {
			::UnmapViewOfFile( block.m_lpMapAddress );
			block.m_lpMapAddress = NULL;
			block.OverrideDataPointer( block.m_allocated_data );
		}
		if ( block.m_hMapFile != INVALID_HANDLE_VALUE ) {
			::CloseHandle( block.m_hMapFile ); // close the file mapping object
			block.m_hMapFile = INVALID_HANDLE_VALUE;
		}
	}

	void CAsyncInputStream::OnProcessBlock( CRingBufferBlock & block_ ) {

		CAsyncInputStreamBlock & block = static_cast < CAsyncInputStreamBlock & > ( block_ );

		CloseBlock( block );

		uint64_t size_to_read = m_file_size.QuadPart - m_current_file_offset;
		if ( size_to_read > GetPageSize() ) {
			size_to_read = GetPageSize();
		}

		uint64_t current_file_offset = m_current_file_offset;
		size_t current_page_prefix_size = 0;
		if ( current_file_offset > m_page_prefix_size ) {
			current_file_offset -= m_page_prefix_size;
			current_page_prefix_size = m_page_prefix_size;
		}
		LONGLONG llFileMapStart = ( current_file_offset / m_dwSysGran ) * m_dwSysGran;

		// Calculate the size of the file mapping view.
		DWORD dwMapViewSize = ( current_file_offset % m_dwSysGran ) + ( DWORD )( size_to_read + current_page_prefix_size );

		// How large will the file mapping object be?
		LONGLONG llFileMapSize = current_file_offset + size_to_read + current_page_prefix_size;

		DWORD dwMapOffset = ( DWORD )( ( current_file_offset - llFileMapStart ) & 0xFFFFFFFF );

		if ( llFileMapSize <= 0 ) {
			// EOF
			block.SetDataSize( 0 );
			return;
		}

		block.m_hMapFile = ::CreateFileMapping(
			m_file,          // current file handle
			NULL,           // default security
			PAGE_READONLY, // read/write permission
			llFileMapSize >> 32,              // size of mapping object, high
			llFileMapSize & 0xFFFFFFFF,  // size of mapping object, low
			NULL );

		if ( block.m_hMapFile == NULL ) {
			log( "Error mapping file\n" );
			DWORD dw = GetLastError();
			// debug
			dw++; dw--;
			block.SetDataSize( 0 );
			return;
		}

		block.m_lpMapAddress = ::MapViewOfFile( block.m_hMapFile,            // handle to
			// mapping object
			FILE_MAP_READ,
			llFileMapStart >> 32,                   // high-order 32
			// bits of file
			// offset
			llFileMapStart & 0xFFFFFFFF,      // low-order 32
			// bits of file
			// offset
			0 );      // number of bytes
		// to map
		if ( block.m_lpMapAddress == NULL ) {
			log( "Error mapping file\n" );
			DWORD dw = GetLastError();
			// debug
			dw++; dw--;
			block.SetDataSize( 0 );
			return;
		}

		m_current_file_offset += size_to_read;

		// Calculate the pointer to the data.
		auto pData = ( char * )block.m_lpMapAddress + LONGLONG( dwMapOffset ) + current_page_prefix_size;
		block.OverrideDataPointer( pData );

		block.SetDataSize( ( DWORD )size_to_read );
	}

	bool CAsyncInputStream::GetPage( const char *& buffer_read, size_t & size_read ) {

		CRingBufferBlock * result = GetNextProcessedBlock(); // get the next processed buffer item from the ring buffer
		if ( result == NULL ) {
			buffer_read = NULL;
			size_read = 0;
			return false;
		}

		buffer_read = result->GetData();
		size_read = result->GetDataSize();

		return true;
	}

} // namespace LogReader
