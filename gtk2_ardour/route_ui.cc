/*
    Copyright (C) 2002-2006 Paul Davis

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

#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm2ext/choice.h>
#include <gtkmm2ext/doi.h>
#include <gtkmm2ext/bindable_button.h>
#include <gtkmm2ext/barcontroller.h>
#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm2ext/utils.h>

#include "ardour/route_group.h"
#include "ardour/dB.h"
#include "pbd/memento_command.h"
#include "pbd/stacktrace.h"
#include "pbd/controllable.h"
#include "pbd/enumwriter.h"

#include "ardour_ui.h"
#include "editor.h"
#include "route_ui.h"
#include "ardour_button.h"
#include "keyboard.h"
#include "utils.h"
#include "prompter.h"
#include "gui_thread.h"
#include "ardour_dialog.h"
#include "latency_gui.h"
#include "mixer_strip.h"
#include "automation_time_axis.h"
#include "route_time_axis.h"
#include "group_tabs.h"
#include "timers.h"
#include "ui_config.h"

#include "ardour/audio_track.h"
#include "ardour/audioengine.h"
#include "ardour/filename_extensions.h"
#include "ardour/midi_track.h"
#include "ardour/internal_send.h"
#include "ardour/profile.h"
#include "ardour/send.h"
#include "ardour/route.h"
#include "ardour/session.h"
#include "ardour/template_utils.h"

#include "i18n.h"
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;
using namespace PBD;
using namespace std;

uint32_t RouteUI::_max_invert_buttons = 3;
PBD::Signal1<void, boost::shared_ptr<Route> > RouteUI::BusSendDisplayChanged;
boost::weak_ptr<Route> RouteUI::_showing_sends_to;

RouteUI::RouteUI (ARDOUR::Session* sess)
	: AxisView(sess)
	, mute_menu(0)
	, solo_menu(0)
	, sends_menu(0)
	, record_menu(0)
	, comment_window(0)
	, comment_area(0)
	, input_selector (0)
	, output_selector (0)
	, _invert_menu(0)
{
	if (sess) init ();
}

RouteUI::~RouteUI()
{
	if (_route) {
		gui_object_state().remove_node (route_state_id());
	}

	_route.reset (); /* drop reference to route, so that it can be cleaned up */
	route_connections.drop_connections ();

	delete solo_menu;
	delete mute_menu;
	delete sends_menu;
        delete record_menu;
	delete comment_window;
	delete input_selector;
	delete output_selector;
	delete _invert_menu;

	send_blink_connection.disconnect ();
	rec_blink_connection.disconnect ();
}

void
RouteUI::init ()
{
	self_destruct = true;
	mute_menu = 0;
	solo_menu = 0;
	sends_menu = 0;
        record_menu = 0;
	_invert_menu = 0;
	pre_fader_mute_check = 0;
	post_fader_mute_check = 0;
	listen_mute_check = 0;
	main_mute_check = 0;
        solo_safe_check = 0;
        solo_isolated_check = 0;
        solo_isolated_led = 0;
        solo_safe_led = 0;
	_solo_release = 0;
	_mute_release = 0;
	denormal_menu_item = 0;
        step_edit_item = 0;
	multiple_mute_change = false;
	multiple_solo_change = false;
	_i_am_the_modifier = 0;

	input_selector = 0;
	output_selector = 0;

	setup_invert_buttons ();

	mute_button = manage (new ArdourButton);
	mute_button->set_name ("mute button");
	UI::instance()->set_tip (mute_button, _("Mute this track"), "");

	solo_button = manage (new ArdourButton);
	solo_button->set_name ("solo button");
	UI::instance()->set_tip (solo_button, _("Mute other (non-soloed) tracks"), "");
	solo_button->set_no_show_all (true);

	rec_enable_button = manage (new ArdourButton);
	rec_enable_button->set_name ("record enable button");
	rec_enable_button->set_icon (ArdourIcon::RecButton);
	UI::instance()->set_tip (rec_enable_button, _("Enable recording on this track"), "");

	if (UIConfiguration::instance().get_blink_rec_arm()) {
		rec_blink_connection = Timers::blink_connect (sigc::mem_fun (*this, &RouteUI::blink_rec_display));
	}

	show_sends_button = manage (new ArdourButton);
	show_sends_button->set_name ("send alert button");
	UI::instance()->set_tip (show_sends_button, _("make mixer strips show sends to this bus"), "");

	monitor_input_button = manage (new ArdourButton (ArdourButton::default_elements));
	monitor_input_button->set_name ("monitor button");
	monitor_input_button->set_text (_("In"));
	UI::instance()->set_tip (monitor_input_button, _("Monitor input"), "");
	monitor_input_button->set_no_show_all (true);

	monitor_disk_button = manage (new ArdourButton (ArdourButton::default_elements));
	monitor_disk_button->set_name ("monitor button");
	monitor_disk_button->set_text (_("Disk"));
	UI::instance()->set_tip (monitor_disk_button, _("Monitor playback"), "");
	monitor_disk_button->set_no_show_all (true);

	_session->SoloChanged.connect (_session_connections, invalidator (*this), boost::bind (&RouteUI::solo_changed_so_update_mute, this), gui_context());
	_session->TransportStateChange.connect (_session_connections, invalidator (*this), boost::bind (&RouteUI::check_rec_enable_sensitivity, this), gui_context());
	_session->RecordStateChanged.connect (_session_connections, invalidator (*this), boost::bind (&RouteUI::session_rec_enable_changed, this), gui_context());

	_session->config.ParameterChanged.connect (*this, invalidator (*this), boost::bind (&RouteUI::parameter_changed, this, _1), gui_context());
	Config->ParameterChanged.connect (*this, invalidator (*this), boost::bind (&RouteUI::parameter_changed, this, _1), gui_context());

	rec_enable_button->signal_button_press_event().connect (sigc::mem_fun(*this, &RouteUI::rec_enable_press), false);
	rec_enable_button->signal_button_release_event().connect (sigc::mem_fun(*this, &RouteUI::rec_enable_release), false);

	show_sends_button->signal_button_press_event().connect (sigc::mem_fun(*this, &RouteUI::show_sends_press), false);
	show_sends_button->signal_button_release_event().connect (sigc::mem_fun(*this, &RouteUI::show_sends_release), false);

	solo_button->signal_button_press_event().connect (sigc::mem_fun(*this, &RouteUI::solo_press), false);
	solo_button->signal_button_release_event().connect (sigc::mem_fun(*this, &RouteUI::solo_release), false);
	mute_button->signal_button_press_event().connect (sigc::mem_fun(*this, &RouteUI::mute_press), false);
	mute_button->signal_button_release_event().connect (sigc::mem_fun(*this, &RouteUI::mute_release), false);

	monitor_input_button->set_distinct_led_click (false);
	monitor_disk_button->set_distinct_led_click (false);

	monitor_input_button->signal_button_press_event().connect (sigc::mem_fun(*this, &RouteUI::monitor_input_press), false);
	monitor_input_button->signal_button_release_event().connect (sigc::mem_fun(*this, &RouteUI::monitor_input_release), false);

	monitor_disk_button->signal_button_press_event().connect (sigc::mem_fun(*this, &RouteUI::monitor_disk_press), false);
	monitor_disk_button->signal_button_release_event().connect (sigc::mem_fun(*this, &RouteUI::monitor_disk_release), false);

	BusSendDisplayChanged.connect_same_thread (*this, boost::bind(&RouteUI::bus_send_display_changed, this, _1));
}

void
RouteUI::reset ()
{
	route_connections.drop_connections ();

	delete solo_menu;
	solo_menu = 0;

	delete mute_menu;
	mute_menu = 0;

	denormal_menu_item = 0;
}

void
RouteUI::self_delete ()
{
	delete this;
}

