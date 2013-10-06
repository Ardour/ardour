/*-
 * Copyright (c) 2007, 2008 Edward Tomasz NapieraÅ≈Ça <trasz@FreeBSD.org>
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
#include <string.h>
#include <stdint.h>
#include <cairo/cairo.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "gtk_pianokeyboard.h"

#define PIANO_KEYBOARD_DEFAULT_WIDTH 730
#define PIANO_KEYBOARD_DEFAULT_HEIGHT 70

enum {
	NOTE_ON_SIGNAL,
	NOTE_OFF_SIGNAL,
	REST_SIGNAL,
	LAST_SIGNAL
};

static guint	piano_keyboard_signals[LAST_SIGNAL] = { 0 };

static void
draw_keyboard_cue(PianoKeyboard *pk, cairo_t* cr)
{
	int		w = pk->notes[0].w;
	int		h = pk->notes[0].h;

	int		first_note_in_lower_row = (pk->octave + 5) * 12;
	int		last_note_in_lower_row = (pk->octave + 6) * 12 - 1;
	int		first_note_in_higher_row = (pk->octave + 6) * 12;
	int		last_note_in_higher_row = (pk->octave + 7) * 12 + 4;

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
	GdkWindow* w = GTK_WIDGET(pk)->window;

	if (w) {
		GdkRectangle r;

		r.x = pk->notes[note].x;
		r.y = 0;
		r.width = pk->notes[note].w;
		r.height = pk->notes[note].h;

		gdk_window_invalidate_rect (w, &r, TRUE);
	}
}

static void
draw_note(PianoKeyboard *pk, cairo_t* cr, int note)
{
	int		is_white = pk->notes[note].white;

	int		x = pk->notes[note].x;
	int		w = pk->notes[note].w;
	int		h = pk->notes[note].h;

	if (pk->notes[note].pressed || pk->notes[note].sustained) {
		if (is_white) {
			cairo_set_source_rgb (cr, 0.60f, 0.60f, 0.60f);
		} else {
			cairo_set_source_rgb (cr, 0.50f, 0.50f, 0.50f);
		}
	} else {
		if (is_white) {
			cairo_set_source_rgb (cr, 1.0f, 1.0f, 1.0f);
		} else {
			cairo_set_source_rgb (cr, 0.0f, 0.0f, 0.0f);
		}
	}

	cairo_set_line_width (cr, 1.0);

	cairo_rectangle (cr, x, 0, w, h);
	cairo_fill (cr);

	cairo_set_source_rgb(cr, 0.0f, 0.0f, 0.0f); /* black outline */
	cairo_rectangle (cr, x, 0, w, h);
	cairo_stroke (cr);

	if (pk->enable_keyboard_cue) {
		draw_keyboard_cue (pk, cr);
	}

	/* We need to redraw black keys that partially obscure the white one. */
	if (note < NNOTES - 2 && !pk->notes[note + 1].white) {
		draw_note(pk, cr, note + 1);
	}

	if (note > 0 && !pk->notes[note - 1].white) {
		draw_note(pk, cr, note - 1);
	}
}

static int
press_key(PianoKeyboard *pk, int key)
{
	assert(key >= 0);
	assert(key < NNOTES);

	pk->maybe_stop_sustained_notes = 0;

	/* This is for keyboard autorepeat protection. */
	if (pk->notes[key].pressed)
		return 0;

	if (pk->sustain_new_notes)
		pk->notes[key].sustained = 1;
	else
		pk->notes[key].sustained = 0;

	pk->notes[key].pressed = 1;

	g_signal_emit_by_name(GTK_WIDGET(pk), "note-on", key);
	queue_note_draw(pk, key);

	return 1;
}

static int
release_key(PianoKeyboard *pk, int key)
{
	assert(key >= 0);
	assert(key < NNOTES);

	pk->maybe_stop_sustained_notes = 0;

	if (!pk->notes[key].pressed)
		return 0;

	if (pk->sustain_new_notes)
		pk->notes[key].sustained = 1;

	pk->notes[key].pressed = 0;

	if (pk->notes[key].sustained)
		return 0;

	g_signal_emit_by_name(GTK_WIDGET(pk), "note-off", key);
	queue_note_draw(pk, key);

	return 1;
}

