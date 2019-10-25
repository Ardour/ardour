/*-
 * Copyright (c) 2007, 2008 Edward Tomasz Napierała <trasz@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * This is piano_keyboard, piano keyboard-like GTK+ widget.  It contains
 * no MIDI-specific code.
 *
 * For questions and comments, contact Edward Tomasz Napierala <trasz@FreeBSD.org>.
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

#include "gtk_pianokeyboard.h"

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

void
PianoKeyboard::draw_keyboard_cue (cairo_t* cr, int note)
{
#if 0
	int w = _notes[0].w;
	int h = _notes[0].h;

	int first_note_in_lower_row  = (_octave + 1) * 12;
	int last_note_in_lower_row   = (_octave + 2) * 12 - 1;
	int first_note_in_higher_row = (_octave + 2) * 12;
	int last_note_in_higher_row  = (_octave + 3) * 12 + 4;

	first_note_in_lower_row  = MIN (127, MAX (0, first_note_in_lower_row));
	last_note_in_lower_row   = MIN (127, MAX (0, last_note_in_lower_row));
	first_note_in_higher_row = MIN (127, MAX (0, first_note_in_higher_row));
	last_note_in_higher_row  = MIN (127, MAX (0, last_note_in_higher_row));

	cairo_set_source_rgb (cr, 1.0f, 0.0f, 0.0f);
	cairo_move_to (cr, _notes[first_note_in_lower_row].x + 3, h - 6);
	cairo_line_to (cr, _notes[last_note_in_lower_row].x + w - 3, h - 6);
	cairo_stroke (cr);

	cairo_set_source_rgb (cr, 0.0f, 0.0f, 1.0f);
	cairo_move_to (cr, _notes[first_note_in_higher_row].x + 3, h - 9);
	cairo_line_to (cr, _notes[last_note_in_higher_row].x + w - 3, h - 9);
	cairo_stroke (cr);
#endif

	int nkey = note - _octave * 12;
	if (nkey < 0 || nkey >= NNOTES) {
		return;
	}
	if (_note_bindings.find (nkey) == _note_bindings.end ()) {
		return;
	}

	// TODO Cache PangoFontDescription for each expose call.
	// TODO display above note/octave label if both are visible
	int is_white = _notes[note].white;
	int x        = _notes[note].x;
	int w        = _notes[note].w;
	int h        = _notes[note].h;

	int  tw, th;
	char buf[32];
	sprintf (buf, "ArdourMono %dpx", MAX (8, MIN (20, w / 2 + 3)));
	PangoFontDescription* font = pango_font_description_from_string (buf);
	snprintf (buf, 16, "%lc", gdk_keyval_to_unicode (gdk_keyval_to_upper (gdk_keyval_from_name (_note_bindings[nkey].c_str ()))));
	PangoLayout* pl = pango_cairo_create_layout (cr);
	pango_layout_set_font_description (pl, font);
	pango_layout_set_text (pl, buf, -1);
	pango_layout_set_alignment (pl, PANGO_ALIGN_LEFT);
	pango_layout_get_pixel_size (pl, &tw, &th);

	if (is_white) {
		cairo_set_source_rgba (cr, 0.0, 0.0, 0.5, 1.0);
	} else {
		cairo_set_source_rgba (cr, 1.0, 1.0, 0.5, 1.0);
	}

	if (tw < w) {
		cairo_save (cr);
		cairo_move_to (cr, x + (w - tw) / 2, h - th - 5);
		pango_cairo_show_layout (cr, pl);
		cairo_restore (cr);
	}
	g_object_unref (pl);
	pango_font_description_free (font);
}

void
PianoKeyboard::queue_note_draw (int note)
{
	Gdk::Rectangle            rect;
	Glib::RefPtr<Gdk::Window> win = get_window ();

	rect.set_x (_notes[note].x);
	rect.set_y (0);
	rect.set_width (_notes[note].w);
	rect.set_height (_notes[note].h);

	win->invalidate_rect (rect, true); // ->  queue_draw_area ()
}

void
PianoKeyboard::draw_note (cairo_t* cr, int note)
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

	if (_enable_keyboard_cue) {
		draw_keyboard_cue (cr, note);
	} else if (_print_note_label && (note % 12) == 0) {
		int  tw, th;
		char buf[32];
		sprintf (buf, "ArdourMono %dpx", MAX (10, MIN (20, MIN (w / 2 + 3, h / 7))));
		PangoFontDescription* font = pango_font_description_from_string (buf);
		sprintf (buf, "C%2d", (note / 12) - 1);
		PangoLayout* pl = pango_cairo_create_layout (cr);
		pango_layout_set_font_description (pl, font);
		pango_layout_set_text (pl, buf, -1);
		pango_layout_set_alignment (pl, PANGO_ALIGN_LEFT);
		pango_layout_get_pixel_size (pl, &tw, &th);

		if (th < w && tw < h * .3) {
			cairo_save (cr);
			cairo_move_to (cr, x + (w - th) / 2, h - 3);
			cairo_rotate (cr, M_PI / -2.0);
			cairo_set_source_rgba (cr, .0, .0, .0, 1.0);
			pango_cairo_show_layout (cr, pl);
			cairo_restore (cr);
		}
		g_object_unref (pl);
		pango_font_description_free (font);
	}

	/* We need to redraw black keys that partially obscure the white one. */
	if (note < NNOTES - 2 && !_notes[note + 1].white) {
		draw_note (cr, note + 1);
	}

	if (note > 0 && !_notes[note - 1].white) {
		draw_note (cr, note - 1);
	}
}

