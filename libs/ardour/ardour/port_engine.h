/*
 * Copyright (C) 2013-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015-2018 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __libardour_port_engine_h__
#define __libardour_port_engine_h__

#include <vector>
#include <string>

#include <stdint.h>

#include "ardour/data_type.h"
#include "ardour/libardour_visibility.h"
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

class LIBARDOUR_API ProtoPort {
  public:
	ProtoPort() {}
	virtual ~ProtoPort () {}
};

class LIBARDOUR_API PortEngine
{
public:
	PortEngine (PortManager& pm) : manager (pm) {}
	virtual ~PortEngine() {}

	/** Return a private, type-free pointer to any data
	 * that might be useful to a concrete implementation
	 */
	virtual void* private_handle() const = 0;

	/** Opaque handle to use as reference for Ports
	 *
	 * The handle needs to be lifetime managed (i.e. a shared_ptr type)
	 * in order to allow RCU to provide lock-free cross-thread operations
	 * on ports and ports containers.
	 *
	 * We could theoretically use a template (PortEngine\<T\>) and define
	 * PortHandle as T, but this complicates the desired inheritance
	 * pattern in which FooPortEngine handles things for the Foo API,
	 * rather than being a derivative of PortEngine\<Foo\>.
	 *
	 * We use this to declare return values and members of structures.
	 */
	typedef boost::shared_ptr<ProtoPort> PortPtr;

	/* We use this to declare arguments to methods/functions, in order to
	 * avoid copying shared_ptr<ProtoPort> every time (a practice we use in
	 * other contexts where we pass shared_ptr<T>). 
	 */
	typedef PortPtr const & PortHandle;

	/** Return the name of this process as used by the port manager
	 * when naming ports.
	 */
	virtual const std::string& my_name() const = 0;

	/** Return the maximum size of a port name */
	virtual uint32_t port_name_size() const = 0;

	/** Set/rename port
	 *
	 * @param port \ref PortHandle to operate on
	 * @param name new name to use for this port
	 * @return zero if successful, non-zero otherwise
	 */
	virtual int         set_port_name (PortHandle port, const std::string& name) = 0;

	/** Query port name
	 *
	 * @param port \ref PortHandle
	 * @return the name of the port referred to by @param port . If the port
	 *         does not exist, return an empty string.
	 */
	virtual std::string get_port_name (PortHandle port) const = 0;

	/** Query port-flags
	 *
	 * @param port \ref PortHandle
	 * @return the flags of the port referred to by \p port . If the port
	 *         does not exist, return PortFlags (0)
	 */
	virtual PortFlags get_port_flags (PortHandle port) const = 0;

	/** Return the port-property value and type for a given key.
	 * (eg query a human readable port name)
	 *
	 * The API follows jack_get_property():
	 *
	 * @param key The key of the property to retrieve
	 * @param value Set to the value of the property if found
	 * @param type The type of the property if set (
	 *             Type of data, either a MIME type or URI.
	 *             If type is empty, the data is assumed to be a UTF-8 encoded string.
	 *
	 * @return 0 on success, -1 if the @p subject has no @p key property.
	 *
	 * for available keys, see
	 * https://github.com/jackaudio/headers/blob/master/metadata.h
	 * https://github.com/drobilla/jackey/blob/master/jackey.h
	 */
	virtual int get_port_property (PortHandle, const std::string& key, std::string& value, std::string& type) const { return -1; }

	/** Set the port-property value and type for a given key
	 *
	 * The API follows jack_set_property():
	 * @param key The key of the property.
	 * @param value The value of the property.
	 * @param type The type of the property.
	 *
	 * @return 0 on success, -1 on error
	 */
	virtual int set_port_property (PortHandle, const std::string& key, const std::string& value, const std::string& type) { return -1; }

	/** Return a reference to a port with the fullname \p name .
	 *
	 * @param name Full port-name to lookup
	 * @return PortHandle if lookup was successful, or an "empty" PortHandle (analogous to a null pointer) if no such port exists.
	 */
	virtual PortPtr get_port_by_name (const std::string& name) const = 0;

	/** Find the set of ports whose names, types and flags match
	 * specified values, place the names of each port into \p ports .
	 *
	 * @param port_name_pattern match by given pattern. To avoid selecting by name, pass an empty string.
	 * @param type filter by given type; pass DataType::NIL to match all types.
	 * @param flags filter by flags, pass PortFlags (0) to avoid selecting by flags.
	 * @param ports array filled with matching port-names
	 * @return the count of the number found
	 */
	virtual int get_ports (const std::string& port_name_pattern, DataType type, PortFlags flags, std::vector<std::string>& ports) const = 0;

	/** Lookup data type of a port
	 *
	 * @param port \ref PortHandle of the port to lookup.
	 * @return the Ardour data type handled by the port referred to by \p port .
	 *         DataType::NIL is returned if the port does not exist.
	 */
	virtual DataType port_data_type (PortHandle port) const = 0;

	/** Create a new port whose fullname will be the conjunction of my_name(),
	 * ":" and \p shortname . The port will handle data specified by \p type
	 * and will have the flags given by \p flags . If successful,
	 *
	 * @param shortname Name of port to create
	 * @param type type of port to create
	 * @param flags flags of the port to create
	 * @return a reference to the port, otherwise return a null pointer.
	 */
	virtual PortPtr register_port (const std::string& shortname, ARDOUR::DataType type, ARDOUR::PortFlags flags) = 0;

	/* Destroy the port referred to by \p port, including all resources
	 * associated with it. This will also disconnect \p port from any ports it
	 * is connected to.
	 *
	 * @param port \ref PortHandle of the port to destroy
	 */
	virtual void    unregister_port (PortHandle port) = 0;

	/* Connection management */

	/** Ensure that data written to the port named by \p src will be
	 * readable from the port named by \p dst
	 *
	 * @param src name of source port to connect
	 * @param dst name of destination (sink) port
	 * @return zero on success, non-zero otherwise.
	 */
	virtual int   connect (const std::string& src, const std::string& dst) = 0;

	/** Remove any existing connection between the ports named by \p src and
	 * \p dst
	 *
	 * @param src name of source port to dis-connect to disconnect from
	 * @param dst name of destination (sink) port to disconnect
	 * @return zero on success, non-zero otherwise.
	 */
	virtual int   disconnect (const std::string& src, const std::string& dst) = 0;

	/** Ensure that data written to the port referenced by \p src will be
	 * readable from the port named by \p dst
	 *
	 * @param src \ref PortHandle of source port to connect
	 * @param dst \ref PortHandle of destination (sink) port
	 * @return zero on success, non-zero otherwise.
	 */
	virtual int   connect (PortHandle src, const std::string& dst) = 0;

	/** Remove any existing connection between the port referenced by \p src and
	 * the port named \p dst
	 *
	 * @param src \ref PortHandle of source port to disconnect from
	 * @param dst \ref PortHandle of destination (sink) port to disconnect
	 * @return zero on success, non-zero otherwise.
	 */
	virtual int   disconnect (PortHandle src, const std::string& dst) = 0;

	/** Remove all connections between the port referred to by \p port and
	 * any other ports.
	 *
	 * @param port \ref PortHandle of port to disconnect
	 * @return zero on success, non-zero otherwise.
	 */
	virtual int   disconnect_all (PortHandle port) = 0;

	/** Test if given \p port is connected
	 *
	 * @param port \ref PortHandle of port to test
	 * @param process_callback_safe true if this method is not called from rt-context of backend callbacks
	 * @return true if the port referred to by \p port has any connections to other ports. Return false otherwise.
	 */
	virtual bool  connected (PortHandle port, bool process_callback_safe = true) = 0;

	/** Test port connection
	 *
	 * @param port \ref PortHandle of source port to test
	 * @param name name of destination to test
	 * @param process_callback_safe true if this method is not called from rt-context of backend callbacks
	 * @return true if the port referred to by \p port is connected to the port named by \p name . Return false otherwise.
	 */
	virtual bool  connected_to (PortHandle port, const std::string& name, bool process_callback_safe = true) = 0;

	/** Test if given \p port is connected to physical I/O ports.
	 *
	 * @param port \ref PortHandle of source port to test
	 * @param process_callback_safe true if this method is not called from rt-context of backend callbacks
	 * @return true if the port referred to by \p port has any connections
	 *         to ports marked with the PortFlag IsPhysical. Return false otherwise.
	 */
	virtual bool  physically_connected (PortHandle port, bool process_callback_safe = true) = 0;

	/** Test if given \p port is has external connections.
	 *
	 * @param port \ref PortHandle of port to test
	 * @param process_callback_safe true if this method is not called from rt-context of backend callbacks
	 * @return true if the port referred to by \p port has any connections
	 *         to external, not-ardour owned, ports.
	 */
	virtual bool  externally_connected (PortHandle port, bool process_callback_safe = true) {
		/* only with JACK, provides client ports that are not physical */
		return physically_connected (port, process_callback_safe);
	}

	/** Place the names of all ports connected to the port named by
	 * \p port into \p names .
	 *
	 * @param port \ref PortHandle
	 * @param names array or returned port-names
	 * @param process_callback_safe true if this method is not called from rt-context of backend callbacks
	 * @return number of connections found
	 */
	virtual int   get_connections (PortHandle port, std::vector<std::string>& names, bool process_callback_safe = true) = 0;

	/* MIDI */

	/** Retrieve a MIDI event from the data at \p port_buffer . The event
	 * number to be retrieved is given by \p event_index (a value of zero
	 * indicates that the first event in the port_buffer should be retrieved).
	 *
	 * The data associated with the event will be copied into the buffer at
	 * \p buf and the number of bytes written will be stored in \p size .
	 * The timestamp of the event (which is always relative to the start
	 * of the current process cycle, in samples) will be stored in \p timestamp .
	 *
	 * @param timestamp time in samples relative to the current cycle start
	 * @param size number of bytes read into \p buf
	 * @param buf raw MIDI data
	 * @param port_buffer the midi-port buffer
	 * @param event_index index of event to retrieve
	 * @return 0 on success, -1 otherwise
	 */
	virtual int      midi_event_get (pframes_t& timestamp, size_t& size, uint8_t const** buf, void* port_buffer, uint32_t event_index) = 0;

	/** Place a MIDI event consisting of \p size bytes copied from the data
	 * at \p buffer into the port buffer referred to by \p port_buffer .
	 * The MIDI event will be marked with a time given by \p timestamp .
	 *
	 * Events  must be added monotonically to a port buffer. An attempt to
	 * add a non-monotonic event (e.g. out-of-order) will cause this method
	 * to return a failure status.
	 *
	 * @param port_buffer the midi-port buffer
	 * @param timestamp time in samples relative to the current cycle start
	 * @param buffer raw MIDI data to emplace
	 * @param size number of bytes of \p buffer
	 * @return zero on success, non-zero otherwise.
	 */
	virtual int      midi_event_put (void* port_buffer, pframes_t timestamp, const uint8_t* buffer, size_t size) = 0;

	/** Query the number of MIDI events in the data at \p port_buffer
	 *
	 * @param port_buffer the midi-port buffer
	 * @return the number of MIDI events in the data at \p port_buffer
	*/
	virtual uint32_t get_midi_event_count (void* port_buffer) = 0;

	/** Clear the buffer at \p port_buffer of all MIDI events.
	 *
	 * After a call to this method, an immediate, subsequent call to
	 * \ref get_midi_event_count with the same \p port_buffer argument must
	 * return zero.
	 *
	 * @param port_buffer the buffer to clear
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
	 * of the hardware channel represented by the port referred to by
	 * \p port .
	 *
	 * If the number of requests rises above zero, input monitoring will
	 * be enabled (if can_monitor_input() returns true for the implementation).
	 *
	 * If the number of requests falls to zero, input monitoring will be
	 * disabled (if can_monitor_input() returns true for the implementation)
	 *
	 * @param port \ref PortHandle
	 * @param yn true to enable hardware monitoring, false to disable
	 * @return 0 on success, -1 otherwise
	 */
	virtual int   request_input_monitoring (PortHandle port, bool yn) = 0;

	/* Force input monitoring of the hardware channel represented by the port
	 * referred to by \p port to be on or off, depending on the true/false
	 * status of \p yn. The request count is ignored when using this
	 * method, so if this is called with \p yn set to false, input monitoring will
	 * be disabled regardless of the number of requests to enable it.
	 *
	 * @param port \ref PortHandle
	 * @param yn true to enable hardware monitoring, false to disable
	 * @return 0 on success, -1 otherwise
	 */
	virtual int   ensure_input_monitoring (PortHandle port, bool yn) = 0;

	/** Query status of hardware monitoring for given \p port
	 *
	 * @param port \ref PortHandle to test
	 * @return true if input monitoring is enabled for the hardware channel
	 *         represented by the port referred to by \p port .
	 *         Return false otherwise.
	 */
	virtual bool  monitoring_input (PortHandle port) = 0;

	/* Latency management */

	/** Set the latency range for the port referred to by \p port to
	 * \p r . The playback range will be set if \p for_playback is true,
	 * otherwise the capture range will be set.
	 *
	 * @param port \ref PortHandle to operate on
	 * @param for_playback When true, playback latency is set: How long will it be
	 *                     until the signal arrives at the edge of the process graph.
	 *                     When false the capture latency is set: ow long has it been
	 *                     since the signal arrived at the edge of the process graph.
	 * @param r min/max latency for given port.
	 */
	virtual void          set_latency_range (PortHandle port, bool for_playback, LatencyRange r) = 0;

	/** Return the latency range for the port referred to by \p port .
	 * The playback range will be returned if @param for_playback is true,
	 * otherwise the capture range will be returned.
	 *
	 * @param port The PortHandle to query
	 * @param for_playback When true, playback (downstream) latency is queried,
	 *                     false for capture (upstream) latency.
	 */
	virtual LatencyRange  get_latency_range (PortHandle port, bool for_playback) = 0;

	/* Discovering physical ports */

	/** Return true if the port referred to by \p port has the IsPhysical
	 * flag set. Return false otherwise.
	 *
	 * @param port \ref PortHandle to query
	 */
	virtual bool      port_is_physical (PortHandle port) const = 0;

	/** Store into \p names the names of all ports with the IsOutput and
	 * IsPhysical flag set, that handle data of type \p type .
	 *
	 * This can be used to discover outputs associated with hardware devices.
	 *
	 * @param type Data-type to lookup
	 * @param names return value to populate with names
	 */
	virtual void      get_physical_outputs (DataType type, std::vector<std::string>& names) = 0;

	/** Store into @param names the names of all ports with the IsInput and
	 * IsPhysical flags set, that handle data of type @param type .
	 *
	 * This can be used to discover inputs associated with hardware devices.
	 */
	virtual void      get_physical_inputs (DataType type, std::vector<std::string>& names) = 0;

	/** @return the total count (possibly mixed between different data types)
	 * of the number of ports with the IsPhysical and IsOutput flags set.
	 */
	virtual ChanCount n_physical_outputs () const = 0;

	/** @return the total count (possibly mixed between different data types)
	 * of the number of ports with the IsPhysical and IsInput flags set.
	 */
	virtual ChanCount n_physical_inputs () const = 0;

	/** Return the address of the memory area where data for the port can be
	 * written (if the port has the PortFlag IsOutput set) or read (if the port
	 * has the PortFlag IsInput set).
	 *
	 * The return value is untyped because buffers containing different data
	 * depending on the port type.
	 *
	 * @param port \ref PortHandle
	 * @param off memory offset
	 * @return pointer to raw memory area
	 */
	virtual void* get_buffer (PortHandle port, pframes_t off) = 0;

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
	virtual samplepos_t sample_time_at_cycle_start () = 0;

protected:
	PortManager& manager;
};

} // namespace

#endif /* __libardour_port_engine_h__ */
