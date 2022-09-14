#include "pch.h"

#include "linked_list.h"

namespace LogReader {

	CLinkedListItem::CLinkedListItem( void ) {
	}

	CLinkedListItem::~CLinkedListItem() {
	}

	void CLinkedListItem::SetNext( CLinkedListItem * next ) {
		m_next = next;
		next->m_prev = this;
	}

	CLinkedList::CLinkedList( void ) {
	}

	CLinkedList::~CLinkedList() {
		Clear();
	}

	void CLinkedList::Add( CLinkedListItem * item ) {
		if ( m_first_item == NULL ) {
			m_first_item = item;
		} else {
			m_last_item->SetNext( item );
		}
		m_last_item = item;
	}

	void CLinkedList::Clear( void ) {
		while ( m_first_item != NULL ) {
			CLinkedListItem * next = m_first_item->GetNext();
			delete m_first_item;
			m_first_item = next;
		}
		m_last_item = NULL;
	}

} // namespace LogReader
