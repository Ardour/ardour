/*
    Copyright (C) 2008 Paul Davis
    Author: Audan Holland ??

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

#ifndef __ardour_piano_roll_header_h__
#define __ardour_piano_roll_header_h__

#include "ardour/types.h"

#include <gtkmm/drawingarea.h>

namespace ARDOUR {
	class MidiTrack;
}

class MidiTimeAxisView;
class MidiStreamView;
class PublicEditor;

class PianoRollHeader : public Gtk::DrawingArea {
public:
	PianoRollHeader(MidiStreamView&);

	bool on_expose_event (GdkEventExpose*);
	bool on_motion_notify_event (GdkEventMotion*);
	bool on_button_press_event (GdkEventButton*);
	bool on_button_release_event (GdkEventButton*);
	bool on_scroll_event (GdkEventScroll*);
	bool on_enter_notify_event (GdkEventCrossing*);
	bool on_leave_notify_event (GdkEventCrossing*);

	void on_size_request(Gtk::Requisition*);
	void on_size_allocate(Gtk::Allocation& a);

	void note_range_changed();

	struct Color {
		Color();
		Color(double _r, double _g, double _b);
		inline void set(const Color& c);

		double r;
		double g;
		double b;
	};

	sigc::signal<void,uint8_t> SetNoteSelection;
	sigc::signal<void,uint8_t> AddNoteSelection;
	sigc::signal<void,uint8_t> ToggleNoteSelection;
	sigc::signal<void,uint8_t> ExtendNoteSelection;

private:
	static Color white;
	static Color white_highlight;
	static Color white_shade_light;
	static Color white_shade_dark;
	static Color black;
	static Color black_highlight;
	static Color black_shade_light;
	static Color black_shade_dark;

	PianoRollHeader(const PianoRollHeader&);

	enum ItemType {
		BLACK_SEPARATOR,
		BLACK_MIDDLE_SEPARATOR,
		BLACK,
		WHITE_SEPARATOR,
		WHITE_RECT,
		WHITE_CF,
		WHITE_EB,
		WHITE_DGA
	};

	void invalidate_note_range(int lowest, int highest);

	void get_path(ItemType, int note, double x[], double y[]);

	void send_note_on(uint8_t note);
	void send_note_off(uint8_t note);

	void reset_clicked_note(uint8_t, bool invalidate = true);

	MidiStreamView& _view;

	uint8_t _event[3];

	Cairo::RefPtr<Cairo::Context> cc;
	bool _active_notes[128];
	uint8_t _highlighted_note;
	uint8_t _clicked_note;
	double _grab_y;
	bool _dragging;

	double _note_height;
	double _black_note_width;

	PublicEditor& editor() const;
};

#endif /* __ardour_piano_roll_header_h__ */
