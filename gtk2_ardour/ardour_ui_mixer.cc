/*
    Copyright (C) 2000 Paul Davis 

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

/* This file contains any ARDOUR_UI methods that require knowledge of
   the mixer, and exists so that no compilation dependency exists
   between the main ARDOUR_UI modules and the mixer classes. This
   is to cut down on the nasty compile times for these classes.
*/

#include "ardour_ui.h"
#include "mixer_ui.h"

using namespace ARDOUR;

int
ARDOUR_UI::create_mixer ()

{
	try {
		mixer = new Mixer_UI (*engine);
	} 

	catch (failed_constructor& err) {
		return -1;
	}

	return 0;
}
