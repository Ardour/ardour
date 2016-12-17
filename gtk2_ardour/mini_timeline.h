/*
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __gtkardour_mini_timeline_h__
#define __gtkardour_mini_timeline_h__

#include <list>
#include <pangomm.h>

#include "pbd/signals.h"

#include "ardour/ardour.h"
#include "ardour/session_handle.h"

#include "gtkmm2ext/cairo_widget.h"
#include "gtkmm2ext/utils.h"

#include "audio_clock.h"

namespace ARDOUR {
	class Session;
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
	void on_style_changed (const Glib::RefPtr<Gtk::Style>&);
	void on_name_changed ();
	void set_colors ();

	void calculate_time_width ();
	void update_minitimeline ();
	void draw_dots (cairo_t*, int left, int right, int y);
	int  draw_mark (cairo_t*, int x0, int x1, int h, const std::string&);

	void render (cairo_t*, cairo_rectangle_t*);
	void format_time (framepos_t when);

	bool on_button_release_event (GdkEventButton *ev);
	bool on_scroll_event (GdkEventScroll*);

	Glib::RefPtr<Pango::Layout> _layout;
	sigc::connection super_rapid_connection;
	PBD::ScopedConnectionList marker_connection;

	framepos_t _last_update_frame;
	AudioClock::Mode _clock_mode;
	int _time_width;
	int _time_height;

	struct JumpRange {
		JumpRange (int l, int r, framepos_t t)
			: left (l), right (r), to (t) {}
		int left;
		int right;
		framepos_t to;
	};

	typedef std::list <JumpRange> JumpList;
	JumpList _jumplist;
};

#endif
