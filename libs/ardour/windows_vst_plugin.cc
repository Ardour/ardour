/*
    Copyright (C) 2004 Paul Davis

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

#include <algorithm>
#include <vector>
#include <string>
#include <cctype>

#include <cstdlib>
#include <cstdio> // so libraptor doesn't complain
#include <cmath>
#include <dirent.h>
#include <cstring> // for memmove
#include <sys/stat.h>
#include <cerrno>

#include <glibmm/miscutils.h>

#include <lrdf.h>
#include <fst.h>

#include "pbd/compose.h"
#include "pbd/error.h"
#include "pbd/pathscanner.h"
#include "pbd/xml++.h"

#include <fst.h>

#include "ardour/session.h"
#include "ardour/audioengine.h"
#include "ardour/filesystem_paths.h"
#include "ardour/windows_vst_plugin.h"
#include "ardour/buffer_set.h"
#include "ardour/audio_buffer.h"
#include "ardour/midi_buffer.h"

#include "pbd/stl_delete.h"

#include "i18n.h"
#include <locale.h>

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using std::min;
using std::max;

WindowsVSTPlugin::WindowsVSTPlugin (AudioEngine& e, Session& session, FSTHandle* h)
	: Plugin (e, session)
{
	handle = h;

	if ((_fst = fst_instantiate (handle, Session::vst_callback, this)) == 0) {
		throw failed_constructor();
	}

	_plugin = _fst->plugin;
	_plugin->user = this;

	/* set rate and blocksize */

	_plugin->dispatcher (_plugin, effSetSampleRate, 0, 0, NULL,
			     (float) session.frame_rate());
	_plugin->dispatcher (_plugin, effSetBlockSize, 0,
			     session.get_block_size(), NULL, 0.0f);

	/* set program to zero */

	_plugin->dispatcher (_plugin, effSetProgram, 0, 0, NULL, 0.0f);

	// Plugin::setup_controls ();
}

WindowsVSTPlugin::WindowsVSTPlugin (const WindowsVSTPlugin &other)
	: Plugin (other)
{
	handle = other.handle;

	if ((_fst = fst_instantiate (handle, Session::vst_callback, this)) == 0) {
		throw failed_constructor();
	}
	_plugin = _fst->plugin;

	// Plugin::setup_controls ();
}

WindowsVSTPlugin::~WindowsVSTPlugin ()
{
	deactivate ();
	fst_close (_fst);
}

int
WindowsVSTPlugin::set_block_size (pframes_t nframes)
{
	deactivate ();
	_plugin->dispatcher (_plugin, effSetBlockSize, 0, nframes, NULL, 0.0f);
	activate ();
        return 0;
}

float
WindowsVSTPlugin::default_value (uint32_t port)
{
	return 0;
}

void
WindowsVSTPlugin::set_parameter (uint32_t which, float val)
{
	_plugin->setParameter (_plugin, which, val);

	if (_fst->want_program == -1 && _fst->want_chunk == 0) {
		/* Heinous hack: Plugin::set_parameter below updates the `modified' status of the
		   current preset, but if _fst->want_program is not -1 then there is a preset
		   setup pending or in progress, which we don't want any `modified' updates
		   to happen for.  So we only do this if _fst->want_program is -1.
		*/
		Plugin::set_parameter (which, val);
	}
}

float
WindowsVSTPlugin::get_parameter (uint32_t which) const
{
	return _plugin->getParameter (_plugin, which);

}

uint32_t
WindowsVSTPlugin::nth_parameter (uint32_t n, bool& ok) const
{
	ok = true;
	return n;
}

/** Get VST chunk as base64-encoded data.
 *  @param single true for single program, false for all programs.
 *  @return 0-terminated base64-encoded data; must be passed to g_free () by caller.
 */
gchar *
WindowsVSTPlugin::get_chunk (bool single) const
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
WindowsVSTPlugin::set_chunk (gchar const * data, bool single)
{
	gsize size = 0;
	guchar* raw_data = g_base64_decode (data, &size);
	int const r = _plugin->dispatcher (_plugin, 24 /* effSetChunk */, single ? 1 : 0, size, raw_data, 0);
	g_free (raw_data);
	return r;
}

