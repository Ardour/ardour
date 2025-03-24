/*
 * Copyright (C) 2005-2024 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2013-2024 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2014-2024 Ben Loftis <ben@harrisonconsoles.com>
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

#include <ytkmm/box.h>
#include <ytkmm/label.h>
#include <ytkmm/table.h>

#include "ardour/ardour.h"
#include "ardour/types.h"
#include "ardour/session_handle.h"

#include "widgets/ardour_button.h"
#include "widgets/ardour_dropdown.h"
#include "widgets/ardour_spacer.h"

#include "main_clock.h"
#include "mini_timeline.h"
#include "shuttle_control.h"
#include "startup_fsm.h"
#include "transport_control.h"
#include "transport_control_ui.h"
#include "main_clock.h"
#include "visibility_group.h"
#include "window_manager.h"

class BasicUI;
class TimeInfoBox;
class LevelMeterHBox;

namespace ARDOUR {
	class Route;
	class RouteGroup;
}

#define MAX_LUA_ACTION_BUTTONS 12

class ApplicationBar : public Gtk::HBox, public ARDOUR::SessionHandlePtr
{
public:
	ApplicationBar ();
	~ApplicationBar();

	void set_session (ARDOUR::Session *);

	void focus_on_clock ();

private:
	void on_parent_changed (Gtk::Widget*);

	bool sync_button_clicked (GdkEventButton*);

	void parameter_changed (std::string);
	void ui_actions_ready ();

	void repack_transport_hbox ();

	void map_transport_state ();
	void set_transport_sensitivity (bool);

	void set_record_mode (ARDOUR::RecordMode);

	void latency_switch_changed ();
	void session_latency_updated (bool);

	void update_clock_visibility ();
	void every_point_zero_something_seconds ();

	void solo_blink (bool);
	void audition_blink (bool);
	void feedback_blink (bool);

	void soloing_changed (bool);
	void auditioning_changed (bool);
	void _auditioning_changed (bool);

	void feedback_detected ();
	void successful_graph_sort ();

	bool solo_alert_press (GdkEventButton* ev);
	void audition_alert_clicked ();

	void cue_ffwd_state_clicked ();
	void cue_rec_state_changed ();
	void cue_rec_state_clicked ();

	void reset_peak_display ();
	void reset_route_peak_display (ARDOUR::Route*);
	void reset_group_peak_display (ARDOUR::RouteGroup*);

	bool editor_meter_peak_button_release (GdkEventButton*);

	bool bind_lua_action_script (GdkEventButton*, int);
	void action_script_changed (int i, const std::string&);

	/* blinking alerts */
	void sync_blink (bool);
	void blink_handler (bool);

	bool                          _have_layout;
	BasicUI*                      _basic_ui;
	Gtk::Table                    _table;
	TransportControlUI            _transport_ctrl;
	ShuttleControl                _shuttle_box;
	ArdourWidgets::ArdourButton   _sync_button;
	Gtk::Label                    _punch_label;
	Gtk::Label                    _layered_label;
	Gtk::Label                    _punch_space;
	ArdourWidgets::ArdourButton   _punch_in_button;
	ArdourWidgets::ArdourButton   _punch_out_button;
	ArdourWidgets::ArdourDropdown _record_mode_selector;
	ArdourWidgets::ArdourVSpacer  _recpunch_spacer;
	ArdourWidgets::ArdourVSpacer  _latency_spacer;
	ArdourWidgets::ArdourButton   _latency_disable_button;
	Gtk::Label                    _route_latency_value;
	ArdourWidgets::ArdourButton   _auto_return_button;
	ArdourWidgets::ArdourButton   _follow_edits_button;
	TransportClock                _primary_clock;
	TransportClock                _secondary_clock;
	ArdourWidgets::ArdourVSpacer* _secondary_clock_spacer;
	ArdourWidgets::ArdourButton   _auditioning_alert_button;
	ArdourWidgets::ArdourButton   _solo_alert_button;
	ArdourWidgets::ArdourButton   _feedback_alert_button;
	Gtk::VBox                     _alert_box;
	ArdourWidgets::ArdourVSpacer  _monitor_spacer;
	ArdourWidgets::ArdourButton   _monitor_dim_button;
	ArdourWidgets::ArdourButton   _monitor_mono_button;
	ArdourWidgets::ArdourButton   _monitor_mute_button;
	ArdourWidgets::ArdourVSpacer  _cuectrl_spacer;
	ArdourWidgets::ArdourButton   _cue_rec_enable;
	ArdourWidgets::ArdourButton   _cue_play_enable;
	Gtk::HBox                     _transport_hbox;
	MiniTimeline                  _mini_timeline;
	TimeInfoBox*                  _time_info_box;
	Gtk::Table                    _editor_meter_table;
	ArdourWidgets::ArdourButton   _editor_meter_peak_display;
	LevelMeterHBox *              _editor_meter;
	ArdourWidgets::ArdourVSpacer  _meterbox_spacer;
	Gtk::HBox                     _meterbox_spacer2;
	ArdourWidgets::ArdourVSpacer  _scripts_spacer;
	ArdourWidgets::ArdourButton   _action_script_call_btn[MAX_LUA_ACTION_BUTTONS];

	bool _feedback_exists;
	bool _ambiguous_latency;
	bool _clear_editor_meter;
	bool _editor_meter_peaked;

	std::vector<std::string> _record_mode_strings;

	sigc::connection          _blink_connection;
	PBD::ScopedConnectionList _forever_connections;
	sigc::connection          _point_zero_something_second_connection;
};
