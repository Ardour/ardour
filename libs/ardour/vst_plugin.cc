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
#include <ctype.h>

#include <cstdlib>
#include <cstdio> // so libraptor doesn't complain
#include <cmath>
#include <dirent.h>
#include <string.h> // for memmove
#include <sys/stat.h>
#include <cerrno>

#include <lrdf.h>
#include <fst.h>

#include <pbd/compose.h>
#include <pbd/error.h>
#include <pbd/pathscanner.h>
#include <pbd/xml++.h>

#include <vst/aeffectx.h>

#include <ardour/ardour.h>
#include <ardour/session.h>
#include <ardour/audioengine.h>
#include <ardour/vst_plugin.h>

#include <pbd/stl_delete.h>

#include "i18n.h"
#include <locale.h>

using namespace ARDOUR;
using namespace PBD;
using std::min;
using std::max;

VSTPlugin::VSTPlugin (AudioEngine& e, Session& session, FSTHandle* h)
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
	
	Plugin::setup_controls ();
}

VSTPlugin::VSTPlugin (const VSTPlugin &other)
	: Plugin (other)
{
	handle = other.handle;

	if ((_fst = fst_instantiate (handle, Session::vst_callback, this)) == 0) {
		throw failed_constructor();
	}
	_plugin = _fst->plugin;

	Plugin::setup_controls ();
}

VSTPlugin::~VSTPlugin ()
{
	deactivate ();
	GoingAway (); /* EMIT SIGNAL */
	fst_close (_fst);
}

void
VSTPlugin::set_block_size (nframes_t nframes)
{
	deactivate ();
	_plugin->dispatcher (_plugin, effSetBlockSize, 0, nframes, NULL, 0.0f);
	activate ();
}

float
VSTPlugin::default_value (uint32_t port)
{
	return 0;
}	

void
VSTPlugin::set_parameter (uint32_t which, float val)
{
	_plugin->setParameter (_plugin, which, val);
	ParameterChanged (which, val); /* EMIT SIGNAL */
}

float
VSTPlugin::get_parameter (uint32_t which) const
{
	return _plugin->getParameter (_plugin, which);
	
}

uint32_t
VSTPlugin::nth_parameter (uint32_t n, bool& ok) const
{
	ok = true;
	return n;
}

XMLNode&
VSTPlugin::get_state()
{
	XMLNode *root = new XMLNode (state_node_name());
	LocaleGuard lg (X_("POSIX"));
	
	if (_plugin->flags & effFlagsProgramChunks) {

		/* fetch the current chunk */
		
		void* data;
		long  data_size;
		
		if ((data_size = _plugin->dispatcher (_plugin, effGetChunk, 0, 0, &data, false)) == 0) {
			return *root;
		}

		/* save it to a file */

		string path;
		struct stat sbuf;

		path = getenv ("HOME");
		path += "/.ardour/vst";

		if (stat (path.c_str(), &sbuf)) {
			if (errno == ENOENT) {
				if (g_mkdir_with_parents (path.c_str(), 0600)) {
					error << string_compose (_("cannot create VST chunk directory: %1"),
								 strerror (errno))
					      << endmsg;
					return *root;
				}

			} else {

				warning << string_compose (_("cannot check VST chunk directory: %1"), strerror (errno))
					<< endmsg;
				return *root;
			}

		} else if (!S_ISDIR (sbuf.st_mode)) {
			error << string_compose (_("%1 exists but is not a directory"), path)
			      << endmsg;
			return *root;
		}
		
		path += "something";
		
		/* store information */

		XMLNode* chunk_node = new XMLNode (X_("chunk"));
		chunk_node->add_property ("path", path);
		
		root->add_child_nocopy (*chunk_node);
		
	} else {

		XMLNode* parameters = new XMLNode ("parameters");

		for (long n = 0; n < _plugin->numParams; ++n) {
			char index[64];
			char val[32];
			snprintf (index, sizeof (index), "param_%ld", n);
			snprintf (val, sizeof (val), "%.12g", _plugin->getParameter (_plugin, n));
			parameters->add_property (index, val);
		}

		root->add_child_nocopy (*parameters);
	}

	return *root;
}

