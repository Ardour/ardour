/*
    Copyright (C) 2012 Paul Davis 

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

#include "canvas-note.h"
#include "midi_region_view.h"
#include "public_editor.h"
#include "evoral/Note.hpp"
#include "utils.h"

using namespace ARDOUR;

namespace Gnome {
namespace Canvas {

CanvasHit::CanvasHit (MidiRegionView&                   region,
                      Group&                            group,
                      double                            size,
                      const boost::shared_ptr<NoteType> note,
                      bool with_events)
	: Diamond(group, size)
	, CanvasNoteEvent(region, this, note)
{
	if (with_events) {
		signal_event().connect (sigc::mem_fun (*this, &CanvasHit::on_event));
	}
}

bool
CanvasHit::on_event(GdkEvent* ev)
{
	if (!CanvasNoteEvent::on_event (ev)) {
		return _region.get_time_axis_view().editor().canvas_note_event (ev, this);
	}
	return true;
}

void
CanvasHit::move_event(double dx, double dy)
{
	move_by (dx, dy);
}

} // namespace Gnome
} // namespace Canvas
