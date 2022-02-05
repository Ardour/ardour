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

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#endif

#include <algorithm>
#include <gtkmm/stock.h>

#include <gtkmm/menu.h>
#include <gtkmm/menuitem.h>

#include "pbd/string_convert.h"

#include "ardour/audioengine.h"
#include "ardour/audio_port.h"
#include "ardour/audio_track.h"
#include "ardour/midi_port.h"
#include "ardour/midi_track.h"
#include "ardour/monitor_return.h"
#include "ardour/profile.h"
#include "ardour/region.h"
#include "ardour/session.h"
#include "ardour/solo_mute_release.h"

#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/keyboard.h"
#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/window_title.h"

#include "widgets/ardour_icon.h"
#include "widgets/prompter.h"
#include "widgets/tooltips.h"

#include "actions.h"
#include "ardour_dialog.h"
#include "ardour_ui.h"
#include "gui_thread.h"
#include "instrument_selector.h"
#include "public_editor.h"
#include "recorder_group_tabs.h"
#include "recorder_ui.h"
#include "timers.h"
#include "timers.h"
#include "track_record_axis.h"
#include "ui_config.h"
#include "utils.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace Gtkmm2ext;
using namespace ArdourWidgets;
using namespace Gtk;
using namespace std;
using namespace Menu_Helpers;


#define PX_SCALE(px) std::max ((float)px, rintf ((float)px* UIConfiguration::instance ().get_ui_scale ()))

