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

#ifndef __libardour_audiobackend_h__
#define __libardour_audiobackend_h__

#include <string>
#include <vector>

#include <stdint.h>
#include <stdlib.h>

#include <boost/function.hpp>

#include "ardour/types.h"
#include "ardour/audioengine.h"
#include "ardour/port_engine.h"
#include "ardour/visibility.h"

#ifdef ARDOURBACKEND_DLL_EXPORTS // defined if we are building the ARDOUR Panners DLLs (instead of using them)
    #define ARDOURBACKEND_API LIBARDOUR_HELPER_DLL_EXPORT
#else
    #define ARDOURBACKEND_API LIBARDOUR_HELPER_DLL_IMPORT
#endif 
#define ARDOURBACKEND_LOCAL LIBARDOUR_HELPER_DLL_LOCAL

namespace ARDOUR {

class AudioBackend : public PortEngine {
  public:

    AudioBackend (AudioEngine& e) : PortEngine (e), engine (e) {}
    virtual ~AudioBackend () {}

    /** Return the name of this backend.
     *
     * Should use a well-known, unique term. Expected examples
     * might include "JACK", "CoreAudio", "ASIO" etc.
     */
    virtual std::string name() const = 0;

    /** Return true if the callback from the underlying mechanism/API
     * (CoreAudio, JACK, ASIO etc.) occurs in a thread subject to realtime
     * constraints. Return false otherwise.
    */
    virtual bool is_realtime () const = 0;

    /* Discovering devices and parameters */

    /** Return true if this backend requires the selection of a "driver"
     * before any device can be selected. Return false otherwise.
     *
     * Intended mainly to differentiate between meta-APIs like JACK
     * which can still expose different backends (such as ALSA or CoreAudio 
     * or FFADO or netjack) and those like ASIO or CoreAudio which
     * do not.
     */
    virtual bool requires_driver_selection() const { return false; }

    /** If the return value of requires_driver_selection() is true,
     * then this function can return the list of known driver names.
     *
     * If the return value of requires_driver_selection() is false,
     * then this function should not be called. If it is called
     * its return value is an empty vector of strings.
     */
    virtual std::vector<std::string> enumerate_drivers() const { return std::vector<std::string>(); }

    /** Returns zero if the backend can successfully use @param name as the
     * driver, non-zero otherwise.
     *
     * Should not be used unless the backend returns true from
     * requires_driver_selection()
     */
    virtual int set_driver (const std::string& /*drivername*/) { return 0; }

    /** used to list device names along with whether or not they are currently
     *  available. 
    */
    struct DeviceStatus {
	std::string name;
	bool        available;

	DeviceStatus (const std::string& s, bool avail) : name (s), available (avail) {}
    };

    /** Returns a collection of DeviceStatuses identifying devices discovered
     * by this backend since the start of the process.
     *
     * Any of the names in each DeviceStatus may be used to identify a
     * device in other calls to the backend, though any of them may become
     * invalid at any time.
     */
    virtual std::vector<DeviceStatus> enumerate_devices () const = 0;

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

    /** Returns the maximum number of input channels that are potentially
     * usable with the hardware identified by @param device.  Any number from 1
     * to the value returned may be supplied in other calls to this backend as
     * the input channel count to use with the name device, but the requested
     * count may turn out to be unavailable, or become invalid at any time.
     */
    virtual uint32_t available_input_channel_count (const std::string& device) const = 0;

    /** Returns the maximum number of output channels that are potentially
     * usable with the hardware identified by @param device.  Any number from 1
     * to the value returned may be supplied in other calls to this backend as
     * the output channel count to use with the name device, but the requested
     * count may turn out to be unavailable, or become invalid at any time.
     */
    virtual uint32_t available_output_channel_count (const std::string& device) const = 0;

    /* Return true if the derived class can change the sample rate of the
     * device in use while the device is already being used. Return false
     * otherwise. (example: JACK cannot do this as of September 2013)
     */
    virtual bool can_change_sample_rate_when_running () const = 0;
    /* Return true if the derived class can change the buffer size of the
     * device in use while the device is already being used. Return false
     * otherwise. 
     */
    virtual bool can_change_buffer_size_when_running () const = 0;

    /* Set the hardware parameters.
     * 
     * If called when the current state is stopped or paused,
     * the changes will not take effect until the state changes to running.
     *
     * If called while running, the state will change as fast as the
     * implementation allows.
     *
     * All set_*() methods return zero on success, non-zero otherwise.
     */

