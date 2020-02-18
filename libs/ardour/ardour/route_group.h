/*
 * Copyright (C) 2000-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005-2006 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2016-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2018 Len Ovens <len@ovenwerks.net>
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

#ifndef __ardour_route_group_h__
#define __ardour_route_group_h__

#include <list>
#include <set>
#include <string>
#include <stdint.h>

#include "pbd/controllable.h"
#include "pbd/signals.h"
#include "pbd/stateful.h"

#include "ardour/control_group.h"
#include "ardour/types.h"
#include "ardour/session_object.h"

#include "ardour/libardour_visibility.h"

namespace ARDOUR {

namespace Properties {
	LIBARDOUR_API extern PBD::PropertyDescriptor<bool> group_relative;
	LIBARDOUR_API extern PBD::PropertyDescriptor<bool> group_gain;
	LIBARDOUR_API extern PBD::PropertyDescriptor<bool> group_mute;
	LIBARDOUR_API extern PBD::PropertyDescriptor<bool> group_solo;
	LIBARDOUR_API extern PBD::PropertyDescriptor<bool> group_recenable;
	LIBARDOUR_API extern PBD::PropertyDescriptor<bool> group_select;
	LIBARDOUR_API extern PBD::PropertyDescriptor<bool> group_route_active;
	LIBARDOUR_API extern PBD::PropertyDescriptor<bool> group_color;
	LIBARDOUR_API extern PBD::PropertyDescriptor<bool> group_monitoring;
	LIBARDOUR_API extern PBD::PropertyDescriptor<bool> active;
	LIBARDOUR_API extern PBD::PropertyDescriptor<int32_t> group_master_number;
	/* we use these declared in region.cc */
	LIBARDOUR_API extern PBD::PropertyDescriptor<bool> hidden;
};

class Route;
class Track;
class AudioTrack;
class Session;

/** A group identifier for routes.
 *
 * RouteGroups permit to define properties which are shared
 * among all Routes that use the given identifier.
 *
 * A route can at most be in one group.
 */
class LIBARDOUR_API RouteGroup : public SessionObject
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
	int32_t group_master_number() const { return _group_master_number.val(); }
	boost::weak_ptr<Route> subgroup_bus() const { return _subgroup_bus; }

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

	template<class T> void apply (void (Track::*func)(T, PBD::Controllable::GroupControlDisposition), T val, PBD::Controllable::GroupControlDisposition);

	/* fills at_set with all members of the group that are AudioTracks */

	void audio_track_group (std::set<boost::shared_ptr<AudioTrack> >& at_set);

	void clear () {
		routes->clear ();
		changed();
	}

	bool has_subgroup() const;
	void make_subgroup (bool, Placement);
	void destroy_subgroup ();

	boost::shared_ptr<RouteList> route_list() { return routes; }

	/** Emitted when a route has been added to this group */
	PBD::Signal2<void, RouteGroup *, boost::weak_ptr<ARDOUR::Route> > RouteAdded;
	/** Emitted when a route has been removed from this group */
	PBD::Signal2<void, RouteGroup *, boost::weak_ptr<ARDOUR::Route> > RouteRemoved;

	XMLNode& get_state ();

	int set_state (const XMLNode&, int version);

	void assign_master (boost::shared_ptr<VCA>);
	void unassign_master (boost::shared_ptr<VCA>);
	bool has_control_master() const;
	bool slaved () const;

	uint32_t rgba () const { return _rgba; }

	/** set route-group color and notify UI about change */
	void set_rgba (uint32_t);

	/* directly set color only, used to convert old 5.x gui-object-state
	 * to libardour color */
	void migrate_rgba (uint32_t color) { _rgba = color; }

private:
	boost::shared_ptr<RouteList> routes;
	boost::shared_ptr<Route> _subgroup_bus;
	boost::weak_ptr<VCA> group_master;

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
	PBD::Property<int32_t> _group_master_number;

	boost::shared_ptr<ControlGroup> _solo_group;
	boost::shared_ptr<ControlGroup> _mute_group;
	boost::shared_ptr<ControlGroup> _rec_enable_group;
	boost::shared_ptr<ControlGroup> _gain_group;
	boost::shared_ptr<ControlGroup> _monitoring_group;

	void remove_when_going_away (boost::weak_ptr<Route>);
	int set_state_2X (const XMLNode&, int);

	void post_set (PBD::PropertyChange const &);
	void push_to_groups ();

	uint32_t _rgba;
	bool _used_to_share_gain;
};

} /* namespace */

#endif /* __ardour_route_group_h__ */
