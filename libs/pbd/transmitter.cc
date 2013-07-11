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

    $Id$
*/

#include <cstdlib>
#include <signal.h>
#include <string>

#include "pbd/transmitter.h"
#include "pbd/error.h"

using std::string;
using std::ios;

Transmitter::Transmitter (Channel c)
{
	channel = c;
	switch (c) {
	case Error:
		send = &error;
		break;
	case Warning:
		send = &warning;
		break;
	case Info:
		send = &info;
		break;
	case Fatal:
		send = &fatal;
		break;
	case Throw:
		/* we should never call Transmitter::deliver
		   for thrown messages (because its overridden in the
		   class heirarchy). force a segv if we do.
		*/
		send = 0;
		break;
	}
}		

void
Transmitter::deliver ()

{
	string foo;

	/* NOTE: this is just a default action for a Transmitter or a
	   derived class. Any class can override this to produce some
	   other action when deliver() is called. 
	*/

	*this << '\0';

	/* send the SigC++ signal */

	foo = str();
	(*send) (channel, foo.c_str());

	/* XXX when or how can we delete this ? */
	// delete foo;

	/* return to a pristine state */

	clear ();
	seekp (0, ios::beg);
	seekg (0, ios::beg);

	/* do the right thing if this should not return */
	
	if (does_not_return()) {
#ifndef WIN32
		sigset_t mask;
		
		sigemptyset (&mask);
		sigsuspend (&mask);
#endif
		/*NOTREACHED*/
		exit (1);
	} 
}

bool
Transmitter::does_not_return ()

{
	if (channel == Fatal || channel == Throw) {
		return true;
	} else {
		return false;
	}
}


extern "C" {
  void pbd_c_error (const char *str)
 
  {
	PBD::error << str << endmsg;
  }
}
