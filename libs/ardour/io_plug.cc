/*
 * Copyright (C) 2022 Robin Gareus <robin@gareus.org>
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
#include <cassert>

#include "pbd/types_convert.h"
#include "pbd/xml++.h"

#include "temporal/tempo.h"

#include "ardour/audioengine.h"
#include "ardour/audio_buffer.h"
#include "ardour/audio_port.h"
#include "ardour/event_type_map.h"
#include "ardour/graph.h"
#include "ardour/io.h"
#include "ardour/io_plug.h"
#include "ardour/lv2_plugin.h"
#include "ardour/readonly_control.h"
#include "ardour/session.h"
#include "ardour/utils.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;

IOPlug::IOPlug (Session& s, boost::shared_ptr<Plugin> p, bool pre)
	: SessionObject (s, "")
	, GraphNode (s._process_graph)
	, _plugin (p)
	, _pre (pre)
	, _plugin_signal_latency (0)
	, _window_proxy (0)
{
	g_atomic_int_set (&_stat_reset, 0);
	g_atomic_int_set (&_reset_meters, 0);

	if (_plugin) {
		setup ();
		set_name (p->get_info()->name);
	}
	_input.reset (new IO (_session, io_name (), IO::Input));
	_output.reset (new IO (_session, io_name (), IO::Output));
}

IOPlug::~IOPlug ()
{
	for (CtrlOutMap::const_iterator i = _control_outputs.begin(); i != _control_outputs.end(); ++i) {
		boost::dynamic_pointer_cast<ReadOnlyControl>(i->second)->drop_references ();
	}
}

std::string
IOPlug::io_name (std::string const& n) const
{
	return (string_compose ("%1/%2/%3", _("IO"), _pre ? _("Pre"): _("Post"), n.empty () ? name () : n));
}

std::string
IOPlug::ensure_io_name (std::string newname) const
{
	while (!_session.io_name_is_legal (io_name (newname))) {
		newname = bump_name_once (newname, ' ');
		if (newname == name()) {
			break;
		}
	}
	return newname;
}

XMLNode&
IOPlug::get_state() const
{
	XMLNode* node = new XMLNode (/*state_node_name*/ "IOPlug");

	Latent::add_state (node);

	node->set_property("type", _plugin->state_node_name ());
	node->set_property("unique-id", _plugin->unique_id ());

	node->set_property("id", id());
	node->set_property("name", name());
	node->set_property("pre", _pre);

	_plugin->set_insert_id(this->id());
	node->add_child_nocopy (_plugin->get_state());

	for (auto const& c : controls()) {
		boost::shared_ptr<AutomationControl> ac = boost::dynamic_pointer_cast<AutomationControl> (c.second);
		if (ac) {
			node->add_child_nocopy (ac->get_state());
		}
	}

	if (_input) {
		XMLNode& i (_input->get_state ());
		node->add_child_nocopy (i);
	}
	if (_output) {
		XMLNode& o (_output->get_state ());
		node->add_child_nocopy (o);
	}
	return *node;
}

int
IOPlug::set_state (const XMLNode& node, int version)
{
	set_id (node);
	assert (!regenerate_xml_or_string_ids ());

	ARDOUR::PluginType type;
	std::string unique_id;
	if (! parse_plugin_type (node, type, unique_id)) {
		return -1;
	}

	bool any_vst;
	_plugin = find_and_load_plugin (_session, node, type, unique_id, any_vst);

	if (!_plugin) {
		return -1;
	}

	node.get_property ("pre", _pre);

	string name;
	if (node.get_property ("name", name)) {
		set_name (name);
	} else {
		set_name (_plugin->get_info()->name);
	}

	setup ();
	set_control_ids (node, version);

	_plugin->set_insert_id (this->id());

	XMLNodeList nlist = node.children();
	XMLNodeIterator niter;

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		if ((*niter)->name() == _plugin->state_node_name ()) {
			_plugin->set_state (**niter, version);
			break;
		}
	}

	if (_input) {
		std::string str;
		const string instr = enum_2_string (IO::Input);
		for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
			if ((*niter)->get_property ("direction", str) && str == instr) {
				_input->set_state(**niter, version);
				break;
			}
		}
	}
	if (_output) {
		std::string str;
		const string outstr = enum_2_string (IO::Output);
		for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
			if ((*niter)->get_property ("direction", str) && str == outstr) {
				_output->set_state(**niter, version);
			}
		}
	}

	Latent::set_state (node, version);

	return 0;
}

