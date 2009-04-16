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

#include <glibmm/ustring.h>
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
#include "ardour/vst_plugin.h"
#include "ardour/buffer_set.h"
#include "ardour/audio_buffer.h"

#include "pbd/stl_delete.h"

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
	
	// Plugin::setup_controls ();
}

VSTPlugin::VSTPlugin (const VSTPlugin &other)
	: Plugin (other)
{
	handle = other.handle;

	if ((_fst = fst_instantiate (handle, Session::vst_callback, this)) == 0) {
		throw failed_constructor();
	}
	_plugin = _fst->plugin;
	
	// Plugin::setup_controls ();
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
	//ParameterChanged (which, val); /* EMIT SIGNAL */
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

	if (_fst->current_program != -1) {
		char buf[32];
		snprintf (buf, sizeof (buf), "%d", _fst->current_program);
		root->add_property ("current-program", buf);
	}

	if (_plugin->flags & 32 /* effFlagsProgramsChunks */) {

		/* fetch the current chunk */
		
		guchar* data;
		long  data_size;
		
		if ((data_size = _plugin->dispatcher (_plugin, 23 /* effGetChunk */, 0, 0, &data, false)) == 0) {
			return *root;
		}

		/* store information */

		XMLNode* chunk_node = new XMLNode (X_("chunk"));

		gchar * encoded_data = g_base64_encode (data, data_size);
		chunk_node->add_content (encoded_data);
		g_free (encoded_data);

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

	const XMLProperty* prop;

	if ((prop = node.property ("current-program")) != 0) {
		_fst->current_program = atoi (prop->value());
	}

	XMLNode* child;
	int ret = -1;
	
	if ((child = find_named_node (node, X_("chunk"))) != 0) {

		XMLPropertyList::const_iterator i;
		XMLNodeList::const_iterator n;
		int ret = -1;

		for (n = child->children ().begin (); n != child->children ().end (); ++n) {
			if ((*n)->is_content ()) {
				gsize chunk_size = 0;
				guchar * data = g_base64_decode ((*n)->content ().c_str (), &chunk_size);
				//cerr << "Dispatch setChunk for " << name() << endl;
				ret = _plugin->dispatcher (_plugin, 24 /* effSetChunk */, 0, chunk_size, data, 0);
				g_free (data);
			}
		}

	} else if ((child = find_named_node (node, X_("parameters"))) != 0) {
		
		XMLPropertyList::const_iterator i;

		for (i = child->properties().begin(); i != child->properties().end(); ++i) {
			long param;
			float val;

			sscanf ((*i)->name().c_str(), "param_%ld", &param);
			sscanf ((*i)->value().c_str(), "%f", &val);

			_plugin->setParameter (_plugin, param, val);
		}

		/* program number is not knowable */

		_fst->current_program = -1;

		ret = 0;

	}

	return ret;
}

int
VSTPlugin::get_parameter_descriptor (uint32_t which, ParameterDescriptor& desc) const
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
VSTPlugin::load_preset (string name)
{
	if (_plugin->flags & 32 /* effFlagsProgramsChunks */) {
		error << _("no support for presets using chunks at this time")
		      << endmsg;
		return false;
	}
	return Plugin::load_preset (name);
}

bool
VSTPlugin::save_preset (string name)
{
	if (_plugin->flags & 32 /* effFlagsProgramsChunks */) {
		error << _("no support for presets using chunks at this time")
		      << endmsg;
		return false;
	}
	return Plugin::save_preset (name, "vst");
}

string
VSTPlugin::describe_parameter (Evoral::Parameter param)
{
	char name[64];
	_plugin->dispatcher (_plugin, effGetParamName, param.id(), 0, name, 0);
	return name;
}

nframes_t
VSTPlugin::signal_latency () const
{
	if (_user_latency) {
		return _user_latency;
	}

#ifdef VESTIGE_HEADER
        return *((nframes_t *) (((char *) &_plugin->flags) + 12)); /* initialDelay */
#else
	return _plugin->initial_delay;
#endif
}

set<Evoral::Parameter>
VSTPlugin::automatable () const
{
	set<Evoral::Parameter> ret;

	for (uint32_t i = 0; i < parameter_count(); ++i){
		ret.insert (ret.end(), Evoral::Parameter(PluginAutomation, 0, i));
	}

	return ret;
}

int
VSTPlugin::connect_and_run (BufferSet& bufs, uint32_t& in_index, uint32_t& out_index, nframes_t nframes, nframes_t offset)
{
	float *ins[_plugin->numInputs];
	float *outs[_plugin->numOutputs];
	int32_t i;

	const uint32_t nbufs = bufs.count().n_audio();

	for (i = 0; i < (int32_t) _plugin->numInputs; ++i) {
		ins[i] = bufs.get_audio(min((uint32_t) in_index, nbufs - 1)).data() + offset;
		in_index++;
	}

	for (i = 0; i < (int32_t) _plugin->numOutputs; ++i) {
		outs[i] = bufs.get_audio(min((uint32_t) out_index, nbufs - 1)).data() + offset;

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

string
VSTPlugin::unique_id() const
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
