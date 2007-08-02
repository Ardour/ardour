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
#include "keyboard.h"

using namespace std;
using ARDOUR::MidiModel;

namespace Gnome {
namespace Canvas {


CanvasMidiEvent::CanvasMidiEvent(MidiRegionView& region, Item* item, const ARDOUR::MidiModel::Note* note)
	: _region(region)
	, _item(item)
	, _state(None)
	, _note(note)
{	
}


bool
CanvasMidiEvent::on_event(GdkEvent* ev)
{
	static double last_x, last_y;
	double event_x, event_y, dx, dy;

	if (_region.get_time_axis_view().editor.current_mouse_mode() != Editing::MouseNote)
		return false;

	switch (ev->type) {
	case GDK_KEY_PRESS:
		if (_note && ev->key.keyval == GDK_Delete) {
			_region.start_remove_command();
			_region.command_remove_note(this);
		}
		break;
	
	case GDK_KEY_RELEASE:
		if (ev->key.keyval == GDK_Delete) {
			_region.apply_command();
		}
		break;
	
	case GDK_ENTER_NOTIFY:
		Keyboard::magic_widget_grab_focus();
		_item->grab_focus();
		_region.note_entered(this);
		break;

	case GDK_LEAVE_NOTIFY:
		Keyboard::magic_widget_drop_focus();
		_region.get_canvas_group()->grab_focus();
		break;

	case GDK_BUTTON_PRESS:
		_state = Pressed;
		return true;

	case GDK_MOTION_NOTIFY:
		event_x = ev->motion.x;
		event_y = ev->motion.y;
		//cerr << "MOTION @ " << event_x << ", " << event_y << endl;
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

			dx = event_x - last_x;
			dy = event_y - last_y;
			
			last_x = event_x;

			// Snap to note rows
			if (abs(dy) < _region.note_height()) {
				dy = 0.0;
			} else {
				dy = _region.note_height() * ((dy > 0) ? 1 : -1);
				last_y = event_y;
			}

			_item->move(dx, dy);

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
			if (_note) {
				cerr << "Move and stuff." << endl;
				// This would be nicer with a MoveCommand that doesn't need to copy...
				/*_region.start_delta_command();
				_region.command_remove_note(this);
				Note copy_of_me(*this); 
				copy_of_me.time = trackview.editor.pixel_to_frame(property_x1());
				copy_of_me.note = stuff;
				_region.apply_command();
				*/
			}
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

