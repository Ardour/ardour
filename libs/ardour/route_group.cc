/*
 * Copyright (C) 2006-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2007-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2015-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2016 Tim Mayberry <mojofunk@gmail.com>
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

#include <inttypes.h>

#include <algorithm>

#include "pbd/error.h"
#include "pbd/enumwriter.h"
#include "pbd/strsplit.h"
#include "pbd/types_convert.h"
#include "pbd/debug.h"

#include "ardour/amp.h"
#include "ardour/audio_track.h"
#include "ardour/debug.h"
#include "ardour/monitor_control.h"
#include "ardour/route.h"
#include "ardour/route_group.h"
#include "ardour/session.h"
#include "ardour/surround_send.h"
#include "ardour/vca.h"
#include "ardour/vca_manager.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;

namespace ARDOUR {
	namespace Properties {
		PropertyDescriptor<bool> active;
		PropertyDescriptor<bool> group_relative;
		PropertyDescriptor<bool> group_gain;
		PropertyDescriptor<bool> group_mute;
		PropertyDescriptor<bool> group_solo;
		PropertyDescriptor<bool> group_recenable;
		PropertyDescriptor<bool> group_sursend_enable;
		PropertyDescriptor<bool> group_select;
		PropertyDescriptor<bool> group_route_active;
		PropertyDescriptor<bool> group_color;
		PropertyDescriptor<bool> group_monitoring;
		PropertyDescriptor<int32_t> group_master_number;
	}
}

void
RouteGroup::make_property_quarks ()
{
	Properties::active.property_id = g_quark_from_static_string (X_("active"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for active = %1\n", Properties::active.property_id));

	Properties::group_relative.property_id = g_quark_from_static_string (X_("relative"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for relative = %1\n", Properties::group_relative.property_id));
	Properties::group_gain.property_id = g_quark_from_static_string (X_("gain"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for gain = %1\n", Properties::group_gain.property_id));
	Properties::group_mute.property_id = g_quark_from_static_string (X_("mute"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for mute = %1\n", Properties::group_mute.property_id));
	Properties::group_solo.property_id = g_quark_from_static_string (X_("solo"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for solo = %1\n", Properties::group_solo.property_id));
	Properties::group_recenable.property_id = g_quark_from_static_string (X_("recenable"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for recenable = %1\n", Properties::group_recenable.property_id));
	Properties::group_sursend_enable.property_id = g_quark_from_static_string (X_("sursend_enable"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for sursend_enable = %1\n", Properties::group_sursend_enable.property_id));
	Properties::group_select.property_id = g_quark_from_static_string (X_("select"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for select = %1\n", Properties::group_select.property_id));
	Properties::group_route_active.property_id = g_quark_from_static_string (X_("route-active"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for route-active = %1\n", Properties::group_route_active.property_id));
	Properties::group_color.property_id = g_quark_from_static_string (X_("color"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for color = %1\n", Properties::group_color.property_id));
	Properties::group_monitoring.property_id = g_quark_from_static_string (X_("monitoring"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for monitoring = %1\n", Properties::group_monitoring.property_id));
	Properties::group_master_number.property_id = g_quark_from_static_string (X_("group-master-number"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for group-master-number = %1\n", Properties::group_master_number.property_id));
}

#define ROUTE_GROUP_DEFAULT_PROPERTIES  _relative (Properties::group_relative, true) \
	, _active (Properties::active, true) \
	, _hidden (Properties::hidden, false) \
	, _gain (Properties::group_gain, true) \
	, _mute (Properties::group_mute, true) \
	, _solo (Properties::group_solo, true) \
	, _recenable (Properties::group_recenable, true) \
	, _sursend_enable (Properties::group_sursend_enable, true) \
	, _select (Properties::group_select, true) \
	, _route_active (Properties::group_route_active, true) \
	, _color (Properties::group_color, true) \
	, _monitoring (Properties::group_monitoring, true) \
	, _group_master_number (Properties::group_master_number, -1)

RouteGroup::RouteGroup (Session& s, const string &n)
	: SessionObject (s, n)
	, routes (new RouteList)
	, ROUTE_GROUP_DEFAULT_PROPERTIES
	, _solo_group (new ControlGroup (SoloAutomation))
	, _mute_group (new ControlGroup (MuteAutomation))
	, _rec_enable_group (new ControlGroup (RecEnableAutomation))
	, _sursend_enable_group (new ControlGroup (BusSendEnable))
	, _gain_group (new GainControlGroup ())
	, _monitoring_group (new ControlGroup (MonitoringAutomation))
	, _rgba (0)
	, _used_to_share_gain (false)
{
	_xml_node_name = X_("RouteGroup");

	add_property (_relative);
	add_property (_active);
	add_property (_hidden);
	add_property (_gain);
	add_property (_mute);
	add_property (_solo);
	add_property (_recenable);
	add_property (_sursend_enable);
	add_property (_select);
	add_property (_route_active);
	add_property (_color);
	add_property (_monitoring);
	add_property (_group_master_number);

	s.SurroundMasterAddedOrRemoved.connect_same_thread (*this, boost::bind (&RouteGroup::update_surround_sends, this));
}

RouteGroup::~RouteGroup ()
{
	_solo_group->clear ();
	_mute_group->clear ();
	_gain_group->clear ();
	_rec_enable_group->clear ();
	_sursend_enable_group->clear ();
	_monitoring_group->clear ();

	std::shared_ptr<VCA> vca (group_master.lock());

	for (RouteList::iterator i = routes->begin(); i != routes->end();) {
		RouteList::iterator tmp = i;
		++tmp;

		(*i)->set_route_group (0);

		if (vca) {
			(*i)->unassign (vca);
		}

		i = tmp;
	}
}

/** Add a route to a group.  Adding a route which is already in the group is allowed; nothing will happen.
 *  @param r Route to add.
 */