RecorderUI::RecorderUI ()
	: Tabbable (_content, _("Recorder"), X_("recorder"))
	, _toolbar_sep (1.0)
	, _btn_rec_all (_("All"))
	, _btn_rec_none (_("None"))
	, _btn_rec_forget (_("Discard Last Take"))
	, _btn_peak_reset (_("Reset Peak Hold"))
	, _monitor_in_button (_("All In"))
	, _monitor_disk_button (_("All Disk"))
	, _btn_new_plist (_("New Playlist for All Tracks"))
	, _btn_new_plist_rec (_("New Playlist for Rec-Armed"))
	, _auto_input_button (_("Auto-Input"), ArdourButton::led_default_elements)
	, _toolbar_button_height (SizeGroup::create (Gtk::SIZE_GROUP_VERTICAL))
	, _toolbar_recarm_width (SizeGroup::create (Gtk::SIZE_GROUP_HORIZONTAL))
	, _toolbar_monitoring_width (SizeGroup::create (Gtk::SIZE_GROUP_HORIZONTAL))
	, _meter_box_width (50)
	, _meter_area_cols (2)
	, _vertical (false)
	, _ruler_sep (1.0)
{

	load_bindings ();
	register_actions ();

	_transport_ctrl.setup (ARDOUR_UI::instance ());
	_transport_ctrl.map_actions ();
	_transport_ctrl.set_no_show_all ();

	signal_tabbed_changed.connect (sigc::mem_fun (*this, &RecorderUI::tabbed_changed));

	/* monitoring */
	_auto_input_button.set_related_action (ActionManager::get_action ("Transport", "ToggleAutoInput"));
	_auto_input_button.set_name ("transport option button");

	_monitor_in_button.set_related_action (ActionManager::get_action ("Transport", "SessionMonitorIn"));
	_monitor_in_button.set_name ("monitor button");

	_monitor_disk_button.set_related_action (ActionManager::get_action ("Transport", "SessionMonitorDisk"));
	_monitor_disk_button.set_name ("monitor button");

	/* rec all/none */
	_recs_label.set_text(_("Arm Tracks:"));
	_btn_rec_all.set_name ("generic button");
	_btn_rec_all.set_related_action (ActionManager::get_action (X_("Recorder"), X_("arm-all")));

	_btn_rec_none.set_name ("generic button");
	_btn_rec_none.set_related_action (ActionManager::get_action (X_("Recorder"), X_("arm-none")));

	_btn_rec_forget.set_name ("generic button");
	_btn_rec_forget.set_related_action (ActionManager::get_action (X_("Editor"), X_("remove-last-capture")));

	_btn_peak_reset.set_name ("generic button");
	_btn_peak_reset.set_related_action (ActionManager::get_action (X_("Recorder"), X_("reset-input-peak-hold")));

	/*playlists*/
	_btn_new_plist.set_name ("generic button");
	_btn_new_plist.set_related_action (ActionManager::get_action (X_("Editor"), X_("new-playlists-for-all-tracks")));

	_btn_new_plist_rec.set_name ("generic button");
	_btn_new_plist_rec.set_related_action (ActionManager::get_action (X_("Editor"), X_("new-playlists-for-armed-tracks")));

	/* standardize some button width. */
	_toolbar_recarm_width->add_widget (_btn_rec_none);
	_toolbar_recarm_width->add_widget (_btn_rec_all);

	_toolbar_monitoring_width->add_widget (_monitor_in_button);
	_toolbar_monitoring_width->add_widget (_monitor_disk_button);

	/* standardize some button heights. */
	_toolbar_button_height->add_widget (_btn_rec_all);
	_toolbar_button_height->add_widget (_btn_rec_none);
	_toolbar_button_height->add_widget (_btn_rec_forget);
	_toolbar_button_height->add_widget (_monitor_in_button);
	_toolbar_button_height->add_widget (_monitor_disk_button);
	_toolbar_button_height->add_widget (_auto_input_button);

	_toolbar_button_height->add_widget (_btn_new_plist);
	_toolbar_button_height->add_widget (_btn_new_plist_rec);

	_meter_area.set_spacing (0);
	_meter_area.pack_start (_meter_table, true, true);
	_meter_area.signal_size_request().connect (sigc::mem_fun (*this, &RecorderUI::meter_area_size_request));
	_meter_area.signal_size_allocate ().connect (mem_fun (this, &RecorderUI::meter_area_size_allocate));
	_meter_scroller.add (_meter_area);
	_meter_scroller.set_policy (POLICY_AUTOMATIC, POLICY_AUTOMATIC);

	_scroller_base.set_flags (CAN_FOCUS);
	_scroller_base.add_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK);
	_scroller_base.signal_button_press_event().connect (sigc::mem_fun(*this, &RecorderUI::scroller_button_event));
	_scroller_base.signal_button_release_event().connect (sigc::mem_fun(*this, &RecorderUI::scroller_button_event));
	_scroller_base.set_size_request (-1, PX_SCALE (20));
	_scroller_base.signal_expose_event ().connect (sigc::bind (sigc::ptr_fun (&ArdourWidgets::ArdourIcon::expose_with_text), &_scroller_base, ArdourWidgets::ArdourIcon::ShadedPlusSign,
			_("Right-click or Double-click here\nto add Tracks")));

	/* LAYOUT */

	_rec_area.set_spacing (0);
	_rec_area.pack_end (_scroller_base, true, true);
	_rec_area.pack_end (_ruler_sep, false, false, 0);

	/* HBox [ groups | tracks] */
	_rec_group_tabs = new RecorderGroupTabs (this);
	_rec_groups.pack_start (*_rec_group_tabs, false, false);
	_rec_groups.pack_start (_rec_area, true, true);

	/* Vertical scroll, all tracks */
	_rec_scroller.add (_rec_groups);
	_rec_scroller.set_shadow_type(SHADOW_IN);
	_rec_scroller.set_policy (POLICY_NEVER, POLICY_AUTOMATIC);

	/* HBox, ruler on top  [ space above headers | time-ruler ] */
	_ruler_box.pack_start (_space, false, false);
	_ruler_box.pack_start (_ruler, true, true);

	/* VBox, ruler + scroll-area for tracks */
	_rec_container.pack_start (_ruler_box, false, false);
	_rec_container.pack_start (_rec_scroller, true, true);

	_pane.add (_rec_container);
	_pane.add (_meter_scroller);

	/* Top-level VBox */
	_content.pack_start (_toolbar_sep, false, false, 1);
	_content.pack_start (_toolbar, false, false, 2);
	_content.pack_start (_pane, true, true);

	/* button_table setup is similar to transport_table in ardour_ui */
	int vpadding = 1;
	int hpadding = 2;
	int spacepad = 3;
	int col = 0;

	_button_table.attach (_transport_ctrl, col,  col + 1, 0, 1, FILL, FILL, hpadding, vpadding);
	col += 1;

	_button_table.attach (_duration_info_box,  col,     col + 1, 0, 1, FILL, FILL,   hpadding, vpadding);
	_button_table.attach (_xrun_info_box,      col + 1, col + 2, 0, 1, FILL, FILL,   hpadding, vpadding);
	_button_table.attach (_btn_rec_forget,     col,     col + 2, 1, 2, FILL, SHRINK, hpadding, vpadding);
	col += 2;

	_button_table.attach (*(manage (new ArdourVSpacer ())),  col,  col + 1, 0, 2, FILL, FILL, spacepad, vpadding);
	col += 1;

	_button_table.attach (_recs_label,   col,     col + 2, 0, 1, FILL, FILL, hpadding, vpadding);
	_button_table.attach (_btn_rec_all,  col,     col + 1, 1, 2, FILL, FILL, hpadding, vpadding);
	_button_table.attach (_btn_rec_none, col + 1, col + 2, 1, 2, FILL, FILL, hpadding, vpadding);
	col += 2;

	_button_table.attach (*(manage (new ArdourVSpacer ())),  col,  col + 1, 0, 2, FILL, FILL, spacepad, vpadding);
	col += 1;

	_button_table.attach (_auto_input_button,   col,     col + 2, 0, 1 , FILL, SHRINK, hpadding, vpadding);
	_button_table.attach (_monitor_in_button,   col,     col + 1, 1, 2 , FILL, SHRINK, hpadding, vpadding);
	_button_table.attach (_monitor_disk_button, col + 1, col + 2, 1, 2 , FILL, SHRINK, hpadding, vpadding);
	col += 2;

	_button_table.attach (*(manage (new ArdourVSpacer ())),  col,  col + 1, 0, 2, FILL, FILL, spacepad, vpadding);
	col += 1;

	_button_table.attach (_btn_new_plist,       col,     col + 2, 0, 1 , FILL, SHRINK, hpadding, vpadding);
	_button_table.attach (_btn_new_plist_rec,   col,     col + 2, 1, 2 , FILL, SHRINK, hpadding, vpadding);
	col += 2;

	_button_table.attach (*(manage (new ArdourVSpacer ())),  col,  col + 1, 0, 2, FILL, FILL, spacepad, vpadding);
	col += 1;

	_toolbar.pack_start (_button_table, false, false);
	_toolbar.pack_end (_btn_peak_reset, false, false, 4);
	_toolbar.pack_end (_remain_info_box, false, false, 4);

	/* tooltips */
	set_tooltip (_btn_rec_all, _("Record enable all tracks"));
	set_tooltip (_btn_rec_none, _("Disable recording of all tracks"));
	set_tooltip (_btn_peak_reset, _("Reset peak-hold indicator of all input meters"));
	set_tooltip (_auto_input_button, _("Track Input Monitoring automatically follows transport state"));
	set_tooltip (_monitor_in_button, _("Force all tracks to monitor Input, unless they are explicitly set to monitor Disk"));
	set_tooltip (_monitor_disk_button, _("Force all tracks to monitor Disk playback, unless they are explicitly set to Input"));
	set_tooltip (_btn_new_plist, _("Create a new playlist for all tracks and switch to it."));
	set_tooltip (_btn_new_plist_rec, _("Create a new playlist for all rec-armed tracks"));
	set_tooltip (_xrun_info_box, _("X-runs: Soundcard buffer under- or over-run occurrences in the last recording take"));
	set_tooltip (_remain_info_box, _("Remaining Time:  Recording time available on the current disk with currently armed tracks"));
	set_tooltip (_duration_info_box, _("Duration: Length of the most recent (or current) recording take"));
	set_tooltip (_btn_rec_forget, _("Delete the region AND the audio files of the last recording take"));

	/* show [almost] all */
	_btn_rec_all.show ();
	_btn_rec_none.show ();
	_btn_rec_forget.show ();
	_btn_peak_reset.show ();
	_btn_new_plist.show ();
	_btn_new_plist_rec.show ();
	_button_table.show ();
	_monitor_in_button.show ();
	_monitor_disk_button.show ();
	_auto_input_button.show ();
	_space.show ();
	_ruler_box.show ();
	_ruler_sep.show ();
	_scroller_base.show ();
	_toolbar_sep.show ();
	_rec_area.show ();
	_rec_scroller.show ();
	_rec_groups.show ();
	_rec_group_tabs->show ();
	_rec_container.show ();
	_duration_info_box.show ();
	_xrun_info_box.show ();
	_remain_info_box.show ();
	_meter_table.show ();
	_meter_area.show ();
	_meter_scroller.show ();
	_pane.show ();
	_content.show ();

	/* setup keybidings */
	_content.set_data ("ardour-bindings", bindings);

	/* subscribe to signals */
	AudioEngine::instance ()->Running.connect (_engine_connections, invalidator (*this), boost::bind (&RecorderUI::start_updating, this), gui_context ());
	AudioEngine::instance ()->Stopped.connect (_engine_connections, invalidator (*this), boost::bind (&RecorderUI::stop_updating, this), gui_context ());
	AudioEngine::instance ()->Halted.connect (_engine_connections, invalidator (*this), boost::bind (&RecorderUI::stop_updating, this), gui_context ());
	AudioEngine::instance ()->PortConnectedOrDisconnected.connect (_engine_connections, invalidator (*this), boost::bind (&RecorderUI::port_connected_or_disconnected, this, _2, _4), gui_context ());
	AudioEngine::instance ()->PortPrettyNameChanged.connect (_engine_connections, invalidator (*this), boost::bind (&RecorderUI::port_pretty_name_changed, this, _1), gui_context ());
	AudioEngine::instance ()->PhysInputChanged.connect (_engine_connections, invalidator (*this), boost::bind (&RecorderUI::add_or_remove_io, this, _1, _2, _3), gui_context ());

	PresentationInfo::Change.connect (*this, invalidator (*this), boost::bind (&RecorderUI::presentation_info_changed, this, _1), gui_context());
	Config->ParameterChanged.connect (*this, invalidator (*this), boost::bind (&RecorderUI::parameter_changed, this, _1), gui_context ());
	UIConfiguration::instance().ParameterChanged.connect (sigc::mem_fun (*this, &RecorderUI::parameter_changed));
	//ARDOUR_UI::instance()->Escape.connect (*this, invalidator (*this), boost::bind (&RecorderUI::escape, this), gui_context());

	/* init */
	update_title ();
	update_sensitivity ();

	float          fract;
	XMLNode const* settings = ARDOUR_UI::instance()->recorder_settings();
	if (!settings || !settings->get_property ("recorder-vpane-pos", fract) || fract > 1.0) {
		fract = 0.75f;
	}
	_pane.set_divider (0, fract);
}

