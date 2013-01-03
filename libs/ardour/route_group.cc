/*
    Copyright (C) 2000-2009 Paul Davis

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

#include <inttypes.h>

#include <algorithm>

#include "pbd/error.h"
#include "pbd/enumwriter.h"
#include "pbd/strsplit.h"

#include "ardour/amp.h"
#include "ardour/audio_track.h"
#include "ardour/route.h"
#include "ardour/route_group.h"
#include "ardour/session.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;

namespace ARDOUR {
	namespace Properties {
		PropertyDescriptor<bool> relative;
		PropertyDescriptor<bool> active;
		PropertyDescriptor<bool> gain;
		PropertyDescriptor<bool> mute;
		PropertyDescriptor<bool> solo;
		PropertyDescriptor<bool> recenable;
		PropertyDescriptor<bool> select;
		PropertyDescriptor<bool> route_active;
		PropertyDescriptor<bool> color;
		PropertyDescriptor<bool> monitoring;
	}
}

void
RouteGroup::make_property_quarks ()
{
	Properties::relative.property_id = g_quark_from_static_string (X_("relative"));
        DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for relative = %1\n", 	Properties::relative.property_id));
	Properties::active.property_id = g_quark_from_static_string (X_("active"));
        DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for active = %1\n", 	Properties::active.property_id));
	Properties::hidden.property_id = g_quark_from_static_string (X_("hidden"));
        DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for hidden = %1\n", 	Properties::hidden.property_id));
	Properties::gain.property_id = g_quark_from_static_string (X_("gain"));
        DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for gain = %1\n", 	Properties::gain.property_id));
	Properties::mute.property_id = g_quark_from_static_string (X_("mute"));
        DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for mute = %1\n", 	Properties::mute.property_id));
	Properties::solo.property_id = g_quark_from_static_string (X_("solo"));
        DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for solo = %1\n", 	Properties::solo.property_id));
	Properties::recenable.property_id = g_quark_from_static_string (X_("recenable"));
        DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for recenable = %1\n", 	Properties::recenable.property_id));
	Properties::select.property_id = g_quark_from_static_string (X_("select"));
        DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for select = %1\n", 	Properties::select.property_id));
	Properties::route_active.property_id = g_quark_from_static_string (X_("route-active"));
        DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for route-active = %1\n", Properties::route_active.property_id));
	Properties::color.property_id = g_quark_from_static_string (X_("color"));
        DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for color = %1\n",       Properties::color.property_id));
	Properties::monitoring.property_id = g_quark_from_static_string (X_("monitoring"));
        DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for monitoring = %1\n",       Properties::monitoring.property_id));
}

#define ROUTE_GROUP_DEFAULT_PROPERTIES  _relative (Properties::relative, false) \
	, _active (Properties::active, false) \
	, _hidden (Properties::hidden, false) \
	, _gain (Properties::gain, false) \
	, _mute (Properties::mute, false) \
	, _solo (Properties::solo, false) \
	, _recenable (Properties::recenable, false) \
	, _select (Properties::select, false) \
	, _route_active (Properties::route_active, false) \
	, _color (Properties::color, false) \
	, _monitoring (Properties::monitoring, false)

RouteGroup::RouteGroup (Session& s, const string &n)
	: SessionObject (s, n)
	, routes (new RouteList)
	, ROUTE_GROUP_DEFAULT_PROPERTIES
{
	_xml_node_name = X_("RouteGroup");

	add_property (_relative);
	add_property (_active);
	add_property (_hidden);
	add_property (_gain);
	add_property (_mute);
	add_property (_solo);
	add_property (_recenable);
	add_property (_select);
	add_property (_route_active);
	add_property (_color);
	add_property (_monitoring);
}

RouteGroup::~RouteGroup ()
{
	for (RouteList::iterator i = routes->begin(); i != routes->end();) {
		RouteList::iterator tmp = i;
		++tmp;

		(*i)->set_route_group (0);

		i = tmp;
	}
}

/** Add a route to a group.  Adding a route which is already in the group is allowed; nothing will happen.
 *  @param r Route to add.
 */
