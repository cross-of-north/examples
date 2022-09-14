#pragma once

#include "ring_buffer.h"

namespace LogReader {

	class CAsyncInputStreamBlock: public CRingBufferBlock {
		friend class CAsyncInputStream;
	protected:
		HANDLE m_hMapFile = INVALID_HANDLE_VALUE;
		LPVOID m_lpMapAddress = NULL;
	public:
		CAsyncInputStreamBlock( CRingBuffer & ring_buffer );
		virtual ~CAsyncInputStreamBlock();
	};

	/**
	* Asynchronous input stream.
	*/
	class CAsyncInputStream: public CRingBuffer {

	protected:

		size_t m_page_size; // size of page buffer to read data from file to
		size_t m_page_prefix_size; // size of page prefix to place additional data to
		uint64_t m_current_file_offset = 0;
		LARGE_INTEGER m_file_size = { 0 };
		DWORD m_dwSysGran = 0;
		HANDLE m_file = INVALID_HANDLE_VALUE;

		static DWORD GetPageSize( void );

		// Reads data from file. Called from the worker thread.
		virtual void OnProcessBlock( CRingBufferBlock & block );

		virtual CRingBufferBlock * CreateBlock( void );

		void CloseBlock( CAsyncInputStreamBlock & block );

	public:

		/**
		* block_count - count of data buffers of size page_prefix_size + page_size
		*/
		CAsyncInputStream( const size_t page_prefix_size );
		virtual ~CAsyncInputStream();

		/**
		* Opens underlying file and creates buffers.
		*/
		bool Open( const char * filename );

		/**
		* Waits for all reader workers to finish.
		* Then closes the underlying file and destroys buffers.
		*/
		void Close( void );

		/**
		* Fills buffer_read by the pointer to the data read from file.
		* The size of data at buffer_read is size_read.
		* At m_page_prefix_size before buffer_read starts the uninitialized memory area of m_page_prefix_size for additional data.
		* The data located before and at buffer_read is freely mutable and valid until the next GetPage() call with any parameters.
		* size_read is zero on EOF.
		* 
		* Returns false on critical failure.
		*/
		bool GetPage( const char *& buffer_read, size_t & size_read );

		bool IsOpened( void ) const;

	};

} // namespace LogReader