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

#pragma once

#include <list>
#include <memory>
#include <vector>

#include <ytkmm/alignment.h>
#include <ytkmm/box.h>
#include <ytkmm/scrolledwindow.h>
#include <ytkmm/sizegroup.h>
#include <ytkmm/table.h>

#include "pbd/natsort.h"

#include "ardour/session_handle.h"
#include "ardour/circular_buffer.h"
#include "ardour/types.h"

#include "gtkmm2ext/bindings.h"
#include "gtkmm2ext/cairo_widget.h"

#include "widgets/ardour_button.h"
#include "widgets/ardour_spacer.h"
#include "widgets/frame.h"
#include "widgets/pane.h"
#include "widgets/tabbable.h"

#include "application_bar.h"
#include "input_port_monitor.h"
#include "rec_info_box.h"
#include "shuttle_control.h"

namespace ARDOUR {
	class SoloMuteRelease;
	class IOPlug;
}

class TrackRecordAxis;
class RecorderGroupTabs;

class RecorderUI : public ArdourWidgets::Tabbable, public ARDOUR::SessionHandlePtr, public PBD::ScopedConnectionList
{
public:
	RecorderUI ();
	~RecorderUI ();

	void set_session (ARDOUR::Session*);
	void cleanup ();

	XMLNode& get_state () const;
	int set_state (const XMLNode&, int /* version */);

	Gtk::Window* use_own_window (bool and_fill_it);

	void spill_port (std::string const&);
	void add_track (std::string const&);

	void focus_on_clock();

private:
	void load_bindings ();
	void register_actions ();
	void update_title ();
	void session_going_away ();
	void parameter_changed (std::string const&);
	void presentation_info_changed (PBD::PropertyChange const&);
	void gui_extents_changed ();
	void regions_changed (std::shared_ptr<ARDOUR::RegionList>, PBD::PropertyChange const&);

	void start_updating ();
	void stop_updating ();
	bool update_meters ();
	void add_or_remove_io (ARDOUR::DataType, std::vector<std::string>, bool);
	void io_plugins_changed ();
	void io_plugin_add (std::shared_ptr<ARDOUR::IOPlug>);
	void io_plugin_going_away (std::weak_ptr<ARDOUR::IOPlug>);
	void post_add_remove (bool);
	void update_io_widget_labels ();

	void initial_track_display ();
	void add_routes (ARDOUR::RouteList&);
	void remove_route (TrackRecordAxis*);
	void tra_name_edit (TrackRecordAxis*, bool);
	void update_rec_table_layout ();
	void update_spacer_width (Gtk::Allocation&, TrackRecordAxis*);

	void set_connections (std::string const&);
	void port_connected_or_disconnected (std::string, std::string);
	void port_pretty_name_changed (std::string);

	void meter_area_size_allocate (Gtk::Allocation&);
	void meter_area_size_request (GtkRequisition*);
	void meter_area_layout ();

	bool scroller_button_event (GdkEventButton*);

	void arm_all ();
	void arm_none ();

	void rec_undo ();
	void rec_redo ();

	void peak_reset ();

	void update_sensitivity ();
	void update_recordstate ();
	void update_monitorstate (std::string, bool);
	void new_track_for_port (ARDOUR::DataType, std::string const&);

	static int calc_columns (int child_width, int parent_width);

	Gtkmm2ext::Bindings*  bindings;
	Gtk::HBox            _toolbar;
	Gtk::Table           _button_table;
	ArdourWidgets::VPane _pane;
	Gtk::ScrolledWindow  _rec_scroller;
	Gtk::VBox            _rec_container;
	Gtk::HBox            _rec_groups;
	Gtk::VBox            _rec_area;
	Gtk::ScrolledWindow  _meter_scroller;
	Gtk::VBox            _meter_area;
	Gtk::Table           _meter_table;
	Gtk::EventBox        _scroller_base;

	ArdourWidgets::ArdourHSpacer _toolbar_sep;
	Gtk::Label                   _recs_label;
	ArdourWidgets::ArdourButton  _btn_rec_all;
	ArdourWidgets::ArdourButton  _btn_rec_none;
	ArdourWidgets::ArdourButton  _btn_rec_forget;
	ArdourWidgets::ArdourButton  _btn_peak_reset;
	ArdourWidgets::ArdourButton  _monitor_in_button;
	ArdourWidgets::ArdourButton  _monitor_disk_button;
	ArdourWidgets::ArdourButton  _btn_new_plist;
	ArdourWidgets::ArdourButton  _btn_new_plist_rec;
	ArdourWidgets::ArdourButton  _auto_input_button;
	DurationInfoBox              _duration_info_box;
	XrunInfoBox                  _xrun_info_box;
	RemainInfoBox                _remain_info_box;
	ApplicationBar               _application_bar;
	Glib::RefPtr<Gtk::SizeGroup> _toolbar_button_height;
	Glib::RefPtr<Gtk::SizeGroup> _toolbar_recarm_width;
	Glib::RefPtr<Gtk::SizeGroup> _toolbar_monitoring_width;

