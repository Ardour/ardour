/*
 * Copyright (C) 2005-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2006-2007 Doug McLain <doug@nostar.net>
 * Copyright (C) 2006-2007 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2006 Sampo Savolainen <v2@iki.fi>
 * Copyright (C) 2007-2015 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2008-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2008 Hans Baier <hansfbaier@googlemail.com>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2012-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2014-2018 Ben Loftis <ben@harrisonconsoles.com>
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

#include "pbd/convert.h"
#include "pbd/unwind.h"

#include "ardour/lv2_plugin.h"
#include "ardour/rc_configuration.h"
#include "ardour/session.h"
#include "ardour/transport_master_manager.h"

#include "gtkmm2ext/utils.h"
#include "waveview/wave_view.h"

#include "ardour_message.h"
#include "audio_clock.h"
#include "ardour_ui.h"
#include "actions.h"
#include "gui_thread.h"
#include "public_editor.h"
#include "main_clock.h"

#include "pbd/i18n.h"

using namespace Gtk;
using namespace Gtkmm2ext;
using namespace ARDOUR;
using namespace PBD;
using namespace ArdourWidgets;

void
ARDOUR_UI::toggle_external_sync()
{
	if (_session) {
		if (_session->config.get_video_pullup() != 0.0f && (TransportMasterManager::instance().current()->type() == Engine)) {
			ArdourMessageDialog msg (_("It is not possible to use JACK as the sync source\n when the pull up/down setting is non-zero."));
			msg.run ();
			return;
		}

		ActionManager::toggle_config_state_foo ("Transport", "ToggleExternalSync", sigc::mem_fun (_session->config, &SessionConfiguration::set_external_sync), sigc::mem_fun (_session->config, &SessionConfiguration::get_external_sync));

		/* activating a slave is a session-property.
		 * The slave type is a RC property.
		 * When the slave is active is must not be reconfigured.
		 * This is a UI limitation, imposed by audio-clock and
		 * status displays which combine RC-config & session-properties.
		 *
		 * Notify RCOptionEditor by emitting a signal if the active
		 * status changed:
		 */
		Config->ParameterChanged("sync-source");
	}
}

void
ARDOUR_UI::toggle_time_master ()
{
	ActionManager::toggle_config_state_foo ("Transport", "ToggleTimeMaster", sigc::mem_fun (_session->config, &SessionConfiguration::set_jack_time_master), sigc::mem_fun (_session->config, &SessionConfiguration::get_jack_time_master));
}

void
ARDOUR_UI::toggle_send_mtc ()
{
	ActionManager::toggle_config_state ("Options", "SendMTC", &RCConfiguration::set_send_mtc, &RCConfiguration::get_send_mtc);
}

void
ARDOUR_UI::toggle_send_mmc ()
{
	ActionManager::toggle_config_state ("Options", "SendMMC", &RCConfiguration::set_send_mmc, &RCConfiguration::get_send_mmc);
}

void
ARDOUR_UI::toggle_send_midi_clock ()
{
	ActionManager::toggle_config_state ("Options", "SendMidiClock", &RCConfiguration::set_send_midi_clock, &RCConfiguration::get_send_midi_clock);
}

void
ARDOUR_UI::toggle_use_mmc ()
{
	ActionManager::toggle_config_state ("Options", "UseMMC", &RCConfiguration::set_mmc_control, &RCConfiguration::get_mmc_control);
}

void
ARDOUR_UI::toggle_auto_input ()
{
	ActionManager::toggle_config_state_foo ("Transport", "ToggleAutoInput", sigc::mem_fun (_session->config, &SessionConfiguration::set_auto_input), sigc::mem_fun (_session->config, &SessionConfiguration::get_auto_input));
}

void
ARDOUR_UI::toggle_auto_play ()
{
	ActionManager::toggle_config_state_foo ("Transport", "ToggleAutoPlay", sigc::mem_fun (_session->config, &SessionConfiguration::set_auto_play), sigc::mem_fun (_session->config, &SessionConfiguration::get_auto_play));
}

void
ARDOUR_UI::toggle_auto_return ()
{
	ActionManager::toggle_config_state_foo ("Transport", "ToggleAutoReturn", sigc::mem_fun (_session->config, &SessionConfiguration::set_auto_return), sigc::mem_fun (_session->config, &SessionConfiguration::get_auto_return));
}

