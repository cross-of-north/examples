#include "pch.h"

#include "stream_iterator.h"

#include "misc.h"

namespace LogReader {

    // a character to replace 0x00 in the output buffer
    // replace \0 in the output by spaces
    static char CStreamIterator_replacement_char = ' ';

    CStreamIterator::CStreamIterator( void ) {
    }

    CStreamIterator::~CStreamIterator() {
        Close();
        delete m_stream_matcher;
    }

    bool CStreamIterator::Open( const char * filename ) {

        if ( m_filter_list == NULL ) {
            log( "Filter is not set\n" );
            return false;
        }

        if ( filename == NULL ) {
            log( "NULL filename\n" );
            return false;
        }

        if ( m_tail_data_storage == NULL ) {
            m_tail_data_storage = new ( std::nothrow ) char[ m_paged_data_offset ];
            if ( m_tail_data_storage == NULL ) {
                log( "Allocation failure\n" );
                return false;
            }
            memset( m_tail_data_storage, 0, m_paged_data_offset );
        }

        m_input_stream = new ( std::nothrow ) CAsyncInputStream( m_paged_data_offset );
        if ( m_input_stream == NULL ) {
            log( "Allocation failure\n" );
            return false;
        }

        bool bResult = m_input_stream->Open( filename );
        if ( bResult ) {
            m_bEOF = false;
            m_file_pos = 0;
            m_paged_data_position = m_page_size;
        }

        return bResult;
    }

    void CStreamIterator::Close() {
        m_paged_data_position = 0;
        m_page_size = 0;
        m_file_pos = 0;
        m_bEOF = true;
        m_stream_page = NULL;
        delete[] m_tail_data_storage;
        m_tail_data_storage = NULL;
        if ( m_input_stream != NULL ) {
            m_input_stream->Close();
            delete m_input_stream;
            m_input_stream = NULL;
        }
    }

    bool CStreamIterator::SetFilter( const CFilterList * filter_list ) {

        if ( m_input_stream && m_input_stream->IsOpened() ) {
            log( "File processing is already started\n" );
            return false;
        }

        m_filter_list = filter_list;

        // Set size of space allocated before m_stream_page equal to the longest fixed size filter sequence
        // to provide filter matcher with the transparent backtrack/literals matching buffer at page borders.
        m_paged_data_offset = 0;
        const CFilterItem * p = static_cast < CFilterItem * > ( m_filter_list->GetFirst() );
        while ( p != NULL ) {
            m_paged_data_offset = max( m_paged_data_offset, p->GetChainLength() );
            p = static_cast < CFilterItem * > ( p->GetNext() );
        }

        // re-create filter matcher with the new filter
        delete m_stream_matcher;
        m_stream_matcher = new ( std::nothrow ) CStreamMatcher( m_filter_list );

        return true;
    }

    void CStreamIterator::AddStringToOutputBuffer( const char * source, const size_t string_length_to_copy ) {
        
        if ( !m_bStringTooLong ) {

            // copy possibly matching string data from the file page buffer to the output buffer
            size_t copy_length = min( string_length_to_copy, m_output_buffer_length - m_output_buffer_position );
            memcpy( m_output_buffer + m_output_buffer_position, source, copy_length );
            if ( string_length_to_copy > copy_length ) {
                m_bStringTooLong = true;
            }
            
            // replace \0 in the output buffer
            if ( CStreamIterator_replacement_char != '\0' ) {
                char * p = m_output_buffer + m_output_buffer_position;
                char * p_end = p + copy_length;
                while ( p < p_end && ( p = reinterpret_cast < char * >( memchr( p, '\0', p_end - p ) ) ) != NULL ) {
                    *p = CStreamIterator_replacement_char;
                    p++;
                }
            }

            m_output_buffer_position += copy_length;
        }

    }

    void CStreamIterator::ConsumeCRLF( const bool bNew ) {

        if ( m_paged_data_position < m_page_size ) {

            size_t remaining_buffer = m_page_size - m_paged_data_position;
            if ( bNew ) {
                // no previous CRLF
                // consume first EOL character
                char c = *m_current_string_start;
                if ( c == CR || c == LF ) {
                    m_line_end_char = *m_current_string_start;
                    m_current_string_start++;
                    m_paged_data_position++;
                    remaining_buffer--;
                }
            }

            // consume second EOL character
            // it should differ from the first
            if ( remaining_buffer > 0 && ( m_line_end_char == CR || m_line_end_char == LF ) ) {
                char c = *m_current_string_start;
                if ( ( c == CR || c == LF ) && c != m_line_end_char ) {
                    m_current_string_start++;
                    m_paged_data_position++;
                }
                m_line_end_char = 0;
                m_bLineEnd = true;
            }

        }
    }