int
RouteGroup::add (boost::shared_ptr<Route> r)
{
	if (find (routes->begin(), routes->end(), r) != routes->end()) {
		return 0;
	}

	if (r->route_group()) {
		r->route_group()->remove (r);
	}
	
	routes->push_back (r);

	r->set_route_group (this);
	r->DropReferences.connect_same_thread (*this, boost::bind (&RouteGroup::remove_when_going_away, this, boost::weak_ptr<Route> (r)));

	_session.set_dirty ();
	RouteAdded (this, boost::weak_ptr<Route> (r)); /* EMIT SIGNAL */
	return 0;
}

void
RouteGroup::remove_when_going_away (boost::weak_ptr<Route> wr)
{
	boost::shared_ptr<Route> r (wr.lock());

	if (r) {
		remove (r);
	}
}

int
RouteGroup::remove (boost::shared_ptr<Route> r)
{
	RouteList::iterator i;

	if ((i = find (routes->begin(), routes->end(), r)) != routes->end()) {
		r->set_route_group (0);
		routes->erase (i);
		_session.set_dirty ();
		RouteRemoved (this, boost::weak_ptr<Route> (r)); /* EMIT SIGNAL */
		return 0;
	}

	return -1;
}


gain_t
RouteGroup::get_min_factor (gain_t factor)
{
	for (RouteList::iterator i = routes->begin(); i != routes->end(); ++i) {
		gain_t const g = (*i)->amp()->gain();

		if ((g + g * factor) >= 0.0f) {
			continue;
		}

		if (g <= 0.0000003f) {
			return 0.0f;
		}

		factor = 0.0000003f / g - 1.0f;
	}
	
	return factor;
}

gain_t
RouteGroup::get_max_factor (gain_t factor)
{
	for (RouteList::iterator i = routes->begin(); i != routes->end(); i++) {
		gain_t const g = (*i)->amp()->gain();

		// if the current factor woulnd't raise this route above maximum
		if ((g + g * factor) <= 1.99526231f) {
			continue;
		}

		// if route gain is already at peak, return 0.0f factor
		if (g >= 1.99526231f) {
			return 0.0f;
		}

		// factor is calculated so that it would raise current route to max
		factor = 1.99526231f / g - 1.0f;
	}

	return factor;
}

XMLNode&
RouteGroup::get_state ()
{
	XMLNode *node = new XMLNode ("RouteGroup");

	char buf[64];
	id().print (buf, sizeof (buf));
	node->add_property ("id", buf);

	add_properties (*node);

	if (!routes->empty()) {
		stringstream str;

		for (RouteList::iterator i = routes->begin(); i != routes->end(); ++i) {
			str << (*i)->id () << ' ';
		}

		node->add_property ("routes", str.str());
	}

	return *node;
}

int
RouteGroup::set_state (const XMLNode& node, int version)
{
	if (version < 3000) {
		return set_state_2X (node, version);
	}

	const XMLProperty *prop;

	set_id (node);
	set_values (node);

	if ((prop = node.property ("routes")) != 0) {
		stringstream str (prop->value());
		vector<string> ids;
		split (str.str(), ids, ' ');

		for (vector<string>::iterator i = ids.begin(); i != ids.end(); ++i) {
			PBD::ID id (*i);
			boost::shared_ptr<Route> r = _session.route_by_id (id);

			if (r) {
				add (r);
			}
		}
	}

	return 0;
}

int
RouteGroup::set_state_2X (const XMLNode& node, int /*version*/)
{
	set_values (node);

	if (node.name() == "MixGroup") {
		_gain = true;
		_mute = true;
		_solo = true;
		_recenable = true;
		_route_active = true;
		_color = false;
	} else if (node.name() == "EditGroup") {
		_gain = false;
		_mute = false;
		_solo = false;
		_recenable = false;
		_route_active = false;
		_color = false;
	}

	return 0;
}

void
RouteGroup::set_gain (bool yn)
{
	if (is_gain() == yn) {
		return;
	}
	_gain = yn;
	send_change (PropertyChange (Properties::gain));
}

void
RouteGroup::set_mute (bool yn)
{
	if (is_mute() == yn) {
		return;
	}
	_mute = yn;
	send_change (PropertyChange (Properties::mute));
}

void
RouteGroup::set_solo (bool yn)
{
	if (is_solo() == yn) {
		return;
	}
	_solo = yn;
	send_change (PropertyChange (Properties::solo));
}

void
RouteGroup::set_recenable (bool yn)
{
	if (is_recenable() == yn) {
		return;
	}
	_recenable = yn;
	send_change (PropertyChange (Properties::recenable));
}

