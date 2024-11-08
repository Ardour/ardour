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

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#include "gtk2ardour-version.h"
#endif

#include <glib.h>
#include "pbd/gstdio_compat.h"

#include <gtkmm/accelmap.h>
#include <gtkmm/messagedialog.h>
#include <gtkmm/stock.h>
#include <gtkmm/uimanager.h>

#include "pbd/error.h"
#include "pbd/compose.h"
#include "pbd/convert.h"
#include "pbd/failed_constructor.h"
#include "pbd/memento_command.h"
#include "pbd/openuri.h"
#include "pbd/types_convert.h"
#include "pbd/file_utils.h"
#include <pbd/localtime_r.h>
#include "pbd/pthread_utils.h"
#include "pbd/replace_all.h"
#include "pbd/scoped_file_descriptor.h"
#include "pbd/xml++.h"

#include "gtkmm2ext/application.h"
#include "gtkmm2ext/bindings.h"
#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/menu_elems.h"
#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/window_title.h"

#include "widgets/tooltips.h"

#include "ardour/ardour.h"
#include "ardour/audioengine.h"
#include "ardour/profile.h"
#include "ardour/revision.h"
#include "ardour/transport_master.h"
#include "ardour/transport_master_manager.h"
#include "ardour/triggerbox.h"
#include "ardour/track.h"
#include "ardour/vca_manager.h"
#include "ardour/utils.h"

#include "control_protocol/basic_ui.h"

#include "actions.h"
#include "application_bar.h"
#include "ardour_ui.h"
#include "debug.h"
#include "gui_object.h"
#include "gui_thread.h"
#include "keyboard.h"
#include "keyeditor.h"
#include "luainstance.h"
#include "main_clock.h"
#include "meter_patterns.h"
#include "mixer_ui.h"
#include "public_editor.h"
#include "rc_option_editor.h"
#include "recorder_ui.h"
#include "session_dialog.h"
#include "session_option_editor.h"
#include "splash.h"
#include "time_info_box.h"
#include "timers.h"
#include "trigger_page.h"
#include "triggerbox_ui.h"
#include "utils.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;
using namespace PBD;
using namespace Gtkmm2ext;
using namespace ArdourWidgets;
using namespace Gtk;
using namespace std;
using namespace Gtk::Menu_Helpers;

static const gchar *_record_mode_strings_[] = {
	N_("Layered"),
	N_("Non-Layered"),
	N_("Snd on Snd"),
	0
};

#define PX_SCALE(px) std::max((float)px, rintf((float)px * UIConfiguration::instance().get_ui_scale()))

ApplicationBar::ApplicationBar ()
	: _have_layout (false)
	, _basic_ui (0)
	, _latency_disable_button (ArdourButton::led_default_elements)
	, _auto_return_button (ArdourButton::led_default_elements)
	, _follow_edits_button (ArdourButton::led_default_elements)
	, _secondary_clock_spacer (0)
{
	_record_mode_strings = I18N (_record_mode_strings_);
}

ApplicationBar::~ApplicationBar ()
{
}

