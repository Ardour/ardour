/*
 * Copyright (C) 2024 Robin Gareus <robin@gareus.org>
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
#include "libardour-config.h"
#endif

#include <string>

#include "pbd/assert.h"
#include "pbd/types_convert.h"
#include "pbd/xml++.h"

#include "ardour/automatable.h"
#include "ardour/debug.h"
#include "ardour/plugin.h"
#include "ardour/readonly_control.h"
#include "ardour/region_fx_plugin.h"
#include "ardour/session.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

RegionFxPlugin::RegionFxPlugin (Session& s, Temporal::TimeDomain const td, std::shared_ptr<Plugin> plug)
	: SessionObject (s, (plug ? plug->name () : string ("toBeRenamed")))
	, TimeDomainProvider (td)
	, _plugin_signal_latency (0)
	, _configured (false)
	, _no_inplace (false)
	, _window_proxy (0)
{
	_flush.store (0);

	if (plug) {
		add_plugin (plug);
		create_parameters ();
	}
}

RegionFxPlugin::~RegionFxPlugin ()
{
	for (auto const& i : _control_outputs) {
		std::dynamic_pointer_cast<ReadOnlyControl>(i.second)->drop_references ();
	}

	Glib::Threads::Mutex::Lock lm (_control_lock);
	for (auto const& i : _controls) {
		std::dynamic_pointer_cast<AutomationControl>(i.second)->drop_references ();
	}
	_controls.clear ();
}

XMLNode&
RegionFxPlugin::get_state () const
{
	XMLNode* node = new XMLNode (/*state_node_name*/ "RegionFXPlugin");

	Latent::add_state (node);

	node->set_property ("type", _plugins[0]->state_node_name ());
	node->set_property ("unique-id", _plugins[0]->unique_id ());
	node->set_property ("count", (uint32_t)_plugins.size ());

	node->set_property ("id", id ());
	node->set_property ("name", name ());

	_plugins[0]->set_insert_id (this->id ());
	node->add_child_nocopy (_plugins[0]->get_state ());

	for (auto const& c : controls ()) {
		std::shared_ptr<AutomationControl> ac = std::dynamic_pointer_cast<AutomationControl> (c.second);
		if (!ac) {
			continue;
		}
		node->add_child_nocopy (ac->get_state ());
		std::shared_ptr<AutomationList> l = ac->alist ();
		if (l && 0 == (ac->flags() & Controllable::NotAutomatable)) {
			node->add_child_nocopy (l->get_state ());
		}
	}

	return *node;
}

int
RegionFxPlugin::set_state (const XMLNode& node, int version)
{
	set_id (node);

	/* with ::regenerate_xml_or_string_ids(), set_id() creates a new ID */
	PBD::ID new_id = this->id();
	PBD::ID old_id = this->id();

	node.get_property ("id", old_id);

	ARDOUR::PluginType type;
	std::string        unique_id;
	if (!parse_plugin_type (node, type, unique_id)) {
		return -1;
	}

	bool any_vst;

	uint32_t count = 1;
	node.get_property ("count", count);

	if (_plugins.empty()) {
		std::shared_ptr<Plugin> plugin = find_and_load_plugin (_session, node, type, unique_id, any_vst);

		if (!plugin) {
			return -1;
		}

		add_plugin (plugin);
		create_parameters ();
		set_control_ids (node, version);

		if (_plugins.size () != count) {
			for (uint32_t n = 1; n < count; ++n) {
				add_plugin (plugin_factory (plugin));
			}
		}

	} else {
		assert (_plugins[0]->unique_id() == unique_id);
		set_control_ids (node, version, true);
	}

	string name;
	if (node.get_property ("name", name)) {
		set_name (name);
	} else {
		set_name (_plugins[0]->get_info ()->name);
	}

	XMLNodeList     nlist = node.children ();
	XMLNodeIterator niter;

	for (niter = nlist.begin (); niter != nlist.end (); ++niter) {
		if ((*niter)->name () != "AutomationList") {
			continue;
		}
		XMLProperty const* id_prop = (*niter)->property("automation-id");
		if (!id_prop) {
			assert (0);
			continue;
		}
		Evoral::Parameter param = EventTypeMap::instance().from_symbol(id_prop->value());
		std::shared_ptr<Evoral::Control> c = control (param);
		std::shared_ptr<AutomationControl> ac = std::dynamic_pointer_cast<AutomationControl> (c);
		if (ac && ac->alist () && 0 == (ac->flags() & Controllable::NotAutomatable)) {
			ac->alist()->set_state (**niter, version);
		}
	}

	for (niter = nlist.begin (); niter != nlist.end (); ++niter) {
		if ((*niter)->name () == _plugins[0]->state_node_name () || (any_vst && ((*niter)->name () == "lxvst" || (*niter)->name () == "windows-vst" || (*niter)->name () == "mac-vst"))) {
			for (auto const& i : _plugins) {
				if (!regenerate_xml_or_string_ids ()) {
					i->set_insert_id (new_id);
				} else {
					i->set_insert_id (old_id);
				}

				i->set_state (**niter, version);

				if (regenerate_xml_or_string_ids ()) {
					i->set_insert_id (new_id);
				}
			}
		}
	}
	for (auto const& i : _plugins) {
		i->activate ();
	}

	/* when copying plugin state, notify UI */
	for (auto const& i : _controls) {
		std::shared_ptr<AutomationControl> ac = std::dynamic_pointer_cast<AutomationControl> (i.second);
		if (ac) {
			ac->Changed (false, Controllable::NoGroup); /* EMIT SIGNAL */
		}
	}
	return 0;
}

