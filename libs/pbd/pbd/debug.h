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

#include <bitset>
#include <stdint.h>

#include <sstream>

#include "pbd/libpbd_visibility.h"
#include "pbd/timing.h"

#if defined(COMPILER_MSVC) && !defined(PTW32_VERSION)
#include <ardourext/pthread.h> // Gets us 'PTW32_VERSION'
#endif

namespace PBD {

	typedef std::bitset<128> DebugBits;

	LIBPBD_API extern DebugBits debug_bits;
	LIBPBD_API DebugBits new_debug_bit (const char* name);
	LIBPBD_API void debug_print (const char* prefix, std::string str);
	LIBPBD_API void set_debug_bits (DebugBits bits);
	LIBPBD_API int parse_debug_options (const char* str);
	LIBPBD_API void list_debug_options ();

	namespace DEBUG {

		/* this namespace is so that we can write DEBUG::bit_name */

		LIBPBD_API extern DebugBits Stateful;
		LIBPBD_API extern DebugBits Properties;
		LIBPBD_API extern DebugBits FileManager;
		LIBPBD_API extern DebugBits Pool;
		LIBPBD_API extern DebugBits EventLoop;
		LIBPBD_API extern DebugBits AbstractUI;
		LIBPBD_API extern DebugBits Configuration;
		LIBPBD_API extern DebugBits FileUtils;
		LIBPBD_API extern DebugBits UndoHistory;
		LIBPBD_API extern DebugBits Timing;
		LIBPBD_API extern DebugBits Threads;
		LIBPBD_API extern DebugBits Locale;
		LIBPBD_API extern DebugBits StringConvert;

		/* See notes in ../debug.cc on why these are defined here */

                LIBPBD_API extern DebugBits WavesMIDI;
                LIBPBD_API extern DebugBits WavesAudio;
	}
}

#ifndef NDEBUG
#define DEBUG_TRACE(bits,str) if (((bits) & PBD::debug_bits).any()) { PBD::debug_print (# bits, str); }
#define DEBUG_STR_DECL(id) std::stringstream __debug_str ## id;
#define DEBUG_STR(id) __debug_str ## id
#define DEBUG_STR_APPEND(id,s) __debug_str ## id << s;
#define DEBUG_ENABLED(bits) (((bits) & PBD::debug_bits).any())
#ifdef PTW32_VERSION
#define DEBUG_THREAD_SELF pthread_self().p
#else
#define DEBUG_THREAD_SELF pthread_self()
#endif

#define DEBUG_TIMING_START(bits,td) if (DEBUG_ENABLED (bits)) { td.start_timing (); }
#define DEBUG_TIMING_ADD_ELAPSED(bits,td) if (DEBUG_ENABLED (bits)) { td.add_elapsed (); }
#define DEBUG_TIMING_RESET(bits,td) if (DEBUG_ENABLED (bits)) { td.reset (); }

#else
#define DEBUG_TRACE(bits,fmt,...) /*empty*/
#define DEBUG_STR(a) /* empty */
#define DEBUG_STR_APPEND(a,b) /* empty */
#define DEBUG_ENABLED(b) (0)
#define DEBUG_THREAD_SELF 0

#define DEBUG_TIMING_START(bits,td) /*empty*/
#define DEBUG_TIMING_ADD_ELAPSED(bits,td) /*empty*/
#define DEBUG_TIMING_RESET(bits,td) /*empty*/

#endif
#endif /* __libpbd_debug_h__ */

