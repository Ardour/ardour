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

#include "waves_audiobackend.h"
#include "waves_audioport.h"
#include "waves_midiport.h"
#include "waves_midi_event.h"

using namespace ARDOUR;

uint32_t
WavesAudioBackend::port_name_size () const
{
    return 256+64;
}

int
WavesAudioBackend::set_port_name (PortHandle port_handle, const std::string& port_name)
{
    // COMMENTED DBG LOGS */ std::cout  << "WavesAudioBackend::set_port_name (): [" << std::hex << port_handle << std::dec << "], [" << port_name << "]" << std::endl;
    
    if (!_registered (port_handle)) {
        std::cerr << "WavesAudioBackend::set_port_name (): Failed to find port [" << std::hex << port_handle << std::dec << "]!" << std::endl;
        return -1;
    }

    return ((WavesAudioPort*)port_handle)->set_name (__instantiated_name + ":" + port_name);
}


std::string
WavesAudioBackend::get_port_name (PortHandle port_handle) const
{
    // COMMENTED DBG LOGS */ std::cout  << "WavesAudioBackend::get_port_name (): [" << std::hex << port_handle << std::dec << "]" << std::endl;
    if (!_registered (port_handle)) {
        std::cerr << "WavesAudioBackend::get_port_name (): Failed to find port [" << std::hex << port_handle << std::dec << "]!" << std::endl;
        return std::string ();
    }
    // COMMENTED DBG LOGS */ else std::cout  << "\t[" << ((WavesAudioPort*)port_handle)->name () << "]" << std::endl;

    return ((WavesAudioPort*)port_handle)->name ();
}


PortEngine::PortHandle
WavesAudioBackend::get_port_by_name (const std::string& port_name) const
{
    // COMMENTED DBG LOGS */ std::cout  << "WavesAudioBackend::get_port_by_name (): [" << port_name << "]" << std::endl;

    PortHandle port_handle = (PortHandle)_find_port (port_name);
    if (!port_handle) {
        std::cerr << "WavesAudioBackend::get_port_by_name (): Failed to find port [" << port_name << "]!" << std::endl;
    }

    return port_handle;
}


WavesDataPort* 
WavesAudioBackend::_find_port (const std::string& port_name) const
{
    for (std::vector<WavesDataPort*>::const_iterator it = _ports.begin (); it != _ports.end (); ++it) {
        if ((*it)->name () == port_name) {
            return *it;
        }
    }

    return NULL;
}


int
WavesAudioBackend::get_ports (const std::string& port_name_pattern, DataType type, PortFlags flags, std::vector<std::string>& port_names) const
{
  
    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::get_ports (): \n\tPattern: [" << port_name_pattern << "]\n\tType: " << type << "\n\tFlags: " << flags << endl;
    
    unsigned found_ports =0;
    
    for (size_t i = 0; i < _ports.size (); ++i) {
        WavesDataPort* port = _ports[i];
        
        if ((port->type () == type) && (port->flags () & flags)) {
            port_names.push_back (port->name ());
            found_ports++;
        }
    }
    return found_ports;
}


DataType
WavesAudioBackend::port_data_type (PortHandle port_handle) const
{
    // COMMENTED DBG LOGS */ std::cout  << "WavesAudioBackend::port_data_type" << std::endl;

    if (!_registered (port_handle)) {
        std::cerr << "WavesAudioBackend::port_data_type (): Failed to find port [" << std::hex << port_handle << std::dec << "]!" << std::endl;
        return DataType::NIL;
    }
    
    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::port_data_type: " << endl;
    
    return ((WavesAudioPort*)port_handle)->type ();
}


PortEngine::PortHandle
WavesAudioBackend::register_port (const std::string& shortname, ARDOUR::DataType type, ARDOUR::PortFlags flags)
{
    // COMMENTED DBG LOGS */ std::cout  << "WavesAudioBackend::register_port (): " << type.to_string () << " [" << shortname << "]" << std::endl;

    if (shortname.size () == 0) {
        std::cerr << "WavesAudioBackend::register_port (): Invalid (empty) port name!" << std::endl;
        return NULL;
    }

    if (flags & IsPhysical) {
        std::cerr << "WavesAudioBackend::register_port (): Unexpected attribute for port [" << shortname << "]! The port must not be physical!";
        return NULL;
    }

    return (PortEngine::PortHandle)_register_port (__instantiated_name + ":" + shortname, type, flags);
}


