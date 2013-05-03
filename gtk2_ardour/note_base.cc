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

#include "canvas/text.h"

#include "note_base.h"
#include "midi_region_view.h"
#include "public_editor.h"
#include "editing_syms.h"
#include "keyboard.h"

using namespace std;
using namespace Gtkmm2ext;
using ARDOUR::MidiModel;
using namespace ArdourCanvas;

PBD::Signal1<void,NoteBase*> NoteBase::NoteBaseDeleted;

/// dividing the hue circle in 16 parts, hand adjusted for equal look, courtesy Thorsten Wilms
const uint32_t NoteBase::midi_channel_colors[16] = {
	  0xd32d2dff,  0xd36b2dff,  0xd3972dff,  0xd3d12dff,
	  0xa0d32dff,  0x7dd32dff,  0x2dd45eff,  0x2dd3c4ff,
	  0x2da5d3ff,  0x2d6fd3ff,  0x432dd3ff,  0x662dd3ff,
	  0x832dd3ff,  0xa92dd3ff,  0xd32dbfff,  0xd32d67ff
	};

NoteBase::NoteBase(MidiRegionView& region, bool with_events, const boost::shared_ptr<NoteType> note)
	: _region(region)
	, _item (0)
	, _text(0)
//	, _channel_selector_widget()
	, _state(None)
	, _note(note)
	, _with_events (with_events)
	, _selected(false)
	, _valid (true)
	, _mouse_x_fraction (-1.0)
	, _mouse_y_fraction (-1.0)
{
}

NoteBase::~NoteBase()
{
	NoteBaseDeleted (this);

	delete _text;

	/* XXX */
//	delete _channel_selector_widget;
}

void
NoteBase::set_item (Item* item)
{
	_item = item;
	_item->set_data ("notebase", this);

	if (_with_events) {
		_item->Event.connect (sigc::mem_fun (*this, &NoteBase::event_handler));
	}
}

void
NoteBase::invalidate ()
{
	_valid = false;
}

void
NoteBase::validate ()
{
	_valid = true;
}

void
NoteBase::show_velocity()
{
	if (!_text) {
		_text = new Text (_item->parent ());
		_text->set_ignore_events (true);
		_text->set_color (ARDOUR_UI::config()->get_canvasvar_MidiNoteVelocityText());
		_text->set_alignment (Pango::ALIGN_CENTER);
	}

	_text->set_x_position ((x0() + x1()) / 2);
	_text->set_y_position ((y0() + y1()) / 2);
	ostringstream velo(ios::ate);
	velo << int(_note->velocity());
	_text->set (velo.str ());
	_text->show();
	_text->raise_to_top();
}

void
NoteBase::hide_velocity()
{
	delete _text;
	_text = 0;
}

void
NoteBase::on_channel_selection_change(uint16_t selection)
{
	// make note change its color if its channel is not marked active
	if ( (selection & (1 << _note->channel())) == 0 ) {
		set_fill_color(ARDOUR_UI::config()->get_canvasvar_MidiNoteInactiveChannel());
		set_outline_color(calculate_outline(ARDOUR_UI::config()->get_canvasvar_MidiNoteInactiveChannel()));
	} else {
		// set the color according to the notes selection state
		set_selected(_selected);
	}
	// this forces the item to update..... maybe slow...
	_item->hide();
	_item->show();
}

void
NoteBase::on_channel_change(uint8_t channel)
{
	_region.note_selected(this, true);
	hide_channel_selector();
	_region.change_channel(channel);
}

void
NoteBase::show_channel_selector ()
{
#if 0	
	if (_channel_selector_widget == 0) {
	  
	  	if(_region.channel_selector_scoped_note() != 0){
		    _region.channel_selector_scoped_note()->hide_channel_selector();
		    _region.set_channel_selector_scoped_note(0);
		}

		SingleMidiChannelSelector* _channel_selector = new SingleMidiChannelSelector(_note->channel());
		_channel_selector->show_all();
		_channel_selector->channel_selected.connect(
			sigc::mem_fun(this, &NoteBase::on_channel_change));

		_channel_selector->clicked.connect (
			sigc::mem_fun (this, &NoteBase::hide_channel_selector));

		_channel_selector_widget = new Widget(*(_item->property_parent()),
		                                      x1(),
		                                      y2() + 2,
		                                      (Gtk::Widget &) *_channel_selector);

		_channel_selector_widget->hide();
		_channel_selector_widget->property_height() = 100;
		_channel_selector_widget->property_width() = 100;
		_channel_selector_widget->raise_to_top();
		_channel_selector_widget->show();
		
		_region.set_channel_selector_scoped_note(this);
	} else {
		hide_channel_selector();
	}
#endif	
}

void
NoteBase::hide_channel_selector ()
{
#if 0
	if (_channel_selector_widget) {
		_channel_selector_widget->hide();
		delete _channel_selector_widget;
		_channel_selector_widget = 0;
	}
#endif	
}

void
NoteBase::set_selected(bool selected)
{
	if (!_note) {
		return;
	}

	_selected = selected;
	set_fill_color (base_color());
        
	if (_selected) {
		set_outline_color(calculate_outline(ARDOUR_UI::config()->get_canvasvar_MidiNoteSelected()));
		
		if(_region.channel_selector_scoped_note() != 0){
		    _region.channel_selector_scoped_note()->hide_channel_selector();
		    _region.set_channel_selector_scoped_note(0);
		}
	} else {
		set_outline_color(calculate_outline(base_color()));
		hide_channel_selector();
	}

}

#define SCALE_USHORT_TO_UINT8_T(x) ((x) / 257)

uint32_t
NoteBase::base_color()
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
		                         ARDOUR_UI::config()->get_canvasvar_MidiNoteSelected(), 0.5);
	}

	case ChannelColors:
		return UINT_INTERPOLATE (UINT_RGBA_CHANGE_A (NoteBase::midi_channel_colors[_note->channel()],
		                                             opacity), 
		                         ARDOUR_UI::config()->get_canvasvar_MidiNoteSelected(), 0.5);

	default:
		return meter_style_fill_color(_note->velocity(), selected());
	};

	return 0;
}

void
NoteBase::set_mouse_fractions (GdkEvent* ev)
{
	double ix, iy;
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

	boost::optional<ArdourCanvas::Rect> bbox = _item->bounding_box ();
	assert (bbox);

	_item->canvas_to_item (ix, iy);
	/* XXX: CANVAS */
	/* hmm, something wrong here. w2i should give item-local coordinates
	   but it doesn't. for now, finesse this.
	*/
	ix = ix - bbox.get().x0;
	iy = iy - bbox.get().y0;

	/* fraction of width/height */
	double xf;
	double yf;
	bool notify = false;

	xf = ix / bbox.get().width ();
	yf = iy / bbox.get().height ();

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
NoteBase::event_handler (GdkEvent* ev)
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

	return _region.get_time_axis_view().editor().canvas_note_event (ev, _item);
}

bool
NoteBase::mouse_near_ends () const
{
	return (_mouse_x_fraction >= 0.0 && _mouse_x_fraction < 0.25) ||
		(_mouse_x_fraction >= 0.75 && _mouse_x_fraction < 1.0);
}

bool
NoteBase::big_enough_to_trim () const
{
        return (x1() - x0()) > 10;
}

