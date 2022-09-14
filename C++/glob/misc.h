#pragma once

namespace LogReader {

#define ANY_CHAR '?'
#define ANY_LENGTH '*'
#define ESCAPE '\\'
#define ESCAPED_ZERO '0'
#define LF '\n'
#define CR '\r'

#undef max
#undef min

	template <typename T>
	T max( T a, T b ) {
		return a > b ? a : b;
	}

	template <typename T>
	T min( T a, T b ) {
		return a < b ? a : b;
	}

	// log sink typedef
	typedef void ( *log_function_t )( const char * s );
	// set log sink
	void set_log_sink( log_function_t f );

	// log sink output
	void log( const char * s );

	// Like strstr, but treats zero as non-special character.
	const char * memmem( const char * haystack, size_t haystack_length, const char * needle, size_t needle_length );

} // namespace LogReader