RecorderUI::~RecorderUI ()
{
	delete _rec_group_tabs;
}

void
RecorderUI::cleanup ()
{
	_visible_recorders.clear ();
	stop_updating ();
	_engine_connections.drop_connections ();
}

Gtk::Window*
RecorderUI::use_own_window (bool and_fill_it)
{
	bool new_window = !own_window ();

	Gtk::Window* win = Tabbable::use_own_window (and_fill_it);

	if (win && new_window) {
		win->set_name ("RecorderWindow");
		ARDOUR_UI::instance ()->setup_toplevel_window (*win, _("Recorder"), this);
		win->signal_event ().connect (sigc::bind (sigc::ptr_fun (&Keyboard::catch_user_event_for_pre_dialog_focus), win));
		win->set_data ("ardour-bindings", bindings);
		update_title ();
#if 0 // TODO
		if (!win->get_focus()) {
			win->set_focus (scroller);
		}
#endif
	}

	contents ().show ();
	return win;
}

void
RecorderUI::tabbed_changed (bool tabbed)
{
	if (tabbed) {
		_transport_ctrl.hide ();
	} else {
		_transport_ctrl.show ();
	}
}

XMLNode&
RecorderUI::get_state ()
{
	XMLNode* node = new XMLNode (X_("Recorder"));
	node->add_child_nocopy (Tabbable::get_state ());
	node->set_property (X_("recorder-vpane-pos"), _pane.get_divider ());
	return *node;
}

int
RecorderUI::set_state (const XMLNode& node, int version)
{
	return Tabbable::set_state (node, version);
}

void
RecorderUI::load_bindings ()
{
	bindings = Bindings::get_bindings (X_("Recorder"));
}

void
RecorderUI::register_actions ()
{
	Glib::RefPtr<ActionGroup> group = ActionManager::create_action_group (bindings, X_("Recorder"));
	ActionManager::register_action (group, "reset-input-peak-hold", _("Reset Input Peak Hold"), sigc::mem_fun (*this, &RecorderUI::peak_reset));
	ActionManager::register_action (group, "arm-all", _("Record Arm All Tracks"), sigc::mem_fun (*this, &RecorderUI::arm_all));
	ActionManager::register_action (group, "arm-none", _("Disable Record Arm of All Tracks"), sigc::mem_fun (*this, &RecorderUI::arm_none));
}

void
RecorderUI::set_session (Session* s)
{
	SessionHandlePtr::set_session (s);

	_ruler.set_session (s);
	_duration_info_box.set_session (s);
	_xrun_info_box.set_session (s);
	_remain_info_box.set_session (s);
	_transport_ctrl.set_session (s);
	_rec_group_tabs->set_session (s);

	update_sensitivity ();

	if (!_session) {
		_recorders.clear ();
		_visible_recorders.clear ();
		return;
	}

	XMLNode* node = ARDOUR_UI::instance()->recorder_settings();
	set_state (*node, Stateful::loading_state_version);

	_session->DirtyChanged.connect (_session_connections, invalidator (*this), boost::bind (&RecorderUI::update_title, this), gui_context ());
	_session->StateSaved.connect (_session_connections, invalidator (*this), boost::bind (&RecorderUI::update_title, this), gui_context ());

	_session->RouteAdded.connect (_session_connections, invalidator (*this), boost::bind (&RecorderUI::add_routes, this, _1), gui_context ());
	TrackRecordAxis::CatchDeletion.connect (*this, invalidator (*this), boost::bind (&RecorderUI::remove_route, this, _1), gui_context ());
	TrackRecordAxis::EditNextName.connect (*this, invalidator (*this), boost::bind (&RecorderUI::tra_name_edit, this, _1, _2), gui_context ());

	_session->config.ParameterChanged.connect (_session_connections, invalidator (*this), boost::bind (&RecorderUI::parameter_changed, this, _1), gui_context ());

	Region::RegionsPropertyChanged.connect (*this, invalidator (*this), boost::bind (&RecorderUI::regions_changed, this, _1, _2), gui_context());
	_session->StartTimeChanged.connect (_session_connections, invalidator (*this), boost::bind (&RecorderUI::gui_extents_changed, this), gui_context());
	_session->EndTimeChanged.connect (_session_connections, invalidator (*this), boost::bind (&RecorderUI::gui_extents_changed, this), gui_context());
	_session->RecordStateChanged.connect (_session_connections, invalidator (*this), boost::bind (&RecorderUI::update_sensitivity, this), gui_context());
	_session->UpdateRouteRecordState.connect (_session_connections, invalidator (*this), boost::bind (&RecorderUI::update_recordstate, this), gui_context());

	/* map_parameters */
	parameter_changed ("show-group-tabs");

	update_title ();
	initial_track_display ();
	start_updating ();
}

void
RecorderUI::session_going_away ()
{
	ENSURE_GUI_THREAD (*this, &RecorderUI::session_going_away);
	SessionHandlePtr::session_going_away ();
	update_title ();
}

void
RecorderUI::update_title ()
{
	if (!own_window ()) {
		return;
	}

	if (_session) {
		string n;

		if (_session->snap_name () != _session->name ()) {
			n = _session->snap_name ();
		} else {
			n = _session->name ();
		}

		if (_session->dirty ()) {
			n = "*" + n;
		}

		WindowTitle title (n);
		title += S_("Window|Recorder");
		title += Glib::get_application_name ();
		own_window ()->set_title (title.get_string ());

	} else {
		WindowTitle title (S_("Window|Recorder"));
		title += Glib::get_application_name ();
		own_window ()->set_title (title.get_string ());
	}
}

void
RecorderUI::update_sensitivity ()
{
	const bool en      = _session ? true : false;
	const bool have_ms = Config->get_use_monitor_bus();

	ActionManager::get_action (X_("Recorder"), X_("arm-all"))->set_sensitive (en);
	ActionManager::get_action (X_("Recorder"), X_("arm-none"))->set_sensitive (en);

	for (InputPortMap::const_iterator i = _input_ports.begin (); i != _input_ports.end (); ++i) {
		i->second->allow_monitoring (have_ms && en);
		i->second->set_sensitive (en);
		if (!en) {
			i->second->clear ();
		}
	}
}

void
RecorderUI::update_recordstate ()
{
	for (InputPortMap::const_iterator i = _input_ports.begin (); i != _input_ports.end (); ++i) {
		i->second->update_rec_stat ();
	}
}

void
RecorderUI::update_monitorstate (std::string pn, bool en)
{
	InputPortMap::iterator im = _input_ports.find (pn);
	if (im != _input_ports.end()) {
		im->second->update_monitorstate (en);
	}
}

void
RecorderUI::parameter_changed (string const& p)
{
	if (p == "input-meter-layout") {
		start_updating ();
	} else if (p == "input-meter-scopes") {
		start_updating ();
	} else if (p == "use-monitor-bus") {
		bool have_ms = Config->get_use_monitor_bus();
		for (InputPortMap::const_iterator i = _input_ports.begin (); i != _input_ports.end (); ++i) {
			i->second->allow_monitoring (have_ms);
		}
	} else if (p == "show-group-tabs") {
		bool const s = _session ? _session->config.get_show_group_tabs () : true;
		if (s) {
			_rec_group_tabs->show ();
		} else {
			_rec_group_tabs->hide ();
		}
	}
}

bool
RecorderUI::scroller_button_event (GdkEventButton* ev)
{
	if ((ev->type == GDK_2BUTTON_PRESS && ev->button == 1) || (ev->type == GDK_BUTTON_RELEASE && Keyboard::is_context_menu_event (ev))) {
		ARDOUR_UI::instance()->add_route ();
		return true;
	}
	return false;
}