void
RouteUI::set_route (boost::shared_ptr<Route> rp)
{
	reset ();

	_route = rp;

	if (set_color_from_route()) {
		set_color (unique_random_color());
	}

	if (self_destruct) {
		rp->DropReferences.connect (route_connections, invalidator (*this), boost::bind (&RouteUI::self_delete, this), gui_context());
	}

	delete input_selector;
	input_selector = 0;

	delete output_selector;
	output_selector = 0;

	mute_button->set_controllable (_route->mute_control());
	solo_button->set_controllable (_route->solo_control());

	_route->active_changed.connect (route_connections, invalidator (*this), boost::bind (&RouteUI::route_active_changed, this), gui_context());
	_route->mute_changed.connect (route_connections, invalidator (*this), boost::bind (&RouteUI::update_mute_display, this), gui_context());

	_route->comment_changed.connect (route_connections, invalidator (*this), boost::bind (&RouteUI::comment_changed, this, _1), gui_context());

	_route->solo_changed.connect (route_connections, invalidator (*this), boost::bind (&RouteUI::update_solo_display, this), gui_context());
	_route->solo_safe_changed.connect (route_connections, invalidator (*this), boost::bind (&RouteUI::update_solo_display, this), gui_context());
	_route->listen_changed.connect (route_connections, invalidator (*this), boost::bind (&RouteUI::update_solo_display, this), gui_context());
	_route->solo_isolated_changed.connect (route_connections, invalidator (*this), boost::bind (&RouteUI::update_solo_display, this), gui_context());
	if (is_track()) {
		track()->TrackModeChanged.connect (route_connections, invalidator (*this), boost::bind (&RouteUI::track_mode_changed, this), gui_context());
		track_mode_changed();
	}

	_route->phase_invert_changed.connect (route_connections, invalidator (*this), boost::bind (&RouteUI::polarity_changed, this), gui_context());
	_route->PropertyChanged.connect (route_connections, invalidator (*this), boost::bind (&RouteUI::property_changed, this, _1), gui_context());

	_route->io_changed.connect (route_connections, invalidator (*this), boost::bind (&RouteUI::setup_invert_buttons, this), gui_context ());
	_route->gui_changed.connect (route_connections, invalidator (*this), boost::bind (&RouteUI::route_gui_changed, this, _1), gui_context ());

	if (_session->writable() && is_track()) {
		boost::shared_ptr<Track> t = boost::dynamic_pointer_cast<Track>(_route);

		t->RecordEnableChanged.connect (route_connections, invalidator (*this), boost::bind (&RouteUI::route_rec_enable_changed, this), gui_context());

		rec_enable_button->show();
 		rec_enable_button->set_controllable (t->rec_enable_control());

                if (is_midi_track()) {
                        midi_track()->StepEditStatusChange.connect (route_connections, invalidator (*this),
                                                                    boost::bind (&RouteUI::step_edit_changed, this, _1), gui_context());
                }

	}

	/* this will work for busses and tracks, and needs to be called to
	   set up the name entry/name label display.
	*/

	if (is_track()) {
		boost::shared_ptr<Track> t = boost::dynamic_pointer_cast<Track>(_route);
		t->MonitoringChanged.connect (route_connections, invalidator (*this), boost::bind (&RouteUI::monitoring_changed, this), gui_context());

		update_monitoring_display ();
	}

	mute_button->unset_flags (Gtk::CAN_FOCUS);
	solo_button->unset_flags (Gtk::CAN_FOCUS);

	mute_button->show();

	if (_route->is_monitor() || _route->is_master()) {
		solo_button->hide ();
	} else {
		solo_button->show();
	}

	map_frozen ();

	setup_invert_buttons ();
	set_invert_button_state ();

	boost::shared_ptr<Route> s = _showing_sends_to.lock ();
	bus_send_display_changed (s);

	update_mute_display ();
	update_solo_display ();

	if (!UIConfiguration::instance().get_blink_rec_arm()) {
		blink_rec_display(true); // set initial rec-en button state
	}

	route_color_changed();
}

void
RouteUI::polarity_changed ()
{
        if (!_route) {
                return;
        }

	set_invert_button_state ();
}

bool
RouteUI::mute_press (GdkEventButton* ev)
{
	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS ) {
		return true;
	}

	//if this is a binding action, let the ArdourButton handle it
	if ( BindingProxy::is_bind_action(ev) )
		return false;

	multiple_mute_change = false;

	if (Keyboard::is_context_menu_event (ev)) {

		if (mute_menu == 0){
			build_mute_menu();
		}

		mute_menu->popup(0,ev->time);

		return true;

	} else {

		if (Keyboard::is_button2_event (ev)) {
			// button2-click is "momentary"

			_mute_release = new SoloMuteRelease (_route->muted ());
		}

		if (ev->button == 1 || Keyboard::is_button2_event (ev)) {

			if (Keyboard::modifier_state_equals (ev->state, Keyboard::ModifierMask (Keyboard::PrimaryModifier|Keyboard::TertiaryModifier))) {

				/* toggle mute on everything (but
				 * exclude the master and monitor)
				 *
				 * because we are going to erase
				 * elements of the list we need to work
				 * on a copy.
				 */

				boost::shared_ptr<RouteList> copy (new RouteList);

				*copy = *_session->get_routes ();

				for (RouteList::iterator i = copy->begin(); i != copy->end(); ) {
					if ((*i)->is_master() || (*i)->is_monitor()) {
						i = copy->erase (i);
					} else {
						++i;
					}
				}

				if (_mute_release) {
					_mute_release->routes = copy;
				}

				DisplaySuspender ds;
				_session->set_mute (copy, !_route->muted());

			} else if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {

				/* Primary-button1 applies change to the mix group even if it is not active
				   NOTE: Primary-button2 is MIDI learn.
				*/

				boost::shared_ptr<RouteList> rl;

				if (ev->button == 1) {

					if (_route->route_group()) {

						rl = _route->route_group()->route_list();

						if (_mute_release) {
							_mute_release->routes = rl;
						}
					} else {
						rl.reset (new RouteList);
						rl->push_back (_route);
					}

					DisplaySuspender ds;
					_session->set_mute (rl, !_route->muted(), Session::rt_cleanup, true);
				}

			} else {

				/* plain click applies change to this route */

				boost::shared_ptr<RouteList> rl (new RouteList);
				rl->push_back (_route);

				if (_mute_release) {
					_mute_release->routes = rl;
				}

				_session->set_mute (rl, !_route->muted());

			}
		}
	}

	return false;
}

bool
RouteUI::mute_release (GdkEventButton* /*ev*/)
{
	if (_mute_release){
		DisplaySuspender ds;
		_session->set_mute (_mute_release->routes, _mute_release->active, Session::rt_cleanup, true);
		delete _mute_release;
		_mute_release = 0;
	}

	return false;
}

void
RouteUI::edit_output_configuration ()
{
	if (output_selector == 0) {

		boost::shared_ptr<Send> send;
		boost::shared_ptr<IO> output;

		if ((send = boost::dynamic_pointer_cast<Send>(_current_delivery)) != 0) {
			if (!boost::dynamic_pointer_cast<InternalSend>(send)) {
				output = send->output();
			} else {
				output = _route->output ();
			}
		} else {
			output = _route->output ();
		}

		output_selector = new IOSelectorWindow (_session, output);
	}

	if (output_selector->is_visible()) {
		output_selector->get_toplevel()->get_window()->raise();
	} else {
		output_selector->present ();
	}

	//output_selector->set_keep_above (true);
}

void
RouteUI::edit_input_configuration ()
{
	if (input_selector == 0) {
		input_selector = new IOSelectorWindow (_session, _route->input());
	}

	if (input_selector->is_visible()) {
		input_selector->get_toplevel()->get_window()->raise();
	} else {
		input_selector->present ();
	}

	//input_selector->set_keep_above (true);
}