WavesDataPort*
WavesAudioBackend::_register_port (const std::string& port_name, ARDOUR::DataType type, ARDOUR::PortFlags flags)
{
    // COMMENTED DBG LOGS */ std::cout  << "WavesAudioBackend::_register_port (): [" << port_name << "]" << std::endl;

    if (_find_port (port_name) != NULL) {
        std::cerr << "WavesAudioBackend::register_port () : Port [" << port_name << "] is already registered!" << std::endl;
        return NULL;
    }

    WavesDataPort* port = NULL;
    switch (type) {
        case ARDOUR::DataType::AUDIO: {
            WavesAudioPort* audio_port = new WavesAudioPort (port_name, flags);
            if (flags & IsPhysical)
            {
                if (flags & IsOutput)
                {
                    _physical_audio_inputs.push_back (audio_port);
                    // COMMENTED DBG LOGS */ std::cout  << "\t\t" << port_name << " added to physical AUDIO Inputs !" << std::endl;
                }
                else if (flags & IsInput)
                {
                    _physical_audio_outputs.push_back (audio_port);
                    // COMMENTED DBG LOGS */ std::cout  << "\t\t" << port_name << " added to physical AUDIO Outputs !" << std::endl;
                }
            }
            port = audio_port;
        } break;
        case ARDOUR::DataType::MIDI: {
            WavesMidiPort* midi_port = new WavesMidiPort (port_name, flags);
            if (flags & IsPhysical)
            {
                if (flags & IsOutput)
                {
                    _physical_midi_inputs.push_back (midi_port);
                    // COMMENTED DBG LOGS */ std::cout  << "\t\t" << port_name << " added to physical MIDI Inputs !" << std::endl;
                }
                else if (flags & IsInput)
                {
                    _physical_midi_outputs.push_back (midi_port);
                    // COMMENTED DBG LOGS */ std::cout  << "\t\t" << port_name << " added to physical MIDI Outputs !" << std::endl;
                }
            }
            port = midi_port;
        } break;
        default:
            std::cerr << "WavesAudioBackend::register_port () : Invalid data type (" << (uint32_t)type << ") applied to port [" << port_name << "]!" << std::endl;
        return NULL;
    }
    
    _ports.push_back (port);

    return port;
}


