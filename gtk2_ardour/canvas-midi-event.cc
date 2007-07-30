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

#include <iostream>
#include "canvas-midi-event.h"
#include "midi_region_view.h"
#include "public_editor.h"
#include "editing_syms.h"


using namespace std;

namespace Gnome {
namespace Canvas {


CanvasMidiEvent::CanvasMidiEvent(MidiRegionView& region, Item* item)
	: _region(region)
	, _item(item)
	, _state(None)
{	
}


bool
CanvasMidiEvent::on_event(GdkEvent* ev)
{
	static double last_x, last_y;
	double event_x, event_y;

	if (_region.get_time_axis_view().editor.current_mouse_mode() != Editing::MouseNote)
		return false;

	switch (ev->type) {
	/*case GDK_ENTER_NOTIFY:
		cerr << "ENTERED: " << ev->crossing.state << endl;
		if ( (ev->crossing.state & GDK_BUTTON2_MASK) ) {

		}
		break;
	*/
	case GDK_KEY_PRESS:
		cerr << "EVENT KEY PRESS\n"; // doesn't work :/
		break;

	case GDK_BUTTON_PRESS:
		_state = Pressed;
		return true;

	case GDK_MOTION_NOTIFY:
		event_x = ev->motion.x;
		event_y = ev->motion.y;
		_item->property_parent().get_value()->w2i(event_x, event_y);

		switch (_state) {
		case Pressed:
			_item->grab(GDK_POINTER_MOTION_MASK | GDK_BUTTON_RELEASE_MASK,
					Gdk::Cursor(Gdk::FLEUR), ev->motion.time);
			_state = Dragging;
			last_x = event_x;
			last_y = event_y;
			return true;
		case Dragging:
			if (ev->motion.is_hint) {
				int t_x;
				int t_y;
				GdkModifierType state;
				gdk_window_get_pointer(ev->motion.window, &t_x, &t_y, &state);
				event_x = t_x;
				event_y = t_y;
			}

			_item->move(event_x - last_x, event_y - last_y);

			last_x = event_x;
			last_y = event_y;

			return true;
		default:
			break;
		}
		break;
	
	case GDK_BUTTON_RELEASE:
		switch (_state) {
		case Pressed: // Clicked
			_state = None;
			return true;
		case Dragging: // Dropped
			_item->ungrab(ev->button.time);
			_state = None;
			return true;
		default:
			break;
		}

	default:
		break;
	}

	return false;
}

} // namespace Canvas
} // namespace Gnome

