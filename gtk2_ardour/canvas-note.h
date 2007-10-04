/*
    Copyright (C) 2007 Paul Davis 
    Author: Dave Robillard

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

#ifndef __gtk_ardour_canvas_note_h__
#define __gtk_ardour_canvas_note_h__

#include <iostream>
#include "simplerect.h"
#include "canvas-midi-event.h"
#include "midi_util.h"

namespace Gnome {
namespace Canvas {

class CanvasNote : public SimpleRect, public CanvasMidiEvent {
public:
	CanvasNote(MidiRegionView& region, Group& group, const ARDOUR::Note* note=NULL, bool copy_note=false)
		: SimpleRect(group), CanvasMidiEvent(region, this, note, copy_note)
	{
	}
	
	double x1() { return property_x1(); }
	double y1() { return property_y1(); }
	double x2() { return property_x2(); }
	double y2() { return property_y2(); }
	
	void set_outline_color(uint32_t c) { property_outline_color_rgba() = c; }
	void set_fill_color(uint32_t c) { property_fill_color_rgba() = c; }
	
	bool on_event(GdkEvent* ev) { return CanvasMidiEvent::on_event(ev); }
};

} // namespace Gnome
} // namespace Canvas

#endif /* __gtk_ardour_canvas_note_h__ */
