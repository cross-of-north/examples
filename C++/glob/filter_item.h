#pragma once

#include "linked_list.h"

namespace LogReader {

	/**
	* One separate filter item.
	*/
	class CFilterItem : public CLinkedListItem {

		friend class CFilterList;

	public:

		enum class Type {
			NOP, // not used
			LineStart, // ^ RegExp, fixed zero-length
			LineEnd, // $ RegExp, fixed zero-length
			Literal, // exact literal sequence, fixed length
			AnyChar, // .{n} RegExp (glob ? sequence), fixed length n
			AnyLength, // .* RegExp (glob *), variable length
		};

	private:

		CFilterItem( const CFilterItem & ) = delete;

	protected:

		const CFilterItem::Type m_type; // Filter type.
		const char * m_string; // A string buffer with the literal value, not zero-terminated.
		const size_t m_length; // Exact value of how many chars will be consumed on filter match (length of literals, n for ?).
		size_t m_fixed_length_chain_length = 0; // Exact value of how many chars will be consumed on match of the whole fixed-length filter sequence.

		/**
		* Link next item
		*/
		virtual void SetNext( CLinkedListItem * next );

	public:

		CFilterItem( const CFilterItem::Type type, const char * s, const size_t length );
		~CFilterItem();

		bool IsFixedLength( void ) const;

		CFilterItem::Type GetType( void ) const {
			return m_type;
		}

		const char * GetString( void ) const {
			return m_string;
		}

		size_t GetLength( void ) const {
			return m_length;
		}

		size_t GetChainLength( void ) const {
			return m_fixed_length_chain_length;
		}

	};

} // namespace LogReader
