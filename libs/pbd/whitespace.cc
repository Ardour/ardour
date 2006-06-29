#include <pbd/whitespace.h>

using namespace std;

void
strip_whitespace_edges (string& str)
{   
    string::size_type i; 
    string::size_type len;    
	string::size_type s;
			            
    len = str.length();
				        
    for (i = 0; i < len; ++i) {
        if (isgraph (str[i])) {
            break;
        }
    }

    s = i;

    for (i = len - 1; i >= 0; --i) {
        if (isgraph (str[i])) {
            break;
        }
    }

    str = str.substr (s, (i - s) + 1);
}

