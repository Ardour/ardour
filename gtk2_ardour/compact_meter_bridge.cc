/*
    Copyright (C) 2014 Waves Audio Ltd.

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

#include "compact_meter_bridge.h"

#include "keyboard.h"
#include "monitor_section.h"
#include "public_editor.h"
#include "ardour_ui.h"
#include "utils.h"
#include "route_sorter.h"
#include "actions.h"
#include "gui_thread.h"
#include "global_signals.h"
#include "meter_patterns.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Glib;
using namespace Gtkmm2ext;
using namespace std;
using namespace ArdourMeter;

using PBD::atoi;

struct SignalOrderRouteSorter {
	bool operator() (boost::shared_ptr<Route> a, boost::shared_ptr<Route> b) {
		if (a->is_master() || a->is_monitor() || !boost::dynamic_pointer_cast<Track>(a)) {
			/* "a" is a special route (master, monitor, etc), and comes
			 * last in the mixer ordering
			 */
			return false;
		} else if (b->is_master() || b->is_monitor() || !boost::dynamic_pointer_cast<Track>(b)) {
			/* everything comes before b */
			return true;
		}
		return a->order_key () < b->order_key ();
	}
};

CompactMeterbridge::CompactMeterbridge ()
	: Gtk::EventBox()
	, WavesUI ("compact_meter_bridge.xml", *this)
	, _compact_meter_strips_home (get_box ("compact_meter_strips_home"))
{
	set_attributes (*this, *xml_tree ()->root (), XMLNodeMap ());
	signal_configure_event().connect (sigc::mem_fun (*ARDOUR_UI::instance(), &ARDOUR_UI::configure_handler));
	Route::SyncOrderKeys.connect (*this, invalidator (*this), boost::bind (&CompactMeterbridge::sync_order_keys, this), gui_context());
	CompactMeterStrip::CatchDeletion.connect (*this, invalidator (*this), boost::bind (&CompactMeterbridge::remove_strip, this, _1), gui_context());
}

CompactMeterbridge::~CompactMeterbridge ()
{
}

void
CompactMeterbridge::set_session (Session* s)
{
	SessionHandlePtr::set_session (s);

	if (!_session) {
		return;
	}

	boost::shared_ptr<RouteList> routes = _session->get_routes();

	add_strips(*routes);

	_session->RouteAdded.connect (_session_connections, invalidator (*this), boost::bind (&CompactMeterbridge::add_strips, this, _1), gui_context());

	start_updating ();
}

void
CompactMeterbridge::session_going_away ()
{
	ENSURE_GUI_THREAD (*this, &CompactMeterbridge::session_going_away);

	for (std::map <boost::shared_ptr<ARDOUR::Route>, CompactMeterStrip*>::iterator i = _strips.begin(); i != _strips.end(); ++i) {
		delete (*i).second;
	}

	_strips.clear ();
	stop_updating ();

	SessionHandlePtr::session_going_away ();
	_session = 0;
}

gint
CompactMeterbridge::start_updating ()
{
	fast_screen_update_connection = ARDOUR_UI::instance()->SuperRapidScreenUpdate.connect (sigc::mem_fun(*this, &CompactMeterbridge::fast_update_strips));
	return 0;
}

gint
CompactMeterbridge::stop_updating ()
{
	fast_screen_update_connection.disconnect();
	return 0;
}

void
CompactMeterbridge::fast_update_strips ()
{
	if (!is_mapped () || !_session) {
		return;
	}
	for (std::map <boost::shared_ptr<ARDOUR::Route>, CompactMeterStrip*>::iterator i = _strips.begin(); i != _strips.end(); ++i) {
		(*i).second->fast_update ();
	}
}

void
CompactMeterbridge::add_strips (RouteList& routes)
{

	// WARNING: This is a rapid implementation. It must be OPTIMIZED in order to
	// eliminate the code duplicating (see sync_reorder_keys())

	// First detach all the prviously added strips from the ui tree.
	for (std::map<boost::shared_ptr<ARDOUR::Route>, CompactMeterStrip*>::iterator i = _strips.begin(); i != _strips.end(); ++i) {
		_compact_meter_strips_home.remove (*(*i).second); // we suppose _compact_meter_strips_home is
		                                                  // the parnet. 
	}

	// Now create the strips for newly added routes
	for (RouteList::iterator x = routes.begin(); x != routes.end(); ++x) {
		boost::shared_ptr<Route> route = (*x);
		if (route->is_auditioner() || route->is_monitor() || route->is_master()
			|| !boost::dynamic_pointer_cast<Track> (route)) {
			continue;
		}

		CompactMeterStrip* strip = new CompactMeterStrip (_session, route);
		_strips [route] = strip;
		strip->show();
	}

	// Now sort the session's routes and pack the strips accordingly
	SignalOrderRouteSorter sorter;
	RouteList copy(*_session->get_routes());
	copy.sort(sorter);

	size_t serial_number = 0;
	for (RouteList::iterator x = copy.begin(); x != copy.end(); ++x) {
		boost::shared_ptr<Route> route = (*x);
		if (route->is_auditioner() || route->is_monitor() || route->is_master() ||
			!boost::dynamic_pointer_cast<Track>(route)) {
			continue;
		}
		std::map <boost::shared_ptr<ARDOUR::Route>, CompactMeterStrip*>::iterator i = _strips.find (route);
		if (i != _strips.end ()) {
			_compact_meter_strips_home.pack_start (*(*i).second, false, false);
			(*i).second->set_serial_number (++serial_number);
            (*i).second->update_tooltip ();
		}
	}
}

void
CompactMeterbridge::remove_strip (CompactMeterStrip* strip)
{
	if (_session && _session->deletion_in_progress()) {
		return;
	}

	boost::shared_ptr<ARDOUR::Route> route = strip->route ();
	std::map <boost::shared_ptr<ARDOUR::Route>, CompactMeterStrip*>::iterator i = _strips.find (route);
	if (i != _strips.end ()) {
		_strips.erase (i);
	}
}

void
CompactMeterbridge::sync_order_keys ()
{
	Glib::Threads::Mutex::Lock lm (_resync_mutex);

	if (!_session) {
		return;
	}

	// First detach all the prviously added strips from the ui tree.
	for (std::map<boost::shared_ptr<ARDOUR::Route>, CompactMeterStrip*>::iterator i = _strips.begin(); i != _strips.end(); ++i) {
		_compact_meter_strips_home.remove (*(*i).second); // we suppose _compact_meter_strips_home is
		                                                  // the parnet. 
	}

	// Now sort the session's routes and pack the strips accordingly
	SignalOrderRouteSorter sorter;	
	RouteList copy(*_session->get_routes());
	copy.sort(sorter);

	size_t serial_number = 0;
	for (RouteList::iterator x = copy.begin(); x != copy.end(); ++x) {
		boost::shared_ptr<Route> route = (*x);
		if (route->is_auditioner() || route->is_monitor() || route->is_master() ||
			!boost::dynamic_pointer_cast<Track> (route)) {
			continue;
		}
		std::map <boost::shared_ptr<ARDOUR::Route>, CompactMeterStrip*>::iterator i = _strips.find (route);
		if (i != _strips.end ()) {
			_compact_meter_strips_home.pack_start (*(*i).second, false, false);
			(*i).second->set_serial_number (++serial_number);
            (*i).second->update_tooltip ();
		}
	}
}
