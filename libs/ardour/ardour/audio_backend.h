/*
 * Copyright (C) 2013-2016 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2013-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2016 Robin Gareus <robin@gareus.org>
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

#ifndef __libardour_audiobackend_h__
#define __libardour_audiobackend_h__

#include <string>
#include <vector>

#include <stdint.h>
#include <stdlib.h>

#include <boost/function.hpp>

#include "ardour/audioengine.h"
#include "ardour/libardour_visibility.h"
#include "ardour/port_engine.h"
#include "ardour/types.h"

#ifdef ARDOURBACKEND_DLL_EXPORTS // defined if we are building the ARDOUR Panners DLLs (instead of using them)
# define ARDOURBACKEND_API LIBARDOUR_DLL_EXPORT
#else
# define ARDOURBACKEND_API LIBARDOUR_DLL_IMPORT
#endif
#define ARDOURBACKEND_LOCAL LIBARDOUR_DLL_LOCAL

namespace ARDOUR
{
struct LIBARDOUR_API AudioBackendInfo {
	const char* name;

	/** Using arg1 and arg2, initialize this audiobackend.
	 *
	 * Returns zero on success, non-zero otherwise.
	 */
	int (*instantiate) (const std::string& arg1, const std::string& arg2);

	/** Release all resources associated with this audiobackend */
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
	bool (*already_configured) ();

	/** Return true if the underlying mechanism/API can be
	 * used on the given system.
	 *
	 * If this function returns false, the backend is not
	 * listed in the engine dialog.
	 */
	bool (*available) ();
};

/** AudioBackend is an high-level abstraction for interacting with the operating system's
 * audio and midi I/O.
 */
class LIBARDOUR_API AudioBackend : public PortEngine
{
public:
	AudioBackend (AudioEngine& e, AudioBackendInfo& i)
	        : PortEngine (e)
	        , _info (i)
	        , engine (e)
	{}

	virtual ~AudioBackend () {}

	enum ErrorCode {
		NoError                    = 0,
		BackendInitializationError = -64,
		BackendDeinitializationError,
		BackendReinitializationError,
		AudioDeviceOpenError,
		AudioDeviceCloseError,
		AudioDeviceInvalidError,
		AudioDeviceNotAvailableError,
		AudioDeviceNotConnectedError,
		AudioDeviceReservationError,
		AudioDeviceIOError,
		MidiDeviceOpenError,
		MidiDeviceCloseError,
		MidiDeviceNotAvailableError,
		MidiDeviceNotConnectedError,
		MidiDeviceIOError,
		SampleFormatNotSupportedError,
		SampleRateNotSupportedError,
		RequestedInputLatencyNotSupportedError,
		RequestedOutputLatencyNotSupportedError,
		PeriodSizeNotSupportedError,
		PeriodCountNotSupportedError,
		DeviceConfigurationNotSupportedError,
		ChannelCountNotSupportedError,
		InputChannelCountNotSupportedError,
		OutputChannelCountNotSupportedError,
		AquireRealtimePermissionError,
		SettingAudioThreadPriorityError,
		SettingMIDIThreadPriorityError,
		ProcessThreadStartError,
		FreewheelThreadStartError,
		PortRegistrationError,
		PortReconnectError,
		OutOfMemoryError,
	};

	static std::string get_error_string (ErrorCode);

	enum StandardDeviceName {
		DeviceNone,
		DeviceDefault
	};

	static std::string get_standard_device_name (StandardDeviceName);

	/** Return the AudioBackendInfo object from which this backend
	 * was constructed.
	 */
	AudioBackendInfo& info () const
	{
		return _info;
	}

	/** Return the name of this backend.
	 *
	 * Should use a well-known, unique term. Expected examples
	 * might include "JACK", "CoreAudio", "ASIO" etc.
	 */
	virtual std::string name () const = 0;

