/* Piano-keyboard based on jack-keyboard
 *
 * Copyright (C) 2019 Robin Gareus <robin@gareus.org>
 * Copyright (c) 2007, 2008 Edward Tomasz Napierała <trasz@FreeBSD.org>
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

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#include <cairo/cairo.h>
#include <pango/pango.h>
#include <pango/pangocairo.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "gtkmm2ext/keyboard.h"

#include "pianokeyboard.h"

#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif

#ifndef MIN
#  define MIN(A, B) ((A) < (B)) ? (A) : (B)
#endif

#ifndef MAX
#  define MAX(A, B) ((A) > (B)) ? (A) : (B)
#endif

#define PIANO_KEYBOARD_DEFAULT_WIDTH 730
#define PIANO_KEYBOARD_DEFAULT_HEIGHT 70

#define PIANO_MIN_NOTE 21
#define PIANO_MAX_NOTE 108
#define OCTAVE_MIN (-1)
#define OCTAVE_MAX (7)

void
APianoKeyboard::annotate_layout (cairo_t* cr, int note) const
{
	int nkey = note - _octave * 12;

	if (nkey < 0 || nkey >= NNOTES) {
		return;
	}

	const char* key_name = _keyboard_layout.note_binding (nkey);
	if (!key_name) {
		return;
	}

	int x = _notes[note].x;
	int w = _notes[note].w;
	int h = _notes[note].h;

	int  tw, th;
	char buf[32];
	snprintf (buf, 16, "%lc",
			gdk_keyval_to_unicode (gdk_keyval_to_upper (gdk_keyval_from_name (key_name))));
	PangoLayout* pl = pango_cairo_create_layout (cr);
	pango_layout_set_font_description (pl, _font_cue);
	pango_layout_set_text (pl, buf, -1);
	pango_layout_set_alignment (pl, PANGO_ALIGN_LEFT);
	pango_layout_get_pixel_size (pl, &tw, &th);

	if (_notes[note].white) {
		cairo_set_source_rgba (cr, 0.0, 0.0, 0.5, 1.0);
	} else {
		cairo_set_source_rgba (cr, 1.0, 1.0, 0.5, 1.0);
	}

	if (tw < w) {
		cairo_save (cr);
		if (_notes[note].white) {
			cairo_move_to (cr, x + (w - tw) / 2, h * 2 / 3 + 3);
		} else {
			cairo_move_to (cr, x + (w - tw) / 2, h - th - 3);
		}
		pango_cairo_show_layout (cr, pl);
		cairo_restore (cr);
	}
	g_object_unref (pl);
}

void
APianoKeyboard::annotate_note (cairo_t* cr, int note) const
{
	assert ((note % 12) == 0);

	int x = _notes[note].x;
	int w = _notes[note].w;
	int h = _notes[note].h;

	int  tw, th;
	char buf[32];
	sprintf (buf, "C%d", (note / 12) - 1);
	PangoLayout* pl = pango_cairo_create_layout (cr);
	pango_layout_set_font_description (pl, _font_octave);
	pango_layout_set_text (pl, buf, -1);
	pango_layout_set_alignment (pl, PANGO_ALIGN_LEFT);
	pango_layout_get_pixel_size (pl, &tw, &th);

	if (th < w && tw < h * .3) {
		cairo_save (cr);
		cairo_move_to (cr, x + (w - th) / 2, h - 3);
		cairo_rotate (cr, M_PI / -2.0);

		cairo_set_line_width (cr, 1.0);
		cairo_set_source_rgba (cr, 0, 0, 0, 1.0);
		pango_cairo_show_layout (cr, pl);

#if 0
		cairo_rel_move_to (cr, -.5, -.5);
		pango_cairo_update_layout (cr, pl);
		cairo_set_source_rgba (cr, 1, 1, 1, 0.3);
		pango_cairo_layout_path (cr, pl);
		cairo_set_line_width (cr, 1.5);
		cairo_stroke (cr);
#endif

		cairo_restore (cr);
	}
	g_object_unref (pl);
}

void
APianoKeyboard::draw_note (cairo_t* cr, int note) const
{
	if (note < _min_note || note > _max_note) {
		return;
	}

	int is_white = _notes[note].white;
	int x        = _notes[note].x;
	int w        = _notes[note].w;
	int h        = _notes[note].h;

	if (_notes[note].pressed || _notes[note].sustained) {
		if (is_white) {
			cairo_set_source_rgb (cr, 0.7, 0.5, 0.5);
		} else {
			cairo_set_source_rgb (cr, 0.6, 0.4, 0.4);
		}
	} else if (_highlight_grand_piano_range && (note < PIANO_MIN_NOTE || note > PIANO_MAX_NOTE)) {
		if (is_white) {
			cairo_set_source_rgb (cr, 0.7, 0.7, 0.7);
		} else {
			cairo_set_source_rgb (cr, 0.3, 0.3, 0.3);
		}
	} else {
		if (is_white) {
			cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
		} else {
			cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
		}
	}

	cairo_set_line_width (cr, 1.0);

	cairo_rectangle (cr, x, 0, w, h);
	cairo_fill (cr);

	cairo_set_source_rgb (cr, 0.0f, 0.0f, 0.0f); /* black outline */
	cairo_rectangle (cr, x, 0, w, h);
	cairo_stroke (cr);

	if (_annotate_octave && (note % 12) == 0) {
		annotate_note (cr, note);
	}

	if (_annotate_layout) {
		annotate_layout (cr, note);
	}

	/* We need to redraw black keys that partially obscure the white one. */
	if (note < NNOTES - 2 && !_notes[note + 1].white) {
		draw_note (cr, note + 1);
	}

	if (note > 0 && !_notes[note - 1].white) {
		draw_note (cr, note - 1);
	}
}