int
RouteGroup::add (std::shared_ptr<Route> r)
{
	if (r->is_master()) {
		return 0;
	}

	if (find (routes->begin(), routes->end(), r) != routes->end()) {
		return 0;
	}

	if (r->route_group()) {
		r->route_group()->remove (r);
	}

	routes->push_back (r);

	_solo_group->add_control (r->solo_control());
	_mute_group->add_control (r->mute_control());
	_gain_group->add_control (r->gain_control());
	std::shared_ptr<Track> trk = std::dynamic_pointer_cast<Track> (r);
	if (trk) {
		_rec_enable_group->add_control (trk->rec_enable_control());
		_monitoring_group->add_control (trk->monitoring_control());
	}

	if (r->surround_send ()) {
		_sursend_enable_group->add_control (r->surround_send ()->send_enable_control ());
	}

	r->set_route_group (this);
	r->DropReferences.connect_same_thread (*this, boost::bind (&RouteGroup::remove_when_going_away, this, std::weak_ptr<Route> (r)));

	std::shared_ptr<VCA> vca (group_master.lock());

	if (vca) {
		r->assign (vca);
	}

	_session.set_dirty ();
	RouteAdded (this, std::weak_ptr<Route> (r)); /* EMIT SIGNAL */
	return 0;
}

void
RouteGroup::remove_when_going_away (std::weak_ptr<Route> wr)
{
	std::shared_ptr<Route> r (wr.lock());

	if (r) {
		remove (r);
	}
}

void
RouteGroup::update_surround_sends ()
{
	for (auto const& r : *routes) {
		if (r->surround_send ()) {
			_sursend_enable_group->add_control (r->surround_send ()->send_enable_control ());
		}
		// Note: ctrl is removed via DropReferences
	}
}

void
RouteGroup::unset_subgroup_bus ()
{
	if (_session.deletion_in_progress()) {
		return;
	}
	_subgroup_bus.reset ();
}

