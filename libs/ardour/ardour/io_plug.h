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

#ifndef _libardour_io_plug_h_
#define _libardour_io_plug_h_

#include <boost/shared_ptr.hpp>

#include "pbd/timing.h"
#include "pbd/g_atomic_compat.h"

#include "ardour/ardour.h"
#include "ardour/automation_control.h"
#include "ardour/buffer_set.h"
#include "ardour/latent.h"
#include "ardour/graphnode.h"
#include "ardour/plugin.h"
#include "ardour/port_manager.h"
#include "ardour/session_object.h"
#include "ardour/plug_insert_base.h"

namespace Gtkmm2ext {
class WindowProxy;
}

namespace ARDOUR {

class IO;
class ReadOnlyControl;

class LIBARDOUR_API IOPlug : public SessionObject, public PlugInsertBase, public Latent, public GraphNode
{
public:
	IOPlug (Session&, boost::shared_ptr<Plugin> = boost::shared_ptr<Plugin>(), bool pre = true);
	virtual ~IOPlug ();

	bool set_name (std::string const&);

	std::string io_name (std::string const& name = "") const;

	XMLNode& get_state (void) const;
	int set_state (const XMLNode&, int version);

	void run (samplepos_t, pframes_t);
	int  set_block_size (pframes_t);
	void set_public_latency (bool);
	bool ensure_io ();

	bool is_pre () const { return _pre; }

	boost::shared_ptr<IO> input () const { return _input; }
	boost::shared_ptr<IO> output () const { return _output; }

	Gtkmm2ext::WindowProxy* window_proxy () const { return _window_proxy; }
	void set_window_proxy (Gtkmm2ext::WindowProxy* wp) { _window_proxy = wp; }

	PortManager::AudioInputPorts audio_input_ports () const { return _audio_input_ports; }
	PortManager::MIDIInputPorts  midi_input_ports () const { return _midi_input_ports; }

	void reset_input_meters ();

	/* Latent */
	samplecnt_t signal_latency () const;

	/* PlugInsertBase */
	uint32_t get_count () const { return 1; }
	boost::shared_ptr<Plugin> plugin (uint32_t num = 0) const { return _plugin; }
	PluginType type () const { return _plugin->get_info()->type; }

	UIElements ui_elements () const;

	bool write_immediate_event (Evoral::EventType event_type, size_t size, const uint8_t* buf);
	bool load_preset (Plugin::PresetRecord);

	boost::shared_ptr<ReadOnlyControl> control_output (uint32_t) const;

	bool reset_parameters_to_default () { return false;}
	bool can_reset_all_parameters () { return false; }

	virtual bool provides_stats () const { return true; }
	virtual bool get_stats (PBD::microseconds_t&, PBD::microseconds_t&, double&, double&) const;
	virtual void clear_stats ();

	/* ControlSet */
	boost::shared_ptr<Evoral::Control> control_factory (const Evoral::Parameter& id);

	/* GraphNode */
	std::string graph_node_name () const {
		return name ();
	}
	bool direct_feeds_according_to_reality (boost::shared_ptr<GraphNode>, bool* via_send_only = 0);
	void process ();

protected:
	std::string describe_parameter (Evoral::Parameter);

	/** A control that manipulates a plugin parameter (control port). */
	struct PluginControl : public AutomationControl
	{
		PluginControl (IOPlug*                    p,
		               Evoral::Parameter const&   param,
		               ParameterDescriptor const& desc);

		double get_value () const;
		void catch_up_with_external_value (double val);
		XMLNode& get_state() const;
		std::string get_user_string() const;
	private:
		void actually_set_value (double val, PBD::Controllable::GroupControlDisposition group_override);
		IOPlug* _iop;
	};

	/** A control that manipulates a plugin property (message). */
	struct PluginPropertyControl : public AutomationControl
	{
		PluginPropertyControl (IOPlug*                    p,
		                       Evoral::Parameter const&   param,
		                       ParameterDescriptor const& desc);

		double get_value () const;
		XMLNode& get_state() const;
	private:
		void actually_set_value (double value, PBD::Controllable::GroupControlDisposition);
		IOPlug* _iop;
		Variant _value;
	};

private:
	/* disallow copy construction */
	IOPlug (IOPlug const&);

	std::string ensure_io_name (std::string) const;
	void create_parameters ();
	void parameter_changed_externally (uint32_t, float);

	void setup ();

	ChanCount _n_in;
	ChanCount _n_out;
	PluginPtr _plugin;
	bool      _pre;
	uint32_t  _plugin_signal_latency;

	typedef std::map<uint32_t, boost::shared_ptr<ReadOnlyControl> >CtrlOutMap;
	CtrlOutMap _control_outputs;

	BufferSet             _bufs;
	boost::shared_ptr<IO> _input;
	boost::shared_ptr<IO> _output;

	PortManager::AudioInputPorts _audio_input_ports;
	PortManager::MIDIInputPorts  _midi_input_ports;

	Gtkmm2ext::WindowProxy* _window_proxy;

	PBD::TimingStats  _timing_stats;
	GATOMIC_QUAL gint _stat_reset;
	GATOMIC_QUAL gint _reset_meters;
};

}
#endif
