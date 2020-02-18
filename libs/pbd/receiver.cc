/*
 * Copyright (C) 1998-2016 Paul Davis <paul@linuxaudiosystems.com>
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

#include <vector>

#include "pbd/receiver.h"
#include "pbd/transmitter.h"

using namespace std;
using namespace sigc;

Receiver::Receiver () {}

Receiver::~Receiver ()

{
	hangup ();
}

void
Receiver::hangup ()
{
	connections.drop_connections ();
}

void
Receiver::listen_to (Transmitter &transmitter)

{
	/* odd syntax here because boost's placeholders (_1, _2) are in an
	   anonymous namespace which causes ambiguity with sigc++ (and will also
	   do so with std::placeholder in the C++11 future
	*/
	transmitter.sender().connect_same_thread (connections, boost::bind (&Receiver::receive, this, boost::arg<1>(), boost::arg<2>()));

}