bool
RouteUI::solo_press(GdkEventButton* ev)
{
	/* ignore double/triple clicks */

	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS ) {
		return true;
	}

	//if this is a binding action, let the ArdourButton handle it
	if ( BindingProxy::is_bind_action(ev) )
		return false;

	multiple_solo_change = false;

	if (Keyboard::is_context_menu_event (ev)) {

		if (! (solo_isolated_led && solo_isolated_led->is_visible()) ||
		    ! (solo_safe_led && solo_safe_led->is_visible())) {

			if (solo_menu == 0) {
				build_solo_menu ();
			}

			solo_menu->popup (1, ev->time);
		}

	} else {

		if (Keyboard::is_button2_event (ev)) {

			// button2-click is "momentary"
			_solo_release = new SoloMuteRelease (_route->self_soloed());
		}

		if (ev->button == 1 || Keyboard::is_button2_event (ev)) {

			if (Keyboard::modifier_state_equals (ev->state, Keyboard::ModifierMask (Keyboard::PrimaryModifier|Keyboard::TertiaryModifier))) {

				/* Primary-Tertiary-click applies change to all routes */

				if (_solo_release) {
					_solo_release->routes = _session->get_routes ();
				}

				DisplaySuspender ds;
				if (Config->get_solo_control_is_listen_control()) {
					_session->set_listen (_session->get_routes(), !_route->listening_via_monitor(),  Session::rt_cleanup, false);
				} else {
					_session->set_solo (_session->get_routes(), !_route->self_soloed(),  Session::rt_cleanup, false);
				}

			} else if (Keyboard::modifier_state_contains (ev->state, Keyboard::ModifierMask (Keyboard::PrimaryModifier|Keyboard::SecondaryModifier))) {

				// Primary-Secondary-click: exclusively solo this track

				if (_solo_release) {
					_solo_release->exclusive = true;

					boost::shared_ptr<RouteList> routes = _session->get_routes();

					for (RouteList::iterator i = routes->begin(); i != routes->end(); ++i) {
						if ((*i)->soloed ()) {
							_solo_release->routes_on->push_back (*i);
						} else {
							_solo_release->routes_off->push_back (*i);
						}
					}
				}

				if (Config->get_solo_control_is_listen_control()) {
					/* ??? we need a just_one_listen() method */
				} else {
					DisplaySuspender ds;
					_session->set_just_one_solo (_route, true);
				}

			} else if (Keyboard::modifier_state_equals (ev->state, Keyboard::TertiaryModifier)) {

				// shift-click: toggle solo isolated status

				_route->set_solo_isolated (!_route->solo_isolated(), this);
				delete _solo_release;
				_solo_release = 0;

			} else if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {

				/* Primary-button1: solo mix group.
				   NOTE: Primary-button2 is MIDI learn.
				*/

				/* Primary-button1 applies change to the mix group even if it is not active
				   NOTE: Primary-button2 is MIDI learn.
				*/

				boost::shared_ptr<RouteList> rl;

				if (ev->button == 1) {
					if (ARDOUR::Profile->get_mixbus() && _route->route_group()) {

						rl = _route->route_group()->route_list();

						if (_solo_release) {
							_solo_release->routes = rl;
						}
					} else {
						rl.reset (new RouteList);
						rl->push_back (_route);
					}

					DisplaySuspender ds;
					if (Config->get_solo_control_is_listen_control()) {
						_session->set_listen (rl, !_route->listening_via_monitor(),  Session::rt_cleanup, true);
					} else {
						_session->set_solo (rl, !_route->self_soloed(),  Session::rt_cleanup, true);
					}
				}

				delete _solo_release;
				_solo_release = 0;

			} else {

				/* click: solo this route */

				boost::shared_ptr<RouteList> rl (new RouteList);
				rl->push_back (route());

				if (_solo_release) {
					_solo_release->routes = rl;
				}

				DisplaySuspender ds;
				if (Config->get_solo_control_is_listen_control()) {
					_session->set_listen (rl, !_route->listening_via_monitor());
				} else {
					_session->set_solo (rl, !_route->self_soloed());
				}
			}
		}
	}

	return false;
}

bool
RouteUI::solo_release (GdkEventButton* /*ev*/)
{
	if (_solo_release) {

		if (_solo_release->exclusive) {

		} else {
			DisplaySuspender ds;
			if (Config->get_solo_control_is_listen_control()) {
				_session->set_listen (_solo_release->routes, _solo_release->active, Session::rt_cleanup, false);
			} else {
				_session->set_solo (_solo_release->routes, _solo_release->active, Session::rt_cleanup, false);
			}
		}

		delete _solo_release;
		_solo_release = 0;
	}

	return false;
}

bool
RouteUI::rec_enable_press(GdkEventButton* ev)
{
	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS ) {
		return true;
	}

	//if this is a binding action, let the ArdourButton handle it
	if ( BindingProxy::is_bind_action(ev) )
		return false;

	if (!_session->engine().connected()) {
	        MessageDialog msg (_("Not connected to AudioEngine - cannot engage record"));
		msg.run ();
		return false;
	}

	if (is_midi_track()) {

		/* rec-enable button exits from step editing */

		if (midi_track()->step_editing()) {
			midi_track()->set_step_editing (false);
			return false;
		}
	}

	if (is_track() && rec_enable_button) {

		if (Keyboard::is_button2_event (ev)) {

			//rec arm does not have a momentary mode
			return false;

		} else if (Keyboard::modifier_state_equals (ev->state, Keyboard::ModifierMask (Keyboard::PrimaryModifier|Keyboard::TertiaryModifier))) {

			DisplaySuspender ds;
			_session->set_record_enabled (_session->get_routes(), !_route->record_enabled());

		} else if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {

			/* Primary-button1 applies change to the route group (even if it is not active)
			   NOTE: Primary-button2 is MIDI learn.
			*/

			if (ev->button == 1) {

				boost::shared_ptr<RouteList> rl;

				if (_route->route_group()) {

					rl = _route->route_group()->route_list();

				} else {
					rl.reset (new RouteList);
					rl->push_back (_route);
				}

				DisplaySuspender ds;
				_session->set_record_enabled (rl, !_route->record_enabled(), Session::rt_cleanup, true);
			}

		} else if (Keyboard::is_context_menu_event (ev)) {

			/* do this on release */

		} else {

			boost::shared_ptr<RouteList> rl (new RouteList);
			rl->push_back (route());
			DisplaySuspender ds;
			_session->set_record_enabled (rl, !_route->record_enabled());
		}
	}

	return false;
}

void
RouteUI::monitoring_changed ()
{
	update_monitoring_display ();
}

void
RouteUI::update_monitoring_display ()
{
	if (!_route) {
		return;
	}

	boost::shared_ptr<Track> t = boost::dynamic_pointer_cast<Track>(_route);

	if (!t) {
		return;
	}

	MonitorState ms = t->monitoring_state();

	if (t->monitoring_choice() & MonitorInput) {
		monitor_input_button->set_active_state (Gtkmm2ext::ExplicitActive);
	} else {
		if (ms & MonitoringInput) {
			monitor_input_button->set_active_state (Gtkmm2ext::ImplicitActive);
		} else {
			monitor_input_button->unset_active_state ();
		}
	}

	if (t->monitoring_choice() & MonitorDisk) {
		monitor_disk_button->set_active_state (Gtkmm2ext::ExplicitActive);
	} else {
		if (ms & MonitoringDisk) {
			monitor_disk_button->set_active_state (Gtkmm2ext::ImplicitActive);
		} else {
			monitor_disk_button->unset_active_state ();
		}
	}
}

bool
RouteUI::monitor_input_press(GdkEventButton*)
{
	return false;
}

bool
RouteUI::monitor_input_release(GdkEventButton* ev)
{
	return monitor_release (ev, MonitorInput);
}

bool
RouteUI::monitor_disk_press (GdkEventButton*)
{
	return false;
}

bool
RouteUI::monitor_disk_release (GdkEventButton* ev)
{
	return monitor_release (ev, MonitorDisk);
}

