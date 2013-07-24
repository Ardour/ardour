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

#ifndef __ardour_audiobackend_h__
#define __ardour_audiobackend_h__

#include <string>
#include <vector>

#include <stdint.h>
#include <stdlib.h>

namespace ARDOUR {

class AudioEngine;

class AudioBackend {
  public:

    enum State {
	    Stopped = 0x1,
	    Running = 0x2,
	    Paused =  0x4,
	    Freewheeling = 0x8,
    };

    AudioBackend (AudioEngine& e) : engine (e), _state (Stopped) {}
    virtual ~AudioBackend () {}

    /** return true if the underlying mechanism/API is still available
     * for us to utilize. return false if some or all of the AudioBackend
     * API can no longer be effectively used.
     */
    virtual bool connected() const = 0;

    /** return true if the callback from the underlying mechanism/API
     * (CoreAudio, JACK, ASIO etc.) occurs in a thread subject to realtime
     * constraints. Return false otherwise.
    */
    virtual bool is_realtime () const = 0;

    /* Discovering devices and parameters */

    /** Returns a collection of strings identifying devices known
     * to this backend. Any of these strings may be used to identify a
     * device in other calls to the backend, though any of them may become
     * invalid at any time.
     */
    virtual std::vector<std::string> enumerate_devices () const = 0;
    /** Returns a collection of float identifying sample rates that are
     * potentially usable with the hardware identified by @param device.
     * Any of these values may be supplied in other calls to this backend
     * as the desired sample rate to use with the name device, but the
     * requested sample rate may turn out to be unavailable, or become invalid
     * at any time.
     */
    virtual std::vector<float> available_sample_rates (const std::string& device) const = 0;
    /** Returns a collection of uint32 identifying buffer sizes that are
     * potentially usable with the hardware identified by @param device.
     * Any of these values may be supplied in other calls to this backend
     * as the desired buffer size to use with the name device, but the
     * requested buffer size may turn out to be unavailable, or become invalid
     * at any time.
     */
    virtual std::vector<uint32_t> available_buffer_sizes (const std::string& device) const = 0;

    struct Parameters {
	std::string device_name;
	float       sample_rate;
	uint32_t    buffer_size;
	uint32_t    systemic_input_latency;
	uint32_t    systemic_output_latency;
	uint32_t    input_channels;
	uint32_t    output_channels;
    };

    virtual int set_parameters (const Parameters&) = 0;
    virtual int get_parameters (Parameters&) const = 0;

    /* Basic state control */

    /** Start using the device named in the most recent call
     * to set_parameters(), with the parameters also provided
     * to that call.
     * 
     * At some undetermined time after this function is successfully called,
     * the backend will start calling the ::process_callback() method of
     * the AudioEngine referenced by @param engine. These calls will
     * occur in a thread created by and/or under the control of the backend.
     *
     * Return zero if successful, negative values otherwise.
     */
    virtual int start () = 0;

    /** Stop using the device named in the most recent call to set_parameters().
     *
     * If the function is successfully called, no subsequent calls to the
     * process_callback() of @param engine will be made after the function
     * returns, until set_parameters() and start() are called again.
     * 
     * The backend is considered to be un-configured after a successful
     * return, and requires a call to set_parameters() before it can be
     * start()-ed again. See pause() for a way to avoid this. stop() should
     * only be used when reconfiguration is required OR when there are no 
     * plans to use the backend in the future with a reconfiguration.
     *
     * Return zero if successful, 1 if the device is not in use, negative values on error
     */
    virtual int stop () = 0;

    /** Temporarily cease using the device named in the most recent call to set_parameters().
     *
     * If the function is successfully called, no subsequent calls to the
     * process_callback() of @param engine will be made after the function
     * returns, until start() is called again.
     * 
     * The backend will retain its existing parameter configuration after a successful
     * return, and requires a call to set_parameters() before it can be
     * start()-ed again. See pause() for a way to avoid this. stop() should
     * only be used when reconfiguration is required OR when there are no 
     * plans to use the backend in the future with a reconfiguration.
     *
     * Return zero if successful, 1 if the device is not in use, negative values on error
     */
    virtual int pause () = 0;

    /** While remaining connected to the device, and without changing its
     * configuration, start (or stop) calling the process_callback() of @param engine
     * without waiting for the device. 
     *
     * If @param start_stop is true, begin this behaviour, otherwise cease this
     * behaviour if it currently occuring, and return to calling
     * process_callback() of @param engine by waiting for the device.
     *
     * Return zero on success, non-zero otherwise.
     */
    virtual int freewheel (bool start_stop) = 0;

    /** return the fraction of the time represented by the current buffer
     * size that is being used for each buffer process cycle, as a value
     * from 0.0 to 1.0
    */
    virtual float get_cpu_load() const  = 0;

    /* Transport Control (JACK is the only audio API that currently offers
       the concept of shared transport control)
    */
    
    /** Attempt to change the transport state to TransportRolling. 
     */
    virtual void transport_start () {}
    /** Attempt to change the transport state to TransportStopped. 
     */
    virtual void transport_stop () {}
    /** return the current transport state
     */
    virtual TransportState transport_state () { return TransportStopped; }
    /** Attempt to locate the transport to @param pos
     */
    virtual void transport_locate (framepos_t pos) {}
    /** Return the current transport location, in samples measured
     * from the origin (defined by the transport time master)
     */
    virtual framepos_t transport_frame() { return 0; }

    virtual framecnt_t sample_rate () const;
    virtual pframes_t  samples_per_cycle () const;
    virtual int        usecs_per_cycle () const { return _usecs_per_cycle; }
    virtual size_t     raw_buffer_size (DataType t);
    
    /* Process time */
    
    /** return the time according to the sample clock in use, measured in
     * samples since an arbitrary zero time in the past. The value should
     * increase monotonically and linearly, without interruption from any
     * source (including CPU frequency scaling).
     *
     * It is extremely likely that any implementation will use a DLL, since
     * this function can be called from any thread, at any time, and must be 
     * able to accurately determine the correct sample time.
     */
    virtual pframes_t sample_time () = 0;

    /** return the time according to the sample clock in use when the current 
     * buffer process cycle began. 
     * 
     * Can ONLY be called from within a process() callback tree (which
     * implies that it can only be called by a process thread)
     */
    virtual pframes_t sample_time_at_cycle_start () = 0;

    /** return the time since the current buffer process cycle started,
     * in samples, according to the sample clock in use.
     * 
     * Can ONLY be called from within a process() callback tree (which
     * implies that it can only be called by a process thread)
     */
    virtual pframes_t samples_since_cycle_start () = 0;

    /** return true if it possible to determine the offset in samples of the
     * first video frame that starts within the current buffer process cycle,
     * measured from the first sample of the cycle. If returning true,
     * set @param offset to that offset.
     *
     * Eg. if it can be determined that the first video frame within the cycle
     * starts 28 samples after the first sample of the cycle, then this method
     * should return true and set @param offset to 28.
     *
     * May be impossible to support outside of JACK, which has specific support
     * (in some cases, hardware support) for this feature.
     *
     * Can ONLY be called from within a process() callback tree (which implies
     * that it can only be called by a process thread)
     */
    virtual bool get_sync_offset (pframes_t& offset) const { return 0; }
    
  private:
    AudioEngine&          engine;
    Parameters           _last_requested_parameters;
    State                _state;
};

}

#endif /* __ardour_audiobackend_h__ */
    