void
APianoKeyboard::queue_note_draw (int note)
{
	queue_draw_area (_notes[note].x, 0, _notes[note].w, _notes[note].h);
}

void
APianoKeyboard::press_key (int key, int vel)
{
	assert (key >= 0);
	assert (key < NNOTES);

	/* This is for keyboard autorepeat protection. */
	if (_notes[key].pressed) {
		return;
	}

	if (_sustain_new_notes) {
		_notes[key].sustained = true;
	} else {
		_notes[key].sustained = false;
	}

	if (_monophonic && _last_key != key) {
		bool signal_off = _notes[_last_key].pressed || _notes[_last_key].sustained;
		_notes[_last_key].pressed = false;
		_notes[_last_key].sustained = false;
		if (signal_off) {
			NoteOff (_last_key); /* EMIT SIGNAL */
		}
		queue_note_draw (_last_key);
	}
	_last_key = key;

	_notes[key].pressed = true;

	NoteOn (key, vel); /* EMIT SIGNAL */
	queue_note_draw (key);
}

void
APianoKeyboard::release_key (int key)
{
	assert (key >= 0);
	assert (key < NNOTES);

	if (!_notes[key].pressed) {
		return;
	}

	if (_sustain_new_notes) {
		_notes[key].sustained = true;
	}

	_notes[key].pressed = false;

	if (_notes[key].sustained) {
		return;
	}

	NoteOff (key); /* EMIT SIGNAL */
	queue_note_draw (key);
}

void
APianoKeyboard::stop_unsustained_notes ()
{
	for (int i = 0; i < NNOTES; ++i) {
		if (_notes[i].pressed && !_notes[i].sustained) {
			_notes[i].pressed = false;
			NoteOff (i); /* EMIT SIGNAL */
			queue_note_draw (i);
		}
	}
}

void
APianoKeyboard::stop_sustained_notes ()
{
	for (int i = 0; i < NNOTES; ++i) {
		if (_notes[i].sustained) {
			_notes[i].sustained = false;
			if (_notes[i].pressed) {
				continue;
			}
			NoteOff (i); /* EMIT SIGNAL */
			queue_note_draw (i);
		}
	}
}

bool
APianoKeyboard::handle_fixed_keys (GdkEventKey* ev)
{
	if (ev->type == GDK_KEY_PRESS) {
		switch (ev->keyval) {
			case GDK_KEY_Left:
				SwitchOctave (false);
				return true;
			case GDK_KEY_Right:
				SwitchOctave (true);
				return true;
			case GDK_KEY_F1:
				PitchBend (0, false);
				return true;
			case GDK_KEY_F2:
				PitchBend (4096, false);
				return true;
			case GDK_KEY_F3:
				PitchBend (12288, false);
				return true;
			case GDK_KEY_F4:
				PitchBend (16383, false);
				return true;
			case GDK_KEY_Down:
				PitchBend (0, true);
				return true;
			case GDK_KEY_Up:
				PitchBend (16383, true);
				return true;
			default:
				break;
		}
	} else if (ev->type == GDK_KEY_RELEASE) {
		switch (ev->keyval) {
			case GDK_KEY_F1:
				/* fallthrough */
			case GDK_KEY_F2:
				/* fallthrough */
			case GDK_KEY_F3:
				/* fallthrough */
			case GDK_KEY_F4:
				PitchBend (8192, false);
				break;
			case GDK_KEY_Up:
				/* fallthrough */
			case GDK_KEY_Down:
				PitchBend (8192, true);
				return true;
			default:
				break;
		}
	}
	return false;
}

