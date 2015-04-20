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
#include <boost/assign/list_of.hpp>

#include "waves_audiobackend.h"
#include "waves_midiport.h"
#include "waves_midi_event.h"
#include "waves_midi_buffer.h"

using namespace ARDOUR;

#ifdef __APPLE__

const std::vector<std::string> WavesAudioBackend::__available_midi_options = boost::assign::list_of ("CoreMIDI") ("None");

#elif PLATFORM_WINDOWS

const std::vector<std::string> WavesAudioBackend::__available_midi_options = boost::assign::list_of ("System MIDI (MME)") ("None");

#endif


std::vector<std::string> 
WavesAudioBackend::enumerate_midi_options () const
{
    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::enumerate_midi_options ()" << std::endl;
    return __available_midi_options;
}


int 
WavesAudioBackend::set_midi_option (const std::string& option)
{
    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::set_midi_option ( " << option << " )" << std::endl;
    if (option == __available_midi_options[1]) {
        _use_midi = false;
        // COMMENTED DBG LOGS */ std::cout << "\tNO MIDI system used)" << std::endl;
    }
    else if (option == __available_midi_options[0]) {
        _use_midi = true;
        // COMMENTED DBG LOGS */ std::cout << "\tNO MIDI system used)" << std::endl;
    }
    else {
        std::cerr << "WavesAudioBackend::set_midi_option (): Invalid MIDI option!" << std::endl;
        return -1;
    }

    return 0;
}


std::string
WavesAudioBackend::midi_option () const
{
    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::midi_option ():" << std::endl;
    return * (__available_midi_options.begin () + (_use_midi?0:1));
}


int
WavesAudioBackend::midi_event_get (pframes_t& timestamp, size_t& size, uint8_t** buffer, void* port_buffer, uint32_t event_index)
{
    // COMMENTED FREQUENT DBG LOGS */ std::cout << "WavesAudioBackend::midi_event_get ():" << std::endl;

    if (buffer == NULL) {
        std::cerr << "WavesAudioBackend::midi_event_get () : NULL in the 'buffer' argument!\n";
        return -1;
    }

    if (port_buffer == NULL) {
        std::cerr << "WavesAudioBackend::midi_event_get () : NULL in the 'port_buffer' argument!\n";
        return -1;
    }

    WavesMidiBuffer& source = * (WavesMidiBuffer*)port_buffer;

    if (event_index >= source.size ()) {
        std::cerr << "WavesAudioBackend::midi_event_get () : 'event_index' is out of the number of events stored in 'port_buffer'!\n";
        return -1;
    }

    WavesMidiEvent* waves_midi_event = source[event_index];

    timestamp = waves_midi_event->timestamp ();
    size = waves_midi_event->size ();
    *buffer = waves_midi_event->data ();

    return 0;
}


int
WavesAudioBackend::midi_event_put (void* port_buffer, pframes_t timestamp, const uint8_t* buffer, size_t size)
{
    // COMMENTED FREQUENT DBG LOGS */ std::cout << "WavesAudioBackend::midi_event_put ():" << std::endl;
    if (buffer == NULL) {
        std::cerr << "WavesAudioBackend::midi_event_put () : NULL in the 'buffer' argument!\n";
        return -1;
    }

    if (port_buffer == NULL) {
        std::cerr << "WavesAudioBackend::midi_event_put () : NULL in the 'port_buffer' argument!\n";
        return -1;
    }

    WavesMidiBuffer& target = * (WavesMidiBuffer*)port_buffer;
    // COMMENTED FREQUENT DBG LOGS */ std::cout << "\t [" << target.name () << "]"<< std::endl;

    if (target.size () && (pframes_t)target.back ()->timestamp () > timestamp) {
        std::cerr << "WavesAudioBackend::midi_event_put (): The MIDI Event to put is a bit late!" << std::endl;
        std::cerr << "\tprev timestamp is " << (pframes_t)target.back ()->timestamp () << " as the current one is " << timestamp << std::endl;
        return -1;
    }

    target.push_back (new WavesMidiEvent (timestamp, buffer, size));
    return 0;
}