static void
rest (PianoKeyboard* pk)
{
	g_signal_emit_by_name(GTK_WIDGET(pk), "rest");
}

static void
stop_unsustained_notes(PianoKeyboard *pk)
{
	int		i;

	for (i = 0; i < NNOTES; i++) {
		if (pk->notes[i].pressed && !pk->notes[i].sustained) {
			pk->notes[i].pressed = 0;
			g_signal_emit_by_name(GTK_WIDGET(pk), "note-off", i);
			queue_note_draw(pk, i);
		}
	}
}

static void
stop_sustained_notes(PianoKeyboard *pk)
{
	int		i;

	for (i = 0; i < NNOTES; i++) {
		if (pk->notes[i].sustained) {
			pk->notes[i].pressed = 0;
			pk->notes[i].sustained = 0;
			g_signal_emit_by_name(GTK_WIDGET(pk), "note-off", i);
			queue_note_draw(pk, i);
		}
	}
}

static int
key_binding(PianoKeyboard *pk, const char *key)
{
	gpointer notused, note;
	gboolean found;

	assert(pk->key_bindings != NULL);

	found = g_hash_table_lookup_extended(pk->key_bindings, key, &notused, &note);

	if (!found)
		return -1;

	return (intptr_t)note;
}

static void
bind_key(PianoKeyboard *pk, const char *key, int note)
{
	assert(pk->key_bindings != NULL);

	g_hash_table_insert(pk->key_bindings, (const gpointer)key, (gpointer)((intptr_t)note));
}

static void
clear_notes(PianoKeyboard *pk)
{
	assert(pk->key_bindings != NULL);

	g_hash_table_remove_all(pk->key_bindings);
}

static void
bind_keys_qwerty(PianoKeyboard *pk)
{
	clear_notes(pk);

	bind_key(pk, "space", 128);

	/* Lower keyboard row - "zxcvbnm". */
	bind_key(pk, "z", 12);	/* C0 */
	bind_key(pk, "s", 13);
	bind_key(pk, "x", 14);
	bind_key(pk, "d", 15);
	bind_key(pk, "c", 16);
	bind_key(pk, "v", 17);
	bind_key(pk, "g", 18);
	bind_key(pk, "b", 19);
	bind_key(pk, "h", 20);
	bind_key(pk, "n", 21);
	bind_key(pk, "j", 22);
	bind_key(pk, "m", 23);

	/* Upper keyboard row, first octave - "qwertyu". */
	bind_key(pk, "q", 24);
	bind_key(pk, "2", 25);
	bind_key(pk, "w", 26);
	bind_key(pk, "3", 27);
	bind_key(pk, "e", 28);
	bind_key(pk, "r", 29);
	bind_key(pk, "5", 30);
	bind_key(pk, "t", 31);
	bind_key(pk, "6", 32);
	bind_key(pk, "y", 33);
	bind_key(pk, "7", 34);
	bind_key(pk, "u", 35);

	/* Upper keyboard row, the rest - "iop". */
	bind_key(pk, "i", 36);
	bind_key(pk, "9", 37);
	bind_key(pk, "o", 38);
	bind_key(pk, "0", 39);
	bind_key(pk, "p", 40);
}

static void
bind_keys_qwertz(PianoKeyboard *pk)
{
	bind_keys_qwerty(pk);

	/* The only difference between QWERTY and QWERTZ is that the "y" and "z" are swapped together. */
	bind_key(pk, "y", 12);
	bind_key(pk, "z", 33);
}