    /** Set the name of the device to be used
     */
    virtual int set_device_name (const std::string&) = 0;
    /** Set the sample rate to be used
     */
    virtual int set_sample_rate (float) = 0;
    /** Set the buffer size to be used.
     *
     * The device is assumed to use a double buffering scheme, so that one
     * buffer's worth of data can be processed by hardware while software works
     * on the other buffer. All known suitable audio APIs support this model
     * (though ALSA allows for alternate numbers of buffers, and CoreAudio
     * doesn't directly expose the concept).
     */
    virtual int set_buffer_size (uint32_t) = 0;
    /** Set the preferred underlying hardware sample format
     *
     * This does not change the sample format (32 bit float) read and
     * written to the device via the Port API.
     */
    virtual int set_sample_format (SampleFormat) = 0;
    /** Set the preferred underlying hardware data layout.
     * If @param yn is true, then the hardware will interleave
     * samples for successive channels; otherwise, the hardware will store
     * samples for a single channel contiguously.
     * 
     * Setting this does not change the fact that all data streams
     * to and from Ports are mono (essentially, non-interleaved)
     */
    virtual int set_interleaved (bool yn) = 0;
    /** Set the number of input channels that should be used
     */
    virtual int set_input_channels (uint32_t) = 0;
    /** Set the number of output channels that should be used
     */
    virtual int set_output_channels (uint32_t) = 0;
    /** Set the (additional) input latency that cannot be determined via 
     * the implementation's underlying code (e.g. latency from
     * external D-A/D-A converters. Units are samples.
     */
    virtual int set_systemic_input_latency (uint32_t) = 0;
    /** Set the (additional) output latency that cannot be determined via 
     * the implementation's underlying code (e.g. latency from
     * external D-A/D-A converters. Units are samples.
     */
    virtual int set_systemic_output_latency (uint32_t) = 0;

    /* Retrieving parameters */

    virtual std::string  device_name () const = 0;
    virtual float        sample_rate () const = 0;
    virtual uint32_t     buffer_size () const = 0;
    virtual SampleFormat sample_format () const = 0;
    virtual bool         interleaved () const = 0;
    virtual uint32_t     input_channels () const = 0;
    virtual uint32_t     output_channels () const = 0;
    virtual uint32_t     systemic_input_latency () const = 0;
    virtual uint32_t     systemic_output_latency () const = 0;

    /** override this if this implementation returns true from
     * requires_driver_selection()
     */
    virtual std::string  driver_name() const { return std::string(); }

    /** Return the name of a control application for the 
     * selected/in-use device. If no such application exists,
     * or if no device has been selected or is in-use,
     * return an empty string.
     */
    virtual std::string control_app_name() const = 0;
    /** Launch the control app for the currently in-use or
     * selected device. May do nothing if the control
     * app is undefined or cannot be launched.
     */
    virtual void launch_control_app () = 0;
    /* Basic state control */

    /** Start using the device named in the most recent call
     * to set_device(), with the parameters set by various
     * the most recent calls to set_sample_rate() etc. etc.
     * 
     * At some undetermined time after this function is successfully called,
     * the backend will start calling the ::process_callback() method of
     * the AudioEngine referenced by @param engine. These calls will
     * occur in a thread created by and/or under the control of the backend.
     *
     * Return zero if successful, negative values otherwise.
     */
    virtual int start () = 0;

    /** Stop using the device currently in use. 
     *
     * If the function is successfully called, no subsequent calls to the
     * process_callback() of @param engine will be made after the function
     * returns, until parameters are reset and start() are called again.
     * 
     * The backend is considered to be un-configured after a successful
     * return, and requires calls to set hardware parameters before it can be
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
     * return, and does NOT require any calls to set hardware parameters before it can be
     * start()-ed again. 
     *
     * Return zero if successful, 1 if the device is not in use, negative values on error
     */
    virtual int pause () = 0;

    /** While remaining connected to the device, and without changing its
     * configuration, start (or stop) calling the process_callback() of @param engine
     * without waiting for the device. Once process_callback() has returned, it
     * will be called again immediately, thus allowing for faster-than-realtime
     * processing.
     *
     * All registered ports remain in existence and all connections remain
     * unaltered. However, any physical ports should NOT be used by the
     * process_callback() during freewheeling - the data behaviour is undefined.
     *
     * If @param start_stop is true, begin this behaviour; otherwise cease this
     * behaviour if it currently occuring, and return to calling
     * process_callback() of @param engine by waiting for the device.
     *
     * Return zero on success, non-zero otherwise.
     */
    virtual int freewheel (bool start_stop) = 0;