int
VSTPlugin::set_state(const XMLNode& node)
{
	LocaleGuard lg (X_("POSIX"));

	if (node.name() != state_node_name()) {
		error << _("Bad node sent to VSTPlugin::set_state") << endmsg;
		return 0;
	}

	XMLNode* child;

	if ((child = find_named_node (node, X_("chunks"))) != 0) {

		return 0;

	} else if ((child = find_named_node (node, X_("parameters"))) != 0) {
		
		XMLPropertyList::const_iterator i;

		for (i = child->properties().begin(); i != child->properties().end(); ++i) {
			long param;
			float val;

			sscanf ((*i)->name().c_str(), "param_%ld", &param);
			sscanf ((*i)->value().c_str(), "%f", &val);

			_plugin->setParameter (_plugin, param, val);
		}

		return 0;
	}

	return -1;
}

int
VSTPlugin::get_parameter_descriptor (uint32_t which, ParameterDescriptor& desc) const
{
	VstParameterProperties prop;

	desc.min_unbound = false;
	desc.max_unbound = false;

	if (_plugin->dispatcher (_plugin, effGetParameterProperties, which, 0, &prop, 0)) {

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
VSTPlugin::load_preset (string name)
{
	if (_plugin->flags & effFlagsProgramChunks) {
		error << _("no support for presets using chunks at this time")
		      << endmsg;
		return false;
	}
	return Plugin::load_preset (name);
}

bool
VSTPlugin::save_preset (string name)
{
	if (_plugin->flags & effFlagsProgramChunks) {
		error << _("no support for presets using chunks at this time")
		      << endmsg;
		return false;
	}
	return Plugin::save_preset (name, "vst");
}

string
VSTPlugin::describe_parameter (uint32_t param)
{
	char name[64];
	_plugin->dispatcher (_plugin, effGetParamName, param, 0, name, 0);
	return name;
}

nframes_t
VSTPlugin::latency () const
{
	return _plugin->initialDelay;
}

set<uint32_t>
VSTPlugin::automatable () const
{
	set<uint32_t> ret;

	for (uint32_t i = 0; i < parameter_count(); ++i){
		ret.insert (ret.end(), i);
	}

	return ret;
}

int
VSTPlugin::connect_and_run (vector<Sample*>& bufs, uint32_t maxbuf, int32_t& in_index, int32_t& out_index, nframes_t nframes, nframes_t offset)
{
	float *ins[_plugin->numInputs];
	float *outs[_plugin->numOutputs];
	int32_t i;

	for (i = 0; i < (int32_t) _plugin->numInputs; ++i) {
		ins[i] = bufs[min((uint32_t) in_index,maxbuf - 1)] + offset;
		in_index++;
	}

	for (i = 0; i < (int32_t) _plugin->numOutputs; ++i) {
		outs[i] = bufs[min((uint32_t) out_index,maxbuf - 1)] + offset;

		/* unbelievably, several VST plugins still rely on Cubase
		   behaviour and do not silence the buffer in processReplacing 
		   when they have no output.
		*/
		
		// memset (outs[i], 0, sizeof (Sample) * nframes);
		out_index++;
	}


	/* we already know it can support processReplacing */

	_plugin->processReplacing (_plugin, ins, outs, nframes);
	
	return 0;
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

uint32_t 
VSTPlugin::unique_id() const
{
	return _plugin->uniqueID;
}


const char *
VSTPlugin::name () const
{
	return handle->name;
}

const char *
VSTPlugin::maker () const
{
	return "imadeit";
}

const char *
VSTPlugin::label () const
{
	return handle->name;
}

uint32_t
VSTPlugin::parameter_count() const
{
	return _plugin->numParams;
}

bool
VSTPlugin::has_editor () const
{
	return _plugin->flags & effFlagsHasEditor;
}

void
VSTPlugin::print_parameter (uint32_t param, char *buf, uint32_t len) const
{
	char lab[9];
	char *first_nonws;

	_plugin->dispatcher (_plugin, effGetParamLabel, param, 0, lab, 0);
	_plugin->dispatcher (_plugin, effGetParamDisplay, param, 0, buf, 0);

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
VSTPluginInfo::load (Session& session)
{
	try {
		PluginPtr plugin;

		if (Config->get_use_vst()) {
			FSTHandle* handle;
			
			handle = fst_load(path.c_str());
	
			if ( (int)handle == -1) {
				error << string_compose(_("VST: cannot load module from \"%1\""), path) << endmsg;
			} else {
				plugin.reset (new VSTPlugin (session.engine(), session, handle));
			}
		} else {
			error << _("You asked ardour to not use any VST plugins") << endmsg;
			return PluginPtr ((Plugin*) 0);
		}

		plugin->set_info(PluginInfoPtr(new VSTPluginInfo(*this)));
		return plugin;
	}	

	catch (failed_constructor &err) {
		return PluginPtr ((Plugin*) 0);
	}
}