bool
RouteUI::monitor_release (GdkEventButton* ev, MonitorChoice monitor_choice)
{
	if (ev->button != 1) {
		return false;
	}

	boost::shared_ptr<Track> t = boost::dynamic_pointer_cast<Track>(_route);

	if (!t) {
		return true;
	}

	MonitorChoice mc;
	boost::shared_ptr<RouteList> rl;

	/* XXX for now, monitoring choices are orthogonal. cue monitoring
	   will follow in 3.X but requires mixing the input and playback (disk)
	   signal together, which requires yet more buffers.
	*/

	if (t->monitoring_choice() & monitor_choice) {
		mc = MonitorChoice (t->monitoring_choice() & ~monitor_choice);
	} else {
		/* this line will change when the options are non-orthogonal */
		// mc = MonitorChoice (t->monitoring_choice() | monitor_choice);
		mc = monitor_choice;
	}

	if (Keyboard::modifier_state_equals (ev->state, Keyboard::ModifierMask (Keyboard::PrimaryModifier|Keyboard::TertiaryModifier))) {
		rl = _session->get_routes ();

	} else if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {
		if (_route->route_group() && _route->route_group()->is_monitoring()) {
			rl = _route->route_group()->route_list();
		} else {
			rl.reset (new RouteList);
			rl->push_back (route());
		}
	} else {
		rl.reset (new RouteList);
		rl->push_back (route());
	}

	DisplaySuspender ds;
	_session->set_monitoring (rl, mc, Session::rt_cleanup, true);

	return false;
}

void
RouteUI::build_record_menu ()
{
	if (record_menu) {
		return;
	}

	/* no rec-button context menu for non-MIDI tracks
	 */

	if (is_midi_track()) {
		record_menu = new Menu;
		record_menu->set_name ("ArdourContextMenu");

		using namespace Menu_Helpers;
		MenuList& items = record_menu->items();

		items.push_back (CheckMenuElem (_("Step Entry"), sigc::mem_fun (*this, &RouteUI::toggle_step_edit)));
		step_edit_item = dynamic_cast<Gtk::CheckMenuItem*> (&items.back());

		if (_route->record_enabled()) {
			step_edit_item->set_sensitive (false);
		}

		step_edit_item->set_active (midi_track()->step_editing());
	}
}

void
RouteUI::toggle_step_edit ()
{
	if (!is_midi_track() || _route->record_enabled()) {
		return;
	}

	midi_track()->set_step_editing (step_edit_item->get_active());
}

void
RouteUI::step_edit_changed (bool yn)
{
	if (yn) {
		if (rec_enable_button) {
			rec_enable_button->set_active_state (Gtkmm2ext::ExplicitActive);
		}

		start_step_editing ();

		if (step_edit_item) {
			step_edit_item->set_active (true);
		}

	} else {

		if (rec_enable_button) {
			rec_enable_button->unset_active_state ();
		}

		stop_step_editing ();

		if (step_edit_item) {
			step_edit_item->set_active (false);
		}
	}
}

bool
RouteUI::rec_enable_release (GdkEventButton* ev)
{
	if (Keyboard::is_context_menu_event (ev)) {
		build_record_menu ();
		if (record_menu) {
			record_menu->popup (1, ev->time);
		}
		return false;
	}

	return false;
}

void
RouteUI::build_sends_menu ()
{
	using namespace Menu_Helpers;

	sends_menu = new Menu;
	sends_menu->set_name ("ArdourContextMenu");
	MenuList& items = sends_menu->items();

	items.push_back (
		MenuElem(_("Assign all tracks (prefader)"), sigc::bind (sigc::mem_fun (*this, &RouteUI::create_sends), PreFader, false))
		);

	items.push_back (
		MenuElem(_("Assign all tracks and buses (prefader)"), sigc::bind (sigc::mem_fun (*this, &RouteUI::create_sends), PreFader, true))
		);

	items.push_back (
		MenuElem(_("Assign all tracks (postfader)"), sigc::bind (sigc::mem_fun (*this, &RouteUI::create_sends), PostFader, false))
		);

	items.push_back (
		MenuElem(_("Assign all tracks and buses (postfader)"), sigc::bind (sigc::mem_fun (*this, &RouteUI::create_sends), PostFader, true))
		);

	items.push_back (
		MenuElem(_("Assign selected tracks (prefader)"), sigc::bind (sigc::mem_fun (*this, &RouteUI::create_selected_sends), PreFader, false))
		);

	items.push_back (
		MenuElem(_("Assign selected tracks and buses (prefader)"), sigc::bind (sigc::mem_fun (*this, &RouteUI::create_selected_sends), PreFader, true)));

	items.push_back (
		MenuElem(_("Assign selected tracks (postfader)"), sigc::bind (sigc::mem_fun (*this, &RouteUI::create_selected_sends), PostFader, false))
		);

	items.push_back (
		MenuElem(_("Assign selected tracks and buses (postfader)"), sigc::bind (sigc::mem_fun (*this, &RouteUI::create_selected_sends), PostFader, true))
		);

	items.push_back (MenuElem(_("Copy track/bus gains to sends"), sigc::mem_fun (*this, &RouteUI::set_sends_gain_from_track)));
	items.push_back (MenuElem(_("Set sends gain to -inf"), sigc::mem_fun (*this, &RouteUI::set_sends_gain_to_zero)));
	items.push_back (MenuElem(_("Set sends gain to 0dB"), sigc::mem_fun (*this, &RouteUI::set_sends_gain_to_unity)));

}

void
RouteUI::create_sends (Placement p, bool include_buses)
{
	_session->globally_add_internal_sends (_route, p, include_buses);
}

void
RouteUI::create_selected_sends (Placement p, bool include_buses)
{
	boost::shared_ptr<RouteList> rlist (new RouteList);
	TrackSelection& selected_tracks (ARDOUR_UI::instance()->the_editor().get_selection().tracks);

	for (TrackSelection::iterator i = selected_tracks.begin(); i != selected_tracks.end(); ++i) {
		RouteTimeAxisView* rtv;
		RouteUI* rui;
		if ((rtv = dynamic_cast<RouteTimeAxisView*>(*i)) != 0) {
			if ((rui = dynamic_cast<RouteUI*>(rtv)) != 0) {
				if (include_buses || boost::dynamic_pointer_cast<AudioTrack>(rui->route())) {
					rlist->push_back (rui->route());
				}
			}
		}
	}

	_session->add_internal_sends (_route, p, rlist);
}

void
RouteUI::set_sends_gain_from_track ()
{
	_session->globally_set_send_gains_from_track (_route);
}

void
RouteUI::set_sends_gain_to_zero ()
{
	_session->globally_set_send_gains_to_zero (_route);
}

void
RouteUI::set_sends_gain_to_unity ()
{
	_session->globally_set_send_gains_to_unity (_route);
}

bool
RouteUI::show_sends_press(GdkEventButton* ev)
{
	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS ) {
		return true;
	}

	if (!is_track() && show_sends_button) {

		if (Keyboard::is_button2_event (ev) && Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {

			// do nothing on midi sigc::bind event
			return false;

		} else if (Keyboard::is_context_menu_event (ev)) {

			if (sends_menu == 0) {
				build_sends_menu ();
			}

			sends_menu->popup (0, ev->time);

		} else {

			boost::shared_ptr<Route> s = _showing_sends_to.lock ();

			if (s == _route) {
				set_showing_sends_to (boost::shared_ptr<Route> ());
			} else {
				set_showing_sends_to (_route);
			}
		}
	}

	return true;
}

bool
RouteUI::show_sends_release (GdkEventButton*)
{
	return true;
}

void
RouteUI::send_blink (bool onoff)
{
	if (!show_sends_button) {
		return;
	}

	if (onoff) {
		show_sends_button->set_active_state (Gtkmm2ext::ExplicitActive);
	} else {
		show_sends_button->unset_active_state ();
	}
}

Gtkmm2ext::ActiveState
RouteUI::solo_active_state (boost::shared_ptr<Route> r)
{
	if (r->is_master() || r->is_monitor()) {
		return Gtkmm2ext::Off;
	}

	if (Config->get_solo_control_is_listen_control()) {

		if (r->listening_via_monitor()) {
			return Gtkmm2ext::ExplicitActive;
		} else {
			return Gtkmm2ext::Off;
		}

	}

	if (r->soloed()) {
                if (!r->self_soloed()) {
                        return Gtkmm2ext::ImplicitActive;
                } else {
                        return Gtkmm2ext::ExplicitActive;
                }
	} else {
		return Gtkmm2ext::Off;
	}
}

