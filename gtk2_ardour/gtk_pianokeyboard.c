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
#  include <pango/pango.h>
#include <pango/pangocairo.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "gtk_pianokeyboard.h"

#ifndef MIN
#  define MIN(A, B) ((A) < (B)) ? (A) : (B)
#endif

#ifndef MAX
#  define MAX(A, B) ((A) > (B)) ? (A) : (B)
#endif

#define PIANO_KEYBOARD_DEFAULT_WIDTH 730
#define PIANO_KEYBOARD_DEFAULT_HEIGHT 70

enum {
	NOTE_ON_SIGNAL,
	NOTE_OFF_SIGNAL,
	REST_SIGNAL,
	LAST_SIGNAL
};

static guint piano_keyboard_signals[LAST_SIGNAL] = { 0 };

static void
draw_keyboard_cue (PianoKeyboard* pk, cairo_t* cr)
{
	int w = pk->notes[0].w;
	int h = pk->notes[0].h;

	int first_note_in_lower_row  = (pk->octave + 1) * 12;
	int last_note_in_lower_row   = (pk->octave + 2) * 12 - 1;
	int first_note_in_higher_row = (pk->octave + 2) * 12;
	int last_note_in_higher_row  = (pk->octave + 3) * 12 + 4;

	first_note_in_lower_row  = MIN (127, MAX (0, first_note_in_lower_row));
	last_note_in_lower_row   = MIN (127, MAX (0, last_note_in_lower_row));
	first_note_in_higher_row = MIN (127, MAX (0, first_note_in_higher_row));
	last_note_in_higher_row  = MIN (127, MAX (0, last_note_in_higher_row));

	cairo_set_source_rgb (cr, 1.0f, 0.0f, 0.0f);
	cairo_move_to (cr, pk->notes[first_note_in_lower_row].x + 3, h - 6);
	cairo_line_to (cr, pk->notes[last_note_in_lower_row].x + w - 3, h - 6);
	cairo_stroke (cr);

	cairo_set_source_rgb (cr, 0.0f, 0.0f, 1.0f);
	cairo_move_to (cr, pk->notes[first_note_in_higher_row].x + 3, h - 9);
	cairo_line_to (cr, pk->notes[last_note_in_higher_row].x + w - 3, h - 9);
	cairo_stroke (cr);
}

static void
queue_note_draw (PianoKeyboard* pk, int note)
{
	GdkWindow* w = GTK_WIDGET (pk)->window;

	if (w) {
		GdkRectangle r;

		r.x      = pk->notes[note].x;
		r.y      = 0;
		r.width  = pk->notes[note].w;
		r.height = pk->notes[note].h;

		gdk_window_invalidate_rect (w, &r, TRUE);
	}
}

