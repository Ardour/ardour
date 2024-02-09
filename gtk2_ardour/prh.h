/*
 * Copyright (C) 2008-2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2010-2012 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __ardour_canvas_piano_roll_header_h__
#define __ardour_canvas_piano_roll_header_h__

#include <pangomm/layout.h>
#include <glibmm/refptr.h>

#include "ardour/types.h"

#include "canvas/rectangle.h"

namespace ARDOUR {
	class MidiTrack;
}

class MidiView;
class MidiViewBackground;
class EditingContext;

namespace ArdourCanvas {

class PianoRollHeader : public ArdourCanvas::Rectangle {
public:
	PianoRollHeader (ArdourCanvas::Item* parent, MidiView&);

	void size_request (double& w, double& h) const;

	void render (ArdourCanvas::Rect const & area, Cairo::RefPtr<Cairo::Context>) const;
	void size_allocate (ArdourCanvas::Rect const &);

	void instrument_info_change ();

	void note_range_changed();
	void set_note_highlight (uint8_t note);

	sigc::signal<void,uint8_t> SetNoteSelection;
	sigc::signal<void,uint8_t> AddNoteSelection;
	sigc::signal<void,uint8_t> ToggleNoteSelection;
	sigc::signal<void,uint8_t> ExtendNoteSelection;

private:

	bool event_handler (GdkEvent*);
	bool motion_handler (GdkEventMotion*);
	bool button_press_handler (GdkEventButton*);
	bool button_release_handler (GdkEventButton*);
	bool scroll_handler (GdkEventScroll*);
	bool enter_handler (GdkEventCrossing*);
	bool leave_handler (GdkEventCrossing*);

	// void on_size_request(Gtk::Requisition*);

	struct NoteName {
		std::string name;
		bool from_midnam;
	};
	NoteName note_names[128];
	bool have_note_names;
	void set_min_page_size(double page_size);
	void render_scroomer(Cairo::RefPtr<Cairo::Context>) const;
	NoteName get_note_name (int note);

	Gtk::Adjustment& _adj;

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

	void get_path(int note, double x[], double y[]) const;

	void send_note_on(uint8_t note);
	void send_note_off(uint8_t note);

	void reset_clicked_note(uint8_t, bool invalidate = true);

	MidiView& _view;

	uint8_t _event[3];

	mutable Glib::RefPtr<Pango::Layout> _layout;
	mutable Glib::RefPtr<Pango::Layout> _big_c_layout;
	mutable Glib::RefPtr<Pango::Layout> _midnam_layout;
	mutable Pango::FontDescription _font_descript;
	Pango::FontDescription _font_descript_big_c;
	mutable Pango::FontDescription _font_descript_midnam;
	bool _active_notes[128];
	uint8_t _highlighted_note;
	uint8_t _clicked_note;
	double _grab_y;
	bool _dragging;
	double _scroomer_size;
	bool _scroomer_drag;
	double _old_y;
	double _fract;
	double _fract_top;
	double _min_page_size;
	enum scr_pos {TOP, BOTTOM, MOVE, NONE};
	scr_pos _scroomer_state;
	scr_pos _scroomer_button_state;
	double _saved_top_val;
	double _saved_bottom_val;
	mutable bool _mini_map_display;
	bool entered;

	bool show_scroomer () const;

	ArdourCanvas::Rect _alloc;
};

}

#endif /* __ardour_piano_roll_header_h__ */