void
ARDOUR_UI::toggle_click ()
{
	ActionManager::toggle_config_state ("Transport", "ToggleClick", &RCConfiguration::set_clicking, &RCConfiguration::get_clicking);
}

void
ARDOUR_UI::toggle_session_monitoring_in ()
{
	Glib::RefPtr<ToggleAction> tact = ActionManager::get_toggle_action (X_("Transport"), X_("SessionMonitorIn"));
	MonitorChoice mc = _session->config.get_session_monitoring ();

	if (tact->get_active() == (0 != (mc & MonitorInput))) {
		return;
	}

	if (tact->get_active()) {
		mc = MonitorChoice (mc | MonitorInput);
	} else {
		mc = MonitorChoice (mc & ~MonitorInput);
	}
	_session->config.set_session_monitoring (mc);
}

void
ARDOUR_UI::toggle_session_monitoring_disk ()
{
	Glib::RefPtr<ToggleAction> tact = ActionManager::get_toggle_action (X_("Transport"), X_("SessionMonitorDisk"));
	MonitorChoice mc = _session->config.get_session_monitoring ();
	if (tact->get_active() == (0 != (mc & MonitorDisk))) {
		return;
	}

	if (tact->get_active()) {
		mc = MonitorChoice (mc | MonitorDisk);
	} else {
		mc = MonitorChoice (mc & ~MonitorDisk);
	}
	_session->config.set_session_monitoring (mc);
}

void
ARDOUR_UI::unset_dual_punch ()
{
	Glib::RefPtr<ToggleAction> tact = ActionManager::get_toggle_action ("Transport", "TogglePunch");
	if (tact) {
		ignore_dual_punch = true;
		tact->set_active (false);
		ignore_dual_punch = false;
	}
}

void
ARDOUR_UI::toggle_punch ()
{
	if (ignore_dual_punch) {
		return;
	}

	Glib::RefPtr<ToggleAction> tact = ActionManager::get_toggle_action ("Transport", "TogglePunch");

	/* drive the other two actions from this one */
	Glib::RefPtr<ToggleAction> in_action = ActionManager::get_toggle_action ("Transport", "TogglePunchIn");
	Glib::RefPtr<ToggleAction> out_action = ActionManager::get_toggle_action ("Transport", "TogglePunchOut");

	in_action->set_active (tact->get_active());
	out_action->set_active (tact->get_active());
}

void
ARDOUR_UI::toggle_punch_in ()
{
	Glib::RefPtr<ToggleAction> tact = ActionManager::get_toggle_action (X_("Transport"), X_("TogglePunchIn"));

	if (tact->get_active() != _session->config.get_punch_in()) {
		_session->config.set_punch_in (tact->get_active ());
	}

	if (tact->get_active()) {
		/* if punch-in is turned on, make sure the loop/punch ruler is visible, and stop it being hidden,
		   to avoid confusing the user */
		show_loop_punch_ruler_and_disallow_hide ();
	}

	reenable_hide_loop_punch_ruler_if_appropriate ();
}

void
ARDOUR_UI::toggle_punch_out ()
{
	Glib::RefPtr<ToggleAction> tact = ActionManager::get_toggle_action (X_("Transport"), X_("TogglePunchOut"));

	if (tact->get_active() != _session->config.get_punch_out()) {
		_session->config.set_punch_out (tact->get_active ());
	}

	if (tact->get_active()) {
		/* if punch-out is turned on, make sure the loop/punch ruler is visible, and stop it being hidden,
		   to avoid confusing the user */
		show_loop_punch_ruler_and_disallow_hide ();
	}

	reenable_hide_loop_punch_ruler_if_appropriate ();
}

void
ARDOUR_UI::show_loop_punch_ruler_and_disallow_hide ()
{
	Glib::RefPtr<ToggleAction> tact = ActionManager::get_toggle_action (X_("Rulers"), "toggle-loop-punch-ruler");

	tact->set_sensitive (false);

	if (!tact->get_active()) {
		tact->set_active ();
	}
}

