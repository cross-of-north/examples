#include "pch.h"

#include "filter_item.h"

namespace LogReader {

	CFilterItem::CFilterItem( const CFilterItem::Type type, const char * s, const size_t length )
		: m_type( type )
		, m_string( s )
		, m_length( length )
	{
		switch ( m_type ) {
			case CFilterItem::Type::Literal:
			case CFilterItem::Type::AnyChar:
			{
				m_fixed_length_chain_length = length;
			}
		}
	}

	CFilterItem::~CFilterItem() {
	}

	bool CFilterItem::IsFixedLength( void ) const {
		switch ( GetType() ) {
			case CFilterItem::Type::Literal:
			case CFilterItem::Type::LineStart:
			case CFilterItem::Type::LineEnd:
			case CFilterItem::Type::AnyChar:
			{
				return true;
			}
		}
		return false;
	}

	void CFilterItem::SetNext( CLinkedListItem * next_ ) {
		CFilterItem * next = static_cast < CFilterItem * > ( next_ );
		m_next = next;
		next->m_prev = this;
		if ( next->IsFixedLength() ) {
			// propagate chain fixed length increase to chain siblings
			CFilterItem * p = this;
			while ( p != NULL && p->IsFixedLength() ) {
				p->m_fixed_length_chain_length += next->m_fixed_length_chain_length;
				p = static_cast < CFilterItem * > ( p->m_prev );
			}
		}
	}

} // namespace LogReader