    bool CStreamIterator::GetNextLine( char * buf, const int bufsize ) {

        if ( m_input_stream == NULL || !m_input_stream->IsOpened() ) {
            // file is not opened
            log( "File is not opened\n" );
            return false;
        }

        if ( m_filter_list == NULL ) {
            // filter is not set
            log( "Filter is not set\n" );
            return false;
        }

        if ( buf == NULL ) {
            // buffer is NULL
            log( "NULL buffer value\n" );
            return false;
        }

        if ( bufsize <= 0 ) {
            // invalid bufsize
            log( "Invalid buffer size\n" );
            return false;
        }

        // initialize state variables
        m_output_buffer = buf;
        m_output_buffer_length = bufsize;
        m_output_buffer_position = 0;
        m_bLineEnd = true;
        m_bStringTooLong = false;
        m_match_result = CStreamMatcher::Match::MatchUnknown;
        m_stream_matcher->Reset();

        while ( !m_bEOF && !( m_match_result == CStreamMatcher::Match::HaveMatch && m_bLineEnd ) ) {

            if ( m_match_result != CStreamMatcher::Match::HaveMatch && m_bLineEnd ) {
                // re-initialize some state variables afer the end of non-matching line
                m_output_buffer_position = 0;
                m_bStringTooLong = false;
                m_bLineEnd = false;
                m_line_end_char = 0;
                m_stream_matcher->Reset();
#ifdef _DEBUG
                memset( buf, 0, bufsize );
#endif // _DEBUG
            }

            if ( m_stream_page == NULL || m_paged_data_position >= m_page_size ) {

                // read new page from file 

                // debug
                //if ( m_file_pos == 0x0 ) {
                //    m_file_pos++;
                //    m_file_pos--;
                //}

                // get the buffer, asynchronously read from the file
                m_input_stream->GetPage( m_stream_page, m_page_size );
                if ( m_stream_page == NULL ) {
                    log( "Invalid page buffer\n" );
                    *buf = '\0';
                    return false;
                }

                m_file_pos += m_page_size;
                // debug
                //if ( m_file_pos == 0x0 ) {
                //    m_file_pos++;
                //    m_file_pos--;
                //}

                m_current_string_start = m_stream_page;
                
                if ( m_page_size == 0 ) {
                    // error reading file or eof
                    m_bEOF = true;
                }
                m_paged_data_position = 0;
                m_bEndOfBuffer = false;
                m_p_next_CRLF = NULL;
                
                // check for the EOL tail at the page border
                ConsumeCRLF( false );
                if ( m_bLineEnd ) {
                    continue;
                }

            } // read new page from file 

            if ( m_bEOF ) {
                
                // finalize last string matching on EOF
                m_stream_matcher->ProcessBuffer( 
                    m_stream_page - m_paged_data_offset, 
                    m_paged_data_offset, 
                    m_paged_data_offset, 
                    true
                );

            } else {

                // debug
                if ( m_file_pos >= 0x1000 && m_paged_data_position >= 0xfdb ) {
                    m_file_pos++;
                    m_file_pos--;
                }

                // search for the next EOL
                if ( !m_bEndOfBuffer ) {
                    m_p_next_CRLF = reinterpret_cast < const char * >( memchr(
                        m_current_string_start,
                        LF,
                        m_page_size - m_paged_data_position
                    ) );
                    if ( m_p_next_CRLF != NULL && m_p_next_CRLF > m_current_string_start && *( m_p_next_CRLF - 1 ) == CR ) {
                        m_p_next_CRLF--;
                    }
                    m_bEndOfBuffer = ( m_p_next_CRLF == NULL );
                    if ( !m_bEndOfBuffer ) {
                        // line at the end of the buffer with possibly CR in the current buffer and LF in another
                        if ( m_p_next_CRLF != NULL && m_paged_data_position + ( m_p_next_CRLF - m_current_string_start ) + 1 == m_page_size ) {
                            m_bEndOfBuffer = true;
                        }
                    }
                }

                size_t buffer_length_to_process = ( m_p_next_CRLF == NULL ) ? m_page_size - m_paged_data_position : m_p_next_CRLF - m_current_string_start;

                if ( m_stream_matcher->GetMatch() == CStreamMatcher::Match::MatchUnknown ) {
                    // process data with matcher only if the match result is not yet determined
                    m_stream_matcher->ProcessBuffer(
                        m_stream_page - m_paged_data_offset,
                        m_paged_data_offset + m_paged_data_position + buffer_length_to_process,
                        m_paged_data_offset + m_paged_data_position,
                        !m_bEndOfBuffer
                    );
                }

                m_match_result = m_stream_matcher->GetMatch();
                if ( m_match_result != CStreamMatcher::Match::NoMatch ) {
                    // if no NoMatch then it is possible that it is Match or will be Match later
                    // therefore we should store the data to the output buffer
                    AddStringToOutputBuffer( m_current_string_start, buffer_length_to_process );
                }

                m_current_string_start += buffer_length_to_process;
                m_paged_data_position += buffer_length_to_process;

                // debug
                if ( m_file_pos >= 0x1000 && m_paged_data_position >= 0xfd0 ) {
                    m_file_pos++;
                    m_file_pos--;
                }

                ConsumeCRLF( true );

                // debug
                if ( m_file_pos >= 0x1000 && m_paged_data_position >= 0xfdb ) {
                    m_file_pos++;
                    m_file_pos--;
                }

                if ( !m_bEndOfBuffer && m_paged_data_position >= m_page_size ) {
                    m_bEndOfBuffer = true;
                }

            }

        }

        if ( m_match_result == CStreamMatcher::Match::HaveMatch ) {
            // collect leaking page border CR's here
            // to simplify processing CR<page_border>LF we just declare that all CR's at the line end are non-significant
            while ( m_output_buffer_position > 0 && m_output_buffer[ m_output_buffer_position - 1 ] == CR ) {
                m_output_buffer_position--;
            }
        }

        // add \0 terminator to the output if there are space left
        if ( m_match_result == CStreamMatcher::Match::HaveMatch && m_output_buffer_position < m_output_buffer_length ) {
            m_output_buffer[ m_output_buffer_position ] = '\0';
        }

        return m_match_result == CStreamMatcher::Match::HaveMatch;
    }

} // namespace LogReader
