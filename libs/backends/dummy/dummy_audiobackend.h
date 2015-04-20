/*
 * Copyright (C) 2014 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2013 Paul Davis
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __libbackend_dummy_audiobackend_h__
#define __libbackend_dummy_audiobackend_h__

#include <string>
#include <vector>
#include <map>
#include <set>

#include <stdint.h>
#include <pthread.h>

#include <boost/shared_ptr.hpp>

#include "ardour/types.h"
#include "ardour/audio_backend.h"

namespace ARDOUR {

class DummyAudioBackend;

namespace DummyMidiData {
	typedef struct _MIDISequence {
		float   beat_time;
		uint8_t size;
		uint8_t event[3];
	} MIDISequence;
};

class DummyMidiEvent {
	public:
		DummyMidiEvent (const pframes_t timestamp, const uint8_t* data, size_t size);
		DummyMidiEvent (const DummyMidiEvent& other);
		~DummyMidiEvent ();
		size_t size () const { return _size; };
		pframes_t timestamp () const { return _timestamp; };
		const unsigned char* const_data () const { return _data; };
		unsigned char* data () { return _data; };
		bool operator< (const DummyMidiEvent &other) const { return timestamp () < other.timestamp (); };
	private:
		size_t _size;
		pframes_t _timestamp;
		uint8_t *_data;
};

typedef std::vector<boost::shared_ptr<DummyMidiEvent> > DummyMidiBuffer;

class DummyPort {
	protected:
		DummyPort (DummyAudioBackend &b, const std::string&, PortFlags);
	public:
		virtual ~DummyPort ();

		const std::string& name () const { return _name; }
		PortFlags flags () const { return _flags; }

		int set_name (const std::string &name) { _name = name; return 0; }

		virtual DataType type () const = 0;

		bool is_input ()     const { return flags () & IsInput; }
		bool is_output ()    const { return flags () & IsOutput; }
		bool is_physical ()  const { return flags () & IsPhysical; }
		bool is_terminal ()  const { return flags () & IsTerminal; }
		bool is_connected () const { return _connections.size () != 0; }
		bool is_connected (const DummyPort *port) const;
		bool is_physically_connected () const;

		const std::vector<DummyPort *>& get_connections () const { return _connections; }

		int connect (DummyPort *port);
		int disconnect (DummyPort *port);
		void disconnect_all ();

		virtual void* get_buffer (pframes_t nframes) = 0;
		void next_period () { _gen_cycle = false; }

		const LatencyRange latency_range (bool for_playback) const
		{
			return for_playback ? _playback_latency_range : _capture_latency_range;
		}

		void set_latency_range (const LatencyRange &latency_range, bool for_playback)
		{
			if (for_playback)
			{
				_playback_latency_range = latency_range;
			}
			else
			{
				_capture_latency_range = latency_range;
			}
		}

	private:
		DummyAudioBackend &_dummy_backend;
		std::string _name;
		const PortFlags _flags;
		LatencyRange _capture_latency_range;
		LatencyRange _playback_latency_range;
		std::vector<DummyPort*> _connections;

		void _connect (DummyPort* , bool);
		void _disconnect (DummyPort* , bool);

	protected:
		// random number generator
		void setup_random_number_generator ();
		inline float    randf ();
		inline uint32_t randi ();
		uint32_t _rseed;

		// signal generator
		volatile bool _gen_cycle;
		Glib::Threads::Mutex generator_lock;

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
			UniformWhiteNoise,
			GaussianWhiteNoise,
			PinkNoise,
			PonyNoise,
			SineWave,
			SquareWave,
			KronekerDelta,
			SineSweep,
			SineSweepSwell,
			SquareSweep,
			SquareSweepSwell,
			Loopback,
		};
		void setup_generator (GeneratorType const, float const);
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

}; // class DummyAudioPort

class DummyMidiPort : public DummyPort {
	public:
		DummyMidiPort (DummyAudioBackend &b, const std::string&, PortFlags);
		~DummyMidiPort ();

		DataType type () const { return DataType::MIDI; };

		void* get_buffer (pframes_t nframes);
		const DummyMidiBuffer * const_buffer () const { return &_buffer; }

		void setup_generator (int, float const);
		void set_loopback (DummyMidiBuffer const * const src);

	private:
		DummyMidiBuffer _buffer;
		DummyMidiBuffer _loopback;

		// midi event generator ('fake' physical inputs)
		void midi_generate (const pframes_t n_samples);
		float   _midi_seq_spb; // samples per beat
		int32_t _midi_seq_time;
		uint32_t _midi_seq_pos;
		DummyMidiData::MIDISequence const * _midi_seq_dat;
}; // class DummyMidiPort

class DummyAudioBackend : public AudioBackend {
	friend class DummyPort;
	public:
	         DummyAudioBackend (AudioEngine& e, AudioBackendInfo& info);
		~DummyAudioBackend ();

		bool is_running () const { return _running; }

		/* AUDIOBACKEND API */

		std::string name () const;
		bool is_realtime () const;

		std::vector<DeviceStatus> enumerate_devices () const;
		std::vector<float> available_sample_rates (const std::string& device) const;
		std::vector<uint32_t> available_buffer_sizes (const std::string& device) const;
		uint32_t available_input_channel_count (const std::string& device) const;
		uint32_t available_output_channel_count (const std::string& device) const;

		bool can_change_sample_rate_when_running () const;
		bool can_change_buffer_size_when_running () const;

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
		framepos_t sample_time ();
		framepos_t sample_time_at_cycle_start ();
		pframes_t samples_since_cycle_start ();

		int create_process_thread (boost::function<void()> func);
		int join_process_threads ();
		bool in_process_thread ();
		uint32_t process_thread_count ();

		void update_latencies ();

		/* PORTENGINE API */

		void* private_handle () const;
		const std::string& my_name () const;
		bool available () const;
		uint32_t port_name_size () const;

		int         set_port_name (PortHandle, const std::string&);
		std::string get_port_name (PortHandle) const;
		PortHandle  get_port_by_name (const std::string&) const;

		int get_ports (const std::string& port_name_pattern, DataType type, PortFlags flags, std::vector<std::string>&) const;

		DataType port_data_type (PortHandle) const;

		PortHandle register_port (const std::string& shortname, ARDOUR::DataType, ARDOUR::PortFlags);
		void unregister_port (PortHandle);

		int  connect (const std::string& src, const std::string& dst);
		int  disconnect (const std::string& src, const std::string& dst);
		int  connect (PortHandle, const std::string&);
		int  disconnect (PortHandle, const std::string&);
		int  disconnect_all (PortHandle);

		bool connected (PortHandle, bool process_callback_safe);
		bool connected_to (PortHandle, const std::string&, bool process_callback_safe);
		bool physically_connected (PortHandle, bool process_callback_safe);
		int  get_connections (PortHandle, std::vector<std::string>&, bool process_callback_safe);

		/* MIDI */
		int midi_event_get (pframes_t& timestamp, size_t& size, uint8_t** buf, void* port_buffer, uint32_t event_index);
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

		/* Discovering physical ports */

		bool      port_is_physical (PortHandle) const;
		void      get_physical_outputs (DataType type, std::vector<std::string>&);
		void      get_physical_inputs (DataType type, std::vector<std::string>&);
		ChanCount n_physical_outputs () const;
		ChanCount n_physical_inputs () const;

		/* Getting access to the data buffer for a port */

		void* get_buffer (PortHandle, pframes_t);

		void* main_process_thread ();

		static size_t max_buffer_size() {return _max_buffer_size;}

	private:
		enum MidiPortMode {
			MidiNoEvents,
			MidiGenerator,
			MidiLoopback,
			MidiToAudio,
		};

		std::string _instance_name;
		static std::vector<std::string> _midi_options;
		static std::vector<AudioBackend::DeviceStatus> _device_status;

		bool  _running;
		bool  _freewheel;
		bool  _freewheeling;

		std::string _device;

		float  _samplerate;
		size_t _samples_per_period;
		float  _dsp_load;
		static size_t _max_buffer_size;

		uint32_t _n_inputs;
		uint32_t _n_outputs;

		uint32_t _n_midi_inputs;
		uint32_t _n_midi_outputs;
		MidiPortMode _midi_mode;

		uint32_t _systemic_input_latency;
		uint32_t _systemic_output_latency;

		framecnt_t _processed_samples;

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
		PortHandle add_port (const std::string& shortname, ARDOUR::DataType, ARDOUR::PortFlags);
		int register_system_ports ();
		void unregister_ports (bool system_only = false);

		std::vector<DummyAudioPort *> _system_inputs;
		std::vector<DummyAudioPort *> _system_outputs;
		std::vector<DummyMidiPort *> _system_midi_in;
		std::vector<DummyMidiPort *> _system_midi_out;
		std::vector<DummyPort *> _ports;

		struct PortConnectData {
			std::string a;
			std::string b;
			bool c;

			PortConnectData (const std::string& a, const std::string& b, bool c)
				: a (a) , b (b) , c (c) {}
		};

		std::vector<PortConnectData *> _port_connection_queue;
		pthread_mutex_t _port_callback_mutex;
		bool _port_change_flag;

		void port_connect_callback (const std::string& a, const std::string& b, bool conn) {
			pthread_mutex_lock (&_port_callback_mutex);
			_port_connection_queue.push_back(new PortConnectData(a, b, conn));
			pthread_mutex_unlock (&_port_callback_mutex);
		}

		void port_connect_add_remove_callback () {
			pthread_mutex_lock (&_port_callback_mutex);
			_port_change_flag = true;
			pthread_mutex_unlock (&_port_callback_mutex);
		}

		bool valid_port (PortHandle port) const {
			return std::find (_ports.begin (), _ports.end (), (DummyPort*)port) != _ports.end ();
		}

		DummyPort * find_port (const std::string& port_name) const {
			for (std::vector<DummyPort*>::const_iterator it = _ports.begin (); it != _ports.end (); ++it) {
				if ((*it)->name () == port_name) {
					return *it;
				}
			}
			return NULL;
		}

}; // class DummyAudioBackend

} // namespace

#endif /* __libbackend_dummy_audiobackend_h__ */