void
RegionFxPlugin::update_id (PBD::ID id)
{
	set_id (id.to_s());
	for (Plugins::iterator i = _plugins.begin(); i != _plugins.end(); ++i) {
		(*i)->set_insert_id (id);
	}
}

void
RegionFxPlugin::add_plugin (std::shared_ptr<Plugin> plugin)
{
	plugin->set_insert_id (this->id ());
	plugin->set_non_realtime (true);

	if (_plugins.empty ()) {
		/* first (and probably only) plugin instance - connect to relevant signals */
		plugin->ParameterChangedExternally.connect_same_thread (*this, boost::bind (&RegionFxPlugin::parameter_changed_externally, this, _1, _2));
		plugin->StartTouch.connect_same_thread (*this, boost::bind (&RegionFxPlugin::start_touch, this, _1));
		plugin->EndTouch.connect_same_thread (*this, boost::bind (&RegionFxPlugin::end_touch, this, _1));
	}

	plugin->set_insert (this, _plugins.size ());

	_plugins.push_back (plugin);

	if (_plugins.size () > 1) {
		_plugins[0]->add_slave (plugin, true);
		plugin->DropReferences.connect_same_thread (*this, boost::bind (&RegionFxPlugin::plugin_removed, this, std::weak_ptr<Plugin> (plugin)));
	}
}

void
RegionFxPlugin::plugin_removed (std::weak_ptr<Plugin> wp)
{
	std::shared_ptr<Plugin> plugin = wp.lock ();
	if (_plugins.size () == 0 || !plugin) {
		return;
	}
	_plugins[0]->remove_slave (plugin);
}

bool
RegionFxPlugin::set_count (uint32_t num)
{
	bool require_state = !_plugins.empty ();

	if (require_state && num > 1 && plugin (0)->get_info ()->type == ARDOUR::AudioUnit) {
		// we don't allow to replicate AUs
		return false;
	}

	if (num == 0) {
		return false;
	} else if (num > _plugins.size ()) {
		uint32_t diff = num - _plugins.size ();

		for (uint32_t n = 0; n < diff; ++n) {
			std::shared_ptr<Plugin> p = plugin_factory (_plugins[0]);
			add_plugin (p);

			if (require_state) {
				_plugins[0]->set_insert_id (this->id ());
				XMLNode& state = _plugins[0]->get_state ();
				p->set_state (state, Stateful::current_state_version);
				delete &state;
			}
			p->activate ();
		}

	} else if (num < _plugins.size ()) {
		uint32_t diff = _plugins.size () - num;
		for (uint32_t n = 0; n < diff; ++n) {
			_plugins.back ()->drop_references ();
			_plugins.pop_back ();
		}
	}
	return true;
}

void
RegionFxPlugin::drop_references ()
{
	for (Plugins::iterator i = _plugins.begin (); i != _plugins.end (); ++i) {
		(*i)->drop_references ();
	}

	SessionObject::drop_references ();
}

ARDOUR::samplecnt_t
RegionFxPlugin::signal_latency () const
{
	return _plugins.front ()->signal_latency ();
}

PlugInsertBase::UIElements
RegionFxPlugin::ui_elements () const
{
	return PluginPreset;
}

void
RegionFxPlugin::create_parameters ()
{
	assert (!_plugins.empty ());

	std::shared_ptr<Plugin> plugin = _plugins.front ();
	set<Evoral::Parameter>  a      = _plugins.front ()->automatable ();

	for (uint32_t i = 0; i < plugin->parameter_count (); ++i) {
		if (!plugin->parameter_is_control (i)) {
			continue;
		}

		ParameterDescriptor desc;
		plugin->get_parameter_descriptor (i, desc);

		if (!plugin->parameter_is_input (i)) {
			_control_outputs[i] = std::shared_ptr<ReadOnlyControl> (new ReadOnlyControl (plugin, desc, i));
			continue;
		}

		Evoral::Parameter param (PluginAutomation, 0, i);
		const bool automatable = a.find(param) != a.end();

		std::shared_ptr<AutomationList> list (new AutomationList (param, desc, *this));
		std::shared_ptr<AutomationControl> c (new PluginControl (_session, this, param, desc, list));
		if (!automatable) {
			c->set_flag (Controllable::NotAutomatable);
		}
		add_control (c);

		plugin->set_automation_control (i, c);
	}

	Plugin::PropertyDescriptors const& pdl (plugin->get_supported_properties ());

	for (Plugin::PropertyDescriptors::const_iterator p = pdl.begin (); p != pdl.end (); ++p) {
		Evoral::Parameter          param (PluginPropertyAutomation, 0, p->first);
		ParameterDescriptor const& desc = plugin->get_property_descriptor (param.id ());
		if (desc.datatype == Variant::NOTHING) {
			continue;
		}
		std::shared_ptr<AutomationControl> c (new PluginPropertyControl (_session, this, param, desc));
		c->set_flag (Controllable::NotAutomatable);
		add_control (c);
	}

	plugin->PresetPortSetValue.connect_same_thread (*this, boost::bind (&RegionFxPlugin::preset_load_set_value, this, _1, _2));
}

