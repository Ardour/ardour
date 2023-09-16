/*
 * Copyright (C) 2005-2006 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2005-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Karsten Wiese <fzuuzf@googlemail.com>
 * Copyright (C) 2006-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2008-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009 Sampo Savolainen <v2@iki.fi>
 * Copyright (C) 2012-2015 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2013-2015 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2014-2017 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2015 André Nusser <andre.nusser@googlemail.com>
 * Copyright (C) 2017 Johannes Mueller <github@johannes-mueller.org>
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

#include <map>
#include <boost/algorithm/string.hpp>

#include <gtkmm/stock.h>

#include "pbd/controllable.h"
#include "pbd/enumwriter.h"

#include "ardour/dB.h"
#include "ardour/route_group.h"
#include "ardour/solo_isolate_control.h"
#include "ardour/vca.h"
#include "ardour/vca_manager.h"
#include "ardour/audio_track.h"
#include "ardour/audio_port.h"
#include "ardour/audioengine.h"
#include "ardour/filename_extensions.h"
#include "ardour/midi_track.h"
#include "ardour/monitor_control.h"
#include "ardour/internal_send.h"
#include "ardour/panner_shell.h"
#include "ardour/polarity_processor.h"
#include "ardour/profile.h"
#include "ardour/phase_control.h"
#include "ardour/send.h"
#include "ardour/route.h"
#include "ardour/session.h"
#include "ardour/session_playlists.h"
#include "ardour/solo_mute_release.h"
#include "ardour/template_utils.h"

#include "gtkmm2ext/colors.h"
#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/doi.h"
#include "gtkmm2ext/utils.h"

#include "widgets/ardour_button.h"
#include "widgets/binding_proxy.h"
#include "widgets/prompter.h"

#include "ardour_ui.h"
#include "editor.h"
#include "group_tabs.h"
#include "gui_object.h"
#include "gui_thread.h"
#include "keyboard.h"
#include "mixer_strip.h"
#include "mixer_ui.h"
#include "patch_change_widget.h"
#include "playlist_selector.h"
#include "plugin_pin_dialog.h"
#include "rgb_macros.h"
#include "route_time_axis.h"
#include "route_ui.h"
#include "save_template_dialog.h"
#include "timers.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace Gtk;
using namespace Gtkmm2ext;
using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;
using namespace ArdourWidgets;
using namespace PBD;
using namespace std;

uint32_t RouteUI::_max_invert_buttons = 3;
PBD::Signal1<void, std::shared_ptr<Route> > RouteUI::BusSendDisplayChanged;
std::weak_ptr<Route> RouteUI::_showing_sends_to;
std::string RouteUI::program_port_prefix;

RouteUI::IOSelectorMap RouteUI::input_selectors;
RouteUI::IOSelectorMap RouteUI::output_selectors;

#define GROUP_ACTION (Config->get_group_override_inverts () ? Controllable::InverseGroup : Controllable::NoGroup)

void
RouteUI::delete_ioselector (IOSelectorMap& m, std::shared_ptr<ARDOUR::Route> r)
{
	if (!r) {
		return;
	}
	IOSelectorMap::iterator i = m.find (r->id ());
	if (i != m.end ()) {
		delete i->second;
		m.erase (i);
	}
}

RouteUI::RouteUI (ARDOUR::Session* sess)
	: monitor_input_button (0)
	, monitor_disk_button (0)
	, mute_menu(0)
	, solo_menu(0)
	, sends_menu(0)
	, playlist_action_menu (0)
	, _playlist_selector(0)
	, _record_menu(0)
	, _comment_window(0)
	, _comment_area(0)
	, _invert_menu(0)
{
	if (program_port_prefix.empty()) {
		// compare to gtk2_ardour/port_group.cc
		string lpn (PROGRAM_NAME);
		boost::to_lower (lpn);
		program_port_prefix = lpn + ":"; // e.g. "ardour:"
	}

	if (sess) {
		assert (_session);
		init ();
	}
}

RouteUI::~RouteUI()
{
	if (_route) {
		ARDOUR_UI::instance()->gui_object_state->remove_node (route_state_id());
	}

	delete_patch_change_dialog ();

	delete_ioselector (input_selectors, _route);
	delete_ioselector (output_selectors, _route);

	_route.reset (); /* drop reference to route, so that it can be cleaned up */
	route_connections.drop_connections ();

	delete solo_menu;
	delete mute_menu;
	delete sends_menu;
	delete monitor_input_button;
	delete monitor_disk_button;
	delete playlist_action_menu;
	delete _record_menu;
	delete _comment_window;
	delete _invert_menu;
	delete _playlist_selector;

	send_blink_connection.disconnect ();
	rec_blink_connection.disconnect ();
}

void
RouteUI::init ()
{
	self_destruct = true;
	_playlist_selector = 0;
	mute_menu = 0;
	solo_menu = 0;
	sends_menu = 0;
	_record_menu = 0;
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
	_step_edit_item = 0;
	_rec_safe_item = 0;
	_ignore_comment_edit = false;
	_i_am_the_modifier = 0;
	_n_polarity_invert = 0;

	setup_invert_buttons ();

	mute_button = manage (new ArdourButton);
	mute_button->set_name ("mute button");
	std::string tip = string_compose( _("Mute this track\n"
							            "%2+Click to Override Group\n"
							            "%1+%3+Click to toggle ALL tracks\n"
							            "%4 for Momentary mute\n"
							            "Right-Click for Context menu")
									  , Keyboard::primary_modifier_short_name(), Keyboard::group_override_event_name(), Keyboard::tertiary_modifier_short_name(), Keyboard::momentary_push_name() );
	UI::instance()->set_tip (mute_button, tip.c_str(), "");

	solo_button = manage (new ArdourButton);
	solo_button->set_name ("solo button");
	solo_button->set_no_show_all (true);

	rec_enable_button = manage (new ArdourButton);
	rec_enable_button->set_name ("record enable button");
	rec_enable_button->set_icon (ArdourIcon::RecButton);
	tip = string_compose( _("Enable Recording on this track\n"
				            "%2+Click to Override group\n"
				            "%1+%3+Click to toggle ALL tracks\n"
				            "Right-Click for Context menu")
						    , Keyboard::primary_modifier_short_name(), Keyboard::group_override_event_name(), Keyboard::tertiary_modifier_short_name(), Keyboard::momentary_push_name() );
	UI::instance()->set_tip (rec_enable_button, tip.c_str(), "");

	if (UIConfiguration::instance().get_blink_rec_arm()) {
		rec_blink_connection = Timers::blink_connect (sigc::mem_fun (*this, &RouteUI::blink_rec_display));
	}

	show_sends_button = manage (new ArdourButton);
	show_sends_button->set_name ("send alert button");
	UI::instance()->set_tip (show_sends_button, _("Show the strips that send to this bus, and control them from the faders"), "");

	monitor_input_button = new ArdourButton (ArdourButton::default_elements);
	monitor_input_button->set_name ("monitor button");
	monitor_input_button->set_text (_("In"));
	UI::instance()->set_tip (monitor_input_button, _("Monitor input"), "");
	monitor_input_button->set_no_show_all (true);

	monitor_disk_button = new ArdourButton (ArdourButton::default_elements);
	monitor_disk_button->set_name ("monitor button");
	monitor_disk_button->set_text (_("Disk"));
	UI::instance()->set_tip (monitor_disk_button, _("Monitor playback"), "");
	monitor_disk_button->set_no_show_all (true);

	_session->SoloChanged.connect (_session_connections, invalidator (*this), boost::bind (&RouteUI::solo_changed_so_update_mute, this), gui_context());
	_session->TransportStateChange.connect (_session_connections, invalidator (*this), boost::bind (&RouteUI::check_rec_enable_sensitivity, this), gui_context());
	_session->RecordStateChanged.connect (_session_connections, invalidator (*this), boost::bind (&RouteUI::session_rec_enable_changed, this), gui_context());
	_session->MonitorBusAddedOrRemoved.connect (_session_connections, invalidator (*this), boost::bind (&RouteUI::update_solo_button, this), gui_context());

	_session->config.ParameterChanged.connect (*this, invalidator (*this), boost::bind (&RouteUI::parameter_changed, this, _1), gui_context());
	Config->ParameterChanged.connect (*this, invalidator (*this), boost::bind (&RouteUI::parameter_changed, this, _1), gui_context());
	UIConfiguration::instance().ParameterChanged.connect (sigc::mem_fun (this, &RouteUI::parameter_changed));

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

	delete_patch_change_dialog ();
	_color_picker.reset ();

	denormal_menu_item = 0;
}

void
RouteUI::self_delete ()
{
	delete this;
}

void
RouteUI::set_session (ARDOUR::Session*s)
{
	SessionHandlePtr::set_session (s);

	if (!s) {
		/* This is needed to clean out IDs of sends, when using output selector
		 * with MixerStrip::_current_delivery.
		 * It's also prudent to hide/destroy input-selectors early, before delayed
		 * self_delete() can do that in the ~RouteUI.
		 */
		for (IOSelectorMap::const_iterator i = input_selectors.begin(); i != input_selectors.end() ; ++i) {
			delete i->second;
		}
		for (IOSelectorMap::const_iterator i = output_selectors.begin(); i != output_selectors.end() ; ++i) {
			delete i->second;
		}
		input_selectors.clear ();
		output_selectors.clear ();
	}
}

