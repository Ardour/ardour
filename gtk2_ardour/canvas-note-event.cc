/*
    Copyright (C) 2007 Paul Davis
    Author: David Robillard

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

#include "gtkmm2ext/keyboard.h"

#include "canvas-note-event.h"
#include "midi_region_view.h"
#include "public_editor.h"
#include "editing_syms.h"
#include "keyboard.h"

using namespace std;
using namespace Gtkmm2ext;
using ARDOUR::MidiModel;

namespace Gnome {
namespace Canvas {

PBD::Signal1<void,CanvasNoteEvent*> CanvasNoteEvent::CanvasNoteEventDeleted;

/// dividing the hue circle in 16 parts, hand adjusted for equal look, courtesy Thorsten Wilms
const uint32_t CanvasNoteEvent::midi_channel_colors[16] = {
	  0xd32d2dff,  0xd36b2dff,  0xd3972dff,  0xd3d12dff,
	  0xa0d32dff,  0x7dd32dff,  0x2dd45eff,  0x2dd3c4ff,
	  0x2da5d3ff,  0x2d6fd3ff,  0x432dd3ff,  0x662dd3ff,
	  0x832dd3ff,  0xa92dd3ff,  0xd32dbfff,  0xd32d67ff
	};

CanvasNoteEvent::CanvasNoteEvent(MidiRegionView& region, Item* item, const boost::shared_ptr<NoteType> note)
	: _region(region)
	, _item(item)
	, _text(0)
	, _state(None)
	, _note(note)
	, _selected(false)
	, _valid (true)
	, _mouse_x_fraction (-1.0)
	, _mouse_y_fraction (-1.0)
	, _channel_selection (0xffff)
{
}

CanvasNoteEvent::~CanvasNoteEvent()
{
	CanvasNoteEventDeleted (this);

	if (_text) {
		_text->hide();
		delete _text;
	}
}

void
CanvasNoteEvent::invalidate ()
{
	_valid = false;
}

void
CanvasNoteEvent::validate ()
{
	_valid = true;
}

void
CanvasNoteEvent::show_velocity()
{
	if (!_text) {
		_text = new NoEventText (*(_item->property_parent()));
		_text->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_MidiNoteVelocityText.get();
		_text->property_justification() = Gtk::JUSTIFY_CENTER;
	}

	_text->property_x() = (x1() + x2()) /2;
	_text->property_y() = (y1() + y2()) /2;
	ostringstream velo(ios::ate);
	velo << int(_note->velocity());
	_text->property_text() = velo.str();
	_text->show();
	_text->raise_to_top();
}

void
CanvasNoteEvent::hide_velocity()
{
	if (_text) {
		_text->hide();
		delete _text;
		_text = 0;
	}
}

void
CanvasNoteEvent::on_channel_selection_change(uint16_t selection)
{
	_channel_selection = selection;
	
	/* this takes into account whether or not the note should be drawn as inactive */
	set_selected (_selected);

	// this forces the item to update..... maybe slow...
	_item->hide();
	_item->show();
}

void
CanvasNoteEvent::on_channel_change(uint8_t channel)
{
	_region.note_selected(this, true);
	_region.change_channel(channel);
}

void
CanvasNoteEvent::set_selected(bool selected)
{
	if (!_note) {
		return;
	}

	_selected = selected;

	bool const active = (_channel_selection & (1 << _note->channel())) != 0;

	if (_selected && active) {
		set_outline_color(calculate_outline(ARDOUR_UI::config()->canvasvar_MidiNoteSelected.get()));
		set_fill_color (base_color ());

	} else {

		if (active) {
			set_fill_color(base_color());
			set_outline_color(calculate_outline(base_color()));
		} else {
			set_fill_color(ARDOUR_UI::config()->canvasvar_MidiNoteInactiveChannel.get());
			set_outline_color(calculate_outline(ARDOUR_UI::config()->canvasvar_MidiNoteInactiveChannel.get()));
		}
	}
}

#define SCALE_USHORT_TO_UINT8_T(x) ((x) / 257)

