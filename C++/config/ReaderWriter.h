#ifndef READERWRITER_483728422_H
#define READERWRITER_483728422_H

#include <vector>

class TConfigTree;
class TConfigData;
class TConfigNode;
class TIniIterator;

namespace Comctrls {
  class TListItems;
}

typedef std::vector < TConfigData * > TReaderWriterList;

class TIniFile;

class TIniWriter {

  protected:

    const TConfigTree * m_meta_tree;
    const TReaderWriterList * m_data;
    TReaderWriterList * m_deleted_data;
    TIniIterator * m_iterator;

    bool WriteData( const TConfigData * data, AnsiString & error );
    void CommitData( const TConfigData * data );
    bool WriteSDF( AnsiString & error );
    bool ProcessDeleted( AnsiString & error );
    bool WriteMisc( AnsiString & error );
    void CommitMisc( void );
    void CommitDeleted( void );

    bool InvalidateReferences( AnsiString & error );

  public:

    TIniWriter( const TConfigTree * meta_tree, const TReaderWriterList * data, TReaderWriterList * deleted_data );
    ~TIniWriter();
    bool Write( void );

};

class TIniReader {

  protected:

    const TConfigTree * m_meta_tree;
    TReaderWriterList * m_data;
    TIniIterator * m_iterator;

    bool ReadSDF( AnsiString & error );
    bool ReadMisc( AnsiString & error );
    void UpdateFilter( void );

  public:

    TIniReader( const TConfigTree * meta_tree, TReaderWriterList * data );
    ~TIniReader();

    bool Read( void );
    bool ReadData( TConfigData * data, AnsiString & error );

};

std::string ExpandFileName( const std::string name, const std::string app, const TConfigData * data, AnsiString & error );

#endif // READERWRITER_483728422_H
