/*
 * Copyright (C) 1998-2015 Paul Davis <paul@linuxaudiosystems.com>
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

#include <iostream>
#include <cstdlib>

#include "pbd/textreceiver.h"

using namespace std;

TextReceiver::TextReceiver (const string &n)

{
	name = n;
}

void
TextReceiver::receive (Transmitter::Channel chn, const char *str)

{
	const char *prefix = "";

	switch (chn) {
	case Transmitter::Debug:
		return;
	case Transmitter::Info:
		prefix = ": [INFO]: ";
		break;
	case Transmitter::Warning:
		prefix = ": [WARNING]: ";
		break;
	case Transmitter::Error:
		prefix = ": [ERROR]: ";
		break;
	case Transmitter::Fatal:
		prefix = ": [FATAL]: ";
		break;
	case Transmitter::Throw:
		/* this isn't supposed to happen */
		abort ();
	}

	/* note: iostreams are already thread-safe: no external
	   lock required.
	*/

	cout << name << prefix << str << endl;

	if (chn == Transmitter::Fatal) {
		exit (9);
	}
}

