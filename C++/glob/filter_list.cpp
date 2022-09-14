#include "pch.h"

#include "filter_list.h"
#include "misc.h"

namespace LogReader {

	CFilterList::CFilterList( void ) {
	}

	CFilterList::~CFilterList() {
	}

	bool CFilterList::AddItem( const CFilterItem::Type type, const char * s, const size_t length ) {
		bool bResult = true;
		CFilterItem * filter_item = new ( std::nothrow ) CFilterItem( type, s, length );
		if ( filter_item == NULL ) {
			log( "Allocation failure\n" );
			bResult = false;
		} else {
			Add( filter_item );
		}
		return bResult;
	}

	void CFilterList::Clear( void ) {
		CLinkedList::Clear();
		delete[] m_filter_string_buffer;
		m_filter_string_buffer = NULL;
	}

	bool CFilterList::SetFilter( const char * filter ) {

		if ( filter == NULL ) {
			log( "NULL filter is passed\n" );
			return false;
		}

		Clear();

		// filter is expected to be reasonably safe if not-NULL
		size_t source_filter_length = strlen( filter );
		// the storage of all literals can't be longer than input string
		m_filter_string_buffer = new ( std::nothrow ) char[ source_filter_length + 1 ];

		if ( m_filter_string_buffer == NULL ) {
			// couldn't allocate filter string
			log( "Allocation failure\n" );
			return false;
		}

		// implicit ^ RegExp
		if ( !AddItem( CFilterItem::Type::LineStart, NULL, 0 ) ) {
			// allocation failure
			return false;
		}

		bool bResult = true;

		size_t buffer_start = 0; // offset of the current literal data start in m_filter_string_buffer
		size_t buffer_offset = 0; // offset to add data at to m_filter_string_buffer
		size_t mask_length = 0; // n for the current .{n}
		bool bEscapedCharacter = false; // flag for processing escaped input data: \\, \*, \?, \0
		CFilterItem::Type filter_type = CFilterItem::Type::NOP; // current filter type

		// iterate over input filter string
		for ( size_t source_filter_index = 0; source_filter_index <= source_filter_length; source_filter_index++ ) {

			// A sign to collect last filter data at the input string length.
			bool bAfterLastChar = ( source_filter_index == source_filter_length );

			char c = filter[ source_filter_index ];

			if ( bEscapedCharacter ) {
				if ( bAfterLastChar ) {
					// non-terminated escaped character
					log( "Non-terminated escaping in the filter string\n" );
					bResult = false;
					break;
				} else {
					if ( c != ANY_LENGTH && c != ESCAPED_ZERO && c != ANY_CHAR && c != ESCAPE ) {
						// invalid escaped character
						log( "Invalid escaped charater\n" );
						bResult = false;
					}
					// add unescaped value to the literals storage
					m_filter_string_buffer[ buffer_offset++ ] = ( c == ESCAPED_ZERO ) ? '\0' : c;
					bEscapedCharacter = false;
					// process next character
					continue;
				}
			}

			bool bCurrentCharIsMask = ( c == ANY_CHAR || c == ANY_LENGTH ); // current char is not literal

			if ( filter_type == CFilterItem::Type::Literal && ( bAfterLastChar || bCurrentCharIsMask ) ) {

				// collect Literal

				size_t token_size = buffer_offset - buffer_start;
				if ( !AddItem( filter_type, m_filter_string_buffer + buffer_start, token_size ) ) {
					// allocation failure
					bResult = false;
					break;
				}
				filter_type = CFilterItem::Type::NOP;

			} else if ( filter_type == CFilterItem::Type::AnyLength && ( c != ANY_LENGTH || bAfterLastChar ) ) {

				// collect AnyLength

				if ( !AddItem( filter_type, NULL, 0 ) ) {
					// allocation failure
					bResult = false;
					break;
				}
				filter_type = CFilterItem::Type::NOP;

			} else if ( filter_type == CFilterItem::Type::AnyChar && ( c != ANY_CHAR || bAfterLastChar ) ) {

				// collect AnyChar

				if ( !AddItem( filter_type, NULL, mask_length ) ) {
					// allocation failure
					bResult = false;
					break;
				}
				filter_type = CFilterItem::Type::NOP;

			}

			if ( filter_type == CFilterItem::Type::NOP ) {
				// new filter start
				buffer_start = buffer_offset;
				mask_length = 0;
			}

			if ( c == ANY_LENGTH ) {

				filter_type = CFilterItem::Type::AnyLength;

			} else if ( c == ANY_CHAR ) {

				filter_type = CFilterItem::Type::AnyChar;
				mask_length++; // increasing .{n}

			} else {

				filter_type = CFilterItem::Type::Literal;

				if ( c == ESCAPE ) {
					bEscapedCharacter = true;
				} else {
					// literal value
					m_filter_string_buffer[ buffer_offset++ ] = c;
				}

			}

		}

		if ( bResult ) {
			// implicit $ RegExp
			if ( !AddItem( CFilterItem::Type::LineEnd, NULL, 0 ) ) {
				// allocation failure
				bResult = false;
			}
		}

		if ( !bResult ) {
			Clear();
		}

		return bResult;
	}

} // namespace LogReader
