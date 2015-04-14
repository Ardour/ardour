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

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#endif

#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <cerrno>
#include <iostream>
#include <cmath>

#include <sigc++/bind.h>
#include "pbd/error.h"
#include "pbd/basename.h"
#include "pbd/fastlog.h"
#include <gtkmm2ext/cairocell.h>
#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/click_box.h>
#include <gtkmm2ext/tearoff.h>

#include "ardour/profile.h"
#include "ardour/session.h"
#include "ardour/types.h"

#include "ardour_ui.h"
#include "keyboard.h"
#include "public_editor.h"
#include "audio_clock.h"
#include "actions.h"
#include "main_clock.h"
#include "utils.h"
#include "theme_manager.h"
#include "midi_tracer.h"
#include "shuttle_control.h"
#include "global_port_matrix.h"
#include "location_ui.h"
#include "rc_option_editor.h"
#include "time_info_box.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtkmm2ext;
using namespace Gtk;
using namespace Glib;
using namespace ARDOUR_UI_UTILS;

int
ARDOUR_UI::setup_windows ()
{
	if (create_editor ()) {
		error << _("UI: cannot setup editor") << endmsg;
		return -1;
	}

	/* all other dialogs are created conditionally */

	we_have_dependents ();

	if (ARDOUR::Profile->get_trx()) {
		top_packer.pack_start (tracks_tools_packer, false, false);
	} else {
		top_packer.pack_start (transport_frame, false, false);
	}

	editor->add_toplevel_controls (top_packer);

	setup_transport();

	build_menu_bar ();
    
    //
    // Set initial sensitivity of the actions
    //
    ActionManager::set_sensitive (ActionManager::session_sensitive_actions, false);
    
	setup_tooltips ();

	return 0;
}

void
ARDOUR_UI::setup_tooltips ()
{
	set_tip (midi_panic_button, _("MIDI Panic\nSend note off and reset controller messages on all MIDI channels"));
	set_tip (follow_edits_button, _("Playhead follows Range Selections and Edits"));
	set_tip (auto_input_button, _("Be sensible about input monitoring"));
	set_tip (click_button, _("Enable/Disable audio click"));
	set_tip (solo_alert_button, _("When active, something is soloed.\nClick to de-solo everything"));
	set_tip (auditioning_alert_button, _("When active, auditioning is taking place\nClick to stop the audition"));
	set_tip (feedback_alert_button, _("When active, there is a feedback loop."));
	set_tip (editor_meter_peak_display, _("Reset Level Meter"));

	synchronize_sync_source_and_video_pullup ();

	editor->setup_tooltips ();
}

XMLNode*
ARDOUR_UI::tearoff_settings (const char* name) const
{
	XMLNode* ui_node = Config->extra_xml(X_("UI"));

	if (ui_node) {
		XMLNode* tearoff_node = ui_node->child (X_("Tearoffs"));
		if (tearoff_node) {
			XMLNode* mnode = tearoff_node->child (name);
			return mnode;
		}
	}

	return 0;
}

