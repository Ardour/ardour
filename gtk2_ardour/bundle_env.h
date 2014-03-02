/*
    Copyright (C) 2001-2012 Paul Davis

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

#ifndef __gtk2_ardour_bundle_env_h__
#define __gtk2_ardour_bundle_env_h__

/** This function must do whatever is necessary to create the right runtime
 * environment for the GTK2 version of ardour, on a per-platform basis. 
 */

void fixup_bundle_environment (int, char* [], const char** localedir);

/** Load any fonts required by the GTK2 version of ardour, on a per-platform
 * basis.
 */

void load_custom_fonts();

#endif /* __gtk2_ardour_bundle_env_h__ */