uint32_t
WavesAudioBackend::get_midi_event_count (void* port_buffer)
{
    // COMMENTED FREQUENT DBG LOGS */ std::cout << "WavesAudioBackend::get_midi_event_count (): " << std::endl;
    
    if (port_buffer == NULL) {
        std::cerr << "WavesAudioBackend::get_midi_event_count () : NULL in the 'port_buffer' argument!\n";
        return -1;
    }

    // COMMENTED FREQUENT DBG LOGS */ std::cout << "\tcount = " << (* (WavesMidiBuffer*)port_buffer).size () << std::endl;

    return (* (WavesMidiBuffer*)port_buffer).size ();
}


void
WavesAudioBackend::midi_clear (void* port_buffer)
{
    // COMMENTED FREQUENT DBG LOGS */ std::cout << "WavesAudioBackend::midi_clear (): " << std::endl;
    if (port_buffer == NULL) {
        std::cerr << "WavesAudioBackend::midi_clear () : NULL in the 'port_buffer' argument!\n";
        return;
    }

    (* (WavesMidiBuffer*)port_buffer).clear ();
}


void
WavesAudioBackend::_changed_midi_devices ()
{
    if (_midi_device_manager.stream (false)) {
        std::cerr << "WavesAudioBackend::_changed_midi_devices (): _midi_device_manager.stream (false) failed!" << std::endl;
        return;
    }

	_unregister_system_midi_ports ();
    _midi_device_manager.stop ();

    if (_midi_device_manager.start () != 0) {
        std::cerr << "WavesAudioBackend::_changed_midi_devices (): _midi_device_manager.start () failed!" << std::endl;
        return;
    }

    if (_register_system_midi_ports () != 0) {
        std::cerr << "WavesAudioBackend::_changed_midi_devices (): _register_system_midi_ports () failed!" << std::endl;
        return;
    }
    
    manager.registration_callback ();

    if (_midi_device_manager.stream (true)) {
        std::cerr << "WavesAudioBackend::_changed_midi_devices (): _midi_device_manager.stream (true) failed!" << std::endl;
        return;
    }
}


void
WavesAudioBackend::_unregister_system_midi_ports ()
{
    // COMMENTED DBG LOGS */ std::cout  << "WavesAudioBackend::_unregister_system_midi_ports ()" << std::endl;
    std::vector<WavesMidiPort*> physical_midi_ports = _physical_midi_inputs;
    physical_midi_ports.insert (physical_midi_ports.begin (), _physical_midi_outputs.begin (), _physical_midi_outputs.end ());

    for (std::vector<WavesMidiPort*>::const_iterator it = physical_midi_ports.begin (); it != physical_midi_ports.end (); ++it) {
        std::vector<WavesDataPort*>::iterator port_iterator = std::find (_ports.begin (), _ports.end (), *it);
        if (port_iterator == _ports.end ()) {
            std::cerr << "WavesAudioBackend::_unregister_system_midi_ports (): Failed to find port [" << (*it)->name () << "]!"  << std::endl;
        }
        else
            _ports.erase (port_iterator);
        delete *it;
    }
    _physical_midi_inputs.clear ();
    _physical_midi_outputs.clear ();
}


