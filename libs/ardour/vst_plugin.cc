/*
 * Copyright (C) 2005-2006 Sampo Savolainen <v2@iki.fi>
 * Copyright (C) 2005-2006 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2005-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2007-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2007-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2016 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2013-2015 John Emmas <john@creativepost.co.uk>
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

#include <glib.h>
#include "pbd/gstdio_compat.h"

#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>
#include <glibmm/convert.h>

#include "pbd/floating.h"
#include "pbd/locale_guard.h"

#include "ardour/vst_types.h"
#include "ardour/vst_plugin.h"
#include "ardour/vst2_scan.h"
#include "ardour/vestige/vestige.h"
#include "ardour/session.h"
#include "ardour/filesystem_paths.h"
#include "ardour/audio_buffer.h"

#include "pbd/i18n.h"

using namespace std;
using namespace PBD;
using namespace ARDOUR;

VSTPlugin::VSTPlugin (AudioEngine& engine, Session& session, VSTHandle* handle)
	: Plugin (engine, session)
	, _handle (handle)
	, _state (0)
	, _plugin (0)
	, _pi (0)
	, _num (0)
	, _transport_sample (0)
	, _transport_speed (0.f)
	, _eff_bypassed (false)
{
	memset (&_timeInfo, 0, sizeof(_timeInfo));
}

VSTPlugin::VSTPlugin (const VSTPlugin& other)
	: Plugin (other)
	, _handle (other._handle)
	, _state (other._state)
	, _plugin (other._plugin)
	, _pi (other._pi)
	, _num (other._num)
	, _midi_out_buf (other._midi_out_buf)
	, _transport_sample (0)
	, _transport_speed (0.f)
	, _parameter_defaults (other._parameter_defaults)
	, _eff_bypassed (other._eff_bypassed)
{
	memset (&_timeInfo, 0, sizeof(_timeInfo));
}

VSTPlugin::~VSTPlugin ()
{

}

void
VSTPlugin::open_plugin ()
{
	_plugin = _state->plugin;
	assert (_plugin->ptr1 == this); // should have been set by {mac_vst|fst|lxvst}_instantiate
	_plugin->ptr1 = this;
	_state->plugin->dispatcher (_plugin, effOpen, 0, 0, 0, 0);
	_state->vst_version = _plugin->dispatcher (_plugin, effGetVstVersion, 0, 0, 0, 0);
}

void
VSTPlugin::init_plugin ()
{
	/* set rate and blocksize */
	_plugin->dispatcher (_plugin, effSetSampleRate, 0, 0, NULL, (float) _session.sample_rate());
	_plugin->dispatcher (_plugin, effSetBlockSize, 0, _session.get_block_size(), NULL, 0.0f);
}


uint32_t
VSTPlugin::designated_bypass_port ()
{
	if (_plugin->dispatcher (_plugin, effCanDo, 0, 0, const_cast<char*> ("bypass"), 0.0f) != 0) {
#ifdef ALLOW_VST_BYPASS_TO_FAIL // yet unused, see also plugin_insert.cc
		return UINT32_MAX - 1; // emulate a port
#else
		/* check if plugin actually supports it,
		 * e.g. u-he Presswerk  CanDo "bypass"  but calling effSetBypass is a NO-OP.
		 * (presumably the plugin-author thinks hard-bypassing is a bad idea,
		 * particularly since the plugin itself provides a bypass-port)
		 */
		intptr_t value = 0; // not bypassed
		if (0 != _plugin->dispatcher (_plugin, 44 /*effSetBypass*/, 0, value, NULL, 0)) {
			cerr << "Emulate VST Bypass Port for " << name() << endl; // XXX DEBUG
			return UINT32_MAX - 1; // emulate a port
		} else {
			cerr << "Do *not* Emulate VST Bypass Port for " << name() << endl; // XXX DEBUG
		}
#endif
	}
	return UINT32_MAX;
}

void
VSTPlugin::deactivate ()
{
	_plugin->dispatcher (_plugin, effMainsChanged, 0, 0, NULL, 0.0f);
}

