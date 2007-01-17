#include <pbd/whitespace.h>

using namespace std;

namespace PBD {
	
void
strip_whitespace_edges (string& str)
{   
    string::size_type i; 
    string::size_type len;    
    string::size_type s = 0;
			            
    len = str.length();

    if (len == 1) {
	    return;
    }

    /* strip front */
				        
    for (i = 0; i < len; ++i) {
        if (isgraph (str[i])) {
            break;
        }
    }

    if (i == len) {
	    /* it's all whitespace, not much we can do */
		str = "";
	    return;
    }

    /* strip back */
    
    if (len > 1) {
    
	    s = i;
	    i = len - 1;

	    if (s == i) {
		    return;
	    }
	    
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

} // namespace PBD