	/** Return true if the callback from the underlying mechanism/API
	 * (CoreAudio, JACK, ASIO etc.) occurs in a thread subject to realtime
	 * constraints. Return false otherwise.
	 */
	virtual bool is_realtime () const = 0;

	virtual int client_real_time_priority () { return PBD_RT_PRI_PROC; }

	/* Discovering devices and parameters */

	/** Return true if this backend requires the selection of a "driver"
	 * before any device can be selected. Return false otherwise.
	 *
	 * Intended mainly to differentiate between meta-APIs like JACK
	 * which can still expose different backends (such as ALSA or CoreAudio
	 * or FFADO or netjack) and those like ASIO or CoreAudio which
	 * do not.
	 */
	virtual bool requires_driver_selection () const
	{
		return false;
	}

	/** If the return value of requires_driver_selection() is true,
	 * then this function can return the list of known driver names.
	 *
	 * If the return value of requires_driver_selection() is false,
	 * then this function should not be called. If it is called
	 * its return value is an empty vector of strings.
	 */
	virtual std::vector<std::string> enumerate_drivers () const
	{
		return std::vector<std::string> ();
	}

	/** Returns zero if the backend can successfully use \p drivername
	 * as the driver, non-zero otherwise.
	 *
	 * Should not be used unless the backend returns true from
	 * requires_driver_selection()
	 */
	virtual int set_driver (const std::string& drivername)
	{
		return 0;
	}

	/** used to list device names along with whether or not they are currently
	 *  available.
	 */
	struct DeviceStatus {
		std::string name;
		bool        available;

		DeviceStatus (const std::string& s, bool avail)
		        : name (s)
		        , available (avail)
		{}
	};

	/** An optional alternate interface for backends to provide a facility to
	 * select separate input and output devices.
	 *
	 * If a backend returns true then enumerate_input_devices() and
	 * enumerate_output_devices() will be used instead of enumerate_devices()
	 * to enumerate devices. Similarly set_input/output_device_name() should
	 * be used to set devices instead of set_device_name().
	 */
	virtual bool use_separate_input_and_output_devices () const
	{
		return false;
	}

	/* Return true if the backend uses separate I/O devices only for the case
	 * of allowing one to be "None".
	 *
	 * ie. Input Device must match Output Device, except if either of them
	 * is get_standard_device_name (DeviceNone).
	 */
	virtual bool match_input_output_devices_or_none () const
	{
		return false;
	}

	/** Returns a collection of DeviceStatuses identifying devices discovered
	 * by this backend since the start of the process.
	 *
	 * Any of the names in each DeviceStatus may be used to identify a
	 * device in other calls to the backend, though any of them may become
	 * invalid at any time.
	 */
	virtual std::vector<DeviceStatus> enumerate_devices () const = 0;

	/** Returns a collection of DeviceStatuses identifying input devices
	 * discovered by this backend since the start of the process.
	 *
	 * Any of the names in each DeviceStatus may be used to identify a
	 * device in other calls to the backend, though any of them may become
	 * invalid at any time.
	 */
	virtual std::vector<DeviceStatus> enumerate_input_devices () const
	{
		return std::vector<DeviceStatus> ();
	}

	/** Returns a collection of DeviceStatuses identifying output devices
	 * discovered by this backend since the start of the process.
	 *
	 * Any of the names in each DeviceStatus may be used to identify a
	 * device in other calls to the backend, though any of them may become
	 * invalid at any time.
	 */
	virtual std::vector<DeviceStatus> enumerate_output_devices () const
	{
		return std::vector<DeviceStatus> ();
	}

	/** An interface to set buffers/period for playback latency.
	 * useful for ALSA or JACK/ALSA on Linux.
	 *
	 * @return true if the backend supports period-size configuration
	 */
	virtual bool can_set_period_size () const
	{
		return false;
	}

	/** Returns a vector of supported period-sizes for the given driver */
	virtual std::vector<uint32_t> available_period_sizes (const std::string& driver, const std::string& device) const
	{
		return std::vector<uint32_t> ();
	}

