#ifndef TCAA_SDF_H
#define TCAA_SDF_H

#include "sdfvar.h"
#include <vector>

#define SDF_SECTION "SDF_SECTION"

// Разбор SDF-файла. Наследники используют виртуальные функции для обработки данных.
// После вызова Error() разбор прекращается.

class TSDFParser {

  protected:

    enum ParserStates {
      psNewLine, // to psNewLine, psSectionStarted, psComment, psKeyWord, psName, whitespace allowed
      psComment, // to psNewLine
      psSectionStarted, // to psSectionFinished
      psSectionFinished, // to psNewLine
      psKeyWord, // to psSpaceAfterKeyWord
      psSpaceAfterKeyWord, // to psValue
      psName, // to psValue
      psValue // to psNewLine
    };

    char m_c;
    ParserStates m_state;
    std::string m_current_token;
    std::string m_token;
    bool m_bError;

    void SetState( ParserStates state );
    void Collect( void );
    bool IsNewline( void );
    bool IsWhitespace( void );

    virtual void OnSection( const std::string & s );
    virtual void OnKeyWord( const std::string & s );
    virtual void OnName( const std::string & s );
    virtual void OnValue( const std::string & s );
    virtual void OnError( const char * s );
    void Error( const char * s ) { OnError( s ); }
    void Error( const std::string & s ) { OnError( s.c_str() ); }

  public:

    TSDFParser( void );
    virtual ~TSDFParser();

    void Parse( std::istream & is );

    bool IsError( void );

};


// Распахивает %% последовательности, используя список переменных.

class TSDFExpander {

  protected:

    const TSDFVariables & m_variables;
    virtual std::string GetValue( const std::string name );

  public:

    TSDFExpander( const TSDFVariables & var );
    ~TSDFExpander();

    std::string Expand( const std::string s );

};


// Анализирует SDF-файл и соответственно заполняет список переменных.

class TSDFProcessor : public TSDFParser, public TSDFExpander {

  protected:

    enum KeyWords {
      kwNone,
      kwIf
    };

    std::string m_section;
    KeyWords m_keyword;
    std::string m_name;
    std::string m_value;
    TSDFVariables m_original_variables;
    TSDFVariables m_current_variables;
    bool m_bSkipSection;
    bool m_bFinished;
    std::string m_force_section;
    TSDFVariables::list * m_all_sections_list;

    virtual void OnSection( const std::string & s );
    virtual void OnKeyWord( const std::string & s );
    virtual void OnName( const std::string & s );
    virtual void OnValue( const std::string & s );
    void OnSectionEnd( void );

    void ResetLine( void );

  public:

    TSDFProcessor( std::string force_section, TSDFVariables::list * all_sections_list );
    virtual ~TSDFProcessor();

    const TSDFVariables & Process( TSDFVariables & var, std::istream & is );

};

#endif // TCAA_SDF_H

