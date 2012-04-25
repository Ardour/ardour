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

#include "gtkmm2ext/gui_thread.h"
#include "midi_region_view.h"
#include "midi_selection.h"
#include "region_view.h"

MidiRegionSelection::MidiRegionSelection ()
{
	RegionView::RegionViewGoingAway.connect (_death_connection, MISSING_INVALIDATOR, boost::bind (&MidiRegionSelection::remove_it, this, _1), gui_context());
}

/** Copy constructor.
 *  @param other MidiRegionSelection to copy.
 */
MidiRegionSelection::MidiRegionSelection (MidiRegionSelection const & other)
	: std::list<MidiRegionView*> (other)
{
	RegionView::RegionViewGoingAway.connect (_death_connection, MISSING_INVALIDATOR, boost::bind (&MidiRegionSelection::remove_it, this, _1), gui_context());
}


void
MidiRegionSelection::remove_it (RegionView* rv)
{
	MidiRegionView* mrv = dynamic_cast<MidiRegionView*> (rv);
	remove (mrv);
}
