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
#include "midi_channel_selector.h"
#include <libgnomecanvasmm/text.h>
#include <libgnomecanvasmm/widget.h>
#include <ardour/midi_model.h>

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
 *
 * A newer, better canvas should remove the need for all the ugly here.
 */
class CanvasNoteEvent : public sigc::trackable {
public:
	CanvasNoteEvent(
			MidiRegionView&                       region,
			Item*                                 item,
			const boost::shared_ptr<ARDOUR::Note> note = boost::shared_ptr<ARDOUR::Note>());

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

	const boost::shared_ptr<ARDOUR::Note> note() const { return _note; }

protected:
	enum State { None, Pressed, Dragging };

	MidiRegionView&                       _region;
	Item* const                           _item;
	Text*                                 _text;
	Widget*                               _channel_selector_widget;
	State                                 _state;
	const boost::shared_ptr<ARDOUR::Note> _note;
	bool                                  _own_note;
	bool                                  _selected;
};

} // namespace Gnome
} // namespace Canvas

#endif /* __gtk_ardour_canvas_midi_event_h__ */
