/*
    Copyright (C) 2000 Paul Davis

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

/* This file contains any ARDOUR_UI methods that require knowledge of
   the various dialog boxes, and exists so that no compilation dependency
   exists between the main ARDOUR_UI modules and their respective classes.
   This is to cut down on the compile times.  It also helps with my sanity.
*/

#include "ardour/session.h"
#include "ardour/audioengine.h"
#include "ardour/automation_watch.h"

#include "actions.h"
#include "add_route_dialog.h"
#include "ardour_ui.h"
#include "bundle_manager.h"
#include "global_port_matrix.h"
#include "gui_object.h"
#include "gui_thread.h"
#include "keyeditor.h"
#include "location_ui.h"
#include "main_clock.h"
#include "midi_tracer.h"
#include "mixer_ui.h"
#include "public_editor.h"
#include "rc_option_editor.h"
#include "route_params_ui.h"
#include "shuttle_control.h"
#include "session_option_editor.h"
#include "speaker_dialog.h"
#include "sfdb_ui.h"
#include "theme_manager.h"
#include "time_info_box.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Glib;
using namespace Gtk;
using namespace Gtkmm2ext;

void
ARDOUR_UI::set_session (Session *s)
{
	SessionHandlePtr::set_session (s);

	for (ARDOUR::DataType::iterator i = ARDOUR::DataType::begin(); i != ARDOUR::DataType::end(); ++i) {
		GlobalPortMatrixWindow* w;
		if ((w = _global_port_matrix[*i]->get()) != 0) {
			w->set_session (s);
		}
	}

	if (!_session) {
		return;
	}

	const XMLNode* node = _session->extra_xml (X_("UI"));

	if (node) {
		const XMLNodeList& children = node->children();
		for (XMLNodeList::const_iterator i = children.begin(); i != children.end(); ++i) {
			if ((*i)->name() == GUIObjectState::xml_node_name) {
				gui_object_state->load (**i);
				break;
			}
		}
	}

	AutomationWatch::instance().set_session (s);

	if (location_ui->get()) {
		location_ui->get()->set_session(s);
	}

        if (speaker_config_window->get()) {
                speaker_config_window->get()->set_speakers (s->get_speakers());
        }

	if (route_params) {
		route_params->set_session (s);
	}

	if (add_route_dialog) {
		add_route_dialog->set_session (s);
	}

	if (session_option_editor) {
		session_option_editor->set_session (s);
	}

	if (shuttle_box) {
		shuttle_box->set_session (s);
	}

	for (ARDOUR::DataType::iterator i = ARDOUR::DataType::begin(); i != ARDOUR::DataType::end(); ++i) {
		if (_global_port_matrix[*i]->get()) {
			_global_port_matrix[*i]->get()->set_session (_session);
		}
	}

	primary_clock->set_session (s);
	secondary_clock->set_session (s);
	big_clock->set_session (s);
	time_info_box->set_session (s);
	video_timeline->set_session (s);

	/* sensitize menu bar options that are now valid */

	ActionManager::set_sensitive (ActionManager::session_sensitive_actions, true);
	ActionManager::set_sensitive (ActionManager::write_sensitive_actions, _session->writable());

	if (_session->locations()->num_range_markers()) {
		ActionManager::set_sensitive (ActionManager::range_sensitive_actions, true);
	} else {
		ActionManager::set_sensitive (ActionManager::range_sensitive_actions, false);
	}

	if (!_session->monitor_out()) {
		Glib::RefPtr<Action> act = ActionManager::get_action (X_("options"), X_("SoloViaBus"));
		if (act) {
			act->set_sensitive (false);
		}
	}

	/* allow wastebasket flush again */

	Glib::RefPtr<Action> act = ActionManager::get_action (X_("Main"), X_("FlushWastebasket"));
	if (act) {
		act->set_sensitive (true);
	}

	/* there are never any selections on startup */

	ActionManager::set_sensitive (ActionManager::time_selection_sensitive_actions, false);
	ActionManager::set_sensitive (ActionManager::track_selection_sensitive_actions, false);
	ActionManager::set_sensitive (ActionManager::line_selection_sensitive_actions, false);
	ActionManager::set_sensitive (ActionManager::point_selection_sensitive_actions, false);
	ActionManager::set_sensitive (ActionManager::playlist_selection_sensitive_actions, false);

	rec_button.set_sensitive (true);

	solo_alert_button.set_active (_session->soloing());

	setup_session_options ();

	Blink.connect (sigc::mem_fun(*this, &ARDOUR_UI::transport_rec_enable_blink));
	Blink.connect (sigc::mem_fun(*this, &ARDOUR_UI::solo_blink));
	Blink.connect (sigc::mem_fun(*this, &ARDOUR_UI::sync_blink));
	Blink.connect (sigc::mem_fun(*this, &ARDOUR_UI::audition_blink));
	Blink.connect (sigc::mem_fun(*this, &ARDOUR_UI::feedback_blink));

	_session->RecordStateChanged.connect (_session_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::record_state_changed, this), gui_context());
	_session->StepEditStatusChange.connect (_session_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::step_edit_status_change, this, _1), gui_context());
	_session->TransportStateChange.connect (_session_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::map_transport_state, this), gui_context());
	_session->DirtyChanged.connect (_session_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::update_autosave, this), gui_context());

	_session->Xrun.connect (_session_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::xrun_handler, this, _1), gui_context());
	_session->SoloActive.connect (_session_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::soloing_changed, this, _1), gui_context());
	_session->AuditionActive.connect (_session_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::auditioning_changed, this, _1), gui_context());
	_session->locations()->added.connect (_session_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::handle_locations_change, this, _1), gui_context());
	_session->locations()->removed.connect (_session_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::handle_locations_change, this, _1), gui_context());
	_session->config.ParameterChanged.connect (_session_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::session_parameter_changed, this, _1), gui_context ());

