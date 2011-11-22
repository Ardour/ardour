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

/**********************************************************************/
/*Native linuxVST (LXVST) variant of vst_plugin.cc etc                */
/**********************************************************************/


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

/*Include for the new native vst engine - vstfx.h*/
#include <stdint.h>
#include <ardour/vstfx.h>

#include "pbd/compose.h"
#include "pbd/error.h"
#include "pbd/pathscanner.h"
#include "pbd/xml++.h"

#include "ardour/session.h"
#include "ardour/audioengine.h"
#include "ardour/filesystem_paths.h"
#include "ardour/lxvst_plugin.h"
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

LXVSTPlugin::LXVSTPlugin (AudioEngine& e, Session& session, VSTFXHandle* h)
	: Plugin (e, session)
{
	handle = h;

	/*Instantiate the plugin and return a VSTFX* */

	if ((_vstfx = vstfx_instantiate (handle, Session::lxvst_callback, this)) == 0) {
		throw failed_constructor();
	}

	/* Call into vstfx to get a pointer to the instance of the VST plugin*/

	_plugin = _vstfx->plugin;
	_plugin->user = this;

	/* set rate and blocksize */

	_plugin->dispatcher (_plugin, effSetSampleRate, 0, 0, NULL, (float) session.frame_rate());
	
	_plugin->dispatcher (_plugin, effSetBlockSize, 0, session.get_block_size(), NULL, 0.0f);

	/* set program to zero */

	_plugin->dispatcher (_plugin, effSetProgram, 0, 0, NULL, 0.0f);

	// Plugin::setup_controls ();
}

LXVSTPlugin::LXVSTPlugin (const LXVSTPlugin &other)
	: Plugin (other)
{
	handle = other.handle;

	if ((_vstfx = vstfx_instantiate (handle, Session::lxvst_callback, this)) == 0) {
		throw failed_constructor();
	}
	_plugin = _vstfx->plugin;

	// Plugin::setup_controls ();
}

LXVSTPlugin::~LXVSTPlugin ()
{
	deactivate ();
	vstfx_close (_vstfx);
}

int 
LXVSTPlugin::set_block_size (pframes_t nframes)
{
	deactivate ();
	_plugin->dispatcher (_plugin, effSetBlockSize, 0, nframes, NULL, 0.0f);
	activate ();
	return 0;
}

float 
LXVSTPlugin::default_value (uint32_t)
{
	return 0;
}

void 
LXVSTPlugin::set_parameter (uint32_t which, float val)
{
	_plugin->setParameter (_plugin, which, val);
	
	if (_vstfx->want_program == -1 && _vstfx->want_chunk == 0) {
		/* Heinous hack: Plugin::set_parameter below updates the `modified' status of the
		   current preset, but if _vstfx->want_program is not -1 then there is a preset
		   setup pending or in progress, which we don't want any `modified' updates
		   to happen for.  So we only do this if _vstfx->want_program is -1.
		*/
		Plugin::set_parameter (which, val);
	}
}

float 
LXVSTPlugin::get_parameter (uint32_t which) const
{
	return _plugin->getParameter (_plugin, which);

}

uint32_t 
LXVSTPlugin::nth_parameter (uint32_t n, bool& ok) const
{
	ok = true;
	return n;
}

/** Get VST chunk as base64-encoded data.
 *  @param single true for single program, false for all programs.
 *  @return 0-terminated base64-encoded data; must be passed to g_free () by caller.
 */
gchar *LXVSTPlugin::get_chunk (bool single) const
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
LXVSTPlugin::set_chunk (gchar const * data, bool single)
{
	gsize size = 0;
	guchar* raw_data = g_base64_decode (data, &size);
	int const r = _plugin->dispatcher (_plugin, 24 /* effSetChunk */, single ? 1 : 0, size, raw_data, 0);
	g_free (raw_data);
	return r;
}

void 
LXVSTPlugin::add_state (XMLNode* root) const
{
	LocaleGuard lg (X_("POSIX"));

	if (_vstfx->current_program != -1) {
		char buf[32];
		snprintf (buf, sizeof (buf), "%d", _vstfx->current_program);
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
LXVSTPlugin::set_state (const XMLNode& node, int version)
{
	LocaleGuard lg (X_("POSIX"));

	if (node.name() != state_node_name()) {
		error << _("Bad node sent to VSTPlugin::set_state") << endmsg;
		return 0;
	}

	const XMLProperty* prop;

	if ((prop = node.property ("current-program")) != 0) {
		_vstfx->want_program = atoi (prop->value().c_str());
	}

	XMLNode* child;
	int ret = -1;

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
			float val;

			sscanf ((*i)->name().c_str(), "param-%d", &param); //This was param_%d (from vst_plugin) which caused all sorts of odd behaviour
			sscanf ((*i)->value().c_str(), "%f", &val);

			_plugin->setParameter (_plugin, param, val);
		}

		/* program number is not knowable */

		_vstfx->current_program = -1;

		ret = 0;
	}

	Plugin::set_state (node, version);
	return ret;
}

int 
LXVSTPlugin::get_parameter_descriptor (uint32_t which, ParameterDescriptor& desc) const
{
	desc.min_unbound = false;
	desc.max_unbound = false;

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
	
	return 0;
}

