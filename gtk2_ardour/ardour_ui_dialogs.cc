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
#include "add_video_dialog.h"
#include "ardour_ui.h"
#include "big_clock_window.h"
#include "bundle_manager.h"
#include "global_port_matrix.h"
#include "gui_object.h"
#include "gui_thread.h"
#include "keyeditor.h"
#include "location_ui.h"
#include "main_clock.h"
#include "meter_patterns.h"
#include "midi_tracer.h"
#include "mixer_ui.h"
#include "public_editor.h"
#include "rc_option_editor.h"
#include "route_params_ui.h"
#include "shuttle_control.h"
#include "session_option_editor.h"
#include "speaker_dialog.h"
#include "splash.h"
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


	if (!_session) {
		WM::Manager::instance().set_session (s);
		/* Session option editor cannot exist across change-of-session */
		session_option_editor.drop_window ();
		/* Ditto for AddVideoDialog */
		add_video_dialog.drop_window ();
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

	WM::Manager::instance().set_session (s);

	AutomationWatch::instance().set_session (s);

	if (shuttle_box) {
		shuttle_box->set_session (s);
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
	point_zero_something_second_connection = Glib::signal_timeout().connect (sigc::mem_fun(*this, &ARDOUR_UI::every_point_zero_something_seconds), 40);

	update_format ();

	if (editor_meter) {
		meter_box.remove(*editor_meter);
		delete editor_meter;
		editor_meter = 0;
	}

	if (_session && _session->master_out()) {
		editor_meter = new LevelMeterHBox(_session);
		editor_meter->set_meter (_session->master_out()->shared_peak_meter().get());
		editor_meter->clear_meters();
		editor_meter->set_type (_session->master_out()->meter_type());
		editor_meter->setup_meters (30, 12, 6);
		meter_box.pack_start(*editor_meter);

		ArdourMeter::ResetAllPeakDisplays.connect (sigc::mem_fun(*this, &ARDOUR_UI::reset_peak_display));
		ArdourMeter::ResetRoutePeakDisplays.connect (sigc::mem_fun(*this, &ARDOUR_UI::reset_route_peak_display));
		ArdourMeter::ResetGroupPeakDisplays.connect (sigc::mem_fun(*this, &ARDOUR_UI::reset_group_peak_display));
	}

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
		meterbridge->hide ();
		theme_manager->hide ();
		audio_port_matrix->hide();
		midi_port_matrix->hide();
		route_params->hide();
	}

	second_connection.disconnect ();
	point_one_second_connection.disconnect ();
	point_zero_something_second_connection.disconnect();

	if (editor_meter) {
		meter_box.remove(*editor_meter);
		delete editor_meter;
		editor_meter = 0;
	}

	ActionManager::set_sensitive (ActionManager::session_sensitive_actions, false);

	rec_button.set_sensitive (false);

	WM::Manager::instance().set_session ((ARDOUR::Session*) 0);
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

static bool
_hide_splash (gpointer arg)
{
	((ARDOUR_UI*)arg)->hide_splash();
	return false;
}

void
ARDOUR_UI::goto_editor_window ()
{
	if (splash && splash->is_visible()) {
		// in 2 seconds, hide the splash screen
		Glib::signal_timeout().connect (sigc::bind (sigc::ptr_fun (_hide_splash), this), 2000);
	}

	editor->show_window ();
	editor->present ();
	/* mixer should now be on top */
	WM::Manager::instance().set_transient_for (editor);
	_mixer_on_top = false;
}

void
ARDOUR_UI::goto_mixer_window ()
{
	Glib::RefPtr<Gdk::Window> win;
	Glib::RefPtr<Gdk::Screen> screen;
	
	if (editor) {
		win = editor->get_window ();
	}

	if (win) {
		screen = win->get_screen();
	} else {
		screen = Gdk::Screen::get_default();
	}
	
	if (screen && screen->get_height() < 700) {
		Gtk::MessageDialog msg (_("This screen is not tall enough to display the mixer window"));
		msg.run ();
		return;
	}

	mixer->show_window ();
	mixer->present ();
	/* mixer should now be on top */
	WM::Manager::instance().set_transient_for (mixer);
	_mixer_on_top = true;
}

void
ARDOUR_UI::toggle_mixer_window ()
{
	Glib::RefPtr<Action> act = ActionManager::get_action (X_("Common"), X_("toggle-mixer"));
	if (!act) {
		return;
	}

	Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic (act);

	if (tact->get_active()) {
		goto_mixer_window ();
	} else {
		mixer->hide ();
	}
}

void
ARDOUR_UI::toggle_meterbridge ()
{
	Glib::RefPtr<Action> act = ActionManager::get_action (X_("Common"), X_("toggle-meterbridge"));
	if (!act) {
		return;
	}

	Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic (act);

	if (tact->get_active()) {
		meterbridge->show_window ();
	} else {
		meterbridge->hide_window (NULL);
	}
}

void
ARDOUR_UI::toggle_editor_mixer ()
{
	bool obscuring = false;
	/* currently, if windows are on different
	   screens then we do nothing; but in the
	   future we may want to bring the window 
	   to the front or something, so I'm leaving this 
	   variable for future use
	*/
        bool same_screen = true; 
	
        if (editor && mixer) {

		/* remeber: Screen != Monitor (Screen is a separately rendered
		 * continuous geometry that make include 1 or more monitors.
		 */
		
                if (editor->get_screen() != mixer->get_screen() && (mixer->get_screen() != 0) && (editor->get_screen() != 0)) {
                        // different screens, so don't do anything
                        same_screen = false;
                } else {
                        // they are on the same screen, see if they are obscuring each other

                        gint ex, ey, ew, eh;
                        gint mx, my, mw, mh;

                        editor->get_position (ex, ey);
                        editor->get_size (ew, eh);

                        mixer->get_position (mx, my);
                        mixer->get_size (mw, mh);

                        GdkRectangle e;
                        GdkRectangle m;
                        GdkRectangle r;

                        e.x = ex;
                        e.y = ey;
                        e.width = ew;
                        e.height = eh;

                        m.x = mx;
                        m.y = my;
                        m.width = mw;
                        m.height = mh;

        		if (gdk_rectangle_intersect (&e, &m, &r)) {
                                obscuring = true;
                        }
                }
        }

        if (mixer && !mixer->not_visible() && mixer->property_has_toplevel_focus()) {
                if (obscuring && same_screen) {
                        goto_editor_window();
                }
        } else if (editor && !editor->not_visible() && editor->property_has_toplevel_focus()) {
                if (obscuring && same_screen) {
                        goto_mixer_window();
                }
        } else if (mixer && mixer->not_visible()) {
                if (obscuring && same_screen) {
                        goto_mixer_window ();
                }
        } else if (editor && editor->not_visible()) {
                if (obscuring && same_screen) {
                        goto_editor_window ();
                }
        } else if (obscuring && same_screen) {
                //it's unclear what to do here, so just do the opposite of what we did last time  (old behavior)
                if (_mixer_on_top) {
			goto_editor_window ();
		} else {
			goto_mixer_window ();
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
		t->show_all ();
		_midi_tracer_windows.push_back (t);
	} else {
		/* re-use the hidden one */
		(*i)->show_all ();
	}
}

BundleManager*
ARDOUR_UI::create_bundle_manager ()
{
	return new BundleManager (_session);
}

AddVideoDialog*
ARDOUR_UI::create_add_video_dialog ()
{
	return new AddVideoDialog (_session);
}

SessionOptionEditor*
ARDOUR_UI::create_session_option_editor ()
{
	return new SessionOptionEditor (_session);
}

BigClockWindow*
ARDOUR_UI::create_big_clock_window ()
{
	return new BigClockWindow (*big_clock);
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
			if (big_clock_window) {
				big_clock_window->set_transient_for (*editor);
			}
		}

	} else {

		if ((ev->changed_mask & GDK_WINDOW_STATE_FULLSCREEN) &&
		    (ev->new_window_state & GDK_WINDOW_STATE_FULLSCREEN)) {
			if (big_clock_window) {
				big_clock_window->set_transient_for (*mixer);
			}
		}
	}

	return false;
}