bool
IOPlug::set_name (std::string const& str)
{
	bool ret = true;

	if (name () == str) {
		return ret;
	}

	std::string new_name = ensure_io_name (str);

	if (ret && _input) {
		ret = _input->set_name (io_name (new_name));
	}

	if (ret && _output) {
		ret = _output->set_name (io_name (new_name));
	}

	if (ret) {
		ret = SessionObject::set_name (new_name); /* never fails */
		assert (ret);
	}

	return ret;
}

void
IOPlug::setup ()
{
	 create_parameters ();

	 PluginInfoPtr pip = _plugin->get_info ();
	 ChanCount aux_in;
	 if (pip->reconfigurable_io()) {
		 _n_in  = _plugin->input_streams ();
		 _n_out = _plugin->output_streams ();
		 if (_n_in.n_total () == 0 && _n_out.n_total () == 0) {
			 if (pip->is_instrument ()) {
				 _n_in.set_midi (1);
			 } else {
				 _n_in.set_audio (2);
			 }
			 _n_out.set_audio (2);
		 }
		 _plugin->match_variable_io (_n_in, aux_in, _n_out);
	 } else {
		 _n_in  = pip->n_inputs;
		 _n_out = pip->n_outputs;
	 }

	 _plugin->reconfigure_io (_n_in, aux_in, _n_out);
	 _plugin->ParameterChangedExternally.connect_same_thread (*this, boost::bind (&IOPlug::parameter_changed_externally, this, _1, _2));
	 _plugin->activate ();
}

samplecnt_t
IOPlug::signal_latency () const
{
	return _plugin->signal_latency ();
}

void
IOPlug::set_public_latency (bool playback)
{
	/* Step1: set private port latency
	 * compare to Route::set_private_port_latencies, Route::update_port_latencies
	 */
	PortSet& from = playback ? _output->ports () : _input->ports ();
	PortSet& to   = playback ? _input->ports () : _output->ports ();

	LatencyRange all_connections;
	all_connections.min = ~((pframes_t) 0);
	all_connections.max = 0;

	for (auto const& p : from) {
		if (!p->connected ()) {
			continue;
		}

		LatencyRange range;
		p->get_connected_latency_range (range, playback);

		all_connections.min = min (all_connections.min, range.min);
		all_connections.max = max (all_connections.max, range.max);
	}
	if (all_connections.min == ~((pframes_t) 0)) {
		all_connections.min = 0;
	}

	/* set the "from" port latencies to the max/min range of all their connections */
	for (auto const& p : from) {
		p->set_private_latency_range (all_connections, playback);
	}

	all_connections.min += _plugin_signal_latency;
	all_connections.max += _plugin_signal_latency;

	for (auto const& p : to) {
		p->set_private_latency_range (all_connections, playback);
	}

	/* Step2: publish private latencies.
	 * compare to Route::set_public_port_latencies
	 */

	if (playback) {
		_output->set_public_port_latency_from_connections ();
		_input->set_public_port_latencies (all_connections.max, true);
	} else {
		_input->set_public_port_latency_from_connections ();
		_output->set_public_port_latencies (all_connections.max, false);
	}
}