	/** Set the period size to be used.
	 * must be called before starting the backend.
	 */
	virtual int set_peridod_size (uint32_t)
	{
		return -1;
	}

	/**
	 * @return true if backend supports requesting an update to the device list
	 * and any cached properties associated with the devices.
	 */
	virtual bool can_request_update_devices ()
	{
		return false;
	}

	/**
	 * Request an update to the list of devices returned in the enumerations.
	 * The Backend must return true from can_request_update_devices to support
	 * this interface.
	 * @return true if the devices were updated
	 */
	virtual bool update_devices ()
	{
		return false;
	}

	/**
	 * @return true if backend supports a blocking or buffered mode, false by
	 * default unless implemented by a derived class.
	 */
	virtual bool can_use_buffered_io ()
	{
		return false;
	}

	/**
	 * Set the backend to use a blocking or buffered I/O mode
	 */
	virtual void set_use_buffered_io (bool) {}

	/**
	 * @return Set the backend to use a blocking or buffered I/O mode, false by
	 * default unless implemented by a derived class.
	 */
	virtual bool get_use_buffered_io ()
	{
		return false;
	}

	/** Returns a collection of float identifying sample rates that are
	 * potentially usable with the hardware identified by \p device .
	 * Any of these values may be supplied in other calls to this backend
	 * as the desired sample rate to use with the name device, but the
	 * requested sample rate may turn out to be unavailable, or become invalid
	 * at any time.
	 */
	virtual std::vector<float> available_sample_rates (const std::string& device) const = 0;

	/* backends that suppor586t separate input and output devices should
	 * implement this function and return an intersection (not union) of available
	 * sample rates valid for the given input + output device combination.
	 */
	virtual std::vector<float> available_sample_rates2 (const std::string& input_device, const std::string& output_device) const
	{
		std::vector<float> input_sizes  = available_sample_rates (input_device);
		std::vector<float> output_sizes = available_sample_rates (output_device);
		std::vector<float> rv;
		std::set_union (input_sizes.begin (), input_sizes.end (),
		                output_sizes.begin (), output_sizes.end (),
		                std::back_inserter (rv));
		return rv;
	}

	/* Returns the default sample rate that will be shown to the user when
	 * configuration options are first presented. If the derived class
	 * needs or wants to override this, it can. It also MUST override this
	 * if there is any chance that an SR of 44.1kHz is not in the list
	 * returned by available_sample_rates()
	 */
	virtual float default_sample_rate () const
	{
		return 44100.0;
	}

	/** Returns a collection of uint32 identifying buffer sizes that are
	 * potentially usable with the hardware identified by \p device .
	 * Any of these values may be supplied in other calls to this backend
	 * as the desired buffer size to use with the name device, but the
	 * requested buffer size may turn out to be unavailable, or become invalid
	 * at any time.
	 */
	virtual std::vector<uint32_t> available_buffer_sizes (const std::string& device) const = 0;

	/* backends that support separate input and output devices should
	 * implement this function and return an intersection (not union) of available
	 * buffer sizes valid for the given input + output device combination.
	 */
	virtual std::vector<uint32_t> available_buffer_sizes2 (const std::string& input_device, const std::string& output_device) const
	{
		std::vector<uint32_t> input_rates  = available_buffer_sizes (input_device);
		std::vector<uint32_t> output_rates = available_buffer_sizes (output_device);
		std::vector<uint32_t> rv;
		std::set_union (input_rates.begin (), input_rates.end (),
		                output_rates.begin (), output_rates.end (),
		                std::back_inserter (rv));
		return rv;
	}
	/* Returns the default buffer size that will be shown to the user when
	 * configuration options are first presented. If the derived class
	 * needs or wants to override this, it can. It also MUST override this
	 * if there is any chance that a buffer size of 1024 is not in the list
	 * returned by available_buffer_sizes()
	 */
	virtual uint32_t default_buffer_size (const std::string& device) const
	{
		return 1024;
	}