void
WavesAudioBackend::unregister_port (PortHandle port_handle)
{
    // COMMENTED DBG LOGS */ std::cout  << "WavesAudioBackend::unregister_port ():" << std::hex << port_handle << std::dec << std::endl;

    // so far we suppose all disconnections will be done prior to unregistering.
    WavesDataPort* port = (WavesDataPort*)port_handle;
    std::vector<WavesDataPort*>::iterator port_iterator = std::find (_ports.begin (), _ports.end (), (WavesDataPort*)port_handle);
    if (port_iterator == _ports.end ()) {
        std::cerr << "WavesAudioBackend::unregister_port (): Failed to find port [" << std::hex << port_handle << std::dec << "]!"  << std::endl;
        return;
    }
    // COMMENTED DBG LOGS */ std::cout  << "\t[" << ((WavesDataPort*)port_handle)->name () << "]" << std::endl;

    _ports.erase (port_iterator);

    if (port->is_physical ()) {
        if (port->is_output ()) {
            switch (port->type ()) {
                case ARDOUR::DataType::AUDIO: {
                    std::vector<WavesAudioPort*>::iterator audio_port_iterator = std::find (_physical_audio_inputs.begin (), _physical_audio_inputs.end (), port);
                    if (audio_port_iterator == _physical_audio_inputs.end ())    {
                        std::cerr << "WavesAudioBackend::unregister_port (): Failed to find port [" << port->name () << "] in the list of registered physical audio inputs!" << std::endl;
                        return;
                    }
                    _physical_audio_inputs.erase (audio_port_iterator);
                }
                break;
                case ARDOUR::DataType::MIDI: {
                    std::vector<WavesMidiPort*>::iterator midi_port_iterator = std::find (_physical_midi_inputs.begin (), _physical_midi_inputs.end (), port);
                    if (midi_port_iterator == _physical_midi_inputs.end ()) {
                        std::cerr << "WavesAudioBackend::unregister_port (): Failed to find port [" << port->name () << "] in the list of registered physical midi inputs!" << std::endl;
                        return;
                    }
                    _physical_midi_inputs.erase (midi_port_iterator);
                }
                break;
                default:
                    std::cerr << "WavesAudioBackend::unregister_port (): Invalid type (" << port->type () << " applied to [" << port->name () << "]!" << std::endl;
                break;
            }
        }
        else if (port->flags () & IsInput) {
            switch (port->type ()) {
                case ARDOUR::DataType::AUDIO: {
                    std::vector<WavesAudioPort*>::iterator audio_port_iterator = std::find (_physical_audio_outputs.begin (), _physical_audio_outputs.end (), port);
                    if (audio_port_iterator == _physical_audio_outputs.end ())
                    {
                        std::cerr << "WavesAudioBackend::unregister_port: Failed to find port [" << port->name () << std::dec << "] in the list of registered physical audio outputs!\n";
                        return;
                    }
                    _physical_audio_outputs.erase (audio_port_iterator);
                }
                break;
                case ARDOUR::DataType::MIDI: {

                    std::vector<WavesMidiPort*>::iterator midi_port_iterator = std::find (_physical_midi_outputs.begin (), _physical_midi_outputs.end (), port);
                    if (midi_port_iterator == _physical_midi_outputs.end ())
                    {
                        std::cerr << "WavesAudioBackend::unregister_port: Failed to find port [" << port->name () << std::dec << "] in the list of registered physical midi outputs!\n";
                        return;
                    }
                    _physical_midi_outputs.erase (midi_port_iterator);
                }
                break;
                default:
                    std::cerr << "WavesAudioBackend::unregister_port (): Invalid type (" << port->type () << " applied to [" << port->name () << "]!" << std::endl;
                break;
            }
        }
    }

    delete port;
}


int
WavesAudioBackend::connect (const std::string& src_port_name, const std::string& dst_port_name)
{
    // COMMENTED DBG LOGS */ std::cout  << "WavesAudioBackend::connect (" << src_port_name << ", " << dst_port_name << "):" << std::endl;

    WavesDataPort* src_port = _find_port (src_port_name);
    if (src_port == NULL) {
        std::cerr << "WavesAudioBackend::connect: Failed to find source port " << src_port_name << " !" << std::endl;
        return -1;
    }
    
    WavesDataPort* dst_port = _find_port (dst_port_name);
    if (dst_port == NULL) {
        std::cerr << "WavesAudioBackend::connect: Failed to find destination port " << dst_port_name << " !" << std::endl;
        return -1;
    }

    // COMMENTED DBG LOGS */ std::cout  << "\t\t (" << src_port << ", " << dst_port << "):" << std::endl;
    return src_port->connect (dst_port);
}


int
WavesAudioBackend::connect (PortHandle src_port_handle, const std::string& dst_port_name)
{
    // COMMENTED DBG LOGS */ std::cout  << "WavesAudioBackend::connect ():" << std::endl;
    if (!_registered (src_port_handle)) {
        std::cerr << "WavesAudioBackend::connect: Failed to find source port [" << std::hex << src_port_handle << std::dec << "]!" << std::endl;
        return -1;
    }

    // COMMENTED DBG LOGS */ std::cout  << "\t[" << std::hex << src_port_handle << std::dec << "]" << std::endl;
    // COMMENTED DBG LOGS */ std::cout  << "\t[" << dst_port_name << "]" << std::endl;

    WavesDataPort* dst_port = _find_port (dst_port_name);
    if (dst_port == NULL) {
        std::cerr << "WavesAudioBackend::connect (): Failed to find destination port [" << dst_port_name << "]!" << std::endl;
        return -1;
    }

    return ((WavesDataPort*)src_port_handle)->connect (dst_port);
}