void
IOPlug::create_parameters ()
{
	assert (_plugin);

	for (uint32_t i = 0; i < _plugin->parameter_count(); ++i) {
		if (!_plugin->parameter_is_control (i)) {
			continue;
		}

		ParameterDescriptor desc;
		_plugin->get_parameter_descriptor (i, desc);

		if (!_plugin->parameter_is_input (i)) {
			_control_outputs[i] = boost::shared_ptr<ReadOnlyControl> (new ReadOnlyControl (_plugin, desc, i));
			continue;
		}

		Evoral::Parameter param (PluginAutomation, 0, i);

		boost::shared_ptr<AutomationControl> c (new PluginControl(this, param, desc));
		c->set_flag (Controllable::NotAutomatable);
		add_control (c);

		_plugin->set_automation_control (i, c);
	}

	Plugin::PropertyDescriptors const& pdl (_plugin->get_supported_properties ());

	for (Plugin::PropertyDescriptors::const_iterator p = pdl.begin(); p != pdl.end(); ++p) {
		Evoral::Parameter param (PluginPropertyAutomation, 0, p->first);
		ParameterDescriptor const& desc = _plugin->get_property_descriptor (param.id());
		if (desc.datatype == Variant::NOTHING) {
			continue;
		}
		boost::shared_ptr<AutomationControl> c (new PluginPropertyControl (this, param, desc));
		c->set_flag (Controllable::NotAutomatable);
		add_control (c);
	}

	_plugin->PresetPortSetValue.connect_same_thread (*this, boost::bind (&IOPlug::preset_load_set_value, this, _1, _2));
}

void
IOPlug::parameter_changed_externally (uint32_t which, float val)
{
	boost::shared_ptr<Evoral::Control> c = control (Evoral::Parameter (PluginAutomation, 0, which));
	boost::shared_ptr<PluginControl> pc = boost::dynamic_pointer_cast<PluginControl> (c);
	if (pc) {
		pc->catch_up_with_external_value (val);
	}
}

int
IOPlug::set_block_size (pframes_t n_samples)
{
	return _plugin->set_block_size (n_samples);
}

PlugInsertBase::UIElements
IOPlug::ui_elements () const
{
	UIElements rv = PluginPreset;
	if (_plugin->get_info ()->is_instrument ()) {
		rv = static_cast<PlugInsertBase::UIElements> (static_cast <std::uint8_t>(rv) | static_cast<std::uint8_t> (PlugInsertBase::MIDIKeyboard));
	}
	return rv;
}

bool
IOPlug::ensure_io ()
{
	/* must be called with process-lock held */
	if (_input->ensure_io (_n_in, false, this) != 0) {
		return false;
	}
	if (_output->ensure_io (_n_out, false, this) != 0) {
		return false;
	}

	_bufs.ensure_buffers (ChanCount::max (_n_in, _n_out), _session.get_block_size ());

	for (uint32_t i = 0; i < _n_in.n_audio (); ++i) {
		const auto& pd (_plugin->describe_io_port (DataType::AUDIO, true, i));
		std::string const pn = string_compose ("%1 %2 - %3", _("IO"), name (), pd.name);
		_input->audio (i)->set_pretty_name (pn);
	}
	for (uint32_t i = 0; i < _n_in.n_midi (); ++i) {
		const auto& pd (_plugin->describe_io_port (DataType::MIDI, true, i));
		std::string const pn = string_compose ("%1 %2 - %3", _("IO"), name (), pd.name);
		_input->midi (i)->set_pretty_name (pn);
	}
	for (uint32_t i = 0; i < _n_out.n_audio (); ++i) {
		const auto& pd (_plugin->describe_io_port (DataType::AUDIO, false, i));
		std::string const pn = string_compose ("%1 %2 - %3", _("IO"), name (), pd.name);
		_output->audio (i)->set_pretty_name (pn);
	}
	for (uint32_t i = 0; i < _n_out.n_midi (); ++i) {
		const auto& pd (_plugin->describe_io_port (DataType::MIDI, false, i));
		std::string const pn = string_compose ("%1 %2 - %3", _("IO"), name (), pd.name);
		_output->midi (i)->set_pretty_name (pn);
	}

	 if (_pre) {
		 for (uint32_t i = 0; i < _n_out.n_audio (); ++i) {
			 std::string const& n = AudioEngine::instance ()->make_port_name_non_relative (_output->audio (i)->name ());
			 _audio_input_ports.insert (make_pair (n, PortManager::AudioInputPort (24288))); // 2^19 ~ 1MB / port
		 }
		 for (uint32_t i = 0; i < _n_out.n_midi (); ++i) {
			 std::string const& n = AudioEngine::instance ()->make_port_name_non_relative (_output->midi (i)->name ());
			 _midi_input_ports.insert (make_pair (n, PortManager::MIDIInputPort (32)));
		 }
	 }
	return true;
}