void
RouteUI::set_route (std::shared_ptr<Route> rp)
{
	reset ();

	_route = rp;

	if (!_route->presentation_info().color_set()) {
		/* deal with older 4.x color, which was stored in the GUI object state */

		string p = ARDOUR_UI::instance()->gui_object_state->get_string (route_state_id(), X_("color"));

		if (!p.empty()) {

			/* old v4.x or earlier session. Use this information */

			int red, green, blue;
			char colon;

			stringstream ss (p);

			/* old color format version was:

			   16bit value for red:16 bit value for green:16 bit value for blue

			   decode to rgb ..
			*/

			ss >> red;
			ss >> colon;
			ss >> green;
			ss >> colon;
			ss >> blue;

			red >>= 2;
			green >>= 2;
			blue >>= 2;

			_route->presentation_info().set_color (RGBA_TO_UINT (red, green, blue, 255));
		}
	}

	if (set_color_from_route()) {
		if (_route->is_track() && UIConfiguration::instance().get_use_palette_for_new_track ()) {
			set_color (gdk_color_to_rgba (AxisView::round_robin_palette_color ()));
		} else if (!_route->is_track() && UIConfiguration::instance().get_use_palette_for_new_bus ()) {
			set_color (gdk_color_to_rgba (AxisView::round_robin_palette_color ()));
		} else {
			string cp = UIConfiguration::instance().get_stripable_color_palette ();
			Gdk::ArrayHandle_Color gc = ColorSelection::palette_from_string (cp);
			std::vector<Gdk::Color> c (gc);
			set_color (gdk_color_to_rgba (c[0]));
		}
	}

	if (self_destruct) {
		rp->DropReferences.connect (route_connections, invalidator (*this), boost::bind (&RouteUI::self_delete, this), gui_context());
	}

	mute_button->set_controllable (_route->mute_control());
	solo_button->set_controllable (_route->solo_control());

	_route->active_changed.connect (route_connections, invalidator (*this), boost::bind (&RouteUI::route_active_changed, this), gui_context());

	_route->comment_changed.connect (route_connections, invalidator (*this), boost::bind (&RouteUI::comment_changed, this), gui_context());

	_route->mute_control()->Changed.connect (route_connections, invalidator (*this), boost::bind (&RouteUI::update_mute_display, this), gui_context());
	_route->solo_control()->Changed.connect (route_connections, invalidator (*this), boost::bind (&RouteUI::update_solo_display, this), gui_context());
	_route->solo_safe_control()->Changed.connect (route_connections, invalidator (*this), boost::bind (&RouteUI::update_solo_display, this), gui_context());
	_route->solo_isolate_control()->Changed.connect (route_connections, invalidator (*this), boost::bind (&RouteUI::update_solo_display, this), gui_context());
	_route->phase_control()->Changed.connect (route_connections, invalidator (*this), boost::bind (&RouteUI::update_polarity_display, this), gui_context());

	if (is_track()) {
		track()->FreezeChange.connect (*this, invalidator (*this), boost::bind (&RouteUI::map_frozen, this), gui_context());
		track_mode_changed();
	}


	_route->PropertyChanged.connect (route_connections, invalidator (*this), boost::bind (&RouteUI::route_property_changed, this, _1), gui_context());
	_route->presentation_info().PropertyChanged.connect (route_connections, invalidator (*this), boost::bind (&RouteUI::route_gui_changed, this, _1), gui_context ());

	_route->polarity()->ConfigurationChanged.connect (route_connections, invalidator (*this), boost::bind (&RouteUI::setup_invert_buttons, this), gui_context());

	if (_session->writable() && is_track()) {
		std::shared_ptr<Track> t = std::dynamic_pointer_cast<Track>(_route);

		t->rec_enable_control()->Changed.connect (route_connections, invalidator (*this), boost::bind (&RouteUI::route_rec_enable_changed, this), gui_context());
		t->rec_safe_control()->Changed.connect (route_connections, invalidator (*this), boost::bind (&RouteUI::route_rec_enable_changed, this), gui_context());

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
		std::shared_ptr<Track> t = std::dynamic_pointer_cast<Track>(_route);
		t->monitoring_control()->Changed.connect (route_connections, invalidator (*this), boost::bind (&RouteUI::update_monitoring_display, this), gui_context());

		update_monitoring_display ();
	}

	if (_route->triggerbox ()) {
		_route->triggerbox ()->EmptyStatusChanged.connect (route_connections, invalidator (*this), boost::bind (&RouteUI::update_monitoring_display, this), gui_context());
	}

	mute_button->set_can_focus (false);
	solo_button->set_can_focus (false);

	mute_button->show();

	if (_route->is_monitor() || _route->is_master()) {
		solo_button->hide ();
	} else {
		solo_button->show();
	}

	map_frozen ();

	setup_invert_buttons ();

	std::shared_ptr<Route> s = _showing_sends_to.lock ();
	bus_send_display_changed (s);

	update_mute_display ();
	update_solo_display ();
	update_solo_button ();

	if (!UIConfiguration::instance().get_blink_rec_arm()) {
		blink_rec_display(true); // set initial rec-en button state
	}

	check_rec_enable_sensitivity ();
	maybe_add_route_print_mgr ();
	route_color_changed();
	route_gui_changed (PropertyChange (Properties::selected));
}

bool
RouteUI::mute_press (GdkEventButton* ev)
{
	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS ) {
		return true;
	}

	//if this is a binding action, let the ArdourButton handle it
	if (BindingProxy::is_bind_action(ev) )
		return false;

	if (Keyboard::is_context_menu_event (ev)) {

		if (mute_menu == 0){
			build_mute_menu();
		}

		mute_menu->popup(0,ev->time);

		return true;

	} else {

		if (Keyboard::is_momentary_push_event (ev)) {
			// button2-click is "momentary"

			_mute_release = new SoloMuteRelease (_route->mute_control()->muted ());
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

				std::shared_ptr<RouteList> copy (new RouteList);

				*copy = *_session->get_routes ();

				for (RouteList::iterator i = copy->begin(); i != copy->end(); ) {
					if ((*i)->is_master() || (*i)->is_monitor()) {
						i = copy->erase (i);
					} else {
						++i;
					}
				}

				if (_mute_release) {
					_mute_release->set (copy);
				}

				_session->set_controls (route_list_to_control_list (copy, &Stripable::mute_control), _route->muted_by_self() ? 0.0 : 1.0, Controllable::UseGroup);

			} else if (Keyboard::is_group_override_event (ev)) {

				/* Tertiary-button1 inverts the implication of
				   the group being active. If the group is
				   active (for mute), then this modifier means
				   "do not apply to mute". If the group is
				   inactive (for mute), then this modifier
				   means "apply to route". This is all
				   accomplished by passing just the actual
				   route, along with the InverseGroup group
				   control disposition.

				   NOTE: Primary-button2 is MIDI learn.
				*/

				std::shared_ptr<RouteList> rl;

				if (ev->button == 1) {

					rl.reset (new RouteList);
					rl->push_back (_route);

					if (_mute_release) {
						_mute_release->set (rl);
					}

					std::shared_ptr<MuteControl> mc = _route->mute_control();
					mc->start_touch (timepos_t (_session->audible_sample ()));
					_session->set_controls (route_list_to_control_list (rl, &Stripable::mute_control), _route->muted_by_self() ? 0.0 : 1.0, Controllable::InverseGroup);
				}

			} else {

				/* plain click applies change to this route */

				std::shared_ptr<RouteList> rl (new RouteList);
				rl->push_back (_route);

				if (_mute_release) {
					_mute_release->set (rl);
				}

				std::shared_ptr<MuteControl> mc = _route->mute_control();
				mc->start_touch (timepos_t (_session->audible_sample ()));
				mc->set_value (!_route->muted_by_self(), Controllable::UseGroup);
			}
		}
	}

	return false;
}

bool
RouteUI::mute_release (GdkEventButton* /*ev*/)
{
	if (_mute_release) {
		_mute_release->release (_session, true);
		delete _mute_release;
		_mute_release = 0;
	}

	_route->mute_control()->stop_touch (timepos_t (_session->audible_sample ()));

	return false;
}

void
RouteUI::edit_output_configuration ()
{
	std::shared_ptr<Send> send = std::dynamic_pointer_cast<Send>(_current_delivery);
	if (send && !std::dynamic_pointer_cast<InternalSend>(send)) {
		send.reset ();
	}

	PBD::ID id = send ? send->id () : _route->id ();

	if (output_selectors.find (id) == output_selectors.end ()) {
		output_selectors[_route->id ()] =  new IOSelectorWindow (_session, send ? send->output () : _route->output ());
	}

	IOSelectorWindow* w = output_selectors[id];

	if (w->get_visible()) {
		w->get_toplevel()->get_window()->raise();
	} else {
		w->present ();
	}

	//w->set_keep_above (true);
}

