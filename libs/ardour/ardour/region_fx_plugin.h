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

#ifndef __ardour_region_fx_plugin_h__
#define __ardour_region_fx_plugin_h__

#include <atomic>

#include "temporal/domain_provider.h"

#include "ardour/ardour.h"
#include "ardour/automation_control.h"
#include "ardour/chan_mapping.h"
#include "ardour/libardour_visibility.h"
#include "ardour/parameter_descriptor.h"
#include "ardour/plug_insert_base.h"
#include "ardour/plugin.h"
#include "ardour/types.h"

namespace Gtkmm2ext
{
class WindowProxy;
}

class XMLNode;

namespace ARDOUR
{
class ReadOnlyControl;

class LIBARDOUR_API RegionFxPlugin : public SessionObject, public PlugInsertBase, public Latent, public Temporal::TimeDomainProvider
{
public:
	RegionFxPlugin (Session&, Temporal::TimeDomain const, std::shared_ptr<Plugin> = std::shared_ptr<Plugin> ());
	~RegionFxPlugin ();

	/* UI Proxy */
	Gtkmm2ext::WindowProxy* window_proxy () const
	{
		return _window_proxy;
	}
	void set_window_proxy (Gtkmm2ext::WindowProxy* wp)
	{
		_window_proxy = wp;
	}

	/* Latent */
	samplecnt_t signal_latency () const;

	/* PlugInsertBase */
	uint32_t get_count () const
	{
		return _plugins.size ();
	}
	PluginType type () const
	{
		return plugin ()->get_info ()->type;
	}
	std::shared_ptr<Plugin> plugin (uint32_t num = 0) const
	{
		if (num < _plugins.size ()) {
			return _plugins[num];
		} else {
			return _plugins[0];
		}
	}

	UIElements ui_elements () const;
	std::shared_ptr<Evoral::Control> control_factory(const Evoral::Parameter& id);

	bool write_immediate_event (Evoral::EventType event_type, size_t size, const uint8_t* buf);
	bool load_preset (Plugin::PresetRecord);

	std::shared_ptr<ReadOnlyControl> control_output (uint32_t) const;

	bool reset_parameters_to_default ();
	bool can_reset_all_parameters ();

	void maybe_emit_changed_signals () const;

	std::string describe_parameter (Evoral::Parameter param);

	bool provides_stats () const
	{
		return false;
	}
	bool get_stats (PBD::microseconds_t&, PBD::microseconds_t&, double&, double&) const
	{
		return false;
	}
	void clear_stats () {}

	ChanMapping input_map (uint32_t num) const {
		if (num < _in_map.size()) {
			return _in_map.find (num)->second;
		} else {
			return ChanMapping ();
		}
	}

	ChanMapping output_map (uint32_t num) const {
		if (num < _out_map.size()) {
			return _out_map.find (num)->second;
		} else {
			return ChanMapping ();
		}
	}

	/* Stateful */
	XMLNode& get_state (void) const;
	int      set_state (const XMLNode&, int version);

	void drop_references ();
	void update_id (PBD::ID);

	/* Fx */
	bool run (BufferSet&, samplepos_t start, samplepos_t end, samplepos_t region_pos, pframes_t nframes, sampleoffset_t off);

	void flush ();
	int  set_block_size (pframes_t nframes);
	void automatables (PBD::ControllableSet&) const;
	void set_default_automation (timepos_t);

	void truncate_automation_start (timecnt_t);
	void truncate_automation_end (timepos_t);

	bool can_support_io_configuration (const ChanCount& in, ChanCount& out);
	bool configure_io (ChanCount in, ChanCount out);

	ChanCount input_streams () const
	{
		return _configured_in;
	}
	ChanCount output_streams () const
	{
		return _configured_out;
	}
	ChanCount required_buffers () const
	{
		return _required_buffers;
	}

	/* wrapped Plugin API */
	PBD::Signal0<void> TailChanged;
	samplecnt_t effective_tail () const;

private:
	/* disallow copy construction */
	RegionFxPlugin (RegionFxPlugin const&);

	void add_plugin (std::shared_ptr<Plugin>);
	void plugin_removed (std::weak_ptr<Plugin>);
	bool set_count (uint32_t num);
	bool check_inplace ();
	void create_parameters ();
	void parameter_changed_externally (uint32_t, float);
	void automation_run (samplepos_t start, pframes_t nframes);
	bool find_next_event (timepos_t const& start, timepos_t const& end, Evoral::ControlEvent& next_event) const;
	void start_touch (uint32_t param_id);
	void end_touch (uint32_t param_id);
	bool connect_and_run (BufferSet&, samplepos_t start, samplepos_t end, samplepos_t region_pos, pframes_t nframes, sampleoffset_t buf_off, sampleoffset_t cycle_off);

	Match private_can_support_io_configuration (ChanCount const&, ChanCount&) const;

	/** details of the match currently being used */
	Match _match;

	uint32_t _plugin_signal_latency;

	typedef std::vector<std::shared_ptr<Plugin>> Plugins;
	Plugins                                      _plugins;

	ChanCount _configured_in;
	ChanCount _configured_out;
	ChanCount _required_buffers;

	std::map <uint32_t, ARDOUR::ChanMapping> _in_map;
	std::map <uint32_t, ARDOUR::ChanMapping> _out_map;

	bool _configured;
	bool _no_inplace;

	mutable samplepos_t _last_emit;

	typedef std::map<uint32_t, std::shared_ptr<ReadOnlyControl>> CtrlOutMap;
	CtrlOutMap                                                   _control_outputs;

	Gtkmm2ext::WindowProxy* _window_proxy;
	std::atomic<int>        _flush;

	mutable Glib::Threads::Mutex _process_lock;
};

} // namespace ARDOUR

#endif
