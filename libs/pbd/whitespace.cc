#include <pbd/whitespace.h>

using namespace std;

void
strip_whitespace_edges (string& str)
{   
    string::size_type i; 
    string::size_type len;    
    string::size_type s;
			            
    len = str.length();

    /* strip front */
				        
    for (i = 0; i < len; ++i) {
        if (isgraph (str[i])) {
            break;
        }
    }

    /* strip back */
    
    if (len > 1) {
    
	    s = i;
	    i = len - 1;
	    
	    do {
		    if (isgraph (str[i]) || i == 0) {
			    break;
		    }

		    --i;

	    } while (true); 
	    
	    str = str.substr (s, (i - s) + 1);

    } else {
	    str = str.substr (s);
    }
}