int
WavesAudioBackend::disconnect (PortHandle src_port_handle, const std::string& dst_port_name)
{
    // COMMENTED DBG LOGS */ std::cout  << "WavesAudioBackend::disconnect (" << src_port_handle << ", " << dst_port_name << "):" << std::endl;
    if (!_registered (src_port_handle)) {
        std::cerr << "WavesAudioBackend::disconnect (): Failed to find source port [" << std::hex << src_port_handle << std::dec << "]!" << std::endl;
        return -1;
    }
    
    // COMMENTED DBG LOGS */ std::cout  << "\t[" << std::hex << src_port_handle << std::dec << "]" << std::endl;
    // COMMENTED DBG LOGS */ std::cout  << "\t[" << dst_port_name << "]" << std::endl;

    WavesDataPort* dst_port = _find_port (dst_port_name);
    if (dst_port == NULL) {
        std::cerr << "WavesAudioBackend::disconnect (): Failed to find destination port [" << dst_port_name << "]!" << std::endl;
        return -1;
    }

    return ((WavesDataPort*)src_port_handle)->disconnect (dst_port);
}


int
WavesAudioBackend::disconnect_all (PortHandle port_handle)
{
    // COMMENTED DBG LOGS */ std::cout  << "WavesAudioBackend::disconnect_all ():" << std::endl;
    if (!_registered (port_handle)) {
        std::cerr << "WavesAudioBackend::disconnect_all : Failed to find port [" << std::hex << port_handle << std::dec << "]!" << std::endl;
        return -1;
    }

 ((WavesDataPort*)port_handle)->disconnect_all ();

    return 0;
}


int
WavesAudioBackend::disconnect (const std::string& src_port_name, const std::string& dst_port_name)
{
    // COMMENTED DBG LOGS */ std::cout  << "WavesAudioBackend::disconnect (" << src_port_name << ", " << dst_port_name << "):" << std::endl;

    WavesDataPort* src_port = _find_port (src_port_name);
    if (src_port == NULL) {
        std::cerr << "WavesAudioBackend::disconnect : Failed to find source port!\n";
        return -1;
    }
    
    WavesDataPort* dst_port = _find_port (dst_port_name);
    if (dst_port == NULL) {
        std::cerr << "WavesAudioBackend::disconnect : Failed to find destination port!\n";
        return -1;
    }

    return dst_port->disconnect (src_port);
}


bool
WavesAudioBackend::connected (PortHandle port_handle, bool process_callback_safe)
{
    // COMMENTED DBG LOGS */ std::cout  << "WavesAudioBackend::connected ():" << std::endl;
    if (!_registered (port_handle)) {
        std::cerr << "WavesAudioBackend::connected (): Failed to find port [" << std::hex << port_handle << std::dec << "]!" << std::endl;
        return false;
    }
    
    return ((WavesDataPort*)port_handle)->is_connected ();
}


bool
WavesAudioBackend::connected_to (PortHandle src_port_handle, const std::string& dst_port_name, bool process_callback_safe)
{
    // COMMENTED DBG LOGS */ std::cout  << "WavesAudioBackend::connected_to (" << src_port_handle << ", " << dst_port_name << ")" << std::endl;

    if (!_registered (src_port_handle)) {
        std::cerr << "WavesAudioBackend::connected_to : Failed to find source port!" << std::endl;
        return false;
    }

    WavesDataPort* dst_port = _find_port (dst_port_name);
    if (dst_port == NULL) {
        std::cerr << "WavesAudioBackend::connected_to : Failed to find destination port!" << std::endl;
        return -1;
    }
    // COMMENTED DBG LOGS */ std::cout  << "\t return " << ((((WavesDataPort*)src_port_handle)->is_connected (dst_port)) ? "YES":"NO") << ", " << dst_port_name << ")" << std::endl;
    return ((WavesDataPort*)src_port_handle)->is_connected (dst_port);
}


bool
WavesAudioBackend::physically_connected (PortHandle port_handle, bool process_callback_safe)
{
    // COMMENTED DBG LOGS */ std::cout  << "WavesAudioBackend::physically_connected ():" << std::endl;

    if (!_registered (port_handle)) {
        std::cerr << "WavesAudioBackend::physically_connected (): Failed to find port [" << std::hex << port_handle << std::dec << "]!" << std::endl;
        return false;
    }

    return ((WavesDataPort*)port_handle)->is_physically_connected ();
}


