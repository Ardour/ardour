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

#include <glib.h>
#include "pbd/gstdio_compat.h"

#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>
#include <glibmm/convert.h>

#include "pbd/floating.h"
#include "pbd/locale_guard.h"

#include "ardour/vst_plugin.h"
#include "ardour/vestige/aeffectx.h"
#include "ardour/session.h"
#include "ardour/vst_types.h"
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
	, _transport_frame (0)
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
	, _transport_frame (0)
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
	assert (_plugin->user == this); // should have been set by {mac_vst|fst|lxvst}_instantiate
	_plugin->user = this;
	_state->plugin->dispatcher (_plugin, effOpen, 0, 0, 0, 0);
	_state->vst_version = _plugin->dispatcher (_plugin, effGetVstVersion, 0, 0, 0, 0);
}

void
VSTPlugin::init_plugin ()
{
	/* set rate and blocksize */
	_plugin->dispatcher (_plugin, effSetSampleRate, 0, 0, NULL, (float) _session.frame_rate());
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
VSTPlugin::set_parameter (uint32_t which, float newval)
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
		Plugin::set_parameter (which, newval);
	}
}

void
VSTPlugin::parameter_changed_externally (uint32_t which, float value )
{
	ParameterChangedExternally (which, value); /* EMIT SIGNAL */
	Plugin::set_parameter (which, value);
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

	if (node.name() != state_node_name()) {
		error << _("Bad node sent to VSTPlugin::set_state") << endmsg;
		return 0;
	}

#ifndef NO_PLUGIN_STATE
	XMLNode* child;

	if ((child = find_named_node (node, X_("chunk"))) != 0) {

		XMLPropertyList::const_iterator i;
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
#endif

	Plugin::set_state (node, version);
	return ret;
}


int
VSTPlugin::get_parameter_descriptor (uint32_t which, ParameterDescriptor& desc) const
{
	VstParameterProperties prop;

	memset (&prop, 0, sizeof (VstParameterProperties));
	desc.min_unbound = false;
	desc.max_unbound = false;
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

		if (prop.flags & kVstParameterUsesIntStep) {

			desc.step = prop.stepInteger;
			desc.smallstep = prop.stepInteger;
			desc.largestep = prop.stepInteger;

		} else if (prop.flags & kVstParameterUsesFloatStep) {

			desc.step = prop.stepFloat;
			desc.smallstep = prop.smallStepFloat;
			desc.largestep = prop.largeStepFloat;

		} else {

			float range = desc.upper - desc.lower;

			desc.step = range / 100.0f;
			desc.smallstep = desc.step / 2.0f;
			desc.largestep = desc.step * 10.0f;
		}

		if (strlen(prop.label) == 0) {
			_plugin->dispatcher (_plugin, effGetParamName, which, 0, prop.label, 0);
		}

		desc.toggled = prop.flags & kVstParameterIsSwitch;
		desc.logarithmic = false;
		desc.sr_dependent = false;
		desc.label = Glib::locale_to_utf8 (prop.label);

	} else {

		/* old style */

		char label[64];
		/* some VST plugins expect this buffer to be zero-filled */
		memset (label, 0, sizeof (label));

		_plugin->dispatcher (_plugin, effGetParamName, which, 0, label, 0);

		desc.label = Glib::locale_to_utf8 (label);
		desc.integer_step = false;
		desc.lower = 0.0f;
		desc.upper = 1.0f;
		desc.step = 0.01f;
		desc.smallstep = 0.005f;
		desc.largestep = 0.1f;
		desc.toggled = false;
		desc.logarithmic = false;
		desc.sr_dependent = false;
	}

	desc.normal = get_parameter (which);
	if (_parameter_defaults.find (which) == _parameter_defaults.end ()) {
		_parameter_defaults[which] = desc.normal;
	}

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
					  // flag error and continue?
						assert (false);
					}

					set_parameter (index, value);
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
		p->set_property (X_("uri"), uri);
		p->set_property (X_("label"), name);
		gchar* data = get_chunk (true);
		p->add_content (string (data));
		g_free (data);

	} else {

		p = new XMLNode (X_("Preset"));
		p->set_property (X_("uri"), uri);
		p->set_property (X_("label"), name);

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
	char name[64];
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

framecnt_t
VSTPlugin::signal_latency () const
{
	if (_user_latency) {
		return _user_latency;
	}

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
		ret.insert (ret.end(), Evoral::Parameter(PluginAutomation, 0, i));
	}

	return ret;
}

int
VSTPlugin::connect_and_run (BufferSet& bufs,
		framepos_t start, framepos_t end, double speed,
		ChanMapping in_map, ChanMapping out_map,
		pframes_t nframes, framecnt_t offset)
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

	_transport_frame = start;
	_transport_speed = speed;

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
		if (valid) {
			v = bufs.get_vst_midi (buf_index_in);
		}
		valid = false;
		const uint32_t buf_index_out = out_map.get(DataType::MIDI, 0, &valid);
		if (valid) {
			_midi_out_buf = &bufs.get_midi(buf_index_out);
			_midi_out_buf->silence(0, 0);
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

void
VSTPlugin::print_parameter (uint32_t param, char *buf, uint32_t /*len*/) const
{
	char *first_nonws;

	_plugin->dispatcher (_plugin, 7 /* effGetParamDisplay */, param, 0, buf, 0);

	if (buf[0] == '\0') {
		return;
	}

	first_nonws = buf;
	while (*first_nonws && isspace (*first_nonws)) {
		first_nonws++;
	}

	if (*first_nonws == '\0') {
		return;
	}

	memmove (buf, first_nonws, strlen (buf) - (first_nonws - buf) + 1);
}

void
VSTPlugin::find_presets ()
{
	/* Built-in presets */

	int const vst_version = _plugin->dispatcher (_plugin, effGetVstVersion, 0, 0, NULL, 0);
	for (int i = 0; i < _plugin->numPrograms; ++i) {
		PresetRecord r (string_compose (X_("VST:%1:%2"), unique_id (), i), "", false);

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
	return string_compose ("vst-%1", unique_id ());
}

