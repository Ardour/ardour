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

class PortManager;

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
    PortEngine (PortManager& pm) : manager (pm) {}
    virtual ~PortEngine() {}
    
    /** Return a private, type-free pointer to any data
     * that might be useful to a concrete implementation
     */
    virtual void* private_handle() const = 0;

    /* We use void* here so that the API can be defined for any implementation.
     * 
     * We could theoretically use a template (PortEngine<T>) and define
     * PortHandle as T, but this complicates the desired inheritance
     * pattern in which FooPortEngine handles things for the Foo API,
     * rather than being a derivative of PortEngine<Foo>.
    */
       
    typedef void* PortHandle;

    /** Return the name of this process as used by the port manager
     * when naming ports.
     */
    virtual const std::string& my_name() const = 0;
 
    /** Return true if the underlying mechanism/API is still available
     * for us to utilize. return false if some or all of the AudioBackend
     * API can no longer be effectively used.
     */
    virtual bool available() const = 0;

    /** Return the maximum size of a port name 
     */
    virtual uint32_t port_name_size() const = 0;

    /** Returns zero if the port referred to by @param port was set to @param
     * name. Return non-zero otherwise.
     */
    virtual int         set_port_name (PortHandle port, const std::string& name) = 0;
    /** Return the name of the port referred to by @param port. If the port
     * does not exist, return an empty string.
     */
    virtual std::string get_port_name (PortHandle) const = 0;
    /** Return a reference to a port with the fullname @param name. Return
     * a null pointer if no such port exists.
     */
    virtual PortHandle* get_port_by_name (const std::string&) const = 0;

    /** Find the set of ports whose names, types and flags match
     * specified values, place the names of each port into @param ports,
     * and return the count of the number found.
     *
     * To avoid selecting by name, pass an empty string for @param
     * port_name_pattern.
     * 
     * To avoid selecting by type, pass DataType::NIL as @param type.
     * 
     * To avoid selecting by flags, pass PortFlags (0) as @param flags.
     */
    virtual int get_ports (const std::string& port_name_pattern, DataType type, PortFlags flags, std::vector<std::string>& ports) const = 0;

    /** Return the Ardour data type handled by the port referred to by @param
     * port. Returns DataType::NIL if the port does not exist.
     */
    virtual DataType port_data_type (PortHandle port) const = 0;

    /** Create a new port whose fullname will be the conjuction of my_name(),
     * ":" and @param shortname. The port will handle data specified by @param
     * type and will have the flags given by @param flags. If successfull,
     * return a reference to the port, otherwise return a null pointer.
    */
    virtual PortHandle register_port (const std::string& shortname, ARDOUR::DataType type, ARDOUR::PortFlags flags) = 0;

    /* Destroy the port referred to by @param port, including all resources
     * associated with it. This will also disconnect @param port from any ports it
     * is connected to.
     */
    virtual void       unregister_port (PortHandle) = 0;
    
    /* Connection management */

    /** Ensure that data written to the port named by @param src will be
     * readable from the port named by @param dst. Return zero on success,
     * non-zero otherwise.
    */
    virtual int   connect (const std::string& src, const std::string& dst) = 0;

    /** Remove any existing connection between the ports named by @param src and 
     * @param dst. Return zero on success, non-zero otherwise.
     */
    virtual int   disconnect (const std::string& src, const std::string& dst) = 0;
    
    
    /** Ensure that data written to the port referenced by @param portwill be
     * readable from the port named by @param dst. Return zero on success,
     * non-zero otherwise.
    */
    virtual int   connect (PortHandle src, const std::string& dst) = 0;
    /** Remove any existing connection between the port referenced by @param src and 
     * the port named @param dst. Return zero on success, non-zero otherwise.
     */ 
    virtual int   disconnect (PortHandle src, const std::string& dst) = 0;

    /** Remove all connections between the port referred to by @param port and
     * any other ports. Return zero on success, non-zero otherwise.
     */
    virtual int   disconnect_all (PortHandle port) = 0;

    /** Return true if the port referred to by @param port has any connections
     * to other ports. Return false otherwise.
     */
    virtual bool  connected (PortHandle port, bool process_callback_safe = true) = 0;
    /** Return true if the port referred to by @param port is connected to
     * the port named by @param name. Return false otherwise.
     */
    virtual bool  connected_to (PortHandle, const std::string& name, bool process_callback_safe = true) = 0;

    /** Return true if the port referred to by @param port has any connections
     * to ports marked with the PortFlag IsPhysical. Return false otherwise.
     */
    virtual bool  physically_connected (PortHandle port, bool process_callback_safe = true) = 0;

    /** Place the names of all ports connected to the port named by @param
     * ports into @param names, and return the number of connections.
     */
    virtual int   get_connections (PortHandle port, std::vector<std::string>& names, bool process_callback_safe = true) = 0;

    /* MIDI */

    /** Retrieve a MIDI event from the data at @param port_buffer. The event
    number to be retrieved is given by @param event_index (a value of zero
    indicates that the first event in the port_buffer should be retrieved).
    * 
    * The data associated with the event will be copied into the buffer at
    * @param buf and the number of bytes written will be stored in @param
    * size. The timestamp of the event (which is always relative to the start 
    * of the current process cycle, in samples) will be stored in @param
    * timestamp
    */
    virtual int      midi_event_get (pframes_t& timestamp, size_t& size, uint8_t** buf, void* port_buffer, uint32_t event_index) = 0;

    /** Place a MIDI event consisting of @param size bytes copied from the data
     * at @param buf into the port buffer referred to by @param
     * port_buffer. The MIDI event will be marked with a time given by @param
     * timestamp. Return zero on success, non-zero otherwise.
     *
     * Events  must be added monotonically to a port buffer. An attempt to 
     * add a non-monotonic event (e.g. out-of-order) will cause this method
     * to return a failure status.
     */
    virtual int      midi_event_put (void* port_buffer, pframes_t timestamp, const uint8_t* buffer, size_t size) = 0;

    /** Return the number of MIDI events in the data at @param port_buffer
     */
    virtual uint32_t get_midi_event_count (void* port_buffer) = 0;

    /** Clear the buffer at @param port_buffer of all MIDI events.
     *
     * After a call to this method, an immediate, subsequent call to
     * get_midi_event_count() with the same @param port_buffer argument must
     * return zero.
    */
    virtual void     midi_clear (void* port_buffer) = 0;

    /* Monitoring */

    /** Return true if the implementation can offer input monitoring.
     *
     * Input monitoring involves the (selective) routing of incoming data
     * to an outgoing data stream, without the data being passed to the CPU.
     *
     * Only certain audio hardware can provide this, and only certain audio
     * APIs can offer it.
     */
    virtual bool  can_monitor_input() const = 0;
    /** Increment or decrement the number of requests to monitor the input 
     * of the hardware channel represented by the port referred to by @param
     * port.
     *
     * If the number of requests rises above zero, input monitoring will
     * be enabled (if can_monitor_input() returns true for the implementation).
     * 
     * If the number of requests falls to zero, input monitoring will be
     * disabled (if can_monitor_input() returns true for the implementation)
     */
    virtual int   request_input_monitoring (PortHandle port, bool yn) = 0;
    /* Force input monitoring of the hardware channel represented by the port
     * referred to by @param port to be on or off, depending on the true/false
     * status of @param yn. The request count is ignored when using this
     * method, so if this is called with yn set to false, input monitoring will
     * be disabled regardless of the number of requests to enable it.
    */
    virtual int   ensure_input_monitoring (PortHandle port, bool yn) = 0;
    /** Return true if input monitoring is enabled for the hardware channel
     * represented by the port referred to by @param port. Return false
     * otherwise.
     */
    virtual bool  monitoring_input (PortHandle port) = 0;

    /* Latency management
     */
    
    /** Set the latency range for the port referred to by @param port to @param
     * r. The playback range will be set if @param for_playback is true,
     * otherwise the capture range will be set.
     */
    virtual void          set_latency_range (PortHandle port, bool for_playback, LatencyRange r) = 0;
    /** Return the latency range for the port referred to by @param port.
     * The playback range will be returned if @param for_playback is true,
     * otherwise the capture range will be returned.
     */
    virtual LatencyRange  get_latency_range (PortHandle port, bool for_playback) = 0;

    /* Discovering physical ports */

    /** Return true if the port referred to by @param port has the IsPhysical
     * flag set. Return false otherwise.
     */
    virtual bool      port_is_physical (PortHandle port) const = 0;

    /** Store into @param names the names of all ports with the IsOutput and
     * IsPhysical flag set, that handle data of type @param type.
     *
     * This can be used to discover outputs associated with hardware devices.
     */
    virtual void      get_physical_outputs (DataType type, std::vector<std::string>& names) = 0;
    /** Store into @param names the names of all ports with the IsInput and
     * IsPhysical flags set, that handle data of type @param type.
     *
     * This can be used to discover inputs associated with hardware devices.
     */
    virtual void      get_physical_inputs (DataType type, std::vector<std::string>& names) = 0;
    /** Return the total count (possibly mixed between different data types)
	of the number of ports with the IsPhysical and IsOutput flags set.
    */
    virtual ChanCount n_physical_outputs () const = 0;
    /** Return the total count (possibly mixed between different data types)
	of the number of ports with the IsPhysical and IsInput flags set.
    */
    virtual ChanCount n_physical_inputs () const = 0;

    /** Return the address of the memory area where data for the port can be
     * written (if the port has the PortFlag IsOutput set) or read (if the port
     * has the PortFlag IsInput set).
     *
     * The return value is untyped because buffers containing different data
     * depending on the port type.
     */
    virtual void* get_buffer (PortHandle, pframes_t) = 0;

    /* MIDI ports (the ones in libmidi++) need this to be able to correctly
     * schedule MIDI events within their buffers. It is a bit odd that we
     * expose this here, because it is also exposed by AudioBackend, but they
     * only have access to a PortEngine object, not an AudioBackend.
     * 
     * Return the time according to the sample clock in use when the current 
     * buffer process cycle began. 
     *
     * XXX to be removed after some more design cleanup. 
     */
    virtual pframes_t sample_time_at_cycle_start () = 0;

  protected:
    PortManager& manager;
};

}

#endif /* __libardour_port_engine_h__ */
