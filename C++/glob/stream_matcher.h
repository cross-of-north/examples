#pragma once

#include "filter_list.h"

namespace LogReader {

	/**
	* Filter matching mechanics.
	*/
	class CStreamMatcher {

	public:

		enum class Match {
			MatchUnknown,
			HaveMatch,
			NoMatch,
		};

	private:

		// the pointer to the filter to work with
		const CFilterList * m_filter_list = NULL;

		/**
		* Filter iterator state variables
		*/

		// current match state
		CStreamMatcher::Match m_match = CStreamMatcher::Match::MatchUnknown;

		// current filter rule to check
		const CFilterItem * m_filter_item = NULL;

		// the first filter rule of the current fixed-length sequence
		const CFilterItem * m_fixed_length_block_start_filter = NULL;

		// the current fixed-length sequence length
		size_t m_fixed_length_block_current_length = 0;

		// current matched length
		size_t m_line_length = 0;

		// pointer to the buffer passed to the current ProcessBuffer() call
		const char * m_buffer = NULL;

		// currently processed position in the buffer passed to the current ProcessBuffer() call
		size_t m_buffer_offset = 0;

		// length of the buffer passed to the current ProcessBuffer() call
		size_t m_buffer_length = 0;

		// position of the new (compared to the preceding ProcessBuffer() call) data in the buffer passed to the current ProcessBuffer() call
		size_t m_zero_position = 0;

		// If matcher needs more data at the end of buffer it returns from ProcessBuffer() with MatchUnknown
		// leaving the unprocessed tail.
		// The higher lefel creates the buffer consisting of the unprocessed tail of the previous buffer at the start
		// concatenated with the new buffer data.
		// To process the unprocessed tail, on the next ProcessBuffer() call
		// the processing starts not from the new data offset but from the new data minus m_process_again_offset.
		size_t m_process_again_offset = 0;

		// performs rollback (backtracking) to the next possible match start position when the fixed length sequence fails to be matched 
		bool Backtrack( void );

		// returns true if matching against the fixed length sequence currently
		bool IsFixedLengthFragment( void ) const;

		// returns true if the current filter is the first in the current fixed length sequence
		bool IsStartOfFixedLength( void ) const;

		// helper to begin processing of the fixed length sequence
		// returns false if the data length remaining in the buffer is not enough to match the current fixed-length sequence
		bool StartFixedLength( void );

		// helper to end processing of the fixed length sequence
		void EndFixedLength( void );

	public:

		CStreamMatcher( const CFilterList * filter_list );

		/**
		* Processes the buffer passed trying to determine if the data in the buffer matches to the filter.
		* 
		* Buffer structure:
		* buffer[0] - start of possibly unprocessed tail of old data
		* buffer[zero_position] - start of new data
		* buffer[buffer_length-1] - the last charater
		* 
		* If bFinal=true then this is the last buffer of the string.
		*/
		bool ProcessBuffer( const char * buffer, size_t buffer_length, size_t zero_position, bool bFinal );

		/**
		* Resets the state variables to start matching of the new string.
		*/
		void Reset( void );

		CStreamMatcher::Match GetMatch( void ) const {
			return m_match;
		}

	};

} // namespace LogReader