	/** Returns the maximum number of input channels that are potentially
	 * usable with the hardware identified by \p device . Any number from 1
	 * to the value returned may be supplied in other calls to this backend as
	 * the input channel count to use with the name device, but the requested
	 * count may turn out to be unavailable, or become invalid at any time.
	 */
	virtual uint32_t available_input_channel_count (const std::string& device) const = 0;

	/** Returns the maximum number of output channels that are potentially
	 * usable with the hardware identified by \p device . Any number from 1
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

	/** return true if the backend is configured using a single
	 * full-duplex device and measuring systemic latency can
	 * produce meaningful results.
	 */
	virtual bool can_measure_systemic_latency () const = 0;

	/** return true if the backend can measure and update
	 * systemic latencies without restart.
	 */
	virtual bool can_change_systemic_latency_when_running () const
	{
		return false;
	}

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

	/** Set the name of the device to be used */
	virtual int set_device_name (const std::string&) = 0;

	/** Set the name of the input device to be used if using separate
	 * input/output devices.
	 *
	 * @see use_separate_input_and_output_devices()
	 */
	virtual int set_input_device_name (const std::string&)
	{
		return 0;
	}

	/** Set the name of the output device to be used if using separate
	 * input/output devices.
	 *
	 * @see use_separate_input_and_output_devices()
	 */
	virtual int set_output_device_name (const std::string&)
	{
		return 0;
	}

	/** Deinitialize and destroy current device */
	virtual int drop_device ()
	{
		return 0;
	};

	/** Set the sample rate to be used */
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

	/** Set the preferred underlying hardware data layout.
	 * If \p yn is true, then the hardware will interleave
	 * samples for successive channels; otherwise, the hardware will store
	 * samples for a single channel contiguously.
	 *
	 * Setting this does not change the fact that all data streams
	 * to and from Ports are mono (essentially, non-interleaved)
	 */
	virtual int set_interleaved (bool yn) = 0;

	/** Set the number of input channels that should be used */
	virtual int set_input_channels (uint32_t) = 0;

	/** Set the number of output channels that should be used */
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

	/** Set the (additional) input latency for a specific midi device,
	 * or if the identifier is empty, apply to all midi devices.
	 */
	virtual int set_systemic_midi_input_latency (std::string const, uint32_t) = 0;

	/** Set the (additional) output latency for a specific midi device,
	 * or if the identifier is empty, apply to all midi devices.
	 */
	virtual int set_systemic_midi_output_latency (std::string const, uint32_t) = 0;

	/* Retrieving parameters */

	virtual std::string device_name () const = 0;
	virtual std::string input_device_name () const
	{
		return std::string ();
	}

	virtual std::string output_device_name () const
	{
		return std::string ();
	}

	virtual float    sample_rate () const                                   = 0;
	virtual uint32_t buffer_size () const                                   = 0;
	virtual bool     interleaved () const                                   = 0;
	virtual uint32_t input_channels () const                                = 0;
	virtual uint32_t output_channels () const                               = 0;
	virtual uint32_t systemic_input_latency () const                        = 0;
	virtual uint32_t systemic_output_latency () const                       = 0;
	virtual uint32_t systemic_midi_input_latency (std::string const) const  = 0;
	virtual uint32_t systemic_midi_output_latency (std::string const) const = 0;

	/* defaults as reported by device driver */
	virtual uint32_t systemic_hw_input_latency () const { return 0; }
	virtual uint32_t systemic_hw_output_latency () const { return 0; }

	virtual uint32_t period_size () const { return 0; }

	/** override this if this implementation returns true from
	 * requires_driver_selection()
	 */
	virtual std::string driver_name () const
	{
		return std::string ();
	}