static void
bind_keys_azerty(PianoKeyboard *pk)
{
	clear_notes(pk);

	bind_key(pk, "space", 128);

	/* Lower keyboard row - "wxcvbn,". */
	bind_key(pk, "w", 12);	/* C0 */
	bind_key(pk, "s", 13);
	bind_key(pk, "x", 14);
	bind_key(pk, "d", 15);
	bind_key(pk, "c", 16);
	bind_key(pk, "v", 17);
	bind_key(pk, "g", 18);
	bind_key(pk, "b", 19);
	bind_key(pk, "h", 20);
	bind_key(pk, "n", 21);
	bind_key(pk, "j", 22);
	bind_key(pk, "comma", 23);

	/* Upper keyboard row, first octave - "azertyu". */
	bind_key(pk, "a", 24);
	bind_key(pk, "eacute", 25);
	bind_key(pk, "z", 26);
	bind_key(pk, "quotedbl", 27);
	bind_key(pk, "e", 28);
	bind_key(pk, "r", 29);
	bind_key(pk, "parenleft", 30);
	bind_key(pk, "t", 31);
	bind_key(pk, "minus", 32);
	bind_key(pk, "y", 33);
	bind_key(pk, "egrave", 34);
	bind_key(pk, "u", 35);

	/* Upper keyboard row, the rest - "iop". */
	bind_key(pk, "i", 36);
	bind_key(pk, "ccedilla", 37);
	bind_key(pk, "o", 38);
	bind_key(pk, "agrave", 39);
	bind_key(pk, "p", 40);
}

static gint
keyboard_event_handler(GtkWidget *mk, GdkEventKey *event, gpointer ignored)
{
	int		note;
	char		*key;
	guint		keyval;
	GdkKeymapKey	kk;
	PianoKeyboard	*pk = PIANO_KEYBOARD(mk);

        (void) ignored;

	/* We're not using event->keyval, because we need keyval with level set to 0.
	   E.g. if user holds Shift and presses '7', we want to get a '7', not '&'. */
	kk.keycode = event->hardware_keycode;
	kk.level = 0;
	kk.group = 0;

	keyval = gdk_keymap_lookup_key(NULL, &kk);

	key = gdk_keyval_name(gdk_keyval_to_lower(keyval));

	if (key == NULL) {
		g_message("gtk_keyval_name() returned NULL; please report this.");
		return FALSE;
	}

	note = key_binding(pk, key);

	if (note < 0) {
		/* Key was not bound.  Maybe it's one of the keys handled in jack-keyboard.c. */
		return FALSE;
	}

	if (note == 128) {
		if (event->type == GDK_KEY_RELEASE) {
			rest (pk);
		}

		return TRUE;
	}

	note += pk->octave * 12;

	assert(note >= 0);
	assert(note < NNOTES);

	if (event->type == GDK_KEY_PRESS) {
		press_key(pk, note);

	} else if (event->type == GDK_KEY_RELEASE) {
		release_key(pk, note);
	}

	return TRUE;
}

static int
get_note_for_xy(PianoKeyboard *pk, int x, int y)
{
	int		height = GTK_WIDGET(pk)->allocation.height;
	int		note;

	if (y <= height / 2) {
		for (note = 0; note < NNOTES - 1; note++) {
			if (pk->notes[note].white)
				continue;

			if (x >= pk->notes[note].x && x <= pk->notes[note].x + pk->notes[note].w)
				return note;
		}
	}

	for (note = 0; note < NNOTES - 1; note++) {
		if (!pk->notes[note].white)
			continue;

		if (x >= pk->notes[note].x && x <= pk->notes[note].x + pk->notes[note].w)
			return note;
	}

	return -1;
}

static gboolean
mouse_button_event_handler(PianoKeyboard *pk, GdkEventButton *event, gpointer ignored)
{
	int		x = event->x;
	int		y = event->y;

	int		note = get_note_for_xy(pk, x, y);

        (void) ignored;

	if (event->button != 1)
		return TRUE;

	if (event->type == GDK_BUTTON_PRESS) {
		/* This is possible when you make the window a little wider and then click
		   on the grey area. */
		if (note < 0) {
			return TRUE;
		}

		if (pk->note_being_pressed_using_mouse >= 0)
			release_key(pk, pk->note_being_pressed_using_mouse);

		press_key(pk, note);
		pk->note_being_pressed_using_mouse = note;

	} else if (event->type == GDK_BUTTON_RELEASE) {
		if (note >= 0) {
			release_key(pk, note);

		} else {
			if (pk->note_being_pressed_using_mouse >= 0)
				release_key(pk, pk->note_being_pressed_using_mouse);
		}

		pk->note_being_pressed_using_mouse = -1;

	}

	return TRUE;
}

