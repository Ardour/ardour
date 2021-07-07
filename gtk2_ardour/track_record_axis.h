/*
 * Copyright (C) 2020 Robin Gareus <robin@gareus.org>
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

#ifndef __gtkardour_track_record_axis_h_
#define __gtkardour_track_record_axis_h_

#include <cmath>
#include <vector>

#include <gtkmm/alignment.h>
#include <gtkmm/box.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/separator.h>
#include <gtkmm/sizegroup.h>

#include "pbd/stateful.h"

#include "ardour/ardour.h"
#include "ardour/types.h"

#include "widgets/ardour_button.h"
#include "widgets/ardour_spacer.h"
#include "widgets/frame.h"

#include "io_button.h"
#include "level_meter.h"
#include "route_ui.h"

namespace ARDOUR
{
	class Region;
	class Route;
	class RouteGroup;
	class Session;
	class Track;
}

class LevelMeterVBox;
class RouteGroupMenu;

class TrackRecordAxis : public Gtk::VBox, public AxisView, public RouteUI
{
public:
	TrackRecordAxis (ARDOUR::Session*, boost::shared_ptr<ARDOUR::Route>);
	~TrackRecordAxis ();

	/* AxisView */
	std::string name () const;
	Gdk::Color  color () const;

	boost::shared_ptr<ARDOUR::Stripable> stripable() const {
		return RouteUI::stripable();
	}

	void set_session (ARDOUR::Session* s);

	void fast_update ();
	bool start_rename ();
	void set_gui_extents (samplepos_t, samplepos_t);

	bool rec_extent (samplepos_t&, samplepos_t&) const;
	int  summary_xpos () const;
	int  summary_width () const;

	static PBD::Signal1<void, TrackRecordAxis*> CatchDeletion;
	static PBD::Signal2<void, TrackRecordAxis*, bool> EditNextName;

protected:
	void self_delete ();

	void on_size_allocate (Gtk::Allocation&);
	void on_size_request (Gtk::Requisition*);

	/* AxisView */
	std::string state_id () const;

	/* route UI */
	void set_button_names ();
	void route_rec_enable_changed ();
	void blink_rec_display (bool onoff);
	void route_active_changed ();
	void map_frozen ();

private:
	void on_theme_changed ();
	void parameter_changed (std::string const& p);

	void set_name_label ();

	void reset_peak_display ();
	void reset_route_peak_display (ARDOUR::Route*);
	void reset_group_peak_display (ARDOUR::RouteGroup*);
	bool namebox_button_press (GdkEventButton*);

	bool playlist_click (GdkEventButton*);
	bool route_ops_click (GdkEventButton*);
	void build_route_ops_menu ();

	/* name editing */
	void end_rename (bool);
	void entry_changed ();
	void entry_activated ();
	bool entry_focus_in (GdkEventFocus*);
	bool entry_focus_out (GdkEventFocus*);
	bool entry_key_press (GdkEventKey*);
	bool entry_key_release (GdkEventKey*);
	bool entry_button_press (GdkEventButton*);
	void entry_populate_popup (Gtk::Menu*);
	void disconnect_entry_signals ();

	/* RouteUI */
	void route_property_changed (const PBD::PropertyChange&);
	void route_color_changed ();
	void update_sensitivity ();

	bool _clear_meters;

	Gtk::Table _ctrls;
	Gtk::Menu* _route_ops_menu;

	bool          _renaming;
	Gtk::EventBox _namebox;
	Gtk::Entry    _nameentry;
	bool          _nameentry_ctx;

	LevelMeterVBox*              _level_meter;
	IOButton                     _input_button;
	ArdourWidgets::ArdourButton  _number_label;
	ArdourWidgets::ArdourButton  _playlist_button;
	ArdourWidgets::Frame         _name_frame;
	ArdourWidgets::ArdourVSpacer _vseparator;

	Glib::RefPtr<Gtk::SizeGroup> _ctrls_button_size_group;
	Glib::RefPtr<Gtk::SizeGroup> _monitor_ctrl_size_group;

	static bool                         _size_group_initialized;
	static Glib::RefPtr<Gtk::SizeGroup> _track_number_size_group;

	PBD::ScopedConnectionList   _route_connections;
	std::list<sigc::connection> _entry_connections;

	struct RecInfo {
		RecInfo (samplepos_t s, samplepos_t e)
			: capture_start (s)
			, capture_end (e)
		{}
		samplepos_t capture_start;
		samplepos_t capture_end;
	};

	class TrackSummary : public CairoWidget
	{
		public:
			TrackSummary (boost::shared_ptr<ARDOUR::Route>);
			~TrackSummary ();

			void playhead_position_changed (samplepos_t p);
			void set_gui_extents (samplepos_t, samplepos_t);
			bool rec_extent (samplepos_t&, samplepos_t&) const;

		protected:
			void render (Cairo::RefPtr<Cairo::Context> const&, cairo_rectangle_t*);
			void on_size_request (Gtk::Requisition*);
			void on_size_allocate (Gtk::Allocation&);
			bool on_button_press_event (GdkEventButton*);

		private:
			void render_region (boost::shared_ptr<ARDOUR::Region>, Cairo::RefPtr<Cairo::Context> const&, double);
			void playlist_changed ();
			void playlist_contents_changed ();
			void property_changed (PBD::PropertyChange const&);
			void maybe_setup_rec_box ();
			void update_rec_box ();

			double sample_to_xpos (samplepos_t p) const
			{
				return (p - _start) * _xscale;
			}

			boost::shared_ptr<ARDOUR::Track> _track;
			samplepos_t _start;
			samplepos_t _end;
			double      _xscale;
			double      _last_playhead;
			bool        _rec_updating;
			bool        _rec_active;

			std::vector<RecInfo>      _rec_rects;
			PBD::ScopedConnection     _playlist_connections;
			PBD::ScopedConnectionList _connections;
			sigc::connection          _screen_update_connection;
	};

	TrackSummary _track_summary;

};

#endif
