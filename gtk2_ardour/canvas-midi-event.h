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

#ifndef __gtk_ardour_canvas_midi_event_h__
#define __gtk_ardour_canvas_midi_event_h__

#include "simplerect.h"

class Editor;
class MidiRegionView;

namespace Gnome {
namespace Canvas {


/** This manages all the event handling for any MIDI event on the canvas.
 *
 * This is not actually a canvas item itself to avoid the dreaded diamond,
 * since various types of canvas items (Note (rect), Hit (diamond), etc)
 * need to share this functionality but can't share an ancestor.
 *
 * Note: Because of this, derived classes need to manually bounce events to
 * on_event, it won't happen automatically.
 */
class CanvasMidiEvent {
public:
	CanvasMidiEvent(MidiRegionView& region, Item* item);
	virtual ~CanvasMidiEvent() {} 

	virtual bool on_event(GdkEvent* ev);

private:
	enum State { None, Pressed, Dragging };

	MidiRegionView& _region;
	Item* const     _item;
	State           _state;
};

} // namespace Gnome
} // namespace Canvas

#endif /* __gtk_ardour_canvas_midi_event_h__ */