int
WavesAudioBackend::_register_system_midi_ports ()
{
    // COMMENTED DBG LOGS */ std::cout  << "WavesAudioBackend::_register_system_midi_ports ()" << std::endl;

    LatencyRange lr = {0,0};
    lr.min = lr.max = _buffer_size;

    for (size_t i = 0; i<_ports.size ();)    {
        WavesMidiPort* midi_port = dynamic_cast<WavesMidiPort*> (_ports[i]);
        if (!midi_port || !midi_port->is_physical () || !midi_port->is_terminal ()) {
            ++i;
            continue;
        }

        if ((midi_port->is_input () && !midi_port->midi_device ()->is_output ()) ||
            (midi_port->is_output () && !midi_port->midi_device ()->is_input ())) {
            disconnect_all (midi_port);
            unregister_port (midi_port);
            continue; // to be here for further additions in the end of this loop
        }

        ++i;
    }

    const std::vector<WavesMidiDevice *>&  devices = _midi_device_manager.devices ();

    for (std::vector<WavesMidiDevice*>::const_iterator it = devices.begin (); it != devices.end (); ++it) {
        if ((*it)->is_input ()) {
            std::string port_name = "system_midi:" + (*it)->name () + " capture";
            WavesDataPort* port = _find_port (port_name);
            WavesMidiPort* midi_port = dynamic_cast<WavesMidiPort*> (port);
            if (midi_port && (midi_port->type () != DataType::MIDI || 
                midi_port->midi_device () != *it || 
                !midi_port->is_output () || 
                !midi_port->is_physical () ||
                !midi_port->is_terminal ())) {
                std::cerr << "WavesAudioBackend::_register_system_midi_ports (): the port [" << midi_port->name () << "] is inconsystently constructed!" << std::endl;
                disconnect_all (midi_port);
                unregister_port (midi_port);
                port = NULL;
            }

            if (port == NULL) {
                port = _register_port ( port_name, DataType::MIDI , static_cast<ARDOUR::PortFlags> (IsOutput | IsPhysical | IsTerminal));
                if (port == NULL) {
                    return -1;
                }
                ((WavesMidiPort*)port)->set_midi_device (*it);
            }
            port->set_latency_range (lr, false); 
        }

        if ((*it)->is_output ()) {
            std::string port_name = "system_midi:" + (*it)->name () + " playback";
            WavesDataPort* port = _find_port (port_name);
            WavesMidiPort* midi_port = dynamic_cast<WavesMidiPort*> (port);
            if (midi_port && (midi_port->type () != DataType::MIDI || 
                midi_port->midi_device () != *it || 
                !midi_port->is_input () || 
                !midi_port->is_physical () ||
                !midi_port->is_terminal ())) {
                std::cerr << "WavesAudioBackend::_register_system_midi_ports (): the port [" << midi_port->name () << "] is inconsystently constructed!" << std::endl;
                disconnect_all (midi_port);
                unregister_port (midi_port);
            }

            if (port == NULL) {
                port = _register_port (port_name,
                                       DataType::MIDI,
                                       static_cast<ARDOUR::PortFlags> (IsInput | IsPhysical | IsTerminal));
                if (port == NULL) {
                    return -1;
                }
            }

            ((WavesMidiPort*)port)->set_midi_device ((*it));
            port->set_latency_range (lr, true);
        }
    }
    
    return 0;
}


int
WavesAudioBackend::_read_midi_data_from_devices ()
{
    // COMMENTED FREQUENT DBG LOGS */ std::cout  << "WavesAudioBackend::_read_midi_data_from_devices ():" << std::endl;
    if (!_midi_device_manager.is_streaming ())
        return 0;
    
    _midi_device_manager.do_read ();

    for (std::vector<WavesMidiPort*>::iterator it = _physical_midi_inputs.begin (); it != _physical_midi_inputs.end (); ++it) {
        WavesMidiDevice* midi_device = (*it)->midi_device ();
        
        WavesMidiBuffer& waves_midi_buffer = (*it)->buffer ();
        waves_midi_buffer.clear ();
        
        while (WavesMidiEvent *waves_midi_event = midi_device->dequeue_input_waves_midi_event ()) {
            int32_t timestamp_st = _buffer_size - (_sample_time_at_cycle_start - waves_midi_event->timestamp ());
            
            if (timestamp_st < 0) {
                timestamp_st = 0;
            } else if (timestamp_st >= (int32_t)_buffer_size) {
                timestamp_st = _buffer_size - 1;
            }
            waves_midi_event->set_timestamp (timestamp_st);
            waves_midi_buffer.push_back (waves_midi_event);
        }
    }
    return 0;
}


int
WavesAudioBackend::_write_midi_data_to_devices (pframes_t nframes)
{
    if (!_midi_device_manager.is_streaming ())
        return 0;
    
    for (std::vector<WavesMidiPort*>::iterator it = _physical_midi_outputs.begin (); it != _physical_midi_outputs.end (); ++it) {
        WavesMidiDevice* midi_device = (*it)->midi_device (); 
        WavesMidiBuffer &waves_midi_buffer = * (WavesMidiBuffer*) (*it)->get_buffer (nframes);

        for (WavesMidiBufferIterator it = waves_midi_buffer.begin (); it != waves_midi_buffer.end ();) {
             WavesMidiEvent* waves_midi_event = *it;
            
            waves_midi_buffer.erase (it);
            
            waves_midi_event->set_timestamp (_sample_time_at_cycle_start + waves_midi_event->timestamp () + nframes);
            midi_device->enqueue_output_waves_midi_event (waves_midi_event);
       }
    }
    _midi_device_manager.do_write ();
    return 0;
}
