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

#ifndef __PIANO_KEYBOARD_H__
#define __PIANO_KEYBOARD_H__

#include <glib.h>
#include <gtk/gtkdrawingarea.h>

G_BEGIN_DECLS

#define TYPE_PIANO_KEYBOARD               (piano_keyboard_get_type ())
#define PIANO_KEYBOARD(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_PIANO_KEYBOARD, PianoKeyboard))
#define PIANO_KEYBOARD_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_PIANO_KEYBOARD, PianoKeyboardClass))
#define IS_PIANO_KEYBOARD(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_PIANO_KEYBOARD))
#define IS_PIANO_KEYBOARD_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_PIANO_KEYBOARD))
#define PIANO_KEYBOARD_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_PIANO_KEYBOARD, PianoKeyboardClass))

typedef struct _PianoKeyboard             PianoKeyboard;
typedef struct _PianoKeyboardClass        PianoKeyboardClass;

#define NNOTES (128)
#define PIANO_MIN_NOTE 21
#define PIANO_MAX_NOTE 108

#define OCTAVE_MIN (-1)
#define OCTAVE_MAX (7)

struct PKNote {
	int pressed;   /* 1 if key is in pressed down state. */
	int sustained; /* 1 if note is sustained. */
	int x;         /* Distance between the left edge of the key
	                * and the left edge of the widget, in pixels. */
	int w;         /* Width of the key, in pixels. */
	int h;         /* Height of the key, in pixels. */
	int white;     /* 1 if key is white; 0 otherwise. */
};

struct _PianoKeyboard
{
	GtkDrawingArea da;
	int            maybe_stop_sustained_notes;
	int            sustain_new_notes;
	gboolean       enable_keyboard_cue;
	gboolean       highlight_grand_piano_range;
	gboolean       print_note_label;
	int            octave;
	int            octave_range;
	int            widget_margin;
	int            note_being_pressed_using_mouse;
	int            min_note;
	int            max_note;
	int            last_key;
	gboolean       monophonic;
	struct PKNote  notes[NNOTES];
	/* Table used to translate from PC keyboard character to MIDI note number. */
	GHashTable*    key_bindings;
	int            min_velocity;
	int            max_velocity;
	int            key_velocity;
};

struct _PianoKeyboardClass
{
	GtkDrawingAreaClass parent_class;
};

GType piano_keyboard_get_type (void) G_GNUC_CONST;
GtkWidget* piano_keyboard_new (void);

void piano_keyboard_sustain_press (PianoKeyboard *pk);
void piano_keyboard_sustain_release (PianoKeyboard *pk);
void piano_keyboard_set_note_on (PianoKeyboard *pk, int note);
void piano_keyboard_set_note_off (PianoKeyboard *pk, int note);
void piano_keyboard_set_keyboard_cue (PianoKeyboard *pk, gboolean enabled);
void piano_keyboard_set_grand_piano_highlight (PianoKeyboard *pk, gboolean enabled);
void piano_keyboard_show_note_label (PianoKeyboard *pk, gboolean enabled);
void piano_keyboard_set_monophonic (PianoKeyboard *pk, gboolean monophonic);
void piano_keyboard_set_octave (PianoKeyboard *pk, int octave);
void piano_keyboard_set_octave_range(PianoKeyboard *pk, int octave_range);

void piano_keyboard_set_keyboard_layout (PianoKeyboard* pk, const char* layout);
void piano_keyboard_set_velocities (PianoKeyboard *pk, int min_vel, int max_vel, int key_vel);

G_END_DECLS

#endif /* __PIANO_KEYBOARD_H__ */