void
VSTPlugin::activate ()
{
	_plugin->dispatcher (_plugin, effMainsChanged, 0, 1, NULL, 0.0f);
}

int
VSTPlugin::set_block_size (pframes_t nframes)
{
	deactivate ();
	_plugin->dispatcher (_plugin, effSetBlockSize, 0, nframes, NULL, 0.0f);
	activate ();
	return 0;
}

bool
VSTPlugin::requires_fixed_sized_buffers () const
{
	/* This controls if Ardour will split the plugin's run()
	 * on automation events in order to pass sample-accurate automation
	 * via standard control-ports.
	 *
	 * When returning true Ardour will *not* sub-divide the process-cycle.
	 * Automation events that happen between cycle-start and cycle-end will be
	 * ignored (ctrl values are interpolated to cycle-start).
	 *
	 * Note: This does not guarantee a fixed block-size.
	 * e.g The process cycle may be split when looping, also
	 * the period-size may change any time: see set_block_size()
	 */
	if (get_info()->n_inputs.n_midi() > 0) {
		/* we don't yet implement midi buffer offsets (for split cycles).
		 * Also session_vst callbacls uses _session.transport_sample() directly
		 * (for BBT) which is not offset for plugin cycle split.
		 */
		return true;
	}
	return false;
}

float
VSTPlugin::default_value (uint32_t which)
{
	return _parameter_defaults[which];
}

float
VSTPlugin::get_parameter (uint32_t which) const
{
	if (which == UINT32_MAX - 1) {
		// ardour uses enable-semantics: 1: enabled, 0: bypassed
		return _eff_bypassed ? 0.f : 1.f;
	}
	return _plugin->getParameter (_plugin, which);
}

void
VSTPlugin::set_parameter (uint32_t which, float newval, sampleoffset_t when)
{
	if (which == UINT32_MAX - 1) {
		// ardour uses enable-semantics: 1: enabled, 0: bypassed
		intptr_t value = (newval <= 0.f) ? 1 : 0;
		cerr << "effSetBypass " << value << endl; // XXX DEBUG
		int rv = _plugin->dispatcher (_plugin, 44 /*effSetBypass*/, 0, value, NULL, 0);
		if (0 != rv) {
			_eff_bypassed = (value == 1);
		} else {
			cerr << "effSetBypass failed rv=" << rv << endl; // XXX DEBUG
#ifdef ALLOW_VST_BYPASS_TO_FAIL // yet unused, see also vst_plugin.cc
			// emit signal.. hard un/bypass from here?!
#endif
		}
		return;
	}

	float oldval = get_parameter (which);

	if (PBD::floateq (oldval, newval, 1)) {
		return;
	}

	_plugin->setParameter (_plugin, which, newval);

	float curval = get_parameter (which);

	if (!PBD::floateq (curval, oldval, 1)) {
		/* value has changed, follow rest of the notification path */
		Plugin::set_parameter (which, newval, when);
	}
}

void
VSTPlugin::parameter_changed_externally (uint32_t which, float value)
{
	ParameterChangedExternally (which, value); /* EMIT SIGNAL */
	Plugin::set_parameter (which, value, 0);
}


uint32_t
VSTPlugin::nth_parameter (uint32_t n, bool& ok) const
{
	ok = true;
	return n;
}

/** Get VST chunk as base64-encoded data.
 *  @param single true for single program, false for all programs.
 *  @return 0-terminated base64-encoded data; must be passed to g_free () by caller.
 */
gchar *
VSTPlugin::get_chunk (bool single) const
{
	guchar* data;
	int32_t data_size = _plugin->dispatcher (_plugin, 23 /* effGetChunk */, single ? 1 : 0, 0, &data, 0);
	if (data_size == 0) {
		return 0;
	}

	return g_base64_encode (data, data_size);
}

/** Set VST chunk from base64-encoded data.
 *  @param 0-terminated base64-encoded data.
 *  @param single true for single program, false for all programs.
 *  @return 0 on success, non-0 on failure
 */
