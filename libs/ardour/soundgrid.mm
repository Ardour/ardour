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
#include <algorithm>
#include <unistd.h>
#include <cstdlib>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <sys/param.h>
#endif

#include <WavesMixerCore/API/WCMixerCore_API.h>

#include "pbd/compose.h"
#include "pbd/stacktrace.h"

#include "ardour/debug.h"
#include "ardour/filesystem_paths.h"
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
int SoundGrid::CommandStatusClosure::id_counter = 0;

SoundGrid::SoundGrid ()
	: dl_handle (0)
        , _sg (0)
        , _host_handle (0)
        , _driver_configured (false)
        , _physical_inputs (0)
        , _physical_outputs (0)
        , _max_plugins (8)
{
}

SoundGrid::~SoundGrid()
{
        if (_sg) {
                remove_all_racks ();
                UnInitializeMixerCoreDLL (_sg);
        }

	if (dl_handle) {
		dlclose (dl_handle);
	}
}

bool
SoundGrid::initialized()
{
        return _instance && _instance->_sg;
}

bool
SoundGrid::driver_configured() const
{
        return _driver_configured;
}

int
SoundGrid::initialize (void* window_handle, uint32_t max_tracks, uint32_t max_busses, 
                       uint32_t /*physical_inputs*/, uint32_t physical_outputs,
                       uint32_t max_plugins_per_rack)
{
        if (initialized()) {
                DEBUG_TRACE (DEBUG::SoundGrid, "SG core already initialized...\n");
                return 1;
        }

        WTErr ret;
        DEBUG_TRACE (DEBUG::SoundGrid, "Initializing SG core...\n");
        
        WSMixerConfig mixer_limits;
        Init_WSMixerConfig (&mixer_limits);
        
        max_tracks = 64;
        
        mixer_limits.m_clusterConfigs[eClusterType_Inputs].m_uiIndexNum = 2; // Physical IO + Device Driver
        mixer_limits.m_clusterConfigs[eClusterType_Outputs].m_uiIndexNum = 2; // Physical IO + Device Driver
        mixer_limits.m_clusterConfigs[eClusterType_InputTrack].m_uiIndexNum = max_tracks;
        mixer_limits.m_clusterConfigs[eClusterType_GroupTrack].m_uiIndexNum = max_busses + physical_outputs;
        
        DEBUG_TRACE (DEBUG::SoundGrid, string_compose ("Initialized SG Core for %1 input racks and %2 group racks\n", 
                                                       max_tracks, max_busses + physical_outputs));
        
        char execpath[MAXPATHLEN+1];
        uint32_t pathsz = sizeof (execpath);
        string driver_path;
        
        if (getenv ("ARDOUR_BUNDLED") == 0) {
#ifdef __APPLE__
                _NSGetExecutablePath (execpath, &pathsz);
#else 
                readlink ("/proc/self/exe", execpath, sizeof(execpath));
#endif        
                vector<string> s;
                s.push_back (Glib::path_get_dirname (execpath));
                s.push_back ("..");
                s.push_back ("libs");
                s.push_back ("soundgrid");
                s.push_back ("SurfaceDriver_App.bundle");
                driver_path = Glib::build_filename (s);
        } else {
                driver_path = Glib::build_filename (ardour_dll_directory(), "SurfaceDriver_App.bundle");
        }
        
        DEBUG_TRACE (DEBUG::SoundGrid, string_compose ("driver path: %1\n", driver_path));

        if ((ret = InitializeMixerCoreDLL (&mixer_limits, driver_path.c_str(), window_handle, _sg_callback, this, &_sg)) != eNoErr) {
                DEBUG_TRACE (DEBUG::SoundGrid, string_compose ("Failed to initialize SG core, ret = %1 core handle %2\n", ret, _sg));
                return -1;
        }
        
        _max_plugins = max_plugins_per_rack;
        
        DEBUG_TRACE (DEBUG::SoundGrid, string_compose ("Initialized SG core, core handle %1\n", _sg));

        return 0;
}