int
PianoKeyboard::press_key (int key, int vel)
{
	assert (key >= 0);
	assert (key < NNOTES);

	_maybe_stop_sustained_notes = false;

	/* This is for keyboard autorepeat protection. */
	if (_notes[key].pressed)
		return 0;

	if (_sustain_new_notes) {
		_notes[key].sustained = 1;
	} else {
		_notes[key].sustained = 0;
	}

	if (_monophonic && _last_key != key) {
		_notes[_last_key].pressed   = 0;
		_notes[_last_key].sustained = 0;
		queue_note_draw (_last_key);
	}
	_last_key = key;

	_notes[key].pressed = 1;

	NoteOn (key, vel); /* EMIT SIGNAL */
	queue_note_draw (key);

	return 1;
}

int
PianoKeyboard::release_key (int key)
{
	assert (key >= 0);
	assert (key < NNOTES);

	_maybe_stop_sustained_notes = false;

	if (!_notes[key].pressed)
		return 0;

	if (_sustain_new_notes) {
		_notes[key].sustained = 1;
	}

	_notes[key].pressed = 0;

	if (_notes[key].sustained)
		return 0;

	NoteOff (key); /* EMIT SIGNAL */
	queue_note_draw (key);

	return 1;
}

void
PianoKeyboard::stop_unsustained_notes ()
{
	int i;
	for (i = 0; i < NNOTES; ++i) {
		if (_notes[i].pressed && !_notes[i].sustained) {
			_notes[i].pressed = 0;
			NoteOff (i); /* EMIT SIGNAL */
			queue_note_draw (i);
		}
	}
}

void
PianoKeyboard::stop_sustained_notes ()
{
	int i;
	for (i = 0; i < NNOTES; ++i) {
		if (_notes[i].sustained) {
			_notes[i].pressed   = 0;
			_notes[i].sustained = 0;
			NoteOff (i); /* EMIT SIGNAL */
			queue_note_draw (i);
		}
	}
}

int
PianoKeyboard::key_binding (const char* key)
{
	if (_key_bindings.find (key) != _key_bindings.end ()) {
		return _key_bindings.at (key);
	}
	return -1;
}

void
PianoKeyboard::bind_key (const char* key, int note)
{
	_key_bindings[key]   = note;
	_note_bindings[note] = key;
}

void
PianoKeyboard::clear_notes ()
{
	_key_bindings.clear ();
	_note_bindings.clear ();
}

void
PianoKeyboard::bind_keys_qwerty ()
{
	clear_notes ();

	bind_key ("space", 128);

	/* Lower keyboard row - "zxcvbnm". */
	bind_key ("z", 12); /* C0 */
	bind_key ("s", 13);
	bind_key ("x", 14);
	bind_key ("d", 15);
	bind_key ("c", 16);
	bind_key ("v", 17);
	bind_key ("g", 18);
	bind_key ("b", 19);
	bind_key ("h", 20);
	bind_key ("n", 21);
	bind_key ("j", 22);
	bind_key ("m", 23);

	/* Upper keyboard row, first octave - "qwertyu". */
	bind_key ("q", 24);
	bind_key ("2", 25);
	bind_key ("w", 26);
	bind_key ("3", 27);
	bind_key ("e", 28);
	bind_key ("r", 29);
	bind_key ("5", 30);
	bind_key ("t", 31);
	bind_key ("6", 32);
	bind_key ("y", 33);
	bind_key ("7", 34);
	bind_key ("u", 35);

	/* Upper keyboard row, the rest - "iop". */
	bind_key ("i", 36);
	bind_key ("9", 37);
	bind_key ("o", 38);
	bind_key ("0", 39);
	bind_key ("p", 40);
}