void
ApplicationBar::on_parent_changed (Gtk::Widget*)
{
	assert (!_have_layout);
	_have_layout = true;

	_transport_ctrl.setup (ARDOUR_UI::instance ());
	_transport_ctrl.map_actions ();

	/* sync_button */
	Glib::RefPtr<Action> act = ActionManager::get_action (X_("Transport"), X_("ToggleExternalSync"));
	_sync_button.set_related_action (act);
	_sync_button.signal_button_press_event().connect (sigc::mem_fun (*this, &ApplicationBar::sync_button_clicked), false);
	_sync_button.set_sizing_text (S_("LogestSync|M-Clk"));

	/* sub-layout for Sync | Shuttle (grow) */
	HBox* ssbox = manage (new HBox);
	ssbox->set_spacing (PX_SCALE(2));
	ssbox->pack_start (_sync_button, false, false, 0);
	ssbox->pack_start (_shuttle_box, true, true, 0);
	ssbox->pack_start (*_shuttle_box.vari_button(), false, false, 0);
	ssbox->pack_start (*_shuttle_box.info_button(), false, false, 0);

	_punch_label.set_text (_("Punch:"));
	_layered_label.set_text (_("Rec:"));

	_punch_in_button.set_text (S_("Punch|In"));
	_punch_out_button.set_text (S_("Punch|Out"));

	_record_mode_selector.AddMenuElem (MenuElem (_record_mode_strings[(int)RecLayered], sigc::bind (sigc::mem_fun (*this, &ApplicationBar::set_record_mode), RecLayered)));
	_record_mode_selector.AddMenuElem (MenuElem (_record_mode_strings[(int)RecNonLayered], sigc::bind (sigc::mem_fun (*this, &ApplicationBar::set_record_mode), RecNonLayered)));
	_record_mode_selector.AddMenuElem (MenuElem (_record_mode_strings[(int)RecSoundOnSound], sigc::bind (sigc::mem_fun (*this, &ApplicationBar::set_record_mode), RecSoundOnSound)));
	_record_mode_selector.set_sizing_texts (_record_mode_strings);

	act = ActionManager::get_action ("Transport", "TogglePunchIn");
	_punch_in_button.set_related_action (act);
	act = ActionManager::get_action ("Transport", "TogglePunchOut");
	_punch_out_button.set_related_action (act);

	act = ActionManager::get_action ("Main", "ToggleLatencyCompensation");
	_latency_disable_button.set_related_action (act);

	_latency_disable_button.set_text (_("Disable PDC"));
	_io_latency_label.set_text (_("I/O Latency:"));

	set_size_request_to_display_given_text (_route_latency_value, "1000 spl", 0, 0);
	set_size_request_to_display_given_text (_io_latency_value, "888.88 ms", 0, 0);

	act = ActionManager::get_action ("Transport", "ToggleAutoReturn");
	_auto_return_button.set_related_action (act);
	act = ActionManager::get_action (X_("Transport"), X_("ToggleFollowEdits"));
	_follow_edits_button.set_related_action (act);

	_auto_return_button.set_text(_("Auto Return"));
	_follow_edits_button.set_text(_("Follow Range"));

	int vpadding = 1;
	int hpadding = 2;
	int col = 0;
#define TCOL col, col + 1

	_table.attach (_transport_ctrl, TCOL, 0, 1 , SHRINK, SHRINK, 0, 0);
	_table.attach (*ssbox,         TCOL, 1, 2 , FILL,   SHRINK, 0, 0);
	++col;

	_table.attach (*(manage (new ArdourVSpacer ())), TCOL, 0, 2 , SHRINK, EXPAND|FILL, 3, 0);
	++col;

	_table.attach (_punch_label, TCOL, 0, 1 , FILL, SHRINK, 3, 0);
	_table.attach (_layered_label, TCOL, 1, 2 , FILL, SHRINK, 3, 0);
	++col;

	_table.attach (_punch_in_button,      col,      col + 1, 0, 1 , FILL, SHRINK, hpadding, vpadding);
	_table.attach (_punch_space,          col + 1,  col + 2, 0, 1 , FILL, SHRINK, 0, vpadding);
	_table.attach (_punch_out_button,     col + 2,  col + 3, 0, 1 , FILL, SHRINK, hpadding, vpadding);
	_table.attach (_record_mode_selector, col,      col + 3, 1, 2 , FILL, SHRINK, hpadding, vpadding);
	col += 3;

	_table.attach (_recpunch_spacer, TCOL, 0, 2 , SHRINK, EXPAND|FILL, 3, 0);
	++col;

	_table.attach (_latency_disable_button, TCOL, 0, 1 , FILL, SHRINK, hpadding, vpadding);
	_table.attach (_io_latency_label, TCOL, 1, 2 , SHRINK, EXPAND|FILL, hpadding, 0);
	++col;
	_table.attach (_route_latency_value, TCOL, 0, 1 , SHRINK, EXPAND|FILL, hpadding, 0);
	_table.attach (_io_latency_value, TCOL, 1, 2 , SHRINK, EXPAND|FILL, hpadding, 0);
	++col;

	_route_latency_value.set_alignment (Gtk::ALIGN_END, Gtk::ALIGN_CENTER);
	_io_latency_value.set_alignment (Gtk::ALIGN_END, Gtk::ALIGN_CENTER);

	_table.attach (_latency_spacer, TCOL, 0, 2 , SHRINK, EXPAND|FILL, 3, 0);
	++col;

	_table.attach (_follow_edits_button, TCOL, 0, 1 , FILL, SHRINK, hpadding, vpadding);
	_table.attach (_auto_return_button,  TCOL, 1, 2 , FILL, SHRINK, hpadding, vpadding);
	++col;

	_table.attach (*(manage (new ArdourVSpacer ())), TCOL, 0, 2 , SHRINK, EXPAND|FILL, 3, 0);
	++col;

	_table.attach (*(ARDOUR_UI::instance()->primary_clock),              col,     col + 2, 0, 1 , FILL, SHRINK, hpadding, 0);
	_table.attach (*(ARDOUR_UI::instance()->primary_clock)->left_btn(),  col,     col + 1, 1, 2 , FILL, SHRINK, hpadding, 0);
	_table.attach (*(ARDOUR_UI::instance()->primary_clock)->right_btn(), col + 1, col + 2, 1, 2 , FILL, SHRINK, hpadding, 0);
	col += 2;

	_table.attach (*(manage (new ArdourVSpacer ())), TCOL, 0, 2 , SHRINK, EXPAND|FILL, 3, 0);
	++col;

	if (!ARDOUR::Profile->get_small_screen()) {
		_table.attach (*(ARDOUR_UI::instance()->secondary_clock),              col,     col + 2, 0, 1 , FILL, SHRINK, hpadding, 0);
		_table.attach (*(ARDOUR_UI::instance()->secondary_clock)->left_btn(),  col,     col + 1, 1, 2 , FILL, SHRINK, hpadding, 0);
		_table.attach (*(ARDOUR_UI::instance()->secondary_clock)->right_btn(), col + 1, col + 2, 1, 2 , FILL, SHRINK, hpadding, 0);
		(ARDOUR_UI::instance()->secondary_clock)->set_no_show_all (true);
		(ARDOUR_UI::instance()->secondary_clock)->left_btn()->set_no_show_all (true);
		(ARDOUR_UI::instance()->secondary_clock)->right_btn()->set_no_show_all (true);
		col += 2;

		_secondary_clock_spacer = manage (new ArdourVSpacer ());
		_table.attach (*_secondary_clock_spacer, TCOL, 0, 2 , SHRINK, EXPAND|FILL, 3, 0);
		++col;
	}

	_table.set_spacings (0);
	_table.set_row_spacings (4);
	_table.set_border_width (1);

	_table.show_all (); // TODO: update visibility somewhere else
	pack_start (_table, false, false);

	/*sizing */
	Glib::RefPtr<SizeGroup> button_height_size_group = ARDOUR_UI::instance()->button_height_size_group;
	button_height_size_group->add_widget (_transport_ctrl.size_button ());
	button_height_size_group->add_widget (_sync_button);
	button_height_size_group->add_widget (_punch_in_button);
	button_height_size_group->add_widget (_punch_out_button);
	button_height_size_group->add_widget (_record_mode_selector);
	button_height_size_group->add_widget (_latency_disable_button);
	button_height_size_group->add_widget (_follow_edits_button);
	button_height_size_group->add_widget (_auto_return_button);

	Glib::RefPtr<SizeGroup> punch_button_size_group = SizeGroup::create (Gtk::SIZE_GROUP_HORIZONTAL);
	punch_button_size_group->add_widget (_punch_in_button);
	punch_button_size_group->add_widget (_punch_out_button);

	/* tooltips */
	Gtkmm2ext::UI::instance()->set_tip (_punch_in_button, _("Start recording at auto-punch start"));
	Gtkmm2ext::UI::instance()->set_tip (_punch_out_button, _("Stop recording at auto-punch end"));
	Gtkmm2ext::UI::instance()->set_tip (_record_mode_selector, _("<b>Layered</b>, new recordings will be added as regions on a layer atop existing regions.\n<b>SoundOnSound</b>, behaves like <i>Layered</i>, except underlying regions will be audible.\n<b>Non Layered</b>, the underlying region will be spliced and replaced with the newly recorded region."));
	Gtkmm2ext::UI::instance()->set_tip (_latency_disable_button, _("Disable all Plugin Delay Compensation. This results in the shortest delay from live input to output, but any paths with delay-causing plugins will sound later than those without."));
	Gtkmm2ext::UI::instance()->set_tip (_auto_return_button, _("Return to last playback start when stopped"));
	Gtkmm2ext::UI::instance()->set_tip (_follow_edits_button, _("Playhead follows Range tool clicks, and Range selections"));

	/* theming */
	_sync_button.set_name ("transport active option button");
	_punch_in_button.set_name ("punch button");
	_punch_out_button.set_name ("punch button");
	_record_mode_selector.set_name ("record mode button");
	_latency_disable_button.set_name ("latency button");
	_auto_return_button.set_name ("transport option button");
	_follow_edits_button.set_name ("transport option button");

	/* indicate global latency compensation en/disable */
	ARDOUR::Latent::DisableSwitchChanged.connect (_forever_connections, MISSING_INVALIDATOR, std::bind (&ApplicationBar::latency_switch_changed, this), gui_context ());

	/* initialize */
	update_clock_visibility ();
	set_transport_sensitivity (false);
	latency_switch_changed ();
	session_latency_updated (true);

	if (_session) {
		repack_transport_hbox ();
	}
}
#undef PX_SCALE
#undef TCOL

