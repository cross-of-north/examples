#include "pch.h"

#include "stream_matcher.h"

#include "misc.h"

namespace LogReader {

    bool CStreamMatcher::Backtrack() {

        bool bResult = true;

        if ( m_fixed_length_block_start_filter == NULL ) {

            // nowhere to backtrack
            log( "No backtracking context\n" );
            bResult = false;

        } else {

            size_t backstep_size = m_fixed_length_block_current_length;
            if ( backstep_size == 0 ) {

                // zero-size sequence backtracking attempted, e.g. for "^<non-matching-literal>..."
                // nowhere to backtrack
                //log( "Nowhere to backtrack\n" );
                bResult = false;

            } else {

                m_filter_item = m_fixed_length_block_start_filter;
                backstep_size--; // start from the next char compared to the preceding iteration
                if ( backstep_size > m_line_length ) {
                    // backstep is too large
                    log( "Backtrack step is too large\n" );
                    bResult = false;
                } else {
                    m_line_length -= backstep_size;
                    m_buffer_offset -= backstep_size; // start from the next char
                    if ( m_buffer_offset >= m_buffer_length ) {
                        // buffer overrun (???)
                        log( "Buffer overrun while backtracking\n" );
                        bResult = false;
                    }
                }

            }

            EndFixedLength(); // will restart in ProcessBuffer()

        }

        return bResult;
    }

    bool CStreamMatcher::IsFixedLengthFragment( void ) const {
        return m_fixed_length_block_start_filter != NULL;
    }

    bool CStreamMatcher::IsStartOfFixedLength( void ) const {
        return m_filter_item == m_fixed_length_block_start_filter;
    }

    bool CStreamMatcher::StartFixedLength( void ) {
        if ( m_buffer_offset + m_filter_item->GetChainLength() > m_buffer_length ) {
            return false; // more data needed to process
        } else {
            m_fixed_length_block_current_length = 0;
            m_fixed_length_block_start_filter = m_filter_item;
            return true;
        }
    }

    void CStreamMatcher::EndFixedLength( void ) {
        m_fixed_length_block_current_length = 0;
        m_fixed_length_block_start_filter = NULL;
    }

    CStreamMatcher::CStreamMatcher( const CFilterList * filter_list )
        : m_filter_list( filter_list ) {
        Reset();
    }

    void CStreamMatcher::Reset( void ) {
        m_match = CStreamMatcher::Match::MatchUnknown;
        m_filter_item = static_cast < CFilterItem * > ( m_filter_list->GetFirst() );
        m_fixed_length_block_start_filter = NULL;
        m_fixed_length_block_current_length = 0;
        m_line_length = 0;
        m_buffer = NULL;
        m_zero_position = 0;
    }

