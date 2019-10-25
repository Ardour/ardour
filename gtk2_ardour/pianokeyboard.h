/*
 * Copyright (C) 2010-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
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

#ifndef _PIANO_KEYBOARD_H_
#define _PIANO_KEYBOARD_H_

#include <map>
#include <gtkmm/drawingarea.h>

#define NNOTES (128)

class APianoKeyboard : public Gtk::DrawingArea
{
public:
	APianoKeyboard ();
	~APianoKeyboard ();

	sigc::signal<void, int, int> NoteOn;
	sigc::signal<void, int>      NoteOff;
	sigc::signal<void>           Rest;

	enum Layout {
		QWERTY,
		QWERTZ,
		AZERTY,
		DVORAK
	};

	void sustain_press ();
	void sustain_release ();

	void set_note_on (int note);
	void set_note_off (int note);

	void set_keyboard_cue (bool enabled);
	void set_grand_piano_highlight (bool enabled);
	void show_note_label (bool enabled);

	void set_monophonic (bool monophonic);
	void set_octave (int octave);
	void set_octave_range (int octave_range);
	void set_keyboard_layout (Layout layout);
	void set_velocities (int min_vel, int max_vel, int key_vel);

protected:
	bool on_key_press_event (GdkEventKey*);
	bool on_key_release_event (GdkEventKey*);
	bool on_button_press_event (GdkEventButton*);
	bool on_button_release_event (GdkEventButton*);
	bool on_motion_notify_event (GdkEventMotion*);
	bool on_expose_event (GdkEventExpose*);

	void on_size_request (Gtk::Requisition*);
	void on_size_allocate (Gtk::Allocation&);

private:
	void annotate_layout (cairo_t* cr, int note) const;
	void annotate_note (cairo_t* cr, int note) const;
	void draw_note (cairo_t* cr, int note) const;

	void queue_note_draw (int note);

	void press_key (int key, int vel);
	void release_key (int key);
	void stop_sustained_notes ();
	void stop_unsustained_notes ();

	int  key_binding (const char* key) const;
	void bind_key (const char* key, int note);
	void clear_notes ();

	void bind_keys_qwerty ();
	void bind_keys_qwertz ();
	void bind_keys_azerty ();
	void bind_keys_dvorak ();

	int get_note_for_xy (int x, int y) const;
	int get_velocity_for_note_at_y (int note, int y) const;

	int    is_black (int key) const;
	double black_key_left_shift (int key) const;

	void recompute_dimensions ();

	struct PKNote {
		PKNote ()
		        : pressed (false)
		        , sustained (false)
		        , white (false)
		        , x (0)
		        , w (0)
		        , h (0)
		{}

		bool pressed;   /* true if key is in pressed down state. */
		bool sustained; /* true if note is sustained. */
		bool white;     /* true if key is white; 0 otherwise. */
		int  x;         /* Distance between the left edge of the key and the left edge of the widget, in pixels. */
		int  w;         /* Width of the key, in pixels. */
		int  h;         /* Height of the key, in pixels. */
	};

	bool _maybe_stop_sustained_notes;
	bool _sustain_new_notes;
	bool _enable_keyboard_cue;
	bool _highlight_grand_piano_range;
	bool _print_note_label;
	int  _octave;
	int  _octave_range;
	int  _note_being_pressed_using_mouse;
	int  _min_note;
	int  _max_note;
	int  _last_key;
	bool _monophonic;
	int  _min_velocity;
	int  _max_velocity;
	int  _key_velocity;

	PKNote _notes[NNOTES];

	std::map<std::string, int> _key_bindings;  /**< Table used to translate from PC keyboard character to MIDI note number. */
	std::map<int, std::string> _note_bindings; /**< Table to translate from MIDI note number to PC keyboard character. */

	/* these are only valid during expose/draw */
	PangoFontDescription* _font_cue;
	PangoFontDescription* _font_octave;
};

#endif