int
VSTPlugin::set_chunk (gchar const * data, bool single)
{
	gsize size = 0;
	int r = 0;
	guchar* raw_data = g_base64_decode (data, &size);
	{
		pthread_mutex_lock (&_state->state_lock);
		r = _plugin->dispatcher (_plugin, 24 /* effSetChunk */, single ? 1 : 0, size, raw_data, 0);
		pthread_mutex_unlock (&_state->state_lock);
	}
	g_free (raw_data);
	return r;
}

void
VSTPlugin::add_state (XMLNode* root) const
{
	LocaleGuard lg;

	if (_plugin->flags & 32 /* effFlagsProgramsChunks */) {

		gchar* data = get_chunk (false);
		if (data == 0) {
			return;
		}

		/* store information */

		XMLNode* chunk_node = new XMLNode (X_("chunk"));

		chunk_node->add_content (data);
		g_free (data);

		chunk_node->set_property ("program", (int) _plugin->dispatcher (_plugin, effGetProgram, 0, 0, NULL, 0));

		root->add_child_nocopy (*chunk_node);

	} else {

		XMLNode* parameters = new XMLNode ("parameters");

		for (int32_t n = 0; n < _plugin->numParams; ++n) {
			char index[64];
			snprintf (index, sizeof (index), "param-%d", n);
			parameters->set_property (index, _plugin->getParameter (_plugin, n));
		}

		root->add_child_nocopy (*parameters);
	}
}

int
VSTPlugin::set_state (const XMLNode& node, int version)
{
	LocaleGuard lg;
	int ret = -1;
	XMLNode* child;

	if ((child = find_named_node (node, X_("chunk"))) != 0) {

		int pgm = -1;
		if (child->get_property (X_("program"), pgm)) {
			_plugin->dispatcher (_plugin, effSetProgram, 0, pgm, NULL, 0);
		};

		XMLNodeList::const_iterator n;

		for (n = child->children ().begin (); n != child->children ().end (); ++n) {
			if ((*n)->is_content ()) {
				/* XXX: this may be dubious for the same reasons that we delay
					 execution of load_preset.
					 */
				ret = set_chunk ((*n)->content().c_str(), false);
			}
		}

	} else if ((child = find_named_node (node, X_("parameters"))) != 0) {

		XMLPropertyList::const_iterator i;

		for (i = child->properties().begin(); i != child->properties().end(); ++i) {
			int32_t param;

			sscanf ((*i)->name().c_str(), "param-%d", &param);
			float value = string_to<float>((*i)->value());

			_plugin->setParameter (_plugin, param, value);
		}

		ret = 0;

	}

	Plugin::set_state (node, version);
	return ret;
}

int
VSTPlugin::get_parameter_descriptor (uint32_t which, ParameterDescriptor& desc) const
{
	VstParameterProperties prop;

	memset (&prop, 0, sizeof (VstParameterProperties));
	prop.flags = 0;

	if (_plugin->dispatcher (_plugin, effGetParameterProperties, which, 0, &prop, 0)) {

		/* i have yet to find or hear of a VST plugin that uses this */
		/* RG: faust2vsti does use this :) */

		if (prop.flags & kVstParameterUsesIntegerMinMax) {
			desc.lower = prop.minInteger;
			desc.upper = prop.maxInteger;
		} else {
			desc.lower = 0;
			desc.upper = 1.0;
		}

		const float range = desc.upper - desc.lower;

		if (prop.flags & kVstParameterUsesIntStep && prop.stepInteger < range) {
			desc.step = prop.stepInteger;
			desc.smallstep = prop.stepInteger;
			desc.largestep = prop.stepInteger;
			desc.integer_step = true;
			desc.rangesteps = 1 + ceilf (range / desc.step);
		} else if (prop.flags & kVstParameterUsesFloatStep && prop.stepFloat < range) {
			desc.step = prop.stepFloat;
			desc.smallstep = prop.smallStepFloat;
			desc.largestep = prop.largeStepFloat;
			desc.rangesteps = 1 + ceilf (range / desc.step);
		} else {
			desc.smallstep = desc.step = range / 300.0f;
			desc.largestep =  range / 30.0f;
		}

		if (strlen(prop.label) == 0) {
			_plugin->dispatcher (_plugin, effGetParamName, which, 0, prop.label, 0);
		}

		desc.toggled = prop.flags & kVstParameterIsSwitch;
		desc.label = Glib::locale_to_utf8 (prop.label);

	} else {

		/* old style */

		char pname[VestigeMaxLabelLen];
		/* some VST plugins expect this buffer to be zero-filled */
		memset (pname, 0, sizeof (pname));

		_plugin->dispatcher (_plugin, effGetParamName, which, 0, pname, 0);

		desc.label = Glib::locale_to_utf8 (pname);
		desc.lower = 0.0f;
		desc.upper = 1.0f;
		desc.smallstep = desc.step = 1.f / 300.f;
		desc.largestep = 1.f / 30.f;
	}

	/* TODO we should really call
	 *   desc.update_steps ()
	 * instead of manually assigning steps. Yet, VST prop is (again)
	 * the odd one out compared to other plugin formats.
	 */

	if (_parameter_defaults.find (which) == _parameter_defaults.end ()) {
		_parameter_defaults[which] = get_parameter (which);
	}
	desc.normal = _parameter_defaults[which];

	return 0;
}