void
IOPlug::process ()
{
	_graph->process_one_ioplug (this);
}

void
IOPlug::run (samplepos_t start, pframes_t n_samples)
{
	Temporal::TempoMap::update_thread_tempo_map ();
	assert (n_samples > 0);

	if (g_atomic_int_compare_and_exchange (&_stat_reset, 1, 0)) {
		_timing_stats.reset ();
	}

	if (!_plugin) {
		_output->silence (n_samples);
		return;
	}

	_timing_stats.start ();

	ARDOUR::ChanMapping in_map (_n_in);
	ARDOUR::ChanMapping out_map (_n_out);

	double speed = 0;
	samplepos_t end = start + n_samples * speed;

	_input->collect_input (_bufs, n_samples, ChanCount::ZERO); /* calls _bufs.set_count() */

	if (_plugin->connect_and_run (_bufs, start, end, speed, in_map, out_map, n_samples, 0)) {
		// TODO report error, deactivate plugin
		_output->silence (n_samples);
		_timing_stats.update ();
		return;
	}

	_bufs.set_count (_n_out);

	for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
		if (_bufs.count().get(*t) > 0) {
			_output->copy_to_outputs (_bufs, *t, n_samples, 0);
		}
	}

	PortSet& ports (_output->ports());
	if (_pre) {
		const bool reset       = g_atomic_int_compare_and_exchange (&_reset_meters, 1, 0);
		samplecnt_t const rate = _session.nominal_sample_rate ();

		auto a = _audio_input_ports.begin ();
		auto m = _midi_input_ports.begin ();
		for (auto p = _bufs.audio_begin (); p != _bufs.audio_end (); ++p, ++a) {
			AudioBuffer const& ab (*p);
			PortManager::AudioInputPort& ai (a->second);
			ai.apply_falloff (n_samples, rate, reset);
			ai.process (ab.data (), n_samples, reset);
		}
		for (auto p = _bufs.midi_begin (); p != _bufs.midi_end (); ++p, ++m) {
			PortManager::MIDIInputPort& mi (m->second);
			MidiBuffer const& mb (*p);
			mi.apply_falloff (n_samples, rate, reset);
			for (MidiBuffer::const_iterator i = mb.begin (); i != mb.end (); ++i) {
				Evoral::Event<samplepos_t> ev (*i, false);
				mi.process_event (ev.buffer (), ev.size ());
			}
		}
	}
	for (PortSet::iterator i = ports.begin(); i != ports.end(); ++i) {
		i->flush_buffers (n_samples);
	}

	const samplecnt_t l = effective_latency ();
	if (_plugin_signal_latency != l) {
		_plugin_signal_latency = l;
		LatencyChanged (); /* EMIT SIGNAL */
	}

	_timing_stats.update ();
}

void
IOPlug::reset_input_meters ()
{
	g_atomic_int_set (&_reset_meters, 1);
}

bool
IOPlug::get_stats (PBD::microseconds_t& min, PBD::microseconds_t& max, double& avg, double& dev) const
{
	return _timing_stats.get_stats (min, max, avg, dev);
}

void
IOPlug::clear_stats ()
{
	g_atomic_int_set (&_stat_reset, 1);
}

boost::shared_ptr<ReadOnlyControl>
IOPlug::control_output (uint32_t num) const
{
	CtrlOutMap::const_iterator i = _control_outputs.find (num);
	if (i == _control_outputs.end ()) {
		return boost::shared_ptr<ReadOnlyControl> ();
	} else {
		return (*i).second;
	}
}

bool
IOPlug::load_preset (Plugin::PresetRecord pr)
{
	return _plugin->load_preset (pr);
}

bool
IOPlug::write_immediate_event (Evoral::EventType event_type, size_t size, const uint8_t* buf)
{
	return _plugin->write_immediate_event (event_type, size, buf);
}