#ifdef HAVE_JACK_SESSION
	engine->JackSessionEvent.connect (*_session, MISSING_INVALIDATOR, boost::bind (&Session::jack_session_event, _session, _1), gui_context());
#endif

	/* Clocks are on by default after we are connected to a session, so show that here.
	*/

	connect_dependents_to_session (s);

	/* listen to clock mode changes. don't do this earlier because otherwise as the clocks
	   restore their modes or are explicitly set, we will cause the "new" mode to be saved
	   back to the session XML ("Extra") state.
	 */

	AudioClock::ModeChanged.connect (sigc::mem_fun (*this, &ARDOUR_UI::store_clock_modes));

	Glib::signal_idle().connect (sigc::mem_fun (*this, &ARDOUR_UI::first_idle));

	start_clocking ();
	start_blinking ();

	map_transport_state ();

	second_connection = Glib::signal_timeout().connect (sigc::mem_fun(*this, &ARDOUR_UI::every_second), 1000);
	point_one_second_connection = Glib::signal_timeout().connect (sigc::mem_fun(*this, &ARDOUR_UI::every_point_one_seconds), 100);
	point_zero_one_second_connection = Glib::signal_timeout().connect (sigc::mem_fun(*this, &ARDOUR_UI::every_point_zero_one_seconds), 40);

	update_format ();
}

int
ARDOUR_UI::unload_session (bool hide_stuff)
{
	if (_session) {
		ARDOUR_UI::instance()->video_timeline->sync_session_state();
	}

	if (_session && _session->dirty()) {
		std::vector<std::string> actions;
		actions.push_back (_("Don't close"));
		actions.push_back (_("Just close"));
		actions.push_back (_("Save and close"));
		switch (ask_about_saving_session (actions)) {
		case -1:
			// cancel
			return 1;

		case 1:
			_session->save_state ("");
			break;
		}
	}

	if (hide_stuff) {
		editor->hide ();
		mixer->hide ();
		theme_manager->hide ();
	}

	second_connection.disconnect ();
	point_one_second_connection.disconnect ();
	point_oh_five_second_connection.disconnect ();
	point_zero_one_second_connection.disconnect();

	ActionManager::set_sensitive (ActionManager::session_sensitive_actions, false);

	rec_button.set_sensitive (false);

	ARDOUR_UI::instance()->video_timeline->close_session();

	stop_blinking ();
	stop_clocking ();

	/* drop everything attached to the blink signal */

	Blink.clear ();

	delete _session;
	_session = 0;

	session_loaded = false;

	update_buffer_load ();

	return 0;
}