void
ApplicationBar::repack_transport_hbox ()
{
	if (!_have_layout) {
		return;
	}

/*	if (time_info_box) {
		if (time_info_box->get_parent()) {
			transport_hbox.remove (*time_info_box);
		}
		if (UIConfiguration::instance().get_show_toolbar_selclock ()) {
			transport_hbox.pack_start (*time_info_box, false, false);
			time_info_box->show();
		}
	}

	if (mini_timeline.get_parent()) {
		transport_hbox.remove (mini_timeline);
	}
	if (UIConfiguration::instance().get_show_mini_timeline ()) {
		transport_hbox.pack_start (mini_timeline, true, true);
		mini_timeline.show();
	}

	if (editor_meter) {
		if (editor_meter_table.get_parent()) {
			transport_hbox.remove (editor_meter_table);
		}
		if (meterbox_spacer.get_parent()) {
			transport_hbox.remove (meterbox_spacer);
			transport_hbox.remove (meterbox_spacer2);
		}

		if (UIConfiguration::instance().get_show_editor_meter()) {
			transport_hbox.pack_end (meterbox_spacer, false, false, 3);
			transport_hbox.pack_end (editor_meter_table, false, false);
			transport_hbox.pack_end (meterbox_spacer2, false, false, 1);
			meterbox_spacer2.set_size_request (1, -1);
			editor_meter_table.show();
			meterbox_spacer.show();
			meterbox_spacer2.show();
		}
	}
*/
	bool show_rec = UIConfiguration::instance().get_show_toolbar_recpunch ();
	if (show_rec) {
		_punch_label.show ();
		_layered_label.show ();
		_punch_in_button.show ();
		_punch_out_button.show ();
		_record_mode_selector.show ();
//	_recpunch_spacer.show ();
	} else {
		_punch_label.hide ();
		_layered_label.hide ();
		_punch_in_button.hide ();
		_punch_out_button.hide ();
		_record_mode_selector.hide ();
//		_recpunch_spacer.hide ();
	}

/*
	bool show_pdc = UIConfiguration::instance().get_show_toolbar_latency ();
	if (show_pdc) {
		latency_disable_button.show ();
		_route_latency_value.show ();
		_io_latency_label.show ();
		_io_latency_value.show ();
		_latency_spacer.show ();
	} else {
		latency_disable_button.hide ();
		_route_latency_value.hide ();
		_io_latency_label.hide ();
		_io_latency_value.hide ();
		_latency_spacer.hide ();
	}

	bool show_cue = UIConfiguration::instance().get_show_toolbar_cuectrl ();
	if (show_cue) {
		_cue_rec_enable.show ();
		_cue_play_enable.show ();
		cuectrl_spacer.show ();
	} else {
		_cue_rec_enable.hide ();
		_cue_play_enable.hide ();
		cuectrl_spacer.hide ();
	}

	bool show_mnfo = UIConfiguration::instance().get_show_toolbar_monitor_info ();
	if (show_mnfo) {
		monitor_dim_button.show ();
		monitor_mono_button.show ();
		monitor_mute_button.show ();
		monitor_spacer.show ();
	} else {
		monitor_dim_button.hide ();
		monitor_mono_button.hide ();
		monitor_mute_button.hide ();
		monitor_spacer.hide ();
	}
*/
}

