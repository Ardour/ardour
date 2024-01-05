/*
 * Copyright (C) 2013-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2015 Tim Mayberry <mojofunk@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __gtk_ardour_note_base_h__
#define __gtk_ardour_note_base_h__

#include <memory>

#include "temporal/beats.h"
#include "canvas/types.h"
#include "gtkmm2ext/colors.h"

#include "ardour/types.h"

#include "rgb_macros.h"
#include "ui_config.h"

class Editor;
class MidiView;

namespace Evoral {
	template<typename T> class Note;
	class Beats;
}

namespace ArdourCanvas {
	class Item;
	class Text;
}

/** Base class for canvas notes (sustained note rectangles, percussive hit diamonds,
 * and velocity lollipops)
 *
 * This is not actually a canvas item itself to avoid the dreaded diamond
 * inheritance pattern, since various types of canvas items (Note (rect), Hit
 * (diamond), etc) need to share this functionality but can't share an
 * ancestor.
 *
 * Note: Because of this, derived classes need to manually bounce events to
 * on_event, it won't happen automatically.
 */
class NoteBase : public sigc::trackable
{
  private:
	enum Flags {
		Selected = 0x1,
		HideSelection = 0x2,
	};

  public:
	typedef Evoral::Note<Temporal::Beats> NoteType;

	NoteBase (MidiView& region, bool, const std::shared_ptr<NoteType> note = std::shared_ptr<NoteType>());
	virtual ~NoteBase ();

	void set_item (ArdourCanvas::Item *);
	ArdourCanvas::Item* item() const { return _item; }

	virtual void show() = 0;
	virtual void hide() = 0;

	bool valid() const { return _valid; }
	void invalidate ();
	void validate ();

	bool selected() const { return _flags & Selected; }
	void set_selected(bool yn);
	void set_hide_selection (bool yn);

	virtual void move_event(double dx, double dy) = 0;

	uint32_t base_color();
	static uint32_t base_color (int velocity, ARDOUR::ColorMode color_mode, Gtkmm2ext::Color, int channel, bool selected);

	void show_velocity();
	void hide_velocity();

	/** Channel changed for this specific event */
	void on_channel_change(uint8_t channel);

	/** Channel selection changed */
	void on_channel_selection_change(uint16_t selection);

	virtual void set_outline_color(uint32_t c) = 0;
	virtual void set_fill_color(uint32_t c) = 0;

	virtual void set_ignore_events(bool ignore) = 0;

	virtual ArdourCanvas::Coord x0 () const = 0;
	virtual ArdourCanvas::Coord y0 () const = 0;
	virtual ArdourCanvas::Coord x1 () const = 0;
	virtual ArdourCanvas::Coord y1 () const = 0;

	virtual void set_velocity (double) {}
	virtual double visual_velocity() const = 0;

	float mouse_x_fraction() const { return _mouse_x_fraction; }
	float mouse_y_fraction() const { return _mouse_y_fraction; }

	const std::shared_ptr<NoteType> note() const { return _note; }
	MidiView& region_view() const { return _region; }

	static void set_colors ();

	static Gtkmm2ext::Color meter_style_fill_color(uint8_t vel, bool selected);

	/// calculate outline colors from fill colors of notes
	inline static uint32_t calculate_outline(uint32_t color, bool showing_selection = false) {
		if (showing_selection) {
			return _selected_col;
		} else {
			return UINT_INTERPOLATE(color, 0x000000ff, 0.5);
		}
	}

	/// hue circle divided into 16 equal-looking parts, courtesy Thorsten Wilms
	static const uint32_t midi_channel_colors[16];

	bool mouse_near_ends () const;
	virtual bool big_enough_to_trim () const;

protected:
	enum State { None, Pressed, Dragging };

	MidiView&                         _region;
	ArdourCanvas::Item*               _item;
	ArdourCanvas::Text*               _text;
	State                             _state;
	const std::shared_ptr<NoteType>   _note;
	bool                              _with_events;
	bool                              _own_note;
	Flags                             _flags;
	bool                              _valid;
	float                             _mouse_x_fraction;
	float                             _mouse_y_fraction;

	void set_mouse_fractions (GdkEvent*);

private:
	bool event_handler (GdkEvent *);

	static Gtkmm2ext::Color _selected_col;
	static Gtkmm2ext::SVAModifier color_modifier;
	static Gtkmm2ext::Color velocity_color_table[128];
	static bool _color_init;
};

#endif /* __gtk_ardour_note_h__ */
