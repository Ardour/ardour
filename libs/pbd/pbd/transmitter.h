/*
    Copyright (C) 1998-99 Paul Barton-Davis 

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

#ifndef __libmisc_transmitter_h__
#define __libmisc_transmitter_h__

#include <sstream>
#include <iostream>

#include <sigc++/sigc++.h>

#include "pbd/libpbd_visibility.h"

class LIBPBD_API Transmitter : public std::stringstream

{
  public:
	enum Channel {
		Info,
		Error,
		Warning,
		Fatal,
		Throw
	};

	Transmitter (Channel);

	sigc::signal<void,Channel, const char *> &sender() { 
		return *send;
	}

	bool does_not_return ();

  protected:
	virtual void deliver ();
	friend std::ostream& endmsg (std::ostream &);

  private:
	Channel channel;
	sigc::signal<void, Channel, const char *> *send;

	sigc::signal<void, Channel, const char *> info;
	sigc::signal<void, Channel, const char *> warning;
	sigc::signal<void, Channel, const char *> error;
	sigc::signal<void, Channel, const char *> fatal;
};

/* for EGCS 2.91.66, if this function is not compiled within the same
   compilation unit as the one where a ThrownError is thrown, then 
   nothing will catch the error. This is a pretty small function, so
   inlining it here seems like a reasonable workaround.
*/

inline std::ostream &
endmsg (std::ostream &ostr)

{
	Transmitter *t;

	/* There is a serious bug in the Cygnus/GCC libstdc++ library:
	   cout is not actually an ostream, but a trick was played
	   to make the compiler think that it is. This will cause
	   the dynamic_cast<> to fail with SEGV. So, first check to
	   see if ostr == cout, and handle it specially.
	*/

	if (&ostr == &std::cout) {
		std::cout << std::endl;
		return ostr;
	} else if (&ostr == &std::cerr) {
		std::cerr << std::endl;
		return ostr;
	}

	if ((t = dynamic_cast<Transmitter *> (&ostr)) != 0) {
		t->deliver ();
	} else {
		/* hmm. not a Transmitter, so just put a newline on
		   it and assume that that will be enough.
		*/
		
		ostr << std::endl;
	}

	return ostr;
}


extern "C" { LIBPBD_API void pbd_c_error (const char *); }

#endif // __libmisc_transmitter_h__