int
RouteGroup::remove (std::shared_ptr<Route> r)
{
	RouteList::iterator i;

	if ((i = find (routes->begin(), routes->end(), r)) != routes->end()) {
		r->set_route_group (0);

		std::shared_ptr<VCA> vca = group_master.lock();

		if (vca) {
			r->unassign (vca);
		}

		_solo_group->remove_control (r->solo_control());
		_mute_group->remove_control (r->mute_control());
		_gain_group->remove_control (r->gain_control());
		std::shared_ptr<Track> trk = std::dynamic_pointer_cast<Track> (r);
		if (trk) {
			_rec_enable_group->remove_control (trk->rec_enable_control());
			_monitoring_group->remove_control (trk->monitoring_control());
		}
		if (r->surround_send ()) {
			_sursend_enable_group->remove_control (r->surround_send ()->send_enable_control ());
		}
		routes->erase (i);
		_session.set_dirty ();
		RouteRemoved (this, std::weak_ptr<Route> (r)); /* EMIT SIGNAL */
		return 0;
	}

	return -1;
}

void
RouteGroup::set_rgba (uint32_t color) {
	_rgba = color;

	PBD::PropertyChange change;
	change.add (Properties::color);
	PropertyChanged (change);

	if (!is_color ()) {
		return;
	}

	for (RouteList::const_iterator i = routes->begin(); i != routes->end(); ++i) {
		(*i)->presentation_info().PropertyChanged (Properties::color);
	}
}

XMLNode&
RouteGroup::get_state () const
{
	XMLNode *node = new XMLNode ("RouteGroup");

	node->set_property ("id", id());
	node->set_property ("rgba", _rgba);
	node->set_property ("used-to-share-gain", _used_to_share_gain);
	if (_subgroup_bus) {
		node->set_property ("subgroup-bus", _subgroup_bus->id ());
	}

	add_properties (*node);

	if (!routes->empty()) {
		stringstream str;

		for (RouteList::iterator i = routes->begin(); i != routes->end(); ++i) {
			str << (*i)->id () << ' ';
		}

		node->set_property ("routes", str.str());
	}

	return *node;
}

int
RouteGroup::set_state (const XMLNode& node, int version)
{
	if (version < 3000) {
		return set_state_2X (node, version);
	}

	set_id (node);
	set_values (node);
	node.get_property ("rgba", _rgba);
	node.get_property ("used-to-share-gain", _used_to_share_gain);

	std::string routes;
	if (node.get_property ("routes", routes)) {
		stringstream str (routes);
		vector<string> ids;
		split (str.str(), ids, ' ');

		for (vector<string>::iterator i = ids.begin(); i != ids.end(); ++i) {
			PBD::ID id (*i);
			std::shared_ptr<Route> r = _session.route_by_id (id);

			if (r) {
				add (r);
			}
		}
	}

	PBD::ID subgroup_id (0);
	if (node.get_property ("subgroup-bus", subgroup_id)) {
		std::shared_ptr<Route> r = _session.route_by_id (subgroup_id);
		if (r) {
			_subgroup_bus = r;
			_subgroup_bus->DropReferences.connect_same_thread (*this, boost::bind (&RouteGroup::unset_subgroup_bus, this));
		}
	}

	if (_group_master_number.val() > 0) {
		std::shared_ptr<VCA> vca = _session.vca_manager().vca_by_number (_group_master_number.val());
		if (vca) {
			/* no need to do the assignment because slaves will
			   handle that themselves. But we can set group_master
			   to use with future assignments of newly added routes.
			*/
			group_master = vca;
		}
	}

	push_to_groups ();

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

	push_to_groups ();

	return 0;
}

void
RouteGroup::set_gain (bool yn)
{
	if (is_gain() == yn) {
		return;
	}

	_gain = yn;
	_gain_group->set_active (yn);

	send_change (PropertyChange (Properties::group_gain));
}

void
RouteGroup::set_mute (bool yn)
{
	if (is_mute() == yn) {
		return;
	}
	_mute = yn;
	_mute_group->set_active (yn);

	send_change (PropertyChange (Properties::group_mute));
}

void
RouteGroup::set_solo (bool yn)
{
	if (is_solo() == yn) {
		return;
	}
	_solo = yn;
	_solo_group->set_active (yn);

	send_change (PropertyChange (Properties::group_solo));
}

void
RouteGroup::set_recenable (bool yn)
{
	if (is_recenable() == yn) {
		return;
	}
	_recenable = yn;
	_rec_enable_group->set_active (yn);
	send_change (PropertyChange (Properties::group_recenable));
}

