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

#include <cstring>
#include <cstdlib>
#include <iostream>

#include "ardour/debug.h"

#include "i18n.h"

using namespace std;

void
ARDOUR::debug_print (const char* prefix, string str)
{
	cerr << prefix << ": " << str;
}

void
ARDOUR::set_debug_bits (uint64_t bits)
{
	debug_bits = bits;
}

int
ARDOUR::parse_debug_options (const char* str)
{
	char* p;
	char* sp;
	uint64_t bits = 0;
	char* copy = strdup (str);

	p = strtok_r (copy, ",", &sp);

	while (p) {
		if (strcasecmp (p, "list") == 0) {
			list_debug_options ();
			free (copy);
			return 1;
		}

		if (strcasecmp (p, "all") == 0) {
			ARDOUR::set_debug_bits (~0ULL);
			free (copy);
			return 0;
		}

		if (strncasecmp (p, "midisourceio", strlen (p)) == 0) {
			bits |= ARDOUR::DEBUG::MidiSourceIO;
		} else if (strncasecmp (p, "midiplaylistio", strlen (p)) == 0) {
			bits |= ARDOUR::DEBUG::MidiPlaylistIO;
		} else if (strncasecmp (p, "mididiskstreamio", strlen (p)) == 0) {
			bits |= ARDOUR::DEBUG::MidiDiskstreamIO;
		} else if (strncasecmp (p, "snapbbt", strlen (p)) == 0) {
			bits |= ARDOUR::DEBUG::SnapBBT;
		} else if (strncasecmp (p, "configuration", strlen (p)) == 0) {
			bits |= ARDOUR::DEBUG::Configuration;
		} else if (strncasecmp (p, "latency", strlen (p)) == 0) {
			bits |= ARDOUR::DEBUG::Latency;
		} else if (strncasecmp (p, "processors", strlen (p)) == 0) {
			bits |= ARDOUR::DEBUG::Processors;
		} else if (strncasecmp (p, "graph", strlen (p)) == 0) {
			bits |= ARDOUR::DEBUG::Graph;
		} else if (strncasecmp (p, "destruction", strlen (p)) == 0) {
			bits |= ARDOUR::DEBUG::Destruction;
		} else if (strncasecmp (p, "mtc", strlen (p)) == 0) {
			bits |= ARDOUR::DEBUG::MTC;
		} else if (strncasecmp (p, "transport", strlen (p)) == 0) {
			bits |= ARDOUR::DEBUG::Transport;
		} else if (strncasecmp (p, "slave", strlen (p)) == 0) {
			bits |= ARDOUR::DEBUG::Slave;
		} else if (strncasecmp (p, "sessionevents", strlen (p)) == 0) {
			bits |= ARDOUR::DEBUG::SessionEvents;
		}

		p = strtok_r (0, ",", &sp);
	}
	
	free (copy);
	ARDOUR::set_debug_bits (bits);
	return 0;
}

void
ARDOUR::list_debug_options ()
{
	cerr << _("The following debug options are available. Separate multipe options with commas.\nNames are case-insensitive and can be abbreviated.") << endl << endl;
	cerr << "\tAll" << endl;
	cerr << "\tMidiSourceIO" << endl;
	cerr << "\tMidiPlaylistIO" << endl;
	cerr << "\tMidiDiskstreamIO" << endl;
	cerr << "\tSnapBBT" << endl;
	cerr << "\tConfiguration" << endl;
	cerr << "\tLatency" << endl;
	cerr << "\tGraph" << endl;
	cerr << "\tDestruction" << endl;
	cerr << "\tMTC" << endl;
	cerr << "\tTransport" << endl;
	cerr << "\tSlave" << endl;
	cerr << "\tSessionEvents" << endl;
}