void
RouteUI::edit_input_configuration ()
{
	if (input_selectors.find (_route->id ()) == input_selectors.end ()) {
		input_selectors[_route->id ()] = new IOSelectorWindow (_session, _route->input());
	}

	IOSelectorWindow* w = input_selectors[_route->id ()];

	if (w->get_visible()) {
		w->get_toplevel()->get_window()->raise();
	} else {
		w->present ();
	}

	//w->set_keep_above (true);
}

bool
RouteUI::solo_press(GdkEventButton* ev)
{
	/* ignore double/triple clicks */

	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS ) {
		return true;
	}

	//if this is a binding action, let the ArdourButton handle it
	if (BindingProxy::is_bind_action(ev) )
		return false;

	if (Keyboard::is_context_menu_event (ev)) {

		if (solo_menu == 0) {
			build_solo_menu ();
		}

		solo_menu->popup (1, ev->time);

	} else {

		if (Keyboard::is_momentary_push_event (ev)) {

			// button2-click is "momentary"
			_solo_release = new SoloMuteRelease (_route->self_soloed());
		}

		if (ev->button == 1 || Keyboard::is_button2_event (ev)) {

			if (Keyboard::modifier_state_equals (ev->state, Keyboard::ModifierMask (Keyboard::PrimaryModifier|Keyboard::TertiaryModifier))) {

				/* Primary-Tertiary-click applies change to all routes */

				if (_solo_release) {
					_solo_release->set (_session->get_routes ());
				}

				_session->set_controls (route_list_to_control_list (_session->get_routes(), &Stripable::solo_control), !_route->solo_control()->get_value(), Controllable::UseGroup);

			} else if (Keyboard::modifier_state_contains (ev->state, Keyboard::ModifierMask (Keyboard::PrimaryModifier|Keyboard::SecondaryModifier)) || (!_route->self_soloed() && Config->get_exclusive_solo ())) {

				/* Primary-Secondary-click: exclusively solo this track */

				if (_solo_release) {
					_session->prepare_momentary_solo (_solo_release, true, _route);
				} else {
					/* clear solo state */
					_session->prepare_momentary_solo (0, true, _route);
				}

				DisplaySuspender ds;
				_route->solo_control()->set_value (1.0, Controllable::NoGroup);

/*			} else if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {   do not explicitly implement Primary Modifier; this is the default for Momentary */

/*			} else if (Keyboard::modifier_state_equals (ev->state, Keyboard::TertiaryModifier)) {   do not explicitly implement Tertiary Modifier; this is the default for Group-Override */

			} else if (Keyboard::is_group_override_event (ev)) {

				/* Tertiary-button1: override mix group.
				   NOTE: Primary-button2 is MIDI learn.
				*/

				std::shared_ptr<RouteList> rl;

				if (ev->button == 1) {

					/* Tertiary-button1 inverts the implication of
					   the group being active. If the group is
					   active (for solo), then this modifier means
					   "do not apply to solo". If the group is
					   inactive (for mute), then this modifier
					   means "apply to route". This is all
					   accomplished by passing just the actual
					   route, along with the InverseGroup group
					   control disposition.

					   NOTE: Primary-button2 is MIDI learn.
					*/

					rl.reset (new RouteList);
					rl->push_back (_route);

					_session->set_controls (route_list_to_control_list (rl, &Stripable::solo_control), !_route->self_soloed(), Controllable::InverseGroup);
				}

				delete _solo_release;
				_solo_release = 0;

			} else {

				/* click: solo this route */

				std::shared_ptr<RouteList> rl (new RouteList);
				rl->push_back (route());

				if (_solo_release) {
					_solo_release->set (rl);
				}

				_session->set_controls (route_list_to_control_list (rl, &Stripable::solo_control), !_route->self_soloed(), Controllable::UseGroup);
			}
		}
	}

	return false;
}