/* This is a bit of a silly name for a method */
void
ARDOUR_UI::reenable_hide_loop_punch_ruler_if_appropriate ()
{
	if (!_session->config.get_punch_in() && !_session->config.get_punch_out()) {
		/* if punch in/out are now both off, reallow hiding of the loop/punch ruler */
		Glib::RefPtr<Action> act = ActionManager::get_action (X_("Rulers"), "toggle-loop-punch-ruler");
		if (act) {
			act->set_sensitive (true);
		}
	}
}

void
ARDOUR_UI::toggle_video_sync()
{
	Glib::RefPtr<ToggleAction> tact = ActionManager::get_toggle_action ("Transport", "ToggleVideoSync");
	_session->config.set_use_video_sync (tact->get_active());
}

void
ARDOUR_UI::toggle_editing_space()
{
	Glib::RefPtr<ToggleAction> tact = ActionManager::get_toggle_action ("Common", "ToggleMaximalEditor");
	if (tact->get_active()) {
		maximise_editing_space ();
	} else {
		restore_editing_space ();
	}
}

void
ARDOUR_UI::toggle_latency_switch ()
{
	Glib::RefPtr<ToggleAction> tact = ActionManager::get_toggle_action ("Main", "ToggleLatencyCompensation");
	ARDOUR::Latent::force_zero_latency (tact->get_active());
}

void
ARDOUR_UI::setup_session_options ()
{
	_session->config.ParameterChanged.connect (_session_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::parameter_changed, this, _1), gui_context());
	boost::function<void (std::string)> pc (boost::bind (&ARDOUR_UI::parameter_changed, this, _1));
	_session->config.map_parameters (pc);
}

