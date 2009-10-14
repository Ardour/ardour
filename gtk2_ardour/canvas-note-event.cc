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
		const boost::shared_ptr<NoteType> note)
	: _region(region)
	, _item(item)
	, _text(0)
	, _channel_selector_widget()
	, _state(None)
	, _note(note)
	, _selected(false)
	, _valid (true)
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
		_text = new InteractiveText(*(_item->property_parent()), this);
	}
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
		_text = 0;
	}
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

		_channel_selector_widget = new Widget(*(_item->property_parent()),
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
	} else {
		set_fill_color(base_color());
		set_outline_color(calculate_outline(base_color()));
	}

	_selected = selected;
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
			return RGBA_TO_UINT(
					SCALE_USHORT_TO_UINT8_T(color.get_red()),
					SCALE_USHORT_TO_UINT8_T(color.get_green()),
					SCALE_USHORT_TO_UINT8_T(color.get_blue()),
					opacity);
		}

	case ChannelColors:
		return UINT_RGBA_CHANGE_A(CanvasNoteEvent::midi_channel_colors[_note->channel()],
				                  opacity);

	default:
		return meter_style_fill_color(_note->velocity());
	};

	return 0;
}

bool
CanvasNoteEvent::on_event(GdkEvent* ev)
{
	PublicEditor& editor (_region.get_time_axis_view().editor());

	if (!editor.internal_editing()) {
		return false;
	}

	switch (ev->type) {
	case GDK_ENTER_NOTIFY:
		_region.note_entered(this);
		//Keyboard::magic_widget_grab_focus();
		break;

	case GDK_LEAVE_NOTIFY:
		//Keyboard::magic_widget_drop_focus();
		_region.note_left (this);
		if (!selected()) {
			hide_velocity();
		}
		break;

	case GDK_BUTTON_PRESS:
		if (ev->button.button == 3) {
			show_channel_selector();
			return true;
		}
		break;

	case GDK_BUTTON_RELEASE:
		if (ev->button.button == 3) {
			return true;
		}
		break;

	default:
		break;
	}

	return false;
}

} // namespace Canvas
} // namespace Gnome