    bool CStreamMatcher::ProcessBuffer( const char * buffer, size_t buffer_length, size_t zero_position, bool bFinal ) {

        // unprocessed tail of the previous iteration
        // is expected to be saved at the start of the buffer by the previous level
        if ( m_process_again_offset > zero_position ) {
            // invalid input buffer
            m_process_again_offset = 0;
            m_buffer_length = 0;
            log( "Invalid input buffer\n" );
            return false;
        }

        m_buffer = buffer;
        m_buffer_length = buffer_length;
        m_buffer_offset = zero_position - m_process_again_offset;
        m_zero_position = zero_position;

        while ( m_filter_item != NULL && m_match == CStreamMatcher::Match::MatchUnknown && m_buffer_offset <= m_buffer_length ) {

            size_t remaining_buffer = ( m_buffer_offset <= m_buffer_length ) ? m_buffer_length - m_buffer_offset : 0;

            if ( IsFixedLengthFragment() != m_filter_item->IsFixedLength() ) {
                // switch mode
                if ( m_filter_item->IsFixedLength() ) {
                    if ( !StartFixedLength() ) {
                        // more data needed to match
                        break;
                    }
                } else {
                    EndFixedLength();
                }
            }

            if ( m_filter_item->GetType() == CFilterItem::Type::LineStart ) {

                if ( m_line_length != 0 ) {
                    // have the final match result
                    m_match = CStreamMatcher::Match::NoMatch;
                    break;
                }
                // else go to the next filter item

            } else if ( m_filter_item->GetType() == CFilterItem::Type::LineEnd ) {

                if ( IsStartOfFixedLength() ) {
                    // something like ^...*$
                    // whatever the remaining string might be, it can't unmatch
                    // have the final match result
                    m_match = CStreamMatcher::Match::HaveMatch;
                    break;
                } else {
                    // something like ^...abc$
                    // must match immediately or backtrack
                    if ( remaining_buffer == 0 ) {
                        if ( bFinal ) {
                            // right at the end
                            // have the final match result
                            m_match = CStreamMatcher::Match::HaveMatch;
                            break;
                        } else {
                            // more data needed
                            break;
                        }
                    } else {
                        if ( Backtrack() ) {
                            // continue from the start with the first filter item of the chain
                            continue;
                        } else {
                            // have the final match result
                            m_match = CStreamMatcher::Match::NoMatch;
                            break;
                        }
                    }
                }

            } else if ( m_filter_item->GetType() == CFilterItem::Type::AnyLength ) {

                // do nothing
                // go to the next filter item

            } else if ( m_filter_item->GetType() == CFilterItem::Type::AnyChar ) {

                if ( m_filter_item->GetLength() > remaining_buffer ) {
                    // more data needed
                    break;
                }

                m_buffer_offset += m_filter_item->GetLength();
                m_line_length += m_filter_item->GetLength();
                m_fixed_length_block_current_length += m_filter_item->GetLength();

            } else if ( m_filter_item->GetType() == CFilterItem::Type::Literal ) {

                if ( m_filter_item->GetLength() > remaining_buffer ) {
                    // more data needed
                    break;
                }

                if ( IsStartOfFixedLength() ) {

                    // may search forward freely

                    const char * p_literal = memmem(
                        buffer + m_buffer_offset, m_buffer_length - m_buffer_offset,
                        m_filter_item->GetString(), m_filter_item->GetLength()
                    );

                    if ( p_literal == NULL ) {
                        // literal is not found
                        size_t consumed_buffer_size = remaining_buffer - m_filter_item->GetLength();
                        m_buffer_offset += consumed_buffer_size;
                        m_line_length += consumed_buffer_size;
                        // more data needed
                        break;
                    } else {
                        // literal is found
                        size_t consumed_buffer_size = p_literal - buffer + m_filter_item->GetLength() - m_buffer_offset;
                        m_buffer_offset += consumed_buffer_size;
                        m_line_length += consumed_buffer_size;
                        m_fixed_length_block_current_length += m_filter_item->GetLength();
                        // go to the next filter item
                    }

                } else {

                    // already pinned to the fixed position

                    if ( memcmp( buffer + m_buffer_offset, m_filter_item->GetString(), m_filter_item->GetLength() ) == 0 ) {
                        // literal is found
                        m_buffer_offset += m_filter_item->GetLength();
                        m_line_length += m_filter_item->GetLength();
                        m_fixed_length_block_current_length += m_filter_item->GetLength();
                        // go to the next filter item
                    } else {
                        // literal is not found right here
                        if ( Backtrack() ) {
                            // continue from the start with the first filter item of the chain
                            continue;
                        } else {
                            // have the final match result
                            m_match = CStreamMatcher::Match::NoMatch;
                            break;
                        }
                    }

                }

            }

            // next filter item
            m_filter_item = static_cast < CFilterItem * > ( m_filter_item->GetNext() );
        }

        if ( bFinal && m_match == CStreamMatcher::Match::MatchUnknown ) {
            // if there are no more data to provide match then it is no match
            m_match = CStreamMatcher::Match::NoMatch;
        }

        if ( m_match == CStreamMatcher::Match::MatchUnknown && m_buffer_length > 0 && m_buffer_offset < m_buffer_length ) {
            // more data is needed but unprocessed data tail exists
            // we will process it at the next ProcessBuffer() call
            m_process_again_offset = m_buffer_length - m_buffer_offset;
        } else {
            m_process_again_offset = 0;
        }

        return m_match == CStreamMatcher::Match::HaveMatch;
    }

} // namespace LogReader
