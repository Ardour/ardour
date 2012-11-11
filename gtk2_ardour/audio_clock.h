/*
    Copyright (C) 1999 Paul Davis

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

#ifndef __audio_clock_h__
#define __audio_clock_h__

#include <map>
#include <vector>

#include <pangomm.h>

#include <gtkmm/alignment.h>
#include <gtkmm/box.h>
#include <gtkmm/menu.h>
#include <gtkmm/label.h>

#include "ardour/ardour.h"
#include "ardour/session_handle.h"

#include "gtkmm2ext/cairo_widget.h"

namespace ARDOUR {
	class Session;
}

class AudioClock : public CairoWidget, public ARDOUR::SessionHandlePtr
{
  public:
	enum Mode {
		Timecode,
		BBT,
		MinSec,
		Frames
	};

	AudioClock (const std::string& clock_name, bool is_transient, const std::string& widget_name,
                    bool editable, bool follows_playhead, bool duration = false, bool with_info = false);
	~AudioClock ();

	Mode mode() const { return _mode; }
	void set_off (bool yn);
	bool off() const { return _off; }
	void set_widget_name (const std::string& name);
	void set_active_state (Gtkmm2ext::ActiveState s);
	void set_editable (bool yn);
	void set_corner_radius (double);
	void set_fixed_width (bool);

	void focus ();

	void set (framepos_t, bool force = false, ARDOUR::framecnt_t offset = 0);
	void set_from_playhead ();
	void locate ();
	void set_mode (Mode);
	void set_bbt_reference (framepos_t);
        void set_is_duration (bool);

	std::string name() const { return _name; }

	framepos_t current_time (framepos_t position = 0) const;
	framepos_t current_duration (framepos_t position = 0) const;
	void set_session (ARDOUR::Session *s);

	sigc::signal<void> ValueChanged;
	sigc::signal<void> mode_changed;
	sigc::signal<void> ChangeAborted;

	static sigc::signal<void> ModeChanged;
	static std::vector<AudioClock*> clocks;

  protected:
	void render (cairo_t*);

	virtual void build_ops_menu ();
	Gtk::Menu  *ops_menu;

  private:
	Mode             _mode;
	std::string      _name;
	bool              is_transient;
	bool              is_duration;
	bool              editable;
	/** true if this clock follows the playhead, meaning that certain operations are redundant */
	bool             _follows_playhead;
	bool             _off;
	bool             _fixed_width;
	int              layout_x_offset;
	int              em_width;
	bool             _edit_by_click_field;

	Glib::RefPtr<Pango::Layout> _layout;
	Glib::RefPtr<Pango::Layout> _left_layout;
	Glib::RefPtr<Pango::Layout> _right_layout;

	Pango::AttrColor*    editing_attr;
	Pango::AttrColor*    foreground_attr;

	Pango::AttrList normal_attributes;
	Pango::AttrList editing_attributes;
	Pango::AttrList info_attributes;

	int first_height;
	int first_width;
	int layout_height;
	int layout_width;
	int info_height;
	int upper_height;
	double mode_based_info_ratio;
	double corner_radius;
	uint32_t font_size;

	static const double info_font_scale_factor;
	static const double separator_height;
	static const double x_leading_padding;

	enum Field {
		Timecode_Hours = 1,
		Timecode_Minutes,
		Timecode_Seconds,
		Timecode_Frames,
		MS_Hours,
		MS_Minutes,
		MS_Seconds,
		MS_Milliseconds,
		Bars,
		Beats,
		Ticks,
		AudioFrames,
	};

	Field index_to_field (int index) const;

	/* this maps the number of input characters/digits when editing
	   to a cursor position. insert_map[N] = index of character/digit
	   where the cursor should be after N chars/digits. it is 
	   mode specific and so it is filled during set_mode().
	*/

	std::vector<int> insert_map;

	bool editing;
	std::string edit_string;
	std::string pre_edit_string;
	std::string input_string;

	framepos_t bbt_reference_time;
	framepos_t last_when;
	bool last_pdelta;
	bool last_sdelta;

	bool dragging;
	double drag_start_y;
	double drag_y;
	double drag_accum;
	Field  drag_field;

	void on_realize ();
	bool on_key_press_event (GdkEventKey *);
	bool on_key_release_event (GdkEventKey *);
	bool on_scroll_event (GdkEventScroll *ev);
	bool on_button_press_event (GdkEventButton *ev);
	bool on_button_release_event(GdkEventButton *ev);
	void on_style_changed (const Glib::RefPtr<Gtk::Style>&);
	void on_size_request (Gtk::Requisition* req);
	bool on_motion_notify_event (GdkEventMotion *ev);
	void on_size_allocate (Gtk::Allocation&);
	bool on_focus_out_event (GdkEventFocus*);

	void set_timecode (framepos_t, bool);
	void set_bbt (framepos_t, bool);
	void set_minsec (framepos_t, bool);
	void set_frames (framepos_t, bool);

	framepos_t get_frame_step (Field, framepos_t pos = 0, int dir = 1);

	bool timecode_validate_edit (const std::string&);
	bool bbt_validate_edit (const std::string&);
	bool minsec_validate_edit (const std::string&);

	framepos_t frames_from_timecode_string (const std::string&) const;
	framepos_t frames_from_bbt_string (framepos_t, const std::string&) const;
	framepos_t frame_duration_from_bbt_string (framepos_t, const std::string&) const;
	framepos_t frames_from_minsec_string (const std::string&) const;
	framepos_t frames_from_audioframes_string (const std::string&) const;

	void session_configuration_changed (std::string);

	Field index_to_field () const;

	void start_edit (Field f = Field (0));
	void end_edit (bool);
	void end_edit_relative (bool);
	void edit_next_field ();
	ARDOUR::framecnt_t parse_as_distance (const std::string&);

	ARDOUR::framecnt_t parse_as_timecode_distance (const std::string&);
	ARDOUR::framecnt_t parse_as_minsec_distance (const std::string&);
	ARDOUR::framecnt_t parse_as_bbt_distance (const std::string&);
	ARDOUR::framecnt_t parse_as_frames_distance (const std::string&);
	
	void set_font ();
	void set_colors ();
	void show_edit_status (int length);
	int  merge_input_and_edit_string ();
	std::string get_field (Field);

	void drop_focus ();
	void dpi_reset ();

	double bg_r, bg_g, bg_b, bg_a;
	double cursor_r, cursor_g, cursor_b, cursor_a;
};

#endif /* __audio_clock_h__ */
