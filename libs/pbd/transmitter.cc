/*
 * Copyright (C) 1998-2015 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015-2019 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
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
	case Debug:
		send = &debug;
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
	/* NOTE: this is just a default action for a Transmitter or a
	   derived class. Any class can override this to produce some
	   other action when deliver() is called.
	*/

	*this << '\0';

	/* send the SigC++ signal */

	(*send) (channel, str().c_str());

	/* XXX when or how can we delete this ? */
	// delete foo;

	/* return to a pristine state */

	clear ();
	seekp (0, ios::beg);
	seekg (0, ios::beg);

	/* do the right thing if this should not return */

	if (does_not_return()) {
#ifndef PLATFORM_WINDOWS
// TODO !!!! Commented out temporarily (for Windows)
		sigset_t mask;

		sigemptyset (&mask);
		sigsuspend (&mask);
		/*NOTREACHED*/
		exit (EXIT_FAILURE);
/* JE - From what I can tell, the above code suspends
 * program execution until (any) signal occurs. Not
 * sure at the moment what this achieves, unless it
 * provides some time for the user to see the message.
 */
#endif
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
