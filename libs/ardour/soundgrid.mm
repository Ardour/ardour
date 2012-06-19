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
#include "pbd/stacktrace.h"

#include "ardour/debug.h"
#include "ardour/soundgrid.h"

#include "i18n.h"

#ifdef __APPLE__

#include <Foundation/Foundation.h>

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
PBD::Signal0<void> SoundGrid::Shutdown;

int SoundGrid::EventCompletionClosure::id_counter = 0;

SoundGrid::SoundGrid ()
	: dl_handle (0)
        , _sg (0)
        , _host_handle (0)
        , _pool (0)
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

#ifdef __APPLE__
        if (_pool) {
                NSAutoreleasePool* p = (NSAutoreleasePool*) _pool;
                [p release];
                _pool = 0;
        }
#endif
}

void
SoundGrid::set_pool (void* pool)
{
        instance()._pool = pool;
}

int
SoundGrid::initialize (void* window_handle)
{
        if (!_sg) {
                WTErr ret;
                DEBUG_TRACE (DEBUG::SoundGrid, "Initializing SG core...\n");
                if ((ret = InitializeMixerCoreDLL (window_handle, sg_callback, &_sg)) != eNoErr) {
                        DEBUG_TRACE (DEBUG::SoundGrid, string_compose ("Initialized SG core, ret = %1 core handle %2\n", ret, _sg));
                        return -1;
                }
                DEBUG_TRACE (DEBUG::SoundGrid, string_compose ("Initialized SG core, core handle %2\n", _sg));
        } else {
                DEBUG_TRACE (DEBUG::SoundGrid, "SG core already initialized...\n");
        }

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

        Shutdown(); /* EMIT SIGNAL */

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

int
SoundGrid::get (WSControlID* id, WSControlInfo* info)
{
        if (!_host_handle) {
                return -1;
        }

        if (_callback_table.getControlInfoProc (_host_handle, this, id, info) != eNoErr) {
                return -1;
        }

        return 0;
}

void
SoundGrid::event_completed (int)
{
        cerr << "CURRENT LAN PORT: " << current_lan_port_name() << endl;
}

void
SoundGrid::finalize (void* ecc, int state)
{
        instance()._finalize (ecc, state);
}

void
SoundGrid::_finalize (void* p, int state)
{
        EventCompletionClosure* ecc = (EventCompletionClosure*) p;
        cerr << ecc->id << ": " << ecc->name << " finished, state = " << state << endl;
        ecc->func (state);
        delete ecc;
}

int
SoundGrid::set (WSEvent* ev, const std::string& what)
{
        if (!_host_handle) {
                return -1;
        }

        ev->sourceSurface = (WSDSurfaceHandle) this;
        ev->eventTicket = new EventCompletionClosure (what, boost::bind (&SoundGrid::event_completed, this, _1));

        if (_callback_table.setEventProc (_host_handle, this, ev) != eNoErr) {
                return -1;
        }

        return 0;
}

#undef str
#undef xstr
#define xstr(s) str(s)
#define str(s) #s
#define __SG_WHERE __FILE__ ":" xstr(__LINE__)

int
SoundGrid::set_parameters (const std::string& device, int sr, int bufsize)
{
        WSAudioParamsEvent ev;
        Init_WSAudioParamsEvent (&ev);
        strncpy (ev.m_audioParams.deviceName, device.c_str(), sizeof (ev.m_audioParams.deviceName));
        ev.m_audioParams.samplingRate = sr;
        ev.m_audioParams.bufferSizeSampleFrames = bufsize;
        
        return instance().set ((WSEvent*) &ev, __SG_WHERE);
}

vector<string>
SoundGrid::lan_port_names ()
{
        WTErr eRetVal;
	std::vector<string> names;

        WSAudioDevicesControlInfo audioDevices;
        Init_WSAudioDevicesControlInfo(&audioDevices);
        eRetVal = instance().get (&audioDevices.m_controlInfo.m_controlID, (WSControlInfo*)&audioDevices);

        for (uint32_t n = 0; n < audioDevices.m_audioDevices.numberOfDevices; ++n) {
        	names.push_back (audioDevices.m_audioDevices.deviceNames[n]);
        }

	return names;
}

std::string
SoundGrid::current_lan_port_name()
{
        WTErr eRetVal;
	std::vector<string> names;

        WSAudioDevicesControlInfo audioDevices;
        Init_WSAudioDevicesControlInfo(&audioDevices);
        eRetVal = instance().get (&audioDevices.m_controlInfo.m_controlID, (WSControlInfo*)&audioDevices);

        if (eRetVal != eNoErr || audioDevices.m_audioDevices.currentDeviceIndex >= audioDevices.m_audioDevices.numberOfDevices) {
                return string();
        }

        return audioDevices.m_audioDevices.deviceNames[audioDevices.m_audioDevices.currentDeviceIndex];
}

string
SoundGrid::coreaudio_device_name () 
{
        /* this may be subject to change, but for now its the one that the SG CoreAudio driver ends up
           with and thus the one we need to tell JACK to use in order to use SoundGrid.
        */
	return "com_waves_WCAudioGridEngine:0";
}

void
SoundGrid::update_inventory (Inventory& inventory)
{
        clear_inventory (inventory);
        
        WSSGDevices currentSGDevices;
        Init_WSSGDevices(&currentSGDevices);
        WTErr eRetVal;

        eRetVal = instance().get (&currentSGDevices.m_controlInfo.m_controlID, (WSControlInfo*)&currentSGDevices);

        if (eRetVal != eNoErr) {
                error << string_compose (_("SoundGrid: could not retrieve inventory (%1)"), eRetVal) << endmsg;
                return;
        }

        DEBUG_TRACE (PBD::DEBUG::SoundGrid, string_compose ("inventory contains %1 IO boxes and %2 servers\n",
                                                            currentSGDevices.m_IOBoxs.m_numberOfDevices,
                                                            currentSGDevices.m_SGServers.m_numberOfDevices));

        for (uint32_t n = 0; n < currentSGDevices.m_IOBoxs.m_numberOfDevices; ++n) {
                IOInventoryItem* ii = new (IOInventoryItem);
                WSIOBoxDevice* dev = &currentSGDevices.m_IOBoxs.m_ioBoxDevices[n];

                ii->assign = dev->assignIndex;
                ii->device = dev->type;
                ii->channels = dev->channelCount;
                ii->channels = dev->outputChannelCount;
                ii->sample_rate = dev->sampleRate;
                ii->name = dev->name;
                ii->hostname = dev->hostname;
                ii->mac = dev->mac;
                if (dev->isIncompatible) {
                        ii->status = _("Incompatible");
                } else if (dev->isActive) {
                        ii->status = _("Active");
                } else {
                        ii->status = _("Inactive");
                }
                
                inventory.push_back (ii);
        }

        for (uint32_t n = 0; n < currentSGDevices.m_SGServers.m_numberOfDevices; ++n) {
                SGSInventoryItem* is = new (SGSInventoryItem);
                WSSGSDevice* dev = &currentSGDevices.m_SGServers.m_sgsDevices[n];

                is->assign = dev->assignIndex;
                is->device = dev->type;
                is->channels = dev->channelCount;
                is->name = dev->name;
                is->hostname = dev->hostname;
                is->mac = dev->mac;
                if (dev->isIncompatible) {
                        is->status = _("Incompatible");
                } else if (dev->isActive) {
                        is->status = _("Active");
                } else {
                        is->status = _("Inactive");
                }
                
                inventory.push_back (is);
        }
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
                _instance->_callback_table = *ct;
                _instance->_mixer_config = *mc;
        }
}