void
ARDOUR_UI::toggle_big_clock_window ()
{
	RefPtr<Action> act = ActionManager::get_action (X_("Common"), X_("ToggleBigClock"));
	if (act) {
		RefPtr<ToggleAction> tact = RefPtr<ToggleAction>::cast_dynamic(act);

		if (tact->get_active()) {
			big_clock_window->get()->show_all ();
			big_clock_window->get()->present ();
		} else {
			big_clock_window->get()->hide ();
		}
	}
}

void
ARDOUR_UI::toggle_speaker_config_window ()
{
	RefPtr<Action> act = ActionManager::get_action (X_("Common"), X_("toggle-speaker-config"));
	if (act) {
		RefPtr<ToggleAction> tact = RefPtr<ToggleAction>::cast_dynamic(act);

		if (tact->get_active()) {
			speaker_config_window->get()->show_all ();
			speaker_config_window->get()->present ();
		} else {
			speaker_config_window->get()->hide ();
		}
	}
}

void
ARDOUR_UI::new_midi_tracer_window ()
{
	RefPtr<Action> act = ActionManager::get_action (X_("Common"), X_("NewMIDITracer"));
	if (!act) {
		return;
	}

	std::list<MidiTracer*>::iterator i = _midi_tracer_windows.begin ();
	while (i != _midi_tracer_windows.end() && (*i)->get_visible() == true) {
		++i;
	}

	if (i == _midi_tracer_windows.end()) {
		/* all our MIDITracer windows are visible; make a new one */
		MidiTracer* t = new MidiTracer ();
		manage_window (*t);
		t->show_all ();
		_midi_tracer_windows.push_back (t);
	} else {
		/* re-use the hidden one */
		(*i)->show_all ();
	}
}

void
ARDOUR_UI::toggle_rc_options_window ()
{
	if (rc_option_editor == 0) {
		rc_option_editor = new RCOptionEditor;
		rc_option_editor->signal_unmap().connect(sigc::bind (sigc::ptr_fun(&ActionManager::uncheck_toggleaction), X_("<Actions>/Common/ToggleRCOptionsEditor")));
		rc_option_editor->set_session (_session);
	}

	RefPtr<Action> act = ActionManager::get_action (X_("Common"), X_("ToggleRCOptionsEditor"));
	if (act) {
		RefPtr<ToggleAction> tact = RefPtr<ToggleAction>::cast_dynamic(act);

		if (tact->get_active()) {
			rc_option_editor->show_all ();
			rc_option_editor->present ();
		} else {
			rc_option_editor->hide ();
		}
	}
}

void
ARDOUR_UI::toggle_session_options_window ()
{
	if (session_option_editor == 0) {
		session_option_editor = new SessionOptionEditor (_session);
		session_option_editor->signal_unmap().connect(sigc::bind (sigc::ptr_fun(&ActionManager::uncheck_toggleaction), X_("<Actions>/Common/ToggleSessionOptionsEditor")));
	}

	RefPtr<Action> act = ActionManager::get_action (X_("Common"), X_("ToggleSessionOptionsEditor"));
	if (act) {
		RefPtr<ToggleAction> tact = RefPtr<ToggleAction>::cast_dynamic (act);

		if (tact->get_active()) {
			session_option_editor->show_all ();
			session_option_editor->present ();
		} else {
			session_option_editor->hide ();
		}
	}
}

int
ARDOUR_UI::create_location_ui ()
{
	if (location_ui->get() == 0) {
		location_ui->set (new LocationUIWindow ());
		location_ui->get()->set_session (_session);
		location_ui->get()->signal_unmap().connect (sigc::bind (sigc::ptr_fun(&ActionManager::uncheck_toggleaction), X_("<Actions>/Common/ToggleLocations")));
	}
	return 0;
}