void
RecorderUI::start_updating ()
{
	if (_input_ports.size ()) {
		stop_updating ();
	}

	PortManager::AudioInputPorts const aip (AudioEngine::instance ()->audio_input_ports ());
	PortManager::MIDIInputPorts const mip (AudioEngine::instance ()->midi_input_ports ());

	if (aip.size () + mip.size () == 0) {
		return;
	}

	switch (UIConfiguration::instance ().get_input_meter_layout ()) {
		case LayoutAutomatic:
			if (aip.size () + mip.size () > 16) {
				_vertical = true;
			} else {
				_vertical = false;
			}
			break;
		case LayoutVertical:
				_vertical = true;
			break;
		case LayoutHorizontal:
				_vertical = false;
			break;
	}

	/* Audio */
	for (PortManager::AudioInputPorts::const_iterator i = aip.begin (); i != aip.end (); ++i) {
		_input_ports[i->first] = boost::shared_ptr<RecorderUI::InputPort> (new InputPort (i->first, DataType::AUDIO, this, _vertical));
		set_connections (i->first);
	}

	/* MIDI */
	for (PortManager::MIDIInputPorts::const_iterator i = mip.begin (); i != mip.end (); ++i) {
		string pn = AudioEngine::instance()->get_pretty_name_by_name (i->first);
		if (PortManager::port_is_control_only (pn)) {
			continue;
		}
		_input_ports[i->first] = boost::shared_ptr<RecorderUI::InputPort> (new InputPort (i->first, DataType::MIDI, this, _vertical));
		set_connections (i->first);
	}

	update_io_widget_labels ();
	meter_area_layout ();
	_meter_area.queue_resize ();

	MonitorPort& mp (AudioEngine::instance()->monitor_port ());
	mp.MonitorInputChanged.connect (_monitor_connection, invalidator (*this), boost::bind (&RecorderUI::update_monitorstate, this, _1, _2), gui_context());

	const bool en      = _session ? true : false;
	const bool have_ms = Config->get_use_monitor_bus();

	for (InputPortMap::const_iterator i = _input_ports.begin (); i != _input_ports.end (); ++i) {
		i->second->update_monitorstate (mp.monitoring (i->first));
		i->second->allow_monitoring (have_ms && en);
		i->second->set_sensitive (en);
	}

	_fast_screen_update_connection.disconnect ();
	/* https://developer.gnome.org/glib/stable/glib-The-Main-Event-Loop.html#G-PRIORITY-HIGH-IDLE:CAPS */
	_fast_screen_update_connection = Glib::signal_timeout().connect (sigc::mem_fun (*this, &RecorderUI::update_meters), 40, GDK_PRIORITY_REDRAW + 10);
}

void
RecorderUI::stop_updating ()
{
	_fast_screen_update_connection.disconnect ();
	_monitor_connection.disconnect ();
	container_clear (_meter_table);
	_input_ports.clear ();
}

void
RecorderUI::add_or_remove_io (DataType dt, vector<string> ports, bool add)
{
	_fast_screen_update_connection.disconnect ();
	bool spill_changed = false;

	if (_input_ports.empty () && add) {
		_monitor_connection.disconnect ();
		MonitorPort& mp (AudioEngine::instance()->monitor_port ());
		mp.MonitorInputChanged.connect (_monitor_connection, invalidator (*this), boost::bind (&RecorderUI::update_monitorstate, this, _1, _2), gui_context());
	}

	if (add) {
		for (vector<string>::const_iterator i = ports.begin (); i != ports.end (); ++i) {
			string pn = AudioEngine::instance()->get_pretty_name_by_name (*i);
			if (dt==DataType::MIDI && PortManager::port_is_control_only (pn)) {
				continue;
			}
			_input_ports[*i] = boost::shared_ptr<RecorderUI::InputPort> (new InputPort (*i, dt, this, _vertical));
			set_connections (*i);
		}
	} else {
		for (vector<string>::const_iterator i = ports.begin (); i != ports.end (); ++i) {
			_input_ports.erase (*i);
			spill_changed |= 0 != _spill_port_names.erase (*i);
		}
	}

	update_io_widget_labels ();
	update_sensitivity ();
	meter_area_layout ();
	_meter_area.queue_resize ();

	if (spill_changed) {
		update_rec_table_layout ();
	}

	if (_input_ports.size ()) {
	_fast_screen_update_connection = Glib::signal_timeout().connect (sigc::mem_fun (*this, &RecorderUI::update_meters), 40, GDK_PRIORITY_REDRAW + 10);
	}
}

void
RecorderUI::update_io_widget_labels ()
{
	uint32_t n_audio = 0;
	uint32_t n_midi = 0;

	InputPortSet ips;
	for (InputPortMap::const_iterator i = _input_ports.begin (); i != _input_ports.end (); ++i) {
		ips.insert (i->second);
	}
	for (InputPortSet::const_iterator i = ips.begin (); i != ips.end (); ++i) {
		boost::shared_ptr<InputPort> const& ip = *i;
		switch (ip->data_type ()) {
			case DataType::AUDIO:
				ip->set_frame_label (string_compose (_("Audio Input %1"), ++n_audio));
				break;
			case DataType::MIDI:
				ip->set_frame_label (string_compose (_("MIDI Input %1"), ++n_midi));
				break;
		}
	}
}

bool
RecorderUI::update_meters ()
{
	PortManager::AudioInputPorts const aip (AudioEngine::instance ()->audio_input_ports ());

	/* scope data needs to be read contiously */
	for (PortManager::AudioInputPorts::const_iterator i = aip.begin (); i != aip.end (); ++i) {
		InputPortMap::iterator im = _input_ports.find (i->first);
		if (im != _input_ports.end()) {
			im->second->update (*(i->second.scope));
		}
	}

	if (!contents ().is_mapped ()) {
		return true;
	}

	for (PortManager::AudioInputPorts::const_iterator i = aip.begin (); i != aip.end (); ++i) {
		InputPortMap::iterator im = _input_ports.find (i->first);
		if (im != _input_ports.end()) {
			im->second->update (accurate_coefficient_to_dB (i->second.meter->level), accurate_coefficient_to_dB (i->second.meter->peak));
		}
	}

	PortManager::MIDIInputPorts const mip (AudioEngine::instance ()->midi_input_ports ());
	for (PortManager::MIDIInputPorts::const_iterator i = mip.begin (); i != mip.end (); ++i) {
		InputPortMap::iterator im = _input_ports.find (i->first);
		if (im != _input_ports.end()) {
			im->second->update ((float const*)i->second.meter->chn_active);
			im->second->update (*(i->second.monitor));
		}
	}

	for (list<TrackRecordAxis*>::const_iterator i = _recorders.begin (); i != _recorders.end (); ++i) {
		(*i)->fast_update ();
	}

	if (_session && _session->actively_recording ()) {
		/* maybe grow showing rec-regions */
		gui_extents_changed ();
	}
	return true;
}

int
RecorderUI::calc_columns (int child_width, int parent_width)
{
	int n_col = parent_width / child_width;
	if (n_col <= 2) {
		/* at least 2 columns*/
		return 2;
	} else if (n_col <= 4) {
		/* allow 3 (2 audio + 1 MIDI) */
		return n_col;
	}
	/* otherwise only even number of cols */
	return n_col & ~1;
}

