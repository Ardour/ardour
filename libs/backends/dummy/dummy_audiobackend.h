/*
 * Copyright (C) 2014-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2018 Robin Gareus <robin@gareus.org>
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

#ifndef __libbackend_dummy_audiobackend_h__
#define __libbackend_dummy_audiobackend_h__

#include <string>
#include <vector>
#include <map>
#include <set>

#include <stdint.h>
#include <pthread.h>

#include <ltc.h>

#include <boost/shared_ptr.hpp>

#include "pbd/natsort.h"
#include "pbd/ringbuffer.h"
#include "ardour/types.h"
#include "ardour/audio_backend.h"
#include "ardour/dsp_load_calculator.h"
#include "ardour/port_engine_shared.h"

namespace ARDOUR {

class DummyAudioBackend;

namespace DummyMidiData {
	typedef struct _MIDISequence {
		float   beat_time;
		uint8_t size;
		uint8_t event[3];
	} MIDISequence;
};


class DummyMidiEvent : public BackendMIDIEvent {
	public:
		DummyMidiEvent (const pframes_t timestamp, const uint8_t* data, size_t size);
		DummyMidiEvent (const DummyMidiEvent& other);
		~DummyMidiEvent ();
		size_t size () const { return _size; };
		pframes_t timestamp () const { return _timestamp; };
		const uint8_t* data () const { return _data; };
	private:
		size_t _size;
		pframes_t _timestamp;
		uint8_t *_data;
};

typedef std::vector<boost::shared_ptr<DummyMidiEvent> > DummyMidiBuffer;

class DummyPort : public BackendPort {
	protected:
		DummyPort (DummyAudioBackend &b, const std::string&, PortFlags);
	public:
		virtual ~DummyPort ();

		void next_period () { _gen_cycle = false; }

	protected:
		/* random number generator */
		void setup_random_number_generator ();
		inline float    randf ();
		inline uint32_t randi ();
		uint32_t _rseed;
		/* engine time */
		pframes_t pulse_position () const;

		// signal generator
		volatile bool _gen_cycle;
		Glib::Threads::Mutex generator_lock;

        private:
		AudioBackend& _engine;

}; // class DummyPort

class DummyAudioPort : public DummyPort {
	public:
		DummyAudioPort (DummyAudioBackend &b, const std::string&, PortFlags);
		~DummyAudioPort ();

		DataType type () const { return DataType::AUDIO; };

		Sample* buffer () { return _buffer; }
		const Sample* const_buffer () const { return _buffer; }
		void* get_buffer (pframes_t nframes);

		enum GeneratorType {
			Silence,
			DC05,
			Demolition,
			UniformWhiteNoise,
			GaussianWhiteNoise,
			PinkNoise,
			PonyNoise,
			SineWave,
			SineWaveOctaves,
			SquareWave,
			KronekerDelta,
			SineSweep,
			SineSweepSwell,
			SquareSweep,
			SquareSweepSwell,
			OneHz,
			LTC,
			Loopback,
		};
		std::string setup_generator (GeneratorType const, float const, int, int);
		void fill_wavetable (const float* d, size_t n_samples) { assert(_wavetable != 0);  memcpy(_wavetable, d, n_samples * sizeof(float)); }
		void midi_to_wavetable (DummyMidiBuffer const * const src, size_t n_samples);

	private:
		Sample _buffer[8192];

		// signal generator ('fake' physical inputs)
		void generate (const pframes_t n_samples);
		GeneratorType _gen_type;

		// generator buffers
		// pink-noise filters
		float _b0, _b1, _b2, _b3, _b4, _b5, _b6;
		// generated sinf() samples
		Sample * _wavetable;
		uint32_t _gen_period;
		uint32_t _gen_offset;
		uint32_t _gen_perio2;
		uint32_t _gen_count2;

		// gaussian noise generator
		float grandf ();
		bool _pass;
		float _rn1;
		// LTC generator
		LTCEncoder* _ltc;
		PBD::RingBuffer<Sample>* _ltcbuf;
		float _ltc_spd;
		float _ltc_rand;


}; // class DummyAudioPort

class DummyMidiPort : public DummyPort {
	public:
		DummyMidiPort (DummyAudioBackend &b, const std::string&, PortFlags);
		~DummyMidiPort ();

		DataType type () const { return DataType::MIDI; };

		void* get_buffer (pframes_t nframes);
		const DummyMidiBuffer * const_buffer () const { return &_buffer; }

		std::string setup_generator (int, float const);
		void set_loopback (DummyMidiBuffer const * const src);

	private:
		DummyMidiBuffer _buffer;
		DummyMidiBuffer _loopback;

		// midi event generator ('fake' physical inputs)
		void midi_generate (const pframes_t n_samples);
		float   _midi_seq_spb; // samples per beat
		int64_t _midi_seq_time;
		uint32_t _midi_seq_pos;
		DummyMidiData::MIDISequence const * _midi_seq_dat;
}; // class DummyMidiPort