    /** return the fraction of the time represented by the current buffer
     * size that is being used for each buffer process cycle, as a value
     * from 0.0 to 1.0
     *
     * E.g. if the buffer size represents 5msec and current processing
     * takes 1msec, the returned value should be 0.2. 
     * 
     * Implementations can feel free to smooth the values returned over
     * time (e.g. high pass filtering, or its equivalent).
     */
    virtual float cpu_load() const  = 0;

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
    virtual TransportState transport_state () const { return TransportStopped; }
    /** Attempt to locate the transport to @param pos
     */
    virtual void transport_locate (framepos_t /*pos*/) {}
    /** Return the current transport location, in samples measured
     * from the origin (defined by the transport time master)
     */
    virtual framepos_t transport_frame() const { return 0; }

    /** If @param yn is true, become the time master for any inter-application transport
     * timebase, otherwise cease to be the time master for the same.
     *
     * Return zero on success, non-zero otherwise
     * 
     * JACK is the only currently known audio API with the concept of a shared
     * transport timebase.
     */
    virtual int set_time_master (bool /*yn*/) { return 0; }

    virtual int        usecs_per_cycle () const { return 1000000 * (buffer_size() / sample_rate()); }
    virtual size_t     raw_buffer_size (DataType t) = 0;
    
    /* Process time */
    
    /** return the time according to the sample clock in use, measured in
     * samples since an arbitrary zero time in the past. The value should
     * increase monotonically and linearly, without interruption from any
     * source (including CPU frequency scaling).
     *
     * It is extremely likely that any implementation will use a DLL, since
     * this function can be called from any thread, at any time, and must be 
     * able to accurately determine the correct sample time.
     *
     * Can be called from any thread.
     */
    virtual pframes_t sample_time () = 0;

    /** Return the time according to the sample clock in use when the most
     * recent buffer process cycle began. Can be called from any thread.
     */
    virtual pframes_t sample_time_at_cycle_start () = 0;

    /** Return the time since the current buffer process cycle started,
     * in samples, according to the sample clock in use.
     * 
     * Can ONLY be called from within a process() callback tree (which
     * implies that it can only be called by a process thread)
     */
    virtual pframes_t samples_since_cycle_start () = 0;

    /** Return true if it possible to determine the offset in samples of the
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
    virtual bool get_sync_offset (pframes_t& /*offset*/) const { return false; }

    /** Create a new thread suitable for running part of the buffer process
     * cycle (i.e. Realtime scheduling, memory allocation, etc. etc are all
     * correctly setup), with a stack size given in bytes by specified @param
     * stacksize. The thread will begin executing @param func, and will exit
     * when that function returns.
     */
    virtual int create_process_thread (boost::function<void()> func, AudioBackendThread*, size_t stacksize) = 0;

    /** Wait for the thread specified by @param thread to exit.
     * 
     * Return zero on success, non-zero on failure.
     */
    virtual int join_process_thread (AudioBackendThread* thread) = 0;

    /** Return true if execution context is in a backend thread
     */
    virtual bool in_process_thread () = 0;

    virtual void update_latencies () = 0;

  protected:
    AudioEngine&          engine;
};

struct AudioBackendInfo {
    const char* name;

    /** Using arg1 and arg2, initialize this audiobackend.
     * 
     * Returns zero on success, non-zero otherwise.
     */
    int (*instantiate) (const std::string& arg1, const std::string& arg2);

    /** Release all resources associated with this audiobackend
     */
    int (*deinstantiate) (void);

    /** Factory method to create an AudioBackend-derived class.
     * 
     * Returns a valid shared_ptr to the object if successfull,
     * or a "null" shared_ptr otherwise.
     */
    boost::shared_ptr<AudioBackend> (*factory) (AudioEngine&);

    /** Return true if the underlying mechanism/API has been
     * configured and does not need (re)configuration in order
     * to be usable. Return false otherwise.
     *
     * Note that this may return true if (re)configuration, even though
     * not currently required, is still possible.
     */
    bool (*already_configured)();
};

} // namespace

#endif /* __libardour_audiobackend_h__ */
    