void
ARDOUR_UI::parameter_changed (std::string p)
{
	if (p == "external-sync") {

		/* session parameter */

		ActionManager::map_some_state ("Transport", "ToggleExternalSync", sigc::mem_fun (_session->config, &SessionConfiguration::get_external_sync));

		if (!_session->config.get_external_sync()) {
			sync_button.set_text (S_("SyncSource|Int."));
			ActionManager::get_action ("Transport", "ToggleAutoPlay")->set_sensitive (true);
			ActionManager::get_action ("Transport", "ToggleAutoReturn")->set_sensitive (true);
			ActionManager::get_action ("Transport", "ToggleFollowEdits")->set_sensitive (true);
		} else {
			/* XXX we need to make sure that auto-play is off as well as insensitive */
			ActionManager::get_action ("Transport", "ToggleAutoPlay")->set_sensitive (false);
			ActionManager::get_action ("Transport", "ToggleFollowEdits")->set_sensitive (false);
			if (!_session->synced_to_engine()) {
				/* JACK transport allows auto-return */
				ActionManager::get_action ("Transport", "ToggleAutoReturn")->set_sensitive (false);
			}
		}

	} else if (p == "sync-source") {

		/* app parameter (RC config) */

		if (_session) {
			if (!_session->config.get_external_sync()) {
				sync_button.set_text (S_("SyncSource|Int."));
			} else {
				sync_button.set_text (TransportMasterManager::instance().current()->display_name());
			}
		} else {
			/* changing sync source without a session is unlikely/impossible , except during startup */
			sync_button.set_text (TransportMasterManager::instance().current()->display_name());
		}

	} else if (p == "follow-edits") {

		ActionManager::map_some_state ("Transport", "ToggleFollowEdits", &UIConfiguration::get_follow_edits);

	} else if (p == "send-mtc") {

		ActionManager::map_some_state ("Options", "SendMTC", &RCConfiguration::get_send_mtc);

	} else if (p == "send-mmc") {

		ActionManager::map_some_state ("Options", "SendMMC", &RCConfiguration::get_send_mmc);

	} else if (p == "mmc-control") {
		ActionManager::map_some_state ("Options", "UseMMC", &RCConfiguration::get_mmc_control);
	} else if (p == "auto-play") {
		ActionManager::map_some_state ("Transport", "ToggleAutoPlay", sigc::mem_fun (_session->config, &SessionConfiguration::get_auto_play));
	} else if (p == "auto-return") {
		ActionManager::map_some_state ("Transport", "ToggleAutoReturn", sigc::mem_fun (_session->config, &SessionConfiguration::get_auto_return));
	} else if (p == "auto-input") {
		ActionManager::map_some_state ("Transport", "ToggleAutoInput", sigc::mem_fun (_session->config, &SessionConfiguration::get_auto_input));
	} else if (p == "session-monitoring") {
		Glib::RefPtr<ToggleAction> tiact = ActionManager::get_toggle_action (X_("Transport"), X_("SessionMonitorIn"));
		Glib::RefPtr<ToggleAction> tdact = ActionManager::get_toggle_action (X_("Transport"), X_("SessionMonitorDisk"));
		MonitorChoice mc = _session->config.get_session_monitoring ();
		tiact->set_active (0 != (mc & MonitorInput));
		tdact->set_active (0 != (mc & MonitorDisk));
	} else if (p == "punch-out") {
		ActionManager::map_some_state ("Transport", "TogglePunchOut", sigc::mem_fun (_session->config, &SessionConfiguration::get_punch_out));
		if (!_session->config.get_punch_out()) {
			unset_dual_punch ();
		}
	} else if (p == "punch-in") {
		ActionManager::map_some_state ("Transport", "TogglePunchIn", sigc::mem_fun (_session->config, &SessionConfiguration::get_punch_in));
		if (!_session->config.get_punch_in()) {
			unset_dual_punch ();
		}
	} else if (p == "clicking") {
		ActionManager::map_some_state ("Transport", "ToggleClick", &RCConfiguration::get_clicking);
	} else if (p == "use-video-sync") {
		ActionManager::map_some_state ("Transport",  "ToggleVideoSync", sigc::mem_fun (_session->config, &SessionConfiguration::get_use_video_sync));
	} else if (p == "sync-source") {

		synchronize_sync_source_and_video_pullup ();
		set_fps_timeout_connection ();

	} else if (p == "show-track-meters") {
		if (editor) editor->toggle_meter_updating();
	} else if (p == "primary-clock-delta-mode") {
		if (UIConfiguration::instance().get_primary_clock_delta_mode() != NoDelta) {
			primary_clock->set_is_duration (true, timepos_t());
			primary_clock->set_editable (false);
			primary_clock->set_widget_name ("transport delta");
		} else {
			primary_clock->set_is_duration (false, timepos_t());
			primary_clock->set_editable (true);
			primary_clock->set_widget_name ("transport");
		}
	} else if (p == "secondary-clock-delta-mode") {
		if (UIConfiguration::instance().get_secondary_clock_delta_mode() != NoDelta) {
			secondary_clock->set_is_duration (true, timepos_t());
			secondary_clock->set_editable (false);
			secondary_clock->set_widget_name ("secondary delta");
		} else {
			secondary_clock->set_is_duration (false, timepos_t());
			secondary_clock->set_editable (true);
			secondary_clock->set_widget_name ("secondary");
		}
	} else if (p == "super-rapid-clock-update") {
		if (_session) {
			stop_clocking ();
			start_clocking ();
		}
	} else if (p == "use-tooltips") {
		/* this doesn't really belong here but it has to go somewhere */
		if (UIConfiguration::instance().get_use_tooltips()) {
			Gtkmm2ext::enable_tooltips ();
		} else {
			Gtkmm2ext::disable_tooltips ();
		}
	} else if (p == "waveform-gradient-depth") {
		ArdourWaveView::WaveView::set_global_gradient_depth (UIConfiguration::instance().get_waveform_gradient_depth());
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
	} else if (p == "show-toolbar-monitor-info") {
		repack_transport_hbox ();
	} else if (p == "show-editor-meter") {
		repack_transport_hbox ();
	} else if (p == "show-secondary-clock") {
		update_clock_visibility ();
	} else if (p == "waveform-scale") {
		ArdourWaveView::WaveView::set_global_logscaled (UIConfiguration::instance().get_waveform_scale() == Logarithmic);
	} else if (p == "widget-prelight") {
		CairoWidget::set_widget_prelight (UIConfiguration::instance().get_widget_prelight());
	} else if (p == "waveform-shape") {
		ArdourWaveView::WaveView::set_global_shape (UIConfiguration::instance().get_waveform_shape() == Rectified
				? ArdourWaveView::WaveView::Rectified : ArdourWaveView::WaveView::Normal);
	} else if (p == "show-waveform-clipping") {
		ArdourWaveView::WaveView::set_global_show_waveform_clipping (UIConfiguration::instance().get_show_waveform_clipping());
	} else if (p == "waveform-cache-size") {
		/* GUI option has units of megabytes; image cache uses units of bytes */
		ArdourWaveView::WaveView::set_image_cache_size (UIConfiguration::instance().get_waveform_cache_size() * 1048576);
	} else if (p == "use-wm-visibility") {
		VisibilityTracker::set_use_window_manager_visibility (UIConfiguration::instance().get_use_wm_visibility());
	} else if (p == "action-table-columns") {
		const uint32_t cols = UIConfiguration::instance().get_action_table_columns ();
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
		}
	} else if (p == "layered-record-mode") {
		layered_button.set_active (_session->config.get_layered_record_mode ());
	} else if (p == "flat-buttons") {
		bool flat = UIConfiguration::instance().get_flat_buttons();
		if (ArdourButton::flat_buttons () != flat) {
			ArdourButton::set_flat_buttons (flat);
			/* force a redraw */
			gtk_rc_reset_styles (gtk_settings_get_default());
			LV2Plugin::set_global_ui_style_flat (flat);
		}
	} else if (p == "boxy-buttons") {
		bool boxy = UIConfiguration::instance().get_boxy_buttons();
		if (ArdourButton::boxy_buttons () != boxy) {
			ArdourButton::set_boxy_buttons (boxy);
			/* force a redraw */
			gtk_rc_reset_styles (gtk_settings_get_default());
			LV2Plugin::set_global_ui_style_boxy (boxy);
		}
	} else if ( (p == "snap-to-region-sync") || (p == "snap-to-region-start") || (p == "snap-to-region-end") ) {
		if (editor) editor->mark_region_boundary_cache_dirty();
	} else if (p == "screen-saver-mode") {
		switch (UIConfiguration::instance().get_screen_saver_mode ()) {
			using namespace ARDOUR_UI_UTILS;
			case InhibitWhileRecording:
				inhibit_screensaver (_session && _session->actively_recording ());
				break;
			case InhibitAlways:
				inhibit_screensaver (true);
				break;
			case InhibitNever:
				inhibit_screensaver (false);
				break;
		}
	}
}

