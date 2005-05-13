/*
    Copyright (C) 2000 Paul Barton-Davis 

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

#include <stdio.h> /* for sprintf, sigh ... */
#include <string>
#include <ctype.h>

#include <gdk/gdkkeysyms.h>
#include <gtkmm2ext/hexentry.h>

using namespace std;
using namespace Gtkmm2ext;

bool
HexEntry::on_key_press_event (GdkEventKey *ev)

{
	if ((ev->keyval >= GDK_a && ev->keyval <= GDK_f) ||
	    (ev->keyval >= GDK_A && ev->keyval <= GDK_A) ||
	    (ev->keyval >= GDK_0 && ev->keyval <= GDK_9) ||
	    ev->keyval == GDK_space || 
	    ev->keyval == GDK_Tab ||
	    ev->keyval == GDK_Return ||
	    ev->keyval == GDK_BackSpace ||
	    ev->keyval == GDK_Delete) {
		return Gtk::Entry::on_key_press_event (ev);
	} else {
		gdk_beep ();
		return FALSE;
	}
}


void
HexEntry::set_hex (unsigned char *msg, unsigned int len) 
	
{
	/* create a textual representation of the MIDI message */
	
	if (msg && len) {
		char *rep;
		
		rep = new char[(len * 3) + 1];
		for (size_t i = 0; i < len; i++) {
			sprintf (&rep[i*3], "%02x ", msg[i]);
		}
		rep[len * 3] = '\0';
		set_text (rep);
		delete [] rep;
	} else {
		set_text ("");
	}
}

unsigned int
HexEntry::get_hex (unsigned char *hexbuf, size_t buflen)

{
	int fetched_len;
	char buf[3];
	string text = get_text();
	string::size_type length = text.length ();
	string::size_type offset;

	fetched_len = 0;
	buf[2] = '\0';
	offset = 0;

	while (1) {
		offset = text.find_first_of ("abcdef0123456789", offset);

		if (offset == string::npos) {
			break;
		}

		/* grab two characters, but no more */
		
		buf[0] = text[offset];

		if (offset < length - 1) {
			buf[1] = text[offset+1];
			offset += 2;
		} else {
			buf[1] = '\0';
			offset += 1;
		}

		hexbuf[fetched_len++] = (char) strtol (buf, 0, 16);
	}

	return fetched_len;
}


