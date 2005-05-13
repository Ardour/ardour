/*
    Copyright (C) 1999 Paul Barton-Davis 

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

#ifndef __gtkmm2ext_hexentry_h__
#define __gtkmm2ext_hexentry_h__

#include <gtkmm.h>

namespace Gtkmm2ext {

class HexEntry : public Gtk::Entry

{
  public:
	/* Take a byte-level representation of a series of hexadecimal
	   values and use them to set the displayed text of the entry.
	   Eg. if hexbuf[0] = 0xff and hexbuf[1] = 0xa1 and buflen = 2,
	   then the text will be set to "ff a1".
	*/

	void set_hex (unsigned char *hexbuf, unsigned int buflen);

	/* puts byte-level representation of current entry text
	   into hexbuf, and returns number of bytes written there.

	   NOTE: this will release the existing memory pointed to
	   by hexbuf if buflen indicates that it is not long enough
	   to hold the new representation, and hexbuf is not zero.

	   If the returned length is zero, the contents of hexbuf 
	   are undefined.
	*/

	unsigned int get_hex (unsigned char *hexbuf, size_t buflen);

  private:
	bool on_key_press_event (GdkEventKey *);
};

} /* namespace */

#endif /* __gtkmm2ext_hexentry_h__ */
