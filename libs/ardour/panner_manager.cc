/*
    Copyright (C) 2011 Paul Davis

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

#include <unistd.h>

#include <glibmm/pattern.h>
#include <glibmm/fileutils.h>

#include "pbd/error.h"
#include "pbd/compose.h"
#include "pbd/pathscanner.h"
#include "pbd/stl_delete.h"

#include "ardour/debug.h"
#include "ardour/panner_manager.h"

#include "ardour/panner_search_path.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

PannerManager* PannerManager::_instance = 0;

PannerManager::PannerManager ()
{
}

PannerManager::~PannerManager ()
{
	for (list<PannerInfo*>::iterator p = panner_info.begin(); p != panner_info.end(); ++p) {
		delete *p;
	}
}

PannerManager&
PannerManager::instance ()
{
	if (_instance == 0) {
		_instance = new PannerManager ();
	}

	return *_instance;
}

static bool panner_filter (const string& str, void */*arg*/)
{
#ifdef __APPLE__
	return str[0] != '.' && (str.length() > 6 && str.find (".dylib") == (str.length() - 6));
#else
	return str[0] != '.' && (str.length() > 3 && (str.find (".so") == (str.length() - 3) || str.find (".dll") == (str.length() - 4)));
#endif
}

void
PannerManager::discover_panners ()
{
	PathScanner scanner;
	std::vector<std::string *> *panner_modules;
	std::string search_path = panner_search_path().to_string();

	DEBUG_TRACE (DEBUG::Panning, string_compose (_("looking for panners in %1\n"), search_path));

	panner_modules = scanner (search_path, panner_filter, 0, false, true, 1, true);

	for (vector<std::string *>::iterator i = panner_modules->begin(); i != panner_modules->end(); ++i) {
		panner_discover (**i);
	}

	vector_delete (panner_modules);
}

int
PannerManager::panner_discover (string path)
{
	PannerInfo* pinfo;

	if ((pinfo = get_descriptor (path)) != 0) {

		list<PannerInfo*>::iterator i;

		for (i = panner_info.begin(); i != panner_info.end(); ++i) {
			if (pinfo->descriptor.name == (*i)->descriptor.name) {
				break;
			}
		}

		if (i == panner_info.end()) {
			panner_info.push_back (pinfo);
			DEBUG_TRACE (DEBUG::Panning, string_compose(_("Panner discovered: \"%1\" in %2\n"), pinfo->descriptor.name, path));
		}
	}

	return 0;
}

PannerInfo*
PannerManager::get_descriptor (string path)
{
	Glib::Module* module = new Glib::Module(path);
	PannerInfo* info = 0;
	PanPluginDescriptor *descriptor = 0;
	PanPluginDescriptor* (*dfunc)(void);
	void* func = 0;

	if (!module) {
		error << string_compose(_("PannerManager: cannot load module \"%1\" (%2)"), path,
				Glib::Module::get_last_error()) << endmsg;
		delete module;
		return 0;
	}

	if (!module->get_symbol("panner_descriptor", func)) {
		error << string_compose(_("PannerManager: module \"%1\" has no descriptor function."), path) << endmsg;
		error << Glib::Module::get_last_error() << endmsg;
		delete module;
		return 0;
	}

	dfunc = (PanPluginDescriptor* (*)(void))func;
	descriptor = dfunc();

	if (descriptor) {
		info = new PannerInfo (*descriptor, module);
	} else {
		delete module;
	}

	return info;
}

PannerInfo*
PannerManager::select_panner (ChanCount in, ChanCount out, std::string const uri)
{
	PannerInfo* rv = NULL;
	PanPluginDescriptor* d;
	int32_t nin = in.n_audio();
	int32_t nout = out.n_audio();
	uint32_t priority = 0;

	/* look for user-preference -- check if channels match */
	for (list<PannerInfo*>::iterator p = panner_info.begin(); p != panner_info.end(); ++p) {
		d = &(*p)->descriptor;
		if (d->panner_uri != uri) continue;
		if (d->in != nin && d->in != -1) continue;
		if (d->out != nout && d->out != -1) continue;
		return *p;
	}

	/* look for exact match first */

	for (list<PannerInfo*>::iterator p = panner_info.begin(); p != panner_info.end(); ++p) {
		d = &(*p)->descriptor;

		if (d->in == nin && d->out == nout && d->priority > priority) {
			priority = d->priority;
			rv = *p;
		}
	}
	if (rv) { return rv; }

	/* no exact match, look for good fit on inputs and variable on outputs */

	priority = 0;
	for (list<PannerInfo*>::iterator p = panner_info.begin(); p != panner_info.end(); ++p) {
		d = &(*p)->descriptor;

		if (d->in == nin && d->out == -1 && d->priority > priority) {
			priority = d->priority;
			rv = *p;
		}
	}
	if (rv) { return rv; }

	/* no exact match, look for good fit on outputs and variable on inputs */

	priority = 0;
	for (list<PannerInfo*>::iterator p = panner_info.begin(); p != panner_info.end(); ++p) {
		d = &(*p)->descriptor;

		if (d->in == -1 && d->out == nout && d->priority > priority) {
			priority = d->priority;
			rv = *p;
		}
	}
	if (rv) { return rv; }

	/* no exact match, look for variable fit on inputs and outputs */

	priority = 0;
	for (list<PannerInfo*>::iterator p = panner_info.begin(); p != panner_info.end(); ++p) {
		d = &(*p)->descriptor;

		if (d->in == -1 && d->out == -1 && d->priority > priority) {
			priority = d->priority;
			rv = *p;
		}
	}
	if (rv) { return rv; }

	warning << string_compose (_("no panner discovered for in/out = %1/%2"), nin, nout) << endmsg;

	return 0;
}

PannerInfo*
PannerManager::get_by_uri (std::string uri) const
{
	PannerInfo* pi = NULL;
	for (list<PannerInfo*>::const_iterator p = panner_info.begin(); p != panner_info.end(); ++p) {
		if ((*p)->descriptor.panner_uri != uri) continue;
		pi = (*p);
		break;
	}
	return pi;
}

PannerUriMap
PannerManager::get_available_panners(uint32_t const a_in, uint32_t const a_out) const
{
	int const in = a_in;
	int const out = a_out;
	PannerUriMap panner_list;

	if (out < 2 || in == 0) {
		return panner_list;
	}

	/* get available panners for current configuration. */
	for (list<PannerInfo*>::const_iterator p = panner_info.begin(); p != panner_info.end(); ++p) {
		 PanPluginDescriptor* d = &(*p)->descriptor;
		 if (d->in != -1 && d->in != in) continue;
		 if (d->out != -1 && d->out != out) continue;
		 if (d->in == -1 && d->out == -1 && out <= 2) continue;
		 panner_list.insert(std::pair<std::string,std::string>(d->panner_uri,d->name));
	}
	return panner_list;
}
