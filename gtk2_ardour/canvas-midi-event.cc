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


CanvasMidiEvent::CanvasMidiEvent(MidiRegionView& region, Item* item,
		const boost::shared_ptr<ARDOUR::Note> note)
	: _region(region)
	, _item(item)
	, _state(None)
	, _note(note)
	, _selected(false)
{	
}


void
CanvasMidiEvent::selected(bool yn)
{
	if (!_note) {
		return;
	} else if (yn) {
		set_fill_color(UINT_INTERPOLATE(note_fill_color(_note->velocity()),
					ARDOUR_UI::config()->canvasvar_MidiNoteSelectedOutline.get(), 0.85));
		set_outline_color(ARDOUR_UI::config()->canvasvar_MidiNoteSelectedOutline.get());
	} else {
		set_fill_color(note_fill_color(_note->velocity()));
		set_outline_color(note_outline_color(_note->velocity()));
	}

	_selected = yn;
}


bool
CanvasMidiEvent::on_event(GdkEvent* ev)
{
	static uint8_t drag_delta_note = 0;
	static double  drag_delta_x = 0;
	static double last_x, last_y;
	double event_x, event_y, dx, dy;
	nframes_t event_frame;
	bool select_mod;

	if (_region.get_time_axis_view().editor.current_mouse_mode() != Editing::MouseNote)
		return false;

	switch (ev->type) {
	case GDK_KEY_PRESS:
		cerr << "EV KEY PRESS\n";
		if (_note && ev->key.keyval == GDK_Delete) {
			cerr << "EV DELETE KEY\n";
			selected(true);
			_region.start_remove_command();
			_region.command_remove_note(this);
		}
		break;
	
	case GDK_KEY_RELEASE:
		cerr << "EV KEY RELEASE\n";
		if (ev->key.keyval == GDK_Delete) {
			_region.apply_command();
		}
		break;
	
	case GDK_ENTER_NOTIFY:
		_region.note_entered(this);
		_item->grab_focus();
		Keyboard::magic_widget_grab_focus();
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
		case Pressed: // Drag begin
			if (_region.mouse_state() != MidiRegionView::SelectTouchDragging) {
				_item->grab(GDK_POINTER_MOTION_MASK | GDK_BUTTON_RELEASE_MASK,
						Gdk::Cursor(Gdk::FLEUR), ev->motion.time);
				_state = Dragging;
				last_x = event_x;
				last_y = event_y;
				drag_delta_x = 0;
				drag_delta_note = 0;
				_region.note_selected(this, true);
			}
			return true;

		case Dragging: // Drag motion
			if (ev->motion.is_hint) {
				int t_x;
				int t_y;
				GdkModifierType state;
				gdk_window_get_pointer(ev->motion.window, &t_x, &t_y, &state);
				event_x = t_x;
				event_y = t_y;
			}
			
			// Snap
			event_frame = _region.midi_view()->editor.pixel_to_frame(event_x);
			_region.midi_view()->editor.snap_to(event_frame);
			event_x = _region.midi_view()->editor.frame_to_pixel(event_frame);

			dx = event_x - last_x;
			dy = event_y - last_y;
			
			last_x = event_x;

			drag_delta_x += dx;

			// Snap to note rows
			if (abs(dy) < _region.midi_stream_view()->note_height()) {
				dy = 0.0;
			} else {
				int8_t this_delta_note;
				if (dy > 0)
					this_delta_note = (int8_t)ceil(dy / _region.midi_stream_view()->note_height() / 2.0);
				else
					this_delta_note = (int8_t)floor(dy / _region.midi_stream_view()->note_height() / 2.0);
				drag_delta_note -= this_delta_note;
				dy = _region.midi_stream_view()->note_height() * this_delta_note;
				last_y = last_y + dy;
			}

			_region.move_selection(dx, dy);

			return true;
		default:
			break;
		}
		break;
	
	case GDK_BUTTON_RELEASE:
		select_mod = (ev->motion.state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK));
		event_x = ev->button.x;
		event_y = ev->button.y;
		_item->property_parent().get_value()->w2i(event_x, event_y);

		switch (_state) {
		case Pressed: // Clicked
			if (_region.midi_view()->editor.current_midi_edit_mode() == Editing::MidiEditSelect) {
				_state = None;

				if (_selected && !select_mod && _region.selection_size() > 1)
					_region.unique_select(this);
				else if (_selected)
					_region.note_deselected(this, select_mod);
				else
					_region.note_selected(this, select_mod);
			} else if (_region.midi_view()->editor.current_midi_edit_mode() == Editing::MidiEditErase) {
				_region.start_remove_command();
				_region.command_remove_note(this);
				_region.apply_command();
			}

			return true;
		case Dragging: // Dropped
			_item->ungrab(ev->button.time);
			_state = None;

			if (_note)
				_region.note_dropped(this,
						_region.midi_view()->editor.pixel_to_frame(abs(drag_delta_x))
								* ((drag_delta_x < 0.0) ? -1 : 1),
						drag_delta_note);
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

