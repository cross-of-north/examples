#include "..\common.h"
#pragma hdrstop

#include "sdf.h"

#include <iostream>
#include <locale>
#include "debtrap.h"
#include "GetOSVar.h"

// TSDFParser *****************************************************************************

TSDFParser::TSDFParser( void )
  : m_c( '\0' )
  , m_state( psNewLine )
  , m_bError( false )
{
}

TSDFParser::~TSDFParser() {
}

void TSDFParser::SetState( ParserStates state ) {
  m_state = state;
  m_token = m_current_token;
  m_current_token = "";
}

void TSDFParser::Collect( void ) {
  m_current_token += m_c;
  m_token = m_current_token;
}

bool TSDFParser::IsNewline( void ) {
  bool bResult = ( ( m_c == '\r' ) || ( m_c == '\n' ) );
  if ( bResult ) {
    SetState( psNewLine );
  }
  return bResult;
}

bool TSDFParser::IsWhitespace( void ) {
  bool bResult = ( ( m_c == ' ' ) || ( m_c == '\t' ) || ( m_c == '\r' ) || ( m_c == '\n' ) );
  return bResult;
}

std::string stringToUpper(std::string myString)
{
//  static ToUpper up(std::locale::classic());
  std::transform(myString.begin(), myString.end(), myString.begin(), toupper);
  return myString;
}

void TSDFParser::OnError( const char * s ) {
  m_bError = true;
  DebugTrap( s );
  Application->MessageBox( s, "Error", MB_OK | MB_ICONSTOP );
}

void TSDFParser::Parse( std::istream & is ) {

  bool bContinue = true;
  m_bError = false;

  while ( bContinue && !m_bError ) {
    is.read( &m_c, 1 );
    bContinue = ( is.good() && !is.eof() );
    if ( !bContinue ) {
      // newline to close any state
      m_c = '\r';
    }
    switch ( m_state ) {

      case psNewLine : {
        if ( !IsWhitespace() && !IsNewline() ) {
          switch ( m_c ) {
            case '[' : {
              SetState( psSectionStarted );
              break;
            }
            case '.' : {
              SetState( psKeyWord );
              break;
            }
            case ';' : {
              SetState( psComment );
              break;
            }
            default : {
              SetState( psName );
              Collect();
              break;
            }
          }
        }
        break;
      }

      case psComment : {
        if ( !IsNewline() ) {
          // ignore any symbol
        }
        break;
      }

      case psSectionStarted : {
        if ( IsNewline() ) {
          Error( "newline in the middle of the section" );
        } else {
          switch ( m_c ) {
            case ']' : {
              SetState( psSectionFinished );
              OnSection( m_token );
              break;
            }
            default : {
              Collect();
              break;
            }
          }
        }
        break;
      }

      case psSectionFinished : {
        if ( !IsNewline() ) {
          // ignore any symbol
          Error( "no symbols are allowed after ']'" );
          SetState( psComment );
        }
        break;
      }

      case psKeyWord : {
        if ( IsNewline() ) {
          Error( "newline instead of keyword" );
        } else if ( IsWhitespace() ) {
          SetState( psSpaceAfterKeyWord );
          OnKeyWord( m_token );
        } else {
          Collect();
        }
        break;
      }

      case psSpaceAfterKeyWord : {
        if ( IsNewline() ) {
          Error( "newline instead of name" );
        } else if ( !IsWhitespace() ) {
          SetState( psName );
          Collect();
        }
        break;
      }

      case psName : {
        if ( IsNewline() ) {
          Error( "name with no assignment" );
        } else {
          switch ( m_c ) {
            case '=' : {
              SetState( psValue );
              OnName( m_token );
              break;
            }
            default : {
              Collect();
              break;
            }
          }
        }
        break;
      }

      case psValue : {
        if ( IsNewline() ) {
          OnValue( m_token );
        } else {
          Collect();
        }
        break;
      }

      default : {
        Error( "Invalid state" );
        break;
      }
    }

  }
}

void TSDFParser::OnSection( const std::string & s ) {
  s;
}

void TSDFParser::OnKeyWord( const std::string & s ) {
  s;
}

void TSDFParser::OnName( const std::string & s ) {
  s;
}

void TSDFParser::OnValue( const std::string & s ) {
  s;
}

bool TSDFParser::IsError( void ) {
  return m_bError;
}

// TSDFExpander ******************************************************************************

TSDFExpander::TSDFExpander( const TSDFVariables & var )
  : m_variables( var )
{
}

TSDFExpander::~TSDFExpander() {
}