uint32_t
CanvasNoteEvent::base_color()
{
	using namespace ARDOUR;

	ColorMode mode = _region.color_mode();

	const uint8_t min_opacity = 15;
	uint8_t       opacity = std::max(min_opacity, uint8_t(_note->velocity() + _note->velocity()));

	switch (mode) {
	case TrackColor:
	{
		Gdk::Color color = _region.midi_stream_view()->get_region_color();
		return UINT_INTERPOLATE (RGBA_TO_UINT(
			                         SCALE_USHORT_TO_UINT8_T(color.get_red()),
			                         SCALE_USHORT_TO_UINT8_T(color.get_green()),
			                         SCALE_USHORT_TO_UINT8_T(color.get_blue()),
			                         opacity),
		                         ARDOUR_UI::config()->canvasvar_MidiNoteSelected.get(), 0.5);
	}

	case ChannelColors:
		return UINT_INTERPOLATE (UINT_RGBA_CHANGE_A (CanvasNoteEvent::midi_channel_colors[_note->channel()],
		                                             opacity),
		                         ARDOUR_UI::config()->canvasvar_MidiNoteSelected.get(), 0.5);

	default:
		return meter_style_fill_color(_note->velocity(), selected());
	};

	return 0;
}

void
CanvasNoteEvent::set_mouse_fractions (GdkEvent* ev)
{
	double ix, iy;
	double bx1, bx2, by1, by2;
	bool set_cursor = false;

	switch (ev->type) {
	case GDK_MOTION_NOTIFY:
		ix = ev->motion.x;
		iy = ev->motion.y;
		set_cursor = true;
		break;
	case GDK_ENTER_NOTIFY:
		ix = ev->crossing.x;
		iy = ev->crossing.y;
		set_cursor = true;
		break;
	case GDK_BUTTON_PRESS:
	case GDK_BUTTON_RELEASE:
		ix = ev->button.x;
		iy = ev->button.y;
		break;
	default:
		_mouse_x_fraction = -1.0;
		_mouse_y_fraction = -1.0;
		return;
	}

	_item->get_bounds (bx1, by1, bx2, by2);
	_item->w2i (ix, iy);
	/* hmm, something wrong here. w2i should give item-local coordinates
	   but it doesn't. for now, finesse this.
	*/
	ix = ix - bx1;
	iy = iy - by1;

	/* fraction of width/height */
	double xf;
	double yf;
	bool notify = false;

	xf = ix / (bx2 - bx1);
	yf = iy / (by2 - by1);

	if (xf != _mouse_x_fraction || yf != _mouse_y_fraction) {
		notify = true;
	}

	_mouse_x_fraction = xf;
	_mouse_y_fraction = yf;

	if (notify) {
                if (big_enough_to_trim()) {
                        _region.note_mouse_position (_mouse_x_fraction, _mouse_y_fraction, set_cursor);
                } else {
                        /* pretend the mouse is in the middle, because this is not big enough
                           to trim right now.
                        */
                        _region.note_mouse_position (0.5, 0.5, set_cursor);
                }
	}
}

bool
CanvasNoteEvent::on_event(GdkEvent* ev)
{
	if (!_region.get_time_axis_view().editor().internal_editing()) {
		return false;
	}

	switch (ev->type) {
	case GDK_ENTER_NOTIFY:
		set_mouse_fractions (ev);
		_region.note_entered (this);
		break;

	case GDK_LEAVE_NOTIFY:
		set_mouse_fractions (ev);
		_region.note_left (this);
		break;

	case GDK_MOTION_NOTIFY:
		set_mouse_fractions (ev);
		break;

	case GDK_BUTTON_PRESS:
		set_mouse_fractions (ev);
		if (ev->button.button == 3 && Keyboard::no_modifiers_active (ev->button.state) && _selected) {
			_region.get_time_axis_view().editor().edit_notes (_region);
			return true;
		}
		break;

	case GDK_BUTTON_RELEASE:
		set_mouse_fractions (ev);
		if (ev->button.button == 3 && Keyboard::no_modifiers_active (ev->button.state)) {
			return true;
		}
		break;

	default:
		break;
	}

	return false;
}

bool
CanvasNoteEvent::mouse_near_ends () const
{
	return (_mouse_x_fraction >= 0.0 && _mouse_x_fraction < 0.25) ||
		(_mouse_x_fraction >= 0.75 && _mouse_x_fraction < 1.0);
}

bool
CanvasNoteEvent::big_enough_to_trim () const
{
        return (x2() - x1()) > 20; /* canvas units, really pixels */
}

} // namespace Canvas
} // namespace Gnome

