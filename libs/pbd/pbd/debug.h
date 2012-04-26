/*
    Copyright (C) 2009 Paul Davis

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

#ifndef __libpbd_debug_h__
#define __libpbd_debug_h__

#include <stdint.h>

#include <sstream>

namespace PBD {

	extern uint64_t debug_bits;
        uint64_t new_debug_bit (const char* name);
	void debug_print (const char* prefix, std::string str);
	void set_debug_bits (uint64_t bits);
	int parse_debug_options (const char* str);
	void list_debug_options ();

	namespace DEBUG {

		/* this namespace is so that we can write DEBUG::bit_name */
                
                extern uint64_t Stateful;
                extern uint64_t Properties;
		extern uint64_t FileManager;
		extern uint64_t Pool;
		extern uint64_t EventLoop;
		extern uint64_t AbstractUI;
	}
}

#ifndef NDEBUG
#define DEBUG_TRACE(bits,str) if ((bits) & PBD::debug_bits) { PBD::debug_print (# bits, str); }
#define DEBUG_STR_DECL(id) std::stringstream __debug_str ## id;
#define DEBUG_STR(id) __debug_str ## id
#define DEBUG_STR_APPEND(id,s) __debug_str ## id << s;
#define DEBUG_ENABLED(bits) ((bits) & PBD::debug_bits)
#else
#define DEBUG_TRACE(bits,fmt,...) /*empty*/
#define DEBUG_STR(a) /* empty */
#define DEBUG_STR_APPEND(a,b) /* empty */
#define DEBUG_ENABLED(b) (0)
#endif

#endif /* __libpbd_debug_h__ */