void
RecorderUI::meter_area_layout ()
{
	container_clear (_meter_table);

	int col = 0;
	int row = 0;
	int spc = 2;

	InputPortSet ips;
	for (InputPortMap::const_iterator i = _input_ports.begin (); i != _input_ports.end (); ++i) {
		boost::shared_ptr<InputPort> const& ip = i->second;
		ip->show ();
		ips.insert (ip);
	}

	for (InputPortSet::const_iterator i = ips.begin (); i != ips.end (); ++i) {
		boost::shared_ptr<InputPort> const& ip = *i;
		_meter_table.attach (*ip, col, col + 1, row, row + 1, SHRINK|FILL, SHRINK, spc, spc);

		if (++col >= _meter_area_cols) {
			col = 0;
			++row;
		}
	}
}

void
RecorderUI::meter_area_size_allocate (Allocation& allocation)
{
	int mac = calc_columns (_meter_box_width, _meter_area.get_width ());
#if 0
	printf ("RecorderUI::meter_area_size_allocate: %dx%d | mbw: %d cols:%d new-cols: %d\n",
			allocation.get_width (), allocation.get_height (),
			_meter_box_width, _meter_area_cols, mac);
#endif

	if (_meter_area_cols == mac || _input_ports.size () == 0) {
		return;
	}

	_meter_area_cols = mac;
	meter_area_layout ();
	_meter_area.queue_resize ();
}

void
RecorderUI::meter_area_size_request (GtkRequisition* requisition)
{
	int width  = 2;
	int height = 2;
	int spc    = 2;

	for (InputPortMap::const_iterator i = _input_ports.begin (); i != _input_ports.end (); ++i) {
		boost::shared_ptr<InputPort> const& ip = i->second;
		Requisition r = ip->size_request ();
		width  = std::max (width, r.width + spc * 2);
		height = std::max (height, r.height + spc * 2);
	}
	_meter_box_width = width;

	//height *= ceilf (_input_ports.size () / (float)_meter_area_cols);

	Requisition r  = _meter_table.size_request ();
	requisition->width  = _meter_box_width * 2; // at least 2 columns wide
	requisition->height = std::max (r.height, height);
#if 0
	printf ("RecorderUI::meter_area_size_request: %dx%d\n", requisition->width, requisition->height);
#endif
}

void
RecorderUI::port_connected_or_disconnected (string p1, string p2)
{
	if (_input_ports.find (p1) != _input_ports.end ()) {
		set_connections (p1);
	}
	if (_input_ports.find (p2) != _input_ports.end ()) {
		set_connections (p2);
	}
}

void
RecorderUI::port_pretty_name_changed (string pn)
{
	if (_input_ports.find (pn) != _input_ports.end ()) {
		_input_ports[pn]->setup_name ();
	}
}

void
RecorderUI::regions_changed (boost::shared_ptr<ARDOUR::RegionList>, PBD::PropertyChange const& what_changed)
{
	PBD::PropertyChange interests;

	interests.add (ARDOUR::Properties::length);
	if (what_changed.contains (interests)) {
		gui_extents_changed ();
	}
}

void
RecorderUI::gui_extents_changed ()
{
	pair<timepos_t, timepos_t> ext = PublicEditor::instance().session_gui_extents ();

	if (ext.first == timepos_t::max (ext.first.time_domain()) || ext.first >= ext.second) {
		return;
	}

	samplepos_t start = ext.first.samples();
	samplepos_t end = ext.second.samples();

	for (list<TrackRecordAxis*>::const_iterator i = _recorders.begin (); i != _recorders.end (); ++i) {
		(*i)->rec_extent (start, end);
	}

	/* round to the next minute */
	if (_session) {
		const samplecnt_t one_minute = 60 * _session->nominal_sample_rate ();
		start  = (start / one_minute) * one_minute;
		end = ((end / one_minute) + 1) * one_minute;
	}

	_ruler.set_gui_extents (start, end);
	for (list<TrackRecordAxis*>::const_iterator i = _recorders.begin (); i != _recorders.end (); ++i) {
		(*i)->set_gui_extents (start, end);
	}
}

void
RecorderUI::set_connections (string const& p)
{
	if (!_session) {
		return;
	}

	WeakRouteList wrl;

	boost::shared_ptr<RouteList> rl = _session->get_tracks ();
	for (RouteList::const_iterator r = rl->begin(); r != rl->end(); ++r) {
		if ((*r)->input()->connected_to (p)) {
			wrl.push_back (*r);
		}
	}

	_input_ports[p]->set_connections (wrl);

	// TODO: think.
	// only clear when port is spilled and cnt == 0 ?
	// otherwise only update spilled tracks if port is spilled?
	if (!_spill_port_names.empty ()) {
		for (InputPortMap::const_iterator i = _input_ports.begin (); i != _input_ports.end (); ++i) {
			i->second->spill (false);
		}
		_spill_port_names.clear ();
		update_rec_table_layout ();
	}
}

void
RecorderUI::add_track (string const& p)
{
	if (!_session) {
		return;
	}
	new_track_for_port (_input_ports[p]->data_type (), p);
}

void
RecorderUI::spill_port (string const& p)
{
	bool ok = false;
	if (_input_ports[p]->spilled ()) {
		ok = _input_ports[p]->spill (true);
	}

	bool update;
	if (ok) {
		pair<set<string>::iterator, bool> rv = _spill_port_names.insert (p);
		update = rv.second;
	} else {
		update = 0 != _spill_port_names.erase (p);
	}
	if (update) {
		update_rec_table_layout ();
	}
}

void
RecorderUI::initial_track_display ()
{
	boost::shared_ptr<RouteList> r = _session->get_tracks ();
	RouteList                    rl (*r);
	_recorders.clear ();
	add_routes (rl);
}

void
RecorderUI::add_routes (RouteList& rl)
{
	rl.sort (Stripable::Sorter ());
	for (RouteList::iterator r = rl.begin (); r != rl.end (); ++r) {
		/* we're only interested in Tracks */
		if (!boost::dynamic_pointer_cast<Track> (*r)) {
			continue;
		}

		TrackRecordAxis* rec = new TrackRecordAxis (/**this,*/ _session, *r);
		_recorders.push_back (rec);
	}
	gui_extents_changed ();
	update_rec_table_layout ();
}

void
RecorderUI::remove_route (TrackRecordAxis* ra)
{
	if (!_session || _session->deletion_in_progress ()) {
		_recorders.clear ();
		return;
	}
	list<TrackRecordAxis*>::iterator i = find (_recorders.begin (), _recorders.end (), ra);
	if (i != _recorders.end ()) {
		_rec_area.remove (**i);
		_recorders.erase (i);
	}
	update_rec_table_layout ();
}

void
RecorderUI::tra_name_edit (TrackRecordAxis* tra, bool next)
{
	list<TrackRecordAxis*>::iterator i = find (_visible_recorders.begin (), _visible_recorders.end (), tra);
	if (i == _visible_recorders.end ()) {
		return;
	}
	if (next && ++i != _visible_recorders.end ()) {
		(*i)->start_rename ();
	} else if (!next && i != _visible_recorders.begin ()) {
		(*--i)->start_rename ();
	}
}

struct TrackRecordAxisSorter {
	bool operator() (const TrackRecordAxis* ca, const TrackRecordAxis* cb)
	{
		boost::shared_ptr<Stripable> const& a = ca->stripable ();
		boost::shared_ptr<Stripable> const& b = cb->stripable ();
		return Stripable::Sorter(true)(a, b);
	}
};