void
WindowsVSTPlugin::add_state (XMLNode* root) const
{
	LocaleGuard lg (X_("POSIX"));

	if (_fst->current_program != -1) {
		char buf[32];
		snprintf (buf, sizeof (buf), "%d", _fst->current_program);
		root->add_property ("current-program", buf);
	}

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
			char val[32];
			snprintf (index, sizeof (index), "param_%d", n);
			snprintf (val, sizeof (val), "%.12g", _plugin->getParameter (_plugin, n));
			parameters->add_property (index, val);
		}

		root->add_child_nocopy (*parameters);
	}
}

int
WindowsVSTPlugin::set_state (const XMLNode& node, int version)
{
	LocaleGuard lg (X_("POSIX"));

	if (node.name() != state_node_name()) {
		error << _("Bad node sent to WindowsVSTPlugin::set_state") << endmsg;
		return 0;
	}

	const XMLProperty* prop;

	if ((prop = node.property ("current-program")) != 0) {
		_fst->want_program = atoi (prop->value().c_str());
	}

	XMLNode* child;
	int ret = -1;

	if ((child = find_named_node (node, X_("chunk"))) != 0) {

		XMLPropertyList::const_iterator i;
		XMLNodeList::const_iterator n;
		int ret = -1;

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
			float val;

			sscanf ((*i)->name().c_str(), "param_%d", &param);
			sscanf ((*i)->value().c_str(), "%f", &val);

			_plugin->setParameter (_plugin, param, val);
		}

		/* program number is not knowable */

		_fst->current_program = -1;

		ret = 0;

	}

	Plugin::set_state (node, version);
	return ret;
}

int
WindowsVSTPlugin::get_parameter_descriptor (uint32_t which, ParameterDescriptor& desc) const
{
	VstParameterProperties prop;

	desc.min_unbound = false;
	desc.max_unbound = false;
	prop.flags = 0;

	if (_plugin->dispatcher (_plugin, effGetParameterProperties, which, 0, &prop, 0)) {

#ifdef VESTIGE_COMPLETE

		/* i have yet to find or hear of a VST plugin that uses this */

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

		desc.toggled = prop.flags & kVstParameterIsSwitch;
		desc.logarithmic = false;
		desc.sr_dependent = false;
		desc.label = prop.label;
#endif

	} else {

		/* old style */

		char label[64];
		label[0] = '\0';

		_plugin->dispatcher (_plugin, effGetParamName, which, 0, label, 0);

		desc.label = label;
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

	return 0;
}

bool
WindowsVSTPlugin::load_preset (PresetRecord r)
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
WindowsVSTPlugin::load_plugin_preset (PresetRecord r)
{
	/* This is a plugin-provided preset.
	   We can't dispatch directly here; too many plugins expects only one GUI thread.
	*/

	/* Extract the index of this preset from the URI */
	int id;
	int index;
	int const p = sscanf (r.uri.c_str(), "VST:%d:%d", &id, &index);
	assert (p == 2);

	_fst->want_program = index;
	return true;
}

bool
WindowsVSTPlugin::load_user_preset (PresetRecord r)
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

		XMLProperty* uri = (*i)->property (X_("uri"));
		XMLProperty* label = (*i)->property (X_("label"));

		assert (uri);
		assert (label);

		if (label->value() != r.label) {
			continue;
		}

		if (_plugin->flags & 32 /* effFlagsProgramsChunks */) {

			/* Load a user preset chunk from our XML file and send it via a circuitous route to the plugin */

			if (_fst->wanted_chunk) {
				g_free (_fst->wanted_chunk);
			}

			for (XMLNodeList::const_iterator j = (*i)->children().begin(); j != (*i)->children().end(); ++j) {
				if ((*j)->is_content ()) {
					/* we can't dispatch directly here; too many plugins expect only one GUI thread */
					gsize size = 0;
					guchar* raw_data = g_base64_decode ((*j)->content().c_str(), &size);
					_fst->wanted_chunk = raw_data;
					_fst->wanted_chunk_size = size;
					_fst->want_chunk = 1;
					return true;
				}
			}

			return false;

		} else {

			for (XMLNodeList::const_iterator j = (*i)->children().begin(); j != (*i)->children().end(); ++j) {
				if ((*j)->name() == X_("Parameter")) {

						XMLProperty* index = (*j)->property (X_("index"));
						XMLProperty* value = (*j)->property (X_("value"));

						assert (index);
						assert (value);

						set_parameter (atoi (index->value().c_str()), atof (value->value().c_str ()));
				}
			}

			return true;
		}
	}

	return false;
}

