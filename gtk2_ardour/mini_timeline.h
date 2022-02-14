/*
 * Copyright (C) 2016-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2016-2018 Ben Loftis <ben@harrisonconsoles.com>
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

#ifndef __gtkardour_mini_timeline_h__
#define __gtkardour_mini_timeline_h__

#include <list>
#include <pangomm.h>

#include "pbd/signals.h"

#include "ardour/ardour.h"
#include "ardour/types.h"
#include "ardour/session_handle.h"

#include "gtkmm2ext/cairo_widget.h"
#include "gtkmm2ext/utils.h"

#include "audio_clock.h"

namespace ARDOUR {
	class Session;
}

namespace Gtk {
	class Menu;
}

class MiniTimeline : public CairoWidget, public ARDOUR::SessionHandlePtr, public PBD::ScopedConnectionList
{

public:
	MiniTimeline ();
	~MiniTimeline ();

	void set_session (ARDOUR::Session *);

private:
	void session_going_away ();
	void super_rapid_update ();

	void on_size_request (Gtk::Requisition*);
	void on_size_allocate (Gtk::Allocation&);
	void dpi_changed ();
	void set_colors ();
	void parameter_changed (std::string const &);

	void calculate_time_width ();
	void calculate_time_spacing ();
	void update_minitimeline ();
	void draw_dots (cairo_t*, int left, int right, int y, Gtkmm2ext::Color);
	int  draw_mark (cairo_t*, int x0, int x1, const std::string&, bool& prelight);
	int  draw_cue (cairo_t*, int x0, int next_cue_left_edge, int tl_width, int cue_index, bool& prelight);
	int  draw_edge (cairo_t*, int x0, int x1, bool left, const std::string&, bool& prelight);

	void render (Cairo::RefPtr<Cairo::Context> const&, cairo_rectangle_t*);
	void format_time (samplepos_t when);

	bool on_button_press_event (GdkEventButton*);
	bool on_button_release_event (GdkEventButton*);
	bool on_scroll_event (GdkEventScroll*);
	bool on_motion_notify_event (GdkEventMotion*);
	bool on_leave_notify_event (GdkEventCrossing*);

	void build_minitl_context_menu ();
	void set_span (ARDOUR::samplecnt_t);

	Glib::RefPtr<Pango::Layout> _layout;
	sigc::connection super_rapid_connection;
	PBD::ScopedConnectionList marker_connection;
	PBD::ScopedConnectionList tempo_map_connection;
	PBD::ScopedConnectionList session_connection;

	samplepos_t _last_update_sample;
	AudioClock::Mode _clock_mode;
	int _time_width;
	int _time_height;

	int _n_labels;
	double _px_per_sample;
	ARDOUR::samplecnt_t _time_granularity;
	ARDOUR::samplecnt_t _time_span_samples;
	int _marker_height;

	int _pointer_x;
	int _pointer_y;

	Gtk::Menu* _minitl_context_menu;

	uint32_t _phead_color;

	struct JumpRange {
		JumpRange (int l, int r, samplepos_t t, bool p = false)
			: left (l), right (r), to (t), prelight (p) {}
		int left;
		int right;
		samplepos_t to;
		bool prelight;
	};

	typedef std::list <JumpRange> JumpList;
	JumpList _jumplist;
};

#endif