bool
VSTPlugin::load_preset (PresetRecord r)
{
	bool s;

	if (r.user) {
		s = load_user_preset (r);
	} else {
		s = load_plugin_preset (r);
	}

	if (s) {
		Plugin::load_preset (r);
	}

	return s;
}

bool
VSTPlugin::load_plugin_preset (PresetRecord r)
{
	/* This is a plugin-provided preset.
	   We can't dispatch directly here; too many plugins expects only one GUI thread.
	*/

	/* Extract the index of this preset from the URI */
	int id;
	int index;
#ifndef NDEBUG
	int const p = sscanf (r.uri.c_str(), "VST:%d:%d", &id, &index);
	assert (p == 2);
#else
	sscanf (r.uri.c_str(), "VST:%d:%d", &id, &index);
#endif
	_state->want_program = index;
	LoadPresetProgram (); /* EMIT SIGNAL */ /* used for macvst */
	return true;
}

bool
VSTPlugin::load_user_preset (PresetRecord r)
{
	/* This is a user preset; we load it, and this code also knows about the
	   non-direct-dispatch thing.
	*/

	boost::shared_ptr<XMLTree> t (presets_tree ());
	if (t == 0) {
		return false;
	}

	XMLNode* root = t->root ();

	for (XMLNodeList::const_iterator i = root->children().begin(); i != root->children().end(); ++i) {
		std::string label;
		(*i)->get_property (X_("label"), label);

		if (label != r.label) {
			continue;
		}

		if (_plugin->flags & 32 /* effFlagsProgramsChunks */) {

			/* Load a user preset chunk from our XML file and send it via a circuitous route to the plugin */

			if (_state->wanted_chunk) {
				g_free (_state->wanted_chunk);
			}

			for (XMLNodeList::const_iterator j = (*i)->children().begin(); j != (*i)->children().end(); ++j) {
				if ((*j)->is_content ()) {
					/* we can't dispatch directly here; too many plugins expect only one GUI thread */
					gsize size = 0;
					guchar* raw_data = g_base64_decode ((*j)->content().c_str(), &size);
					_state->wanted_chunk = raw_data;
					_state->wanted_chunk_size = size;
					_state->want_chunk = 1;
					LoadPresetProgram (); /* EMIT SIGNAL */ /* used for macvst */
					return true;
				}
			}

			return false;

		} else {

			for (XMLNodeList::const_iterator j = (*i)->children().begin(); j != (*i)->children().end(); ++j) {
				if ((*j)->name() == X_("Parameter")) {
					uint32_t index;
					float value;

					if (!(*j)->get_property (X_("index"), index) ||
					    !(*j)->get_property (X_("value"), value)) {
						assert (false);
						// flag error and continue?
						continue;
					}

					set_parameter (index, value, 0);
					PresetPortSetValue (index, value); /* EMIT SIGNAL */
				}
			}
			return true;
		}
	}
	return false;
}

#include "sha1.c"

