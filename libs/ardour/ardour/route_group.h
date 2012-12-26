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

#ifndef __ardour_route_group_h__
#define __ardour_route_group_h__

#include <list>
#include <set>
#include <string>
#include <stdint.h>

#include "pbd/signals.h"
#include "pbd/stateful.h"
#include "pbd/signals.h"

#include "ardour/types.h"
#include "ardour/session_object.h"

namespace ARDOUR {

namespace Properties {
	extern PBD::PropertyDescriptor<bool> relative;
	extern PBD::PropertyDescriptor<bool> active;
	extern PBD::PropertyDescriptor<bool> gain;
	extern PBD::PropertyDescriptor<bool> mute;
	extern PBD::PropertyDescriptor<bool> solo;
	extern PBD::PropertyDescriptor<bool> recenable;
	extern PBD::PropertyDescriptor<bool> select;
	extern PBD::PropertyDescriptor<bool> route_active;
	extern PBD::PropertyDescriptor<bool> color;
	extern PBD::PropertyDescriptor<bool> monitoring;
	/* we use this, but its declared in region.cc */
	extern PBD::PropertyDescriptor<bool> hidden;
};

class Route;
class Track;
class AudioTrack;
class Session;

class RouteGroup : public SessionObject
{
  public:
	static void make_property_quarks();

	RouteGroup (Session& s, const std::string &n);
	~RouteGroup ();

	bool is_active () const { return _active.val(); }
	bool is_relative () const { return _relative.val(); }
	bool is_hidden () const { return _hidden.val(); }
	bool is_gain () const { return _gain.val(); }
	bool is_mute () const { return _mute.val(); }
	bool is_solo () const { return _solo.val(); }
	bool is_recenable () const { return _recenable.val(); }
	bool is_select () const { return _select.val(); }
	bool is_route_active () const { return _route_active.val(); }
	bool is_color () const { return _color.val(); }
	bool is_monitoring() const { return _monitoring.val(); }

	bool empty() const {return routes->empty();}
	size_t size() const { return routes->size();}

	gain_t get_max_factor(gain_t factor);
	gain_t get_min_factor(gain_t factor);

	void set_active (bool yn, void *src);
	void set_relative (bool yn, void *src);
	void set_hidden (bool yn, void *src);

	void set_gain (bool yn);
	void set_mute (bool yn);
	void set_solo (bool yn);
	void set_recenable (bool yn);
	void set_select (bool yn);
	void set_route_active (bool yn);
	void set_color (bool yn);
	void set_monitoring (bool yn);

	bool enabled_property (PBD::PropertyID);

	int add (boost::shared_ptr<Route>);
	int remove (boost::shared_ptr<Route>);

	template<typename Function>
	void foreach_route (Function f) {
		for (RouteList::iterator i = routes->begin(); i != routes->end(); ++i) {
			f (i->get());
		}
	}

	/* to use these, #include "ardour/route_group_specialized.h" */

	template<class T> void apply (void (Track::*func)(T, void *), T val, void *src);

	/* fills at_set with all members of the group that are AudioTracks */

	void audio_track_group (std::set<boost::shared_ptr<AudioTrack> >& at_set);

	void clear () {
		routes->clear ();
		changed();
	}

	void make_subgroup (bool, Placement);
	void destroy_subgroup ();

	boost::shared_ptr<RouteList> route_list() { return routes; }

	/** Emitted when a route has been added to this group */
	PBD::Signal2<void, RouteGroup *, boost::weak_ptr<ARDOUR::Route> > RouteAdded;
	/** Emitted when a route has been removed from this group */
	PBD::Signal2<void, RouteGroup *, boost::weak_ptr<ARDOUR::Route> > RouteRemoved;

	XMLNode& get_state ();

	int set_state (const XMLNode&, int version);

private:
	boost::shared_ptr<RouteList> routes;
	boost::shared_ptr<Route> subgroup_bus;

	PBD::Property<bool> _relative;
	PBD::Property<bool> _active;
	PBD::Property<bool> _hidden;
	PBD::Property<bool> _gain;
	PBD::Property<bool> _mute;
	PBD::Property<bool> _solo;
	PBD::Property<bool> _recenable;
	PBD::Property<bool> _select;
	PBD::Property<bool> _route_active;
	PBD::Property<bool> _color;
	PBD::Property<bool> _monitoring;

	void remove_when_going_away (boost::weak_ptr<Route>);
	int set_state_2X (const XMLNode&, int);
};

} /* namespace */

#endif /* __ardour_route_group_h__ */