void
RecorderUI::presentation_info_changed (PBD::PropertyChange const& what_changed)
{
	if (what_changed.contains (Properties::hidden)) {
		update_rec_table_layout ();
	} else if (what_changed.contains (Properties::order)) {
		/* test if effective order changed. When deleting tracks
		 * the PI:order_key changes, but the layout does not change.
		 */
		list<TrackRecordAxis*> rec (_recorders);
		_recorders.sort (TrackRecordAxisSorter ());
		if (_recorders != rec) {
			update_rec_table_layout ();
		}
	}
}

void
RecorderUI::update_rec_table_layout ()
{
	_visible_recorders.clear ();
	_recorders.sort (TrackRecordAxisSorter ());
	_ruler_width_update_connection.disconnect ();

	list<TrackRecordAxis*>::const_iterator i;
	for (i = _recorders.begin (); i != _recorders.end (); ++i) {
		if ((*i)->route ()->presentation_info ().hidden ()) {
			if ((*i)->get_parent ()) {
				_rec_area.remove (**i);
			}
			continue;
		}

		/* spill */
		if (!_spill_port_names.empty ()) {
			bool connected = false;
			for (set<string>::const_iterator j = _spill_port_names.begin(); j != _spill_port_names.end(); ++j) {
				if ((*i)->route ()->input()->connected_to (*j)) {
					connected = true;
					break;
				}
			}
			if (!connected) {
				if ((*i)->get_parent ()) {
					_rec_area.remove (**i);
				}
				continue;
			}
		}

		if (!(*i)->get_parent ()) {
			_rec_area.pack_start (**i, false, false);
		} else {
			_rec_area.reorder_child (**i, -1);
		}
		(*i)->show ();
		_visible_recorders.push_back (*i);

		if (!_ruler_width_update_connection.connected ()) {
			_ruler_width_update_connection = (*i)->signal_size_allocate().connect (sigc::bind (sigc::mem_fun (*this, &RecorderUI::update_spacer_width), *i));
		}
	}

	if (!_ruler_width_update_connection.connected ()) {
		_ruler.hide ();
	} else {
		_ruler.show ();
	}

	_rec_group_tabs->set_dirty ();
}

list<TrackRecordAxis*>
RecorderUI::visible_recorders () const
{
	return _visible_recorders;
}

void
RecorderUI::update_spacer_width (Allocation&, TrackRecordAxis* rec)
{
	int w = rec->summary_xpos ();
	if (_rec_group_tabs->is_visible ()) {
		w += _rec_group_tabs->get_width ();
	}
	_space.set_size_request (w, -1); //< Note: this is idempotent
	_ruler.set_right_edge (rec->summary_width ());
}

void
RecorderUI::new_track_for_port (DataType dt, string const& port_name)
{
	ArdourDialog d (_("Create track for input"), true, false);

	Entry track_name_entry;
	InstrumentSelector instrument_combo(InstrumentSelector::ForTrackDefault);
	ComboBoxText strict_io_combo;

	string pn = AudioEngine::instance()->get_pretty_name_by_name (port_name);
	if (!pn.empty ()) {
		track_name_entry.set_text (pn);
	} else {
		track_name_entry.set_text (port_name);
	}

	strict_io_combo.append_text (_("Flexible-I/O"));
	strict_io_combo.append_text (_("Strict-I/O"));
	strict_io_combo.set_active (Config->get_strict_io () ? 1 : 0);

	Label* l;
	Table  t;
	int    row = 0;

	t.set_spacings (6);

	l = manage (new Label (string_compose (_("Create new track connected to port '%1'"), pn.empty() ? port_name : pn)));
	t.attach (*l, 0, 2, row, row + 1, EXPAND | FILL, SHRINK);
	++row;

	l = manage (new Label (_("Track name:")));
	t.attach (*l,                0, 1, row, row + 1, SHRINK, SHRINK);
	t.attach (track_name_entry,  1, 2, row, row + 1, EXPAND | FILL, SHRINK);
	++row;

	if (dt == DataType::MIDI) {
		l = manage (new Label (_("Instrument:")));
		t.attach (*l,               0, 1, row, row + 1, SHRINK, SHRINK);
		t.attach (instrument_combo, 1, 2, row, row + 1, EXPAND | FILL, SHRINK);
		++row;
	}

	if (Profile->get_mixbus ()) {
		strict_io_combo.set_active (1);
	} else {
		l = manage (new Label (_("Strict I/O:")));
		t.attach (*l,              0, 1, row, row + 1, SHRINK, SHRINK);
		t.attach (strict_io_combo, 1, 3, row, row + 1, FILL, SHRINK);
		set_tooltip (strict_io_combo, _("With strict-i/o enabled, Effect Processors will not modify the number of channels on a track. The number of output channels will always match the number of input channels."));
	}

	d.get_vbox()->pack_start (t, false, false);
	d.get_vbox()->set_border_width (12);

	d.add_button(Stock::CANCEL, RESPONSE_CANCEL);
	d.add_button(Stock::OK, RESPONSE_OK);
	d.set_default_response (RESPONSE_OK);
	d.set_position (WIN_POS_MOUSE);
	d.show_all ();

	track_name_entry.signal_activate().connect (sigc::bind (sigc::mem_fun (d, &Dialog::response), RESPONSE_OK));

	if (d.run() != RESPONSE_OK) {
		return;
	}

	d.hide ();

	bool strict_io = strict_io_combo.get_active_row_number () == 1;
	string track_name = track_name_entry.get_text();

	uint32_t outputs = 2;
	if (_session->master_out ()) {
		outputs = max (outputs, _session->master_out ()->n_inputs ().n_audio ());
	}

	if (dt == DataType::AUDIO) {
		boost::shared_ptr<Route> r;
		try {
			list<boost::shared_ptr<AudioTrack> > tl = _session->new_audio_track (1, outputs, NULL, 1, track_name, PresentationInfo::max_order, Normal, false);
			r = tl.front ();
		} catch (...) {
			return;
		}
		if (r) {
			r->set_strict_io (strict_io);
			r->input ()->audio (0)->connect (port_name);
		}
	} else if (dt == DataType::MIDI) {
		boost::shared_ptr<Route> r;
		try {
			list<boost::shared_ptr<MidiTrack> > tl = _session->new_midi_track (
					ChanCount (DataType::MIDI, 1), ChanCount (DataType::MIDI, 1),
					strict_io,
					instrument_combo.selected_instrument (), (Plugin::PresetRecord*) 0,
					(RouteGroup*) 0,
					1, track_name, PresentationInfo::max_order, Normal, false);
			r = tl.front ();
		} catch (...) {
			return;
		}
		if (r) {
			r->input ()->midi (0)->connect (port_name);
		}
	}
}

void
RecorderUI::arm_all ()
{
	if (_session) {
		_session->set_all_tracks_record_enabled (true);
	}
}

void
RecorderUI::arm_none ()
{
	if (_session) {
		_session->set_all_tracks_record_enabled (false);
	}
}

void
RecorderUI::peak_reset ()
{
	AudioEngine::instance ()->reset_input_meters ();
}

/* ****************************************************************************/

bool RecorderUI::InputPort::_size_groups_initialized = false;

Glib::RefPtr<Gtk::SizeGroup> RecorderUI::InputPort::_name_size_group;
Glib::RefPtr<Gtk::SizeGroup> RecorderUI::InputPort::_ctrl_size_group;
Glib::RefPtr<Gtk::SizeGroup> RecorderUI::InputPort::_monitor_size_group;