boost::shared_ptr<Evoral::Control>
IOPlug::control_factory(const Evoral::Parameter& param)
{
	Evoral::Control*                  control   = NULL;
	ParameterDescriptor               desc(param);
	boost::shared_ptr<AutomationList> list;

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
		boost::shared_ptr<AutomationList> list;
		control = new AutomationControl (_session, param, desc, list);
	}

	return boost::shared_ptr<Evoral::Control>(control);
}

std::string
IOPlug::describe_parameter (Evoral::Parameter param)
{
	if (param.type() == PluginAutomation) {
		_plugin->describe_parameter (param);
	} else if (param.type() == PluginPropertyAutomation) {
		return string_compose ("Property %1", URIMap::instance ().id_to_uri (param.id()));
	}
	return EventTypeMap::instance ().to_symbol (param);
}

bool
IOPlug::direct_feeds_according_to_reality (boost::shared_ptr<GraphNode> node, bool* via_send_only)
{
	boost::shared_ptr<IOPlug> other (boost::dynamic_pointer_cast<IOPlug> (node));
	assert (other && other->_pre == _pre);
	if (via_send_only) {
		*via_send_only = false;
	}
	return other->input()->connected_to (_output);
}

/* ****************************************************************************/

IOPlug::PluginControl::PluginControl (IOPlug*                     p,
                                      Evoral::Parameter const&    param,
                                      ParameterDescriptor const&  desc)
	: AutomationControl (p->session (), param, desc, boost::shared_ptr<AutomationList> (), p->describe_parameter (param))
	, _iop (p)
{
}

void
IOPlug::PluginControl::actually_set_value (double user_val, PBD::Controllable::GroupControlDisposition group_override)
{
	_iop->plugin ()->set_parameter (parameter().id(), user_val, 0);

	AutomationControl::actually_set_value (user_val, group_override);
}

void
IOPlug::PluginControl::catch_up_with_external_value (double user_val)
{
	AutomationControl::actually_set_value (user_val, Controllable::NoGroup);
}

XMLNode&
IOPlug::PluginControl::get_state () const
{
	XMLNode& node (AutomationControl::get_state());
	node.set_property ("parameter", parameter().id());

	boost::shared_ptr<LV2Plugin> lv2plugin = boost::dynamic_pointer_cast<LV2Plugin> (_iop->plugin ());
	if (lv2plugin) {
		node.set_property ("symbol", lv2plugin->port_symbol (parameter().id()));
	}

	return node;
}

double
IOPlug::PluginControl::get_value () const
{
	boost::shared_ptr<Plugin> plugin = _iop->plugin ();

	if (!plugin) {
		return 0.0;
	}

	return plugin->get_parameter (parameter().id());
}

std::string
IOPlug::PluginControl::get_user_string () const
{
	boost::shared_ptr<Plugin> plugin = _iop->plugin (0);
	if (plugin) {
		std::string pp;
		if (plugin->print_parameter (parameter().id(), pp) && pp.size () > 0) {
			return pp;
		}
	}
	return AutomationControl::get_user_string ();
}

IOPlug::PluginPropertyControl::PluginPropertyControl (IOPlug*                    p,
                                                      Evoral::Parameter const&   param,
                                                      ParameterDescriptor const& desc)
	: AutomationControl (p->session(), param, desc )
	, _iop (p)
{
}

void
IOPlug::PluginPropertyControl::actually_set_value (double user_val, Controllable::GroupControlDisposition gcd)
{
	const Variant value(_desc.datatype, user_val);
	if (value.type() == Variant::NOTHING) {
		return;
	}

	_iop->plugin ()->set_property (parameter().id(), value);

	_value = value;

	AutomationControl::actually_set_value (user_val, gcd);
}

XMLNode&
IOPlug::PluginPropertyControl::get_state () const
{
	XMLNode& node (AutomationControl::get_state());
	node.set_property ("property", parameter ().id ());
	node.remove_property ("value");
	return node;
}

double
IOPlug::PluginPropertyControl::get_value () const
{
	return _value.to_double();
}