static gboolean
mouse_motion_event_handler(PianoKeyboard *pk, GdkEventMotion *event, gpointer ignored)
{
	int		note;

        (void) ignored;

	if ((event->state & GDK_BUTTON1_MASK) == 0)
		return TRUE;

	note = get_note_for_xy(pk, event->x, event->y);

	if (note != pk->note_being_pressed_using_mouse && note >= 0) {

		if (pk->note_being_pressed_using_mouse >= 0)
			release_key(pk, pk->note_being_pressed_using_mouse);
		press_key(pk, note);
		pk->note_being_pressed_using_mouse = note;
	}

	return TRUE;
}

static gboolean
piano_keyboard_expose(GtkWidget *widget, GdkEventExpose *event)
{
	int i;
	PianoKeyboard *pk = PIANO_KEYBOARD(widget);
	cairo_t* cr = gdk_cairo_create (GDK_DRAWABLE (GTK_WIDGET(pk)->window));

	gdk_cairo_region (cr, event->region);
	cairo_clip (cr);

	for (i = 0; i < NNOTES; i++) {
		GdkRectangle r;

		r.x = pk->notes[i].x;
		r.y = 0;
		r.width = pk->notes[i].w;
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
piano_keyboard_size_request(GtkWidget* w, GtkRequisition *requisition)
{
        (void) w;

	requisition->width = PIANO_KEYBOARD_DEFAULT_WIDTH;
	requisition->height = PIANO_KEYBOARD_DEFAULT_HEIGHT;
}

static void
recompute_dimensions(PianoKeyboard *pk)
{
	int		number_of_white_keys = (NNOTES - 1) * (7.0 / 12.0);

	int		key_width;
	int		black_key_width;
	int		useful_width;

	int		note;
	int		white_key = 0;
	int		note_in_octave;

	int		width = GTK_WIDGET(pk)->allocation.width;
	int		height = GTK_WIDGET(pk)->allocation.height;

	key_width = width / number_of_white_keys;
	black_key_width = key_width * 0.8;
	useful_width = number_of_white_keys * key_width;
	pk->widget_margin = (width - useful_width) / 2;

	for (note = 0, white_key = 0; note < NNOTES - 2; note++) {
		note_in_octave = note % 12;

		if (note_in_octave == 1 || note_in_octave == 3 || note_in_octave == 6 ||
			note_in_octave == 8 || note_in_octave == 10) {

			/* This note is black key. */
			pk->notes[note].x = pk->widget_margin + white_key * key_width - black_key_width / 2;
			pk->notes[note].w = black_key_width;
			pk->notes[note].h = height / 2;
			pk->notes[note].white = 0;

			continue;
		}

		/* This note is white key. */
		pk->notes[note].x = pk->widget_margin + white_key * key_width;
		pk->notes[note].w = key_width;
		pk->notes[note].h = height;
		pk->notes[note].white = 1;

		white_key++;
	}
}

static void
piano_keyboard_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
	/* XXX: Are these two needed? */
	g_return_if_fail(widget != NULL);
	g_return_if_fail(allocation != NULL);

	widget->allocation = *allocation;

	recompute_dimensions(PIANO_KEYBOARD(widget));

	if (GTK_WIDGET_REALIZED(widget)) {
		gdk_window_move_resize (widget->window, allocation->x, allocation->y, allocation->width, allocation->height);
	}
}

static void
piano_keyboard_class_init(PianoKeyboardClass *klass)
{
	GtkWidgetClass	*widget_klass;

	/* Set up signals. */
	piano_keyboard_signals[NOTE_ON_SIGNAL] = g_signal_new ("note-on",
		G_TYPE_FROM_CLASS (klass), (GSignalFlags)(G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION),
		0, NULL, NULL, g_cclosure_marshal_VOID__INT, G_TYPE_NONE, 1, G_TYPE_INT);

	piano_keyboard_signals[NOTE_OFF_SIGNAL] = g_signal_new ("note-off",
		G_TYPE_FROM_CLASS (klass), (GSignalFlags)(G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION),
		0, NULL, NULL, g_cclosure_marshal_VOID__INT, G_TYPE_NONE, 1, G_TYPE_INT);

	piano_keyboard_signals[REST_SIGNAL] = g_signal_new ("rest",
                G_TYPE_FROM_CLASS (klass), (GSignalFlags)(G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION),
                0, NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

	widget_klass = (GtkWidgetClass*) klass;

	widget_klass->expose_event = piano_keyboard_expose;
	widget_klass->size_request = piano_keyboard_size_request;
	widget_klass->size_allocate = piano_keyboard_size_allocate;
}

static void
piano_keyboard_init(GtkWidget *mk)
{
	gtk_widget_add_events(mk, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);

	g_signal_connect(G_OBJECT(mk), "button-press-event", G_CALLBACK(mouse_button_event_handler), NULL);
	g_signal_connect(G_OBJECT(mk), "button-release-event", G_CALLBACK(mouse_button_event_handler), NULL);
	g_signal_connect(G_OBJECT(mk), "motion-notify-event", G_CALLBACK(mouse_motion_event_handler), NULL);
	g_signal_connect(G_OBJECT(mk), "key-press-event", G_CALLBACK(keyboard_event_handler), NULL);
	g_signal_connect(G_OBJECT(mk), "key-release-event", G_CALLBACK(keyboard_event_handler), NULL);
}

GType
piano_keyboard_get_type(void)
{
	static GType mk_type = 0;

	if (!mk_type) {
		static const GTypeInfo mk_info = {
			sizeof(PianoKeyboardClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) piano_keyboard_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (PianoKeyboard),
			0,    /* n_preallocs */
			(GInstanceInitFunc) piano_keyboard_init,
                        0,    /* value_table */
		};

		mk_type = g_type_register_static(GTK_TYPE_DRAWING_AREA, "PianoKeyboard", &mk_info, (GTypeFlags)0);
	}

	return mk_type;
}

GtkWidget *
piano_keyboard_new(void)
{
	GtkWidget *widget = (GtkWidget*)gtk_type_new(piano_keyboard_get_type());

	PianoKeyboard *pk = PIANO_KEYBOARD(widget);

	pk->maybe_stop_sustained_notes = 0;
	pk->sustain_new_notes = 0;
	pk->enable_keyboard_cue = 0;
	pk->octave = 4;
	pk->note_being_pressed_using_mouse = -1;
	memset((void *)pk->notes, 0, sizeof(struct Note) * NNOTES);
	pk->key_bindings = g_hash_table_new(g_str_hash, g_str_equal);
	bind_keys_qwerty(pk);

	return widget;
}

void
piano_keyboard_set_keyboard_cue(PianoKeyboard *pk, int enabled)
{
	pk->enable_keyboard_cue = enabled;
}

void
piano_keyboard_sustain_press(PianoKeyboard *pk)
{
	if (!pk->sustain_new_notes) {
		pk->sustain_new_notes = 1;
		pk->maybe_stop_sustained_notes = 1;
	}
}

void
piano_keyboard_sustain_release(PianoKeyboard *pk)
{
	if (pk->maybe_stop_sustained_notes)
		stop_sustained_notes(pk);

	pk->sustain_new_notes = 0;
}

void
piano_keyboard_set_note_on(PianoKeyboard *pk, int note)
{
	if (pk->notes[note].pressed == 0) {
		pk->notes[note].pressed = 1;
		queue_note_draw (pk, note);
	}
}

void
piano_keyboard_set_note_off(PianoKeyboard *pk, int note)
{
	if (pk->notes[note].pressed || pk->notes[note].sustained) {
		pk->notes[note].pressed = 0;
		pk->notes[note].sustained = 0;
		queue_note_draw (pk, note);
	}
}

void
piano_keyboard_set_octave(PianoKeyboard *pk, int octave)
{
	stop_unsustained_notes(pk);
	pk->octave = octave;
	gtk_widget_queue_draw(GTK_WIDGET(pk));
}

gboolean
piano_keyboard_set_keyboard_layout(PianoKeyboard *pk, const char *layout)
{
	assert(layout);

	if (!g_strcasecmp(layout, "QWERTY")) {
		bind_keys_qwerty(pk);

	} else if (!g_strcasecmp(layout, "QWERTZ")) {
		bind_keys_qwertz(pk);

	} else if (!g_strcasecmp(layout, "AZERTY")) {
		bind_keys_azerty(pk);

	} else {
		/* Unknown layout name. */
		return TRUE;
	}

	return FALSE;
}