bool
APianoKeyboard::on_key_press_event (GdkEventKey* event)
{
	if (Gtkmm2ext::Keyboard::modifier_state_contains (event->state, Gtkmm2ext::Keyboard::PrimaryModifier)) {
		return false;
	}
	if (handle_fixed_keys (event)) {
		return true;
	}

	char const* key = PianoKeyBindings::get_keycode (event);
	int note = _keyboard_layout.key_binding (key);

	if (note < -1) {
		return true;
	}
	else if (note < 0) {
		return false;
	}

	if (note == 128) {
		/* Rest is used on release */
		return false;
	}
	if (note == 129) {
		sustain_press ();
		return true;
	}

	std::map<std::string, int>::const_iterator kv = _note_stack.find (key);
	if (kv != _note_stack.end ()) {
		/* key is already pressed, ignore event.
		 * this can happen when changing the octave with the mouse
		 * while playing.
		 */
		return true;
	}

	note += _octave * 12;

	assert (key);
	assert (note >= 0);
	assert (note < NNOTES);

	_note_stack[key] = note;

	press_key (note, _key_velocity);

	return true;
}

bool
APianoKeyboard::on_key_release_event (GdkEventKey* event)
{
	if (Gtkmm2ext::Keyboard::modifier_state_contains (event->state, Gtkmm2ext::Keyboard::PrimaryModifier)) {
		return false;
	}
	if (handle_fixed_keys (event)) {
		return true;
	}

	char const* key = PianoKeyBindings::get_keycode (event);

	if (!key) {
		return false;
	}

	int note = _keyboard_layout.key_binding (key);

	if (note == 128) {
		Rest (); /* EMIT SIGNAL */
		return true;
	}
	if (note == 129) {
		sustain_release ();
		return true;
	}
	if (note < -1) {
		return true;
	}

	std::map<std::string, int>::const_iterator kv = _note_stack.find (key);
	if (kv == _note_stack.end ()) {
		return note != -1;
	}

	release_key (kv->second);
	_note_stack.erase (key);

	return true;
}

int
APianoKeyboard::get_note_for_xy (int x, int y) const
{
	int height = get_height ();
	int note;

	if (y <= ((height * 2) / 3)) { /* might be a black key */
		for (note = 0; note <= _max_note; ++note) {
			if (_notes[note].white) {
				continue;
			}

			if (x >= _notes[note].x && x <= _notes[note].x + _notes[note].w) {
				return note;
			}
		}
	}

	for (note = 0; note <= _max_note; ++note) {
		if (!_notes[note].white) {
			continue;
		}

		if (x >= _notes[note].x && x <= _notes[note].x + _notes[note].w) {
			return note;
		}
	}

	return -1;
}

int
APianoKeyboard::get_velocity_for_note_at_y (int note, int y) const
{
	if (note < 0) {
		return 0;
	}
	int vel = _min_velocity + (_max_velocity - _min_velocity) * y / _notes[note].h;

	if (vel < 1) {
		return 1;
	} else if (vel > 127) {
		return 127;
	}
	return vel;
}

bool
APianoKeyboard::on_button_press_event (GdkEventButton* event)
{
	int x = event->x;
	int y = event->y;

	int note = get_note_for_xy (x, y);

	if (event->button != 1)
		return true;

	if (event->type == GDK_BUTTON_PRESS) {
		if (note < 0) {
			return true;
		}

		if (_note_being_pressed_using_mouse >= 0) {
			release_key (_note_being_pressed_using_mouse);
		}

		press_key (note, get_velocity_for_note_at_y (note, y));
		_note_being_pressed_using_mouse = note;

	} else if (event->type == GDK_BUTTON_RELEASE) {
		if (note >= 0) {
			release_key (note);
		} else {
			if (_note_being_pressed_using_mouse >= 0) {
				release_key (_note_being_pressed_using_mouse);
			}
		}
		_note_being_pressed_using_mouse = -1;
	}

	return true;
}

bool
APianoKeyboard::on_button_release_event (GdkEventButton* event)
{
	return on_button_press_event (event);
}