void
RegionFxPlugin::set_default_automation (timepos_t end)
{
	for (auto const& i : _controls) {
		std::shared_ptr<AutomationControl> ac = std::dynamic_pointer_cast<AutomationControl> (i.second);
		if (ac->alist ()->empty ()) {
			ac->alist ()->fast_simple_add (timepos_t (time_domain ()), ac->normal ());
			ac->alist ()->fast_simple_add (end, ac->normal ());
		} else {
			ac->alist ()->truncate_end (end);
		}
	}
}

void
RegionFxPlugin::truncate_automation_start (timecnt_t start)
{
	for (auto const& i : _controls) {
		std::shared_ptr<AutomationControl> ac = std::dynamic_pointer_cast<AutomationControl> (i.second);
		ac->alist ()->truncate_start (start);
	}
}

void
RegionFxPlugin::truncate_automation_end (timepos_t end)
{
	for (auto const& i : _controls) {
		std::shared_ptr<AutomationControl> ac = std::dynamic_pointer_cast<AutomationControl> (i.second);
		ac->alist ()->truncate_end (end);
	}
}

bool
RegionFxPlugin::write_immediate_event (Evoral::EventType event_type, size_t size, const uint8_t* buf)
{
	bool rv = true;
	for (auto const& i : _plugins) {
		if (!i->write_immediate_event (event_type, size, buf)) {
			rv = false;
		}
	}
	return rv;
}

bool
RegionFxPlugin::load_preset (ARDOUR::Plugin::PresetRecord pr)
{
	bool rv = true;
	for (auto const& i : _plugins) {
		if (!i->load_preset (pr)) {
			rv = false;
		}
	}
	return rv;
}

std::shared_ptr<ReadOnlyControl>
RegionFxPlugin::control_output (uint32_t num) const
{
	CtrlOutMap::const_iterator i = _control_outputs.find (num);
	if (i == _control_outputs.end ()) {
		return std::shared_ptr<ReadOnlyControl> ();
	} else {
		return (*i).second;
	}
}

void
RegionFxPlugin::parameter_changed_externally (uint32_t which, float val)
{
	std::shared_ptr<Evoral::Control> c  = control (Evoral::Parameter (PluginAutomation, 0, which));
	std::shared_ptr<PluginControl>   pc = std::dynamic_pointer_cast<PluginControl> (c);

	if (pc) {
		pc->catch_up_with_external_value (val);
	}

	/* Second propagation: tell all plugins except the first to
	 * update the value of this parameter. For sane plugin APIs,
	 * there are no other plugins, so this is a no-op in those
	 * cases.
	 */

	Plugins::iterator i = _plugins.begin ();

	/* don't set the first plugin, just all the slaves */

	if (i != _plugins.end ()) {
		++i;
		for (; i != _plugins.end (); ++i) {
			(*i)->set_parameter (which, val, 0);
		}
	}
}

std::string
RegionFxPlugin::describe_parameter (Evoral::Parameter param)
{
	if (param.type () == PluginAutomation) {
		return _plugins[0]->describe_parameter (param);
	} else if (param.type () == PluginPropertyAutomation) {
		std::shared_ptr<AutomationControl> c = std::dynamic_pointer_cast<AutomationControl> (control (param));
		if (c && !c->desc ().label.empty ()) {
			return c->desc ().label;
		}
	}
	return EventTypeMap::instance ().to_symbol (param);
}

void
RegionFxPlugin::start_touch (uint32_t param_id)
{
	assert (0); // touch is N/A
	std::shared_ptr<AutomationControl> ac = std::dynamic_pointer_cast<AutomationControl> (control (Evoral::Parameter (PluginAutomation, 0, param_id)));
	if (ac) {
		ac->start_touch (timepos_t (_session.audible_sample ())); // XXX subtract region position
	}
}

void
RegionFxPlugin::end_touch (uint32_t param_id)
{
	assert (0); // touch is N/A
	std::shared_ptr<AutomationControl> ac = std::dynamic_pointer_cast<AutomationControl> (control (Evoral::Parameter (PluginAutomation, 0, param_id)));
	if (ac) {
		ac->stop_touch (timepos_t (_session.audible_sample ())); // XXX subtract region position
	}
}

