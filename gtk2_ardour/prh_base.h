/*
 * Copyright (C) 2008-2025 David Robillard <d@drobilla.net>
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

#pragma once

#include <pangomm/layout.h>
#include <glibmm/refptr.h>

#include "ardour/types.h"

#include "canvas/rectangle.h"

#include <ytkmm/adjustment.h>

namespace Gdk {
	class Window;
}

namespace ARDOUR {
	class MidiTrack;
}

class MidiView;
class MidiViewBackground;
class EditingContext;

class PianoRollHeaderBase : virtual public sigc::trackable {
  public:
	PianoRollHeaderBase (MidiViewBackground&);
	virtual ~PianoRollHeaderBase() {}

	void render (ArdourCanvas::Rect const & self, ArdourCanvas::Rect const & area, Cairo::RefPtr<Cairo::Context>) const;

	virtual void instrument_info_change ();

	void note_range_changed();
	void set_note_highlight (uint8_t note);

	sigc::signal<void,uint8_t> SetNoteSelection;
	sigc::signal<void,uint8_t> AddNoteSelection;
	sigc::signal<void,uint8_t> ToggleNoteSelection;
	sigc::signal<void,uint8_t> ExtendNoteSelection;

	void set_view (MidiView*);

	virtual void redraw () = 0;
	virtual void redraw (double x, double y, double w, double h) = 0;
	virtual double height() const = 0;
	virtual double width() const = 0;
	virtual double event_y_to_y (double evy) const = 0;
	virtual void draw_transform (double& x, double& y) const = 0;
	virtual void event_transform (double& x, double& y) const = 0;
	virtual void _queue_resize () = 0;
	virtual void do_grab() = 0;
	virtual void do_ungrab() = 0;
	virtual Glib::RefPtr<Gdk::Window> cursor_window() = 0;
	virtual std::shared_ptr<ARDOUR::MidiTrack> midi_track() = 0;

  protected:
	MidiViewBackground& _midi_context;
	Gtk::Adjustment&    _adj;
	MidiView*           _view;

	uint8_t             _event[3];

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
	mutable double _scroomer_size;
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

	// void on_size_request(Gtk::Requisition*);

	struct NoteName {
		std::string name;
		bool from_midnam;
	};
	NoteName note_names[128];
	bool have_note_names;

	void set_min_page_size (double page_size);
	void render_scroomer (Cairo::RefPtr<Cairo::Context>) const;
	NoteName get_note_name (int note);

	bool event_handler (GdkEvent*);
	bool motion_handler (GdkEventMotion*);
	bool button_press_handler (GdkEventButton*);
	bool button_release_handler (GdkEventButton*);
	bool scroll_handler (GdkEventScroll*);
	bool enter_handler (GdkEventCrossing*);
	bool leave_handler (GdkEventCrossing*);

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

	void invalidate_note_range (int lowest, int highest);
	void send_note_on (uint8_t note);
	void send_note_off (uint8_t note);
	void reset_clicked_note (uint8_t, bool invalidate = true);
	bool show_scroomer () const;
	void alloc_layouts (Glib::RefPtr<Pango::Context>);
	void set_cursor (Gdk::Cursor*);

	void begin_scroomer_drag (double event_y);
	void end_scroomer_drag ();
	bool idle_apply_range ();
	double idle_lower;
	double idle_upper;
	sigc::connection scroomer_drag_connection;
};