void
ARDOUR_UI::toggle_location_window ()
{
	if (create_location_ui()) {
		return;
	}

	RefPtr<Action> act = ActionManager::get_action (X_("Common"), X_("ToggleLocations"));
	if (act) {
		RefPtr<ToggleAction> tact = RefPtr<ToggleAction>::cast_dynamic(act);

		if (tact->get_active()) {
			location_ui->get()->show_all ();
			location_ui->get()->present ();
		} else {
			location_ui->get()->hide ();
		}
	}
}

void
ARDOUR_UI::toggle_key_editor ()
{
	if (key_editor == 0) {
		key_editor = new KeyEditor;
		key_editor->signal_unmap().connect (sigc::bind (sigc::ptr_fun(&ActionManager::uncheck_toggleaction), X_("<Actions>/Common/ToggleKeyEditor")));
	}

	RefPtr<Action> act = ActionManager::get_action (X_("Common"), X_("ToggleKeyEditor"));
	if (act) {
		RefPtr<ToggleAction> tact = RefPtr<ToggleAction>::cast_dynamic(act);

		if (tact->get_active()) {
			key_editor->show_all ();
			key_editor->present ();
		} else {
			key_editor->hide ();
		}
	}
}

void
ARDOUR_UI::toggle_theme_manager ()
{
	RefPtr<Action> act = ActionManager::get_action (X_("Common"), X_("ToggleThemeManager"));
	if (act) {
		RefPtr<ToggleAction> tact = RefPtr<ToggleAction>::cast_dynamic(act);

		if (tact->get_active()) {
			theme_manager->show_all ();
			theme_manager->present ();
		} else {
			theme_manager->hide ();
		}
	}
}

void
ARDOUR_UI::create_bundle_manager ()
{
	if (bundle_manager == 0) {
		bundle_manager = new BundleManager (_session);
		bundle_manager->signal_unmap().connect (sigc::bind (sigc::ptr_fun (&ActionManager::uncheck_toggleaction), X_("<Actions>/Common/ToggleBundleManager")));
	}
}

void
ARDOUR_UI::toggle_bundle_manager ()
{
	create_bundle_manager ();

	RefPtr<Action> act = ActionManager::get_action (X_("Common"), X_("ToggleBundleManager"));
	if (act) {
		RefPtr<ToggleAction> tact = RefPtr<ToggleAction>::cast_dynamic (act);

		if (tact->get_active()) {
			bundle_manager->show_all ();
			bundle_manager->present ();
		} else {
			bundle_manager->hide ();
		}
	}
}

int
ARDOUR_UI::create_route_params ()
{
	if (route_params == 0) {
		route_params = new RouteParams_UI ();
		route_params->set_session (_session);
		route_params->signal_unmap().connect (sigc::bind(sigc::ptr_fun(&ActionManager::uncheck_toggleaction), X_("<Actions>/Common/ToggleInspector")));
	}
	return 0;
}

void
ARDOUR_UI::toggle_route_params_window ()
{
	if (create_route_params ()) {
		return;
	}

	RefPtr<Action> act = ActionManager::get_action (X_("Common"), X_("ToggleInspector"));
	if (act) {
		RefPtr<ToggleAction> tact = RefPtr<ToggleAction>::cast_dynamic(act);

		if (tact->get_active()) {
			route_params->show_all ();
			route_params->present ();
		} else {
			route_params->hide ();
		}
	}
}

void
ARDOUR_UI::handle_locations_change (Location *)
{
	if (_session) {
		if (_session->locations()->num_range_markers()) {
			ActionManager::set_sensitive (ActionManager::range_sensitive_actions, true);
		} else {
			ActionManager::set_sensitive (ActionManager::range_sensitive_actions, false);
		}
	}
}

bool
ARDOUR_UI::main_window_state_event_handler (GdkEventWindowState* ev, bool window_was_editor)
{
	if (window_was_editor) {

		if ((ev->changed_mask & GDK_WINDOW_STATE_FULLSCREEN) &&
		    (ev->new_window_state & GDK_WINDOW_STATE_FULLSCREEN)) {
			float_big_clock (editor);
		}

	} else {

		if ((ev->changed_mask & GDK_WINDOW_STATE_FULLSCREEN) &&
		    (ev->new_window_state & GDK_WINDOW_STATE_FULLSCREEN)) {
			float_big_clock (mixer);
		}
	}

	return false;
}
