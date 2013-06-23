/*
    Copyright (C) 2012 Paul Davis
    Author: Robin Gareus

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

#include <map>
#include <sigc++/bind.h>

#include <gtkmm/accelmap.h>

#include <glibmm/threads.h>

#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/window_title.h>

#include "ardour/debug.h"
#include "ardour/midi_track.h"
#include "ardour/route_group.h"
#include "ardour/session.h"

#include "meterbridge.h"

#include "monitor_section.h"
#include "public_editor.h"
#include "ardour_ui.h"
#include "utils.h"
#include "route_sorter.h"
#include "actions.h"
#include "gui_thread.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Glib;
using namespace Gtkmm2ext;
using namespace std;

using PBD::atoi;

Meterbridge* Meterbridge::_instance = 0;

Meterbridge*
Meterbridge::instance ()
{
	if (!_instance) {
		_instance  = new Meterbridge;
	}

	return _instance;
}

Meterbridge::Meterbridge ()
	: Window (Gtk::WINDOW_TOPLEVEL)
	, VisibilityTracker (*((Gtk::Window*) this))
	, _visible (false)
{
	set_name ("Meter Bridge");

	set_wmclass (X_("ardour_mixer"), PROGRAM_NAME);

	signal_delete_event().connect (sigc::mem_fun (*this, &Meterbridge::hide_window));
	signal_configure_event().connect (sigc::mem_fun (*ARDOUR_UI::instance(), &ARDOUR_UI::configure_handler));

	MeterStrip::CatchDeletion.connect (*this, invalidator (*this), boost::bind (&Meterbridge::remove_strip, this, _1), gui_context());

	global_hpacker.set_spacing(1);
	scroller.add (global_hpacker);
	scroller.set_policy (Gtk::POLICY_AUTOMATIC, Gtk::POLICY_NEVER);
	global_vpacker.pack_start (scroller, true, true);
	add (global_vpacker);

	global_hpacker.show();
	global_vpacker.show();
	scroller.show();
}

Meterbridge::~Meterbridge ()
{
}

void
Meterbridge::show_window ()
{
	present();
	if (!_visible) {
		set_window_pos_and_size ();
	}
	_visible = true;
}

void
Meterbridge::set_window_pos_and_size ()
{
	resize (m_width, m_height);
	move (m_root_x, m_root_y);
}

void
Meterbridge::get_window_pos_and_size ()
{
	get_position(m_root_x, m_root_y);
	get_size(m_width, m_height);
}

bool
Meterbridge::hide_window (GdkEventAny *ev)
{
	get_window_pos_and_size();
	_visible = false;
	return just_hide_it(ev, static_cast<Gtk::Window *>(this));
}

bool
Meterbridge::on_key_press_event (GdkEventKey* ev)
{
	if (gtk_window_propagate_key_event (GTK_WINDOW(gobj()), ev)) {
		return true;
	}
	return forward_key_press (ev);
}

bool
Meterbridge::on_key_release_event (GdkEventKey* ev)
{
	if (gtk_window_propagate_key_event (GTK_WINDOW(gobj()), ev)) {
		return true;
	}
	/* don't forward releases */
	return true;
}


// copy from gtk2_ardour/mixer_ui.cc 
struct SignalOrderRouteSorter {
    bool operator() (boost::shared_ptr<Route> a, boost::shared_ptr<Route> b) {
	    if (a->is_master() || a->is_monitor()) {
		    /* "a" is a special route (master, monitor, etc), and comes
		     * last in the mixer ordering
		     */
		    return false;
	    } else if (b->is_master() || b->is_monitor()) {
		    /* everything comes before b */
		    return true;
	    }
	    return a->order_key (MixerSort) < b->order_key (MixerSort);
    }
};

void
Meterbridge::set_session (Session* s)
{
	SessionHandlePtr::set_session (s);

	if (!_session) {
		return;
	}

	XMLNode* node = _session->instant_xml(X_("Meterbridge"));
	if (node) {
		set_state (*node);
	}

	SignalOrderRouteSorter sorter;
	boost::shared_ptr<RouteList> routes = _session->get_routes();

	RouteList copy(*routes);
	copy.sort(sorter);
	add_strips(copy);

	_session->RouteAdded.connect (_session_connections, invalidator (*this), boost::bind (&Meterbridge::add_strips, this, _1), gui_context());

	_session->config.ParameterChanged.connect (_session_connections, invalidator (*this), boost::bind (&Meterbridge::parameter_changed, this, _1), gui_context());

	if (_visible) {
		show_window();
		ActionManager::check_toggleaction ("<Actions>/Common/toggle-meterbridge");
	}
	start_updating ();
}