bool
APianoKeyboard::on_motion_notify_event (GdkEventMotion* event)
{
	int note;

	if ((event->state & GDK_BUTTON1_MASK) == 0)
		return true;

	int x = event->x;
	int y = event->y;

	note = get_note_for_xy (x, y);

	if (note != _note_being_pressed_using_mouse && note >= 0) {
		if (_note_being_pressed_using_mouse >= 0) {
			release_key (_note_being_pressed_using_mouse);
		}
		press_key (note, get_velocity_for_note_at_y (note, y));
		_note_being_pressed_using_mouse = note;
	}

	return true;
}

bool
APianoKeyboard::on_expose_event (GdkEventExpose* event)
{
	cairo_t* cr = gdk_cairo_create (GDK_DRAWABLE (get_window ()->gobj ()));
	cairo_rectangle (cr, event->area.x, event->area.y, event->area.width, event->area.height);
	cairo_clip (cr);

	char buf[32];
	sprintf (buf, "ArdourMono %dpx", MAX (8, MIN (20, _notes[1].w / 2 + 3)));
	_font_cue = pango_font_description_from_string (buf);
	sprintf (buf, "ArdourMono %dpx", MAX (8, MIN (20, MIN (_notes[0].w * 11 / 15 , _notes[0].h / 7))));
	_font_octave = pango_font_description_from_string (buf);

	for (int i = 0; i < NNOTES; ++i) {
		GdkRectangle r;

		r.x      = _notes[i].x;
		r.y      = 0;
		r.width  = _notes[i].w;
		r.height = _notes[i].h;

		switch (gdk_region_rect_in (event->region, &r)) {
			case GDK_OVERLAP_RECTANGLE_PART:
			case GDK_OVERLAP_RECTANGLE_IN:
				draw_note (cr, i);
				break;
			default:
				break;
		}
	}

	pango_font_description_free (_font_cue);
	pango_font_description_free (_font_octave);

	cairo_destroy (cr);
	return true;
}

void
APianoKeyboard::on_size_request (Gtk::Requisition* requisition)
{
	requisition->width  = PIANO_KEYBOARD_DEFAULT_WIDTH;
	requisition->height = PIANO_KEYBOARD_DEFAULT_HEIGHT;
	if (_annotate_layout) {
		requisition->height += 16;
	}
	if (_annotate_octave) {
		requisition->height += 24;
	}
}

int
APianoKeyboard::is_black (int key) const
{
	int note_in_octave = key % 12;
	switch (note_in_octave) {
		case 1:
		case 3:
		case 6:
		case 8:
		case 10:
			return 1;
		default:
			return 0;
	}
	return 0;
}

double
APianoKeyboard::black_key_left_shift (int key) const
{
	int note_in_octave = key % 12;
	switch (note_in_octave) {
		case 1:
			return 2.0 / 3.0;
		case 3:
			return 1.0 / 3.0;
		case 6:
			return 2.0 / 3.0;
		case 8:
			return 0.5;
		case 10:
			return 1.0 / 3.0;
		default:
			return 0;
	}
	return 0;
}

void
APianoKeyboard::recompute_dimensions ()
{
	int note;
	int number_of_white_keys = 0;
	int skipped_white_keys   = 0;

	for (note = _min_note; note <= _max_note; ++note) {
		if (!is_black (note)) {
			++number_of_white_keys;
		}
	}
	for (note = 0; note < _min_note; ++note) {
		if (!is_black (note)) {
			++skipped_white_keys;
		}
	}

	assert (number_of_white_keys > 0);

	int width  = get_width ();
	int height = get_height ();

	int key_width       = width / number_of_white_keys;
	int black_key_width = key_width * 0.8;
	int useful_width    = number_of_white_keys * key_width;

	int widget_margin = (width - useful_width) / 2;

	int white_key;
	for (note = 0, white_key = -skipped_white_keys; note < NNOTES; ++note) {
		if (is_black (note)) {
			/* This note is black key. */
			_notes[note].x = widget_margin +
			                 (white_key * key_width) -
			                 (black_key_width * black_key_left_shift (note));
			_notes[note].w     = black_key_width;
			_notes[note].h     = (height * 2) / 3;
			_notes[note].white = 0;
			continue;
		}

		/* This note is white key. */
		_notes[note].x     = widget_margin + white_key * key_width;
		_notes[note].w     = key_width;
		_notes[note].h     = height;
		_notes[note].white = 1;

		white_key++;
	}
}

void
APianoKeyboard::on_size_allocate (Gtk::Allocation& allocation)
{
	DrawingArea::on_size_allocate (allocation);
	recompute_dimensions ();
}

