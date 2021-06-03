/*
 * Copyright (C) 2006-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2007-2018 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __ardour_vst_plugin_h__
#define __ardour_vst_plugin_h__

#include <pbd/signals.h>
#include "ardour/plugin.h"

struct _AEffect;
typedef struct _AEffect AEffect;
struct _VSTHandle;
typedef struct _VSTHandle VSTHandle;
struct _VSTState;
typedef struct _VSTState VSTState;

#include "ardour/vestige/vestige.h"

namespace ARDOUR {

class PluginInsert;
struct VST2Info;

/** Parent class for VST plugins of both Windows and Linux varieties */
class LIBARDOUR_API VSTPlugin : public Plugin
{
public:
	friend class Session;
	VSTPlugin (AudioEngine &, Session &, VSTHandle *);
	VSTPlugin (const VSTPlugin& other);
	virtual ~VSTPlugin ();

	void activate ();
	void deactivate ();

	int set_block_size (pframes_t);
	bool requires_fixed_sized_buffers () const;
	bool inplace_broken() const { return true; }
	float default_value (uint32_t port);
	float get_parameter (uint32_t port) const;
	uint32_t nth_parameter (uint32_t port, bool& ok) const;
	void set_parameter (uint32_t port, float val, sampleoffset_t);
	void set_parameter_automated (uint32_t port, float val);
	bool load_preset (PresetRecord);
	int get_parameter_descriptor (uint32_t which, ParameterDescriptor&) const;
	std::string describe_parameter (Evoral::Parameter);
	std::set<Evoral::Parameter> automatable() const;

	PBD::Signal0<void> LoadPresetProgram;
	PBD::Signal0<void> VSTSizeWindow;

	bool parameter_is_audio (uint32_t) const { return false; }
	bool parameter_is_control (uint32_t) const { return true; }
	bool parameter_is_input (uint32_t) const { return true; }
	bool parameter_is_output (uint32_t) const { return false; }

	uint32_t designated_bypass_port ();

	int connect_and_run (BufferSet&,
			samplepos_t start, samplepos_t end, double speed,
			ChanMapping const& in, ChanMapping const& out,
			pframes_t nframes, samplecnt_t offset
			);

	std::string unique_id () const;
	const char * label () const;
	const char * name () const;
	const char * maker () const;
	int32_t version () const;
	uint32_t parameter_count () const;
	bool print_parameter (uint32_t, std::string&) const;

	bool has_editor () const;

	AEffect * plugin () const { return _plugin; }
	VSTState * state () const { return _state; }
	MidiBuffer * midi_buffer () const { return _midi_out_buf; }

	int set_state (XMLNode const &, int);

	int first_user_preset_index () const;

	void set_insert (PluginInsert* pi, uint32_t num) { _pi = pi; _num = num; }
	PluginInsert* plugin_insert () const { return _pi; }
	uint32_t plugin_number () const { return _num; }
	VstTimeInfo* timeinfo () { return &_timeInfo; }
	samplepos_t transport_sample () const { return _transport_sample; }
	float transport_speed () const { return _transport_speed; }


protected:
	void parameter_changed_externally (uint32_t which, float val);
	virtual void open_plugin ();
	void init_plugin ();
	gchar* get_chunk (bool) const;
	int set_chunk (gchar const *, bool);
	void add_state (XMLNode *) const;
	bool load_user_preset (PresetRecord);
	bool load_plugin_preset (PresetRecord);
	std::string do_save_preset (std::string name);
	void do_remove_preset (std::string name);
	XMLTree * presets_tree () const;
	std::string presets_file () const;
	samplecnt_t plugin_latency() const;
	void find_presets ();

	VSTHandle* _handle;
	VSTState*  _state;
	AEffect*   _plugin;
	PluginInsert* _pi;
	uint32_t      _num;

	MidiBuffer* _midi_out_buf;
	VstTimeInfo _timeInfo;

	samplepos_t _transport_sample;
	float      _transport_speed;
	mutable std::map <uint32_t, float> _parameter_defaults;
	bool       _eff_bypassed;
};

class LIBARDOUR_API VSTPluginInfo : public PluginInfo
{
public:
	VSTPluginInfo (VST2Info const&);
	bool is_instrument () const;
protected:
	bool _is_instrument;
};

}

#endif