RecorderUI::InputPort::InputPort (string const& name, DataType dt, RecorderUI* parent, bool vertical)
	: _dt (dt)
	, _monitor (dt, AudioEngine::instance()->sample_rate (), vertical ? InputPortMonitor::Vertical : InputPortMonitor::Horizontal)
	, _alignment (0.5, 0.5, 0, 0)
	, _frame (vertical ? ArdourWidgets::Frame::Vertical : ArdourWidgets::Frame::Horizontal)
	, _spill_button ("", ArdourButton::default_elements, true)
	, _monitor_button (_("PFL"), ArdourButton::default_elements)
	, _name_button (name)
	, _name_label ("", ALIGN_CENTER, ALIGN_CENTER, false)
	, _add_button ("+")
	, _port_name (name)
	, _solo_release (0)
{
	if (!_size_groups_initialized) {
		_size_groups_initialized = true;
		_name_size_group = Gtk::SizeGroup::create (Gtk::SIZE_GROUP_HORIZONTAL);
		_ctrl_size_group = Gtk::SizeGroup::create (Gtk::SIZE_GROUP_HORIZONTAL);
		_monitor_size_group = Gtk::SizeGroup::create (Gtk::SIZE_GROUP_BOTH);
	}

	Box*   box_t;
	Box*   box_n;
	Table* ctrls = manage (new Table);

	if (vertical) {
		box_t = manage (new VBox);
		box_n = manage (new VBox);
	} else {
		box_t = manage (new HBox);
		box_n = manage (new VBox);
	}

	_spill_button.set_name ("generic button");
	_spill_button.set_sizing_text(_("(none)"));
	_spill_button.signal_clicked.connect (sigc::bind (sigc::mem_fun (*parent, &RecorderUI::spill_port), name));

	_monitor_button.set_name ("solo button");
	//_monitor_button.signal_clicked.connect (sigc::bind (sigc::mem_fun (*parent, &RecorderUI::monitor_port), name));
	_monitor_button.signal_button_press_event().connect (sigc::mem_fun(*this, &InputPort::monitor_press), false);
	_monitor_button.signal_button_release_event().connect (sigc::mem_fun(*this, &InputPort::monitor_release), false);
	set_tooltip (_monitor_button, _("Solo/Listen to this input"));

	_add_button.set_name ("generic button");
	_add_button.set_icon (ArdourIcon::PlusSign);
	_add_button.signal_clicked.connect (sigc::bind (sigc::mem_fun (*parent, &RecorderUI::add_track), name));
	set_tooltip (_add_button, _("Add a track for this input port"));

	_name_button.set_corner_radius (2);
	_name_button.set_name ("generic button");
	_name_button.set_text_ellipsize (Pango::ELLIPSIZE_MIDDLE);
	_name_button.signal_clicked.connect (sigc::mem_fun (*this, &RecorderUI::InputPort::rename_port));

	_name_label.set_ellipsize (Pango::ELLIPSIZE_MIDDLE);

	setup_name ();

	ctrls->attach (_spill_button,     0, 2, 0, 1, EXPAND|FILL, EXPAND|FILL, 1, 1);
	if (dt == DataType::AUDIO) {
		ctrls->attach (_add_button,     0, 1, 1, 2, SHRINK|FILL, EXPAND|FILL, 1, 1);
		ctrls->attach (_monitor_button, 1, 2, 1, 2, SHRINK|FILL, EXPAND|FILL, 1, 1);
	} else {
		ctrls->attach (_add_button,     0, 2, 1, 2, EXPAND|FILL, EXPAND|FILL, 1, 1);
	}

	box_n->pack_start (_name_button, true, true);
#if 0 // MIXBUS ?
	box_n->pack_start (_name_label, true, true);
#endif

	int nh;
	if (vertical) {
		nh = 64 * UIConfiguration::instance ().get_ui_scale ();
		box_t->pack_start (_monitor, false, false);
		box_t->pack_start (*ctrls, false, false, 1);
		box_t->pack_start (*box_n, false, false, 1);
		_name_label.set_max_width_chars (9);
	} else {
		nh = 120 * UIConfiguration::instance ().get_ui_scale ();
		box_t->pack_start (*ctrls, false, false, 1);
		box_t->pack_start (*box_n, false, false, 1);
		box_t->pack_start (_monitor, false, false);
		_name_label.set_max_width_chars (18);
	}
	_name_button.set_layout_ellipsize_width (nh * PANGO_SCALE);

	if (!vertical) {
		/* match width of all name labels */
		_name_size_group->add_widget (*box_n);
		/* match width of control boxes */
		_ctrl_size_group->add_widget (*ctrls);
	}

	/* equal size for all meters + event monitors */
	_monitor_size_group->add_widget (_monitor);

	Gdk::Color bg;
	ARDOUR_UI_UTILS::set_color_from_rgba (bg, UIConfiguration::instance ().color ("neutral:background2"));
	_frame.modify_bg (Gtk::STATE_NORMAL, bg);

	/* top level packing with border */
	_alignment.add (*box_t);
	_alignment.set_padding (2, 2, 4, 4);

	_frame.add (_alignment);
	_frame.set_border_width (3);
	_frame.set_padding (3);

	add (_frame);
	show_all ();

	update_rec_stat ();
}

RecorderUI::InputPort::~InputPort ()
{
	delete _solo_release;
}

void
RecorderUI::InputPort::clear ()
{
	delete _solo_release;
	_solo_release = 0;
	_monitor.clear ();
}

void
RecorderUI::InputPort::update (float l, float p)
{
	_monitor.update (l, p);
}

void
RecorderUI::InputPort::update (CircularSampleBuffer& csb)
{
	_monitor.update (csb);
}

void
RecorderUI::InputPort::update (float const* v)
{
	_monitor.update (v);
}

void
RecorderUI::InputPort::update (CircularEventBuffer& ceb)
{
	_monitor.update (ceb);
}

void
RecorderUI::InputPort::set_frame_label (std::string const& lbl)
{
	_frame.set_label (lbl);
}

void
RecorderUI::InputPort::update_rec_stat ()
{
	bool armed = false;
	for (WeakRouteList::const_iterator r = _connected_routes.begin(); r != _connected_routes.end(); ++r) {
		boost::shared_ptr<Route> rt = r->lock ();
		if (!rt || !rt->rec_enable_control ()) {
			continue;
		}
		if (rt->rec_enable_control ()->get_value ()) {
			armed = true;
			break;
		}
	}
	if (armed) {
		_frame.set_edge_color (0xff0000ff); // red
	} else {
		_frame.set_edge_color (0x000000ff); // black
	}
}

void
RecorderUI::InputPort::set_connections (WeakRouteList wrl)
{
	_connected_routes = wrl;
	size_t cnt = wrl.size ();

	if (cnt > 0) {
		_spill_button.set_text (string_compose("(%1)", cnt));
		_spill_button.set_sensitive (true);
		set_tooltip (_spill_button, string_compose(_("This port feeds %1 tracks. Click to show them"), cnt));
	} else {
		_spill_button.set_text (_("(none)"));
		_spill_button.set_sensitive (false);
		set_tooltip (_spill_button, _("This port is not feeding any tracks"));
	}

	update_rec_stat ();
}

void
RecorderUI::InputPort::setup_name ()
{
	string pn = AudioEngine::instance()->get_pretty_name_by_name (_port_name);
	if (!pn.empty ()) {
		_name_button.set_text (pn);
		_name_label.set_text (_port_name);
	} else {
		_name_button.set_text (_port_name);
		_name_label.set_text ("");
	}
	set_tooltip (_name_button, string_compose (_("Set or edit the custom name for input port '%1'"), _port_name));
}