	int  _meter_box_width;
	int  _meter_area_cols;
	bool _vertical;

	std::set<std::string> _spill_port_names;

	class RecRuler : public CairoWidget , public ARDOUR::SessionHandlePtr
	{
		public:
			RecRuler ();

			void playhead_position_changed (ARDOUR::samplepos_t);
			void set_gui_extents (samplepos_t, samplepos_t);
			void set_right_edge (int);

		protected:
			void render (Cairo::RefPtr<Cairo::Context> const&, cairo_rectangle_t*);
			void on_size_request (Gtk::Requisition*);
			bool on_button_press_event (GdkEventButton*);

		private:
			Glib::RefPtr<Pango::Layout> _layout;
			int                         _time_width;
			int                         _time_height;
			int                         _width;
			ARDOUR::samplecnt_t         _left;
			ARDOUR::samplecnt_t         _right;
	};

	class InputPort : public Gtk::EventBox
	{
		public:
			InputPort (std::string const&, ARDOUR::DataType, RecorderUI*, bool vertical = false, bool ioplug = false);
			~InputPort ();

			void set_frame_label (std::string const&);
			void set_connections (ARDOUR::WeakRouteList);
			void setup_name ();
			bool spill (bool);
			bool spilled () const;
			void allow_monitoring (bool);
			void update_monitorstate (bool);
			void update_rec_stat ();

			ARDOUR::DataType data_type () const;
			bool ioplug () const { return _ioplug; }
			std::string const& name () const;

			void update (float, float); // FastMeter
			void update (float const*); // EventMeter
			void update (ARDOUR::CircularSampleBuffer&); // InputScope
			void update (ARDOUR::CircularEventBuffer&); // EventMonitor
			void clear ();

			bool operator< (InputPort const& o) const {
				if (_ioplug != o._ioplug) {
					return !_ioplug;
				}
				if (_dt == o._dt) {
					return PBD::naturally_less (_port_name.c_str (), o._port_name.c_str ());
				}
				return _dt < (uint32_t) o._dt;
			}

		private:
			void rename_port ();
			bool monitor_press (GdkEventButton*);
			bool monitor_release (GdkEventButton*);

			ARDOUR::DataType            _dt;
			InputPortMonitor            _monitor;
			Gtk::Alignment              _alignment;
			ArdourWidgets::Frame        _frame;
			ArdourWidgets::ArdourButton _spill_button;
			ArdourWidgets::ArdourButton _monitor_button;
			ArdourWidgets::ArdourButton _name_button;
			Gtk::Label                  _name_label;
			ArdourWidgets::ArdourButton _add_button;
			std::string                 _port_name;
			bool                        _ioplug;
			ARDOUR::WeakRouteList       _connected_routes;
			ARDOUR::SoloMuteRelease*    _solo_release;

			static bool                         _size_groups_initialized;
			static Glib::RefPtr<Gtk::SizeGroup> _name_size_group;
			static Glib::RefPtr<Gtk::SizeGroup> _ctrl_size_group;
			static Glib::RefPtr<Gtk::SizeGroup> _monitor_size_group;
	};

	struct InputPortPtrSort {
		bool operator() (std::shared_ptr<InputPort> const& a, std::shared_ptr<InputPort> const& b) const {
			return *a < *b;
		}
	};

	typedef std::map<std::string, std::shared_ptr<InputPort> >     InputPortMap;
	typedef std::set<std::shared_ptr<InputPort>, InputPortPtrSort> InputPortSet;
	typedef std::set<std::shared_ptr<ARDOUR::IOPlug>>              IOPlugSet;

	RecRuler                     _ruler;
	Gtk::EventBox                _space;
	Gtk::HBox                    _ruler_box;
	ArdourWidgets::ArdourHSpacer _ruler_sep;
	RecorderGroupTabs*           _rec_group_tabs;

	InputPortMap                _input_ports;
	std::list<TrackRecordAxis*> _recorders;
	std::list<TrackRecordAxis*> _visible_recorders;
	IOPlugSet                   _ioplugins;

	sigc::connection          _fast_screen_update_connection;
	sigc::connection          _ruler_width_update_connection;
	PBD::ScopedConnectionList _engine_connections;
	PBD::ScopedConnection     _monitor_connection;
	PBD::ScopedConnectionList _going_away_connections;

public:
	/* only for RecorderGroupTab */
	std::list<TrackRecordAxis*> visible_recorders () const;
};