bool
RegionFxPlugin::can_reset_all_parameters ()
{
	bool                    all    = true;
	uint32_t                params = 0;
	std::shared_ptr<Plugin> plugin = _plugins.front ();
	for (uint32_t par = 0; par < plugin->parameter_count (); ++par) {
		bool           ok  = false;
		const uint32_t cid = plugin->nth_parameter (par, ok);

		if (!ok || !plugin->parameter_is_input (cid)) {
			continue;
		}

		std::shared_ptr<AutomationControl> ac = std::dynamic_pointer_cast<AutomationControl> (control (Evoral::Parameter (PluginAutomation, 0, cid)));
		if (!ac) {
			continue;
		}

		++params;

		if (ac->automation_state () & Play) {
			all = false;
			break;
		}
	}
	return all && (params > 0);
}

bool
RegionFxPlugin::reset_parameters_to_default ()
{
	bool                    all    = true;
	std::shared_ptr<Plugin> plugin = _plugins.front ();

	for (uint32_t par = 0; par < plugin->parameter_count (); ++par) {
		bool           ok  = false;
		const uint32_t cid = plugin->nth_parameter (par, ok);

		if (!ok || !plugin->parameter_is_input (cid)) {
			continue;
		}

		const float dflt = plugin->default_value (cid);
		const float curr = plugin->get_parameter (cid);

		if (dflt == curr) {
			continue;
		}

		std::shared_ptr<AutomationControl> ac = std::dynamic_pointer_cast<AutomationControl> (control (Evoral::Parameter (PluginAutomation, 0, cid)));
		if (!ac) {
			continue;
		}

		if (ac->automation_state () & Play) {
			all = false;
			continue;
		}

		ac->set_value (dflt, Controllable::NoGroup);
	}
	return all;
}

void
RegionFxPlugin::flush ()
{
	_flush.store (1);
}

bool
RegionFxPlugin::can_support_io_configuration (const ChanCount& in, ChanCount& out)
{
	return private_can_support_io_configuration (in, out).method != Impossible;
}

PlugInsertBase::Match
RegionFxPlugin::private_can_support_io_configuration (ChanCount const& in, ChanCount& out) const
{
	assert (!_plugins.empty ());
	PluginInfoPtr info = _plugins.front ()->get_info ();
	ChanCount     aux_in;

	/* count sidechain inputs */
	const ChanCount& nis (info->n_inputs);
	for (DataType::iterator t = DataType::begin (); t != DataType::end (); ++t) {
		for (uint32_t in = 0; in < nis.get (*t); ++in) {
			const Plugin::IOPortDescription& iod (plugin (0)->describe_io_port (*t, true, in));
			if (iod.is_sidechain) {
				aux_in.set (*t, 1 + aux_in.n (*t));
			}
		}
	}

	if (info->reconfigurable_io ()) {
		ChanCount  inx (in);
		bool const r = _plugins.front ()->match_variable_io (inx, aux_in, out);
		if (!r) {
			return Match (Impossible, 0);
		}
		out = ChanCount::min (in, out);
		return Match (Delegate, 1, true);
	}

	ChanCount inputs  = info->n_inputs - aux_in;
	ChanCount outputs = info->n_outputs;

	bool no_inputs = true;
	for (DataType::iterator t = DataType::begin (); t != DataType::end (); ++t) {
		if (inputs.get (*t) != 0) {
			no_inputs = false;
			break;
		}
	}

	if (no_inputs) {
		/* RegionFX cannot be generators */
		return Match (Impossible, 0);
	}

	if (inputs == in && outputs == in) {
		out = outputs;
		return Match (ExactMatch, 1);
	}

	/* if the plugin has more outputs than we need, we ignore them */
	if (inputs == in && outputs > in) {
		out = inputs;
		return Match (Split, 1);
	}

	/* test replication of mono plugins */
	uint32_t f             = 0;
	bool     can_replicate = true;

	for (DataType::iterator t = DataType::begin (); t != DataType::end (); ++t) {
		int32_t nin = inputs.get (*t);
		/* No inputs of this type */
		if (nin == 0 && in.get (*t) == 0) {
			continue;
		}
		if (nin != 1 || outputs.get (*t) != 1) {
			can_replicate = false;
			break;
		}

		if (f == 0) {
			f = in.get (*t) / nin;
		}
		if (f != (in.get (*t) / nin)) {
			can_replicate = false;
			break;
		}
	}

	if (can_replicate && f > 0) {
		for (DataType::iterator t = DataType::begin (); t != DataType::end (); ++t) {
			out.set (*t, outputs.get (*t) * f);
		}
		return Match (Replicate, f);
	}

	 /* If the plugin has more inputs than we want, we can `hide' some of them by feeding them silence. */
	bool      could_hide  = false;
	bool      cannot_hide = false;
	ChanCount hide_channels;

	for (DataType::iterator t = DataType::begin (); t != DataType::end (); ++t) {
		if (inputs.get (*t) > in.get (*t)) {
			/* there is potential to hide, since the plugin has more inputs of type t than the insert */
			hide_channels.set (*t, inputs.get (*t) - in.get (*t));
			could_hide = true;
		} else if (inputs.get (*t) < in.get (*t)) {
			/* we definitely cannot hide, since the plugin has fewer inputs of type t than the insert */
			cannot_hide = true;
		}
	}

	if (could_hide && !cannot_hide && outputs >= in) {
		out = ChanCount::min (in, outputs);
		return Match (Hide, 1, false, false, hide_channels);
	}

	/* Test replication of multi-channel plugins:
	 * (at least as many plugins so that output count matches input count)
	 */
	f = 0;
	for (DataType::iterator t = DataType::begin (); t != DataType::end (); ++t) {
		uint32_t nin  = inputs.get (*t);
		uint32_t nout = outputs.get (*t);
		if (nin == 0 || in.get (*t) == 0 || nout == 0) {
			continue;
		}
		// prefer floor() so the count won't overly increase IFF (nin < nout)
		f = max (f, (uint32_t)floor (in.get (*t) / (float)nout));
	}
	if (f > 0 && outputs * f >= out) {
		out = ChanCount::min (in, outputs * f);
		return Match (Replicate, f, true);
	}

	/* Test replication of multi-channel plugins:
	 * (at least as many plugins to connect all inputs)
	 */
	f = 0;
	for (DataType::iterator t = DataType::begin (); t != DataType::end (); ++t) {
		uint32_t nin = inputs.get (*t);
		if (nin == 0 || in.get (*t) == 0) {
			continue;
		}
		f = max (f, (uint32_t)ceil (in.get (*t) / (float)nin));
	}
	if (f > 0) {
		out = ChanCount::min (in, outputs * f);
		return Match (Replicate, f, true);
	}

	return Match (Impossible, 0);
}