void
ApplicationBar::set_session (Session *s)
{
	SessionHandlePtr::set_session (s);

	_transport_ctrl.set_session (s);
	_shuttle_box.set_session (s);

	if (_basic_ui) {
		delete _basic_ui;
	}

	map_transport_state ();

	if (!_session) {
		_blink_connection.disconnect ();

		return;
	}

	_basic_ui = new BasicUI (*s);

	_session->AuditionActive.connect (_session_connections, MISSING_INVALIDATOR, std::bind (&ApplicationBar::auditioning_changed, this, _1), gui_context());
	_session->TransportStateChange.connect (_session_connections, MISSING_INVALIDATOR, std::bind (&ApplicationBar::map_transport_state, this), gui_context());
	_session->config.ParameterChanged.connect (_session_connections, MISSING_INVALIDATOR, std::bind (&ApplicationBar::parameter_changed, this, _1), gui_context());
	_session->LatencyUpdated.connect (_session_connections, MISSING_INVALIDATOR, std::bind (&ApplicationBar::session_latency_updated, this, _1), gui_context());

	//initialize all session config settings
	std::function<void (std::string)> pc (std::bind (&ApplicationBar::parameter_changed, this, _1));
	_session->config.map_parameters (pc);

	/* initialize */
	session_latency_updated (true);

	_blink_connection = Timers::blink_connect (sigc::mem_fun(*this, &ApplicationBar::blink_handler));
}