std::string TSDFExpander::GetValue( const std::string name_ ) {
  bool bFound = false;
  std::string name = stringToUpper( name_ );
  std::string value = m_variables.Get( name, &bFound );
  if ( !bFound ) {
    value = GetOSVariable( name );
  }
  return value;
}

std::string TSDFExpander::Expand( const std::string s ) {
  std::string result;
  int len = s.length();
  int i = 0;
  int last_pos = -1;
  while ( i < len ) {
    char c = s[ i ];
    if ( c == '%' ) {
      if ( last_pos < 0 ) {
        last_pos = i;
      } else {
        if ( last_pos == i - 1 ) {
          // "%%"
          result += '%';
        } else {
          std::string name = stringToUpper( s.substr( last_pos + 1, i - ( last_pos + 1 ) ) );
          std::string value = GetValue( name );
          result += value;
        }
        last_pos = -1;
      }
    } else if ( last_pos < 0 ) {
      result += c;
    }
    i++;
  }
  return result;
}

// TSDFProcessor ******************************************************************************

TSDFProcessor::TSDFProcessor( std::string force_section, TSDFVariables::list * all_sections_list )
  : TSDFParser()
  , TSDFExpander( m_current_variables )
  , m_bSkipSection( true )
  , m_bFinished( false )
  , m_force_section( stringToUpper( force_section ) )
  , m_all_sections_list( all_sections_list )
{
  DebugTrapAssert( !( !m_force_section.empty() && m_all_sections_list != NULL ), "incompartible modes" );
}

TSDFProcessor::~TSDFProcessor()
{
}

void TSDFProcessor::OnSectionEnd( void ) {
  if ( !m_bFinished ) {
    if ( m_bSkipSection ) {
      // wrong section
      m_current_variables = m_original_variables;
    } else {
      // right section
      m_current_variables.Set( SDF_SECTION, m_section );
      if ( m_all_sections_list == NULL ) {
        m_bFinished = true;
      } else {
        m_all_sections_list->push_back( m_section );
      }
    }
    m_bSkipSection = false;
  }
}

void TSDFProcessor::OnSection( const std::string & s ) {
  OnSectionEnd();
  if ( !m_bFinished ) {
    ResetLine();
    m_section = s;
    if ( !m_force_section.empty() && stringToUpper( m_section ) != m_force_section ) {
      // wrong section
      m_bSkipSection = true;
    }
  }
}

void TSDFProcessor::ResetLine( void ) {
  m_keyword = kwNone;
  m_name = "";
  m_value = "";
}

void TSDFProcessor::OnKeyWord( const std::string & s ) {
  if ( !m_bFinished && !m_bSkipSection ) {
    std::string keyword = stringToUpper( s );
    m_keyword = kwNone;
    if ( keyword == "IF" ) {
      m_keyword = kwIf;
    } else {
      Error( "unknown keyword" );
    }
  }
}

void TSDFProcessor::OnName( const std::string & s ) {
  if ( !m_bFinished && !m_bSkipSection ) {
    if ( m_keyword == kwNone ) {
      ResetLine();
    }
    m_name = s;
  }
}

void TSDFProcessor::OnValue( const std::string & s ) {
  if ( !m_bFinished && !m_bSkipSection ) {
    m_value = s;
    switch ( m_keyword ) {
      case kwIf : {
        if ( m_force_section.empty() ) {
          std::string s1 = Expand( m_name );
          s1 = stringToUpper( s1 );
          std::string s2 = Expand( m_value );
          s2 = stringToUpper( s2 );
          TSDFVariables::list values;
          TSDFVariables::Explode( values, s2, "|" );
          TSDFVariables::list::iterator i = values.begin();
          bool bEqual = false;
          while ( i != values.end() && !bEqual ) {
            bEqual = ( s1 == *i );
            i++;
          }
          if ( !bEqual ) {
            // wrong section
            m_bSkipSection = true;
          }
        } else {
          // ignore conditions if section is explicitly specified
        }
        break;
      }
      case kwNone : {
        m_current_variables.Set( stringToUpper( m_name ), Expand( m_value ) );
        break;
      }
      default : {
        DebugTrap( "invalid enum value" );
        break;
      }
    }
    m_keyword = kwNone;
  }
}

const TSDFVariables & TSDFProcessor::Process( TSDFVariables & var, std::istream & is ) {
  m_bFinished = false;
  m_original_variables = var;
  Parse( is );
  OnSectionEnd(); // closing last section
  if ( !m_force_section.empty() && !m_bFinished ) {
    Error( "Section \"" + m_force_section + "\" is not found" );
  }
  return m_bFinished ? m_current_variables : m_original_variables;
}