Gtkmm2ext::ActiveState
RouteUI::solo_isolate_active_state (boost::shared_ptr<Route> r)
{
	if (r->is_master() || r->is_monitor()) {
		return Gtkmm2ext::Off;
	}

	if (r->solo_isolated()) {
		return Gtkmm2ext::ExplicitActive;
	} else {
		return Gtkmm2ext::Off;
	}
}

Gtkmm2ext::ActiveState
RouteUI::solo_safe_active_state (boost::shared_ptr<Route> r)
{
	if (r->is_master() || r->is_monitor()) {
		return Gtkmm2ext::Off;
	}

	if (r->solo_safe()) {
		return Gtkmm2ext::ExplicitActive;
	} else {
		return Gtkmm2ext::Off;
	}
}

void
RouteUI::update_solo_display ()
{
	bool yn = _route->solo_safe ();

	if (solo_safe_check && solo_safe_check->get_active() != yn) {
		solo_safe_check->set_active (yn);
	}

	yn = _route->solo_isolated ();

	if (solo_isolated_check && solo_isolated_check->get_active() != yn) {
		solo_isolated_check->set_active (yn);
	}

        set_button_names ();

        if (solo_isolated_led) {
		if (_route->solo_isolated()) {
			solo_isolated_led->set_active_state (Gtkmm2ext::ExplicitActive);
		} else {
			solo_isolated_led->unset_active_state ();
		}
        }

        if (solo_safe_led) {
		if (_route->solo_safe()) {
			solo_safe_led->set_active_state (Gtkmm2ext::ExplicitActive);
		} else {
			solo_safe_led->unset_active_state ();
		}
        }

	solo_button->set_active_state (solo_active_state (_route));

        /* some changes to solo status can affect mute display, so catch up
         */

        update_mute_display ();
}

void
RouteUI::solo_changed_so_update_mute ()
{
	update_mute_display ();
}

ActiveState
RouteUI::mute_active_state (Session* s, boost::shared_ptr<Route> r)
{
	if (r->is_monitor()) {
		return ActiveState(0);
	}


	if (Config->get_show_solo_mutes() && !Config->get_solo_control_is_listen_control ()) {

		if (r->muted ()) {
			/* full mute */
			return Gtkmm2ext::ExplicitActive;
		} else if (r->muted_by_others()) {
			return Gtkmm2ext::ImplicitActive;
		} else {
			/* no mute at all */
			return Gtkmm2ext::Off;
		}

	} else {

		if (r->muted()) {
			/* full mute */
			return Gtkmm2ext::ExplicitActive;
		} else {
			/* no mute at all */
			return Gtkmm2ext::Off;
		}
	}

	return ActiveState(0);
}

void
RouteUI::update_mute_display ()
{
        if (!_route) {
                return;
        }

        mute_button->set_active_state (mute_active_state (_session, _route));
}

void
RouteUI::route_rec_enable_changed ()
{
	blink_rec_display(true);  //this lets the button change "immediately" rather than wait for the next blink
	update_monitoring_display ();
}

void
RouteUI::session_rec_enable_changed ()
{
	blink_rec_display(true);  //this lets the button change "immediately" rather than wait for the next blink
	update_monitoring_display ();
}

void
RouteUI::blink_rec_display (bool blinkOn)
{
	if (!rec_enable_button || !_route) {
		return;
	}
	if (boost::dynamic_pointer_cast<Send>(_current_delivery)) {
		return;
	}

	if (_route->record_enabled()) {
                switch (_session->record_status ()) {
                case Session::Recording:
                        rec_enable_button->set_active_state (Gtkmm2ext::ExplicitActive);
                        break;

                case Session::Disabled:
                case Session::Enabled:
                        if ( UIConfiguration::instance().get_blink_rec_arm() )
							rec_enable_button->set_active_state ( blinkOn ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off );
						else
							rec_enable_button->set_active_state ( ImplicitActive );
                        break;

                }

                if (step_edit_item) {
                        step_edit_item->set_sensitive (false);
                }

	} else {
		rec_enable_button->unset_active_state ();

                if (step_edit_item) {
                        step_edit_item->set_sensitive (true);
                }
	}


	check_rec_enable_sensitivity ();
}

void
RouteUI::build_solo_menu (void)
{
	using namespace Menu_Helpers;

	solo_menu = new Menu;
	solo_menu->set_name ("ArdourContextMenu");
	MenuList& items = solo_menu->items();
	Gtk::CheckMenuItem* check;

	check = new Gtk::CheckMenuItem(_("Solo Isolate"));
	check->set_active (_route->solo_isolated());
	check->signal_toggled().connect (sigc::bind (sigc::mem_fun (*this, &RouteUI::toggle_solo_isolated), check));
	items.push_back (CheckMenuElem(*check));
        solo_isolated_check = dynamic_cast<Gtk::CheckMenuItem*>(&items.back());
	check->show_all();

	check = new Gtk::CheckMenuItem(_("Solo Safe"));
	check->set_active (_route->solo_safe());
	check->signal_toggled().connect (sigc::bind (sigc::mem_fun (*this, &RouteUI::toggle_solo_safe), check));
	items.push_back (CheckMenuElem(*check));
        solo_safe_check = dynamic_cast<Gtk::CheckMenuItem*>(&items.back());
	check->show_all();

	//items.push_back (SeparatorElem());
	// items.push_back (MenuElem (_("MIDI Bind"), sigc::mem_fun (*mute_button, &BindableToggleButton::midi_learn)));

}

void
RouteUI::build_mute_menu(void)
{
	using namespace Menu_Helpers;

	mute_menu = new Menu;
	mute_menu->set_name ("ArdourContextMenu");

	MenuList& items = mute_menu->items();

	pre_fader_mute_check = manage (new Gtk::CheckMenuItem(_("Pre Fader Sends")));
	init_mute_menu(MuteMaster::PreFader, pre_fader_mute_check);
	pre_fader_mute_check->signal_toggled().connect(sigc::bind (sigc::mem_fun (*this, &RouteUI::toggle_mute_menu), MuteMaster::PreFader, pre_fader_mute_check));
	items.push_back (CheckMenuElem(*pre_fader_mute_check));
	pre_fader_mute_check->show_all();

	post_fader_mute_check = manage (new Gtk::CheckMenuItem(_("Post Fader Sends")));
	init_mute_menu(MuteMaster::PostFader, post_fader_mute_check);
	post_fader_mute_check->signal_toggled().connect(sigc::bind (sigc::mem_fun (*this, &RouteUI::toggle_mute_menu), MuteMaster::PostFader, post_fader_mute_check));
	items.push_back (CheckMenuElem(*post_fader_mute_check));
	post_fader_mute_check->show_all();

	listen_mute_check = manage (new Gtk::CheckMenuItem(_("Control Outs")));
	init_mute_menu(MuteMaster::Listen, listen_mute_check);
	listen_mute_check->signal_toggled().connect(sigc::bind (sigc::mem_fun (*this, &RouteUI::toggle_mute_menu), MuteMaster::Listen, listen_mute_check));
	items.push_back (CheckMenuElem(*listen_mute_check));
	listen_mute_check->show_all();

	main_mute_check = manage (new Gtk::CheckMenuItem(_("Main Outs")));
	init_mute_menu(MuteMaster::Main, main_mute_check);
	main_mute_check->signal_toggled().connect(sigc::bind (sigc::mem_fun (*this, &RouteUI::toggle_mute_menu), MuteMaster::Main, main_mute_check));
	items.push_back (CheckMenuElem(*main_mute_check));
	main_mute_check->show_all();

	//items.push_back (SeparatorElem());
	// items.push_back (MenuElem (_("MIDI Bind"), sigc::mem_fun (*mute_button, &BindableToggleButton::midi_learn)));

	_route->mute_points_changed.connect (route_connections, invalidator (*this), boost::bind (&RouteUI::muting_change, this), gui_context());
}

