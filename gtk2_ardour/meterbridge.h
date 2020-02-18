/*
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2016 Paul Davis <paul@linuxaudiosystems.com>
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
#ifndef __ardour_meterbridge_h__
#define __ardour_meterbridge_h__

#include <glibmm/thread.h>

#include <gtkmm/box.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/label.h>
#include <gtkmm/window.h>

#include "ardour/ardour.h"
#include "ardour/types.h"
#include "ardour/session_handle.h"

#include "pbd/stateful.h"
#include "pbd/signals.h"

#include "gtkmm2ext/visibility_tracker.h"

#include "meter_strip.h"

class Meterbridge :
	public Gtk::Window,
	public PBD::ScopedConnectionList,
	public ARDOUR::SessionHandlePtr,
	public Gtkmm2ext::VisibilityTracker
{
public:
	static Meterbridge* instance();
	~Meterbridge();

	void set_session (ARDOUR::Session *);

	XMLNode& get_state (void);
	int set_state (const XMLNode& );

	void show_window ();
	bool hide_window (GdkEventAny *ev);

private:
	Meterbridge ();
	static Meterbridge* _instance;

	bool _visible;
	bool _show_busses;
	bool _show_master;
	bool _show_midi;

	Gtk::ScrolledWindow scroller;
	Gtk::HBox meterarea;
	Gtk::HBox global_hpacker;
	Gtk::VBox global_vpacker;

	gint start_updating ();
	gint stop_updating ();

	sigc::connection fast_screen_update_connection;
	void fast_update_strips ();

	void add_strips (ARDOUR::RouteList&);
	void remove_strip (MeterStrip *);

	void session_going_away ();
	void sync_order_keys ();
	void resync_order (PBD::PropertyChange what_changed = ARDOUR::Properties::order);
	mutable Glib::Threads::Mutex _resync_mutex;

	struct MeterBridgeStrip {
		MeterStrip *s;
		bool visible;

		MeterBridgeStrip(MeterStrip *ss) {
			s = ss;
			visible = true;
		}
	};

	struct MeterOrderRouteSorter
	{
		bool operator() (struct MeterBridgeStrip ma, struct MeterBridgeStrip mb) {
			boost::shared_ptr<ARDOUR::Route> a = ma.s->route();
			boost::shared_ptr<ARDOUR::Route> b = mb.s->route();
			if (a->is_master() || a->is_monitor()) {
				/* "a" is a special route (master, monitor, etc), and comes
				 * last in the mixer ordering
				 */
				return false;
			} else if (b->is_master() || b->is_monitor()) {
				/* everything comes before b */
				return true;
			}
			return ARDOUR::Stripable::Sorter (true) (a, b);
		}
	};

	std::list<MeterBridgeStrip> strips;

	MeterStrip metrics_left;
	MeterStrip metrics_right;
	std::vector<MeterStrip *> _metrics;

	Gtk::VBox metrics_vpacker_left;
	Gtk::VBox metrics_vpacker_right;
	Gtk::HBox metrics_spacer_left;
	Gtk::HBox metrics_spacer_right;

	static const int32_t default_width = 600;
	static const int32_t default_height = 400;
	static const int max_height = 1200; // == 1024 + 148 + 16 + 12 see meter_strip.cc
	int cur_max_width;

	void update_title ();

	// for restoring window geometry.
	int m_root_x, m_root_y, m_width, m_height;

	void set_window_pos_and_size ();
	void get_window_pos_and_size ();

	bool on_key_press_event (GdkEventKey*);
	bool on_key_release_event (GdkEventKey*);
	bool on_scroll_event (GdkEventScroll*);

	void scroll_left ();
	void scroll_right ();

	void on_size_allocate (Gtk::Allocation&);
	void on_size_request (Gtk::Requisition*);

	void parameter_changed (std::string const & p);
	void on_theme_changed ();

	void on_scroll ();
	sigc::connection scroll_connection;

	int _mm_left, _mm_right;
	ARDOUR::MeterType _mt_left, _mt_right;
};

#endif
