/*
    Copyright (C) 2014 Waves Audio Ltd.

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

#include "waves_dataport.h"
#include "waves_audiobackend.h"

using namespace ARDOUR;


int 
WavesAudioBackend::set_systemic_input_latency (uint32_t systemic_input_latency)
{
    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::set_systemic_input_latency (): " << systemic_input_latency << std::endl;

    _systemic_input_latency = systemic_input_latency;
    return 0;
}


int 
WavesAudioBackend::set_systemic_output_latency (uint32_t systemic_output_latency)
{
    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::set_systemic_output_latency (): " << systemic_output_latency << std::endl;

    _systemic_output_latency = systemic_output_latency;
    return 0;
}

uint32_t     
WavesAudioBackend::systemic_input_latency () const
{
    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::systemic_input_latency ()" << std::endl;

    return _systemic_input_latency;
}


uint32_t     
WavesAudioBackend::systemic_output_latency () const
{
    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::systemic_output_latency ()" << std::endl;

    return _systemic_output_latency;
}


void
WavesAudioBackend::update_latencies ()
{
    // COMMENTED DBG LOGS */ std::cout << "update_latencies:" << std::endl;
}


void
WavesAudioBackend::set_latency_range (PortHandle port_handle, bool for_playback, LatencyRange latency_range)
{
    if (!_registered (port_handle)) {
        std::cerr << "WavesAudioBackend::set_latency_range (): Failed to find port [" << std::hex << port_handle << std::dec << "]!" << std::endl;
        return;
    }
    ((WavesDataPort*)port_handle)->set_latency_range (latency_range, for_playback);
}


LatencyRange
WavesAudioBackend::get_latency_range (PortHandle port_handle, bool for_playback)
{
    if (!_registered (port_handle)) {
        std::cerr << "WavesAudioBackend::get_latency_range (): Failed to find port [" << std::hex << port_handle << std::dec << "]!" << std::endl;
        LatencyRange lr = {0,0};
        return lr;
    }   
    return ((WavesDataPort*)port_handle)->latency_range (for_playback);
}
