#pragma once

#include "filter_item.h"

namespace LogReader {

	/**
	* A wrapper of linked list of filter items.
	* Also filter string parser.
	*/
	class CFilterList : public CLinkedList {

	private:

		char * m_filter_string_buffer = NULL; // storage for values of all literals, non-delimited

		bool AddItem( const CFilterItem::Type type, const char * s, const size_t length );

	public:

		CFilterList( void );
		~CFilterList();

		virtual void Clear( void );

		/**
		* Parse glob filter.
		*/
		bool SetFilter( const char * filter );

	};

} // namespace LogReader