string
VSTPlugin::do_save_preset (string name)
{
	boost::shared_ptr<XMLTree> t (presets_tree ());
	if (t == 0) {
		return "";
	}

	// prevent dups -- just in case
	t->root()->remove_nodes_and_delete (X_("label"), name);

	XMLNode* p = 0;

	char tmp[32];
	snprintf (tmp, 31, "%ld", _presets.size() + 1);
	tmp[31] = 0;

	char hash[41];
	Sha1Digest s;
	sha1_init (&s);
	sha1_write (&s, (const uint8_t *) name.c_str(), name.size ());
	sha1_write (&s, (const uint8_t *) tmp, strlen(tmp));
	sha1_result_hash (&s, hash);

	string const uri = string_compose (X_("VST:%1:x%2"), unique_id (), hash);

	if (_plugin->flags & 32 /* effFlagsProgramsChunks */) {
		p = new XMLNode (X_("ChunkPreset"));
	} else {
		p = new XMLNode (X_("Preset"));
	}

	p->set_property (X_("uri"), uri);
	p->set_property (X_("version"), version ());
	p->set_property (X_("label"), name);
	p->set_property (X_("numParams"), parameter_count ());

	if (_plugin->flags & 32) {

		gchar* data = get_chunk (true);
		p->add_content (string (data));
		g_free (data);

	} else {

		for (uint32_t i = 0; i < parameter_count(); ++i) {
			if (parameter_is_input (i)) {
				XMLNode* c = new XMLNode (X_("Parameter"));
				c->set_property (X_("index"), i);
				c->set_property (X_("value"), get_parameter (i));
				p->add_child_nocopy (*c);
			}
		}
	}

	t->root()->add_child_nocopy (*p);

	std::string f = Glib::build_filename (ARDOUR::user_config_directory (), "presets");
	f = Glib::build_filename (f, presets_file ());

	t->write (f);
	return uri;
}

void
VSTPlugin::do_remove_preset (string name)
{
	boost::shared_ptr<XMLTree> t (presets_tree ());
	if (t == 0) {
		return;
	}

	t->root()->remove_nodes_and_delete (X_("label"), name);

	std::string f = Glib::build_filename (ARDOUR::user_config_directory (), "presets");
	f = Glib::build_filename (f, presets_file ());

	t->write (f);
}

string
VSTPlugin::describe_parameter (Evoral::Parameter param)
{
	char name[VestigeMaxLabelLen];
	if (param.id() == UINT32_MAX - 1) {
		strcpy (name, _("Plugin Enable"));
		return name;
	}

	memset (name, 0, sizeof (name));

	/* some VST plugins expect this buffer to be zero-filled */

	_plugin->dispatcher (_plugin, effGetParamName, param.id(), 0, name, 0);

	if (name[0] == '\0') {
		strcpy (name, _("Unknown"));
	}

	return name;
}

samplecnt_t
VSTPlugin::plugin_latency () const
{
#if ( defined(__x86_64__) || defined(_M_X64) )
	return *((int32_t *) (((char *) &_plugin->flags) + 24)); /* initialDelay */
#else
	return *((int32_t *) (((char *) &_plugin->flags) + 12)); /* initialDelay */
#endif
}

set<Evoral::Parameter>
VSTPlugin::automatable () const
{
	set<Evoral::Parameter> ret;

	for (uint32_t i = 0; i < parameter_count(); ++i) {
		if (_plugin->dispatcher (_plugin, effCanBeAutomated, i, 0, NULL, 0)) {
			ret.insert (ret.end(), Evoral::Parameter(PluginAutomation, 0, i));
		}
	}

	return ret;
}

