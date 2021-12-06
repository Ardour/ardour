/*
 * Copyright (C) 2021 Robin Gareus <robin@gareus.org>
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

#include <gtkmm/label.h>

#include "pbd/properties.h"

#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/keyboard.h"
#include "gtkmm2ext/window_title.h"

#include "actions.h"
#include "ardour_ui.h"
#include "gui_thread.h"
#include "public_editor.h"
#include "timers.h"
#include "trigger_page.h"
#include "trigger_strip.h"
#include "ui_config.h"
#include "utils.h"

#include "audio_clip_editor.h"

#include "pbd/i18n.h"

#define PX_SCALE(px) std::max ((float)px, rintf ((float)px* UIConfiguration::instance ().get_ui_scale ()))

using namespace ARDOUR;
using namespace Gtkmm2ext;
using namespace Gtk;
using namespace std;

TriggerPage::TriggerPage ()
	: Tabbable (_content, _("Trigger Drom"), X_("trigger"))
{
	load_bindings ();
	register_actions ();

#if 1 /* Placeholders */
	_slot_area_box.pack_start (*Gtk::manage (new Gtk::Label ("Fixed\nWidth\nSlot\nArea")));
	_browser_box.pack_start (*Gtk::manage (new Gtk::Label ("File Browser")));
	_parameter_box.pack_start (*Gtk::manage (new Gtk::Label ("Parameter HBox")));
	_slot_area_box.show_all ();
	_browser_box.show_all ();
	_parameter_box.show_all ();
#endif

	/* Upper pane (slot | strips | file browser) */

	_strip_scroller.add (_strip_packer);
	_strip_scroller.set_policy (Gtk::POLICY_ALWAYS, Gtk::POLICY_AUTOMATIC);

	/* last item of strip packer */
	_strip_packer.pack_end (_no_strips, true, true);
	_no_strips.set_size_request (PX_SCALE (60), -1);
	_no_strips.signal_expose_event().connect (sigc::bind (sigc::ptr_fun(&ArdourWidgets::ArdourIcon::expose), &_no_strips, ArdourWidgets::ArdourIcon::CloseCross));

	_strip_group_box.pack_start (_slot_area_box, false, false);
	_strip_group_box.pack_start (_strip_scroller, true, true);

	_pane_upper.add (_strip_group_box);
	_pane_upper.add (_browser_box);

	/* Top-level Layout */
	_pane.add (_pane_upper);
	_pane.add (_parameter_box);

	_content.pack_start (_pane, true, true);
	_content.show ();

	/* Show all */
	_pane.show ();
	_pane_upper.show ();
	_strip_group_box.show ();
	_strip_scroller.show ();
	_strip_packer.show ();
	_slot_area_box.show ();
	_browser_box.show ();
	_parameter_box.show ();

	/* setup keybidings */
	_content.set_data ("ardour-bindings", bindings);

	/* subscribe to signals */
	Config->ParameterChanged.connect (*this, invalidator (*this), boost::bind (&TriggerPage::parameter_changed, this, _1), gui_context ());
	PresentationInfo::Change.connect (*this, invalidator (*this), boost::bind (&TriggerPage::pi_property_changed, this, _1), gui_context ());

	/* init */
	update_title ();

	/* Restore pane state */
	float          fract;
	XMLNode const* settings = ARDOUR_UI::instance ()->trigger_page_settings ();
	if (!settings || !settings->get_property ("triggerpage-vpane-pos", fract) || fract > 1.0) {
		fract = 0.75f;
	}
	_pane.set_divider (0, fract);

	if (!settings || !settings->get_property ("triggerpage-hpane-pos", fract) || fract > 1.0) {
		fract = 0.75f;
	}
	_pane_upper.set_divider (0, fract);
}

TriggerPage::~TriggerPage ()
{
}

Gtk::Window*
TriggerPage::use_own_window (bool and_fill_it)
{
	bool new_window = !own_window ();

	Gtk::Window* win = Tabbable::use_own_window (and_fill_it);

	if (win && new_window) {
		win->set_name ("TriggerWindow");
		ARDOUR_UI::instance ()->setup_toplevel_window (*win, _("Trigger Drom"), this);
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

XMLNode&
TriggerPage::get_state ()
{
	XMLNode* node = new XMLNode (X_("TriggerPage"));
	node->add_child_nocopy (Tabbable::get_state ());

	node->set_property (X_("triggerpage-vpane-pos"), _pane.get_divider ());
	node->set_property (X_("triggerpage-hpane-pos"), _pane_upper.get_divider ());
	return *node;
}

int
TriggerPage::set_state (const XMLNode& node, int version)
{
	return Tabbable::set_state (node, version);
}

void
TriggerPage::load_bindings ()
{
	bindings = Bindings::get_bindings (X_("TriggerPage"));
}

void
TriggerPage::register_actions ()
{
	Glib::RefPtr<ActionGroup> group = ActionManager::create_action_group (bindings, X_("TriggerPage"));
}

void
TriggerPage::set_session (Session* s)
{
	SessionHandlePtr::set_session (s);

	if (!_session) {
		return;
	}

	XMLNode* node = ARDOUR_UI::instance ()->trigger_page_settings ();
	set_state (*node, Stateful::loading_state_version);

	_session->DirtyChanged.connect (_session_connections, invalidator (*this), boost::bind (&TriggerPage::update_title, this), gui_context ());
	_session->StateSaved.connect (_session_connections, invalidator (*this), boost::bind (&TriggerPage::update_title, this), gui_context ());

	_session->RouteAdded.connect (_session_connections, invalidator (*this), boost::bind (&TriggerPage::add_routes, this, _1), gui_context ());
	TriggerStrip::CatchDeletion.connect (*this, invalidator (*this), boost::bind (&TriggerPage::remove_route, this, _1), gui_context ());

	_session->config.ParameterChanged.connect (_session_connections, invalidator (*this), boost::bind (&TriggerPage::parameter_changed, this, _1), gui_context ());

	initial_track_display ();

	update_title ();
	start_updating ();
}

void
TriggerPage::session_going_away ()
{
	ENSURE_GUI_THREAD (*this, &TriggerPage::session_going_away);

	stop_updating ();

#if 0
	/* DropReferneces calls RouteUI::self_delete -> CatchDeletion .. */
	for (list<TriggerStrip*>::iterator i = _strips.begin(); i != _strips.end(); ++i) {
		delete (*i);
	}
#endif
	_strips.clear ();

	SessionHandlePtr::session_going_away ();
	update_title ();
}

void
TriggerPage::update_title ()
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
		title += S_("Window|Trigger");
		title += Glib::get_application_name ();
		own_window ()->set_title (title.get_string ());

	} else {
		WindowTitle title (S_("Window|Trigger"));
		title += Glib::get_application_name ();
		own_window ()->set_title (title.get_string ());
	}
}

