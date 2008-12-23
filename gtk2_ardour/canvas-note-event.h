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

#include <boost/shared_ptr.hpp>
#include <libgnomecanvasmm/text.h>
#include <libgnomecanvasmm/widget.h>

#include "rgb_macros.h"
#include "ardour_ui.h"
#include "ui_config.h"
#include "interactive-item.h"

class Editor;
class MidiRegionView;

namespace Evoral { class Note; }

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
 *
 * A newer, better canvas should remove the need for all the ugly here.
 */
class CanvasNoteEvent : public sigc::trackable, public InteractiveItem {
public:
	CanvasNoteEvent(
			MidiRegionView&                       region,
			Item*                                 item,
			const boost::shared_ptr<Evoral::Note> note = boost::shared_ptr<Evoral::Note>());

	virtual ~CanvasNoteEvent();

	bool on_event(GdkEvent* ev);

	bool selected() const { return _selected; }
	void selected(bool yn);

	void move_event(double dx, double dy);
	
	void show_velocity();
	void hide_velocity();
	
	/**
	 * This slot is called, when a new channel is selected for the single event
	 * */
	void on_channel_change(uint8_t channel);
	void on_channel_selection_change(uint16_t selection);
	
	void show_channel_selector();
	void hide_channel_selector();

	virtual void set_outline_color(uint32_t c) = 0;
	virtual void set_fill_color(uint32_t c) = 0;
	
	virtual double x1() = 0;
	virtual double y1() = 0;
	virtual double x2() = 0;
	virtual double y2() = 0;

	const boost::shared_ptr<Evoral::Note> note() const { return _note; }
	
	inline static uint32_t meter_style_fill_color(uint8_t vel)
	{
		if (vel < 64) {
			return UINT_INTERPOLATE(
					ARDOUR_UI::config()->canvasvar_MeterColorBase.get(),
					ARDOUR_UI::config()->canvasvar_MeterColorMid.get(),
					(vel / (double)63.0));
		} else {
			return UINT_INTERPOLATE(
					ARDOUR_UI::config()->canvasvar_MeterColorMid.get(),
					ARDOUR_UI::config()->canvasvar_MeterColorTop.get(),
					((vel-64) / (double)63.0));
		}
	}

	inline static uint32_t meter_style_outline_color(uint8_t vel)
	{
		return calculate_outline(meter_style_fill_color(vel));
	}
	
	/// calculate outline colors from fill colors of notes
	inline static uint32_t calculate_outline(uint32_t color) 
	{
		return UINT_INTERPOLATE(color, 0x000000ff, 0.5);
	}
	
	/// dividing the hue circle in 16 parts, hand adjusted for equal look, courtesy Thorsten Wilms
	static const uint32_t midi_channel_colors[16];

protected:
	enum State { None, Pressed, Dragging };

	MidiRegionView&                       _region;
	Item* const                           _item;
	Text*                                 _text;
	Widget*                               _channel_selector_widget;
	State                                 _state;
	const boost::shared_ptr<Evoral::Note> _note;
	bool                                  _own_note;
	bool                                  _selected;
};

} // namespace Gnome
} // namespace Canvas

#endif /* __gtk_ardour_canvas_midi_event_h__ */