	/** Return the name of a control application for the
	 * selected/in-use device. If no such application exists,
	 * or if no device has been selected or is in-use,
	 * return an empty string.
	 */
	virtual std::string control_app_name () const = 0;

	/** Launch the control app for the currently in-use or
	 * selected device. May do nothing if the control
	 * app is undefined or cannot be launched.
	 */
	virtual void launch_control_app () = 0;

	/* @return a vector of strings that describe the available
	 * MIDI options.
	 *
	 * These can be presented to the user to decide which
	 * MIDI drivers, options etc. can be used. The returned strings
	 * should be thought of as the key to a map of possible
	 * approaches to handling MIDI within the backend. Ensure that
	 * the strings will make sense to the user.
	 */
	virtual std::vector<std::string> enumerate_midi_options () const = 0;

	/* Request the use of the MIDI option named \p option, which
	 * should be one of the strings returned by enumerate_midi_options()
	 *
	 * @return zero if successful, non-zero otherwise
	 */
	virtual int set_midi_option (const std::string& option) = 0;

	virtual std::string midi_option () const = 0;

	/** Detailed MIDI device list - if available */
	virtual std::vector<DeviceStatus> enumerate_midi_devices () const = 0;

	/** mark a midi-devices as enabled */
	virtual int set_midi_device_enabled (std::string const, bool) = 0;

	/** query if a midi-device is enabled */
	virtual bool midi_device_enabled (std::string const) const = 0;

	/** if backend supports systemic_midi_[in|ou]tput_latency() */
	virtual bool can_set_systemic_midi_latencies () const = 0;

	/* State Control */

	/** Start using the device named in the most recent call
	 * to set_device(), with the parameters set by various
	 * the most recent calls to set_sample_rate() etc. etc.
	 *
	 * At some undetermined time after this function is successfully called,
	 * the backend will start calling the process_callback method of
	 * the AudioEngine referenced by \ref engine. These calls will
	 * occur in a thread created by and/or under the control of the backend.
	 *
	 * @param for_latency_measurement if true, the device is being started
	 *        to carry out latency measurements and the backend should this
	 *        take care to return latency numbers that do not reflect
	 *        any existing systemic latency settings.
	 *
	 * Return zero if successful, negative values otherwise.
	 *
	 *
	 * Why is this non-virtual but \ref _start() is virtual ?
	 * Virtual methods with default parameters create possible ambiguity
	 * because a derived class may implement the same method with a different
	 * type or value of default parameter.
	 *
	 * So we make this non-virtual method to avoid possible overrides of
	 * default parameters. See Scott Meyers or other books on C++ to understand
	 * this pattern, or possibly just this:
	 *
	 * http://stackoverflow.com/questions/12139786/good-pratice-default-arguments-for-pure-virtual-method
	 */
	int start (bool for_latency_measurement = false)
	{
		return _start (for_latency_measurement);
	}

	/** Stop using the device currently in use.
	 *
	 * If the function is successfully called, no subsequent calls to the
	 * process_callback() of \ref engine will be made after the function
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

	/** Reset device.
	 *
	 * Return zero if successful, negative values on error
	 */
	virtual int reset_device () = 0;

	/** While remaining connected to the device, and without changing its
	 * configuration, start (or stop) calling the process_callback of the engine
	 * without waiting for the device. Once process_callback() has returned, it
	 * will be called again immediately, thus allowing for faster-than-realtime
	 * processing.
	 *
	 * All registered ports remain in existence and all connections remain
	 * unaltered. However, any physical ports should NOT be used by the
	 * process_callback() during freewheeling - the data behaviour is undefined.
	 *
	 * If \p start_stop is true, begin this behaviour; otherwise cease this
	 * behaviour if it currently occuring, and return to calling
	 * process_callback() of the engine by waiting for the device.
	 *
	 * @param start_stop true to engage freewheel processing
	 * @return zero on success, non-zero otherwise.
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
	virtual float dsp_load () const = 0;

	/* Transport Control (JACK is the only audio API that currently offers
	 * the concept of shared transport control)
	 */

