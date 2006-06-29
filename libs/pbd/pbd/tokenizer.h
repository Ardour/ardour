#ifndef PBD_TOKENIZER
#define PBD_TOKENIZER

#include <iterator>
#include <string>

namespace PBD {

/**
    Tokenize string, this should work for standard
    strings aswell as Glib::ustring. This is a bit of a hack,
    there are much better string tokenizing patterns out there.
*/
template<typename StringType, typename Iter>
unsigned int
tokenize(const StringType& str,        
        const StringType& delims,
        Iter it)
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
            *it++ = str.substr(start_pos, end_pos - start_pos);
            ++token_count;
            start_pos = str.find_first_not_of(delims, end_pos + 1);
        }
    } while (start_pos != str.npos);

    if (start_pos != str.npos) {
        *it++ = str.substr(start_pos, str.length() - start_pos);
        ++token_count;
    }

    return token_count;
}

} // namespace PBD

#endif // PBD_TOKENIZER


