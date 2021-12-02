/*
 * Copyright (C) 2015-2016 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2015-2018 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2017-2018 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __libbackend_portaudio_backend_h__
#define __libbackend_portaudio_backend_h__

#include <string>
#include <vector>
#include <set>

#include <stdint.h>
#include <pthread.h>

#include <boost/shared_ptr.hpp>

#include "ardour/audio_backend.h"
#include "ardour/dsp_load_calculator.h"
#include "ardour/port_engine_shared.h"
#include "ardour/types.h"

#include "portaudio_io.h"
#include "winmmemidi_io.h"
#include "cycle_timer.h"

namespace ARDOUR {

class PortAudioBackend;

class PortMidiEvent : public BackendMIDIEvent {
	public:
		PortMidiEvent (const pframes_t timestamp, const uint8_t* data, size_t size);
		PortMidiEvent (const PortMidiEvent& other);
		size_t size () const { return _size; };
		pframes_t timestamp () const { return _timestamp; };
		const uint8_t* data () const { return _data; };
	private:
		size_t _size;
		pframes_t _timestamp;
		uint8_t _data[MaxWinMidiEventSize];
};

typedef std::vector<PortMidiEvent> PortMidiBuffer;

class PortAudioPort : public BackendPort {
	public:
		PortAudioPort (PortAudioBackend &b, const std::string&, PortFlags);
		~PortAudioPort ();

		DataType type () const { return DataType::AUDIO; };

		Sample* buffer () { return _buffer; }
		const Sample* const_buffer () const { return _buffer; }
		void* get_buffer (pframes_t nframes);

	private:
		Sample _buffer[8192];
}; // class PortAudioPort

class PortMidiPort : public BackendPort {
	public:
		PortMidiPort (PortAudioBackend &b, const std::string&, PortFlags);
		~PortMidiPort ();

		DataType type () const { return DataType::MIDI; };

		void* get_buffer (pframes_t nframes);
		const PortMidiBuffer * const_buffer () const { return & _buffer[_bufperiod]; }

		void next_period() { if (_n_periods > 1) { get_buffer(0); _bufperiod = (_bufperiod + 1) % _n_periods; } }
		void set_n_periods(int n) { if (n > 0 && n < 3) { _n_periods = n; } }

	private:
		PortMidiBuffer _buffer[2];
		int _n_periods;
		int _bufperiod;
}; // class PortMidiPort

class PortAudioBackend : public AudioBackend, public PortEngineSharedImpl {
	public:
		PortAudioBackend (AudioEngine& e, AudioBackendInfo& info);
		~PortAudioBackend ();

		/* AUDIOBACKEND API */

		std::string name () const;
		bool is_realtime () const;

		bool requires_driver_selection() const;
		std::string driver_name () const;
		std::vector<std::string> enumerate_drivers () const;
		int set_driver (const std::string&);

		bool can_request_update_devices () { return true; }
		bool update_devices ();

		bool can_use_buffered_io () { return true; }
		void set_use_buffered_io (bool);
		bool get_use_buffered_io () { return _use_blocking_api; }

		bool use_separate_input_and_output_devices () const;
		std::vector<DeviceStatus> enumerate_devices () const;
		std::vector<DeviceStatus> enumerate_input_devices () const;
		std::vector<DeviceStatus> enumerate_output_devices () const;

		std::vector<float> available_sample_rates (const std::string& device) const;
		std::vector<uint32_t> available_buffer_sizes (const std::string& device) const;
		uint32_t available_input_channel_count (const std::string& device) const;
		uint32_t available_output_channel_count (const std::string& device) const;

		bool can_change_sample_rate_when_running () const;
		bool can_change_buffer_size_when_running () const;

		int set_device_name (const std::string&);
		int set_input_device_name (const std::string&);
		int set_output_device_name (const std::string&);
		int set_sample_rate (float);
		int set_buffer_size (uint32_t);
		int set_interleaved (bool yn);
		int set_input_channels (uint32_t);
		int set_output_channels (uint32_t);
		int set_systemic_input_latency (uint32_t);
		int set_systemic_output_latency (uint32_t);
		int set_systemic_midi_input_latency (std::string const, uint32_t);
		int set_systemic_midi_output_latency (std::string const, uint32_t);

		int reset_device () { return 0; };

		/* Retrieving parameters */
		std::string  device_name () const;
		std::string  input_device_name () const;
		std::string  output_device_name () const;
		float        sample_rate () const;
		uint32_t     buffer_size () const;
		bool         interleaved () const;
		uint32_t     input_channels () const;
		uint32_t     output_channels () const;
		uint32_t     systemic_input_latency () const;
		uint32_t     systemic_output_latency () const;
		uint32_t     systemic_midi_input_latency (std::string const) const;
		uint32_t     systemic_midi_output_latency (std::string const) const;

		bool can_measure_systemic_latency () const { return true; }
		bool can_set_systemic_midi_latencies () const { return true; }

		/* External control app */
		std::string control_app_name () const;
		void launch_control_app ();

		/* MIDI */
		std::vector<std::string> enumerate_midi_options () const;
		int set_midi_option (const std::string&);
		std::string midi_option () const;

		std::vector<DeviceStatus> enumerate_midi_devices () const;
		int set_midi_device_enabled (std::string const, bool);
		bool midi_device_enabled (std::string const) const;

	protected:
		/* State Control */
		int _start (bool for_latency_measurement);
	public:
		int stop ();
		int freewheel (bool);
		float dsp_load () const;
		size_t raw_buffer_size (DataType t);

		/* Process time */
		samplepos_t sample_time ();
		samplepos_t sample_time_at_cycle_start ();
		pframes_t samples_since_cycle_start ();

		int create_process_thread (boost::function<void()> func);
		int join_process_threads ();
		bool in_process_thread ();
		uint32_t process_thread_count ();

		void update_latencies ();

		/* PORTENGINE API */

		void* private_handle () const;
		const std::string& my_name () const;


		/* PortEngine API - forwarded to PortEngineSharedImpl */

	bool        port_is_physical (PortEngine::PortHandle ph) const { return PortEngineSharedImpl::port_is_physical (ph); }
	void        get_physical_outputs (DataType type, std::vector<std::string>& results) { PortEngineSharedImpl::get_physical_outputs (type, results); }
	void        get_physical_inputs (DataType type, std::vector<std::string>& results) { PortEngineSharedImpl::get_physical_inputs (type, results); }
	ChanCount   n_physical_outputs () const { return PortEngineSharedImpl::n_physical_outputs (); }
	ChanCount   n_physical_inputs () const { return PortEngineSharedImpl::n_physical_inputs (); }
	uint32_t    port_name_size () const { return PortEngineSharedImpl::port_name_size(); }
	int         set_port_name (PortEngine::PortHandle ph, const std::string& name) { return PortEngineSharedImpl::set_port_name (ph, name); }
	std::string get_port_name (PortEngine::PortHandle ph) const { return PortEngineSharedImpl::get_port_name (ph); }
	PortFlags   get_port_flags (PortEngine::PortHandle ph) const { return PortEngineSharedImpl::get_port_flags (ph); }
	PortEngine::PortPtr  get_port_by_name (std::string const & name) const { return PortEngineSharedImpl::get_port_by_name (name); }
	int         get_port_property (PortEngine::PortHandle ph, const std::string& key, std::string& value, std::string& type) const { return PortEngineSharedImpl::get_port_property (ph, key, value, type); }
	int         set_port_property (PortEngine::PortHandle ph, const std::string& key, const std::string& value, const std::string& type) { return PortEngineSharedImpl::set_port_property (ph, key, value, type); }
	int         get_ports (const std::string& port_name_pattern, DataType type, PortFlags flags, std::vector<std::string>& results) const { return PortEngineSharedImpl::get_ports (port_name_pattern, type, flags, results); }
	DataType    port_data_type (PortEngine::PortHandle ph) const { return PortEngineSharedImpl::port_data_type (ph); }
	PortEngine::PortPtr register_port (const std::string& shortname, ARDOUR::DataType type, ARDOUR::PortFlags flags) { return PortEngineSharedImpl::register_port (shortname, type, flags); }
	void        unregister_port (PortHandle ph) { if (!_run) return; PortEngineSharedImpl::unregister_port (ph); }
	int         connect (const std::string& src, const std::string& dst) { return PortEngineSharedImpl::connect (src, dst); }
	int         disconnect (const std::string& src, const std::string& dst) { return PortEngineSharedImpl::disconnect (src, dst); }
	int         connect (PortEngine::PortHandle ph, const std::string& other) { return PortEngineSharedImpl::connect (ph, other); }
	int         disconnect (PortEngine::PortHandle ph, const std::string& other) { return PortEngineSharedImpl::disconnect (ph, other); }
	int         disconnect_all (PortEngine::PortHandle ph) { return PortEngineSharedImpl::disconnect_all (ph); }
	bool        connected (PortEngine::PortHandle ph, bool process_callback_safe) { return PortEngineSharedImpl::connected (ph, process_callback_safe); }
	bool        connected_to (PortEngine::PortHandle ph, const std::string& other, bool process_callback_safe) { return PortEngineSharedImpl::connected_to (ph, other, process_callback_safe); }
	bool        physically_connected (PortEngine::PortHandle ph, bool process_callback_safe) { return PortEngineSharedImpl::physically_connected (ph, process_callback_safe); }
	int         get_connections (PortEngine::PortHandle ph, std::vector<std::string>& results, bool process_callback_safe) { return PortEngineSharedImpl::get_connections (ph, results, process_callback_safe); }

		/* MIDI */
		int midi_event_get (pframes_t& timestamp, size_t& size, uint8_t const** buf, void* port_buffer, uint32_t event_index);
		int midi_event_put (void* port_buffer, pframes_t timestamp, const uint8_t* buffer, size_t size);
		uint32_t get_midi_event_count (void* port_buffer);
		void     midi_clear (void* port_buffer);

		/* Monitoring */

		bool can_monitor_input () const;
		int  request_input_monitoring (PortHandle, bool);
		int  ensure_input_monitoring (PortHandle, bool);
		bool monitoring_input (PortHandle);

		/* Latency management */

		void         set_latency_range (PortHandle, bool for_playback, LatencyRange);
		LatencyRange get_latency_range (PortHandle, bool for_playback);

		/* Getting access to the data buffer for a port */

		void* get_buffer (PortHandle, pframes_t);

		void* blocking_process_thread ();

		void* freewheel_process_thread ();

	private: // Methods
		bool start_blocking_process_thread ();
		bool stop_blocking_process_thread ();
		bool blocking_process_freewheel ();
		bool blocking_process_main (const float* interleaved_input_data,
		                            float* interleaved_output_data);

		void process_port_connection_changes ();
		void process_incoming_midi ();
		void process_outgoing_midi ();

		bool engine_halted ();
		bool running ();

		static int portaudio_callback(const void* input,
	                                  void* output,
	                                  unsigned long frameCount,
	                                  const PaStreamCallbackTimeInfo* timeInfo,
	                                  PaStreamCallbackFlags statusFlags,
	                                  void* userData);

		bool process_callback(const float* input,
	                          float* output,
	                          uint32_t sample_count,
	                          const PaStreamCallbackTimeInfo* timeInfo,
	                          PaStreamCallbackFlags statusFlags);

		bool start_freewheel_process_thread ();
		bool stop_freewheel_process_thread ();

		static bool set_mmcss_pro_audio (HANDLE* task_handle);
		static bool reset_mmcss (HANDLE task_handle);

	private:
		std::string _instance_name;
		PortAudioIO *_pcmio;
		WinMMEMidiIO *_midiio;

		bool  _run; /* keep going or stop, ardour thread */
		bool  _active; /* is running, process thread */
		bool  _use_blocking_api;
		bool  _freewheel;
		bool  _freewheeling;
		bool  _freewheel_ack;
		bool  _reinit_thread_callback;
		bool  _measure_latency;

		ARDOUR::DSPLoadCalculator _dsp_calc;

		bool _freewheel_thread_active;

		pthread_mutex_t _freewheel_mutex;
		pthread_cond_t _freewheel_signal;

		uint64_t _cycle_count;
		uint64_t _total_deviation_us;
		uint64_t _max_deviation_us;

		CycleTimer _cycle_timer;
		uint64_t _last_cycle_start;

		static std::vector<std::string> _midi_options;
		static std::vector<AudioBackend::DeviceStatus> _input_audio_device_status;
		static std::vector<AudioBackend::DeviceStatus> _output_audio_device_status;
		static std::vector<AudioBackend::DeviceStatus> _midi_device_status;

		mutable std::string _input_audio_device;
		mutable std::string _output_audio_device;
		std::string _midi_driver_option;

		/* audio settings */
		float  _samplerate;
		size_t _samples_per_period;
		static size_t _max_buffer_size;

		uint32_t _n_inputs;
		uint32_t _n_outputs;

		uint32_t _systemic_audio_input_latency;
		uint32_t _systemic_audio_output_latency;

		MidiDeviceInfo* midi_device_info(const std::string&) const;

		/* portaudio specific  */
		int name_to_id(std::string) const;

		/* processing */
		float  _dsp_load;
		samplecnt_t _processed_samples;

		/* blocking thread */
		pthread_t _main_blocking_thread;

		/* main thread in callback mode(or fw thread when running) */
		pthread_t _main_thread;

		/* freewheel thread in callback mode */
		pthread_t _pthread_freewheel;

		/* process threads */
		static void* portaudio_process_thread (void *);
		std::vector<pthread_t> _threads;

		struct ThreadData {
			PortAudioBackend* engine;
			boost::function<void ()> f;
			size_t stacksize;

			ThreadData (PortAudioBackend* e, boost::function<void ()> fp, size_t stacksz)
				: engine (e) , f (fp) , stacksize (stacksz) {}
		};

		/* port engine */
		BackendPort* port_factory (std::string const & name, ARDOUR::DataType dt, ARDOUR::PortFlags flags);

		int register_system_audio_ports ();
		int register_system_midi_ports ();

}; // class PortAudioBackend

} // namespace

#endif /* __libbackend_portaudio_backend_h__ */