void
PianoKeyboard::bind_keys_qwertz ()
{
	bind_keys_qwerty ();

	/* The only difference between QWERTY and QWERTZ is that the "y" and "z" are swapped together. */
	bind_key ("y", 12);
	bind_key ("z", 33);
}

void
PianoKeyboard::bind_keys_azerty ()
{
	clear_notes ();

	bind_key ("space", 128);

	/* Lower keyboard row - "wxcvbn,". */
	bind_key ("w", 12); /* C0 */
	bind_key ("s", 13);
	bind_key ("x", 14);
	bind_key ("d", 15);
	bind_key ("c", 16);
	bind_key ("v", 17);
	bind_key ("g", 18);
	bind_key ("b", 19);
	bind_key ("h", 20);
	bind_key ("n", 21);
	bind_key ("j", 22);
	bind_key ("comma", 23);

	/* Upper keyboard row, first octave - "azertyu". */
	bind_key ("a", 24);
	bind_key ("eacute", 25);
	bind_key ("z", 26);
	bind_key ("quotedbl", 27);
	bind_key ("e", 28);
	bind_key ("r", 29);
	bind_key ("parenleft", 30);
	bind_key ("t", 31);
	bind_key ("minus", 32);
	bind_key ("y", 33);
	bind_key ("egrave", 34);
	bind_key ("u", 35);

	/* Upper keyboard row, the rest - "iop". */
	bind_key ("i", 36);
	bind_key ("ccedilla", 37);
	bind_key ("o", 38);
	bind_key ("agrave", 39);
	bind_key ("p", 40);
}

void
PianoKeyboard::bind_keys_dvorak ()
{
	clear_notes ();

	bind_key ("space", 128);

	/* Lower keyboard row - ";qjkxbm". */
	bind_key ("semicolon", 12); /* C0 */
	bind_key ("o", 13);
	bind_key ("q", 14);
	bind_key ("e", 15);
	bind_key ("j", 16);
	bind_key ("k", 17);
	bind_key ("i", 18);
	bind_key ("x", 19);
	bind_key ("d", 20);
	bind_key ("b", 21);
	bind_key ("h", 22);
	bind_key ("m", 23);
	bind_key ("w", 24); /* overlaps with upper row */
	bind_key ("n", 25);
	bind_key ("v", 26);
	bind_key ("s", 27);
	bind_key ("z", 28);

	/* Upper keyboard row, first octave - "',.pyfg". */
	bind_key ("apostrophe", 24);
	bind_key ("2", 25);
	bind_key ("comma", 26);
	bind_key ("3", 27);
	bind_key ("period", 28);
	bind_key ("p", 29);
	bind_key ("5", 30);
	bind_key ("y", 31);
	bind_key ("6", 32);
	bind_key ("f", 33);
	bind_key ("7", 34);
	bind_key ("g", 35);

	/* Upper keyboard row, the rest - "crl". */
	bind_key ("c", 36);
	bind_key ("9", 37);
	bind_key ("r", 38);
	bind_key ("0", 39);
	bind_key ("l", 40);
#if 0
	bind_key("slash", 41); /* extra F */
	bind_key("bracketright", 42);
	bind_key("equal", 43);
#endif
}

bool
PianoKeyboard::on_key_press_event (GdkEventKey* event)
{
	int   note;
	char* key;
	guint keyval;

	GdkKeymapKey kk;

	/* We're not using event->keyval, because we need keyval with level set to 0.
	   E.g. if user holds Shift and presses '7', we want to get a '7', not '&'. */
	kk.keycode = event->hardware_keycode;
	kk.level   = 0;
	kk.group   = 0;

	keyval = gdk_keymap_lookup_key (NULL, &kk);

	key = gdk_keyval_name (gdk_keyval_to_lower (keyval));

	if (key == NULL) {
		g_message ("gtk_keyval_name() returned NULL; please report this.");
		return false;
	}

	note = key_binding (key);

	if (note < 0) {
		return false;
	}

	if (note == 128) {
		if (event->type == GDK_KEY_RELEASE) {
			Rest (); /* EMIT SIGNAL */
		}

		return true;
	}

	note += _octave * 12;

	assert (note >= 0);
	assert (note < NNOTES);

	if (event->type == GDK_KEY_PRESS) {
		press_key (note, _key_velocity);
	} else if (event->type == GDK_KEY_RELEASE) {
		release_key (note);
	}

	return true;
}