void
ARDOUR_UI::setup_transport ()
{
	RefPtr<Action> act;

	editor->get_waves_button ("transport_play_button").set_controllable (roll_controllable);
	editor->get_waves_button ("transport_stop_button").set_controllable (stop_controllable);
	editor->get_waves_button ("transport_start_button").set_controllable (goto_start_controllable);
	editor->get_waves_button ("transport_end_button").set_controllable (goto_end_controllable);
	editor->get_waves_button ("transport_loop_button").set_controllable (auto_loop_controllable);
	editor->get_waves_button ("transport_record_button").set_controllable (rec_controllable);
	
	act = ActionManager::get_action (X_("Main"), X_("toggle-session-lock-dialog"));
	editor->get_waves_button ("lock_session_button").set_related_action (act);
    
	act = ActionManager::get_action (X_("Main"), X_("ToggleMultiOutMode"));
        editor->get_waves_button ("mode_multi_out_button").set_related_action (act);
        
	act = ActionManager::get_action (X_("Main"), X_("ToggleStereoOutMode"));
        editor->get_waves_button ("mode_stereo_out_button").set_related_action (act);
        
	act = ActionManager::get_action (X_("Common"), X_("toggle-mixer"));
	editor->get_waves_button ("mixer_on_button").set_related_action (act);
        
	act = ActionManager::get_action (X_("Editor"), X_("show-editor-mixer"));
	editor->get_waves_button ("inspector_on_button").set_related_action (act);
        
	act = ActionManager::get_action (X_("Common"), X_("toggle-meterbridge"));	
	editor->get_waves_button ("meter_bridge_on_button").set_related_action (act);

	act = ActionManager::get_action (X_("Common"), X_("OpenMediaFolder"));	
	editor->get_waves_button ("media_button").set_related_action (act);

	update_output_operation_mode_buttons();
    
	transport_base.set_name ("TransportBase");
	transport_base.add (transport_hbox);

	transport_frame.set_shadow_type (SHADOW_OUT);
	transport_frame.set_name ("BaseFrame");
	transport_frame.add (transport_base);

	/* XXX WAVES DEMO CODE 
           build auto-return dropdown 
        */

	auto_return_dropdown.set_text (_("Auto Return"));

	auto_return_last_locate = manage (new Gtk::CheckMenuItem (_("Play from last roll")));
	auto_return_last_locate->signal_toggled().connect (sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::toggle_auto_return_state), LastLocate));
	auto_return_last_locate->show ();
	auto_return_dropdown.AddMenuElem (Gtk::Menu_Helpers::CheckMenuElem (*auto_return_last_locate));

	auto_return_region_selection = manage (new Gtk::CheckMenuItem (_("Play from region selection")));
	auto_return_region_selection->signal_toggled().connect (sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::toggle_auto_return_state), RegionSelectionStart));
	auto_return_region_selection->show ();
	auto_return_dropdown.AddMenuElem (Gtk::Menu_Helpers::CheckMenuElem (*auto_return_region_selection));

	auto_return_range_selection = manage (new Gtk::CheckMenuItem (_("Play from range selection")));
	auto_return_range_selection->signal_toggled().connect (sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::toggle_auto_return_state), RangeSelectionStart));
	auto_return_range_selection->show ();
	auto_return_dropdown.AddMenuElem (Gtk::Menu_Helpers::CheckMenuElem (*auto_return_range_selection));

	auto_return_loop = manage (new Gtk::CheckMenuItem (_("Play from loop")));
	auto_return_loop->signal_toggled().connect (sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::toggle_auto_return_state), Loop));
	auto_return_loop->show ();
	auto_return_dropdown.AddMenuElem (Gtk::Menu_Helpers::CheckMenuElem (*auto_return_loop));

	auto_return_dropdown.AddMenuElem (Gtk::Menu_Helpers::MenuElem (_("Disable/Enable All Options"), sigc::mem_fun (*this, &ARDOUR_UI::toggle_all_auto_return)));

	follow_edits_button.set_text(_("Follow Edits"));

	click_button.set_image (get_icon (X_("metronome")));
	act = ActionManager::get_action ("Transport", "ToggleClick");
	click_button.set_related_action (act);
	click_button.signal_button_press_event().connect (sigc::mem_fun (*this, &ARDOUR_UI::click_button_clicked), false);

	follow_edits_button.set_name ("transport option button");
	auto_input_button.set_name ("transport option button");

	/* these have to provide a clear indication of active state */

	click_button.set_name ("transport button");

	editor->get_waves_button ("transport_stop_button").set_active (true);

	midi_panic_button.set_image (get_icon (X_("midi_panic")));
	/* the icon for this has an odd aspect ratio, so fatten up the button */
	midi_panic_button.set_size_request (25, -1);
	
	act = ActionManager::get_action (X_("Transport"), X_("Stop"));
	editor->get_waves_button ("transport_stop_button").set_related_action (act);

	act = ActionManager::get_action (X_("Transport"), X_("Roll"));
	editor->get_waves_button ("transport_play_button").set_related_action (act);


	act = ActionManager::get_action (X_("Transport"), X_("Record"));
	editor->get_waves_button ("transport_record_button").set_related_action (act);

	act = ActionManager::get_action (X_("Transport"), X_("GotoStart"));
	editor->get_waves_button ("transport_start_button").set_related_action (act);

	act = ActionManager::get_action (X_("Transport"), X_("GotoEnd"));
	editor->get_waves_button ("transport_end_button").set_related_action (act);

	act = ActionManager::get_action (X_("Transport"), X_("Loop"));
	editor->get_waves_button ("transport_loop_button").set_related_action (act);
    
    act = ActionManager::get_action (X_("Transport"), X_("PlaySelection"));
	
	act = ActionManager::get_action (X_("MIDI"), X_("panic"));
	midi_panic_button.set_related_action (act);	
	act = ActionManager::get_action (X_("Transport"), X_("ToggleExternalSync"));

	/* clocks, etc. */

	ARDOUR_UI::Clock.connect (sigc::mem_fun (primary_clock, &AudioClock::set));
	ARDOUR_UI::Clock.connect (sigc::mem_fun (secondary_clock, &AudioClock::set));

	primary_clock->ValueChanged.connect (sigc::mem_fun(*this, &ARDOUR_UI::primary_clock_value_changed));
    primary_clock->mode_changed.connect (mem_fun(*this, &ARDOUR_UI::on_primary_clock_mode_changed));
	secondary_clock->ValueChanged.connect (sigc::mem_fun(*this, &ARDOUR_UI::secondary_clock_value_changed));
	big_clock->ValueChanged.connect (sigc::mem_fun(*this, &ARDOUR_UI::big_clock_value_changed));

	act = ActionManager::get_action (X_("Transport"), X_("ToggleFollowEdits"));
	follow_edits_button.set_related_action (act);
	act = ActionManager::get_action ("Transport", "ToggleAutoInput");
	auto_input_button.set_related_action (act);

	/* alerts */

	/* CANNOT sigc::bind these to clicked or toggled, must use pressed or released */

	solo_alert_button.set_name ("rude solo");
	solo_alert_button.signal_button_press_event().connect (sigc::mem_fun(*this,&ARDOUR_UI::solo_alert_press), false);
	auditioning_alert_button.set_name ("rude audition");
	auditioning_alert_button.signal_button_press_event().connect (sigc::mem_fun(*this,&ARDOUR_UI::audition_alert_press), false);
	feedback_alert_button.set_name ("feedback alert");
	feedback_alert_button.signal_button_press_event().connect (sigc::mem_fun (*this, &ARDOUR_UI::feedback_alert_press), false);

	alert_box.pack_start (solo_alert_button, true, false);
	alert_box.pack_start (auditioning_alert_button, true, false);
	alert_box.pack_start (feedback_alert_button, true, false);

	/* all transport buttons should be the same size vertically and
	 * horizontally 
	 */

	primary_clock->set_draw_background (false);
	primary_clock->set_visible_window (false);
	editor->get_box ("primary_clock_home").pack_start (*primary_clock, false, false);

	shuttle_box = manage (new ShuttleControl);
	shuttle_box->show ();
	
	time_info_box = manage (new TimeInfoBox);
    time_info_box->mode_changed.connect (mem_fun(*this, &ARDOUR_UI::on_time_info_box_mode_changed));
    
	editor->get_box ("time_info_box_home").pack_start (*time_info_box, false, false);


	/* desensitize */
	set_transport_sensitivity (false);
}