static void
draw_note (PianoKeyboard* pk, cairo_t* cr, int note)
{
	if (note < pk->min_note || note > pk->max_note) {
		return;
	}

	int is_white = pk->notes[note].white;
	int x        = pk->notes[note].x;
	int w        = pk->notes[note].w;
	int h        = pk->notes[note].h;

	if (pk->notes[note].pressed || pk->notes[note].sustained) {
		if (is_white) {
			cairo_set_source_rgb (cr, 0.7, 0.5, 0.5);
		} else {
			cairo_set_source_rgb (cr, 0.6, 0.4, 0.4);
		}
	} else if (pk->highlight_grand_piano_range && (note < PIANO_MIN_NOTE || note > PIANO_MAX_NOTE)) {
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

	if (pk->enable_keyboard_cue) {
		draw_keyboard_cue (pk, cr);
	}

	if (pk->print_note_label && (note % 12) == 0) {
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
	if (note < NNOTES - 2 && !pk->notes[note + 1].white) {
		draw_note (pk, cr, note + 1);
	}

	if (note > 0 && !pk->notes[note - 1].white) {
		draw_note (pk, cr, note - 1);
	}
}

static int
press_key (PianoKeyboard* pk, int key, int vel)
{
	assert (key >= 0);
	assert (key < NNOTES);

	pk->maybe_stop_sustained_notes = 0;

	/* This is for keyboard autorepeat protection. */
	if (pk->notes[key].pressed)
		return 0;

	if (pk->sustain_new_notes)
		pk->notes[key].sustained = 1;
	else
		pk->notes[key].sustained = 0;

	if (pk->monophonic && pk->last_key != key) {
		pk->notes[pk->last_key].pressed   = 0;
		pk->notes[pk->last_key].sustained = 0;
		queue_note_draw (pk, pk->last_key);
	}
	pk->last_key = key;

	pk->notes[key].pressed = 1;

	g_signal_emit_by_name (GTK_WIDGET (pk), "note-on", key, vel);
	queue_note_draw (pk, key);

	return 1;
}

static int
release_key (PianoKeyboard* pk, int key)
{
	assert (key >= 0);
	assert (key < NNOTES);

	pk->maybe_stop_sustained_notes = 0;

	if (!pk->notes[key].pressed)
		return 0;

	if (pk->sustain_new_notes)
		pk->notes[key].sustained = 1;

	pk->notes[key].pressed = 0;

	if (pk->notes[key].sustained)
		return 0;

	g_signal_emit_by_name (GTK_WIDGET (pk), "note-off", key);
	queue_note_draw (pk, key);

	return 1;
}

static void
rest (PianoKeyboard* pk)
{
	g_signal_emit_by_name (GTK_WIDGET (pk), "rest");
}

static void
stop_unsustained_notes (PianoKeyboard* pk)
{
	int i;
	for (i = 0; i < NNOTES; ++i) {
		if (pk->notes[i].pressed && !pk->notes[i].sustained) {
			pk->notes[i].pressed = 0;
			g_signal_emit_by_name (GTK_WIDGET (pk), "note-off", i);
			queue_note_draw (pk, i);
		}
	}
}

static void
stop_sustained_notes (PianoKeyboard* pk)
{
	int i;
	for (i = 0; i < NNOTES; ++i) {
		if (pk->notes[i].sustained) {
			pk->notes[i].pressed   = 0;
			pk->notes[i].sustained = 0;
			g_signal_emit_by_name (GTK_WIDGET (pk), "note-off", i);
			queue_note_draw (pk, i);
		}
	}
}

static int
key_binding (PianoKeyboard* pk, const char* key)
{
	gpointer notused, note;
	gboolean found;

	assert (pk->key_bindings != NULL);

	found = g_hash_table_lookup_extended (pk->key_bindings, key, &notused, &note);

	if (!found)
		return -1;

	return (intptr_t)note;
}

static void
bind_key (PianoKeyboard* pk, const char* key, int note)
{
	assert (pk->key_bindings != NULL);

	g_hash_table_insert (pk->key_bindings, (const gpointer)key, (gpointer) ((intptr_t)note));
}

static void
clear_notes (PianoKeyboard* pk)
{
	assert (pk->key_bindings != NULL);

	g_hash_table_remove_all (pk->key_bindings);
}

static void
bind_keys_qwerty (PianoKeyboard* pk)
{
	clear_notes (pk);

	bind_key (pk, "space", 128);

	/* Lower keyboard row - "zxcvbnm". */
	bind_key (pk, "z", 12); /* C0 */
	bind_key (pk, "s", 13);
	bind_key (pk, "x", 14);
	bind_key (pk, "d", 15);
	bind_key (pk, "c", 16);
	bind_key (pk, "v", 17);
	bind_key (pk, "g", 18);
	bind_key (pk, "b", 19);
	bind_key (pk, "h", 20);
	bind_key (pk, "n", 21);
	bind_key (pk, "j", 22);
	bind_key (pk, "m", 23);

	/* Upper keyboard row, first octave - "qwertyu". */
	bind_key (pk, "q", 24);
	bind_key (pk, "2", 25);
	bind_key (pk, "w", 26);
	bind_key (pk, "3", 27);
	bind_key (pk, "e", 28);
	bind_key (pk, "r", 29);
	bind_key (pk, "5", 30);
	bind_key (pk, "t", 31);
	bind_key (pk, "6", 32);
	bind_key (pk, "y", 33);
	bind_key (pk, "7", 34);
	bind_key (pk, "u", 35);

	/* Upper keyboard row, the rest - "iop". */
	bind_key (pk, "i", 36);
	bind_key (pk, "9", 37);
	bind_key (pk, "o", 38);
	bind_key (pk, "0", 39);
	bind_key (pk, "p", 40);
}

static void
bind_keys_qwertz (PianoKeyboard* pk)
{
	bind_keys_qwerty (pk);

	/* The only difference between QWERTY and QWERTZ is that the "y" and "z" are swapped together. */
	bind_key (pk, "y", 12);
	bind_key (pk, "z", 33);
}

static void
bind_keys_azerty (PianoKeyboard* pk)
{
	clear_notes (pk);

	bind_key (pk, "space", 128);

	/* Lower keyboard row - "wxcvbn,". */
	bind_key (pk, "w", 12); /* C0 */
	bind_key (pk, "s", 13);
	bind_key (pk, "x", 14);
	bind_key (pk, "d", 15);
	bind_key (pk, "c", 16);
	bind_key (pk, "v", 17);
	bind_key (pk, "g", 18);
	bind_key (pk, "b", 19);
	bind_key (pk, "h", 20);
	bind_key (pk, "n", 21);
	bind_key (pk, "j", 22);
	bind_key (pk, "comma", 23);

	/* Upper keyboard row, first octave - "azertyu". */
	bind_key (pk, "a", 24);
	bind_key (pk, "eacute", 25);
	bind_key (pk, "z", 26);
	bind_key (pk, "quotedbl", 27);
	bind_key (pk, "e", 28);
	bind_key (pk, "r", 29);
	bind_key (pk, "parenleft", 30);
	bind_key (pk, "t", 31);
	bind_key (pk, "minus", 32);
	bind_key (pk, "y", 33);
	bind_key (pk, "egrave", 34);
	bind_key (pk, "u", 35);

	/* Upper keyboard row, the rest - "iop". */
	bind_key (pk, "i", 36);
	bind_key (pk, "ccedilla", 37);
	bind_key (pk, "o", 38);
	bind_key (pk, "agrave", 39);
	bind_key (pk, "p", 40);
}

static void
bind_keys_dvorak (PianoKeyboard* pk)
{
	clear_notes (pk);

	bind_key (pk, "space", 128);

	/* Lower keyboard row - ";qjkxbm". */
	bind_key (pk, "semicolon", 12); /* C0 */
	bind_key (pk, "o", 13);
	bind_key (pk, "q", 14);
	bind_key (pk, "e", 15);
	bind_key (pk, "j", 16);
	bind_key (pk, "k", 17);
	bind_key (pk, "i", 18);
	bind_key (pk, "x", 19);
	bind_key (pk, "d", 20);
	bind_key (pk, "b", 21);
	bind_key (pk, "h", 22);
	bind_key (pk, "m", 23);
	bind_key (pk, "w", 24); /* overlaps with upper row */
	bind_key (pk, "n", 25);
	bind_key (pk, "v", 26);
	bind_key (pk, "s", 27);
	bind_key (pk, "z", 28);

	/* Upper keyboard row, first octave - "',.pyfg". */
	bind_key (pk, "apostrophe", 24);
	bind_key (pk, "2", 25);
	bind_key (pk, "comma", 26);
	bind_key (pk, "3", 27);
	bind_key (pk, "period", 28);
	bind_key (pk, "p", 29);
	bind_key (pk, "5", 30);
	bind_key (pk, "y", 31);
	bind_key (pk, "6", 32);
	bind_key (pk, "f", 33);
	bind_key (pk, "7", 34);
	bind_key (pk, "g", 35);

	/* Upper keyboard row, the rest - "crl". */
	bind_key (pk, "c", 36);
	bind_key (pk, "9", 37);
	bind_key (pk, "r", 38);
	bind_key (pk, "0", 39);
	bind_key (pk, "l", 40);
#if 0
	bind_key(pk, "slash", 41); /* extra F */
	bind_key(pk, "bracketright", 42);
	bind_key(pk, "equal", 43);
#endif
}

static gint
keyboard_event_handler (GtkWidget* mk, GdkEventKey* event, gpointer ignored)
{
	int   note;
	char* key;
	guint keyval;

	GdkKeymapKey   kk;
	PianoKeyboard* pk = PIANO_KEYBOARD (mk);

	(void)ignored;

	/* We're not using event->keyval, because we need keyval with level set to 0.
	   E.g. if user holds Shift and presses '7', we want to get a '7', not '&'. */
	kk.keycode = event->hardware_keycode;
	kk.level   = 0;
	kk.group   = 0;

	keyval = gdk_keymap_lookup_key (NULL, &kk);

	key = gdk_keyval_name (gdk_keyval_to_lower (keyval));

	if (key == NULL) {
		g_message ("gtk_keyval_name() returned NULL; please report this.");
		return FALSE;
	}

	note = key_binding (pk, key);

	if (note < 0) {
		return FALSE;
	}

	if (note == 128) {
		if (event->type == GDK_KEY_RELEASE) {
			rest (pk);
		}

		return TRUE;
	}

	note += pk->octave * 12;

	assert (note >= 0);
	assert (note < NNOTES);

	if (event->type == GDK_KEY_PRESS) {
		press_key (pk, note, pk->key_velocity);
	} else if (event->type == GDK_KEY_RELEASE) {
		release_key (pk, note);
	}

	return TRUE;
}

static int
get_note_for_xy (PianoKeyboard* pk, int x, int y)
{
	int height = GTK_WIDGET (pk)->allocation.height;
	int note;

	if (y <= ((height * 2) / 3)) { /* might be a black key */
		for (note = 0; note <= pk->max_note; ++note) {
			if (pk->notes[note].white) {
				continue;
			}

			if (x >= pk->notes[note].x && x <= pk->notes[note].x + pk->notes[note].w) {
				return note;
			}
		}
	}

	for (note = 0; note <= pk->max_note; ++note) {
		if (!pk->notes[note].white) {
			continue;
		}

		if (x >= pk->notes[note].x && x <= pk->notes[note].x + pk->notes[note].w) {
			return note;
		}
	}

	return -1;
}

static int
get_velocity_for_note_at_y (PianoKeyboard* pk, int note, int y)
{
	if (note < 0) {
		return 0;
	}
	int vel = pk->min_velocity + (pk->max_velocity - pk->min_velocity) * y / pk->notes[note].h;

	if (vel < 1) {
		return 1;
	} else if (vel > 127) {
		return 127;
	}
	return vel;
}

static gboolean
mouse_button_event_handler (PianoKeyboard* pk, GdkEventButton* event, gpointer ignored)
{
	int x = event->x;
	int y = event->y;

	int note = get_note_for_xy (pk, x, y);

	(void)ignored;

	if (event->button != 1)
		return TRUE;

	if (event->type == GDK_BUTTON_PRESS) {
		if (note < 0) {
			return TRUE;
		}

		if (pk->note_being_pressed_using_mouse >= 0) {
			release_key (pk, pk->note_being_pressed_using_mouse);
		}

		press_key (pk, note, get_velocity_for_note_at_y (pk, note, y));
		pk->note_being_pressed_using_mouse = note;

	} else if (event->type == GDK_BUTTON_RELEASE) {
		if (note >= 0) {
			release_key (pk, note);
		} else {
			if (pk->note_being_pressed_using_mouse >= 0) {
				release_key (pk, pk->note_being_pressed_using_mouse);
			}
		}
		pk->note_being_pressed_using_mouse = -1;
	}

	return TRUE;
}

static gboolean
mouse_motion_event_handler (PianoKeyboard* pk, GdkEventMotion* event, gpointer ignored)
{
	int note;

	(void)ignored;

	if ((event->state & GDK_BUTTON1_MASK) == 0)
		return TRUE;

	int x = event->x;
	int y = event->y;

	note = get_note_for_xy (pk, x, y);

	if (note != pk->note_being_pressed_using_mouse && note >= 0) {
		if (pk->note_being_pressed_using_mouse >= 0) {
			release_key (pk, pk->note_being_pressed_using_mouse);
		}
		press_key (pk, note, get_velocity_for_note_at_y (pk, note, y));
		pk->note_being_pressed_using_mouse = note;
	}

	return TRUE;
}

static gboolean
piano_keyboard_expose (GtkWidget* widget, GdkEventExpose* event)
{
	int            i;
	PianoKeyboard* pk = PIANO_KEYBOARD (widget);
	cairo_t*       cr = gdk_cairo_create (GDK_DRAWABLE (GTK_WIDGET (pk)->window));

	gdk_cairo_region (cr, event->region);
	cairo_clip (cr);

	for (i = 0; i < NNOTES; ++i) {
		GdkRectangle r;

		r.x      = pk->notes[i].x;
		r.y      = 0;
		r.width  = pk->notes[i].w;
		r.height = pk->notes[i].h;

		switch (gdk_region_rect_in (event->region, &r)) {
			case GDK_OVERLAP_RECTANGLE_PART:
			case GDK_OVERLAP_RECTANGLE_IN:
				draw_note (pk, cr, i);
				break;
			default:
				break;
		}
	}

	cairo_destroy (cr);

	return TRUE;
}

static void
piano_keyboard_size_request (GtkWidget* w, GtkRequisition* requisition)
{
	(void)w;

	requisition->width  = PIANO_KEYBOARD_DEFAULT_WIDTH;
	requisition->height = PIANO_KEYBOARD_DEFAULT_HEIGHT;
}

static int
is_black (int key)
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

static double
black_key_left_shift (int key)
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

static void
recompute_dimensions (PianoKeyboard* pk)
{
	int note;
	int number_of_white_keys = 0;
	int skipped_white_keys   = 0;

	for (note = pk->min_note; note <= pk->max_note; ++note) {
		if (!is_black (note)) {
			++number_of_white_keys;
		}
	}
	for (note = 0; note < pk->min_note; ++note) {
		if (!is_black (note)) {
			++skipped_white_keys;
		}
	}

	int width  = GTK_WIDGET (pk)->allocation.width;
	int height = GTK_WIDGET (pk)->allocation.height;

	int key_width       = width / number_of_white_keys;
	int black_key_width = key_width * 0.8;
	int useful_width    = number_of_white_keys * key_width;

	pk->widget_margin = (width - useful_width) / 2;

	int white_key;
	for (note = 0, white_key = -skipped_white_keys; note < NNOTES; ++note) {
		if (is_black (note)) {
			/* This note is black key. */
			pk->notes[note].x = pk->widget_margin +
			                    (white_key * key_width) -
			                    (black_key_width * black_key_left_shift (note));
			pk->notes[note].w     = black_key_width;
			pk->notes[note].h     = (height * 2) / 3;
			pk->notes[note].white = 0;
			continue;
		}

		/* This note is white key. */
		pk->notes[note].x     = pk->widget_margin + white_key * key_width;
		pk->notes[note].w     = key_width;
		pk->notes[note].h     = height;
		pk->notes[note].white = 1;

		white_key++;
	}
}

static void
piano_keyboard_size_allocate (GtkWidget* widget, GtkAllocation* allocation)
{
	/* XXX: Are these two needed? */
	g_return_if_fail (widget != NULL);
	g_return_if_fail (allocation != NULL);

	widget->allocation = *allocation;

	recompute_dimensions (PIANO_KEYBOARD (widget));

	if (GTK_WIDGET_REALIZED (widget)) {
		gdk_window_move_resize (widget->window, allocation->x, allocation->y, allocation->width, allocation->height);
	}
}

typedef void (*GMarshalFunc_VOID__INT_INT) (gpointer data1,
                                            gint     arg1,
                                            gint     arg2,
                                            gpointer data2);
static void
g_cclosure_user_marshal_VOID__INT_INT (GClosure* closure,
                                       GValue* return_value G_GNUC_UNUSED,
                                       guint                n_param_values,
                                       const GValue*        param_values,
                                       gpointer invocation_hint G_GNUC_UNUSED,
                                       gpointer                 marshal_data)
{
	GCClosure*                 cc = (GCClosure*)closure;
	gpointer                   data1, data2;
	GMarshalFunc_VOID__INT_INT callback;

	g_return_if_fail (n_param_values == 3);

	if (G_CCLOSURE_SWAP_DATA (closure)) {
		data1 = closure->data;
		data2 = g_value_peek_pointer (param_values + 0);
	} else {
		data1 = g_value_peek_pointer (param_values + 0);
		data2 = closure->data;
	}
	callback = (GMarshalFunc_VOID__INT_INT) (marshal_data ? marshal_data : cc->callback);

	callback (data1,
	          (param_values + 1)->data[0].v_int,
	          (param_values + 2)->data[0].v_int,
	          data2);
}

static void
piano_keyboard_class_init (PianoKeyboardClass* klass)
{
	GtkWidgetClass* widget_klass;

	/* Set up signals. */
	piano_keyboard_signals[NOTE_ON_SIGNAL] = g_signal_new ("note-on",
	                                                       G_TYPE_FROM_CLASS (klass), (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION),
	                                                       0, NULL, NULL, g_cclosure_user_marshal_VOID__INT_INT, G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_INT);

	piano_keyboard_signals[NOTE_OFF_SIGNAL] = g_signal_new ("note-off",
	                                                        G_TYPE_FROM_CLASS (klass), (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION),
	                                                        0, NULL, NULL, g_cclosure_marshal_VOID__INT, G_TYPE_NONE, 1, G_TYPE_INT);

	piano_keyboard_signals[REST_SIGNAL] = g_signal_new ("rest",
	                                                    G_TYPE_FROM_CLASS (klass), (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION),
	                                                    0, NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

	widget_klass = (GtkWidgetClass*)klass;

	widget_klass->expose_event  = piano_keyboard_expose;
	widget_klass->size_request  = piano_keyboard_size_request;
	widget_klass->size_allocate = piano_keyboard_size_allocate;
}

static void
piano_keyboard_init (GtkWidget* mk)
{
	gtk_widget_add_events (mk, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);

	g_signal_connect (G_OBJECT (mk), "button-press-event", G_CALLBACK (mouse_button_event_handler), NULL);
	g_signal_connect (G_OBJECT (mk), "button-release-event", G_CALLBACK (mouse_button_event_handler), NULL);
	g_signal_connect (G_OBJECT (mk), "motion-notify-event", G_CALLBACK (mouse_motion_event_handler), NULL);
	g_signal_connect (G_OBJECT (mk), "key-press-event", G_CALLBACK (keyboard_event_handler), NULL);
	g_signal_connect (G_OBJECT (mk), "key-release-event", G_CALLBACK (keyboard_event_handler), NULL);
}

GType
piano_keyboard_get_type (void)
{
	static GType mk_type = 0;

	if (!mk_type) {
		static const GTypeInfo mk_info = {
			sizeof (PianoKeyboardClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc)piano_keyboard_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (PianoKeyboard),
			0, /* n_preallocs */
			(GInstanceInitFunc)piano_keyboard_init,
			0, /* value_table */
		};

		mk_type = g_type_register_static (GTK_TYPE_DRAWING_AREA, "PianoKeyboard", &mk_info, (GTypeFlags)0);
	}

	return mk_type;
}

GtkWidget*
piano_keyboard_new (void)
{
	GtkWidget* widget = (GtkWidget*)gtk_type_new (piano_keyboard_get_type ());

	PianoKeyboard* pk = PIANO_KEYBOARD (widget);

	pk->maybe_stop_sustained_notes     = 0;
	pk->sustain_new_notes              = 0;
	pk->enable_keyboard_cue            = FALSE;
	pk->highlight_grand_piano_range    = FALSE;
	pk->print_note_label               = FALSE;
	pk->octave                         = 4;
	pk->octave_range                   = 7;
	pk->note_being_pressed_using_mouse = -1;
	pk->min_note                       = 0;
	pk->max_note                       = 127;
	pk->last_key                       = 0;
	pk->monophonic                     = FALSE;

	pk->min_velocity = 1;
	pk->max_velocity = 127;
	pk->key_velocity = 100;

	memset ((void*)pk->notes, 0, sizeof (struct PKNote) * NNOTES);

	pk->key_bindings = g_hash_table_new (g_str_hash, g_str_equal);
	bind_keys_qwerty (pk);

	return widget;
}

void
piano_keyboard_set_keyboard_cue (PianoKeyboard* pk, gboolean enabled)
{
	pk->enable_keyboard_cue = enabled;
	gtk_widget_queue_draw (GTK_WIDGET (pk));
}

void
piano_keyboard_set_grand_piano_highlight (PianoKeyboard* pk, gboolean enabled)
{
	pk->highlight_grand_piano_range = enabled;
	gtk_widget_queue_draw (GTK_WIDGET (pk));
}

void
piano_keyboard_show_note_label (PianoKeyboard* pk, gboolean enabled)
{
	pk->print_note_label = enabled;
	gtk_widget_queue_draw (GTK_WIDGET (pk));
}

void
piano_keyboard_set_monophonic (PianoKeyboard* pk, gboolean monophonic)
{
	pk->monophonic = monophonic;
}

void
piano_keyboard_set_velocities (PianoKeyboard* pk, int min_vel, int max_vel, int key_vel)
{
	if (min_vel <= max_vel && min_vel > 0 && max_vel < 128) {
		pk->min_velocity = min_vel;
		pk->max_velocity = max_vel;
	}

	if (key_vel > 0 && key_vel < 128) {
		pk->key_velocity = key_vel;
	}
}

void
piano_keyboard_sustain_press (PianoKeyboard* pk)
{
	if (!pk->sustain_new_notes) {
		pk->sustain_new_notes          = 1;
		pk->maybe_stop_sustained_notes = 1;
	}
}

void
piano_keyboard_sustain_release (PianoKeyboard* pk)
{
	if (pk->maybe_stop_sustained_notes)
		stop_sustained_notes (pk);

	pk->sustain_new_notes = 0;
}

void
piano_keyboard_set_note_on (PianoKeyboard* pk, int note)
{
	if (pk->notes[note].pressed == 0) {
		pk->notes[note].pressed = 1;
		queue_note_draw (pk, note);
	}
}

void
piano_keyboard_set_note_off (PianoKeyboard* pk, int note)
{
	if (pk->notes[note].pressed || pk->notes[note].sustained) {
		pk->notes[note].pressed   = 0;
		pk->notes[note].sustained = 0;
		queue_note_draw (pk, note);
	}
}

void
piano_keyboard_set_octave (PianoKeyboard* pk, int octave)
{
	stop_unsustained_notes (pk);

	if (pk->octave < -1) {
		pk->octave = -1;
	} else if (pk->octave > 7) {
		pk->octave = 7;
	}

	pk->octave = octave;
	piano_keyboard_set_octave_range (pk, pk->octave_range);
	gtk_widget_queue_draw (GTK_WIDGET (pk));
}

void
piano_keyboard_set_octave_range (PianoKeyboard* pk, int octave_range)
{
	stop_unsustained_notes (pk);

	if (octave_range < 2) {
		octave_range = 2;
	}
	if (octave_range > 11) {
		octave_range = 11;
	}

	pk->octave_range = octave_range;

	/* -1 <= pk->octave <= 7
	 * key-bindings are at offset 12 .. 40
	 * default piano range: octave = 4, range = 7 -> note 21..108
	 */

	switch (octave_range) {
		default:
			assert (0);
			break;
		case 2:
		case 3:
			pk->min_note = (pk->octave + 1) * 12;
			break;
		case 4:
		case 5:
			pk->min_note = (pk->octave + 0) * 12;
			break;
		case 6:
			pk->min_note = (pk->octave - 1) * 12;
			break;
		case 7:
		case 8:
			pk->min_note = (pk->octave - 2) * 12;
			break;
		case 9:
		case 10:
			pk->min_note = (pk->octave - 3) * 12;
			break;
		case 11:
			pk->min_note = (pk->octave - 4) * 12;
			break;
	}

	int upper_offset = 0;

	if (pk->min_note < 3) {
		upper_offset = 0;
		pk->min_note = 0;
	} else if (octave_range > 5) {
		/* extend down to A */
		upper_offset = 3;
		pk->min_note -= 3;
	}

	pk->max_note = MIN (127, upper_offset + pk->min_note + octave_range * 12);

	if (pk->max_note == 127) {
		pk->min_note = MAX (0, pk->max_note - octave_range * 12);
	}

	recompute_dimensions (pk);
	gtk_widget_queue_draw (GTK_WIDGET (pk));
}

void
piano_keyboard_set_keyboard_layout (PianoKeyboard* pk, const char* layout)
{
	assert (layout);

	if (!g_ascii_strcasecmp (layout, "QWERTY")) {
		bind_keys_qwerty (pk);

	} else if (!g_ascii_strcasecmp (layout, "QWERTZ")) {
		bind_keys_qwertz (pk);

	} else if (!g_ascii_strcasecmp (layout, "AZERTY")) {
		bind_keys_azerty (pk);

	} else if (!g_ascii_strcasecmp (layout, "DVORAK")) {
		bind_keys_dvorak (pk);
	} else {
		assert (0);
	}
}