bool
PianoKeyboard::on_key_release_event (GdkEventKey* event)
{
	return on_key_press_event (event);
}

int
PianoKeyboard::get_note_for_xy (int x, int y) const
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
PianoKeyboard::get_velocity_for_note_at_y (int note, int y) const
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
PianoKeyboard::on_button_press_event (GdkEventButton* event)
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
PianoKeyboard::on_button_release_event (GdkEventButton* event)
{
	return on_button_press_event (event);
}

bool
PianoKeyboard::on_motion_notify_event (GdkEventMotion* event)
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
PianoKeyboard::on_expose_event (GdkEventExpose* event)
{
	cairo_t* cr = gdk_cairo_create (GDK_DRAWABLE (get_window ()->gobj ()));
	cairo_rectangle (cr, event->area.x, event->area.y, event->area.width, event->area.height);
	cairo_clip (cr);

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

	cairo_destroy (cr);
	return true;
}

void
PianoKeyboard::on_size_request (Gtk::Requisition* requisition)
{
	requisition->width  = PIANO_KEYBOARD_DEFAULT_WIDTH;
	requisition->height = PIANO_KEYBOARD_DEFAULT_HEIGHT;
}

int
PianoKeyboard::is_black (int key) const
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
PianoKeyboard::black_key_left_shift (int key) const
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
PianoKeyboard::recompute_dimensions ()
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
PianoKeyboard::on_size_allocate (Gtk::Allocation& allocation)
{
	DrawingArea::on_size_allocate (allocation);
	recompute_dimensions ();
}

PianoKeyboard::PianoKeyboard ()
{
	using namespace Gdk;
	add_events (KEY_PRESS_MASK | KEY_RELEASE_MASK | BUTTON_PRESS_MASK | BUTTON_RELEASE_MASK | POINTER_MOTION_MASK | POINTER_MOTION_HINT_MASK);

	_maybe_stop_sustained_notes     = false;
	_sustain_new_notes              = false;
	_enable_keyboard_cue            = false;
	_highlight_grand_piano_range    = false;
	_print_note_label               = false;
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

	bind_keys_qwerty ();
}

PianoKeyboard::~PianoKeyboard ()
{
}

void
PianoKeyboard::set_keyboard_cue (bool enabled)
{
	_enable_keyboard_cue = enabled;
	queue_draw ();
}

void
PianoKeyboard::set_grand_piano_highlight (bool enabled)
{
	_highlight_grand_piano_range = enabled;
	queue_draw ();
}

void
PianoKeyboard::show_note_label (bool enabled)
{
	_print_note_label = enabled;
	queue_draw ();
}

void
PianoKeyboard::set_monophonic (bool monophonic)
{
	_monophonic = monophonic;
}

void
PianoKeyboard::set_velocities (int min_vel, int max_vel, int key_vel)
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
PianoKeyboard::sustain_press ()
{
	if (!_sustain_new_notes) {
		_sustain_new_notes          = true;
		_maybe_stop_sustained_notes = true;
	}
}

void
PianoKeyboard::sustain_release ()
{
	if (_maybe_stop_sustained_notes) {
		stop_sustained_notes ();
	}
	_sustain_new_notes = false;
}

void
PianoKeyboard::set_note_on (int note)
{
	if (_notes[note].pressed == 0) {
		_notes[note].pressed = 1;
		queue_note_draw (note);
	}
}

void
PianoKeyboard::set_note_off (int note)
{
	if (_notes[note].pressed || _notes[note].sustained) {
		_notes[note].pressed   = 0;
		_notes[note].sustained = 0;
		queue_note_draw (note);
	}
}

void
PianoKeyboard::set_octave (int octave)
{
	stop_unsustained_notes ();

	if (octave < -1) {
		octave = -1;
	} else if (octave > 7) {
		octave = 7;
	}

	_octave = octave;
	set_octave_range (_octave_range);
}

void
PianoKeyboard::set_octave_range (int octave_range)
{
	stop_unsustained_notes ();

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
PianoKeyboard::set_keyboard_layout (Layout layout)
{
	switch (layout) {
		case QWERTY:
			bind_keys_qwerty ();
			break;
		case QWERTZ:
			bind_keys_qwertz ();
			break;
		case AZERTY:
			bind_keys_azerty ();
			break;
		case DVORAK:
			bind_keys_dvorak ();
			break;
	}
	queue_draw ();
}