void
ApplicationBar::set_transport_sensitivity (bool yn)
{
	ActionManager::set_sensitive (ActionManager::transport_sensitive_actions, yn);
	_shuttle_box.set_sensitive (yn);
}

void
ApplicationBar::latency_switch_changed ()
{
	bool pdc_off = ARDOUR::Latent::zero_latency ();
	if (_latency_disable_button.get_active() != pdc_off) {
		_latency_disable_button.set_active (pdc_off);
	}
}

void
ApplicationBar::update_clock_visibility ()
{
	if (ARDOUR::Profile->get_small_screen()) {
		_secondary_clock_spacer->hide();
		return;
	}
	if (UIConfiguration::instance().get_show_secondary_clock ()) {
		if (_secondary_clock_spacer) {
			_secondary_clock_spacer->show();
		}
	} else {
		if (_secondary_clock_spacer) {
			_secondary_clock_spacer->hide();
		}
	}
}

void
ApplicationBar::session_latency_updated (bool for_playback)
{
	if (!for_playback) {
		/* latency updates happen in pairs, in the following order:
		 *  - for capture
		 *  - for playback
		 */
		return;
	}

	if (!_session) {
		_route_latency_value.set_text ("--");
		_io_latency_value.set_text ("--");
	} else {
		samplecnt_t wrl = _session->worst_route_latency ();
		samplecnt_t iol = _session->io_latency ();
		float rate      = _session->nominal_sample_rate ();

		_route_latency_value.set_text (samples_as_time_string (wrl, rate));

		if (_session->engine().check_for_ambiguous_latency (true)) {
//			_ambiguous_latency = true;
			_io_latency_value.set_markup ("<span background=\"red\" foreground=\"white\">ambiguous</span>");
		} else {
//			_ambiguous_latency = false;
			_io_latency_value.set_text (samples_as_time_string (iol, rate));
		}
	}
}


void
ApplicationBar::_auditioning_changed (bool onoff)
{
//	auditioning_alert_button.set_active (onoff);
//	auditioning_alert_button.set_sensitive (onoff);
//	if (!onoff) {
//		auditioning_alert_button.set_visual_state (Gtkmm2ext::NoVisualState);
//	}
	set_transport_sensitivity (!onoff);
}

void
ApplicationBar::auditioning_changed (bool onoff)
{
	UI::instance()->call_slot (MISSING_INVALIDATOR, std::bind (&ApplicationBar::_auditioning_changed, this, onoff));
}


