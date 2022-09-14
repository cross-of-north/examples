#pragma once

#include "stream_matcher.h"
#include "input_stream.h"

namespace LogReader {

	/**
	* The mechanics to iterate over file data returning matching strings via GetNextLine() method.
	*/
	class CStreamIterator {

	private:

		// a filter structure to apply
		const CFilterList * m_filter_list = NULL;

		// asynchronous buffered file reader
		CAsyncInputStream * m_input_stream = NULL;

		// filter matching mechanism
		CStreamMatcher * m_stream_matcher = NULL;

		// an owned intermediate storage for the tail of non-processed data from the previous file page
		char * m_tail_data_storage = NULL;


		/**
		* File stream iterator state variables
		*/

		// The pointer to the current file page buffer.
		// Contains space for the tail of non-processed data from the previous file page before the buffer pointed to.
		const char * m_stream_page = NULL;

		// the pointer to the start of currently processed string data
		const char * m_current_string_start = NULL;

		// the offset of the currently processed data at the m_stream_page
		size_t m_paged_data_position = 0;

		// the size of m_stream_page data (not including size of space before m_stream_page)
		size_t m_page_size = 0;

		// the size of of usable space before m_stream_page
		size_t m_paged_data_offset = 0;

		// if true then the buffer passed to the GetNextLine() is already fully filled
		bool m_bStringTooLong = false;

		// the current value of the buffer passed to the GetNextLine() 
		char * m_output_buffer = NULL;

		// the current position of the free space in the buffer passed to the GetNextLine() 
		size_t m_output_buffer_position = 0;

		// the current length of data in the buffer passed to the GetNextLine() 
		size_t m_output_buffer_length = 0;

		// debug
		unsigned long long int m_file_pos = 0;

		// the first encountered EOL character (CR or LF)
		char m_line_end_char = 0;

		// true when EOL parsing is done
		bool m_bLineEnd = false;

		// true when the input file is fully read
		bool m_bEOF = true;

		// true when file page end is processed
		bool m_bEndOfBuffer = true;

		// a pointer to the EOL characters encountered
		const char * m_p_next_CRLF = NULL;

		// the current string filter match state
		CStreamMatcher::Match m_match_result = CStreamMatcher::Match::MatchUnknown;

		// helper to add data to the buffer passed to the GetNextLine()
		void AddStringToOutputBuffer( const char * source, const size_t string_length_to_copy );

		// helper to iterate over EOL characters
		void ConsumeCRLF( const bool bNew );

	public:

		CStreamIterator( void );
		~CStreamIterator();

		bool Open( const char * filename );
		void Close( void );
		bool SetFilter( const CFilterList * filter_list );
		
		/**
		* Scans the file for the next matching string and copies its data to the buffer provided.
		* If buf[ bufsize - 1 ] on return is not \0 then the body of the string found was truncated since it is longer than bufsize.
		* Returns false on EOF.
		*/
		bool GetNextLine( char * buf, const int bufsize );

	};


} // namespace LogReader