void
ARDOUR_UI::session_parameter_changed (std::string p)
{
	if (p == "native-file-data-format" || p == "native-file-header-format") {
		update_format ();
	} else if (p == "timecode-format") {
		set_fps_timeout_connection ();
	} else if (p == "video-pullup" || p == "timecode-format") {
		set_fps_timeout_connection ();

		synchronize_sync_source_and_video_pullup ();
		reset_main_clocks ();
		editor->queue_visual_videotimeline_update();
	} else if (p == "track-name-number") {
		/* DisplaySuspender triggers _route->redisplay() when going out of scope
		 * which eventually calls reset_controls_layout_width() and re-sets the
		 * track-header width.
		 * see also RouteTimeAxisView::update_track_number_visibility()
		 */
		DisplaySuspender ds;
	}
}

void
ARDOUR_UI::reset_main_clocks ()
{
	ENSURE_GUI_THREAD (*this, &ARDOUR_UI::reset_main_clocks)

	if (_session) {
		primary_clock->set (timepos_t (_session->audible_sample()), true);
		secondary_clock->set (timepos_t (_session->audible_sample()), true);
	} else {
		primary_clock->set (timepos_t(), true);
		secondary_clock->set (timepos_t(), true);
	}
}

void
ARDOUR_UI::synchronize_sync_source_and_video_pullup ()
{
	Glib::RefPtr<Action> act = ActionManager::get_action (X_("Transport"), X_("ToggleExternalSync"));

	if (!act) {
		return;
	}

	if (!_session) {
		goto just_label;
	}

	if (_session->config.get_video_pullup() == 0.0f) {
		/* with no video pull up/down, any sync source is OK */
		act->set_sensitive (true);
	} else {
		/* can't sync to JACK if video pullup != 0.0 */
		if (TransportMasterManager::instance().current()->type() == Engine) {
			act->set_sensitive (false);
		} else {
			act->set_sensitive (true);
		}
	}

	/* XXX should really be able to set the video pull up
	   action to insensitive/sensitive, but there is no action.
	   FIXME
	*/

  just_label:
	if (act->get_sensitive ()) {
		set_tip (sync_button, _("Enable/Disable external positional sync"));
	} else {
		set_tip (sync_button, _("Sync to JACK is not possible: video pull up/down is set"));
	}

}