void
Meterbridge::session_going_away ()
{
	ENSURE_GUI_THREAD (*this, &Meterbridge::session_going_away);

	for (list<MeterStrip *>::iterator i = strips.begin(); i != strips.end(); ++i) {
		delete (*i);
	}

	strips.clear ();
	stop_updating ();

	SessionHandlePtr::session_going_away ();

	_session = 0;
}

int
Meterbridge::set_state (const XMLNode& node)
{
	const XMLProperty* prop;
	XMLNode* geometry;

	m_width = default_width;
	m_height = default_height;
	m_root_x = 1;
	m_root_y = 1;

	if ((geometry = find_named_node (node, "geometry")) != 0) {

		XMLProperty* prop;

		if ((prop = geometry->property("x_size")) == 0) {
			prop = geometry->property ("x-size");
		}
		if (prop) {
			m_width = atoi(prop->value());
		}
		if ((prop = geometry->property("y_size")) == 0) {
			prop = geometry->property ("y-size");
		}
		if (prop) {
			m_height = atoi(prop->value());
		}

		if ((prop = geometry->property ("x_pos")) == 0) {
			prop = geometry->property ("x-pos");
		}
		if (prop) {
			m_root_x = atoi (prop->value());

		}
		if ((prop = geometry->property ("y_pos")) == 0) {
			prop = geometry->property ("y-pos");
		}
		if (prop) {
			m_root_y = atoi (prop->value());
		}
	}

	set_window_pos_and_size ();

	if ((prop = node.property ("show-meterbridge"))) {
		if (string_is_affirmative (prop->value())) {
		       _visible = true;
		}
	}

	return 0;
}

XMLNode&
Meterbridge::get_state (void)
{
	XMLNode* node = new XMLNode ("Meterbridge");

	if (is_realized()) {
		Glib::RefPtr<Gdk::Window> win = get_window();

		get_window_pos_and_size ();

		XMLNode* geometry = new XMLNode ("geometry");
		char buf[32];
		snprintf(buf, sizeof(buf), "%d", m_width);
		geometry->add_property(X_("x_size"), string(buf));
		snprintf(buf, sizeof(buf), "%d", m_height);
		geometry->add_property(X_("y_size"), string(buf));
		snprintf(buf, sizeof(buf), "%d", m_root_x);
		geometry->add_property(X_("x_pos"), string(buf));
		snprintf(buf, sizeof(buf), "%d", m_root_y);
		geometry->add_property(X_("y_pos"), string(buf));
		node->add_child_nocopy (*geometry);
	}

	node->add_property ("show-meterbridge", _visible ? "yes" : "no");
	return *node;
}


gint
Meterbridge::start_updating ()
{
	fast_screen_update_connection = ARDOUR_UI::instance()->SuperRapidScreenUpdate.connect (sigc::mem_fun(*this, &Meterbridge::fast_update_strips));
	return 0;
}

gint
Meterbridge::stop_updating ()
{
	fast_screen_update_connection.disconnect();
	return 0;
}

void
Meterbridge::fast_update_strips ()
{
	if (!is_mapped () || !_session) {
		return;
	}
	for (list<MeterStrip *>::iterator i = strips.begin(); i != strips.end(); ++i) {
		(*i)->fast_update ();
	}
}

void
Meterbridge::add_strips (RouteList& routes)
{
	MeterStrip* strip;
	for (RouteList::iterator x = routes.begin(); x != routes.end(); ++x) {
		boost::shared_ptr<Route> route = (*x);
		if (route->is_auditioner()) {
			continue;
		}
		if (route->is_monitor()) {
			continue;
		}

		strip = new MeterStrip (*this, _session, route);
		strips.push_back (strip);

		// TODO sort-routes, insert at proper position
		// order_key

		global_hpacker.pack_start (*strip, false, false);
		strip->show();
	}
}

void
Meterbridge::remove_strip (MeterStrip* strip)
{
	if (_session && _session->deletion_in_progress()) {
		return;
	}

	list<MeterStrip *>::iterator i;
	if ((i = find (strips.begin(), strips.end(), strip)) != strips.end()) {
		strips.erase (i);
	}
}

void
Meterbridge::parameter_changed (string const & p)
{
}