string
WindowsVSTPlugin::do_save_preset (string name)
{
	boost::shared_ptr<XMLTree> t (presets_tree ());
	if (t == 0) {
		return "";
	}

	XMLNode* p = 0;
	/* XXX: use of _presets.size() + 1 for the unique ID here is dubious at best */
	string const uri = string_compose (X_("VST:%1:%2"), unique_id (), _presets.size() + 1);

	if (_plugin->flags & 32 /* effFlagsProgramsChunks */) {

		p = new XMLNode (X_("ChunkPreset"));
		p->add_property (X_("uri"), uri);
		p->add_property (X_("label"), name);
		gchar* data = get_chunk (true);
		p->add_content (string (data));
		g_free (data);

	} else {

		p = new XMLNode (X_("Preset"));
		p->add_property (X_("uri"), uri);
		p->add_property (X_("label"), name);

		for (uint32_t i = 0; i < parameter_count(); ++i) {
			if (parameter_is_input (i)) {
				XMLNode* c = new XMLNode (X_("Parameter"));
				c->add_property (X_("index"), string_compose ("%1", i));
				c->add_property (X_("value"), string_compose ("%1", get_parameter (i)));
				p->add_child_nocopy (*c);
			}
		}

	}

	t->root()->add_child_nocopy (*p);

	sys::path f = ARDOUR::user_config_directory ();
	f /= "presets";
	f /= presets_file ();

	t->write (f.to_string ());
	return uri;
}

void
WindowsVSTPlugin::do_remove_preset (string name)
{
	boost::shared_ptr<XMLTree> t (presets_tree ());
	if (t == 0) {
		return;
	}

	t->root()->remove_nodes_and_delete (X_("label"), name);

	sys::path f = ARDOUR::user_config_directory ();
	f /= "presets";
	f /= presets_file ();

	t->write (f.to_string ());
}

string
WindowsVSTPlugin::describe_parameter (Evoral::Parameter param)
{
	char name[64];
	_plugin->dispatcher (_plugin, effGetParamName, param.id(), 0, name, 0);
	return name;
}

framecnt_t
WindowsVSTPlugin::signal_latency () const
{
	if (_user_latency) {
		return _user_latency;
	}

#ifdef VESTIGE_HEADER
        return *((framecnt_t *) (((char *) &_plugin->flags) + 12)); /* initialDelay */
#else
	return _plugin->initial_delay;
#endif
}

set<Evoral::Parameter>
WindowsVSTPlugin::automatable () const
{
	set<Evoral::Parameter> ret;

	for (uint32_t i = 0; i < parameter_count(); ++i){
		ret.insert (ret.end(), Evoral::Parameter(PluginAutomation, 0, i));
	}

	return ret;
}

int
WindowsVSTPlugin::connect_and_run (BufferSet& bufs,
		ChanMapping in_map, ChanMapping out_map,
		pframes_t nframes, framecnt_t offset)
{
	Plugin::connect_and_run (bufs, in_map, out_map, nframes, offset);

	float *ins[_plugin->numInputs];
	float *outs[_plugin->numOutputs];
	int32_t i;

	const uint32_t nbufs = bufs.count().n_audio();

	int in_index = 0;
	for (i = 0; i < (int32_t) _plugin->numInputs; ++i) {
		ins[i] = bufs.get_audio(min((uint32_t) in_index, nbufs - 1)).data() + offset;
		in_index++;
	}

	int out_index = 0;
	for (i = 0; i < (int32_t) _plugin->numOutputs; ++i) {
		outs[i] = bufs.get_audio(min((uint32_t) out_index, nbufs - 1)).data() + offset;

		/* unbelievably, several VST plugins still rely on Cubase
		   behaviour and do not silence the buffer in processReplacing
		   when they have no output.
		*/

		// memset (outs[i], 0, sizeof (Sample) * nframes);
		out_index++;
	}


	if (bufs.count().n_midi() > 0) {
		VstEvents* v = bufs.get_vst_midi (0);
		_plugin->dispatcher (_plugin, effProcessEvents, 0, 0, v, 0);
	}

	/* we already know it can support processReplacing */
	_plugin->processReplacing (_plugin, ins, outs, nframes);

	return 0;
}