int
WavesAudioBackend::get_connections (PortHandle port_handle, std::vector<std::string>& names, bool process_callback_safe)
{
    // COMMENTED DBG LOGS */ std::cout  << "WavesAudioBackend::get_connections ()" << std::endl;
    
    if (!_registered (port_handle)) {
        std::cerr << "WavesAudioBackend::get_connections (): Failed to find port [" << std::hex << port_handle << std::dec << "]!" << std::endl;
        return -1;
    }

    if (names.size ()) {
        std::cerr << "WavesAudioBackend::get_connections () : Parameter 'names' is not empty!\n";
        return -1;
    }
 
    const std::vector<WavesDataPort*>& connected_ports = ((WavesDataPort*)port_handle)->get_connections ();

    for (std::vector<WavesDataPort*>::const_iterator it = connected_ports.begin (); it != connected_ports.end (); ++it) {
        names.push_back ((*it)->name ());
    }

    return (int)names.size ();
}


int
WavesAudioBackend::request_input_monitoring (PortHandle, bool)
{
    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::request_input_monitoring: " << std::endl;
    return 0;
}


int
WavesAudioBackend::ensure_input_monitoring (PortHandle, bool)
{
    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::ensure_input_monitoring: " << std::endl;
    return 0;
}


bool
WavesAudioBackend::monitoring_input (PortHandle)
{
    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::monitoring_input: " << std::endl;
    return false;
}


bool
WavesAudioBackend::port_is_physical (PortHandle port_handle) const
{
    
    if (!_registered (port_handle)) {
        std::cerr << "WavesAudioBackend::port_is_physical (): Failed to find port [" << std::hex << port_handle << std::dec << "]!" << std::endl;
        return -1;
    }
    
    return (((WavesAudioPort*)port_handle)->flags () & IsPhysical) != 0;
}


void
WavesAudioBackend::get_physical_outputs (DataType type, std::vector<std::string>& names)
{
    // COMMENTED DBG LOGS */ std::cout  << "WavesAudioBackend::get_physical_outputs ():" << std::endl << "\tdatatype = " << type << std::endl;

    switch (type) {
        case ARDOUR::DataType::AUDIO: {
            for (std::vector<WavesAudioPort*>::iterator it = _physical_audio_outputs.begin (); it != _physical_audio_outputs.end (); ++it) {
                // COMMENTED DBG LOGS */ std::cout  << "\t" << (*it)->name () << std::endl;
                names.push_back ((*it)->name ());
            }
        } break;
        case ARDOUR::DataType::MIDI: {
            for (std::vector<WavesMidiPort*>::iterator it = _physical_midi_outputs.begin (); it != _physical_midi_outputs.end (); ++it) {
                // COMMENTED DBG LOGS */ std::cout  << "\t" << (*it)->name () << std::endl;
                names.push_back ((*it)->name ());
            }
        } break;
        default:
            break;
    }
}


void
WavesAudioBackend::get_physical_inputs (DataType type, std::vector<std::string>& names)
{
    // COMMENTED DBG LOGS */ std::cout  << "WavesAudioBackend::get_physical_inputs ():" << std::endl << "\tdatatype = " << type << std::endl;
    switch (type) {
        case ARDOUR::DataType::AUDIO: {
            for (std::vector<WavesAudioPort*>::iterator it = _physical_audio_inputs.begin (); it != _physical_audio_inputs.end (); ++it) {
                // COMMENTED DBG LOGS */ std::cout  << "\t" << (*it)->name () << std::endl;
                names.push_back ((*it)->name ());
            }
        } break;
        case ARDOUR::DataType::MIDI: {
            for (std::vector<WavesMidiPort*>::iterator it = _physical_midi_inputs.begin (); it != _physical_midi_inputs.end (); ++it) {
                // COMMENTED DBG LOGS */ std::cout  << "\t" << (*it)->name () << std::endl;
                names.push_back ((*it)->name ());
            }
        } break;
        default:
        break;
    }
}


ChanCount
WavesAudioBackend::n_physical_outputs () const
{
    ChanCount chan_count;
    chan_count.set (DataType::AUDIO, _physical_audio_outputs.size ());
    chan_count.set (DataType::MIDI, _physical_midi_outputs.size ());

    // COMMENTED DBG LOGS */ std::cout  << "WavesAudioBackend::n_physical_outputs ():" << std::endl << "\ttotal = " << chan_count.n_total () << std::endl;

    return chan_count;
}