int
VSTPlugin::connect_and_run (BufferSet& bufs,
		samplepos_t start, samplepos_t end, double speed,
		ChanMapping const& in_map, ChanMapping const& out_map,
		pframes_t nframes, samplecnt_t offset)
{
	Plugin::connect_and_run(bufs, start, end, speed, in_map, out_map, nframes, offset);

	if (pthread_mutex_trylock (&_state->state_lock)) {
		/* by convention 'effSetChunk' should not be called while processing
		 * http://www.reaper.fm/sdk/vst/vst_ext.php
		 *
		 * All VSTs don't use in-place, PluginInsert::connect_and_run()
		 * does clear output buffers, so we can just return.
		 */
		return 0;
	}

	/* remain at zero during pre-roll at zero */
	_transport_speed = end > 0 ? speed : 0;
	_transport_sample = std::max (samplepos_t (0), start);

	ChanCount bufs_count;
	bufs_count.set(DataType::AUDIO, 1);
	bufs_count.set(DataType::MIDI, 1);
	_midi_out_buf = 0;

	BufferSet& silent_bufs  = _session.get_silent_buffers(bufs_count);
	BufferSet& scratch_bufs = _session.get_scratch_buffers(bufs_count);

	/* VC++ doesn't support the C99 extension that allows

	   typeName foo[variableDefiningSize];

	   Use alloca instead of dynamic array (rather than std::vector which
	   allocs on the heap) because this is realtime code.
	*/

	float** ins = (float**)alloca(_plugin->numInputs*sizeof(float*));
	float** outs = (float**)alloca(_plugin->numOutputs*sizeof(float*));

	int32_t i;

	uint32_t in_index = 0;
	for (i = 0; i < (int32_t) _plugin->numInputs; ++i) {
		uint32_t  index;
		bool      valid = false;
		index = in_map.get(DataType::AUDIO, in_index++, &valid);
		ins[i] = (valid)
					? bufs.get_audio(index).data(offset)
					: silent_bufs.get_audio(0).data(offset);
	}

	uint32_t out_index = 0;
	for (i = 0; i < (int32_t) _plugin->numOutputs; ++i) {
		uint32_t  index;
		bool      valid = false;
		index = out_map.get(DataType::AUDIO, out_index++, &valid);
		outs[i] = (valid)
			? bufs.get_audio(index).data(offset)
			: scratch_bufs.get_audio(0).data(offset);
	}

	if (bufs.count().n_midi() > 0) {
		VstEvents* v = 0;
		bool valid = false;
		const uint32_t buf_index_in = in_map.get(DataType::MIDI, 0, &valid);
		/* TODO: apply offset to MIDI buffer and trim at nframes */
		if (valid) {
			v = bufs.get_vst_midi (buf_index_in);
		}
		valid = false;
		const uint32_t buf_index_out = out_map.get(DataType::MIDI, 0, &valid);
		if (valid) {
			_midi_out_buf = &bufs.get_midi(buf_index_out);
			/* TODO: apply offset to MIDI buffer and trim at nframes */
			_midi_out_buf->silence(nframes, offset);
		} else {
			_midi_out_buf = 0;
		}
		if (v) {
			_plugin->dispatcher (_plugin, effProcessEvents, 0, 0, v, 0);
		}
	}

	/* we already know it can support processReplacing */
	_plugin->processReplacing (_plugin, &ins[0], &outs[0], nframes);
	_midi_out_buf = 0;

	pthread_mutex_unlock (&_state->state_lock);
	return 0;
}

string
VSTPlugin::unique_id () const
{
	char buf[32];

	snprintf (buf, sizeof (buf), "%d", _plugin->uniqueID);

	return string (buf);
}


const char *
VSTPlugin::name () const
{
	if (!_info->name.empty ()) {
		return _info->name.c_str();
	}
	return _handle->name;
}

const char *
VSTPlugin::maker () const
{
	return _info->creator.c_str();
}

const char *
VSTPlugin::label () const
{
	return _handle->name;
}

int32_t
VSTPlugin::version () const
{
	return _plugin->version;
}

uint32_t
VSTPlugin::parameter_count () const
{
	return _plugin->numParams;
}

bool
VSTPlugin::has_editor () const
{
	return _plugin->flags & effFlagsHasEditor;
}