bool
RegionFxPlugin::configure_io (ChanCount in, ChanCount out)
{
	_configured_in  = in;
	_configured_out = out;

	ChanCount natural_input_streams  = _plugins[0]->get_info ()->n_inputs;
	ChanCount natural_output_streams = _plugins[0]->get_info ()->n_outputs;

	_match = private_can_support_io_configuration (in, out);

	if (set_count (_match.plugins) == false) {
		return false;
	}

	/* configure plugins */
	switch (_match.method) {
		case Split:
			/* fallthrough */
		case Hide:
			if (_plugins.front ()->reconfigure_io (natural_input_streams, ChanCount (), out) == false) {
				return false;
			}
			break;
		case Delegate: {
			ChanCount  din (in);
			ChanCount  daux; // no sidechain ports
			ChanCount  dout (_configured_out);
			bool const r = _plugins.front ()->match_variable_io (din, daux, dout);
			assert (r);
			if (_plugins.front ()->reconfigure_io (din, daux, dout) == false) {
				return false;
			}
			DEBUG_TRACE (DEBUG::RegionFx, string_compose ("Delegate configured in: %1, out: %2 for in: %3 out: %4", din, dout, in, _configured_out));
			if (din < in || dout < _configured_out) {
				return false;
			}
		}
			break;
		case Replicate:
			assert (!_plugins.front ()->get_info ()->reconfigurable_io ());
			break;
		default:
			if (_plugins.front ()->reconfigure_io (in, ChanCount (), out) == false) {
				return false;
			}
			break;
	}

	/* compare to PluginInsert::reset_map */
	_in_map.clear ();
	_out_map.clear ();

	/* builld input map, skip plugin sidechain pin. */
	for (DataType::iterator t = DataType::begin (); t != DataType::end (); ++t) {
		uint32_t       pc   = 0;
		uint32_t       cin  = 0;
		const uint32_t cend = _configured_in.get (*t);
		for (Plugins::iterator i = _plugins.begin (); i != _plugins.end (); ++i, ++pc) {
			const uint32_t nis = natural_input_streams.get (*t);
			for (uint32_t in = 0; in < nis; ++in) {
				const Plugin::IOPortDescription& iod (_plugins[pc]->describe_io_port (*t, true, in));
				if (iod.is_sidechain) {
					/* leave N/C */
					continue;
				}
				if (cin < cend) {
					_in_map[pc].set (*t, in, cin);
					++cin;
				} else {
					break;
				}
			}
		}
	}

	/* build output map */
	uint32_t pc = 0;
	for (Plugins::iterator i = _plugins.begin (); i != _plugins.end (); ++i, ++pc) {
		_out_map[pc] = ChanMapping (ChanCount::min (natural_output_streams, _configured_out));
		for (DataType::iterator t = DataType::begin (); t != DataType::end (); ++t) {
			_out_map[pc].offset_to (*t, pc * natural_output_streams.get (*t));
		}
	}

	/* now sanitize maps */
	for (uint32_t pc = 0; pc < get_count (); ++pc) {
		ChanMapping new_in;
		ChanMapping new_out;
		for (DataType::iterator t = DataType::begin (); t != DataType::end (); ++t) {
			for (uint32_t i = 0; i < natural_input_streams.get (*t); ++i) {
				bool     valid;
				uint32_t idx = _in_map[pc].get (*t, i, &valid);
				if (valid && idx < _configured_in.get (*t)) {
					new_in.set (*t, i, idx);
				}
			}
			for (uint32_t o = 0; o < natural_output_streams.get (*t); ++o) {
				bool     valid;
				uint32_t idx = _out_map[pc].get (*t, o, &valid);
				if (valid && idx < _configured_out.get (*t)) {
					new_out.set (*t, o, idx);
				}
			}
		}
		_in_map[pc]  = new_in;
		_out_map[pc] = new_out;
	}

	_no_inplace       = check_inplace ();
	_required_buffers = ChanCount::max (_configured_in, natural_input_streams + ChanCount::max (_configured_out, natural_output_streams * get_count ()));

#ifndef NDEBUG
	if (DEBUG_ENABLED(DEBUG::RegionFx)) {
		pc = 0;
		DEBUG_STR_DECL(a);
		DEBUG_STR_APPEND(a, "\n--------<<--------\n");
		DEBUG_STR_APPEND(a, string_compose ("RFX IO Config for %1 in: %2 out: %3 req: %4\n", name(), _configured_in, _configured_out, _required_buffers));
		DEBUG_STR_APPEND(a, "Match: " << _match << " no inplace: " << _no_inplace << "\n");
		for (Plugins::iterator i = _plugins.begin(); i != _plugins.end(); ++i, ++pc) {
			if (pc > 0) {
				DEBUG_STR_APPEND(a, "----><----\n");
			}
			DEBUG_STR_APPEND(a, string_compose ("Channel Map for %1 plugin %2\n", name(), pc));
			DEBUG_STR_APPEND(a, " * Inputs:\n");
			DEBUG_STR_APPEND(a, _in_map[pc]);
			DEBUG_STR_APPEND(a, " * Outputs:\n");
			DEBUG_STR_APPEND(a, _out_map[pc]);
		}
		DEBUG_STR_APPEND(a, "-------->>--------\n");
	}
#endif

	return true;
}

