/*
    Copyright (C) 2013 Paul Davis

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

#ifndef __libardour_port_engine_h__
#define __libardour_port_engine_h__

#include <vector>
#include <string>

#include <stdint.h>

#include "ardour/data_type.h"
#include "ardour/types.h"

namespace ARDOUR {

/** PortEngine is an abstract base class that defines the functionality
 * required by Ardour. 
 * 
 * A Port is basically an endpoint for a datastream (which can either be
 * continuous, like audio, or event-based, like MIDI). Ports have buffers
 * associated with them into which data can be written (if they are output
 * ports) and from which data can be read (if they input ports). Ports can be
 * connected together so that data written to an output port can be read from
 * an input port. These connections can be 1:1, 1:N OR N:1. 
 *
 * Ports may be associated with software only, or with hardware.  Hardware
 * related ports are often referred to as physical, and correspond to some
 * relevant physical entity on a hardware device, such as an audio jack or a
 * MIDI connector. Physical ports may be potentially asked to monitor their
 * inputs, though some implementations may not support this.
 *
 * Most physical ports will also be considered "terminal", which means that
 * data delivered there or read from there will go to or comes from a system
 * outside of the PortEngine implementation's control (e.g. the analog domain
 * for audio, or external MIDI devices for MIDI). Non-physical ports can also
 * be considered "terminal". For example, the output port of a software
 * synthesizer is a terminal port, because the data contained in its buffer
 * does not and cannot be considered to come from any other port - it is
 * synthesized by its owner.
 *
 * Ports also have latency associated with them. Each port has a playback
 * latency and a capture latency:
 *
 * <b>capture latency</b>: how long since the data read from the buffer of a
 *                  port arrived at at a terminal port.  The data will have
 *                  come from the "outside world" if the terminal port is also
 *                  physical, or will have been synthesized by the entity that
 *                  owns the terminal port.
 *                  
 * <b>playback latency</b>: how long until the data written to the buffer of
 *                   port will reach a terminal port.
 *
 *
 * For more detailed questions about the PortEngine API, consult the JACK API
 * documentation, on which this entire object is based.
 */

class PortEngine {
  public:
    PortEngine() {}
    virtual ~PortEngine();
    
    /* We use void* here so that the API can be defined for any implementation.
     * 
     * We could theoretically use a template (PortEngine<T>) and define
     * PortHandle as T, but this complicates the desired inheritance
     * pattern in which FooPortEngine handles things for the Foo API,
     * rather than being a derivative of PortEngine<Foo>.
    */
       
    typedef void* PortHandle;

    virtual bool connected() const = 0;

    virtual int         set_port_name (PortHandle, const std::string&) = 0;
    virtual std::string get_port_name (PortHandle) const = 0;
    virtual PortHandle* get_port_by_name (const std::string&) const = 0;

    virtual PortHandle register_port (const std::string&, DataType::Symbol, ARDOUR::PortFlags) = 0;
    virtual void  unregister_port (PortHandle) = 0;
    virtual bool  connected (PortHandle) = 0;
    virtual int   disconnect_all (PortHandle) = 0;
    virtual bool  connected_to (PortHandle, const std::string&) = 0;
    virtual int   get_connections (PortHandle, std::vector<std::string>&) = 0;
    virtual bool  physically_connected (PortHandle) = 0;
    virtual int   connect (PortHandle, const std::string&) = 0;
    virtual int   disconnect (PortHandle, const std::string&) = 0;
    
    /* MIDI */

    virtual void midi_event_get (pframes_t& timestamp, size_t& size, uint8_t** buf, void* port_buffer, uint32_t event_index) = 0;
    virtual int  midi_event_put (void* port_buffer, pframes_t timestamp, const uint8_t* buffer, size_t size) = 0;
    virtual uint32_t get_midi_event_count (void* port_buffer);
    virtual void midi_clear (void* port_buffer);

    /* Monitoring */

    virtual bool  can_monitor_input() const = 0;
    virtual int   request_input_monitoring (PortHandle, bool) = 0;
    virtual int   ensure_input_monitoring (PortHandle, bool) = 0;
    virtual bool  monitoring_input (PortHandle) = 0;

    /* Latency management
     */
    
    struct LatencyRange {
	uint32_t min;
	uint32_t max;
    };
    
    virtual void          set_latency_range (PortHandle, int dir, LatencyRange) = 0;
    virtual LatencyRange  get_latency_range (PortHandle, int dir) = 0;
    virtual LatencyRange  get_connected_latency_range (PortHandle, int dir) = 0;

    virtual void* get_buffer (PortHandle, pframes_t) = 0;

    virtual pframes_t last_frame_time () const = 0;
};

}

#endif /* __libardour_port_engine_h__ */
