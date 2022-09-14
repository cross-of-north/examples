#include "pch.h"

#include "misc.h"

namespace LogReader {

	log_function_t log_sink = NULL;
	void set_log_sink( log_function_t f ) {
		log_sink = f;
	}

	void log( const char * s ) {
		if ( log_sink != NULL ) {
			log_sink( s );
		}
	}

	const char * memmem( const char * haystack, size_t haystack_length, const char * needle, size_t needle_length ) {
		if ( haystack == NULL || needle == NULL || needle_length > haystack_length ) {
			return NULL;
		}
		if ( needle_length == 0 ) {
			return haystack;
		}
		const char * result = NULL;
		const char * p = NULL;
		do {
			// repeatedly find first character of the needle in a haystack
			p = reinterpret_cast < const char * > ( memchr( haystack, *needle, haystack_length ) );
			if ( p != NULL ) {
				haystack_length -= p - haystack;
				// start from the next position next iteration
				haystack_length--;
				if ( haystack_length < needle_length - 1 ) {
					// remaining needle can't fit at this position
					p = NULL;
				} else {
					// start from the next position here and next iteration
					haystack = p + 1;
					// checking if remaining needle is here
					if ( memcmp( haystack, needle + 1, needle_length - 1 ) == 0 ) {
						// the needle is here
						result = p;
					}
				}
			}
		} while ( result == NULL && p != NULL );

		return result;
	}

} // namespace LogReader
