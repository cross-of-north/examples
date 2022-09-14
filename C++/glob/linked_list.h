#pragma once

namespace LogReader {

	/**
	* An element of a simple linked list implementation.
	*/
	class CLinkedListItem {

	private:

		CLinkedListItem( const CLinkedListItem & ) = delete;

	protected:

		CLinkedListItem * m_next = NULL; // Linked list next.
		CLinkedListItem * m_prev = NULL; // Linked list prev.

	public:

		CLinkedListItem( void );
		virtual ~CLinkedListItem();

		virtual void SetNext( CLinkedListItem * next );

		CLinkedListItem * GetNext( void ) const {
			return m_next;
		}

		CLinkedListItem * GetPrev( void ) const {
			return m_prev;
		}

	};

	/**
	* A simple linked list implementation.
	*/
	class CLinkedList {

	protected:

		CLinkedListItem * m_first_item = NULL;
		CLinkedListItem * m_last_item = NULL;

	public:

		CLinkedList( void );
		virtual ~CLinkedList();

		void Add( CLinkedListItem * item );
		virtual void Clear( void );

		CLinkedListItem * GetFirst( void ) const {
			return m_first_item;
		}

		CLinkedListItem * GetLast( void ) const {
			return m_last_item;
		}

	};

} // namespace LogReader