void
ApplicationBar::parameter_changed (std::string p)
{
	if (p == "external-sync") {
		if (!_session->config.get_external_sync()) {
			_sync_button.set_text (S_("SyncSource|Int."));
		} else {
		}
	} else if (p == "sync-source") {
		if (_session) {
			if (!_session->config.get_external_sync()) {
				_sync_button.set_text (S_("SyncSource|Int."));
			} else {
				_sync_button.set_text (TransportMasterManager::instance().current()->display_name());
			}
		} else {
			/* changing sync source without a session is unlikely/impossible , except during startup */
			_sync_button.set_text (TransportMasterManager::instance().current()->display_name());
		}
		if (_session->config.get_video_pullup() == 0.0f || TransportMasterManager::instance().current()->type() != Engine) {
			UI::instance()->set_tip (_sync_button, _("Enable/Disable external positional sync"));
		} else {
			UI::instance()->set_tip (_sync_button, _("External sync is not possible: video pull up/down is set"));
		}
	} else if (p == "primary-clock-delta-mode") {
//		primary_clock->set_display_delta_mode(UIConfiguration::instance().get_primary_clock_delta_mode());
	} else if (p == "secondary-clock-delta-mode") {
//		secondary_clock->set_display_delta_mode(UIConfiguration::instance().get_secondary_clock_delta_mode());
	} else if (p == "show-mini-timeline") {
		repack_transport_hbox ();
	} else if (p == "show-dsp-load-info") {
		repack_transport_hbox ();
	} else if (p == "show-disk-space-info") {
		repack_transport_hbox ();
	} else if (p == "show-toolbar-recpunch") {
		repack_transport_hbox ();
	} else if (p == "show-toolbar-monitoring") {
		repack_transport_hbox ();
	} else if (p == "show-toolbar-selclock") {
		repack_transport_hbox ();
	} else if (p == "show-toolbar-latency") {
		repack_transport_hbox ();
	} else if (p == "show-toolbar-cuectrl") {
		repack_transport_hbox ();
	} else if (p == "show-toolbar-monitor-info") {
		repack_transport_hbox ();
	} else if (p == "show-editor-meter") {
		repack_transport_hbox ();
	} else if (p == "show-secondary-clock") {
		update_clock_visibility ();
	} else if (p == "action-table-columns") {
/*		const uint32_t cols = UIConfiguration::instance().get_action_table_columns ();
		for (int i = 0; i < MAX_LUA_ACTION_BUTTONS; ++i) {
			const int col = i / 2;
			if (cols & (1<<col)) {
				action_script_call_btn[i].show();
			} else {
				action_script_call_btn[i].hide();
			}
		}
		if (cols == 0) {
			scripts_spacer.hide ();
		} else {
			scripts_spacer.show ();
		} */
	} else if (p == "cue-behavior") {
		CueBehavior cb (_session->config.get_cue_behavior());
//		_cue_play_enable.set_active (cb & ARDOUR::FollowCues);
	} else if (p == "record-mode") {
		size_t m = _session->config.get_record_mode ();
		assert (m < _record_mode_strings.size ());
		_record_mode_selector.set_active (_record_mode_strings[m]);
	} else if (p == "no-strobe") {
//		stop_clocking ();
//		start_clocking ();
	}
}


bool
ApplicationBar::sync_button_clicked (GdkEventButton* ev)
{
	if (ev->button != 3) {
		/* this handler is just for button-3 clicks */
		return false;
	}

	Glib::RefPtr<ToggleAction> tact = ActionManager::get_toggle_action ("Window", "toggle-transport-masters");
	tact->set_active();
	return true;
}

void
ApplicationBar::set_record_mode (RecordMode m)
{
	if (_session) {
		_session->config.set_record_mode (m);
	}
}

void
ApplicationBar::sync_blink (bool onoff)
{
	if (_session == 0 || !_session->config.get_external_sync()) {
		/* internal sync */
		_sync_button.set_active (false);
		return;
	}

	if (!_session->transport_locked()) {
		/* not locked, so blink on and off according to the onoff argument */

		if (onoff) {
			_sync_button.set_active (true);
		} else {
			_sync_button.set_active (false);
		}
	} else {
		/* locked */
		_sync_button.set_active (true);
	}
}

void
ApplicationBar::blink_handler (bool blink_on)
{
	sync_blink (blink_on);

#if 0
	if (UIConfiguration::instance().get_no_strobe() || !UIConfiguration::instance().get_blink_alert_indicators()) {
		blink_on = true;
	}
	error_blink (blink_on);
	solo_blink (blink_on);
	audition_blink (blink_on);
	feedback_blink (blink_on);
#endif
}

void
ApplicationBar::map_transport_state ()
{
	_shuttle_box.map_transport_state ();

	if (!_session) {
		_record_mode_selector.set_sensitive (false);
		return;
	}

	float sp = _session->transport_speed();

	if (sp != 0.0f) {
		_record_mode_selector.set_sensitive (!_session->actively_recording ());
	} else {
		_record_mode_selector.set_sensitive (true);
//		update_disk_space ();
	}

}