bool
RegionFxPlugin::check_inplace ()
{
	bool inplace_ok = !_plugins.front ()->inplace_broken ();

	if (_match.method == Hide || _match.method == Split) {
		inplace_ok = false;
	}

	if (_match.method == Replicate) {
		for (uint32_t pc = 0; pc < get_count () && inplace_ok; ++pc) {
			if (_in_map[pc] != _out_map[pc]) {
				inplace_ok = false;
				break;
			}
		}

		ChanCount natural_input_streams  = _plugins[0]->get_info ()->n_inputs;
		ChanCount natural_output_streams = _plugins[0]->get_info ()->n_outputs;

		if (natural_input_streams * get_count () != _configured_in) {
			inplace_ok = false;
		}
		if (natural_output_streams * get_count () != _configured_out) {
			inplace_ok = false;
		}

		ARDOUR::ChanMapping in_map;
		ARDOUR::ChanMapping out_map;

		uint32_t pc = 0;
		for (auto const& mi : _in_map) {
			ChanMapping m (mi.second);
			const ChanMapping::Mappings& mp (mi.second.mappings());
			for (auto const& tm: mp) {
				for (auto const& i: tm.second) {
					in_map.set (tm.first, i.first + pc * natural_input_streams.get (tm.first), i.second);
				}
			}
			++pc;
		}

		pc = 0;
		for (auto const& mi : _out_map) {
			ChanMapping m (mi.second);
			const ChanMapping::Mappings& mp (mi.second.mappings());
			for (auto const& tm: mp) {
				for (auto const& i: tm.second) {
					out_map.set (tm.first, i.first + pc * natural_output_streams.get (tm.first), i.second);
				}
			}
			++pc;
		}

		if (!in_map.is_monotonic ()) {
			inplace_ok = false;
		}
		if (!out_map.is_monotonic ()) {
			inplace_ok = false;
		}
		return !inplace_ok;
	}

	for (uint32_t pc = 0; pc < get_count () && inplace_ok; ++pc) {
		if (!_in_map[pc].is_monotonic ()) {
			inplace_ok = false;
		}
		if (!_out_map[pc].is_monotonic ()) {
			inplace_ok = false;
		}
	}
	return !inplace_ok;
}

int
RegionFxPlugin::set_block_size (pframes_t nframes)
{
	int ret = 0;
	for (auto const& i : _plugins) {
		if (i->set_block_size (nframes) != 0) {
			ret = -1;
		}
	}
	return ret;
}

