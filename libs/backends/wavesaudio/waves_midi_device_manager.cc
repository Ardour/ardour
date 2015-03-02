/*
    Copyright (C) 2013 Waves Audio Ltd.

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

#include "waves_midi_device_manager.h"
#include "waves_audiobackend.h"

#ifdef PLATFORM_WINDOWS

#include "windows.h"
#include "mmsystem.h"

#elif defined(__APPLE__)

#include <CoreMIDI/MIDIServices.h>

#define midiInGetNumDevs MIDIGetNumberOfSources
#define midiOutGetNumDevs MIDIGetNumberOfDestinations

#endif

using namespace ARDOUR;

WavesMidiDeviceManager::WavesMidiDeviceManager (WavesAudioBackend& audiobackend)
    : _active (false)
    , _streaming (false)
    , _input_device_count (0)
    , _output_device_count (0)
    , _audiobackend (audiobackend)
{
}


WavesMidiDeviceManager::~WavesMidiDeviceManager ()
{
}


int
WavesMidiDeviceManager::start ()
{
    // COMMENTED DBG LOGS */ std::cout << "WavesMidiDeviceManager::stream ():" << std::endl;
    if ( _active == true ) {
        return -1;
    }

    if (Pm_Initialize () != pmNoError) {
        return -1;
    }

    _create_devices ();

    _input_device_count = midiInGetNumDevs ();
    _output_device_count = midiOutGetNumDevs ();

    _active = true;

    return 0;
}


int
WavesMidiDeviceManager::stream (bool yn)
{
    // COMMENTED DBG LOGS */ std::cout << "WavesMidiDeviceManager::stream ():" << std::endl;
    if (!_active) {
        std::cerr << "WavesMidiDeviceManager::stream (): the midi device manager is not started up !" << std::endl;
        return -1;
    }

    if (_streaming == yn) {
        return 0;
    }

    if (yn)    {
        if ( Pt_Start (1, __portmidi_callback, this) != ptNoError) {
            std::cerr << "WavesMidiDeviceManager::stream (): Pt_Start () failed!" << std::endl;
            return -1;
        }
    }
    else {
        if (Pt_Stop () != ptNoError) {
            std::cerr << "WavesMidiDeviceManager::stream (): Pt_Stop () failed!" << std::endl;
            return -1;
        }
    }

    _streaming = yn;
    return 0;
}


int
WavesMidiDeviceManager::stop ()
{
    // COMMENTED DBG LOGS */ std::cout << "WavesMidiDeviceManager::stop ():" << std::endl;

    if ( _active == false ) {
        return 0;
	}
    
    stream (false);

    _delete_devices ();
    _active = false;

    if (Pm_Terminate () != pmNoError) {
        std::cerr << "WavesMidiDeviceManager::stop (): Pt_Terminate () failed!" << std::endl;
        return -1;
    }

    return 0;
}

void 
WavesMidiDeviceManager::__portmidi_callback (PtTimestamp timestamp, void * userData)
{
    // COMMENTED DBG LOGS */ std::cout << "WavesMidiDeviceManager::__portmidi_callback ():" << std::endl;
    WavesMidiDeviceManager *dm = (WavesMidiDeviceManager *)userData;
    
    if (dm == NULL) {
        return;
    }
    
    dm->_portmidi_callback (timestamp);
}

void
WavesMidiDeviceManager::_portmidi_callback (PtTimestamp timestamp)
{
    if ((!_active) || (!_streaming)) {
        return;
    }

    if ((_input_device_count != midiInGetNumDevs ()) || (_output_device_count != midiOutGetNumDevs ())) {
        _audiobackend._changed_midi_devices ();
        return;
    }
}

void WavesMidiDeviceManager::do_read ()
{
    for (std::vector<WavesMidiDevice *>::const_iterator it = _devices.begin ();  it != _devices.end (); ++it) {
        (*it)->read_midi ();
    }
}


void WavesMidiDeviceManager::do_write ()
{
    for (std::vector<WavesMidiDevice *>::const_iterator it = _devices.begin ();  it != _devices.end (); ++it) {
        (*it)->write_midi ();
    }
}


PmTimestamp
WavesMidiDeviceManager::__get_time_ms (void *time_info)
{ 
    return ((WavesAudioBackend*)time_info)->sample_time ();
}


WavesMidiDevice* WavesMidiDeviceManager::_get_device (const std::string& name)
{
    for (size_t i = 0; i < _devices.size (); i++) {
        if (name == _devices[i]->name ()) {
            return _devices[i];
        }
    }
    return NULL;
}


int
WavesMidiDeviceManager::_create_devices ()
{
    int count = Pm_CountDevices ();

    for (int i = 0; i < count; i++) {

        const PmDeviceInfo* pm_device_info = Pm_GetDeviceInfo (i);

        if (pm_device_info == NULL) {
            std::cerr << "WavesMidiDeviceManager::_create_devices (): Pm_GetDeviceInfo (" << i << ") failed!" << std::endl;
            continue;
        }

        WavesMidiDevice *device = _get_device (pm_device_info->name);
        if (!device) {
            device = new WavesMidiDevice (pm_device_info->name);
            _devices.push_back (device);
			if (device->open (__get_time_ms, (void*)&_audiobackend)) {
				std::cerr << "WavesMidiDeviceManager::_create_devices (): [" << device->name () << "]->open () failed!" << std::endl;
			}
		}
    }

    return 0;
}


int
WavesMidiDeviceManager::_delete_devices ()
{
    // COMMENTED DBG LOGS */ std::cout << "WavesMidiDeviceManager::_delete_devices ():" << std::endl;
    while (!_devices.empty ()) {
        WavesMidiDevice * device = _devices.back ();
        _devices.pop_back ();
		device->close ();
        delete device;
    }
    return 0;
}

