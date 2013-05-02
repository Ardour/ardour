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

#ifndef __gtk_ardour_canvas_midi_event_h__
#define __gtk_ardour_canvas_midi_event_h__

#include <boost/shared_ptr.hpp>
#include <libgnomecanvasmm/text.h>
#include <libgnomecanvasmm/widget.h>

#include "ardour/midi_model.h"

#include "rgb_macros.h"
#include "ardour_ui.h"
#include "canvas-noevent-text.h"
#include "ui_config.h"

class Editor;
class MidiRegionView;

namespace Evoral { template<typename T> class Note; }

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
class CanvasNoteEvent : virtual public sigc::trackable
{
  public:
	typedef Evoral::Note<ARDOUR::MidiModel::TimeType> NoteType;

	CanvasNoteEvent(
			MidiRegionView&                   region,
			Item*                             item,
			const boost::shared_ptr<NoteType> note = boost::shared_ptr<NoteType>());

	virtual ~CanvasNoteEvent();

        static PBD::Signal1<void,CanvasNoteEvent*> CanvasNoteEventDeleted;

	virtual void show() = 0;
	virtual void hide() = 0;
	virtual bool on_event(GdkEvent* ev);

	bool valid() const { return _valid; }
	void invalidate ();
	void validate ();

	bool selected() const { return _selected; }
	void set_selected(bool yn);

	virtual void move_event(double dx, double dy) = 0;

	uint32_t base_color();

	void show_velocity();
	void hide_velocity();

	/** Channel changed for this specific event */
	void on_channel_change(uint8_t channel);

	/** Channel selection changed */
	void on_channel_selection_change(uint16_t selection);

	virtual void set_outline_color(uint32_t c) = 0;
	virtual void set_fill_color(uint32_t c) = 0;

	virtual double x1() const = 0;
	virtual double y1() const = 0;
	virtual double x2() const = 0;
	virtual double y2() const = 0;

        float mouse_x_fraction() const { return _mouse_x_fraction; }
        float mouse_y_fraction() const { return _mouse_y_fraction; }

	const boost::shared_ptr<NoteType> note() const { return _note; }
	MidiRegionView& region_view() const { return _region; }

	inline static uint32_t meter_style_fill_color(uint8_t vel, bool selected) {
                if (selected) {
                        if (vel < 64) {
                                return UINT_INTERPOLATE(
					ARDOUR_UI::config()->canvasvar_SelectedMidiNoteColorBase.get(),
					ARDOUR_UI::config()->canvasvar_SelectedMidiNoteColorMid.get(),
					(vel / (double)63.0));
                        } else {
                                return UINT_INTERPOLATE(
					ARDOUR_UI::config()->canvasvar_SelectedMidiNoteColorMid.get(),
					ARDOUR_UI::config()->canvasvar_SelectedMidiNoteColorTop.get(),
					((vel-64) / (double)63.0));
                        }
                } else {
                        if (vel < 64) {
                                return UINT_INTERPOLATE(
					ARDOUR_UI::config()->canvasvar_MidiNoteColorBase.get(),
					ARDOUR_UI::config()->canvasvar_MidiNoteColorMid.get(),
					(vel / (double)63.0));
                        } else {
                                return UINT_INTERPOLATE(
					ARDOUR_UI::config()->canvasvar_MidiNoteColorMid.get(),
					ARDOUR_UI::config()->canvasvar_MidiNoteColorTop.get(),
					((vel-64) / (double)63.0));
                        }
                }
	}

	/// calculate outline colors from fill colors of notes
	inline static uint32_t calculate_outline(uint32_t color) {
		return UINT_INTERPOLATE(color, 0x000000ff, 0.5);
	}

	/// hue circle divided into 16 equal-looking parts, courtesy Thorsten Wilms
	static const uint32_t midi_channel_colors[16];

        bool mouse_near_ends () const;
        bool big_enough_to_trim () const;

  protected:
	enum State { None, Pressed, Dragging };

	MidiRegionView&                   _region;
	Item* const                       _item;
	NoEventText*                      _text;
	State                             _state;
	const boost::shared_ptr<NoteType> _note;
	bool                              _selected;
	bool                              _valid;
        float                             _mouse_x_fraction;
        float                             _mouse_y_fraction;
	uint16_t                          _channel_selection;

        void set_mouse_fractions (GdkEvent*);
};

} // namespace Gnome
} // namespace Canvas

#endif /* __gtk_ardour_canvas_midi_event_h__ */