void
TriggerPage::initial_track_display ()
{
	boost::shared_ptr<RouteList> r = _session->get_tracks ();
	RouteList                    rl (*r);
	_strips.clear ();
	add_routes (rl);
}

void
TriggerPage::add_routes (RouteList& rl)
{
	rl.sort (Stripable::Sorter ());
	for (RouteList::iterator r = rl.begin (); r != rl.end (); ++r) {
		/* we're only interested in Tracks */
		if (!boost::dynamic_pointer_cast<Track> (*r)) {
			continue;
		}
#if 0
		/* TODO, only subscribe to PropertyChanged, create (and destory) TriggerStrip as needed.
		 * For now we just hide non trigger strips.
		 */
		if (!(*r)->presentation_info ().trigger_track ()) {
			continue;
		}
#endif

		if (!(*r)->triggerbox()) {
			/* This Route has no TriggerBox -- and can never have one */
			continue;
		}

		TriggerStrip* ts = new TriggerStrip (_session, *r);
		_strips.push_back (ts);

		(*r)->presentation_info ().PropertyChanged.connect (*this, invalidator (*this), boost::bind (&TriggerPage::stripable_property_changed, this, _1, boost::weak_ptr<Stripable> (*r)), gui_context ());
		(*r)->PropertyChanged.connect (*this, invalidator (*this), boost::bind (&TriggerPage::stripable_property_changed, this, _1, boost::weak_ptr<Stripable> (*r)), gui_context ());
	}
	redisplay_track_list ();
}

void
TriggerPage::remove_route (TriggerStrip* ra)
{
	if (!_session || _session->deletion_in_progress ()) {
		_strips.clear ();
		return;
	}
	list<TriggerStrip*>::iterator i = find (_strips.begin (), _strips.end (), ra);
	if (i != _strips.end ()) {
		_strip_packer.remove (**i);
		_strips.erase (i);
	}
	redisplay_track_list ();
}

void
TriggerPage::redisplay_track_list ()
{
	bool visible_triggers = false;
	for (list<TriggerStrip*>::iterator i = _strips.begin (); i != _strips.end (); ++i) {
		TriggerStrip*                strip  = *i;
		boost::shared_ptr<Stripable> s      = strip->stripable ();
		boost::shared_ptr<Route> route      = boost::dynamic_pointer_cast<Route> (s);

		bool hidden = s->presentation_info ().hidden ();

		if (!(s)->presentation_info ().trigger_track ()) {
			hidden = true;
		}
		assert (route && route->triggerbox());
		if (!route || !route->triggerbox()) {
			hidden = true;
		}

		if (hidden && strip->get_parent ()) {
			/* if packed, remove it */
			_strip_packer.remove (*strip);
		} else if (!hidden && strip->get_parent ()) {
			/* already packed, put it at the end */
			_strip_packer.reorder_child (*strip, -1); /* put at end */
			visible_triggers = true;
		} else if (!hidden) {
			_strip_packer.pack_start (*strip, false, false);
			visible_triggers = true;
		}
	}
	if (visible_triggers) {
		_no_strips.hide ();
	} else {
		_no_strips.show ();
	}
}

void
TriggerPage::parameter_changed (string const& p)
{
}

void
TriggerPage::pi_property_changed (PBD::PropertyChange const& what_changed)
{
	/* static signal, not yet used */
}

void
TriggerPage::stripable_property_changed (PBD::PropertyChange const& what_changed, boost::weak_ptr<Stripable> ws)
{
	if (what_changed.contains (ARDOUR::Properties::trigger_track)) {
#if 0
		boost::shared_ptr<Stripable> s = ws.lock ();
		/* TODO: find trigger-strip for given stripable, delete *it; */
#else
		/* For now we just hide it */
		redisplay_track_list ();
		return;
#endif
	}
	if (what_changed.contains (ARDOUR::Properties::hidden)) {
		redisplay_track_list ();
	}
}

gint
TriggerPage::start_updating ()
{
	_fast_screen_update_connection = Timers::super_rapid_connect (sigc::mem_fun (*this, &TriggerPage::fast_update_strips));
	return 0;
}

gint
TriggerPage::stop_updating ()
{
	_fast_screen_update_connection.disconnect ();
	return 0;
}

void
TriggerPage::fast_update_strips ()
{
	if (_content.is_mapped () && _session) {
		for (list<TriggerStrip*>::iterator i = _strips.begin (); i != _strips.end (); ++i) {
			(*i)->fast_update ();
		}
	}
}