int
SoundGrid::teardown ()
{
        WTErr retval = eNoErr;

        if (_sg) {
                DEBUG_TRACE (DEBUG::SoundGrid, "shutting down SG core ...\n");
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
SoundGrid::configure_driver (uint32_t inputs, uint32_t outputs, uint32_t tracks) 
{
    WSConfigSGDriverCommand myCommand;

    if (_driver_configured) {
            return 0;
    }

    _physical_outputs = outputs;
    _physical_inputs = inputs;

    uint32_t in = inputs+tracks;
    uint32_t out = outputs+tracks;

    in = std::min (in, 32U);
    out = std::min (out, 32U);

    Init_WSConfigSGDriverCommand (&myCommand, in, out, (WSDControllerHandle)this, 0);

    DEBUG_TRACE (DEBUG::SoundGrid, string_compose ("Initializing SG driver to use %1 inputs + %2 outputs\n", in, out));

    if (command (&myCommand.m_command)) {
            return -1;
    }

    /* set up the in-use bool vector */

    _driver_output_ports_in_use.assign (out, false);
    _driver_input_ports_in_use.assign (in, false);

    /* reserve the actual inputs and outputs */

    for (uint32_t n = 0; n < inputs; ++n) {
            _driver_input_ports_in_use[n] = true;
    }

    for (uint32_t n = 0; n < outputs; ++n) {
            _driver_output_ports_in_use[n] = true;
    }

    /* Create mono GroupTracks for mixing inputs sent to each driver input.

       These are the ONLY chainers that talk to the physical outputs, anything
       else that wants to route to a physical output must go via the corresponding
       PseudoPhysicalOutputPort (which corresponds to one of these GroupTracks).

       As of early December 2012, mono grouptracks do not function correctly, so
       use stereo but only connect/assign their left channel.
     */

    for (uint32_t n = 0; n < outputs; ++n) {
            uint32_t handle; 
            const uint32_t channels = 2;
            const uint32_t pgroup = 7; // these should run last no matter what

            if (add_rack (eClusterType_GroupTrack, pgroup, channels, handle)) {
                    error << string_compose (_("Cannot create mixing channel for driver output %1"), n) << endmsg;
                    return -1;
            }
    }

    for (uint32_t n = 0; n < outputs; ++n) {
            uint32_t handle; 
            const uint32_t channels = 1;
            const uint32_t pgroup = 2;

            if (add_rack (eClusterType_InputTrack, pgroup, channels, handle)) {
                    error << string_compose (_("Cannot create mixing channel for driver output %1"), n) << endmsg;
                    return -1;
            }
    }

    if (outputs) {
            WSAssignmentsCommand outputsCommand;
            Init_WSAddAssignmentsCommand(&outputsCommand, outputs, (WSDControllerHandle)this, 0);
            
            for (uint32_t n = 0; n < outputs; ++n) {
                    
                    ARDOUR::SoundGrid::BusOutputPort src (n, 0); // single output of the group track
                    ARDOUR::SoundGrid::PhysicalOutputPort dst (n);  // real physical channel where the signal should go

                    WSAudioAssignment &assignment (outputsCommand.in_Assignments.m_aAssignments[n]);
                    src.set_source (assignment);
                    dst.set_destination (assignment);
            }

            int ret = command (&outputsCommand.m_command);
            
            Dispose_WSAudioAssignmentBatch (&outputsCommand.in_Assignments);
            
            if (ret != 0) {
                    return -1;
            }
    }

    /* wire up inputs and outputs */

    if (inputs) {
            
            DEBUG_TRACE (DEBUG::SoundGrid, string_compose ("setting up wiring for %1 inputs\n", inputs));

            WSAssignmentsCommand inputsCommand;
            Init_WSAddAssignmentsCommand(&inputsCommand, inputs, (WSDControllerHandle)this, 0);
            
            for (uint32_t n = 0; n < inputs; ++n) {
                    
                    ARDOUR::SoundGrid::PhysicalInputPort src (n); // physical channel where the signal should come from
                    ARDOUR::SoundGrid::DriverOutputPort dst (n);  // driver channel/JACK port where it should be readable from
                    WSAudioAssignment &assignment (inputsCommand.in_Assignments.m_aAssignments[n]);
                    
                    src.set_source (assignment);
                    dst.set_destination (assignment);
            }
            
            int ret = command (&inputsCommand.m_command);
            
            Dispose_WSAudioAssignmentBatch (&inputsCommand.in_Assignments);
            
            if (ret != 0) {
                    return -1;
            }
    }

    if (outputs) {
            int ret;

            DEBUG_TRACE (DEBUG::SoundGrid, string_compose ("setting up wiring for %1 driver playback channels, part one\n", outputs));

#if 0
            WSAssignmentsCommand outputsCommand1;
            Init_WSAddAssignmentsCommand(&outputsCommand1, outputs, (WSDControllerHandle)this, 0);
#endif

            for (uint32_t n = 0; n < outputs; ++n) {

                    ARDOUR::SoundGrid::DriverInputPort src (n);   // writable driver/JACK channel/port
                    ARDOUR::SoundGrid::TrackInputPort dst (n, 0);  // physical channel where the signal should go

#if 0
                    WSAudioAssignment& assignment (outputsCommand1.in_Assignments.m_aAssignments[n]);
                    
                    src.set_source (assignment);
                    dst.set_destination (assignment);
#endif
                    connect (src, dst);
            }
#if 0
            ret = command (&outputsCommand1.m_command);
            
            Dispose_WSAudioAssignmentBatch (&outputsCommand1.in_Assignments);
            
            if (ret != 0) {
                    return -1;
            }
#endif

            DEBUG_TRACE (DEBUG::SoundGrid, string_compose ("setting up wiring for %1 driver playback channels, part two\n", outputs));

#if 0
            WSAssignmentsCommand outputsCommand2;
            Init_WSAddAssignmentsCommand(&outputsCommand2, outputs, (WSDControllerHandle)this, 0);
#endif

            for (uint32_t n = 0; n < outputs; ++n) {

                    ARDOUR::SoundGrid::TrackOutputPort src (n, 0); // track output
                    ARDOUR::SoundGrid::PseudoPhysicalOutputPortXX dst (n); // group track that mixes for physical out N

#if 0
                    WSAudioAssignment& assignment (outputsCommand2.in_Assignments.m_aAssignments[n]);

                    src.set_source (assignment);
                    dst.set_destination (assignment);
#endif
                    connect (src, dst);
            }

#if 0
            ret = command (&outputsCommand2.m_command);
            
            Dispose_WSAudioAssignmentBatch (&outputsCommand2.in_Assignments);
            
            if (ret != 0) {
                    return -1;
            }
#endif

    }

    _driver_configured = true;
    return 0;
}

int
SoundGrid::get (WSControlID* id, WSControlInfo* info)
{
        if (!_host_handle) {
                return -1;
        }

        if (_callback_table.getControlInfoProc (_host_handle, id, info) != eNoErr) {
                return -1;
        }

        return 0;
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
        ecc->func (state);
        delete ecc;
}

int
SoundGrid::set (WSEvent* ev, const std::string& what)
{
        if (!_host_handle) {
                return -1;
        }

        ev->sourceController = (WSDControllerHandle) this;

        DEBUG_TRACE (DEBUG::SoundGrid, string_compose ("Set %1\n", what));

        if (_callback_table.setEventProc (_host_handle, this, ev) != eNoErr) {
                DEBUG_TRACE (DEBUG::SoundGrid, "Set failure\n");
                return -1;
        }

        DEBUG_TRACE (DEBUG::SoundGrid, "Set success\n");
        return 0;
}

static string
command_name (uint32_t type)
{
        switch (type) {
        case eCommandType_AddTrack:
                return "AddTrack";
        case eCommandType_RemoveTrack:
                return "RemoveTrack";
        case eCommandType_RemoveAllTracks:
                return "RemoveAllTracks";
        case eCommandType_AddAssignments:
                return "AddAssignments";
        case eCommandType_RemoveAssignments:
                return "RemoveAssignments";
        case eCommandType_ConfigSGDRiver:
                return "ConfigSGDRiver";
        default:
                break;
        }
        return "Unknown Command";
}
                        
int
SoundGrid::command (WSCommand* cmd)
{
        if (!_host_handle) {
                return -1;
        }

        DEBUG_TRACE (DEBUG::SoundGrid, string_compose ("Deliver command type %1 (\"%2\") @ %3 synchronous %4 from thread %5\n", 
                                                       cmd->in_commandType, command_name (cmd->in_commandType),
                                                       cmd, (cmd->in_commandTicket == 0), pthread_self()));

        WTErr err;
        WTErr expected_result;

        if (cmd->in_commandTicket) {
                expected_result = WMSD_Pending;
        } else {
                expected_result = eNoErr;
        }
        
        if ((err = _callback_table.sendCommandProc (_host_handle, cmd)) != expected_result) {
                DEBUG_TRACE (DEBUG::SoundGrid, string_compose ("\tfailed, code = %1 vs. expected %2\n", err, expected_result));
                return -1;
        }

        DEBUG_TRACE (DEBUG::SoundGrid, "\tsuccess\n");
        return 0;
}

void 
SoundGrid::command_status_update (WSCommand *cmd)
{
        CommandStatusClosure* ecc = (CommandStatusClosure*) cmd->in_commandTicket;

        DEBUG_TRACE (DEBUG::SoundGrid, string_compose ("Command %1: %2 finished, status = %3 progress %4 (in thread %5)\n",
                                                       ecc->id,
                                                       ecc->name,
                                                       cmd->out_status,
                                                       cmd->out_progress,
                                                       pthread_self()));

        /* Execute the closure we wanted to run when the command completed */

        ecc->func (cmd);

        delete ecc;
        delete cmd;
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

bool
SoundGrid::get_driver_config (uint32_t& max_inputs, 
                              uint32_t& max_outputs,
                              uint32_t& current_inputs,
                              uint32_t& current_outputs)
{
        WSDriverConfigParam driverConfig;
        WMSDErr errCode = _callback_table.getParamProc (_host_handle, eParamType_DriverConfig,
                                                        Init_WSDriverConfigParam (&driverConfig));
        
        if (0 == errCode) {
                max_inputs = driverConfig.out_MaxInputChannels;
                max_outputs = driverConfig.out_MaxOutputChannels;
                current_inputs = driverConfig.out_CurrentInputChannels;
                current_outputs = driverConfig.out_CurrentOutputChannels;
                return true;
        } 

        DEBUG_TRACE (DEBUG::SoundGrid, string_compose ("could not get soundgrid driver config, err = %1\n", errCode));
        return false;
}

void
SoundGrid::parameter_updated (WEParamType paramID)
{
        uint32_t maxInputs=0, maxOutputs=0, curInputs=0, curOutputs=0;

        switch (paramID) {
        case eParamType_DriverConfig:
                if (get_driver_config (maxInputs, maxOutputs, curInputs, curOutputs)) {
                        // do something to tell the GUI ?
                        DEBUG_TRACE (DEBUG::SoundGrid, string_compose ("Driver configuration changed to max: %1/%2 current: %3/%4\n",
                                                                       maxInputs, maxOutputs, curInputs, curOutputs));
                }
                break;
        default:
                break;
        }
}

vector<string>
SoundGrid::lan_port_names ()
{
        WTErr eRetVal;
	std::vector<string> names;

        WSAudioDevicesControlInfo audioDevices;
        Init_WSAudioDevicesControlInfo(&audioDevices);
        eRetVal = instance().get (&audioDevices.m_controlInfo.m_controlID, (WSControlInfo*)&audioDevices);

        cerr << "getting lan port names returned " << eRetVal << endl;

        if (eRetVal == eNoErr) {
                for (uint32_t n = 0; n < audioDevices.m_audioDevices.numberOfDevices; ++n) {
                        names.push_back (audioDevices.m_audioDevices.deviceNames[n]);
                }
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
SoundGrid::_sg_callback (void* arg, const WSControlID* cid)
{
        return ((SoundGrid*) arg)->sg_callback (cid);
}

WTErr
SoundGrid::sg_callback (const WSControlID* cid)
{
        cerr << "SG Callback, cluster " << cid->clusterID.clusterType << " (index " 
             << cid->clusterID.clusterHandle
             << ") control " << cid->sectionControlID.sectionType
             << " (index " << cid->sectionControlID.sectionIndex << ')'
             << endl;
        return eNoErr;
}

void
SoundGrid::driver_register (const WSDCoreHandle ch, const WSCoreCallbackTable* ct, const WSMixerConfig* mc)
{
        DEBUG_TRACE (DEBUG::SoundGrid, string_compose ("Register driver complete, handles are core: %1 callbacks: %2, config: %3\n",
                                                       ch, ct, mc));

        if (_instance) {
                _instance->_host_handle = ch;

                if (ct) {
                        _instance->_callback_table = *ct;
                } else {
                        memset (&_instance->_callback_table, 0, sizeof (_instance->_callback_table));
                }
                if (mc) {
                        _instance->_mixer_config = *mc;
                } else {
                        memset (&_instance->_mixer_config, 0, sizeof (_instance->_mixer_config));
                }
        }
}

void
SoundGrid::assignment_complete (WSCommand* cmd)
{
        /* callback from SoundGrid core whenever an async assignment (connection) is completed
         */

        WSAssignmentsCommand* acmd = (WSAssignmentsCommand*) cmd;
        WSAudioAssignment& assignment (acmd->in_Assignments.m_aAssignments[0]);
        WSControlID &destination (assignment.m_asgnDest.m_controlID);
        WSControlID &source (assignment.m_asgnSrc.m_controlID);

        DEBUG_TRACE (DEBUG::SoundGrid, string_compose ("assignment complete for %1:%2:%3:%4:%5:%6 => %7:%8:%9:%10:%11:%12\n",
                                                       source.clusterID.clusterType,
                                                       source.clusterID.clusterHandle,
                                                       source.sectionControlID.sectionType,
                                                       source.sectionControlID.sectionIndex,
                                                       source.sectionControlID.channelIndex,
                                                       assignment.m_asgnSrc.m_PrePost,
                                                       destination.clusterID.clusterType,
                                                       destination.clusterID.clusterHandle,
                                                       destination.sectionControlID.sectionType,
                                                       destination.sectionControlID.sectionIndex,
                                                       destination.sectionControlID.channelIndex,
                                                       assignment.m_asgnDest.m_PrePost));
}

/* Actually do stuff */

bool
SoundGrid::add_rack (uint32_t clusterType, int32_t process_group, uint32_t channels, uint32_t &trackHandle)
{
    WSAddTrackCommand myCommand;

    if (channels == 0) {
            /* we can change it later, but we can get here during initial setup when
               a route has no outputs.
            */
            channels = 1;
    }

    DEBUG_TRACE (DEBUG::SoundGrid, string_compose ("add rack sync, type %1 channels %2 pgroup %3\n",
                                                   clusterType, channels, process_group));

    command (Init_WSAddTrackCommand (&myCommand, clusterType, channels, process_group, _max_plugins, (WSDControllerHandle)this, 0));
    
    if (0 == myCommand.m_command.out_status) {
            trackHandle = myCommand.out_trackID.clusterHandle;
            return 0;
    }
    
    return -1;
}

bool
SoundGrid::remove_rack (uint32_t clusterType, uint32_t trackHandle)
{
    WSRemoveTrackCommand myCommand;
    
    command (Init_WSRemoveTrackCommand (&myCommand, clusterType, trackHandle, (WSDControllerHandle)this, 0));
    
    return (0 == myCommand.m_command.out_status);
}

bool
SoundGrid::remove_all_racks ()
{
    WSCommand myCommand;
    
    command (Init_WSCommand (&myCommand, sizeof(myCommand), eCommandType_RemoveAllTracks,
                                               (WSDControllerHandle)this, 0));
    
    return (0 == myCommand.out_status);
}

int
SoundGrid::set_gain (uint32_t clusterType, uint32_t trackHandle, double gainValue)
{
    WSEvent faderEvent;
    Init_WSEvent(&faderEvent);
    
    faderEvent.eventID = eEventID_MoveTo;
    faderEvent.controllerValue = gainValue;

    Init_WSControlID(&faderEvent.controlID);
    faderEvent.controlID.clusterID.clusterType = clusterType;
    faderEvent.controlID.clusterID.clusterHandle = trackHandle;
    
    faderEvent.controlID.sectionControlID.sectionType = eControlType_Output;
    faderEvent.controlID.sectionControlID.sectionIndex = eControlType_Output_Local;
    faderEvent.controlID.sectionControlID.channelIndex = wvEnum_Unknown;
    faderEvent.controlID.sectionControlID.controlID = eControlID_Output_Gain;
    
    return set (&faderEvent, "fader level");
}

bool
SoundGrid::get_gain (uint32_t clusterType, uint32_t trackHandle, double &out_gainValue)
{
    WSControlInfo faderInfo; 
    
    Init_WSControlInfo(&faderInfo);
    
    Init_WSControlID(&faderInfo.m_controlID);
    faderInfo.m_controlID.clusterID.clusterType = clusterType;
    faderInfo.m_controlID.clusterID.clusterHandle = trackHandle;
    
    faderInfo.m_controlID.sectionControlID.sectionType = eControlType_Output;
    faderInfo.m_controlID.sectionControlID.sectionIndex = eControlType_Output_Local;
    faderInfo.m_controlID.sectionControlID.channelIndex = wvEnum_Unknown;
    faderInfo.m_controlID.sectionControlID.controlID = eControlID_Output_Gain;
    
    if (get (&faderInfo.m_controlID, &faderInfo) == 0) {
            out_gainValue = faderInfo.m_dState;
            return true;
    }

    return false;
}

bool
SoundGrid::jack_port_as_sg_port (const string& jack_port, Port& result)
{
        JACK_SGMap::iterator x = jack_soundgrid_map.find (jack_port);

        if (x != jack_soundgrid_map.end()) {
                result = x->second;
                return true;
        }

        return false;
}

string
SoundGrid::sg_port_as_jack_port (const Port& sgport)
{
        string jack_port;
        SG_JACKMap::iterator x = soundgrid_jack_map.find (sgport);
        
        if (x != soundgrid_jack_map.end()) {
                DEBUG_TRACE (DEBUG::SoundGrid, string_compose ("sgport %1 found in SG/Jack map as %2\n", sgport, x->second));
                return x->second;
        }
        
        /* OK, so this SG port has never been connected (and thus mapped) to
           a JACK port. We need to find a free driver channel and connect it
           to the specified sgport.
        */
        
        DEBUG_TRACE (DEBUG::SoundGrid, string_compose ("Try to map SG port %1 as JACK port\n", sgport));
        
        uint32_t driver_channel;
        bool found = false;
        bool inputs = false;
        
        if (sgport.accepts_input()) {
                
                inputs = true;
                
                uint32_t n = _driver_output_ports_in_use.size () - 1;
                
                /* Find next free driver channel to use to represent the given SG port.
                   
                   Note that we search backwards. This doesn't eliminate the issues that
                   will arise if we end up overlapping with the ports used for normal
                   physical output, but makes it less likely.
                */
                
                for (vector<bool>::reverse_iterator x = _driver_output_ports_in_use.rbegin(); x != _driver_output_ports_in_use.rend(); ++x) {
                        if (!(*x)) {
                                
                                DEBUG_TRACE (DEBUG::SoundGrid, string_compose ("Found unused driver output channel %1 to map %2\n", n, sgport));
                                driver_channel = n;
                            found = true;
                            break;
                    }
                    --n;
            }

    } else {

            uint32_t n = _driver_input_ports_in_use.size () - 1;

            /* Find next free driver channel to use to represent the given SG port.
               
               Note that we search backwards. This doesn't eliminate the issues that
               will arise if we end up overlapping with the ports used for normal
               physical output, but makes it less likely.
            */
            
            for (vector<bool>::reverse_iterator x = _driver_input_ports_in_use.rbegin(); x != _driver_input_ports_in_use.rend(); ++x) {
                    if (!(*x)) {
                            
                            DEBUG_TRACE (DEBUG::SoundGrid, string_compose ("Found unused driver input channel %1 to map %2\n", n, sgport));
                            driver_channel = n;
                            found = true;
                            break;
                    }
                    --n;
            }
    }
    
    if (found) {
            if (inputs) {
                    Port driverport = DriverInputPort (driver_channel);
                    if (connect (driverport, sgport) != 0) {
                            return string();
                    }
            } else {
                    Port driverport = DriverOutputPort (driver_channel);
                    if (connect (sgport, driverport) != 0) {
                            return string();
                    }
            }

            /* do record keeping */
            
            if (inputs) {
                    _driver_output_ports_in_use[driver_channel] = true;
                    jack_port = string_compose ("system:playback_%1", driver_channel+1);
            } else {
                    _driver_input_ports_in_use[driver_channel] = true;
                    jack_port = string_compose ("system:capture_%1", driver_channel+1);
            }

            jack_soundgrid_map.insert (make_pair (jack_port, sgport));
            soundgrid_jack_map.insert (make_pair (sgport, jack_port));
    
            
            DEBUG_TRACE (DEBUG::SoundGrid, string_compose ("map SG port %1 as %2\n", sgport, jack_port));

    } else {
            DEBUG_TRACE (DEBUG::SoundGrid, "no spare driver channels/JACK ports were found to use for routing to SG\n");
    }

    
    return jack_port;
}

void
SoundGrid::drop_sg_jack_mapping (const string& jack_port)
{
        jack_soundgrid_map.erase (jack_port);

        for (SG_JACKMap::iterator i = soundgrid_jack_map.begin(); i != soundgrid_jack_map.end(); ++i) {
                if (i->second == jack_port) {
                        soundgrid_jack_map.erase (i);
                }
        }
}

int
SoundGrid::connect (const Port& src, const Port& dst)
{
        WSAssignmentsCommand myCommand;

        Init_WSAddAssignmentsCommand(&myCommand, 1, (WSDControllerHandle)this, 0);
        WSAudioAssignment &assignment (myCommand.in_Assignments.m_aAssignments[0]);

        src.set_source (assignment);
        dst.set_destination (assignment);

        DEBUG_TRACE (DEBUG::SoundGrid, string_compose ("connect %1 => %2\n", src, dst));
        
        int ret = command (&myCommand.m_command);

        Dispose_WSAudioAssignmentBatch (&myCommand.in_Assignments);

        return ret;
}

int
SoundGrid::disconnect (const Port& src, const Port& dst)
{
        WSAssignmentsCommand myCommand;

        Init_WSRemoveAssignmentsCommand(&myCommand, 1, (WSDControllerHandle)this, 0);
        WSAudioAssignment &assignment (myCommand.in_Assignments.m_aAssignments[0]);

        src.set_source (assignment);
        dst.set_destination (assignment);

        DEBUG_TRACE (DEBUG::SoundGrid, string_compose ("DIS-connect %1 ... %2\n", src, dst));

        int ret = command (&myCommand.m_command);

        Dispose_WSAudioAssignmentBatch (&myCommand.in_Assignments);

        return ret;
}

void
SoundGrid::Port::set_source (WSAudioAssignment& assignment) const
{
        WSControlID &c (assignment.m_asgnSrc.m_controlID);

        c.clusterID.clusterType = ctype;
        c.clusterID.clusterHandle = cid;
        c.sectionControlID.sectionType = stype;
        c.sectionControlID.sectionIndex = sindex;
        c.sectionControlID.controlID = sid;
        c.sectionControlID.channelIndex = channel;

        assignment.m_asgnSrc.m_PrePost = sg_source();
}

void
SoundGrid::Port::set_destination (WSAudioAssignment& assignment) const       
{
        WSControlID &c (assignment.m_asgnDest.m_controlID);

        c.clusterID.clusterType = ctype;
        c.clusterID.clusterHandle = cid;
        c.sectionControlID.sectionType = stype;
        c.sectionControlID.sectionIndex = sindex;
        c.sectionControlID.controlID = sid;
        c.sectionControlID.channelIndex = channel;

        assignment.m_asgnDest.m_PrePost = sg_source();
}

int
SoundGrid::configure_io (uint32_t cluster_type, uint32_t rack_id, uint32_t channels)
{
    WSEvent configEvent;
    Init_WSEvent(&configEvent);

    /* XXX not sure how this works for multichannel as of oct 30th 2012 */

    configEvent.eventID = eEventID_MoveTo;
    configEvent.controllerValue = (channels == 2) ? 1 : 0;
    
    Init_WSControlID(&configEvent.controlID);
    configEvent.controlID.clusterID.clusterType  = cluster_type;
    configEvent.controlID.clusterID.clusterHandle  = rack_id;

    configEvent.controlID.sectionControlID.sectionType = eControlType_Input;
    configEvent.controlID.sectionControlID.sectionIndex = eControlType_Input_Local;
    configEvent.controlID.sectionControlID.channelIndex = wvEnum_Unknown;
    configEvent.controlID.sectionControlID.controlID = eControlID_Input_MonoStereo;

    if (set (&configEvent, string_compose ("I/O configure ctype %1 id %2 to have %3 channels", 
                                           cluster_type, rack_id, channels))) {
            return -1;
    }
    
    return 0;
}

std::ostream&
operator<< (std::ostream& out, const SoundGrid::Port& p)
{
        switch (p.ctype) {
        case eClusterType_Inputs:
                out << "inputs";
                switch (p.cid) {
                case eClusterHandle_Physical_Driver:
                        out << ":driver";
                        break;
                case eClusterHandle_Physical_IO:
                        out << ":IO";
                        break;
                default:
                        out << ":unknown";
                }
                break;
        case eClusterType_Outputs:
                out << "outputs";
                switch (p.cid) {
                case eClusterHandle_Physical_Driver:
                        out << ":driver";
                        break;
                case eClusterHandle_Physical_IO:
                        out << ":IO";
                        break;
                default:
                        out << ":unknown";
                }
                break;
        case eClusterType_InputTrack:
                out << "InputTrack";
                break;
        case eClusterType_GroupTrack:
                out << "GroupTrack";
                break;
        case eClusterType_AuxTrack:
                out << "AuxTrack";
                break;
        case eClusterType_MatrixTrack:
                out << "MatrixTrack";
                break;
        case eClusterType_LCRMTrack:
                out << "LCRMTrack";
                break;
        case eClusterType_DCATrack:
                out << "DCATrack";
                break;
        case eClusterType_CueTrack:
                out << "CueTrack";
                break;
        case eClusterType_TBTrack:
                out << "TBTrack";
                break;
        default:
                out << "unknown";
                break;
        }
         
        if (p.ctype != eClusterType_Inputs && p.ctype != eClusterType_Outputs) {

                out << ":ID " << p.cid << ':';
                
                switch (p.stype) {

                case eControlType_Input:
                        out << "input " << p.sindex << ':';
                        switch (p.sid) {
                        case eControlID_Input_Assignment_Left:
                                out << "assignment_left";
                                break;
                        case eControlID_Input_Assignment_Right:
                                out << "assignment_right";
                                break;
                        case eControlID_Input_MonoStereo:
                                out << "monostereo";
                                break;
                        case eControlID_Input_Choose_Left_Right:
                                out << "choose_left_right";
                                break;
                        case eControlID_Input_Choose_Link_UnLink:
                                out << "choose_link_unlink";
                                break;
                        case eControlID_Input_Phase_On_Off:
                                out << "phase_on_off";
                                break;
                        case eControlID_Input_Pad_On_Off:
                                out << "pad_on_off";
                                break;
                        case eControlID_Input_48V_On_Off:
                                out << "48v_on_off";
                                break;
                        case eControlID_Input_Digital_Trim:
                                out << "digital_trim";
                                break;
                        case eControlID_Input_Digital_Delay:
                                out << "digital_delay";
                                break;
                        case eControlID_Input_Direct:
                                out << "direct";
                                break;
                        case eControlID_Input_Choose_In_Pre_Post:
                                out << "choose_in_pre_post";
                                break;
                        case eControlID_Input_VU_Left:
                                out << "vu_left";
                                break;
                        case eControlID_Input_VU_Right:
                                out << "vu_right";
                                break;
                        default:
                                out << "unknown";
                        }
                        break;
                case eControlType_Output:
                        out << "output " << p.sindex << ':';
                        switch (p.sid) {
                        case eControlID_Output_Gain:
                                out << "gain";
                                break;
                        case eControlID_Output_Pan:
                                out << "pan";
                                break;
                        case eControlID_Output_Arm_On_Off:
                                out << "arm_on_off";
                                break;
                        case eControlID_Output_Assign_On_Off:
                                out << "assign_on_off";
                                break;
                        case eControlID_Output_Select_On_Off:
                                out << "select_on_off";
                                break;
                        case eControlID_Output_Cue_On_Off:
                                out << "cue_on_off";
                                break;
                        case eControlID_Output_Mute_On_Off:
                                out << "mute_on_off";
                                break;
                        case eControlID_Output_VU_Left:
                                out << "vu_left";
                                break;
                        case eControlID_Output_VU_Right:
                                out << "vu_right";
                                break;
                        case eControlID_Output_MonoStereoPreFader:
                                out << "monostereoprefader";
                                break;
                        case eControlID_Output_MonoStereoPostFader:
                                out << "monostereopostfader";
                                break;
                        case eControlID_Output_MonoStereoPostPan:
                                out << "monostereopostpan";
                                break;
                        case eControlID_Output_DirectMixMtxSrc:
                                out << "directmixmtxsrc";
                                break;
                        case eControlID_Output_DirectLeft:
                                out << "directleft";
                                break;
                        case eControlID_Output_DirectRight:
                                out << "directright";
                                break;
                        default:
                                out << "unknown";
                                break;
                        }
                        break;
                case eControlType_Filter:
                        out << "filter";
                        break;
                case eControlType_Dynamics:
                        out << "dynamics";
                        break;
                case eControlType_EQ:
                        out << "eq";
                        break;
                case eControlType_UserPlugins:
                        out << "userplugins";
                        break;
                case eControlType_AuxSend:
                        out << "auxsend";
                        break;
                case eControlType_AuxReturn:
                        out << "auxreturn";
                        break;
                case eControlType_Name:
                        out << "name";
                        break;
                case eControlType_Display:
                        out << "display";
                        break;
                }
        }

        out << ":chn " << p.channel << ':';

        switch (p.position) {
        case SoundGrid::Port::Post:
                out << "post";
                break;
        case SoundGrid::Port::Pre:
                out << "pre";
                break;
        }

        return out;
}

