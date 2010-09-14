/*
    Copyright (C) 2000-2007 Paul Davis 

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "pbd/whitespace.h"

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
        if (!isspace (str[i])) {
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
		    if (!isspace (str[i]) || i == 0) {
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
