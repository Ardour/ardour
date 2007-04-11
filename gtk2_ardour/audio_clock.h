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

#include <gtkmm/box.h>
#include <gtkmm/menu.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/label.h>
#include <gtkmm/frame.h>
#include <ardour/ardour.h>

namespace ARDOUR {
	class Session;
}

class AudioClock : public Gtk::HBox
{
  public:
	enum Mode {
		SMPTE,
		BBT,
		MinSec,
		Frames,
		Off
	};
	
	AudioClock (std::string clock_name, bool transient, std::string widget_name, bool editable, bool is_duration = false, bool with_info = false);

	Mode mode() const { return _mode; }
	
	void set (nframes_t, bool force = false);
	void set_mode (Mode);
	
	void set_widget_name (std::string);

	std::string name() const { return _name; }

	nframes_t current_time (nframes_t position = 0) const;
	nframes_t current_duration (nframes_t position = 0) const;
	void set_session (ARDOUR::Session *s);

	sigc::signal<void> ValueChanged;

	static sigc::signal<void> ModeChanged;
	static std::vector<AudioClock*> clocks;

  private:
	ARDOUR::Session  *session;
	Mode             _mode;
	uint32_t          key_entry_state;
	std::string      _name;
	bool              is_transient;
	bool              is_duration;
	bool              editable;

	Gtk::Menu  *ops_menu;

	Gtk::HBox   smpte_packer_hbox;
	Gtk::HBox   smpte_packer;

	Gtk::HBox   minsec_packer_hbox;
	Gtk::HBox   minsec_packer;

	Gtk::HBox   bbt_packer_hbox;
	Gtk::HBox   bbt_packer;

	Gtk::HBox   frames_packer_hbox;
	Gtk::HBox   frames_packer;
       
	enum Field {
		SMPTE_Hours,
		SMPTE_Minutes,
		SMPTE_Seconds,
		SMPTE_Frames,
		MS_Hours,
		MS_Minutes,
		MS_Seconds,
		Bars,
		Beats, 
		Ticks,
		AudioFrames
	};

	Gtk::EventBox  audio_frames_ebox;
	Gtk::Label     audio_frames_label;

	Gtk::EventBox  hours_ebox;
	Gtk::EventBox  minutes_ebox;
	Gtk::EventBox  seconds_ebox;
	Gtk::EventBox  frames_ebox;

	Gtk::EventBox  ms_hours_ebox;
	Gtk::EventBox  ms_minutes_ebox;
	Gtk::EventBox  ms_seconds_ebox;

	Gtk::EventBox  bars_ebox;
	Gtk::EventBox  beats_ebox;
	Gtk::EventBox  ticks_ebox;

	Gtk::Label  hours_label;
	Gtk::Label  minutes_label;
	Gtk::Label  seconds_label;
	Gtk::Label  frames_label;
	Gtk::Label  colon1, colon2, colon3;

	Gtk::Label  ms_hours_label;
	Gtk::Label  ms_minutes_label;
	Gtk::Label  ms_seconds_label;
	Gtk::Label  colon4, colon5;

	Gtk::Label  bars_label;
	Gtk::Label  beats_label;
	Gtk::Label  ticks_label;
	Gtk::Label  b1;
	Gtk::Label  b2;

	Gtk::Label*  frames_upper_info_label;
	Gtk::Label*  frames_lower_info_label;

	Gtk::Label*  smpte_upper_info_label;
	Gtk::Label*  smpte_lower_info_label;
	
	Gtk::Label*  bbt_upper_info_label;
	Gtk::Label*  bbt_lower_info_label;

	Gtk::VBox   frames_info_box;
	Gtk::VBox   smpte_info_box;
	Gtk::VBox   bbt_info_box;

	Gtk::EventBox  clock_base;
	Gtk::Frame     clock_frame;

	nframes_t last_when;

	uint32_t last_hrs;
	uint32_t last_mins;
	uint32_t last_secs;
	uint32_t last_frames;
	bool last_negative;

	long  ms_last_hrs;
	long  ms_last_mins;
	float ms_last_secs;

	bool dragging;
	double drag_start_y;
	double drag_y;
	double drag_accum;

	void on_realize ();
	
	bool field_motion_notify_event (GdkEventMotion *ev, Field);
	bool field_button_press_event (GdkEventButton *ev, Field);
	bool field_button_release_event (GdkEventButton *ev, Field);
	bool field_button_scroll_event (GdkEventScroll *ev, Field);
	bool field_key_release_event (GdkEventKey *, Field);
	bool field_focus_in_event (GdkEventFocus *, Field);
	bool field_focus_out_event (GdkEventFocus *, Field);

	void set_smpte (nframes_t, bool);
	void set_bbt (nframes_t, bool);
	void set_minsec (nframes_t, bool);
	void set_frames (nframes_t, bool);

	nframes_t get_frames (Field,nframes_t pos = 0,int dir=1);
	
	void smpte_sanitize_display();
	nframes_t smpte_frame_from_display () const;
	nframes_t bbt_frame_from_display (nframes_t) const;
	nframes_t bbt_frame_duration_from_display (nframes_t) const;
	nframes_t minsec_frame_from_display () const;
	nframes_t audio_frame_from_display () const;

	void build_ops_menu ();
	void setup_events ();

	void smpte_offset_changed ();
	void set_size_requests ();

	static const uint32_t field_length[(int)AudioFrames+1];
};

#endif /* __audio_clock_h__ */
