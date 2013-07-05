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

#include "ardour/audio_track.h"
#include "ardour/midi_track.h"

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

/* copy from gtk2_ardour/mixer_ui.cc -- TODO consolidate
 * used by Meterbridge::set_session() below
 */
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

/* modified version of above
 * used in Meterbridge::sync_order_keys()
 */
struct MeterOrderRouteSorter {
	bool operator() (MeterStrip *ma, MeterStrip *mb) {
		boost::shared_ptr<Route> a = ma->route();
		boost::shared_ptr<Route> b = mb->route();
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


Meterbridge::Meterbridge ()
	: Window (Gtk::WINDOW_TOPLEVEL)
	, VisibilityTracker (*((Gtk::Window*) this))
	, _visible (false)
	, _show_busses (false)
{
	set_name ("Meter Bridge");

	update_title ();

	set_wmclass (X_("ardour_mixer"), PROGRAM_NAME);

	Gdk::Geometry geom;
	geom.max_width = 1<<16;
	geom.max_height = 1024 + 148 + 16 + 12 ; // see FastMeter::max_pattern_metric_size + meter-strip widgets
	set_geometry_hints(*((Gtk::Window*) this), geom, Gdk::HINT_MAX_SIZE);

	set_keep_above (true);
	set_border_width (0);

	metrics_left = manage(new MeterStrip (2));
	global_hpacker.pack_start (*metrics_left, false, false);
	metrics_left->show();

	metrics_right = manage(new MeterStrip (3));
	global_hpacker.pack_start (*metrics_right, false, false);
	metrics_right->show();

	signal_delete_event().connect (sigc::mem_fun (*this, &Meterbridge::hide_window));
	signal_configure_event().connect (sigc::mem_fun (*ARDOUR_UI::instance(), &ARDOUR_UI::configure_handler));
	Route::SyncOrderKeys.connect (*this, invalidator (*this), boost::bind (&Meterbridge::sync_order_keys, this, _1), gui_context());
	MeterStrip::CatchDeletion.connect (*this, invalidator (*this), boost::bind (&Meterbridge::remove_strip, this, _1), gui_context());
	MeterStrip::ResetAllPeakDisplays.connect_same_thread (*this, boost::bind(&Meterbridge::reset_all_peaks, this));
	MeterStrip::ResetGroupPeakDisplays.connect_same_thread (*this, boost::bind (&Meterbridge::reset_group_peaks, this, _1));

	global_hpacker.set_spacing(0);
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

/* code duplicated from gtk2_ardour/mixer_ui.cc  Mixer_UI::update_title() */
void
Meterbridge::update_title ()
{
	if (_session) {
		string n;

		if (_session->snap_name() != _session->name()) {
			n = _session->snap_name ();
		} else {
			n = _session->name ();
		}

		if (_session->dirty ()) {
			n = "*" + n;
		}

		WindowTitle title (n);
		title += S_("Window|Meterbridge");
		title += Glib::get_application_name ();
		set_title (title.get_string());

	} else {

		WindowTitle title (S_("Window|Meterbridge"));
		title += Glib::get_application_name ();
		set_title (title.get_string());
	}
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

	update_title ();
	_show_busses = _session->config.get_show_busses_in_meterbridge();

	SignalOrderRouteSorter sorter;
	boost::shared_ptr<RouteList> routes = _session->get_routes();

	RouteList copy(*routes);
	copy.sort(sorter);
	add_strips(copy);

	_session->RouteAdded.connect (_session_connections, invalidator (*this), boost::bind (&Meterbridge::add_strips, this, _1), gui_context());
	_session->DirtyChanged.connect (_session_connections, invalidator (*this), boost::bind (&Meterbridge::update_title, this), gui_context());
	_session->StateSaved.connect (_session_connections, invalidator (*this), boost::bind (&Meterbridge::update_title, this), gui_context());
	_session->config.ParameterChanged.connect (*this, invalidator (*this), ui_bind (&Meterbridge::parameter_changed, this, _1), gui_context());

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
	update_title ();
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

		strip = new MeterStrip (_session, route);
		strips.push_back (strip);

		global_hpacker.pack_start (*strip, false, false);
		strip->show();
	}

	sync_order_keys(MixerSort);
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
Meterbridge::reset_all_peaks ()
{
	for (list<MeterStrip *>::iterator i = strips.begin(); i != strips.end(); ++i) {
		(*i)->reset_peak_display ();
	}
}

void
Meterbridge::reset_group_peaks (RouteGroup* rg)
{
	for (list<MeterStrip *>::iterator i = strips.begin(); i != strips.end(); ++i) {
		(*i)->reset_group_peak_display (rg);
	}
}

void
Meterbridge::sync_order_keys (RouteSortOrderKey src)
{
	MeterOrderRouteSorter sorter;
	std::list<MeterStrip *> copy (strips);
	copy.sort(sorter);

	int pos = 0;
	global_hpacker.reorder_child(*metrics_left, pos++);

	for (list<MeterStrip *>::iterator i = copy.begin(); i != copy.end(); ++i) {

#if 0 // TODO subscribe to route active,inactive changes, merge w/ bus
		if (! (*i)->route()->active()) {
			(*i)->hide();
		} else {
			(*i)->show();
		}
#endif

		// TODO simplyfy, abstract ->is_bus()
		if ((*i)->route()->is_master()) {
			/* always show master */
			(*i)->show();
		}
		else if (boost::dynamic_pointer_cast<AudioTrack>((*i)->route()) == 0
				&& boost::dynamic_pointer_cast<MidiTrack>((*i)->route()) == 0
				) {
			/* non-master bus */
			if (_show_busses) {
				(*i)->show();
			} else {
				(*i)->hide();
			}
		}
		else {
			(*i)->show();
		}

		global_hpacker.reorder_child(*(*i), pos++);
	}
	global_hpacker.reorder_child(*metrics_right, pos);
}

void
Meterbridge::parameter_changed (std::string const & p)
{
	if (p == "show-busses-in-meterbridge") {
		_show_busses = _session->config.get_show_busses_in_meterbridge();
		sync_order_keys(MixerSort);
	}
}
