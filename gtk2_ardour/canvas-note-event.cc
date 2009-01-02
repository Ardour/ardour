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
#include "canvas-note-event.h"
#include "midi_region_view.h"
#include "public_editor.h"
#include "editing_syms.h"
#include "keyboard.h"

using namespace std;
using ARDOUR::MidiModel;

namespace Gnome {
namespace Canvas {

/// dividing the hue circle in 16 parts, hand adjusted for equal look, courtesy Thorsten Wilms
const uint32_t CanvasNoteEvent::midi_channel_colors[16] = {
	  0xd32d2dff,  0xd36b2dff,  0xd3972dff,  0xd3d12dff,  
	  0xa0d32dff,  0x7dd32dff,  0x2dd45eff,  0x2dd3c4ff,  
	  0x2da5d3ff,  0x2d6fd3ff,  0x432dd3ff,  0x662dd3ff,  
	  0x832dd3ff,  0xa92dd3ff,  0xd32dbfff,  0xd32d67ff
	};

CanvasNoteEvent::CanvasNoteEvent(MidiRegionView& region, Item* item,
		const boost::shared_ptr<Evoral::Note> note)
	: _region(region)
	, _item(item)
	, _text(0)
	, _channel_selector_widget()
	, _state(None)
	, _note(note)
	, _selected(false)
{
}

CanvasNoteEvent::~CanvasNoteEvent() 
{ 
	if (_text) {
		_text->hide();
		delete _text;
	}
	
	delete _channel_selector_widget;
}

void 
CanvasNoteEvent::move_event(double dx, double dy)
{
	_item->move(dx, dy);
	if (_text) {
		_text->hide();
		_text->move(dx, dy);
		_text->show();
	}
}

void
CanvasNoteEvent::show_velocity()
{
	hide_velocity();
	_text = new InteractiveText(*(_item->property_parent()), this);
	_text->property_x() = (x1() + x2()) /2;
	_text->property_y() = (y1() + y2()) /2;
	ostringstream velo(ios::ate);
	velo << int(_note->velocity());
	_text->property_text() = velo.str();
	_text->property_justification() = Gtk::JUSTIFY_CENTER;
	_text->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_MidiNoteVelocityText.get();
	_text->show();
	_text->raise_to_top();
}

void
CanvasNoteEvent::hide_velocity()
{
	if (_text) {
		_text->hide();
		delete _text;
	}
	_text = 0;
}

void 
CanvasNoteEvent::on_channel_selection_change(uint16_t selection)
{
	// make note change its color if its channel is not marked active
	if ( (selection & (1 << _note->channel())) == 0 ) {
		set_fill_color(ARDOUR_UI::config()->canvasvar_MidiNoteInactiveChannel.get());
		set_outline_color(calculate_outline(ARDOUR_UI::config()->canvasvar_MidiNoteInactiveChannel.get()));
	} else {
		// set the color according to the notes selection state
		selected(_selected);
	}
	// this forces the item to update..... maybe slow...
	_item->hide();
	_item->show();
}

void 
CanvasNoteEvent::on_channel_change(uint8_t channel)
{
	_region.note_selected(this, true);
	hide_channel_selector();
	_region.change_channel(channel);
}

void
CanvasNoteEvent::show_channel_selector(void)
{
	if (_channel_selector_widget == 0) {
		cerr << "Note has channel: " << int(_note->channel()) << endl;
		SingleMidiChannelSelector* _channel_selector = new SingleMidiChannelSelector(_note->channel());
		_channel_selector->show_all();
		_channel_selector->channel_selected.connect(
			sigc::mem_fun(this, &CanvasNoteEvent::on_channel_change));

		_channel_selector_widget = 
			new Widget(*(_item->property_parent()), 
			           x1(), 
			           y2() + 2, 
			           (Gtk::Widget &) *_channel_selector);
		
		_channel_selector_widget->hide();
		_channel_selector_widget->property_height() = 100;
		_channel_selector_widget->property_width() = 100;
		_channel_selector_widget->raise_to_top();
		_channel_selector_widget->show();
	} else {
		hide_channel_selector();
	}
}

void
CanvasNoteEvent::hide_channel_selector(void)
{
	if (_channel_selector_widget) {
		_channel_selector_widget->hide();
		delete _channel_selector_widget;
		_channel_selector_widget = 0;
	}
}

void
CanvasNoteEvent::selected(bool selected)
{
	if (!_note) {
		return;
	} else if (selected) {
		set_fill_color(UINT_INTERPOLATE(base_color(),
				ARDOUR_UI::config()->canvasvar_MidiNoteSelected.get(), 0.5));
		set_outline_color(calculate_outline(
				ARDOUR_UI::config()->canvasvar_MidiNoteSelected.get()));
		show_velocity();
	} else {
		set_fill_color(base_color());
		set_outline_color(calculate_outline(base_color()));
		hide_velocity();
	}

	_selected = selected;
}

#define SCALE_USHORT_TO_UINT8_T(x) ((x) / 257)

uint32_t 
CanvasNoteEvent::base_color()
{
	using namespace ARDOUR;
	
	ColorMode mode = _region.color_mode();
	
	const uint8_t minimal_opaqueness = 15;
	uint8_t       opaqueness = std::max(minimal_opaqueness, uint8_t(_note->velocity() + _note->velocity()));
	
	switch (mode) {
	case TrackColor:
		{
			Gdk::Color color = _region.midi_stream_view()->get_region_color();
			return RGBA_TO_UINT(
					SCALE_USHORT_TO_UINT8_T(color.get_red()), 
					SCALE_USHORT_TO_UINT8_T(color.get_green()), 
					SCALE_USHORT_TO_UINT8_T(color.get_blue()), 
					opaqueness);
		}
		
	case ChannelColors:
		return UINT_RGBA_CHANGE_A(CanvasNoteEvent::midi_channel_colors[_note->channel()], 
				                  opaqueness);
		
	default:
		return meter_style_fill_color(_note->velocity());
	};
	
	return 0;
}

bool
CanvasNoteEvent::on_event(GdkEvent* ev)
{
	MidiStreamView *streamview = _region.midi_stream_view();
	static uint8_t drag_delta_note = 0;
	static double  drag_delta_x = 0;
	static double last_x, last_y;
	double event_x, event_y, dx, dy;
	bool select_mod;
	uint8_t d_velocity = 10;
	
	if (_region.get_time_axis_view().editor().current_mouse_mode() != Editing::MouseNote)
		return false;

	switch (ev->type) {
	case GDK_SCROLL:
		if (Keyboard::modifier_state_equals (ev->scroll.state, Keyboard::Level4Modifier)) {
			d_velocity = 1;
		}

		if (ev->scroll.direction == GDK_SCROLL_UP) {
			_region.change_velocity(this, d_velocity, true);
			return true;
		} else if (ev->scroll.direction == GDK_SCROLL_DOWN) {
			_region.change_velocity(this, -d_velocity, true);
			return true;
		} else {
			return false;
		}
		
	case GDK_KEY_PRESS:
		if (_note && ev->key.keyval == GDK_Delete) {
			selected(true);
			_region.start_delta_command();
			_region.command_remove_note(this);
		}
		break;

	case GDK_KEY_RELEASE:
		if (ev->key.keyval == GDK_Delete) {
			_region.apply_command();
		}
		break;

	case GDK_ENTER_NOTIFY:
		_region.note_entered(this);
		_item->grab_focus();
		show_velocity();
		Keyboard::magic_widget_grab_focus();
		break;

	case GDK_LEAVE_NOTIFY:
		Keyboard::magic_widget_drop_focus();
		if (! selected()) {
			hide_velocity();
		}
		_region.get_canvas_group()->grab_focus();
		break;

	case GDK_BUTTON_PRESS:
		if (ev->button.button == 1) {
			_state = Pressed;
		} else if (ev->button.button == 3) {
			show_channel_selector();
		}
		return true;

	case GDK_MOTION_NOTIFY:
		event_x = ev->motion.x;
		event_y = ev->motion.y;

		switch (_state) {
		case Pressed: // Drag begin
			if (_region.midi_view()->editor().current_midi_edit_mode() == Editing::MidiEditSelect
					&& _region.mouse_state() != MidiRegionView::SelectTouchDragging
					&& _region.mouse_state() != MidiRegionView::EraseTouchDragging) {
				_item->grab(GDK_POINTER_MOTION_MASK | GDK_BUTTON_RELEASE_MASK,
						Gdk::Cursor(Gdk::FLEUR), ev->motion.time);
				_state = Dragging;
				_item->property_parent().get_value()->w2i(event_x, event_y);
				event_x = _region.snap_to_pixel(event_x); 
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
			_item->property_parent().get_value()->w2i(event_x, event_y);
			
			// Snap
			event_x = _region.snap_to_pixel(event_x); 

			dx = event_x - last_x;
			dy = event_y - last_y;

			last_x = event_x;

			drag_delta_x += dx;

			// Snap to note rows
			if (abs(dy) < streamview->note_height()) {
				dy = 0.0;
			} else {
				int8_t this_delta_note;
				if (dy > 0) {
					this_delta_note = (int8_t)ceil(dy / streamview->note_height() / 2.0);
				} else {
					this_delta_note = (int8_t)floor(dy / streamview->note_height() / 2.0);
				}
				drag_delta_note -= this_delta_note;
				dy = streamview->note_height() * this_delta_note;
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
		
		if (ev->button.button == 3) {
			return true;
		}
		
		switch (_state) {
		case Pressed: // Clicked
			if (_region.midi_view()->editor().current_midi_edit_mode() == Editing::MidiEditSelect) {
				_state = None;

				if (_selected && !select_mod && _region.selection_size() > 1)
					_region.unique_select(this);
				else if (_selected)
					_region.note_deselected(this, select_mod);
				else
					_region.note_selected(this, select_mod);
			} else if (_region.midi_view()->editor().current_midi_edit_mode() == Editing::MidiEditErase) {
				_region.start_delta_command();
				_region.command_remove_note(this);
				_region.apply_command();
			}

			return true;
		case Dragging: // Dropped
			_item->ungrab(ev->button.time);
			_state = None;

			if (_note)
				_region.note_dropped(this,
						     _region.midi_view()->editor().pixel_to_frame(abs(drag_delta_x))
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