std::shared_ptr<Evoral::Control>
RegionFxPlugin::control_factory(const Evoral::Parameter& param)
{
	Evoral::Control*                  control   = NULL;
	ParameterDescriptor               desc(param);
	std::shared_ptr<AutomationList> list;

#if 0
	if (param.type() == PluginAutomation) {
		_plugin->get_parameter_descriptor(param.id(), desc);
		control = new IOPlug::PluginControl (pi, param, desc);
	} else if (param.type() == PluginPropertyAutomation) {
		desc = _plugin->get_property_descriptor (param.id());
		if (desc.datatype != Variant::NOTHING) {
			control = new IOPlug::PluginPropertyControl(pi, param, desc, list);
		}
	}
#endif

	if (!control) {
		std::shared_ptr<AutomationList> list;
		control = new AutomationControl (_session, param, desc, list);
	}

	return std::shared_ptr<Evoral::Control>(control);
}

void
RegionFxPlugin::automatables (ControllableSet& s) const
{
	for (auto const& i : _controls) {
		std::shared_ptr<AutomationControl> ac = std::dynamic_pointer_cast<AutomationControl> (i.second);
		if (ac) {
			s.insert (ac);
		}
	}
}

void
RegionFxPlugin::automation_run (samplepos_t start, pframes_t nframes)
{
	for (auto const& i : controls ()) {
		std::shared_ptr<AutomationControl> c = std::dynamic_pointer_cast<AutomationControl> (i.second);
		if (!c) {
			continue;
		}
		c->automation_run (start, nframes);
	}
}

bool
RegionFxPlugin::find_next_event (timepos_t const& start, timepos_t const& end, Evoral::ControlEvent& next_event) const
{
	next_event.when = start <= end ? timepos_t::max (start.time_domain()) : timepos_t (start.time_domain());

	for (auto const& i : controls ()) {
		std::shared_ptr<AutomationControl> c = std::dynamic_pointer_cast<AutomationControl> (i.second);
		if (c) {
			Automatable::find_next_ac_event (c, start, end, next_event);
		}
	}
	return next_event.when != (start <= end ? timepos_t::max (next_event.when.time_domain ()) : timepos_t (next_event.when.time_domain ()));
}

bool
RegionFxPlugin::run (BufferSet& bufs, samplepos_t start, samplepos_t end, samplepos_t pos, pframes_t nframes, sampleoffset_t off)
{
	int canderef (1);
	if (_flush.compare_exchange_strong (canderef,  0)) {
		for (auto const& i : _plugins) {
			i->flush ();
		}
	}

	const bool no_split_cycle = _plugins.front ()->requires_fixed_sized_buffers () /* || _plugins.front ()->get_info ()->type == ARDOUR::VST3 */;

	Evoral::ControlEvent next_event (timepos_t (Temporal::AudioTime), 0.0f);
	samplecnt_t          offset = 0;

	Glib::Threads::Mutex::Lock lm (control_lock ());

	if (no_split_cycle || !find_next_event (timepos_t (start), timepos_t (end), next_event)) {
		/* no events have a time within the relevant range */
		return connect_and_run (bufs, start, end, pos, nframes, off, offset);
	}

	while (nframes) {
		samplecnt_t cnt = min (timepos_t (start).distance (next_event.when).samples (), (samplecnt_t)nframes);

		/* An event returned by find_next_event is always be *after* `start`. */
		assert (timepos_t (start) < next_event.when);
		/* However it may still be at the sample sample (when event is using BeatTime),
		 * in which case we need to look for the next event, after that.
		 */
		int timeout = 8; // just in case there is more than one music-time event for the given sample.
		while (cnt == 0 && --timeout > 0 && Temporal::AudioTime != next_event.when.time_domain ()) {
			timepos_t _start = next_event.when; // copy, since find_next_event uses a reference, and modifies next_event
			if (!find_next_event (_start, timepos_t (end), next_event)) {
				cnt = nframes;
				break;
			} else {
				cnt = min (timepos_t (start).distance (next_event.when).samples (), (samplecnt_t)nframes);
			}
		}

		if (cnt <= 0) {
			/* prevent endless loops, just skip over events until next cycle.
			 * (alternatively we could single step and set  cnt = 1;)
			 */
			break;
		}

		if (!connect_and_run (bufs, start, start + cnt, pos, cnt, off, offset)) {
			return false;
		}

		nframes -= cnt;
		offset += cnt;
		start += cnt;

		timepos_t _start = next_event.when;
		if (!find_next_event (_start, timepos_t (end), next_event)) {
			break;
		}
	}

	if (nframes) {
		return connect_and_run (bufs, start, start + nframes, pos, nframes, off, offset);
	}
	return true;
}