void
RouteUI::init_mute_menu(MuteMaster::MutePoint mp, Gtk::CheckMenuItem* check)
{
	check->set_active (_route->mute_points() & mp);
}

void
RouteUI::toggle_mute_menu(MuteMaster::MutePoint mp, Gtk::CheckMenuItem* check)
{
	if (check->get_active()) {
		_route->set_mute_points (MuteMaster::MutePoint (_route->mute_points() | mp));
	} else {
		_route->set_mute_points (MuteMaster::MutePoint (_route->mute_points() & ~mp));
	}
}

void
RouteUI::muting_change ()
{
	ENSURE_GUI_THREAD (*this, &RouteUI::muting_change)

	bool yn;
	MuteMaster::MutePoint current = _route->mute_points ();

	yn = (current & MuteMaster::PreFader);

	if (pre_fader_mute_check->get_active() != yn) {
		pre_fader_mute_check->set_active (yn);
	}

	yn = (current & MuteMaster::PostFader);

	if (post_fader_mute_check->get_active() != yn) {
		post_fader_mute_check->set_active (yn);
	}

	yn = (current & MuteMaster::Listen);

	if (listen_mute_check->get_active() != yn) {
		listen_mute_check->set_active (yn);
	}

	yn = (current & MuteMaster::Main);

	if (main_mute_check->get_active() != yn) {
		main_mute_check->set_active (yn);
	}
}

bool
RouteUI::solo_isolate_button_release (GdkEventButton* ev)
{
	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS) {
		return true;
	}

	bool view = solo_isolated_led->active_state();
	bool model = _route->solo_isolated();

	/* called BEFORE the view has changed */

	if (ev->button == 1) {
		if (Keyboard::modifier_state_equals (ev->state, Keyboard::ModifierMask (Keyboard::PrimaryModifier|Keyboard::TertiaryModifier))) {

			if (model) {
				/* disable isolate for all routes */
				DisplaySuspender ds;
				_session->set_solo_isolated (_session->get_routes(), false, Session::rt_cleanup, true);
			} else {
				/* enable isolate for all routes */
				DisplaySuspender ds;
				_session->set_solo_isolated (_session->get_routes(), true, Session::rt_cleanup, true);
			}

		} else {

			if (model == view) {

				/* flip just this route */

				boost::shared_ptr<RouteList> rl (new RouteList);
				rl->push_back (_route);
				DisplaySuspender ds;
				_session->set_solo_isolated (rl, !view, Session::rt_cleanup, true);
			}
		}
	}

	return false;
}

bool
RouteUI::solo_safe_button_release (GdkEventButton* ev)
{
	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS) {
		return true;
	}

	bool view = solo_safe_led->active_state();
	bool model = _route->solo_safe();

	if (ev->button == 1) {
		if (Keyboard::modifier_state_equals (ev->state, Keyboard::ModifierMask (Keyboard::PrimaryModifier|Keyboard::TertiaryModifier))) {
			boost::shared_ptr<RouteList> rl (_session->get_routes());
			if (model) {
				/* disable solo safe for all routes */
				DisplaySuspender ds;
				for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
					(*i)->set_solo_safe (false, this);
				}
			} else {
				/* enable solo safe for all routes */
				DisplaySuspender ds;
				for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
					(*i)->set_solo_safe (true, this);
				}
			}
		}
		else {
			if (model == view) {
				/* flip just this route */
				_route->set_solo_safe (!view, this);
			}
		}
	}

	return false;
}

void
RouteUI::toggle_solo_isolated (Gtk::CheckMenuItem* check)
{
        bool view = check->get_active();
        bool model = _route->solo_isolated();

        /* called AFTER the view has changed */

        if (model != view) {
                _route->set_solo_isolated (view, this);
        }
}

void
RouteUI::toggle_solo_safe (Gtk::CheckMenuItem* check)
{
	_route->set_solo_safe (check->get_active(), this);
}

/** Ask the user to choose a colour, and then apply that color to my route
 */
void
RouteUI::choose_color ()
{
	bool picked;
	Gdk::Color const color = Gtkmm2ext::UI::instance()->get_color (_("Color Selection"), picked, &_color);

	if (picked) {
		set_color(color);
	}
}

/** Set the route's own color.  This may not be used for display if
 *  the route is in a group which shares its color with its routes.
 */
void
RouteUI::set_color (const Gdk::Color & c)
{
	/* leave _color alone in the group case so that tracks can retain their
	 * own pre-group colors.
	 */

	char buf[64];
	_color = c;
	snprintf (buf, sizeof (buf), "%d:%d:%d", c.get_red(), c.get_green(), c.get_blue());

	/* note: we use the route state ID here so that color is the same for both
	   the time axis view and the mixer strip
	*/

	gui_object_state().set_property<string> (route_state_id(), X_("color"), buf);
	_route->gui_changed ("color", (void *) 0); /* EMIT_SIGNAL */
}

/** @return GUI state ID for things that are common to the route in all its representations */
string
RouteUI::route_state_id () const
{
	return string_compose (X_("route %1"), _route->id().to_s());
}

int
RouteUI::set_color_from_route ()
{
	const string str = gui_object_state().get_string (route_state_id(), X_("color"));

	if (str.empty()) {
		return 1;
	}

	int r, g, b;

	sscanf (str.c_str(), "%d:%d:%d", &r, &g, &b);

	_color.set_red (r);
	_color.set_green (g);
	_color.set_blue (b);

	return 0;
}

/** @return true if this name should be used for the route, otherwise false */
bool
RouteUI::verify_new_route_name (const std::string& name)
{
	if (name.find (':') == string::npos) {
		return true;
	}

	MessageDialog colon_msg (
		_("The use of colons (':') is discouraged in track and bus names.\nDo you want to use this new name?"),
		false, MESSAGE_QUESTION, BUTTONS_NONE
		);

	colon_msg.add_button (_("Use the new name"), Gtk::RESPONSE_ACCEPT);
	colon_msg.add_button (_("Re-edit the name"), Gtk::RESPONSE_CANCEL);

	return (colon_msg.run () == Gtk::RESPONSE_ACCEPT);
}

void
RouteUI::route_rename ()
{
	ArdourPrompter name_prompter (true);
	string result;
	bool done = false;

	if (is_track()) {
		name_prompter.set_title (_("Rename Track"));
	} else {
		name_prompter.set_title (_("Rename Bus"));
	}
	name_prompter.set_prompt (_("New name:"));
	name_prompter.set_initial_text (_route->name());
	name_prompter.add_button (_("Rename"), Gtk::RESPONSE_ACCEPT);
	name_prompter.set_response_sensitive (Gtk::RESPONSE_ACCEPT, false);
	name_prompter.show_all ();

	while (!done) {
		switch (name_prompter.run ()) {
		case Gtk::RESPONSE_ACCEPT:
			name_prompter.get_result (result);
			name_prompter.hide ();
			if (result.length()) {
				if (verify_new_route_name (result)) {
					_route->set_name (result);
					done = true;
				} else {
					/* back to name prompter */
				}

			} else {
				/* nothing entered, just get out of here */
				done = true;
			}
			break;
		default:
			done = true;
			break;
		}
	}

	return;

}

void
RouteUI::property_changed (const PropertyChange& what_changed)
{
	if (what_changed.contains (ARDOUR::Properties::name)) {
		name_label.set_text (_route->name());
	}
}

void
RouteUI::toggle_comment_editor ()
{
//	if (ignore_toggle) {
//		return;
//	}

	if (comment_window && comment_window->is_visible ()) {
		comment_window->hide ();
	} else {
		open_comment_editor ();
	}
}


void
RouteUI::open_comment_editor ()
{
	if (comment_window == 0) {
		setup_comment_editor ();
	}

	string title;
	title = _route->name();
	title += _(": comment editor");

	comment_window->set_title (title);
	comment_window->present();
}