ChanCount
WavesAudioBackend::n_physical_inputs () const
{
    ChanCount chan_count;
    chan_count.set (DataType::AUDIO, _physical_audio_inputs.size ());
    chan_count.set (DataType::MIDI, _physical_midi_inputs.size ());

    // COMMENTED DBG LOGS */ std::cout  << "WavesAudioBackend::n_physical_outputs ():" << std::endl << "\ttotal = " << chan_count.n_total () << std::endl;

    return chan_count;
}


void*
WavesAudioBackend::get_buffer (PortHandle port_handle, pframes_t nframes)
{
    // Here we would check if the port is registered. However, we will not do it as
    // it's relatively VERY SLOW operation. So let's count on consistency
    // of the caller as get_buffer normally is called hundreds of "kilotimes" per second.

    if (port_handle == NULL) {
        std::cerr << "WavesAudioBackend::get_buffer : Invalid port handler <NULL>!" << std::endl;
        return NULL;
    }  
    
    return ((WavesAudioPort*)port_handle)->get_buffer (nframes);
}


int
WavesAudioBackend::_register_system_audio_ports ()
{
    if (!_device) {
        std::cerr << "WavesAudioBackend::_register_system_audio_ports (): No device is set!" << std::endl;
        return -1;
    }
    
    std::vector<std::string> input_channels = _device->InputChannels ();
    _max_input_channels = input_channels.size ();
    
    uint32_t channels = (_input_channels ? _input_channels : input_channels.size ());
    uint32_t port_number = 0;

    LatencyRange lr = {0,0};

    // Get latency for capture
    lr.min = lr.max = _device->GetLatency (false) + _device->CurrentBufferSize () + _systemic_input_latency;
    for (std::vector<std::string>::iterator it = input_channels.begin (); 
         (port_number < channels) && (it != input_channels.end ());
        ++it) {
        std::ostringstream port_name;
        port_name << "capture_" << ++port_number;

        WavesDataPort* port = _register_port ("system:" + port_name.str (), DataType::AUDIO , static_cast<PortFlags> (IsOutput | IsPhysical | IsTerminal));
        if (port == NULL) {
            std::cerr << "WavesAudioBackend::_create_system_audio_ports (): Failed registering port [" << port_name << "] for [" << _device->DeviceName () << "]" << std::endl;
            return-1;
        }
        set_latency_range (port, false, lr);
    }
    
    std::vector<std::string> output_channels = _device->OutputChannels ();
    _max_output_channels = output_channels.size ();
    channels = (_output_channels ? _output_channels : _max_output_channels);
    port_number = 0;
    
    // Get latency for playback
    lr.min = lr.max = _device->GetLatency (true) + _device->CurrentBufferSize () + _systemic_output_latency;

    for (std::vector<std::string>::iterator it = output_channels.begin ();
         (port_number < channels) && (it != output_channels.end ());
        ++it) {
        std::ostringstream port_name;
        port_name << "playback_" << ++port_number;
        WavesDataPort* port = _register_port ("system:" + port_name.str (), DataType::AUDIO , static_cast<PortFlags> (IsInput| IsPhysical | IsTerminal));
        if (port == NULL) {
            std::cerr << "WavesAudioBackend::_create_system_audio_ports (): Failed registering port ]" << port_name << "] for [" << _device->DeviceName () << "]" << std::endl;
            return-1;
        }
        set_latency_range (port, true, lr);
    }
    
    return 0;
}


void
WavesAudioBackend::_unregister_system_audio_ports ()
{
    std::vector<WavesAudioPort*> physical_audio_ports = _physical_audio_inputs;
    physical_audio_ports.insert (physical_audio_ports.begin (), _physical_audio_outputs.begin (), _physical_audio_outputs.end ());
        
    for (std::vector<WavesAudioPort*>::const_iterator it = physical_audio_ports.begin (); it != physical_audio_ports.end (); ++it) {
        std::vector<WavesDataPort*>::iterator port_iterator = std::find (_ports.begin (), _ports.end (), *it);
        if (port_iterator == _ports.end ()) {
            std::cerr << "WavesAudioBackend::_unregister_system_audio_ports (): Failed to find port [" << (*it)->name () << "]!"  << std::endl;
        }
        else {
            _ports.erase (port_iterator);
        }
        delete *it;
    }

    _physical_audio_inputs.clear ();
    _physical_audio_outputs.clear ();
}