bool 
LXVSTPlugin::load_preset (PresetRecord r)
{
	bool s;

	if (r.user) {
		s = load_user_preset (r);
	}
	else {
		s = load_plugin_preset (r);
	}

	if (s) {
		Plugin::load_preset (r);
	}

	return s;
}

bool 
LXVSTPlugin::load_plugin_preset (PresetRecord r)
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
	
	_vstfx->want_program = index;
	return true;
}

bool 
LXVSTPlugin::load_user_preset (PresetRecord r)
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
		XMLProperty* label = (*i)->property (X_("label"));

		assert (label);

		if (label->value() != r.label) {
			continue;
		}

		if (_plugin->flags & 32 /* effFlagsProgramsChunks */) {

			/* Load a user preset chunk from our XML file and send it via a circuitous route to the plugin */

			if (_vstfx->wanted_chunk) {
				g_free (_vstfx->wanted_chunk);
			}

			for (XMLNodeList::const_iterator j = (*i)->children().begin(); j != (*i)->children().end(); ++j) {
				if ((*j)->is_content ()) {
					/* we can't dispatch directly here; too many plugins expect only one GUI thread */
					gsize size = 0;
					guchar* raw_data = g_base64_decode ((*j)->content().c_str(), &size);
					_vstfx->wanted_chunk = raw_data;
					_vstfx->wanted_chunk_size = size;
					_vstfx->want_chunk = 1;
					return true;
				}
			}

			return false;

		}
		else {
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
LXVSTPlugin::do_save_preset (string name)
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

	}
	else {

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
LXVSTPlugin::do_remove_preset (string name)
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
LXVSTPlugin::describe_parameter (Evoral::Parameter param)
{
	char name[64] = "Unkown";
	_plugin->dispatcher (_plugin, effGetParamName, param.id(), 0, name, 0);
	return name;
}

framecnt_t 
LXVSTPlugin::signal_latency () const
{
	if (_user_latency) {
		return _user_latency;
	}

	return *((int32_t *) (((char *) &_plugin->flags) + 12)); /* initialDelay */
}

set<Evoral::Parameter> 
LXVSTPlugin::automatable () const
{
	set<Evoral::Parameter> ret;

	for (uint32_t i = 0; i < parameter_count(); ++i) {
		ret.insert (ret.end(), Evoral::Parameter(PluginAutomation, 0, i));
	}

	return ret;
}

int
LXVSTPlugin::connect_and_run (BufferSet& bufs,
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
LXVSTPlugin::deactivate ()
{
	_plugin->dispatcher (_plugin, effMainsChanged, 0, 0, NULL, 0.0f);
}

void 
LXVSTPlugin::activate ()
{
	_plugin->dispatcher (_plugin, effMainsChanged, 0, 1, NULL, 0.0f);
}

string 
LXVSTPlugin::unique_id() const
{
	char buf[32];
	

	snprintf (buf, sizeof (buf), "%d", _plugin->uniqueID);

	
	return string (buf);
}


const char * 
LXVSTPlugin::name () const
{
	return handle->name;
}

const char * 
LXVSTPlugin::maker () const
{
	return _info->creator.c_str();
}

const char * 
LXVSTPlugin::label () const
{
	return handle->name;
}

uint32_t 
LXVSTPlugin::parameter_count() const
{
	return _plugin->numParams;
}

bool 
LXVSTPlugin::has_editor () const
{
	return _plugin->flags & effFlagsHasEditor;
}

void 
LXVSTPlugin::print_parameter (uint32_t param, char *buf, uint32_t /*len*/) const
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
LXVSTPluginInfo::load (Session& session)
{
	try {
		PluginPtr plugin;

		if (Config->get_use_lxvst()) {
			VSTFXHandle* handle;

			handle = vstfx_load(path.c_str());

			if (handle == NULL) {
				error << string_compose(_("LXVST: cannot load module from \"%1\""), path) << endmsg;
			}
			else {
				plugin.reset (new LXVSTPlugin (session.engine(), session, handle));
			}
		}
		else {
			error << _("You asked ardour to not use any LXVST plugins") << endmsg;
			return PluginPtr ((Plugin*) 0);
		}

		plugin->set_info(PluginInfoPtr(new LXVSTPluginInfo(*this)));
		return plugin;
	}

	catch (failed_constructor &err) {
		return PluginPtr ((Plugin*) 0);
	}
}

void 
LXVSTPlugin::find_presets ()
{
	/* Built-in presets */

	int const vst_version = _plugin->dispatcher (_plugin, effGetVstVersion, 0, 0, NULL, 0);

	for (int i = 0; i < _plugin->numPrograms; ++i) {
		PresetRecord r (string_compose (X_("LXVST:%1:%2"), unique_id (), i), "", false);

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
LXVSTPlugin::presets_tree () const
{
	XMLTree* t = new XMLTree;

	sys::path p = ARDOUR::user_config_directory ();
	p /= "presets";

	if (!is_directory (p)) {
		create_directory (p);
	}

	p /= presets_file ();

	if (!exists (p)) {
		t->set_root (new XMLNode (X_("LXVSTPresets")));
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
LXVSTPlugin::first_user_preset_index () const
{
	return _plugin->numPrograms;
}

string LXVSTPlugin::presets_file () const
{
	return string_compose ("lxvst-%1", unique_id ());
}

LXVSTPluginInfo::LXVSTPluginInfo()
{
       type = ARDOUR::LXVST;
}