void
RouteGroup::set_select (bool yn)
{
	if (is_select() == yn) {
		return;
	}
	_select = yn;
	send_change (PropertyChange (Properties::select));
}

void
RouteGroup::set_route_active (bool yn)
{
	if (is_route_active() == yn) {
		return;
	}
	_route_active = yn;
	send_change (PropertyChange (Properties::route_active));
}

void
RouteGroup::set_color (bool yn)
{
	if (is_color() == yn) {
		return;
	}
	_color = yn;

	send_change (PropertyChange (Properties::color));

	/* This is a bit of a hack, but this might change
	   our route's effective color, so emit gui_changed
	   for our routes.
	*/

	for (RouteList::iterator i = routes->begin(); i != routes->end(); ++i) {
		(*i)->gui_changed (X_("color"), this);
	}
}

void
RouteGroup::set_monitoring (bool yn) 
{
	if (is_monitoring() == yn) {
		return;
	}

	_monitoring = yn;
	send_change (PropertyChange (Properties::monitoring));

	_session.set_dirty ();
}

void
RouteGroup::set_active (bool yn, void* /*src*/)
{
	if (is_active() == yn) {
		return;
	}

	_active = yn;
	send_change (PropertyChange (Properties::active));
	_session.set_dirty ();
}

void
RouteGroup::set_relative (bool yn, void* /*src*/)
{
	if (is_relative() == yn) {
		return;
	}
	_relative = yn;
	send_change (PropertyChange (Properties::relative));
	_session.set_dirty ();
}

void
RouteGroup::set_hidden (bool yn, void* /*src*/)
{
	if (is_hidden() == yn) {
		return;
	}

	if (yn) {
		_hidden = true;
		if (Config->get_hiding_groups_deactivates_groups()) {
			_active = false;
		}
	} else {
		_hidden = false;
		if (Config->get_hiding_groups_deactivates_groups()) {
			_active = true;
		}
	}

	send_change (Properties::hidden);

	_session.set_dirty ();
}

void
RouteGroup::audio_track_group (set<boost::shared_ptr<AudioTrack> >& ats)
{
	for (RouteList::iterator i = routes->begin(); i != routes->end(); ++i) {
		boost::shared_ptr<AudioTrack> at = boost::dynamic_pointer_cast<AudioTrack>(*i);
		if (at) {
			ats.insert (at);
		}
	}
}

void
RouteGroup::make_subgroup (bool aux, Placement placement)
{
	RouteList rl;
	uint32_t nin = 0;

	/* since we don't do MIDI Busses yet, check quickly ... */

	for (RouteList::iterator i = routes->begin(); i != routes->end(); ++i) {
		if ((*i)->output()->n_ports().n_midi() != 0) {
			PBD::info << _("You cannot subgroup MIDI tracks at this time") << endmsg;
			return;
		}
	}

	for (RouteList::iterator i = routes->begin(); i != routes->end(); ++i) {
		nin = max (nin, (*i)->output()->n_ports().n_audio());
	}

	try {
		/* use master bus etc. to determine default nouts */
		rl = _session.new_audio_route (nin, 2, 0, 1);
	} catch (...) {
		return;
	}

	subgroup_bus = rl.front();
	subgroup_bus->set_name (_name);

	if (aux) {

		_session.add_internal_sends (subgroup_bus, placement, routes);

	} else {

		boost::shared_ptr<Bundle> bundle = subgroup_bus->input()->bundle ();

		for (RouteList::iterator i = routes->begin(); i != routes->end(); ++i) {
			(*i)->output()->disconnect (this);
			(*i)->output()->connect_ports_to_bundle (bundle, false, this);
		}
	}
}

void
RouteGroup::destroy_subgroup ()
{
	if (!subgroup_bus) {
		return;
	}

	for (RouteList::iterator i = routes->begin(); i != routes->end(); ++i) {
		(*i)->output()->disconnect (this);
		/* XXX find a new bundle to connect to */
	}

	_session.remove_route (subgroup_bus);
	subgroup_bus.reset ();
}

bool
RouteGroup::has_subgroup() const
{
	return subgroup_bus != 0;
}

bool
RouteGroup::enabled_property (PBD::PropertyID prop)
{
	OwnedPropertyList::iterator i = _properties->find (prop);
	if (i == _properties->end()) {
		return false;
	}

	return dynamic_cast<const PropertyTemplate<bool>* > (i->second)->val ();
}
