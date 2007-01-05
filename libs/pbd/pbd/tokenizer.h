#ifndef PBD_TOKENIZER
#define PBD_TOKENIZER

#include <iterator>
#include <string>

#include <pbd/whitespace.h>

namespace PBD {

/**
    Tokenize string, this should work for standard
    strings as well as Glib::ustring. This is a bit of a hack,
    there are much better string tokenizing patterns out there.
	If strip_whitespace is set to true, tokens will be checked to see
	that they still have a length after stripping.  If no length, they
	are discarded.
*/
template<typename StringType, typename Iter>
unsigned int
tokenize(const StringType& str,        
        const StringType& delims,
        Iter it,
		bool strip_whitespace=false)
{
    typename StringType::size_type start_pos = 0;
    typename StringType::size_type end_pos = 0;
    unsigned int token_count = 0;

    do {
        start_pos = str.find_first_not_of(delims, start_pos);
        end_pos = str.find_first_of(delims, start_pos);
        if (start_pos != end_pos) {
            if (end_pos == str.npos) {
                end_pos = str.length();
            }
	    	if (strip_whitespace) {
				StringType stripped = str.substr(start_pos, end_pos - start_pos);
				strip_whitespace_edges (stripped);
				if (stripped.length()) {
					*it++ = stripped;
				}
			} else {
            	*it++ = str.substr(start_pos, end_pos - start_pos);
			}
            ++token_count;
            start_pos = str.find_first_not_of(delims, end_pos + 1);
        }
    } while (start_pos != str.npos);

    if (start_pos != str.npos) {
    	if (strip_whitespace) {
			StringType stripped = str.substr(start_pos, str.length() - start_pos);
			strip_whitespace_edges (stripped);
			if (stripped.length()) {
				*it++ = stripped;
			}
		} else {
        	*it++ = str.substr(start_pos, str.length() - start_pos);
		}
        ++token_count;
    }

    return token_count;
}

} // namespace PBD

#endif // PBD_TOKENIZER


