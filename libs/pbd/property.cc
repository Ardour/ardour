/*
    Copyright (C) 2010 Paul Davis

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

#include <stdint.h>

#include "pbd/properties.h"
#include "pbd/error.h"

#include "i18n.h"

using namespace PBD;

PropertyChange
PBD::new_change ()
{
	static uint64_t change_bit = 1;

	/* catch out-of-range */
	if (!change_bit) {
		fatal << _("programming error: ")
			<< "change_bit out of range in ARDOUR::new_change()"
			<< endmsg;
		/*NOTREACHED*/
	}

	PropertyChange c = PropertyChange (change_bit);
	change_bit <<= 1;	// if it shifts too far, change_bit == 0

	return c;
}