bool
RouteUI::solo_release (GdkEventButton* /*ev*/)
{
	if (_solo_release) {
		_solo_release->release (_session, false);
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
	if (BindingProxy::is_bind_action(ev) )
		return false;

	if (!ARDOUR_UI_UTILS::engine_is_running ()) {
		return false;
	}

	if (is_midi_track()) {

		/* rec-enable button exits from step editing, but not context click */

		if (!Keyboard::is_context_menu_event (ev) && midi_track()->step_editing()) {
			midi_track()->set_step_editing (false);
			return false;
		}
	}

	if (is_track() && rec_enable_button) {

		if (Keyboard::is_momentary_push_event (ev)) {

			//rec arm does not have a momentary mode
			return false;

		} else if (Keyboard::modifier_state_equals (ev->state, Keyboard::ModifierMask (Keyboard::PrimaryModifier|Keyboard::TertiaryModifier))) {

			_session->set_controls (route_list_to_control_list (_session->get_routes(), &Stripable::rec_enable_control), !track()->rec_enable_control()->get_value(), Controllable::NoGroup);

		} else if (Keyboard::is_group_override_event (ev)) {

			/* Tertiary-button1 applies change to the route group (even if it is not active)
			   NOTE: Primary-button2 is MIDI learn.
			*/

			if (ev->button == 1) {

				std::shared_ptr<RouteList> rl;

				rl.reset (new RouteList);
				rl->push_back (_route);

				_session->set_controls (route_list_to_control_list (rl, &Stripable::rec_enable_control), !track()->rec_enable_control()->get_value(), GROUP_ACTION);
			}

		} else if (Keyboard::is_context_menu_event (ev)) {

			/* do this on release */

		} else {

			std::shared_ptr<Track> trk = track();
			trk->rec_enable_control()->set_value (!trk->rec_enable_control()->get_value(), Controllable::UseGroup);
		}
	}

	return false;
}

void
RouteUI::update_monitoring_display ()
{
	if (!_route) {
		return;
	}

	std::shared_ptr<Track> t = std::dynamic_pointer_cast<Track>(_route);

	if (!t) {
		return;
	}

	MonitorState ms = t->monitoring_state();

	if (t->monitoring_control()->monitoring_choice() & MonitorInput) {
		monitor_input_button->set_active_state (Gtkmm2ext::ExplicitActive);
	} else {
		if (ms & MonitoringInput) {
			monitor_input_button->set_active_state (Gtkmm2ext::ImplicitActive);
		} else {
			monitor_input_button->unset_active_state ();
		}
	}

	if (t->monitoring_control()->monitoring_choice() & MonitorDisk) {
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

	std::shared_ptr<Track> t = std::dynamic_pointer_cast<Track>(_route);

	if (!t) {
		return true;
	}

	MonitorChoice mc;

	if (t->monitoring_control()->monitoring_choice() & monitor_choice) {
		mc = MonitorChoice (t->monitoring_control()->monitoring_choice() & ~monitor_choice);
	} else {
		mc = MonitorChoice (t->monitoring_control()->monitoring_choice() | monitor_choice);
	}

	if (Keyboard::modifier_state_equals (ev->state, Keyboard::ModifierMask (Keyboard::PrimaryModifier|Keyboard::TertiaryModifier))) {
		/* Primary-Tertiary-click applies change to all routes */
		std::shared_ptr<RouteList const> rl = _session->get_routes ();
		_session->set_controls (route_list_to_control_list (rl, &Stripable::monitoring_control), (double) mc, Controllable::NoGroup);
	} else if (Keyboard::is_group_override_event (ev)) {
		/* Tertiary-click overrides group */
		std::shared_ptr<RouteList> rl (new RouteList);
		rl->push_back (route());
		_session->set_controls (route_list_to_control_list (rl, &Stripable::monitoring_control), (double) mc, GROUP_ACTION);
	} else {
		std::shared_ptr<RouteList> rl (new RouteList);
		rl->push_back (route());
		_session->set_controls (route_list_to_control_list (rl, &Stripable::monitoring_control), (double) mc, Controllable::UseGroup);
	}

	return false;
}

void
RouteUI::build_record_menu ()
{
	if (!_record_menu) {
		_record_menu = new Menu;
		_record_menu->set_name ("ArdourContextMenu");
		using namespace Menu_Helpers;
		MenuList& items = _record_menu->items();

		items.push_back (CheckMenuElem (_("Rec-Safe"), sigc::mem_fun (*this, &RouteUI::toggle_rec_safe)));
		_rec_safe_item = dynamic_cast<Gtk::CheckMenuItem*> (&items.back());

		if (is_midi_track()) {
			items.push_back (SeparatorElem());
			items.push_back (CheckMenuElem (_("Step Entry"), sigc::mem_fun (*this, &RouteUI::toggle_step_edit)));
			_step_edit_item = dynamic_cast<Gtk::CheckMenuItem*> (&items.back());
		}
	}

	if (_step_edit_item) {
		if (track()->rec_enable_control()->get_value()) {
			_step_edit_item->set_sensitive (false);
		}
		_step_edit_item->set_active (midi_track()->step_editing());
	}
	if (_rec_safe_item) {
		_rec_safe_item->set_sensitive (!_route->rec_enable_control()->get_value());
		_rec_safe_item->set_active (_route->rec_safe_control()->get_value());
	}
}

void
RouteUI::toggle_step_edit ()
{
	if (!is_midi_track() || track()->rec_enable_control()->get_value()) {
		return;
	}

	midi_track()->set_step_editing (_step_edit_item->get_active());
}

void
RouteUI::toggle_rec_safe ()
{
	std::shared_ptr<AutomationControl> rs = _route->rec_safe_control();

	if (!rs) {
		return;
	}

	/* This check is made inside the control too, but dong it here can't
	 * hurt.
	 */

	if (_route->rec_enable_control()->get_value()) {
		return;
	}

	rs->set_value (_rec_safe_item->get_active (), Controllable::UseGroup);
}

void
RouteUI::step_edit_changed (bool yn)
{
	if (yn) {
		if (rec_enable_button) {
			rec_enable_button->set_active_state (Gtkmm2ext::ExplicitActive);
		}

		start_step_editing ();

		if (_step_edit_item) {
			_step_edit_item->set_active (true);
		}

	} else {

		if (rec_enable_button) {
			rec_enable_button->unset_active_state ();
		}

		stop_step_editing ();

		if (_step_edit_item) {
			_step_edit_item->set_active (false);
		}
	}
}

bool
RouteUI::rec_enable_release (GdkEventButton* ev)
{
	if (Keyboard::is_context_menu_event (ev)) {
		build_record_menu ();
		if (_record_menu) {
			_record_menu->popup (1, ev->time);
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

	if (!is_foldbackbus ()) {
		items.push_back (
				MenuElem(_("Assign all tracks (prefader)"), sigc::bind (sigc::mem_fun (*this, &RouteUI::create_sends), PreFader, false))
				);

		items.push_back (
				MenuElem(_("Assign all tracks and busses (prefader)"), sigc::bind (sigc::mem_fun (*this, &RouteUI::create_sends), PreFader, true))
				);

		items.push_back (
				MenuElem(_("Assign all tracks (postfader)"), sigc::bind (sigc::mem_fun (*this, &RouteUI::create_sends), PostFader, false))
				);

		items.push_back (
				MenuElem(_("Assign all tracks and busses (postfader)"), sigc::bind (sigc::mem_fun (*this, &RouteUI::create_sends), PostFader, true))
				);
	}

	items.push_back (
		MenuElem(_("Assign selected tracks (prefader)"), sigc::bind (sigc::mem_fun (*this, &RouteUI::create_selected_sends), PreFader, false))
		);

	if (!is_foldbackbus ()) {
		items.push_back (
				MenuElem(_("Assign selected tracks and busses (prefader)"), sigc::bind (sigc::mem_fun (*this, &RouteUI::create_selected_sends), PreFader, true)));
	}

	items.push_back (
		MenuElem(_("Assign selected tracks (postfader)"), sigc::bind (sigc::mem_fun (*this, &RouteUI::create_selected_sends), PostFader, false))
		);

	if (!is_foldbackbus ()) {
		items.push_back (
				MenuElem(_("Assign selected tracks and busses (postfader)"), sigc::bind (sigc::mem_fun (*this, &RouteUI::create_selected_sends), PostFader, true))
				);
	}

	items.push_back (SeparatorElem());

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
	std::shared_ptr<RouteList> rlist (new RouteList);
	TrackSelection& selected_tracks (ARDOUR_UI::instance()->the_editor().get_selection().tracks);

	for (TrackSelection::iterator i = selected_tracks.begin(); i != selected_tracks.end(); ++i) {
		RouteTimeAxisView* rtv;
		RouteUI* rui;
		if ((rtv = dynamic_cast<RouteTimeAxisView*>(*i)) != 0) {
			if ((rui = dynamic_cast<RouteUI*>(rtv)) != 0) {
				if (include_buses || std::dynamic_pointer_cast<AudioTrack>(rui->route())) {
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
	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS) {
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

		} else if (ev->button == 1) {

			std::shared_ptr<Route> s = _showing_sends_to.lock ();

			if (s == _route) {
				set_showing_sends_to (std::shared_ptr<Route> ());
				Mixer_UI::instance()->show_spill (std::shared_ptr<ARDOUR::Stripable>());
			} else {
				set_showing_sends_to (_route);
				Mixer_UI::instance()->show_spill (_route);
			}
		}
		return true;
	}

	return false;
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
RouteUI::solo_active_state (std::shared_ptr<Stripable> s)
{
	std::shared_ptr<SoloControl> sc = s->solo_control();

	if (!sc) {
		return Gtkmm2ext::Off;
	}

	if (!sc->can_solo()) {
		return Gtkmm2ext::Off;
	}


	if (sc->self_soloed()) {
		return Gtkmm2ext::ExplicitActive;
	} else if (sc->soloed_by_others()) {
		return Gtkmm2ext::ImplicitActive;
	} else {
		return Gtkmm2ext::Off;
	}
}

Gtkmm2ext::ActiveState
RouteUI::solo_isolate_active_state (std::shared_ptr<Stripable> s)
{
	std::shared_ptr<SoloIsolateControl> sc = s->solo_isolate_control();

	if (!sc) {
		return Gtkmm2ext::Off;
	}

	if (s->is_master() || s->is_monitor()) {
		return Gtkmm2ext::Off;
	}

	if (sc->solo_isolated()) {
		return Gtkmm2ext::ExplicitActive;
	} else {
		return Gtkmm2ext::Off;
	}
}

Gtkmm2ext::ActiveState
RouteUI::solo_safe_active_state (std::shared_ptr<Stripable> s)
{
	std::shared_ptr<SoloSafeControl> sc = s->solo_safe_control();

	if (!sc) {
		return Gtkmm2ext::Off;
	}

	if (s->is_master() || s->is_monitor()) {
		return Gtkmm2ext::Off;
	}

	if (sc->solo_safe()) {
		return Gtkmm2ext::ExplicitActive;
	} else {
		return Gtkmm2ext::Off;
	}
}

void
RouteUI::update_solo_display ()
{
	bool yn = _route->solo_safe_control()->solo_safe ();

	if (solo_safe_check && solo_safe_check->get_active() != yn) {
		solo_safe_check->set_active (yn);
	}

	yn = _route->solo_isolate_control()->solo_isolated ();

	if (solo_isolated_check && solo_isolated_check->get_active() != yn) {
		solo_isolated_check->set_active (yn);
	}

	set_button_names ();

	if (solo_isolated_led) {
		if (_route->solo_isolate_control()->solo_isolated()) {
			solo_isolated_led->set_active_state (Gtkmm2ext::ExplicitActive);
		} else {
			solo_isolated_led->unset_active_state ();
		}
	}

	if (solo_safe_led) {
		if (_route->solo_safe_control()->solo_safe()) {
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
RouteUI::mute_active_state (Session*, std::shared_ptr<Stripable> s)
{
	std::shared_ptr<MuteControl> mc = s->mute_control();

	if (s->is_monitor()) {
		return Gtkmm2ext::Off;
	}

	if (!mc) {
		return Gtkmm2ext::Off;
	}

	if (Config->get_show_solo_mutes() && !Config->get_solo_control_is_listen_control ()) {

		if (mc->muted_by_self ()) {
			/* full mute */
			return Gtkmm2ext::ExplicitActive;
		} else if (mc->muted_by_others_soloing () || mc->muted_by_masters ()) {
			/* this will reflect both solo mutes AND master mutes */
			return Gtkmm2ext::ImplicitActive;
		} else {
			/* no mute at all */
			return Gtkmm2ext::Off;
		}

	} else {

		if (mc->muted_by_self()) {
			/* full mute */
			return Gtkmm2ext::ExplicitActive;
		} else if (mc->muted_by_masters ()) {
			/* this shows only master mutes, not mute-by-others-soloing */
			return Gtkmm2ext::ImplicitActive;
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
	blink_rec_display (true);  //this lets the button change "immediately" rather than wait for the next blink
}

void
RouteUI::session_rec_enable_changed ()
{
	blink_rec_display (true);  //this lets the button change "immediately" rather than wait for the next blink
}

void
RouteUI::blink_rec_display (bool blinkOn)
{
	if (!rec_enable_button || !_route) {
		return;
	}

	if (std::dynamic_pointer_cast<Send>(_current_delivery)) {
		return;
	}

	if (!is_track()) {
		return;
	}

	if (track()->rec_enable_control()->get_value()) {
		switch (_session->record_status ()) {
			case Session::Recording:
				rec_enable_button->set_active_state (Gtkmm2ext::ExplicitActive);
				break;

			case Session::Disabled:
			case Session::Enabled:
				if (UIConfiguration::instance().get_blink_rec_arm()) {
					rec_enable_button->set_active_state ( blinkOn ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off );
				} else {
					rec_enable_button->set_active_state ( ImplicitActive );
				}
				break;
		}

		if (_step_edit_item) {
			_step_edit_item->set_sensitive (false);
		}

	} else {
		rec_enable_button->unset_active_state ();

		if (_step_edit_item) {
			_step_edit_item->set_sensitive (true);
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
	check->set_active (_route->solo_isolate_control()->solo_isolated());
	check->signal_toggled().connect (sigc::bind (sigc::mem_fun (*this, &RouteUI::toggle_solo_isolated), check));
	items.push_back (CheckMenuElem(*check));
	solo_isolated_check = dynamic_cast<Gtk::CheckMenuItem*>(&items.back());
	check->show_all();

	check = new Gtk::CheckMenuItem(_("Solo Safe (Locked)"));
	check->set_active (_route->solo_safe_control()->solo_safe());
	check->signal_toggled().connect (sigc::bind (sigc::mem_fun (*this, &RouteUI::toggle_solo_safe), check));
	items.push_back (CheckMenuElem(*check));
	solo_safe_check = dynamic_cast<Gtk::CheckMenuItem*>(&items.back());
	check->show_all();
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

	_route->mute_points_changed.connect (route_connections, invalidator (*this), boost::bind (&RouteUI::muting_change, this), gui_context());
}

void
RouteUI::init_mute_menu(MuteMaster::MutePoint mp, Gtk::CheckMenuItem* check)
{
	check->set_active (_route->mute_control()->mute_points() & mp);
}

void
RouteUI::toggle_mute_menu(MuteMaster::MutePoint mp, Gtk::CheckMenuItem* check)
{
	if (check->get_active()) {
		_route->mute_control()->set_mute_points (MuteMaster::MutePoint (_route->mute_control()->mute_points() | mp));
	} else {
		_route->mute_control()->set_mute_points (MuteMaster::MutePoint (_route->mute_control()->mute_points() & ~mp));
	}
}

void
RouteUI::muting_change ()
{
	ENSURE_GUI_THREAD (*this, &RouteUI::muting_change)

	bool yn;
	MuteMaster::MutePoint current = _route->mute_control()->mute_points ();

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
	bool model = _route->solo_isolate_control()->solo_isolated();

	/* called BEFORE the view has changed */

	if (ev->button == 1) {
		if (Keyboard::modifier_state_equals (ev->state, Keyboard::ModifierMask (Keyboard::PrimaryModifier|Keyboard::TertiaryModifier))) {

			if (model) {
				/* disable isolate for all routes */
				_session->set_controls (route_list_to_control_list (_session->get_routes(), &Stripable::solo_isolate_control), 0.0, Controllable::NoGroup);
			} else {
				/* enable isolate for all routes */
				_session->set_controls (route_list_to_control_list (_session->get_routes(), &Stripable::solo_isolate_control), 1.0, Controllable::NoGroup);
			}

		} else {

			if (model == view) {

				/* flip just this route */

				std::shared_ptr<RouteList> rl (new RouteList);
				rl->push_back (_route);
				_session->set_controls (route_list_to_control_list (rl, &Stripable::solo_isolate_control), view ? 0.0 : 1.0, Controllable::NoGroup);
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
	bool model = _route->solo_safe_control()->solo_safe();

	if (ev->button == 1) {
		if (Keyboard::modifier_state_equals (ev->state, Keyboard::ModifierMask (Keyboard::PrimaryModifier|Keyboard::TertiaryModifier))) {
			std::shared_ptr<RouteList const> rl (_session->get_routes());
			if (model) {
				/* disable solo safe for all routes */
				DisplaySuspender ds;
				for (auto const& i : *rl) {
					i->solo_safe_control()->set_value (0.0, Controllable::NoGroup);
				}
			} else {
				/* enable solo safe for all routes */
				DisplaySuspender ds;
				for (auto const& i : *rl) {
					i->solo_safe_control()->set_value (1.0, Controllable::NoGroup);
				}
			}
		}
		else {
			if (model == view) {
				/* flip just this route */
				_route->solo_safe_control()->set_value (view ? 0.0 : 1.0, Controllable::NoGroup);
			}
		}
	}

	return false;
}

void
RouteUI::toggle_solo_isolated (Gtk::CheckMenuItem* check)
{
	bool view = check->get_active();
	bool model = _route->solo_isolate_control()->solo_isolated();

	/* called AFTER the view has changed */

	if (model != view) {
		_route->solo_isolate_control()->set_value (view ? 1.0 : 0.0, Controllable::UseGroup);
	}
}

void
RouteUI::toggle_solo_safe (Gtk::CheckMenuItem* check)
{
	_route->solo_safe_control()->set_value (check->get_active() ? 1.0 : 0.0, Controllable::UseGroup);
}

void
RouteUI::delete_patch_change_dialog ()
{
	if (!_route) {
		return;
	}
	delete _route->patch_selector_dialog ();
	_route->set_patch_selector_dialog (0);
}

PatchChangeGridDialog*
RouteUI::patch_change_dialog () const
{
	return _route->patch_selector_dialog ();
}

void
RouteUI::select_midi_patch ()
{
	if (patch_change_dialog ()) {
		patch_change_dialog()->present ();
		return;
	}

	/* note: RouteTimeAxisView is resoponsible to updating
	 * the Dialog (PatchChangeGridDialog::refresh())
	 * when the midnam model changes.
	 */
	PatchChangeGridDialog* d = new PatchChangeGridDialog (_route);
	_route->set_patch_selector_dialog (d);
	d->present ();
}

/** Ask the user to choose a colour, and then apply that color to my route */
void
RouteUI::choose_color ()
{
	_color_picker.popup (_route);
}

/** Set the route's own color.  This may not be used for display if
 *  the route is in a group which shares its color with its routes.
 */
void
RouteUI::set_color (uint32_t c)
{
	_route->presentation_info().set_color (c);
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
	if (_route->presentation_info().color_set()) {
		return 0; /* nothing to do */
	}

	return 1; /* pick a color */
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
	ArdourWidgets::Prompter name_prompter (true);
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
RouteUI::toggle_comment_editor ()
{
	if (_comment_window && _comment_window->get_visible ()) {
		_comment_window->hide ();
	} else {
		open_comment_editor ();
	}
}


void
RouteUI::open_comment_editor ()
{
	if (_comment_window == 0) {
		setup_comment_editor ();
	}

	string title;
	title = _route->name();
	title += _(": comment editor");

	_comment_window->set_title (title);
	_comment_window->present();
}

void
RouteUI::setup_comment_editor ()
{
	_comment_window = new ArdourWindow (""); // title will be reset to show route
	_comment_window->set_skip_taskbar_hint (true);
	_comment_window->signal_hide().connect (sigc::mem_fun(*this, &MixerStrip::comment_editor_done_editing));
	_comment_window->set_default_size (400, 200);

	_comment_area = manage (new TextView());
	_comment_area->set_name ("MixerTrackCommentArea");
	_comment_area->set_wrap_mode (WRAP_WORD);
	_comment_area->set_editable (true);
	_comment_area->get_buffer()->set_text (_route->comment());
	_comment_area->show ();

	_comment_window->add (*_comment_area);
}

void
RouteUI::comment_changed ()
{
	_ignore_comment_edit = true;
	if (_comment_area) {
		_comment_area->get_buffer()->set_text (_route->comment());
	}
	_ignore_comment_edit = false;
}

void
RouteUI::comment_editor_done_editing ()
{
	ENSURE_GUI_THREAD (*this, &MixerStrip::comment_editor_done_editing, src)

	string const str = _comment_area->get_buffer()->get_text();
	if (str == _route->comment ()) {
		return;
	}

	_route->set_comment (str, this);
}

void
RouteUI::set_route_active (bool a, bool apply_to_selection)
{
	if (apply_to_selection) {
		ARDOUR_UI::instance()->the_editor().get_selection().tracks.foreach_route_ui (boost::bind (&RouteUI::set_route_active, _1, a, false));
	} else if (!is_master ()
#ifdef MIXBUS
		         && !_route->mixbus()
#endif
			) {
		_route->set_active (a, this);
	}
}

void
RouteUI::duplicate_selected_routes ()
{
	ARDOUR_UI::instance()->start_duplicate_routes();
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
	return std::dynamic_pointer_cast<Track>(_route) != 0;
}

bool
RouteUI::is_master () const
{
	return _route && _route->is_master ();
}

bool
RouteUI::is_foldbackbus () const
{
	return _route && _route->is_foldbackbus ();
}

std::shared_ptr<Track>
RouteUI::track() const
{
	return std::dynamic_pointer_cast<Track>(_route);
}

bool
RouteUI::is_audio_track () const
{
	return std::dynamic_pointer_cast<AudioTrack>(_route) != 0;
}

std::shared_ptr<AudioTrack>
RouteUI::audio_track() const
{
	return std::dynamic_pointer_cast<AudioTrack>(_route);
}

bool
RouteUI::is_midi_track () const
{
	return std::dynamic_pointer_cast<MidiTrack>(_route) != 0;
}

std::shared_ptr<MidiTrack>
RouteUI::midi_track() const
{
	return std::dynamic_pointer_cast<MidiTrack>(_route);
}

bool
RouteUI::has_audio_outputs () const
{
	return (_route->n_outputs().n_audio() > 0);
}

void
RouteUI::map_frozen ()
{
	ENSURE_GUI_THREAD (*this, &RouteUI::map_frozen)

	AudioTrack* at = dynamic_cast<AudioTrack*>(_route.get());

	if (at) {
		check_rec_enable_sensitivity ();
	}
}

void
RouteUI::save_as_template_dialog_response (int response, SaveTemplateDialog* d)
{
	if (response == RESPONSE_ACCEPT) {
		const string name = d->get_template_name ();
		const string desc = d->get_description ();
		const string path = Glib::build_filename(ARDOUR::user_route_template_directory (), legalize_for_path (name) + ARDOUR::template_suffix);

		if (Glib::file_test (path, Glib::FILE_TEST_EXISTS)) { /* file already exists. */
			bool overwrite = overwrite_file_dialog (*d,
								_("Confirm Template Overwrite"),
								_("A template already exists with that name. Do you want to overwrite it?"));

			if (!overwrite) {
				d->show ();
				return;
			}
		}
		_route->save_as_template (path, name, desc);
	}

	delete d;
}

void
RouteUI::save_as_template ()
{
	const std::string dir = ARDOUR::user_route_template_directory ();

	if (g_mkdir_with_parents (dir.c_str(), 0755)) {
		error << string_compose (_("Cannot create template directory %1"), dir) << endmsg;
		return;
	}

	SaveTemplateDialog* d = new SaveTemplateDialog (_route->name(), _route->comment());
	d->signal_response().connect (sigc::bind (sigc::mem_fun (*this, &RouteUI::save_as_template_dialog_response), d));
	d->show ();
}

void
RouteUI::check_rec_enable_sensitivity ()
{
	if (!rec_enable_button) {
		assert (0); // This should not happen
		return;
	}
	if (!_session->writable()) {
		rec_enable_button->set_sensitive (false);
		return;
	}

	if (_session->transport_rolling() && rec_enable_button->active_state() && Config->get_disable_disarm_during_roll()) {
		rec_enable_button->set_sensitive (false);
	} else if (is_audio_track ()  && track()->freeze_state() == AudioTrack::Frozen) {
		rec_enable_button->set_sensitive (false);
	} else {
		rec_enable_button->set_sensitive (true);
	}
	if (_route && _route->rec_safe_control () && _route->rec_safe_control()->get_value()) {
		rec_enable_button->set_visual_state (Gtkmm2ext::VisualState (solo_button->visual_state() | Gtkmm2ext::Insensitive));
	} else {
		rec_enable_button->set_visual_state (Gtkmm2ext::VisualState (solo_button->visual_state() & ~Gtkmm2ext::Insensitive));
	}
	update_monitoring_display ();
}

void
RouteUI::update_solo_button ()
{
	set_button_names ();
	std::string tip;

	if (Config->get_solo_control_is_listen_control()) {
			tip = string_compose( _("Listen to this track\n"
			                                  "%2+Click to Override Group\n"
			                                  "%1+%3+Click to toggle ALL tracks\n"
			                                  "%4 for Momentary listen\n"
			                                  "Right-Click for Context menu")
			                                  , Keyboard::primary_modifier_short_name(), Keyboard::group_override_event_name(), Keyboard::tertiary_modifier_short_name(), Keyboard::momentary_push_name() );
	} else {
			tip = string_compose( _("Solo this track\n"
			                                  "%2+Click to Override Group\n"
			                                  "%1+%5+Click for Exclusive solo\n"
			                                  "%1+%3+Click to toggle ALL tracks\n"
			                                  "%4 for Momentary solo\n"
			                                  "Right-Click for Context menu")
			                                  , Keyboard::primary_modifier_short_name(), Keyboard::group_override_event_name(), Keyboard::tertiary_modifier_short_name(), Keyboard::momentary_push_name(), Keyboard::secondary_modifier_short_name() );
	}
	UI::instance()->set_tip (solo_button, tip.c_str());
}

void
RouteUI::parameter_changed (string const & p)
{
	/* this handles RC and per-session parameter changes */

	if (p == "disable-disarm-during-roll") {
		check_rec_enable_sensitivity ();
	} else if (p == "solo-control-is-listen-control" || p == "listen-position") {
		update_solo_button ();
	} else if (p == "session-monitoring") {
		update_monitoring_display ();
	} else if (p == "auto-input") {
		update_monitoring_display ();
	} else if (p == "triggerbox-overrides-disk-monitoring") {
		update_monitoring_display ();
	} else if (p == "record-mode") {
		update_monitoring_display ();
	} else if (p == "auto-input-does-talkback") {
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
RouteUI::setup_invert_buttons ()
{
	uint32_t N = _route ? _route->phase_control()->size() : 0;

	std::shared_ptr<Send> send = std::dynamic_pointer_cast<Send>(_current_delivery);
	send_connections.drop_connections ();
	if (send) {
		std::shared_ptr<AutomationControl> ac = send->polarity_control ();
		if (ac) {
			N = 1;
			ac->Changed.connect (send_connections, invalidator (*this), boost::bind (&RouteUI::update_polarity_display, this), gui_context());
			if (ac->alist ()) {
				ac->alist()->automation_state_changed.connect (send_connections, invalidator (*this), boost::bind (&RouteUI::update_phase_invert_sensitivty, this), gui_context());
				update_phase_invert_sensitivty ();
			}
		} else {
			N = 0;
		}
	}

	if (_n_polarity_invert == N) {
		/* buttons are already setup for this strip, but we should still set the values */
		update_polarity_display ();
		update_polarity_tooltips ();
		return;
	}
	_n_polarity_invert = N;

	/* remove old invert buttons */
	for (vector<ArdourButton*>::iterator i = _invert_buttons.begin(); i != _invert_buttons.end(); ++i) {
		invert_button_box.remove (**i);
	}

	_invert_buttons.clear ();

	if (N == 0) {
		return;
	}

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

		_invert_buttons.push_back (b);
		invert_button_box.pack_start (*b);
	}

	invert_button_box.set_spacing (1);
	invert_button_box.show_all ();

	update_polarity_display ();
	update_polarity_tooltips ();
}

void
RouteUI::update_polarity_display ()
{
	std::shared_ptr<Send> send = std::dynamic_pointer_cast<Send>(_current_delivery);
	if (send) {
		if (send->polarity_control()) {
			ArdourButton* b = _invert_buttons.front ();
			b->set_active_state (send->polarity_control()->get_value () > 0 ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
		}
		return;
	}

	if (!_route) {
		return;
	}

	uint32_t const N = _route->phase_control()->size();
	if (N > _max_invert_buttons) {

		/* One button for many channels; explicit active if all channels are inverted,
		   implicit active if some are, off if none are.
		*/

		ArdourButton* b = _invert_buttons.front ();

		if (_route->phase_control()->count() == _route->phase_control()->size()) {
			b->set_active_state (Gtkmm2ext::ExplicitActive);
		} else if (_route->phase_control()->any()) {
			b->set_active_state (Gtkmm2ext::ImplicitActive);
		} else {
			b->set_active_state (Gtkmm2ext::Off);
		}

	} else {

		/* One button per channel; just set active */

		int j = 0;
		for (vector<ArdourButton*>::iterator i = _invert_buttons.begin(); i != _invert_buttons.end(); ++i, ++j) {
			(*i)->set_active (_route->phase_control()->inverted (j));
		}

	}
}
void
RouteUI::update_polarity_tooltips ()
{
	std::shared_ptr<Send> send = std::dynamic_pointer_cast<Send>(_current_delivery);
	int i = 0;
	for (auto const& b : _invert_buttons) {
		if (send) {
			UI::instance()->set_tip (*b, _("Click to invert polarity of all send channels"));
		} else if (_n_polarity_invert <= _max_invert_buttons) {
			UI::instance()->set_tip (*b, string_compose (_("Left-click to invert polarity of channel %1 of this track. Right-click to show menu."), ++i));
		} else {
			UI::instance()->set_tip (*b, _("Click to show a menu of channels to invert polarity"));
		}
	}
}

bool
RouteUI::invert_release (GdkEventButton* ev, uint32_t i)
{
	if (ev->button == 1 && i < _invert_buttons.size()) {
		uint32_t const N = _route->phase_control()->size();
		if (N <= _max_invert_buttons) {
			std::shared_ptr<Send> send = std::dynamic_pointer_cast<Send>(_current_delivery);
			if (send) {
				send->polarity_control ()->set_value (_invert_buttons[i]->get_active() ? 0 : 1, Controllable::NoGroup);
				return false;
			}
			/* left-click inverts phase so long as we have a button per channel */
			_route->phase_control()->set_phase_invert (i, !_invert_buttons[i]->get_active());
			return false;
		}
	}
	return false;
}

bool
RouteUI::invert_press (GdkEventButton* ev)
{
	using namespace Menu_Helpers;

	uint32_t const N = _route->phase_control()->size();
	if (N <= _max_invert_buttons && ev->button != 3) {
		/* If we have an invert button per channel, we only pop
		   up a menu on right-click; left click is handled
		   on release.
		*/
		return false;
	}

	if (std::dynamic_pointer_cast<Send>(_current_delivery)) {
		/* do not show context menu for send polarity */
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
		e->set_active (_route->phase_control()->inverted (i));
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

	_route->phase_control()->set_phase_invert (c, !_route->phase_control()->inverted (c));
}

void
RouteUI::update_phase_invert_sensitivty ()
{
	bool yn = false;
	std::shared_ptr<Send> send = std::dynamic_pointer_cast<Send>(_current_delivery);
	if (send) {
		std::shared_ptr<AutomationControl> ac = send->polarity_control ();
		if (ac) {
			yn = (ac->alist()->automation_state() & Play) == 0;
		}
	} else if (_route) {
		yn = _route->active () || ARDOUR::Profile->get_mixbus();
	}

	for (vector<ArdourButton*>::iterator b = _invert_buttons.begin(); b != _invert_buttons.end(); ++b) {
		(*b)->set_sensitive (yn);
	}
}

/** The Route's gui_changed signal has been emitted */
void
RouteUI::route_gui_changed (PropertyChange const& what_changed)
{
	if (what_changed.contains (Properties::color)) {
		if (set_color_from_route () == 0) {
			route_color_changed ();
		}
	}
}

void
RouteUI::track_mode_changed (void)
{
	assert(is_track());
	rec_enable_button->set_icon (ArdourIcon::RecButton);
	rec_enable_button->queue_draw();
}

/** @return the color that this route should use; it maybe its own,
 *  or it maybe that of its route group.
 */
Gdk::Color
RouteUI::route_color () const
{
	Gdk::Color c;
	RouteGroup* g = _route->route_group ();
	string p;

	if (g && g->is_color()) {
		Gtkmm2ext::set_color_from_rgba (c, GroupTabs::group_color (g));
	} else {
		Gtkmm2ext::set_color_from_rgba (c, _route->presentation_info().color());
	}

	return c;
}

Gdk::Color
RouteUI::route_color_tint () const
{
	return route_color ();
#if 0
	Gdk::Color lighter_bg;

	HSV l (gdk_color_to_rgba (route_color()));
	l.h += std::min (l.h + 0.08, 1.0);
	l.s = 0.15;
	l.v -= std::max (0.0, 0.05);
	set_color_from_rgba (lighter_bg, l.color ());
	return lighter_bg;
#endif
}

void
RouteUI::set_showing_sends_to (std::shared_ptr<Route> send_to)
{
	_showing_sends_to = send_to;
	BusSendDisplayChanged (send_to); /* EMIT SIGNAL */
}

void
RouteUI::bus_send_display_changed (std::shared_ptr<Route> send_to)
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

void
RouteUI::help_count_plugins (std::weak_ptr<Processor> p, uint32_t* plugin_insert_cnt)
{
	std::shared_ptr<Processor> processor (p.lock ());
	if (!processor || !processor->display_to_user()) {
		return;
	}
	std::shared_ptr<PluginInsert> pi = std::dynamic_pointer_cast<PluginInsert> (processor);
#ifdef MIXBUS
	if (pi && pi->is_channelstrip ()) {
		return;
	}
#endif
	if (pi) {
		++(*plugin_insert_cnt);
	}
}

RoutePinWindowProxy::RoutePinWindowProxy(std::string const &name, std::shared_ptr<ARDOUR::Route> route)
	: WM::ProxyBase (name, string())
	, _route (std::weak_ptr<Route> (route))
{
	route->DropReferences.connect (going_away_connection, MISSING_INVALIDATOR, boost::bind (&RoutePinWindowProxy::route_going_away, this), gui_context());
}

RoutePinWindowProxy::~RoutePinWindowProxy()
{
	_window = 0;
}

ARDOUR::SessionHandlePtr*
RoutePinWindowProxy::session_handle ()
{
	ArdourWindow* aw = dynamic_cast<ArdourWindow*> (_window);
	if (aw) { return aw; }
	return 0;
}

Gtk::Window*
RoutePinWindowProxy::get (bool create)
{
	std::shared_ptr<Route> r = _route.lock ();
	if (!r) {
		return 0;
	}

	if (!_window) {
		if (!create) {
			return 0;
		}
		_window = new PluginPinDialog (r);
		ArdourWindow* aw = dynamic_cast<ArdourWindow*> (_window);
		if (aw) {
			aw->set_session (_session);
		}
		_window->show_all ();
	}
	return _window;
}

void
RoutePinWindowProxy::route_going_away ()
{
	delete _window;
	_window = 0;
	WM::Manager::instance().remove (this);
	going_away_connection.disconnect();
	delete this;
}

void
RouteUI::maybe_add_route_print_mgr ()
{
	if (_route->pinmgr_proxy ()) {
		return;
	}
	RoutePinWindowProxy* wp = new RoutePinWindowProxy (
			string_compose ("RPM-%1", _route->id()), _route);
	wp->set_session (_session);

	const XMLNode* ui_xml = _session->extra_xml (X_("UI"));
	if (ui_xml) {
		wp->set_state (*ui_xml, 0);
	}

#if 0
	void* existing_ui = _route->pinmgr_proxy ();
	if (existing_ui) {
		wp->use_window (*(reinterpret_cast<Gtk::Window*>(existing_ui)));
	}
#endif
	_route->set_pingmgr_proxy (wp);

	WM::Manager::instance().register_window (wp);
}

void
RouteUI::manage_pins ()
{
	RoutePinWindowProxy* proxy = _route->pinmgr_proxy ();
	if (proxy) {
		proxy->get (true);
		proxy->present();
	}
}

void
RouteUI::fan_out (bool to_busses, bool group)
{
	Mixer_UI::instance()->fan_out (_route, to_busses, group);
}

bool
RouteUI::mark_hidden (bool yn)
{
	if (yn != _route->presentation_info().hidden()) {
		_route->presentation_info().set_hidden (yn);
		return true; // things changed
	}
	return false;
}

std::shared_ptr<Stripable>
RouteUI::stripable () const
{
	return _route;
}

void
RouteUI::set_disk_io_point (DiskIOPoint diop)
{
	if (_route && is_track()) {
		track()->set_disk_io_point (diop);
	}
}


std::string
RouteUI::playlist_tip () const
{
	if (!is_track()) {
		return "";
	}

	RouteGroup* rg = route_group ();
	if (rg && rg->is_active() && rg->enabled_property (ARDOUR::Properties::group_select.property_id)) {
		string group_string = "." + rg->name() + ".";

		string take_name = track()->playlist()->name();
		string::size_type idx = take_name.find(group_string);

		if (idx != string::npos) {
			/* find the bit containing the take number / name */
			take_name = take_name.substr (idx + group_string.length());

			/* set the playlist button tooltip to the take name */
				return string_compose(_("Take: %1.%2"),
					Gtkmm2ext::markup_escape_text (rg->name()),
					Gtkmm2ext::markup_escape_text (take_name));
		}
	}

	/* set the playlist button tooltip to the playlist name */
	return  _("Playlist") + std::string(": ") + Gtkmm2ext::markup_escape_text (track()->playlist()->name());
}

std::string
RouteUI::resolve_new_group_playlist_name (std::string const& basename, vector<std::shared_ptr<Playlist> > const& playlists)
{
	std::string ret (basename);

	std::string const group_string = "." + route_group()->name() + ".";

	// iterate through all playlists
	int maxnumber = 0;
	for (vector<std::shared_ptr<Playlist> >::const_iterator i = playlists.begin(); i != playlists.end(); ++i) {
		std::string tmp = (*i)->name();

		std::string::size_type idx = tmp.find(group_string);
		// find those which belong to this group
		if (idx != string::npos) {
			tmp = tmp.substr(idx + group_string.length());

			// and find the largest current number
			int x = atoi(tmp);
			if (x > maxnumber) {
				maxnumber = x;
			}
		}
	}

	maxnumber++;

	char buf[32];
	snprintf (buf, sizeof(buf), "%d", maxnumber);

	ret = _route->name() + "." + route_group()->name () + "." + buf;

	return ret;
}

void
RouteUI::use_new_playlist (std::string name, std::string gid, vector<std::shared_ptr<Playlist> > const& playlists_before_op, bool copy)
{
	std::shared_ptr<Track> tr = track ();
	if (!tr) {
		return;
	}

	std::shared_ptr<const Playlist> pl = tr->playlist();
	if (!pl) {
		return;
	}

	XMLNode* before = &tr->playlist_state ();

	if (copy) {
		tr->use_copy_playlist ();
	} else {
		tr->use_default_new_playlist ();
	}

	tr->playlist()->clear_changes ();
	tr->playlist()->clear_owned_changes ();
	tr->playlist()->set_name (name);
	tr->playlist()->set_pgroup_id (gid);

	XMLNode* after = &tr->playlist_state ();
	if (*before != *after) {
		_session->begin_reversible_command (string_compose (_("New Playlist for track %1"), tr->name ()));
		tr->playlist()->rdiff_and_add_command (_session);
		_session->commit_reversible_command (new MementoCommand<Track>(*tr, before, after));
	} else {
		delete before;
		delete after;
	}
}

void
RouteUI::clear_playlist ()
{
	std::shared_ptr<Track> tr = track ();
	if (!tr) {
		return;
	}

	std::shared_ptr<Playlist> pl = tr->playlist();
	if (!pl) {
		return;
	}

	ARDOUR_UI::instance()->the_editor().clear_playlist (pl);
}

void
RouteUI::build_playlist_menu ()
{
	using namespace Menu_Helpers;

	if (!is_track()) {
		return;
	}

	PublicEditor* editor = &ARDOUR_UI::instance()->the_editor();

	delete playlist_action_menu;
	playlist_action_menu = new Menu;
	playlist_action_menu->set_name ("ArdourContextMenu");

	MenuList& playlist_items = playlist_action_menu->items();
	playlist_action_menu->set_name ("ArdourContextMenu");
	playlist_items.clear();

	RadioMenuItem::Group playlist_group;
	std::shared_ptr<Track> tr = track ();

	vector<std::shared_ptr<Playlist> > playlists_tr = _session->playlists()->playlists_for_track (tr);

	/* sort the playlists */
	PlaylistSorterByID cmp;
	sort (playlists_tr.begin(), playlists_tr.end(), cmp);

	/* add the playlists to the menu */
	for (vector<std::shared_ptr<Playlist> >::iterator i = playlists_tr.begin(); i != playlists_tr.end(); ++i) {
		string text = (*i)->name();
		playlist_items.push_back (RadioMenuElem (playlist_group, text));
		RadioMenuItem *item = static_cast<RadioMenuItem*>(&playlist_items.back());
		if (tr->playlist()->id() == (*i)->id()) {
			item->set_active();
		}
		item->signal_toggled().connect(sigc::bind (sigc::mem_fun (*this, &RouteUI::use_playlist), item, std::weak_ptr<Playlist> (*i)));
	}

	playlist_items.push_back (SeparatorElem());
	playlist_items.push_back (MenuElem(_("Select ..."), sigc::mem_fun(*this, &RouteUI::show_playlist_selector)));

	playlist_items.push_back (SeparatorElem());
	playlist_items.push_back (MenuElem (_("Rename..."), sigc::mem_fun(*this, &RouteUI::rename_current_playlist)));
	playlist_items.push_back (SeparatorElem());

	if (!route_group() || !route_group()->is_active() || !route_group()->enabled_property (ARDOUR::Properties::group_select.property_id)) {
		playlist_items.push_back (MenuElem (_("New Playlist..."), sigc::bind(sigc::mem_fun(editor, &PublicEditor::new_playlists_for_grouped_tracks), this, false)));
		playlist_items.push_back (MenuElem (_("Copy Playlist..."), sigc::bind(sigc::mem_fun(editor, &PublicEditor::new_playlists_for_grouped_tracks), this, true)));
	} else {
		playlist_items.push_back (MenuElem (_("New Playlist (for group)"), sigc::bind(sigc::mem_fun(editor, &PublicEditor::new_playlists_for_grouped_tracks), this, false)));
		playlist_items.push_back (MenuElem (_("Copy Playlist (for group)"), sigc::bind(sigc::mem_fun(editor, &PublicEditor::new_playlists_for_grouped_tracks), this, true)));
	}

	playlist_items.push_back (SeparatorElem());
	if (!route_group() || !route_group()->is_active() || !route_group()->enabled_property (ARDOUR::Properties::group_select.property_id)) {
		playlist_items.push_back (MenuElem (_("Clear Current"), sigc::bind(sigc::mem_fun(editor, &PublicEditor::clear_grouped_playlists), this)));
	} else {
		playlist_items.push_back (MenuElem (_("Clear Current (for group)"), sigc::bind(sigc::mem_fun(editor, &PublicEditor::clear_grouped_playlists), this)));
	}
	playlist_items.push_back (SeparatorElem());

	Menu* advanced_menu = manage (new Menu);
	MenuList& advanced_items = advanced_menu->items();
	advanced_items.push_back (MenuElem(_("Copy from ..."), sigc::mem_fun(*this, &RouteUI::show_playlist_copy_selector)));
	advanced_items.push_back (MenuElem(_("Share with ..."), sigc::mem_fun(*this, &RouteUI::show_playlist_share_selector)));
	advanced_items.push_back (MenuElem(_("Steal from ..."), sigc::mem_fun(*this, &RouteUI::show_playlist_steal_selector)));
	playlist_items.push_back (MenuElem (_("Advanced"), *advanced_menu));
}

void
RouteUI::use_playlist (RadioMenuItem *item, std::weak_ptr<Playlist> wpl)
{
	// exit if we were triggered by deactivating the old playlist
	if (item && !item->get_active()) {
		return;
	}

	PublicEditor::instance().mapover_grouped_routes (sigc::bind (sigc::mem_fun (PublicEditor::instance(), &PublicEditor::mapped_select_playlist_matching), wpl), this, ARDOUR::Properties::group_select.property_id);
}


void
RouteUI::select_playlist_matching (std::weak_ptr<Playlist> wpl)
{
	if (!is_track()) {
		return;
	}

	std::shared_ptr<Playlist> pl (wpl.lock());

	if (!pl) {
		return;
	}

	if (track()->freeze_state() == Track::Frozen) {
		/* Don't change playlists of frozen tracks */
		return;
	}

	if (track()->playlist() == pl) {
		/* already selected; nothing to do */
		return;
	}

	std::shared_ptr<Playlist> ipl;
	std::shared_ptr<Track> t = track ();
	XMLNode* before = &t->playlist_state ();

	if (t->id() == pl->get_orig_track_id()) {
		/* this playlist is one of this track's own, no need to match by pgroup-id or name */
		t->use_playlist(t->data_type(), pl);
		goto checkdiff;
	}

	/* Search for a matching playlist .. either by pgroup_id or name */
	if (0 != (ipl = session()->playlists()->for_pgroup(pl->pgroup_id(), t->id()))) {
		// found a playlist that matches the pgroup_id, use it
		t->use_playlist (t->data_type(), ipl);
	} else { // fallback to prior behavior ... try to find matching names /*DEPRECATED*/

		std::string take_name = pl->name();
		std::string group_name;
		if (t->route_group()) {
			group_name = t->route_group()->name();
		}
		std::string group_string = "." + group_name + ".";

		std::string::size_type idx = take_name.find(group_string);

		if (idx != std::string::npos) {
			take_name = take_name.substr(idx + group_string.length()); // find the bit containing the take number / name
			std::string playlist_name = t->name()+group_string+take_name;

			std::shared_ptr<Playlist> ipl = session()->playlists()->by_name(playlist_name);
			if (ipl) {
				t->use_playlist(t->data_type(), ipl);
			}
		}
	}

checkdiff:
	XMLNode* after = &t->playlist_state ();
	if (*before != *after) {
		_session->begin_reversible_command (string_compose (_("Switch Playlist for track %1"), t->name ()));
		_session->commit_reversible_command (new MementoCommand<Track>(*t, before, after));
	} else {
		delete before;
		delete after;
	}
}

void
RouteUI::show_playlist_selector ()
{
	if (!_playlist_selector) {
		_playlist_selector = new PlaylistSelector();
		_playlist_selector->set_session(_session);
	}

	_playlist_selector->prepare(this, PlaylistSelector::plSelect);
	_playlist_selector->show ();
}

void
RouteUI::show_playlist_copy_selector ()
{
	if (!_playlist_selector) {
		_playlist_selector = new PlaylistSelector();
		_playlist_selector->set_session(_session);
	}

	_playlist_selector->prepare(this, PlaylistSelector::plCopy);
	_playlist_selector->show ();
}

void
RouteUI::show_playlist_share_selector ()
{
	if (!_playlist_selector) {
		_playlist_selector = new PlaylistSelector();
		_playlist_selector->set_session(_session);
	}

	_playlist_selector->prepare(this, PlaylistSelector::plShare);
	_playlist_selector->show ();
}

void
RouteUI::show_playlist_steal_selector ()
{
	if (!_playlist_selector) {
		_playlist_selector = new PlaylistSelector();
		_playlist_selector->set_session(_session);
	}

	_playlist_selector->prepare(this, PlaylistSelector::plSteal);
	_playlist_selector->show ();
}

void
RouteUI::rename_current_playlist ()
{
	Prompter prompter (true);
	string name;

	std::shared_ptr<Track> tr = track();
	if (!tr) {
		return;
	}

	std::shared_ptr<Playlist> pl = tr->playlist();
	if (!pl) {
		return;
	}

	prompter.set_title (_("Rename Playlist"));
	prompter.set_prompt (_("New name for playlist:"));
	prompter.add_button (_("Rename"), Gtk::RESPONSE_ACCEPT);
	prompter.set_initial_text (pl->name());
	prompter.set_response_sensitive (Gtk::RESPONSE_ACCEPT, false);

	while (true) {
		if (prompter.run () != Gtk::RESPONSE_ACCEPT) {
			break;
		}
		prompter.get_result (name);
		if (name.length()) {
			if (_session->playlists()->by_name (name)) {
				prompter.set_prompt (_("That name is already in use.  Use this instead?"));
				prompter.set_initial_text (Playlist::bump_name (name, *_session));
			} else {
				break;
			}
		}
	}

	if (name.length()) {
		vector<std::shared_ptr<Playlist> > playlists_gr = _session->playlists()->playlists_for_pgroup (pl->pgroup_id());
		for (vector<std::shared_ptr<Playlist> >::iterator i = playlists_gr.begin(); i != playlists_gr.end(); ++i) {
			(*i)->set_name (name);
		}
	}
}