void
ARDOUR_UI::soloing_changed (bool onoff)
{
	if (solo_alert_button.get_active() != onoff) {
		solo_alert_button.set_active (onoff);
	}
}

void
ARDOUR_UI::_auditioning_changed (bool onoff)
{
	auditioning_alert_button.set_active (onoff);
	set_transport_sensitivity (!onoff);
}

void
ARDOUR_UI::auditioning_changed (bool onoff)
{
	UI::instance()->call_slot (MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::_auditioning_changed, this, onoff));
}

bool
ARDOUR_UI::audition_alert_press (GdkEventButton*)
{
	if (_session) {
		_session->cancel_audition();
	}
	return true;
}

bool
ARDOUR_UI::solo_alert_press (GdkEventButton*)
{
	if (_session) {
		if (_session->soloing()) {
			_session->set_solo (_session->get_routes(), false);
		} else if (_session->listening()) {
			_session->set_listen (_session->get_routes(), false);
		}
	}
	return true;
}

bool
ARDOUR_UI::feedback_alert_press (GdkEventButton *)
{
	return true;
}

void
ARDOUR_UI::solo_blink (bool onoff)
{
	if (_session == 0) {
		return;
	}

	if (_session->soloing() || _session->listening()) {
		if (onoff) {
			solo_alert_button.set_active (true);
		} else {
			solo_alert_button.set_active (false);
		}
	} else {
		solo_alert_button.set_active (false);
	}
}

