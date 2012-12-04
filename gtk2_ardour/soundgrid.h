/*
    Copyright (C) 1999-2002 Paul Davis

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

#ifndef __gtk2_ardour_soundgrid_h__
#define __gtk2_ardour_soundgrid_h__

namespace Gtk {
        class Window;
}

void soundgrid_setup ();
int  soundgrid_init (uint32_t max_phys_inputs, uint32_t max_phys_outputs, uint32_t max_tracks, 
                     uint32_t max_busses, uint32_t max_plugins_per_track);

#endif /* __gtk2_ardour_soundgrid_h__ */
