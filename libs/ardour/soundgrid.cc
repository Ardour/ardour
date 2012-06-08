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

#include <glibmm/miscutils.h>

#include <dlfcn.h>
#include <iostream>

#include <WavesPublicAPI/WCMixerCore_API.h>

#include "pbd/compose.h"

#include "ardour/debug.h"
#include "ardour/soundgrid.h"

#ifdef __APPLE__
const char* sndgrid_dll_name = "mixerapplicationcoresg.dylib";
#else
const char* sndgrid_dll_name = "mixerapplicationcoresg.so";
#endif

using namespace ARDOUR;
using namespace PBD;
using std::vector;
using std::string;
using std::cerr;
using std::endl;

SoundGrid* SoundGrid::_instance = 0;

SoundGrid::SoundGrid ()
	: dl_handle (0)
        , _sg (0)
        , _host_handle (0)
        , _callback_table (0)
        , _mixer_config (0)
{
        const char *s;
        string path;

        s =  getenv ("SOUNDGRID_PATH");
        
        /* Load from some defined location */
        
        if (!s) {
                cerr << "SOUNDGRID_PATH not defined - exiting\n";
                ::exit (1);
        }
        
        vector<string> p;
        p.push_back (s);
        p.push_back (sndgrid_dll_name);
        
        path = Glib::build_filename (p);
        
        DEBUG_TRACE (DEBUG::SoundGrid, string_compose ("Loading dylib %1\n", path));

	if ((dl_handle = dlopen (path.c_str(), RTLD_NOW)) == 0) {
                DEBUG_TRACE (DEBUG::SoundGrid, "\tfailed\n");
		return;
	}
}

SoundGrid::~SoundGrid()
{
        if (_sg) {
                UnInitializeMixerCoreDLL (_sg);
        }

	if (dl_handle) {
		dlclose (dl_handle);
	}
}

int
SoundGrid::initialize (void* window_handle)
{
        WTErr ret;
        DEBUG_TRACE (DEBUG::SoundGrid, "Initializing SG core...\n");
        ret = InitializeMixerCoreDLL (window_handle, sg_callback, &_sg);
        DEBUG_TRACE (DEBUG::SoundGrid, string_compose ("Initialized SG core, ret = %1 core handle %2\n", ret, _sg));
        return 0;
}

int
SoundGrid::teardown ()
{
        WTErr retval = eNoErr;

        if (_sg) {
                retval = UnInitializeMixerCoreDLL (_sg);
                _sg = 0;
	}

        return retval == eNoErr ? 0 : -1;
}

SoundGrid&
SoundGrid::instance ()
{
	if (!_instance) {
		_instance = new SoundGrid;
	}

	return *_instance;
}

bool
SoundGrid::available ()
{
	return instance().dl_handle != 0;
}

vector<string>
SoundGrid::lan_port_names ()
{
	std::vector<string> names;
	names.push_back ("00:00:00:1e:af - builtin ethernet controller");
	return names;
}

string
SoundGrid::coreaudio_device_name () 
{
	return "com_waves_WCAudioGridEngine:0";
}

void
SoundGrid::update_inventory (Inventory& inventory)
{
	clear_inventory (inventory);

	IOInventoryItem* ii = new (IOInventoryItem);

	ii->assign = 1;
	ii->device = "IO: Waves Virtual IO";
	ii->channels = 8;
	ii->name = "Waves Virtual IO-1";
	ii->mac = "00:16:cb:8a:e8:3e";
	ii->status = "N/A";

	inventory.push_back (ii);

	ii = new IOInventoryItem;
	ii->assign = 1;
	ii->device = "IO: Yamaha Y16";
	ii->channels = 32;
	ii->name = "Yamaha/Waves Y16";
	ii->mac = "00:16:cb:8a:e8:3e";
	ii->status = "OK";

	inventory.push_back (ii);

	SGSInventoryItem* is = new (SGSInventoryItem);
	
	is->assign = 1;
	is->name = "Waves Impact Server";
	is->mac = "00:00:fe:ed:fa:ce";
	is->channels = 16;

	inventory.push_back (is);
}

void
SoundGrid::clear_inventory (Inventory& inventory)
{
	for (Inventory::iterator i = inventory.begin(); i != inventory.end(); ++i) {
		delete *i;
	}
	inventory.clear();
}

vector<uint32_t>
SoundGrid::possible_network_buffer_sizes ()
{
	vector<uint32_t> v;
	v.push_back (80);
	v.push_back (160);
	v.push_back (256);
	v.push_back (512);
	v.push_back (992);

	return v;
}

uint32_t
SoundGrid::current_network_buffer_size ()
{
	return 256;
}

/* callback */
WTErr 
SoundGrid::_sg_callback (const WSControlID* cid)
{
        return SoundGrid::instance().sg_callback (cid);
}

WTErr
SoundGrid::sg_callback (const WSControlID* cid)
{
        cerr << "SG Callback, cluster " << cid->clusterID.clusterType << " (index " 
             << cid->clusterID.clusterTypeIndex
             << ") control " << cid->clusterControlID.controlType
             << " (index " << cid->clusterControlID.controlTypeIndex << ')'
             << endl;
        return eNoErr;
}

void
SoundGrid::driver_register (const WSDCoreHandle ch, const WSCoreCallbackTable* ct, const WSMixerConfig* mc)
{
        if (_instance) {
                _instance->_host_handle = ch;
                _instance->_callback_table = ct;
                _instance->_mixer_config = mc;
        }
}