	/** Attempt to change the transport state to TransportRolling.  */
	virtual void transport_start () {}

	/** Attempt to change the transport state to TransportStopped.  */
	virtual void transport_stop () {}

	/** return the current transport state */
	virtual TransportState transport_state () const
	{
		return TransportStopped;
	}

	/** Attempt to locate the transport to \p pos */
	virtual void transport_locate (samplepos_t pos) {}

	/** Return the current transport location, in samples measured
	 * from the origin (defined by the transport time master)
	 */
	virtual samplepos_t transport_sample () const
	{
		return 0;
	}

	/** If \p yn is true, become the time master for any inter-application transport
	 * timebase, otherwise cease to be the time master for the same.
	 *
	 * Return zero on success, non-zero otherwise
	 *
	 * JACK is the only currently known audio API with the concept of a shared
	 * transport timebase.
	 */
	virtual int set_time_master (bool yn)
	{
		return 0;
	}

	virtual int usecs_per_cycle () const
	{
		return 1000000 * (buffer_size () / sample_rate ());
	}
	virtual size_t raw_buffer_size (DataType t) = 0;

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
	virtual samplepos_t sample_time () = 0;

	/** Return the time according to the sample clock in use when the most
	 * recent buffer process cycle began. Can be called from any thread.
	 */
	virtual samplepos_t sample_time_at_cycle_start () = 0;

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
	 * set \p offset to that offset.
	 *
	 * Eg. if it can be determined that the first video frame within the cycle
	 * starts 28 samples after the first sample of the cycle, then this method
	 * should return true and set \p offset to 28.
	 *
	 * May be impossible to support outside of JACK, which has specific support
	 * (in some cases, hardware support) for this feature.
	 *
	 * Can ONLY be called from within a process() callback tree (which implies
	 * that it can only be called by a process thread)
	 */
	virtual bool get_sync_offset (pframes_t& offset) const
	{
		return false;
	}

	/** Create a new thread suitable for running part of the buffer process
	 * cycle (i.e. Realtime scheduling, memory allocation, stacksize, etc.
	 * are all correctly setup).
	 * The thread will begin executing func, and will exit
	 * when that function returns.
	 *
	 * @param func process function to run
	 */
	virtual int create_process_thread (boost::function<void()> func) = 0;

	/** Wait for all processing threads to exit.
	 *
	 * Return zero on success, non-zero on failure.
	 */
	virtual int join_process_threads () = 0;

	/** Return true if execution context is in a backend thread */
	virtual bool in_process_thread () = 0;

	/** Return the minimum stack size of audio threads in bytes */
	static size_t thread_stack_size ()
	{
		return 100000;
	}

	/** Return number of processing threads */
	virtual uint32_t process_thread_count () = 0;

	virtual void update_latencies () = 0;

	/** Set \p speed and \p position to the current speed and position
	 * indicated by some transport sync signal.  Return whether the current
	 * transport state is pending, or finalized.
	 *
	 * Derived classes only need implement this if they provide some way to
	 * sync to a transport sync signal (e.g. Sony 9 Pin) that is not
	 * handled by Ardour itself (LTC and MTC are both handled by Ardour).
	 * The canonical example is JACK Transport.
	 */
	virtual bool speed_and_position (double& speed, samplepos_t& position)
	{
		speed    = 0.0;
		position = 0;
		return false;
	}

	enum TimingTypes {
		DeviceWait = 0,
		RunLoop,
		/* end */
		NTT
	};

	PBD::TimingStats dsp_stats[NTT];

protected:
	AudioBackendInfo& _info;
	AudioEngine&      engine;

	virtual int _start (bool for_latency_measurement) = 0;
};

} // namespace ARDOUR

#endif /* __libardour_audiobackend_h__ */