void
RouteUI::setup_comment_editor ()
{
	comment_window = new ArdourWindow (""); // title will be reset to show route
	comment_window->set_skip_taskbar_hint (true);
	comment_window->signal_hide().connect (sigc::mem_fun(*this, &MixerStrip::comment_editor_done_editing));
	comment_window->set_default_size (400, 200);

	comment_area = manage (new TextView());
	comment_area->set_name ("MixerTrackCommentArea");
	comment_area->set_wrap_mode (WRAP_WORD);
	comment_area->set_editable (true);
	comment_area->get_buffer()->set_text (_route->comment());
	comment_area->show ();

	comment_window->add (*comment_area);
}

void
RouteUI::comment_changed (void *src)
{
	ENSURE_GUI_THREAD (*this, &MixerStrip::comment_changed, src)

	if (src != this) {
		ignore_comment_edit = true;
		if (comment_area) {
			comment_area->get_buffer()->set_text (_route->comment());
		}
		ignore_comment_edit = false;
	}
}

void
RouteUI::comment_editor_done_editing ()
{
	ENSURE_GUI_THREAD (*this, &MixerStrip::comment_editor_done_editing, src)

	string const str = comment_area->get_buffer()->get_text();
	if (str == _route->comment ()) {
		return;
	}

	_route->set_comment (str, this);
}

void
RouteUI::set_route_active (bool a, bool apply_to_selection)
{
	if (apply_to_selection) {
		ARDOUR_UI::instance()->the_editor().get_selection().tracks.foreach_route_ui (boost::bind (&RouteTimeAxisView::set_route_active, _1, a, false));
	} else {
		_route->set_active (a, this);
	}
}

void
RouteUI::toggle_denormal_protection ()
{
	if (denormal_menu_item) {

		bool x;

		ENSURE_GUI_THREAD (*this, &RouteUI::toggle_denormal_protection)

		if ((x = denormal_menu_item->get_active()) != _route->denormal_protection()) {
			_route->set_denormal_protection (x);
		}
	}
}

void
RouteUI::denormal_protection_changed ()
{
	if (denormal_menu_item) {
		denormal_menu_item->set_active (_route->denormal_protection());
	}
}

void
RouteUI::disconnect_input ()
{
	_route->input()->disconnect (this);
}

void
RouteUI::disconnect_output ()
{
	_route->output()->disconnect (this);
}

bool
RouteUI::is_track () const
{
	return boost::dynamic_pointer_cast<Track>(_route) != 0;
}

boost::shared_ptr<Track>
RouteUI::track() const
{
	return boost::dynamic_pointer_cast<Track>(_route);
}

bool
RouteUI::is_audio_track () const
{
	return boost::dynamic_pointer_cast<AudioTrack>(_route) != 0;
}

boost::shared_ptr<AudioTrack>
RouteUI::audio_track() const
{
	return boost::dynamic_pointer_cast<AudioTrack>(_route);
}

bool
RouteUI::is_midi_track () const
{
	return boost::dynamic_pointer_cast<MidiTrack>(_route) != 0;
}

boost::shared_ptr<MidiTrack>
RouteUI::midi_track() const
{
	return boost::dynamic_pointer_cast<MidiTrack>(_route);
}

bool
RouteUI::has_audio_outputs () const
{
	return (_route->n_outputs().n_audio() > 0);
}

string
RouteUI::name() const
{
	return _route->name();
}

void
RouteUI::map_frozen ()
{
	ENSURE_GUI_THREAD (*this, &RouteUI::map_frozen)

 	AudioTrack* at = dynamic_cast<AudioTrack*>(_route.get());

	if (at) {
		switch (at->freeze_state()) {
		case AudioTrack::Frozen:
			rec_enable_button->set_sensitive (false);
			break;
		default:
			rec_enable_button->set_sensitive (true);
			break;
		}
	}
}

void
RouteUI::adjust_latency ()
{
	LatencyDialog dialog (_route->name() + _(" latency"), *(_route->output()), _session->frame_rate(), AudioEngine::instance()->samples_per_cycle());
}

bool
RouteUI::process_save_template_prompter (ArdourPrompter& prompter, const std::string& dir)
{
	std::string path;
	std::string safe_name;
	std::string name;

	prompter.get_result (name, true);

	safe_name = legalize_for_path (name);
	safe_name += template_suffix;

	path = Glib::build_filename (dir, safe_name);

	if (Glib::file_test (path, Glib::FILE_TEST_EXISTS)) {
		bool overwrite = overwrite_file_dialog (prompter,
		                                        _("Confirm Template Overwrite"),
							_("A template already exists with that name. Do you want to overwrite it?"));

		if (!overwrite) {
			return false;
		}
	}

	_route->save_as_template (path, name);

	return true;
}

void
RouteUI::save_as_template ()
{
	std::string dir;

	dir = ARDOUR::user_route_template_directory ();

	if (g_mkdir_with_parents (dir.c_str(), 0755)) {
		error << string_compose (_("Cannot create route template directory %1"), dir) << endmsg;
		return;
	}

	ArdourPrompter prompter (true); // modal

	prompter.set_title (_("Save As Template"));
	prompter.set_prompt (_("Template name:"));
	prompter.add_button (Gtk::Stock::SAVE, Gtk::RESPONSE_ACCEPT);

	bool finished = false;
	while (!finished) {
		switch (prompter.run()) {
		case RESPONSE_ACCEPT:
			finished = process_save_template_prompter (prompter, dir);
			break;
		default:
			finished = true;
			break;
		}
	}
}

void
RouteUI::check_rec_enable_sensitivity ()
{
	if (_session->transport_rolling() && rec_enable_button->active_state() && Config->get_disable_disarm_during_roll()) {
		rec_enable_button->set_sensitive (false);
	} else {
		rec_enable_button->set_sensitive (true);
	}

	update_monitoring_display ();
}

void
RouteUI::parameter_changed (string const & p)
{
	/* this handles RC and per-session parameter changes */

	if (p == "disable-disarm-during-roll") {
		check_rec_enable_sensitivity ();
	} else if (p == "use-monitor-bus" || p == "solo-control-is-listen-control" || p == "listen-position") {
		set_button_names ();
	} else if (p == "auto-input") {
		update_monitoring_display ();
	} else if (p == "blink-rec-arm") {
		if (UIConfiguration::instance().get_blink_rec_arm()) {
			rec_blink_connection.disconnect ();
			rec_blink_connection = Timers::blink_connect (sigc::mem_fun (*this, &RouteUI::blink_rec_display));
		} else {
			rec_blink_connection.disconnect ();
			RouteUI::blink_rec_display(false);
		}
	}
}

void
RouteUI::step_gain_up ()
{
	_route->set_gain (dB_to_coefficient (accurate_coefficient_to_dB (_route->gain_control()->get_value()) + 0.1), this);
}

void
RouteUI::page_gain_up ()
{
	_route->set_gain (dB_to_coefficient (accurate_coefficient_to_dB (_route->gain_control()->get_value()) + 0.5), this);
}

void
RouteUI::step_gain_down ()
{
	_route->set_gain (dB_to_coefficient (accurate_coefficient_to_dB (_route->gain_control()->get_value()) - 0.1), this);
}

void
RouteUI::page_gain_down ()
{
	_route->set_gain (dB_to_coefficient (accurate_coefficient_to_dB (_route->gain_control()->get_value()) - 0.5), this);
}

