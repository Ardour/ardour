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

#ifndef __gtk_ardour_canvas_hit_h__
#define __gtk_ardour_canvas_hit_h__

#include <iostream>
#include "simplerect.h"
#include "diamond.h"

namespace Gnome {
namespace Canvas {

class CanvasHit : public Diamond, public CanvasMidiEvent {
public:
	CanvasHit(MidiRegionView& region, Group& group, double size)
		: Diamond(group, size), CanvasMidiEvent(region, this) {}

	bool on_event(GdkEvent* ev) { return CanvasMidiEvent::on_event(ev); }
};

} // namespace Gnome
} // namespace Canvas

#endif /* __gtk_ardour_canvas_hit_h__ */