void
RouteGroup::set_sursend_enable (bool yn)
{
	if (is_sursend_enable() == yn) {
		return;
	}
	_sursend_enable = yn;
	_sursend_enable_group->set_active (yn);
	send_change (PropertyChange (Properties::group_sursend_enable));
}

void
RouteGroup::set_select (bool yn)
{
	if (is_select() == yn) {
		return;
	}
	_select = yn;
	send_change (PropertyChange (Properties::group_select));
}

void
RouteGroup::set_route_active (bool yn)
{
	if (is_route_active() == yn) {
		return;
	}
	_route_active = yn;
	send_change (PropertyChange (Properties::group_route_active));
}

void
RouteGroup::set_color (bool yn)
{
	if (is_color() == yn) {
		return;
	}
	_color = yn;

	send_change (PropertyChange (Properties::group_color));

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
	_monitoring_group->set_active (yn);

	send_change (PropertyChange (Properties::group_monitoring));

	_session.set_dirty ();
}

void
RouteGroup::set_active (bool yn, void* /*src*/)
{
	if (is_active() == yn) {
		return;
	}

	_active = yn;

	push_to_groups ();

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

	push_to_groups ();

	send_change (PropertyChange (Properties::group_relative));
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
RouteGroup::audio_track_group (set<std::shared_ptr<AudioTrack> >& ats)
{
	for (RouteList::iterator i = routes->begin(); i != routes->end(); ++i) {
		std::shared_ptr<AudioTrack> at = std::dynamic_pointer_cast<AudioTrack>(*i);
		if (at) {
			ats.insert (at);
		}
	}
}

bool
RouteGroup::check_subgroup (bool aux, Placement placement, DataType& dt, uint32_t& nin) const
{
	assert (routes->size () > 0);

	if (has_subgroup ()) {
		return false;
	}

	bool midi_only  = true; // no audio ports at all
	bool audio_ok   = true; // all have at least 1 audio port
	bool have_midi  = false; // at least 1 MIDI port
	bool have_audio = false; // at least 1 Audio port

	for (auto const& r : *routes) {
#ifdef MIXBUS
		if (r->mixbus ()) {
			return false;
		}
#endif
		ChanCount cc (r->output()->n_ports());
		if (aux) {
			std::shared_ptr<Processor> proc = (placement == PreFader) ? std::dynamic_pointer_cast <Processor> (r->amp ()) : std::dynamic_pointer_cast <Processor> (r->main_outs ());
			if (proc) {
				cc = proc->input_streams ();
			}
		}

		if (cc.n_audio() == 0) {
			audio_ok = false;
		} else {
			have_audio = true;
			midi_only  = false;
		}

		if (cc.n_midi() == 0) {
			midi_only = false;
		} else {
			have_midi = true;
		}
	}

	/* if all tracks only have a MIDI output -> MIDI subgroup */
	dt  = midi_only ? DataType::MIDI : DataType::AUDIO;
	nin = 0;

	/* for aux, all tracks need to have at least one of a given data-type */
	if (aux) {
		if (!(midi_only && have_midi) && !(audio_ok && have_audio)) {
			return false;
		}
	}

	bool have_one = false;

	for (auto const& r : *routes) {
		ChanCount cc (r->output()->n_ports());
		if (aux) {
			std::shared_ptr<Processor> proc = (placement == PreFader) ? std::dynamic_pointer_cast <Processor> (r->amp ()) : std::dynamic_pointer_cast <Processor> (r->main_outs ());
			if (proc) {
				cc = proc->input_streams ();
			}
		}
		if (have_one && !aux && nin != cc.get (dt)) {
			return false;
		}
		nin = max (nin, cc.get(dt));
		have_one = true;
	}

	return have_one && nin > 0;
}

bool
RouteGroup::can_subgroup (bool aux, Placement placement) const
{
	DataType dt (DataType::NIL);
	uint32_t nin;
	return check_subgroup (aux, placement, dt, nin);
}

void
RouteGroup::make_subgroup (bool aux, Placement placement)
{
	DataType  dt (DataType::NIL);
	uint32_t  nin;
	RouteList rl;

	if (!check_subgroup (aux, placement, dt, nin)) {
		if (has_subgroup ()) {
			PBD::warning << _("So far only one subgroup per group is supported") << endmsg;
		} else {
			PBD::warning << _("You cannot subgroup tracks with different type or number of ports.") << endmsg;
		}
		return;
	}

	try {
		if (dt == DataType::MIDI) {
			rl = _session.new_midi_route (0, 1, string(), true, std::shared_ptr<PluginInfo>(), 0, PresentationInfo::MidiBus, PresentationInfo::max_order);
		} else {
			uint32_t nout = nin;
			if (_session.master_out ()) {
				nout = std::max (nout, _session.master_out ()->n_inputs ().n_audio ());
			}
			rl = _session.new_audio_route (nin, nout, 0, 1, string(), PresentationInfo::AudioBus, PresentationInfo::max_order);
		}
	} catch (...) {
		return;
	}

	_subgroup_bus = rl.front();
	_subgroup_bus->set_name (_name);
	_subgroup_bus->DropReferences.connect_same_thread (*this, boost::bind (&RouteGroup::unset_subgroup_bus, this));

	if (aux) {

		_session.add_internal_sends (_subgroup_bus, placement, routes);

	} else {

		std::shared_ptr<Bundle> bundle = _subgroup_bus->input()->bundle ();

		for (RouteList::iterator i = routes->begin(); i != routes->end(); ++i) {
			(*i)->output()->disconnect (this);
			(*i)->output()->connect_ports_to_bundle (bundle, false, true, this);
		}
	}
}

void
RouteGroup::destroy_subgroup ()
{
	if (!_subgroup_bus) {
		return;
	}

	for (RouteList::iterator i = routes->begin(); i != routes->end(); ++i) {
		(*i)->output()->disconnect (this);
		/* XXX find a new bundle to connect to */
	}

	_session.remove_route (_subgroup_bus);
	_subgroup_bus.reset ();
}

bool
RouteGroup::has_subgroup() const
{
	return _subgroup_bus != 0;
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

void
RouteGroup::post_set (PBD::PropertyChange const &)
{
	push_to_groups ();
}

void
RouteGroup::push_to_groups ()
{
	if (is_relative()) {
		_gain_group->set_mode (ControlGroup::Mode (_gain_group->mode()|ControlGroup::Relative));
	} else {
		_gain_group->set_mode (ControlGroup::Mode (_gain_group->mode()&~ControlGroup::Relative));
	}

	if (_active) {
		_gain_group->set_active (is_gain());
		_solo_group->set_active (is_solo());
		_mute_group->set_active (is_mute());
		_rec_enable_group->set_active (is_recenable());
		_sursend_enable_group->set_active (is_sursend_enable());
		_monitoring_group->set_active (is_monitoring());
	} else {
		_gain_group->set_active (false);
		_solo_group->set_active (false);
		_mute_group->set_active (false);
		_rec_enable_group->set_active (false);
		_sursend_enable_group->set_active (false);
		_monitoring_group->set_active (false);
	}
}

void
RouteGroup::assign_master (std::shared_ptr<VCA> master)
{
	if (!routes || routes->empty()) {
		return;
	}

	std::shared_ptr<Route> front = routes->front ();

	if (front->slaved_to (master)) {
		return;
	}

	for (RouteList::iterator r = routes->begin(); r != routes->end(); ++r) {
		(*r)->assign (master);
	}

	group_master = master;
	_group_master_number = master->number();

	_used_to_share_gain = is_gain ();
	set_gain (false);
}

void
RouteGroup::unassign_master (std::shared_ptr<VCA> master)
{
	if (!routes || routes->empty()) {
		return;
	}

	std::shared_ptr<Route> front = routes->front ();

	if (!front->slaved_to (master)) {
		return;
	}

	for (RouteList::iterator r = routes->begin(); r != routes->end(); ++r) {
		(*r)->unassign (master);
	}

	group_master.reset ();
	_group_master_number = -1;

	set_gain (_used_to_share_gain);
}

bool
RouteGroup::slaved () const
{
	if (!routes || routes->empty()) {
		return false;
	}

	return routes->front()->slaved ();
}

bool
RouteGroup::has_control_master() const
{
	return group_master.lock() != 0;
}