void
RouteUI::open_remote_control_id_dialog ()
{
	ArdourDialog dialog (_("Remote Control ID"));
	SpinButton* spin = 0;

	dialog.get_vbox()->set_border_width (18);

	if (Config->get_remote_model() == UserOrdered) {
		uint32_t const limit = _session->ntracks() + _session->nbusses () + 4;

		HBox* hbox = manage (new HBox);
		hbox->set_spacing (6);
		hbox->pack_start (*manage (new Label (_("Remote control ID:"))));
		spin = manage (new SpinButton);
		spin->set_digits (0);
		spin->set_increments (1, 10);
		spin->set_range (0, limit);
		spin->set_value (_route->remote_control_id());
		hbox->pack_start (*spin);
		dialog.get_vbox()->pack_start (*hbox);

		dialog.add_button (Stock::CANCEL, RESPONSE_CANCEL);
		dialog.add_button (Stock::APPLY, RESPONSE_ACCEPT);
	} else {
		Label* l = manage (new Label());
		if (_route->is_master() || _route->is_monitor()) {
			l->set_markup (string_compose (_("The remote control ID of %1 is: %2\n\n\n"
							 "The remote control ID of %3 cannot be changed."),
						       Gtkmm2ext::markup_escape_text (_route->name()),
						       _route->remote_control_id(),
						       (_route->is_master() ? _("the master bus") : _("the monitor bus"))));
		} else {
			l->set_markup (string_compose (_("The remote control ID of %5 is: %2\n\n\n"
							 "Remote Control IDs are currently determined by track/bus ordering in %6.\n\n"
							 "%3Use the User Interaction tab of the Preferences window if you want to change this%4"),
						       (is_track() ? _("track") : _("bus")),
						       _route->remote_control_id(),
						       "<span size=\"small\" style=\"italic\">",
						       "</span>",
						       Gtkmm2ext::markup_escape_text (_route->name()),
						       PROGRAM_NAME));
		}
		dialog.get_vbox()->pack_start (*l);
		dialog.add_button (Stock::OK, RESPONSE_CANCEL);
	}

	dialog.show_all ();
	int const r = dialog.run ();

	if (r == RESPONSE_ACCEPT && spin) {
		_route->set_remote_control_id (spin->get_value_as_int ());
	}
}

void
RouteUI::setup_invert_buttons ()
{
	/* remove old invert buttons */
	for (vector<ArdourButton*>::iterator i = _invert_buttons.begin(); i != _invert_buttons.end(); ++i) {
		_invert_button_box.remove (**i);
	}

	_invert_buttons.clear ();

	if (!_route || !_route->input()) {
		return;
	}

	uint32_t const N = _route->input()->n_ports().n_audio ();

	uint32_t const to_add = (N <= _max_invert_buttons) ? N : 1;

	for (uint32_t i = 0; i < to_add; ++i) {
		ArdourButton* b = manage (new ArdourButton);
		b->signal_button_press_event().connect (sigc::mem_fun (*this, &RouteUI::invert_press), false);
		b->signal_button_release_event().connect (sigc::bind (sigc::mem_fun (*this, &RouteUI::invert_release), i), false);

		b->set_name (X_("invert button"));
		if (to_add == 1) {
			if (N > 1) {
				b->set_text (string_compose (X_("Ø (%1)"), N));
			} else {
				b->set_text (X_("Ø"));
			}
		} else {
			b->set_text (string_compose (X_("Ø%1"), i + 1));
		}

		if (N <= _max_invert_buttons) {
			UI::instance()->set_tip (*b, string_compose (_("Left-click to invert (phase reverse) channel %1 of this track.  Right-click to show menu."), i + 1));
		} else {
			UI::instance()->set_tip (*b, _("Click to show a menu of channels for inversion (phase reverse)"));
		}

		_invert_buttons.push_back (b);
		_invert_button_box.pack_start (*b);
	}

	_invert_button_box.set_spacing (1);
	_invert_button_box.show_all ();
}

void
RouteUI::set_invert_button_state ()
{
	uint32_t const N = _route->input()->n_ports().n_audio();
	if (N > _max_invert_buttons) {

		/* One button for many channels; explicit active if all channels are inverted,
		   implicit active if some are, off if none are.
		*/

		ArdourButton* b = _invert_buttons.front ();

		if (_route->phase_invert().count() == _route->phase_invert().size()) {
			b->set_active_state (Gtkmm2ext::ExplicitActive);
		} else if (_route->phase_invert().any()) {
			b->set_active_state (Gtkmm2ext::ImplicitActive);
		} else {
			b->set_active_state (Gtkmm2ext::Off);
		}

	} else {

		/* One button per channel; just set active */

		int j = 0;
		for (vector<ArdourButton*>::iterator i = _invert_buttons.begin(); i != _invert_buttons.end(); ++i, ++j) {
			(*i)->set_active (_route->phase_invert (j));
		}

	}
}

bool
RouteUI::invert_release (GdkEventButton* ev, uint32_t i)
{
	if (ev->button == 1 && i < _invert_buttons.size()) {
		uint32_t const N = _route->input()->n_ports().n_audio ();
		if (N <= _max_invert_buttons) {
			/* left-click inverts phase so long as we have a button per channel */
			_route->set_phase_invert (i, !_invert_buttons[i]->get_active());
			return false;
		}
	}
	return false;
}


bool
RouteUI::invert_press (GdkEventButton* ev)
{
	using namespace Menu_Helpers;

	uint32_t const N = _route->input()->n_ports().n_audio();
	if (N <= _max_invert_buttons && ev->button != 3) {
		/* If we have an invert button per channel, we only pop
		   up a menu on right-click; left click is handled
		   on release.
		*/
		return false;
	}

	delete _invert_menu;
	_invert_menu = new Menu;
	_invert_menu->set_name ("ArdourContextMenu");
	MenuList& items = _invert_menu->items ();

	for (uint32_t i = 0; i < N; ++i) {
		items.push_back (CheckMenuElem (string_compose (X_("Ø%1"), i + 1), sigc::bind (sigc::mem_fun (*this, &RouteUI::invert_menu_toggled), i)));
		Gtk::CheckMenuItem* e = dynamic_cast<Gtk::CheckMenuItem*> (&items.back ());
		++_i_am_the_modifier;
		e->set_active (_route->phase_invert (i));
		--_i_am_the_modifier;
	}

	_invert_menu->popup (0, ev->time);

	return true;
}

void
RouteUI::invert_menu_toggled (uint32_t c)
{
	if (_i_am_the_modifier) {
		return;
	}

	_route->set_phase_invert (c, !_route->phase_invert (c));
}

void
RouteUI::set_invert_sensitive (bool yn)
{
        for (vector<ArdourButton*>::iterator b = _invert_buttons.begin(); b != _invert_buttons.end(); ++b) {
                (*b)->set_sensitive (yn);
        }
}

void
RouteUI::request_redraw ()
{
	if (_route) {
		_route->gui_changed ("track_height", (void *) 0); /* EMIT_SIGNAL */
	}
}

/** The Route's gui_changed signal has been emitted */
void
RouteUI::route_gui_changed (string what_changed)
{
	if (what_changed == "color") {
		if (set_color_from_route () == 0) {
			route_color_changed ();
		}
	}
}

void
RouteUI::track_mode_changed (void)
{
	assert(is_track());
	switch (track()->mode()) {
		case ARDOUR::NonLayered:
		case ARDOUR::Normal:
			rec_enable_button->set_icon (ArdourIcon::RecButton);
			break;
		case ARDOUR::Destructive:
			rec_enable_button->set_icon (ArdourIcon::RecTapeMode);
			break;
	}
	rec_enable_button->queue_draw();
}

/** @return the color that this route should use; it maybe its own,
    or it maybe that of its route group.
*/
Gdk::Color
RouteUI::color () const
{
	RouteGroup* g = _route->route_group ();

	if (g && g->is_color()) {
		Gdk::Color c;
		set_color_from_rgba (c, GroupTabs::group_color (g));
		return c;
	}

	return _color;
}

void
RouteUI::set_showing_sends_to (boost::shared_ptr<Route> send_to)
{
	_showing_sends_to = send_to;
	BusSendDisplayChanged (send_to); /* EMIT SIGNAL */
}

void
RouteUI::bus_send_display_changed (boost::shared_ptr<Route> send_to)
{
	if (_route == send_to) {
		show_sends_button->set_active (true);
		send_blink_connection = Timers::blink_connect (sigc::mem_fun (*this, &RouteUI::send_blink));
	} else {
		show_sends_button->set_active (false);
		send_blink_connection.disconnect ();
	}
}

RouteGroup*
RouteUI::route_group() const
{
	return _route->route_group();
}
