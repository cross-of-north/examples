#include "pch.h"

#include "log_reader.h"

#include "stream_iterator.h"
#include "misc.h"

/**
* An implementation of the log reader mechanics.
*/
class CLogReaderImp {

private:

	LogReader::CFilterList m_filter_list; // filter to process file with
	LogReader::CStreamIterator m_file_matcher; // file processing mechanics

public:

	CLogReaderImp( void ) {
	}

	~CLogReaderImp() {
	}

	bool Open( const char * filename ) {
		return m_file_matcher.Open( filename );
	}

	void Close() {
		m_file_matcher.Close();
	}

	bool SetFilter( const char * filter ) {
		return
			// parse the filter string filling filter structures
			m_filter_list.SetFilter( filter )
			&&
			// then pass the filter to the file processor
			m_file_matcher.SetFilter( &m_filter_list );
	}

	bool GetNextLine( char * buf, const int bufsize ) {
		return m_file_matcher.GetNextLine( buf, bufsize );
	}

};


CLogReader::CLogReader( void )
	: m_imp( new ( std::nothrow ) CLogReaderImp() )
{
	if ( !m_imp ) {
		LogReader::log( "Allocation failure\n" );
	}
}

CLogReader::~CLogReader() {
	delete m_imp;
}

bool CLogReader::Open( const char * filename ) {
	if ( !m_imp ) {
		return false;
	}
	return m_imp->Open( filename );
}

void CLogReader::Close() {
	if ( !m_imp ) {
		return;
	}
	m_imp->Close();
}

bool CLogReader::SetFilter( const char * filter ) {
	if ( !m_imp ) {
		return false;
	}
	return m_imp->SetFilter( filter );
}

bool CLogReader::GetNextLine( char * buf, const int bufsize ) {
	if ( !m_imp ) {
		return false;
	}
	return m_imp->GetNextLine( buf, bufsize );
}
