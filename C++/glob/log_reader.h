#pragma once

class CLogReaderImp;

/**
* The public interface of the log reader class.
*/
class CLogReader {

private:

    CLogReaderImp * m_imp; // an implementation of the log reader mechanics

public:
    CLogReader(void);
    ~CLogReader();

    /**
    * opens the file, returns false on error
    */
    bool Open( const char * filename );
    
    /**
    * closes the file
    */
    void Close();

    /**
    * sets the filter value, returns false on error
    */
    bool SetFilter( const char * filter );
    
    /**
    * request the next found line
    * buf - buffer, bufsize - max length
    * false - false - file end or error
    */
    bool GetNextLine( char * buf, const int bufsize );  
                             
};