void
ARDOUR_UI::sync_blink (bool onoff)
{
	if (_session == 0 || !_session->config.get_external_sync()) {
		/* internal sync */
		// NO NEED TO HAVE IT: sync_button.set_active (false);
		return;
	}

	if (!_session->transport_locked()) {
		/* not locked, so blink on and off according to the onoff argument */

		if (onoff) {
			// NO NEED TO HAVE IT: sync_button.set_active (true);
		} else {
			// NO NEED TO HAVE IT: sync_button.set_active (false);
		}
	} else {
		/* locked */
		// NO NEED TO HAVE IT: sync_button.set_active (true);
	}
}

void
ARDOUR_UI::audition_blink (bool onoff)
{
	if (_session == 0) {
		return;
	}

	if (_session->is_auditioning()) {
		if (onoff) {
			auditioning_alert_button.set_active (true);
		} else {
			auditioning_alert_button.set_active (false);
		}
	} else {
		auditioning_alert_button.set_active (false);
	}
}

void
ARDOUR_UI::feedback_blink (bool onoff)
{
	if (_feedback_exists) {
		if (onoff) {
			feedback_alert_button.set_active (true);
		} else {
			feedback_alert_button.set_active (false);
		}
	} else {
		feedback_alert_button.set_active (false);
	}
}

void
ARDOUR_UI::set_transport_sensitivity (bool yn)
{
	ActionManager::set_sensitive (ActionManager::transport_sensitive_actions, yn);
	shuttle_box->set_sensitive (yn);
}

void
ARDOUR_UI::editor_realized ()
{
	boost::function<void (string)> pc (boost::bind (&ARDOUR_UI::parameter_changed, this, _1));
	Config->map_parameters (pc);

	reset_dpi ();
}

void
ARDOUR_UI::update_tearoff_visibility ()
{
	if (editor) {
		editor->update_tearoff_visibility ();
	}
}

void
ARDOUR_UI::maximise_editing_space ()
{
	if (editor) {
		editor->maximise_editing_space ();
	}
}

void
ARDOUR_UI::restore_editing_space ()
{
	if (editor) {
		editor->restore_editing_space ();
	}
}

void
ARDOUR_UI::show_ui_prefs ()
{
	RefPtr<Action> act = ActionManager::get_action (X_("Window"), X_("toggle-rc-options-editor"));
	assert (act);

	act->activate();

	rc_option_editor->set_current_page (_("GUI"));
}


bool
ARDOUR_UI::click_button_clicked (GdkEventButton* ev)
{
	if (ev->button != 3) {
		/* this handler is just for button-3 clicks */
		return false;
	}

	RefPtr<Action> act = ActionManager::get_action (X_("Window"), X_("toggle-rc-options-editor"));
	assert (act);

	act->activate();

	rc_option_editor->set_current_page (_("Misc"));
	return true;
}

void
ARDOUR_UI::toggle_follow_edits ()
{
	RefPtr<Action> act = ActionManager::get_action (X_("Transport"), X_("ToggleFollowEdits"));
	assert (act);

	RefPtr<ToggleAction> tact = RefPtr<ToggleAction>::cast_dynamic (act);
	assert (tact);

	Config->set_follow_edits (tact->get_active ());
}