void
RecorderUI::InputPort::rename_port ()
{
	Prompter prompter (true, true);

	prompter.set_name ("Prompter");

	prompter.add_button (Stock::REMOVE, RESPONSE_NO);
	prompter.add_button (Stock::OK, RESPONSE_ACCEPT);

	prompter.set_title (_("Customize port name"));
	prompter.set_prompt (_("Port name"));
	prompter.set_initial_text (AudioEngine::instance()->get_pretty_name_by_name (_port_name));

	string name;
	switch (prompter.run ()) {
		case RESPONSE_ACCEPT:
			prompter.get_result (name);
			break;
		case RESPONSE_NO:
			/* use blank name, reset */
			break;
		default:
			return;
	}

	AudioEngine::instance()->set_port_pretty_name (_port_name, name);
}

bool
RecorderUI::InputPort::spill (bool en)
{
	bool active = _spill_button.get_active ();
	bool act = active;

	if (!en) {
		act = false;
	}

	if (_connected_routes.size () == 0) {
		act = false;
	}

	if (active != act) {
		_spill_button.set_active (act);
	}
	return act;
}

bool
RecorderUI::InputPort::spilled () const
{
	return _spill_button.get_active ();
}

void
RecorderUI::InputPort::allow_monitoring (bool en)
{
	if (_dt != DataType::AUDIO) {
		en = false;
	}
	if (!en && _monitor_button.get_active ()) {
		_monitor_button.set_active (false);
	}
	_monitor_button.set_sensitive (en);
}

void
RecorderUI::InputPort::update_monitorstate (bool en)
{
	if (_dt == DataType::AUDIO) {
		_monitor_button.set_active (en);
	}
}

bool
RecorderUI::InputPort::monitor_press (GdkEventButton* ev)
{
	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS ) {
		return true;
	}
	if (Keyboard::is_context_menu_event (ev)) {
		return false;
	}
	if (ev->button != 1 && !Keyboard::is_button2_event (ev)) {
		return false;
	}

	MonitorPort& mp (AudioEngine::instance()->monitor_port ());
	Session* s = AudioEngine::instance()->session ();
	assert (s);

	if (Keyboard::is_button2_event (ev)) {
		/* momentary */
		_solo_release = new SoloMuteRelease (mp.monitoring (_port_name));
	}

	if (Keyboard::modifier_state_equals (ev->state, Keyboard::ModifierMask (Keyboard::PrimaryModifier|Keyboard::TertiaryModifier))) {
		/* Primary-Tertiary-click applies change to all */
		if (_solo_release) {
			s->prepare_momentary_solo (_solo_release);
		}

		if (!_monitor_button.get_active ()) {
			std::vector<std::string> ports;
			AudioEngine::instance()->get_physical_inputs (DataType::AUDIO, ports);
			std::list<std::string> portlist;
			std::copy (ports.begin (), ports.end (), std::back_inserter (portlist));
			mp.set_active_monitors (portlist);
		} else {
			mp.clear_ports (false);
		}

	} else if (Keyboard::modifier_state_contains (ev->state, Keyboard::ModifierMask (Keyboard::PrimaryModifier|Keyboard::SecondaryModifier)) || (!_monitor_button.get_active () && Config->get_exclusive_solo ())) {
		/* Primary-Secondary-click: exclusive solo */
		if (_solo_release) {
			s->prepare_momentary_solo (_solo_release, true);
		} else {
			/* clear solo state */
			s->prepare_momentary_solo (0, true);
		}
		/* exclusively solo */
		if (!_monitor_button.get_active ()) {
			mp.add_port (_port_name);
		} else {
			delete _solo_release;
			_solo_release = 0;
		}
	} else {
		if (_solo_release) {
			s->prepare_momentary_solo (_solo_release);
		}

		/* Toggle Port Listen */
		if (!_monitor_button.get_active ()) {
			mp.add_port (_port_name);
		} else {
			mp.remove_port (_port_name);
		}
	}

	return false;
}

bool
RecorderUI::InputPort::monitor_release (GdkEventButton* ev)
{
	if (_solo_release) {
		_solo_release->release (AudioEngine::instance()->session (), false);
		delete _solo_release;
		_solo_release = 0;
	}
	return false;
}

string const&
RecorderUI::InputPort::name () const
{
	return _port_name;
}

DataType
RecorderUI::InputPort::data_type () const
{
	return _dt;
}

/* ****************************************************************************/

RecorderUI::RecRuler::RecRuler ()
	: _width (200)
	, _left (0)
	, _right (0)
{
	_layout = Pango::Layout::create (get_pango_context ());
	_layout->set_font_description (UIConfiguration::instance ().get_SmallMonospaceFont ());
	_layout->set_text ("88:88:88");
	_layout->get_pixel_size (_time_width, _time_height);
}

void
RecorderUI::RecRuler::set_right_edge (int w)
{
	if (_width == w) {
		return;
	}
	_width = w;
	set_dirty ();
}

void
RecorderUI::RecRuler::set_gui_extents (samplepos_t start, samplepos_t end)
{
	if (_left == start && _right == end) {
		return;
	}
	_left = start;
	_right = end;
	set_dirty ();
}

void
RecorderUI::RecRuler::render (Cairo::RefPtr<Cairo::Context> const& cr, cairo_rectangle_t* r)
{
  cr->rectangle (r->x, r->y, r->width, r->height);
  cr->clip ();

	if (!_session || _left >= _right) {
		return;
	}

	const int width  = std::min (_width, get_width ());
	const int height = get_height ();

	const int         n_labels         = floor (width / (_time_width * 1.75));
	const samplecnt_t time_span        = _right - _left;
	const samplecnt_t time_granularity = ceil ((double)time_span / n_labels / _session->sample_rate ()) * _session->sample_rate ();
	const double      px_per_sample    = width / (double) time_span;

	const samplepos_t lower = (_left / time_granularity) * time_granularity;

	Gtkmm2ext::set_source_rgba (cr, UIConfiguration::instance().color ("ruler text"));
	cr->set_line_width (1);

	for (int i = 0; i < 2 + n_labels; ++i) {
		samplepos_t when = lower + i * time_granularity;
		int xpos         = (when - _left) * px_per_sample;
		if (xpos < 0) {
			continue;
		}

		char buf[32];
		int lw, lh;
		AudioClock::print_minsec (when, buf, sizeof (buf), _session->sample_rate (), 0);
		_layout->set_text (string(buf).substr(1));
		_layout->get_pixel_size (lw, lh);

		if (xpos + lw > width) {
			break;
		}

		int x0 = xpos + 2;
		int y0 = height - _time_height - 3;

		cr->move_to (xpos + .5 , 0);
		cr->line_to (xpos + .5 , height);
		cr->stroke ();

		cr->move_to (x0, y0);
		_layout->show_in_cairo_context (cr);
	}
}

void
RecorderUI::RecRuler::on_size_request (Requisition* req)
{
	req->width = 200;
	req->height = _time_height + 4;
}

bool
RecorderUI::RecRuler::on_button_press_event (GdkEventButton* ev)
{
	if (!_session || _session->actively_recording()) {
		return false;
	}
	// TODO start "drag"  editor->_dragging_playhead = true
	// CursorDrag::start_grab
	// RecRuler internal drag (leave editor + TC transmission alone?!)

	_session->request_locate (_left + (double) (_right - _left) * ev->x / get_width ());
	return true;
}