class DummyAudioBackend : public AudioBackend, public PortEngineSharedImpl {
	public:
	         DummyAudioBackend (AudioEngine& e, AudioBackendInfo& info);
		~DummyAudioBackend ();

		bool is_running () const { return _running; }

		/* AUDIOBACKEND API */

		std::string name () const;
		bool is_realtime () const;

		bool requires_driver_selection() const { return true; }
		std::string driver_name () const;
		std::vector<std::string> enumerate_drivers () const;
		int set_driver (const std::string&);

		std::vector<DeviceStatus> enumerate_devices () const;
		std::vector<float> available_sample_rates (const std::string& device) const;
		std::vector<uint32_t> available_buffer_sizes (const std::string& device) const;
		uint32_t available_input_channel_count (const std::string& device) const;
		uint32_t available_output_channel_count (const std::string& device) const;

		bool can_change_sample_rate_when_running () const;
		bool can_change_buffer_size_when_running () const;
		bool can_measure_systemic_latency () const { return true; }

		int set_device_name (const std::string&);
		int set_sample_rate (float);
		int set_buffer_size (uint32_t);
		int set_interleaved (bool yn);
		int set_input_channels (uint32_t);
		int set_output_channels (uint32_t);
		int set_systemic_input_latency (uint32_t);
		int set_systemic_output_latency (uint32_t);
		int set_systemic_midi_input_latency (std::string const, uint32_t) { return 0; }
		int set_systemic_midi_output_latency (std::string const, uint32_t) { return 0; }

		int reset_device () { return 0; };

		/* Retrieving parameters */
		std::string  device_name () const;
		float        sample_rate () const;
		uint32_t     buffer_size () const;
		bool         interleaved () const;
		uint32_t     input_channels () const;
		uint32_t     output_channels () const;
		uint32_t     systemic_input_latency () const;
		uint32_t     systemic_output_latency () const;
		uint32_t     systemic_midi_input_latency (std::string const) const { return 0; }
		uint32_t     systemic_midi_output_latency (std::string const) const { return 0; }

		/* External control app */
		std::string control_app_name () const { return std::string (); }
		void launch_control_app () {}

		/* MIDI */
		std::vector<std::string> enumerate_midi_options () const;
		int set_midi_option (const std::string&);
		std::string midi_option () const;

		std::vector<DeviceStatus> enumerate_midi_devices () const {
			return std::vector<AudioBackend::DeviceStatus> ();
		}
		int set_midi_device_enabled (std::string const, bool) {
			return 0;
		}
		bool midi_device_enabled (std::string const) const {
			return true;
		}
		bool can_set_systemic_midi_latencies () const {
			return false;
		}

		/* State Control */
	protected:
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
	void        unregister_port (PortHandle ph) { if (!_running) return; PortEngineSharedImpl::unregister_port (ph); }
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

		void* main_process_thread ();

		static size_t max_buffer_size() {return _max_buffer_size;}

	private:
		enum MidiPortMode {
			MidiNoEvents,
			MidiGenerator,
			MidiOneHz,
			MidiLoopback,
			MidiToAudio,
		};

		struct DriverSpeed {
			std::string name;
			float speedup;
			DriverSpeed (const std::string& n, float s) : name (n), speedup (s) {}
		};

		std::string _instance_name;
		static std::vector<std::string> _midi_options;
		static std::vector<AudioBackend::DeviceStatus> _device_status;
		static std::vector<DummyAudioBackend::DriverSpeed> _driver_speed;

		bool  _running;
		bool  _freewheel;
		bool  _freewheeling;
		float _speedup;

		std::string _device;

		float  _samplerate;
		size_t _samples_per_period;
		float  _dsp_load;
		DSPLoadCalculator _dsp_load_calc;
		static size_t _max_buffer_size;

		uint32_t _n_inputs;
		uint32_t _n_outputs;

		uint32_t _n_midi_inputs;
		uint32_t _n_midi_outputs;
		MidiPortMode _midi_mode;

		uint32_t _systemic_input_latency;
		uint32_t _systemic_output_latency;

		samplecnt_t _processed_samples;

		pthread_t _main_thread;

		/* process threads */
		static void* dummy_process_thread (void *);
		std::vector<pthread_t> _threads;

		struct ThreadData {
			DummyAudioBackend* engine;
			boost::function<void ()> f;
			size_t stacksize;

			ThreadData (DummyAudioBackend* e, boost::function<void ()> fp, size_t stacksz)
				: engine (e) , f (fp) , stacksize (stacksz) {}
		};

		/* port engine */
		int register_system_ports ();

	BackendPort* port_factory (std::string const & name, ARDOUR::DataType type, ARDOUR::PortFlags);

}; // class DummyAudioBackend

} // namespace

#endif /* __libbackend_dummy_audiobackend_h__ */