bool
VSTPlugin::print_parameter (uint32_t param, std::string& rv) const
{
	char buf[64];
	size_t len = sizeof(buf);
	assert (len > VestigeMaxShortLabelLen);
	memset (buf, 0, len);

	_plugin->dispatcher (_plugin, 7 /* effGetParamDisplay */, param, 0, buf, 0);

	if (buf[0] == '\0') {
		return false;
	}

	buf[len - 1] = '\0';

	char* first_nonws = buf;
	while (*first_nonws && isspace (*first_nonws)) {
		++first_nonws;
	}

	if (*first_nonws == '\0') {
		return false;
	}

	memmove (buf, first_nonws, strlen (buf) - (first_nonws - buf) + 1);

	/* optional Unit label */
	char label[VestigeMaxNameLen];
	memset (label, 0, sizeof (label));
	_plugin->dispatcher (_plugin, 6 /* effGetParamLabel */, param, 0, label, 0);

	if (strlen (label) > 0) {
		std::string lbl = std::string (" ") + Glib::locale_to_utf8 (label);
		strncat (buf, lbl.c_str(), strlen (buf) - 1);
	}

	rv = std::string (buf);
	return true;
}

void
VSTPlugin::find_presets ()
{
	/* Built-in presets */

	int const vst_version = _plugin->dispatcher (_plugin, effGetVstVersion, 0, 0, NULL, 0);
	for (int i = 0; i < _plugin->numPrograms; ++i) {

		PresetRecord r (string_compose (X_("VST:%1:%2"), unique_id (), std::setw(4), std::setfill('0'), i), "", false);

		if (vst_version >= 2) {
			char buf[256];
			if (_plugin->dispatcher (_plugin, 29, i, 0, buf, 0) == 1) {
				r.label = buf;
			} else {
				r.label = string_compose (_("Preset %1"), i);
			}
		} else {
			r.label = string_compose (_("Preset %1"), i);
		}

		_presets.insert (make_pair (r.uri, r));
	}

	/* User presets from our XML file */

	boost::shared_ptr<XMLTree> t (presets_tree ());

	if (t) {
		XMLNode* root = t->root ();
		for (XMLNodeList::const_iterator i = root->children().begin(); i != root->children().end(); ++i) {
			std::string uri;
			std::string label;

			if (!(*i)->get_property (X_("uri"), uri) || !(*i)->get_property (X_("label"), label)) {
				assert(false);
			}

			PresetRecord r (uri, label, true);
			_presets.insert (make_pair (r.uri, r));
		}
	}

}

/** @return XMLTree with our user presets; could be a new one if no existing
 *  one was found, or 0 if one was present but badly-formatted.
 */
XMLTree *
VSTPlugin::presets_tree () const
{
	XMLTree* t = new XMLTree;

	std::string p = Glib::build_filename (ARDOUR::user_config_directory (), "presets");

	if (!Glib::file_test (p, Glib::FILE_TEST_IS_DIR)) {
		if (g_mkdir_with_parents (p.c_str(), 0755) != 0) {
			error << _("Unable to make VST presets directory") << endmsg;
		};
	}

	p = Glib::build_filename (p, presets_file ());

	if (!Glib::file_test (p, Glib::FILE_TEST_EXISTS)) {
		t->set_root (new XMLNode (X_("VSTPresets")));
		return t;
	}

	t->set_filename (p);
	if (!t->read ()) {
		delete t;
		return 0;
	}

	return t;
}

/** @return Index of the first user preset in our lists */
int
VSTPlugin::first_user_preset_index () const
{
	return _plugin->numPrograms;
}

string
VSTPlugin::presets_file () const
{
	return string("vst-") + unique_id ();
}


VSTPluginInfo::VSTPluginInfo (VST2Info const& nfo)
{

	char buf[32];
	snprintf (buf, sizeof (buf), "%d", nfo.id);
	unique_id = buf;

	index = 0;

	name     = nfo.name;
	creator  = nfo.creator;
	category = nfo.category;

	n_inputs.set_audio  (nfo.n_inputs);
	n_outputs.set_audio (nfo.n_outputs);
	n_inputs.set_midi   (nfo.n_midi_inputs);
	n_outputs.set_midi  (nfo.n_midi_outputs);

	_is_instrument = nfo.is_instrument;
}

bool
VSTPluginInfo::is_instrument () const
{
	if (_is_instrument) {
		return true;
	}
	return PluginInfo::is_instrument ();
}
