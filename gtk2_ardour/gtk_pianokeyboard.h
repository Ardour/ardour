#ifndef __PIANO_KEYBOARD_H__
#define __PIANO_KEYBOARD_H__

#include <glib.h>
#include <gtk/gtkdrawingarea.h>

G_BEGIN_DECLS

#define TYPE_PIANO_KEYBOARD			(piano_keyboard_get_type ())
#define PIANO_KEYBOARD(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_PIANO_KEYBOARD, PianoKeyboard))
#define PIANO_KEYBOARD_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_PIANO_KEYBOARD, PianoKeyboardClass))
#define IS_PIANO_KEYBOARD(obj)			(G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_PIANO_KEYBOARD))
#define IS_PIANO_KEYBOARD_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_PIANO_KEYBOARD))
#define PIANO_KEYBOARD_GET_CLASS(obj)		(G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_PIANO_KEYBOARD, PianoKeyboardClass))

typedef struct	_PianoKeyboard			PianoKeyboard;
typedef struct	_PianoKeyboardClass		PianoKeyboardClass;

#define NNOTES			127

#define OCTAVE_MIN	-1
#define OCTAVE_MAX	7

struct Note {
	int			pressed;		/* 1 if key is in pressed down state. */
	int			sustained;		/* 1 if note is sustained. */
	int			x;			/* Distance between the left edge of the key
							 * and the left edge of the widget, in pixels. */
	int			w;			/* Width of the key, in pixels. */
	int			h;			/* Height of the key, in pixels. */
	int			white;			/* 1 if key is white; 0 otherwise. */
};

struct _PianoKeyboard
{
	GtkDrawingArea		da;
	int			maybe_stop_sustained_notes;
	int			sustain_new_notes;
	int			enable_keyboard_cue;
	int			octave;
	int			widget_margin;
	int			note_being_pressed_using_mouse;
	volatile struct Note 	notes[NNOTES];
	/* Table used to translate from PC keyboard character to MIDI note number. */
	GHashTable		*key_bindings;
};

struct _PianoKeyboardClass
{
	GtkDrawingAreaClass	parent_class;
};

GType		piano_keyboard_get_type		(void) G_GNUC_CONST;
GtkWidget*	piano_keyboard_new		(void);
void		piano_keyboard_sustain_press	(PianoKeyboard *pk);
void		piano_keyboard_sustain_release	(PianoKeyboard *pk);
void		piano_keyboard_set_note_on	(PianoKeyboard *pk, int note);
void		piano_keyboard_set_note_off	(PianoKeyboard *pk, int note);
void		piano_keyboard_set_keyboard_cue	(PianoKeyboard *pk, int enabled);
void		piano_keyboard_set_octave (PianoKeyboard *pk, int octave);
gboolean	piano_keyboard_set_keyboard_layout (PianoKeyboard *pk, const char *layout);

G_END_DECLS

#endif /* __PIANO_KEYBOARD_H__ */