APianoKeyboard::APianoKeyboard ()
{
	using namespace Gdk;
	add_events (KEY_PRESS_MASK | KEY_RELEASE_MASK | BUTTON_PRESS_MASK | BUTTON_RELEASE_MASK | POINTER_MOTION_MASK | POINTER_MOTION_HINT_MASK);

	_sustain_new_notes              = false;
	_highlight_grand_piano_range    = true;
	_annotate_layout                = false;
	_annotate_octave                = false;
	_octave                         = 4;
	_octave_range                   = 7;
	_note_being_pressed_using_mouse = -1;
	_min_note                       = 0;
	_max_note                       = 127;
	_last_key                       = 0;
	_monophonic                     = false;

	_min_velocity = 1;
	_max_velocity = 127;
	_key_velocity = 100;
}

APianoKeyboard::~APianoKeyboard ()
{
}

void
APianoKeyboard::set_grand_piano_highlight (bool enabled)
{
	_highlight_grand_piano_range = enabled;
	queue_draw ();
}

void
APianoKeyboard::set_annotate_layout (bool enabled)
{
	_annotate_layout = enabled;
	queue_draw ();
}

void
APianoKeyboard::set_annotate_octave (bool enabled)
{
	_annotate_octave = enabled;
	queue_draw ();
}

void
APianoKeyboard::set_monophonic (bool monophonic)
{
	_monophonic = monophonic;
}

void
APianoKeyboard::set_velocities (int min_vel, int max_vel, int key_vel)
{
	if (min_vel <= max_vel && min_vel > 0 && max_vel < 128) {
		_min_velocity = min_vel;
		_max_velocity = max_vel;
	}

	if (key_vel > 0 && key_vel < 128) {
		_key_velocity = key_vel;
	}
}

void
APianoKeyboard::sustain_press ()
{
	if (_sustain_new_notes) {
		return;
	}
	_sustain_new_notes = true;
	SustainChanged (true); /* EMIT SIGNAL */
}

void
APianoKeyboard::sustain_release ()
{
	stop_sustained_notes ();
	if (_sustain_new_notes) {
		_sustain_new_notes = false;
		SustainChanged (false); /* EMIT SIGNAL */
	}
}

void
APianoKeyboard::reset ()
{
	sustain_release ();
	stop_unsustained_notes ();
}

void
APianoKeyboard::set_note_on (int note)
{
	if (!_notes[note].pressed) {
		_notes[note].pressed = true;
		queue_note_draw (note);
	}
}

void
APianoKeyboard::set_note_off (int note)
{
	if (_notes[note].pressed || _notes[note].sustained) {
		_notes[note].pressed   = false;
		_notes[note].sustained = false;
		queue_note_draw (note);
	}
}

void
APianoKeyboard::set_octave (int octave)
{
	if (octave < -1) {
		octave = -1;
	} else if (octave > 7) {
		octave = 7;
	}

	_octave = octave;
	set_octave_range (_octave_range);
}

void
APianoKeyboard::set_octave_range (int octave_range)
{
	if (octave_range < 2) {
		octave_range = 2;
	}
	if (octave_range > 11) {
		octave_range = 11;
	}

	_octave_range = octave_range;

	/* -1 <= _octave <= 7
	 * key-bindings are at offset 12 .. 40
	 * default piano range: _octave = 4, range = 7 -> note 21..108
	 */

	switch (_octave_range) {
		default:
			assert (0);
			break;
		case 2:
		case 3:
			_min_note = (_octave + 1) * 12;
			break;
		case 4:
		case 5:
			_min_note = (_octave + 0) * 12;
			break;
		case 6:
			_min_note = (_octave - 1) * 12;
			break;
		case 7:
		case 8:
			_min_note = (_octave - 2) * 12;
			break;
		case 9:
		case 10:
			_min_note = (_octave - 3) * 12;
			break;
		case 11:
			_min_note = (_octave - 4) * 12;
			break;
	}

	int upper_offset = 0;

	if (_min_note < 3) {
		upper_offset = 0;
		_min_note    = 0;
	} else if (_octave_range > 5) {
		/* extend down to A */
		upper_offset = 3;
		_min_note -= 3;
	}

	_max_note = MIN (127, upper_offset + _min_note + _octave_range * 12);

	if (_max_note == 127) {
		_min_note = MAX (0, _max_note - _octave_range * 12);
	}

	recompute_dimensions ();
	queue_draw ();
}

void
APianoKeyboard::set_keyboard_layout (PianoKeyBindings::Layout layout)
{
	_keyboard_layout.set_layout (layout);
	queue_draw ();
}
