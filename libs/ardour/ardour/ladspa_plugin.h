/*
 * Copyright (C) 2006-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2007 Sampo Savolainen <v2@iki.fi>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_ladspa_plugin_h__
#define __ardour_ladspa_plugin_h__

#include <set>
#include <vector>
#include <string>

#include <glibmm/module.h>

#include "pbd/stateful.h"

#include "ardour/ladspa.h"
#include "ardour/plugin.h"

namespace ARDOUR {
class AudioEngine;
class Session;

class LIBARDOUR_API LadspaPlugin : public ARDOUR::Plugin
{
  public:
	LadspaPlugin (std::string module_path, ARDOUR::AudioEngine&, ARDOUR::Session&, uint32_t index, samplecnt_t sample_rate);
	LadspaPlugin (const LadspaPlugin &);
	~LadspaPlugin ();

	/* Plugin interface */

	std::string unique_id() const;
	const char* label() const           { return _descriptor->Label; }
	const char* name() const            { return _descriptor->Name; }
	const char* maker() const           { return _descriptor->Maker; }
	uint32_t    parameter_count() const { return _descriptor->PortCount; }
	float       default_value (uint32_t port) { return _default_value (port); }
	void        set_parameter (uint32_t port, float val, sampleoffset_t);
	float       get_parameter (uint32_t port) const;
	int         get_parameter_descriptor (uint32_t which, ParameterDescriptor&) const;
	uint32_t    nth_parameter (uint32_t port, bool& ok) const;

	std::set<Evoral::Parameter> automatable() const;

	void activate () {
		if (!_was_activated && _descriptor->activate)
			_descriptor->activate (_handle);

		_was_activated = true;
	}

	void deactivate () {
		if (_was_activated && _descriptor->deactivate)
			_descriptor->deactivate (_handle);

		_was_activated = false;
	}

	void cleanup () {
		activate();
		deactivate();

		if (_descriptor->cleanup)
			_descriptor->cleanup (_handle);
	}

	int set_block_size (pframes_t /*nframes*/) { return 0; }

	int connect_and_run (BufferSet& bufs,
			samplepos_t start, samplepos_t end, double speed,
			ChanMapping const& in, ChanMapping const& out,
			pframes_t nframes, samplecnt_t offset);

	std::string describe_parameter (Evoral::Parameter);
	std::string state_node_name() const { return "ladspa"; }

	bool parameter_is_audio(uint32_t) const;
	bool parameter_is_control(uint32_t) const;
	bool parameter_is_input(uint32_t) const;
	bool parameter_is_output(uint32_t) const;
	bool parameter_is_toggled(uint32_t) const;

	boost::shared_ptr<ScalePoints>
	get_scale_points(uint32_t port_index) const;

	int set_state (const XMLNode&, int version);

	bool load_preset (PresetRecord);

	bool has_editor() const { return false; }

	/* LADSPA extras */

	LADSPA_Properties           properties() const                { return _descriptor->Properties; }
	uint32_t                    index() const                     { return _index; }
	const char *                copyright() const                 { return _descriptor->Copyright; }
	LADSPA_PortDescriptor       port_descriptor(uint32_t i) const;
	const LADSPA_PortRangeHint* port_range_hints() const          { return _descriptor->PortRangeHints; }
	const char * const *        port_names() const                { return _descriptor->PortNames; }

	void set_gain (float gain)                    { _descriptor->set_run_adding_gain (_handle, gain); }
	void run_adding (uint32_t nsamples)           { _descriptor->run_adding (_handle, nsamples); }
	void connect_port (uint32_t port, float *ptr) { _descriptor->connect_port (_handle, port, ptr); }

  private:
	float                    _default_value (uint32_t port) const;
	std::string              _module_path;
	Glib::Module*            _module;
	const LADSPA_Descriptor* _descriptor;
	LADSPA_Handle            _handle;
	samplecnt_t              _sample_rate;
	LADSPA_Data*             _control_data;
	LADSPA_Data*             _shadow_data;
	LADSPA_Data*             _latency_control_port;
	uint32_t                 _index;
	bool                     _was_activated;

	samplecnt_t plugin_latency() const;
	void find_presets ();

	void init (std::string module_path, uint32_t index, samplecnt_t rate);
	void run_in_place (pframes_t nsamples);
	void latency_compute_run ();
	int set_state_2X (const XMLNode&, int version);
	std::string do_save_preset (std::string name);
	void do_remove_preset (std::string name);
	std::string preset_envvar () const;
	std::string preset_source (std::string) const;
	bool write_preset_file (std::string);
	void add_state (XMLNode *) const;
};

class LIBARDOUR_API LadspaPluginInfo : public PluginInfo {
  public:
	LadspaPluginInfo ();
	~LadspaPluginInfo () { };

	bool is_instrument () const { return false; } /* ladspa's are never instruments */
#ifdef MIXBUS
	/* for mixbus, relegate ladspa's to the Utils folder. */
	bool is_effect () const { return false; }
	bool is_utility () const { return true; }
#endif

	PluginPtr load (Session& session);
	std::vector<Plugin::PresetRecord> get_presets (bool user_only) const;
};

typedef boost::shared_ptr<LadspaPluginInfo> LadspaPluginInfoPtr;

} // namespace ARDOUR

#endif /* __ardour_ladspa_plugin_h__ */