void
WindowsVSTPlugin::deactivate ()
{
	_plugin->dispatcher (_plugin, effMainsChanged, 0, 0, NULL, 0.0f);
}

void
WindowsVSTPlugin::activate ()
{
	_plugin->dispatcher (_plugin, effMainsChanged, 0, 1, NULL, 0.0f);
}

string
WindowsVSTPlugin::unique_id() const
{
	char buf[32];

#ifdef VESTIGE_HEADER
       snprintf (buf, sizeof (buf), "%d", *((int32_t*) &_plugin->unused_id));
#else
       snprintf (buf, sizeof (buf), "%d", _plugin->uniqueID);
#endif
       return string (buf);
}


const char *
WindowsVSTPlugin::name () const
{
	return handle->name;
}

const char *
WindowsVSTPlugin::maker () const
{
	return _info->creator.c_str();
}

const char *
WindowsVSTPlugin::label () const
{
	return handle->name;
}

uint32_t
WindowsVSTPlugin::parameter_count() const
{
	return _plugin->numParams;
}

bool
WindowsVSTPlugin::has_editor () const
{
	return _plugin->flags & effFlagsHasEditor;
}

void
WindowsVSTPlugin::print_parameter (uint32_t param, char *buf, uint32_t len) const
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

PluginPtr
WindowsVSTPluginInfo::load (Session& session)
{
	try {
		PluginPtr plugin;

		if (Config->get_use_windows_vst ()) {
			FSTHandle* handle;

			handle = fst_load(path.c_str());

			if ( (int)handle == -1) {
				error << string_compose(_("VST: cannot load module from \"%1\""), path) << endmsg;
			} else {
				plugin.reset (new WindowsVSTPlugin (session.engine(), session, handle));
			}
		} else {
			error << _("You asked ardour to not use any VST plugins") << endmsg;
			return PluginPtr ((Plugin*) 0);
		}

		plugin->set_info(PluginInfoPtr(new WindowsVSTPluginInfo(*this)));
		return plugin;
	}

	catch (failed_constructor &err) {
		return PluginPtr ((Plugin*) 0);
	}
}

void
WindowsVSTPlugin::find_presets ()
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

			XMLProperty* uri = (*i)->property (X_("uri"));
			XMLProperty* label = (*i)->property (X_("label"));

			assert (uri);
			assert (label);

			PresetRecord r (uri->value(), label->value(), true);
			_presets.insert (make_pair (r.uri, r));
		}
	}

}

/** @return XMLTree with our user presets; could be a new one if no existing
 *  one was found, or 0 if one was present but badly-formatted.
 */
XMLTree *
WindowsVSTPlugin::presets_tree () const
{
	XMLTree* t = new XMLTree;

	sys::path p = ARDOUR::user_config_directory ();
	p /= "presets";

	if (!is_directory (p)) {
		create_directory (p);
	}

	p /= presets_file ();

	if (!exists (p)) {
		t->set_root (new XMLNode (X_("WindowsVSTPresets")));
		return t;
	}

	t->set_filename (p.to_string ());
	if (!t->read ()) {
		delete t;
		return 0;
	}

	return t;
}

/** @return Index of the first user preset in our lists */
int
WindowsVSTPlugin::first_user_preset_index () const
{
	return _plugin->numPrograms;
}

string
WindowsVSTPlugin::presets_file () const
{
	return string_compose ("vst-%1", unique_id ());
}

WindowsVSTPluginInfo::WindowsVSTPluginInfo()
{
       type = ARDOUR::Windows_VST;
}