bool
RegionFxPlugin::connect_and_run (BufferSet& bufs, samplepos_t start, samplepos_t end, samplepos_t pos, pframes_t nframes, samplecnt_t buf_off, samplecnt_t cycle_off)
{
	const bool no_inplace = _no_inplace;
	Temporal::TempoMap::update_thread_tempo_map ();

	//bufs.set_count(ChanCount::max(bufs.count(), _configured_internal)); // ADD SC
	bufs.set_count (ChanCount::max (bufs.count (), _configured_out));

	automation_run (start, nframes);
	// TODO set VST3 event-list, then unset no_split_cycle

	ChanCount natural_input_streams  = _plugins[0]->get_info ()->n_inputs;
	ChanCount natural_output_streams = _plugins[0]->get_info ()->n_outputs;

	std::map<uint32_t, ARDOUR::ChanMapping>         in_map (_in_map);
	std::map<uint32_t, ARDOUR::ChanMapping> const& out_map (_out_map);

	if (no_inplace) {
		uint32_t            pc           = 0;
		BufferSet&          inplace_bufs = _session.get_noinplace_buffers ();
		ARDOUR::ChanMapping used_outputs;

		assert (inplace_bufs.count () >= natural_input_streams + _configured_out);

		/* build used-output map */
		for (Plugins::iterator i = _plugins.begin (); i != _plugins.end (); ++i, ++pc) {
			for (DataType::iterator t = DataType::begin (); t != DataType::end (); ++t) {
				for (uint32_t out = 0; out < natural_output_streams.get (*t); ++out) {
					bool     valid;
					uint32_t out_idx = out_map.at (pc).get (*t, out, &valid);
					if (valid) {
						used_outputs.set (*t, out_idx, 1); // mark as used
					}
				}
			}
		}
		/* silence outputs */
		for (DataType::iterator t = DataType::begin (); t != DataType::end (); ++t) {
			for (uint32_t out = 0; out < bufs.count ().get (*t); ++out) {
				bool     valid;
				uint32_t m = out + natural_input_streams.get (*t);
				used_outputs.get (*t, out, &valid);
				if (valid) {
					/* the plugin is expected to write here, but may not :(
					 * (e.g. drumgizmo w/o kit loaded)
					 */
					inplace_bufs.get_available (*t, m).silence (nframes);
				}
			}
		}

		pc = 0;
		for (Plugins::iterator i = _plugins.begin (); i != _plugins.end (); ++i, ++pc) {
			ARDOUR::ChanMapping i_in_map (natural_input_streams);
			ARDOUR::ChanMapping i_out_map (out_map.at (pc));
			ARDOUR::ChanCount   mapped;

			/* map inputs sequentially */
			for (DataType::iterator t = DataType::begin (); t != DataType::end (); ++t) {
				for (uint32_t in = 0; in < natural_input_streams.get (*t); ++in) {
					bool     valid;
					uint32_t in_idx = in_map.at (pc).get (*t, in, &valid);
					uint32_t m      = mapped.get (*t);
					if (valid) {
						inplace_bufs.get_available (*t, m).read_from (bufs.get_available (*t, in_idx), nframes, cycle_off, cycle_off + buf_off);
					} else {
						inplace_bufs.get_available (*t, m).silence (nframes, cycle_off);
						i_in_map.unset (*t, in);
					}
					mapped.set (*t, m + 1);
				}
			}

			/* outputs are mapped to inplace_bufs after the inputs */
			for (DataType::iterator t = DataType::begin (); t != DataType::end (); ++t) {
				i_out_map.offset_to (*t, natural_input_streams.get (*t));
			}

			if ((*i)->connect_and_run (inplace_bufs, pos + start, pos + end, 1.0, i_in_map, i_out_map, nframes, cycle_off)) {
				return false;
			}
		}
		/* all instances have completed, now copy data that was written
		 * and zero unconnected buffers */
		ARDOUR::ChanMapping nonzero_out (used_outputs);
		for (DataType::iterator t = DataType::begin (); t != DataType::end (); ++t) {
			for (uint32_t out = 0; out < bufs.count ().get (*t); ++out) {
				bool valid;
				used_outputs.get (*t, out, &valid);
				if (!valid) {
					bufs.get_available (*t, out).silence (nframes, cycle_off + buf_off);
				} else {
					uint32_t m = out + natural_input_streams.get (*t);
					bufs.get_available (*t, out).read_from (inplace_bufs.get_available (*t, m), nframes, cycle_off + buf_off, cycle_off);
				}
			}
		}
	} else {
		/* in-place processing */
		uint32_t pc = 0;
		for (Plugins::iterator i = _plugins.begin (); i != _plugins.end (); ++i, ++pc) {
			if ((*i)->connect_and_run (bufs, pos + start, pos + end, 1.0, in_map.at (pc), out_map.at (pc), nframes, cycle_off + buf_off)) {
				return false;
			}
		}
	}

	const samplecnt_t l = effective_latency ();
	if (_plugin_signal_latency != l) {
		_plugin_signal_latency= l;
		LatencyChanged (); /* EMIT SIGNAL */
	}
	return true;
}
