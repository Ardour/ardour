/*
    Copyright (C) 2010 Paul Davis

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

#ifndef __ardour_vst_plugin_h__
#define __ardour_vst_plugin_h__

#include "ardour/plugin.h"

struct _AEffect;
typedef struct _AEffect AEffect;
struct _VSTHandle;
typedef struct _VSTHandle VSTHandle;
struct _VSTState;
typedef struct _VSTState VSTState;

#include "ardour/vestige/aeffectx.h"

namespace ARDOUR {

class PluginInsert;

/** Parent class for VST plugins of both Windows and Linux varieties */
class LIBARDOUR_API VSTPlugin : public Plugin
{
public:
	VSTPlugin (AudioEngine &, Session &, VSTHandle *);
	virtual ~VSTPlugin ();

	void activate ();
	void deactivate ();

	int set_block_size (pframes_t);
	bool inplace_broken() const { return true; }
	float default_value (uint32_t port);
	float get_parameter (uint32_t port) const;
	uint32_t nth_parameter (uint32_t port, bool& ok) const;
	void set_parameter (uint32_t port, float val);
	void set_parameter_automated (uint32_t port, float val);
	bool load_preset (PresetRecord);
	int get_parameter_descriptor (uint32_t which, ParameterDescriptor&) const;
	std::string describe_parameter (Evoral::Parameter);
	framecnt_t signal_latency() const;
	std::set<Evoral::Parameter> automatable() const;

	bool parameter_is_audio (uint32_t) const { return false; }
	bool parameter_is_control (uint32_t) const { return true; }
	bool parameter_is_input (uint32_t) const { return true; }
	bool parameter_is_output (uint32_t) const { return false; }

	int connect_and_run (BufferSet&,
			framepos_t start, framepos_t end, double speed,
			ChanMapping in, ChanMapping out,
			pframes_t nframes, framecnt_t offset
			);

	std::string unique_id () const;
	const char * label () const;
	const char * name () const;
	const char * maker () const;
	uint32_t parameter_count () const;
        void print_parameter (uint32_t, char*, uint32_t len) const;

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
	framepos_t transport_frame () const { return _transport_frame; }
	float transport_speed () const { return _transport_speed; }


protected:
	void set_plugin (AEffect *);
	gchar* get_chunk (bool) const;
	int set_chunk (gchar const *, bool);
	void add_state (XMLNode *) const;
	bool load_user_preset (PresetRecord);
	bool load_plugin_preset (PresetRecord);
	std::string do_save_preset (std::string name);
	void do_remove_preset (std::string name);
	XMLTree * presets_tree () const;
	std::string presets_file () const;
	void find_presets ();

	VSTHandle* _handle;
	VSTState*  _state;
	AEffect*   _plugin;
	PluginInsert* _pi;
	uint32_t      _num;

	MidiBuffer* _midi_out_buf;
	VstTimeInfo _timeInfo;

	framepos_t _transport_frame;
	float      _transport_speed;
	mutable std::map <uint32_t, float> _parameter_defaults;
};

}

#endif
