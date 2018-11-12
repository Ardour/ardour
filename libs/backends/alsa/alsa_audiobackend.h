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

#ifndef __libbackend_alsa_audiobackend_h__
#define __libbackend_alsa_audiobackend_h__

#include <string>
#include <vector>
#include <map>
#include <set>

#include <stdint.h>
#include <pthread.h>

#include <boost/shared_ptr.hpp>

#include "pbd/natsort.h"
#include "ardour/audio_backend.h"
#include "ardour/dsp_load_calculator.h"
#include "ardour/system_exec.h"
#include "ardour/types.h"

#include "ardouralsautil/deviceinfo.h"

#include "zita-alsa-pcmi.h"
#include "alsa_rawmidi.h"
#include "alsa_sequencer.h"
#include "alsa_slave.h"

namespace ARDOUR {

class AlsaAudioBackend;

class AlsaMidiEvent {
	public:
		AlsaMidiEvent (const pframes_t timestamp, const uint8_t* data, size_t size);
		AlsaMidiEvent (const AlsaMidiEvent& other);
		size_t size () const { return _size; };
		pframes_t timestamp () const { return _timestamp; };
		const uint8_t* data () const { return _data; };
		bool operator< (const AlsaMidiEvent &other) const { return timestamp () < other.timestamp (); };
	private:
		size_t _size;
		pframes_t _timestamp;
		uint8_t _data[MaxAlsaMidiEventSize];
};

typedef std::vector<AlsaMidiEvent> AlsaMidiBuffer;

class AlsaPort {
	protected:
		AlsaPort (AlsaAudioBackend &b, const std::string&, PortFlags);
	public:
		virtual ~AlsaPort ();

		const std::string& name () const { return _name; }
		const std::string& pretty_name () const { return _pretty_name; }
		PortFlags flags () const { return _flags; }

		int set_name (const std::string &name) { _name = name; return 0; }
		int set_pretty_name (const std::string &name) { _pretty_name = name; return 0; }

		virtual DataType type () const = 0;

		bool is_input ()     const { return flags () & IsInput; }
		bool is_output ()    const { return flags () & IsOutput; }
		bool is_physical ()  const { return flags () & IsPhysical; }
		bool is_terminal ()  const { return flags () & IsTerminal; }
		bool is_connected () const { return _connections.size () != 0; }
		bool is_connected (const AlsaPort *port) const;
		bool is_physically_connected () const;

		const std::set<AlsaPort *>& get_connections () const { return _connections; }

		int connect (AlsaPort *port);
		int disconnect (AlsaPort *port);
		void disconnect_all ();

		virtual void* get_buffer (pframes_t nframes) = 0;

		const LatencyRange latency_range (bool for_playback) const
		{
			return for_playback ? _playback_latency_range : _capture_latency_range;
		}

		void set_latency_range (const LatencyRange &latency_range, bool for_playback);

		void update_connected_latency (bool for_playback);

	private:
		AlsaAudioBackend &_alsa_backend;
		std::string _name;
		std::string _pretty_name;
		const PortFlags _flags;
		LatencyRange _capture_latency_range;
		LatencyRange _playback_latency_range;
		std::set<AlsaPort*> _connections;

		void _connect (AlsaPort* , bool);
		void _disconnect (AlsaPort* , bool);

}; // class AlsaPort

class AlsaAudioPort : public AlsaPort {
	public:
		AlsaAudioPort (AlsaAudioBackend &b, const std::string&, PortFlags);
		~AlsaAudioPort ();

		DataType type () const { return DataType::AUDIO; };

		Sample* buffer () { return _buffer; }
		const Sample* const_buffer () const { return _buffer; }
		void* get_buffer (pframes_t nframes);

	private:
		Sample _buffer[8192];
}; // class AlsaAudioPort

class AlsaMidiPort : public AlsaPort {
	public:
		AlsaMidiPort (AlsaAudioBackend &b, const std::string&, PortFlags);
		~AlsaMidiPort ();

		DataType type () const { return DataType::MIDI; };

		void* get_buffer (pframes_t nframes);
		const AlsaMidiBuffer * const_buffer () const { return & _buffer[_bufperiod]; }

		void next_period() { if (_n_periods > 1) { get_buffer(0); _bufperiod = (_bufperiod + 1) % _n_periods; } }
		void set_n_periods(int n) { if (n > 0 && n < 4) { _n_periods = n; } }

	private:
		AlsaMidiBuffer _buffer[3];
		int _n_periods;
		int _bufperiod;
}; // class AlsaMidiPort

class AlsaDeviceReservation
{
	public:
		AlsaDeviceReservation ();
		AlsaDeviceReservation (const char* device_name);
		~AlsaDeviceReservation ();

		bool acquire_device (const char* device_name);
		void release_device ();

	private:
		ARDOUR::SystemExec* _device_reservation;
		PBD::ScopedConnectionList _reservation_connection;
		void reservation_stdout (std::string, size_t);
		bool _reservation_succeeded;
};

class AlsaAudioBackend : public AudioBackend {
	friend class AlsaPort;
	public:
		AlsaAudioBackend (AudioEngine& e, AudioBackendInfo& info);
		~AlsaAudioBackend ();

		/* AUDIOBACKEND API */

		std::string name () const;
		bool is_realtime () const;

		bool use_separate_input_and_output_devices () const { return true; }
		bool match_input_output_devices_or_none () const { return true; }
		bool can_set_period_size () const { return true; }

		std::vector<DeviceStatus> enumerate_devices () const;
		std::vector<DeviceStatus> enumerate_input_devices () const;
		std::vector<DeviceStatus> enumerate_output_devices () const;
		std::vector<float> available_sample_rates (const std::string& device) const;
		std::vector<float> available_sample_rates2 (const std::string&, const std::string&) const;
		std::vector<uint32_t> available_buffer_sizes (const std::string& device) const;
		std::vector<uint32_t> available_buffer_sizes2 (const std::string&, const std::string&) const;
		std::vector<uint32_t> available_period_sizes (const std::string& driver) const;
		uint32_t available_input_channel_count (const std::string& device) const;
		uint32_t available_output_channel_count (const std::string& device) const;

		bool can_change_sample_rate_when_running () const;
		bool can_change_buffer_size_when_running () const;

		bool can_change_systemic_latency_when_running () const { return true; }

		int set_device_name (const std::string&);
		int set_input_device_name (const std::string&);
		int set_output_device_name (const std::string&);
		int set_sample_rate (float);
		int set_buffer_size (uint32_t);
		int set_peridod_size (uint32_t);
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
		uint32_t     period_size () const;
		bool         interleaved () const;
		uint32_t     input_channels () const;
		uint32_t     output_channels () const;
		uint32_t     systemic_input_latency () const;
		uint32_t     systemic_output_latency () const;
		uint32_t     systemic_midi_input_latency (std::string const) const;
		uint32_t     systemic_midi_output_latency (std::string const) const;

		bool can_set_systemic_midi_latencies () const { return true; }

		/* External control app */
		std::string control_app_name () const { return std::string (); }
		void launch_control_app () {}

		/* MIDI */
		std::vector<std::string> enumerate_midi_options () const;
		int set_midi_option (const std::string&);
		std::string midi_option () const;

		std::vector<DeviceStatus> enumerate_midi_devices () const;
		int set_midi_device_enabled (std::string const, bool);
		bool midi_device_enabled (std::string const) const;

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
		bool available () const;
		uint32_t port_name_size () const;

		int         set_port_name (PortHandle, const std::string&);
		std::string get_port_name (PortHandle) const;
		PortHandle  get_port_by_name (const std::string&) const;

		int get_port_property (PortHandle, const std::string& key, std::string& value, std::string& type) const;
		int set_port_property (PortHandle, const std::string& key, const std::string& value, const std::string& type);

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

		/* Discovering physical ports */

		bool      port_is_physical (PortHandle) const;
		void      get_physical_outputs (DataType type, std::vector<std::string>&);
		void      get_physical_inputs (DataType type, std::vector<std::string>&);
		ChanCount n_physical_outputs () const;
		ChanCount n_physical_inputs () const;

		/* Getting access to the data buffer for a port */

		void* get_buffer (PortHandle, pframes_t);

		void* main_process_thread ();

	private:
		std::string _instance_name;
		Alsa_pcmi *_pcmi;

		bool  _run; /* keep going or stop, ardour thread */
		bool  _active; /* is running, process thread */
		bool  _freewheel;
		bool  _freewheeling;
		bool  _measure_latency;

		uint64_t _last_process_start;

		static std::vector<std::string> _midi_options;
		static std::vector<AudioBackend::DeviceStatus> _input_audio_device_status;
		static std::vector<AudioBackend::DeviceStatus> _output_audio_device_status;
		static std::vector<AudioBackend::DeviceStatus> _duplex_audio_device_status;
		static std::vector<AudioBackend::DeviceStatus> _midi_device_status;
		static ARDOUR::ALSADeviceInfo _input_audio_device_info;
		static ARDOUR::ALSADeviceInfo _output_audio_device_info;

		mutable std::string _input_audio_device;
		mutable std::string _output_audio_device;
		std::string _midi_driver_option;

		/* audio device reservation */
		AlsaDeviceReservation _device_reservation;

		/* audio settings */
		float  _samplerate;
		size_t _samples_per_period;
		size_t _periods_per_cycle;
		static size_t _max_buffer_size;

		uint32_t _n_inputs;
		uint32_t _n_outputs;

		uint32_t _systemic_audio_input_latency;
		uint32_t _systemic_audio_output_latency;

		/* midi settings */
		struct AlsaMidiDeviceInfo {
			bool     enabled;
			uint32_t systemic_input_latency;
			uint32_t systemic_output_latency;
			AlsaMidiDeviceInfo()
				: enabled (true)
				, systemic_input_latency (0)
				, systemic_output_latency (0)
			{}
		};

		mutable std::map<std::string, struct AlsaMidiDeviceInfo *> _midi_devices;
		struct AlsaMidiDeviceInfo * midi_device_info(std::string const) const;

		/* processing */
		float  _dsp_load;
		ARDOUR::DSPLoadCalculator  _dsp_load_calc;
		samplecnt_t _processed_samples;
		pthread_t _main_thread;

		/* DLL, track main process callback timing */
		double _t0, _t1;

		/* process threads */
		static void* alsa_process_thread (void *);
		std::vector<pthread_t> _threads;

		struct ThreadData {
			AlsaAudioBackend* engine;
			boost::function<void ()> f;
			size_t stacksize;

			ThreadData (AlsaAudioBackend* e, boost::function<void ()> fp, size_t stacksz)
				: engine (e) , f (fp) , stacksize (stacksz) {}
		};

		/* port engine */
		PortHandle add_port (const std::string& shortname, ARDOUR::DataType, ARDOUR::PortFlags);
		int register_system_audio_ports ();
		int register_system_midi_ports (const std::string device = "");
		void unregister_ports (bool system_only = false);
		void update_system_port_latecies ();

		std::vector<AlsaPort *> _system_inputs;
		std::vector<AlsaPort *> _system_outputs;
		std::vector<AlsaPort *> _system_midi_in;
		std::vector<AlsaPort *> _system_midi_out;

		struct SortByPortName
		{
			bool operator ()(const AlsaPort* lhs, const AlsaPort* rhs) const
			{
				return PBD::naturally_less (lhs->name ().c_str (), rhs->name ().c_str ());
			}
		};

		typedef std::map<std::string, AlsaPort *> PortMap; // fast lookup in _ports
		typedef std::set<AlsaPort *, SortByPortName> PortIndex; // fast lookup in _ports
		PortMap _portmap;
		PortIndex _ports;

		std::vector<AlsaMidiOut *> _rmidi_out;
		std::vector<AlsaMidiIn  *> _rmidi_in;

		unsigned _midi_ins;
		unsigned _midi_outs;

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
			return std::find (_ports.begin(), _ports.end(), static_cast<AlsaPort*>(port)) != _ports.end ();
		}

		AlsaPort* find_port (const std::string& port_name) const {
			PortMap::const_iterator it = _portmap.find (port_name);
			if (it == _portmap.end()) {
				return NULL;
			}
			return (*it).second;
		}

		void update_systemic_audio_latencies ();
		void update_systemic_midi_latencies ();

		/* additional re-sampled I/O */
		bool add_slave (const char*  slave_device,
		                unsigned int slave_rate,
		                unsigned int slave_spp,
		                unsigned int duplex = 3);

		class AudioSlave : public AlsaDeviceReservation, public AlsaAudioSlave {
			public:
				AudioSlave (
						const char*  device,
						unsigned int duplex,
						unsigned int master_rate,
						unsigned int master_samples_per_period,
						unsigned int slave_rate,
						unsigned int slave_samples_per_period,
						unsigned int periods_per_cycle);

				~AudioSlave ();

				bool active; // set in sync with process-cb
				bool halt;
				bool dead;

				std::vector<AlsaPort *> inputs;
				std::vector<AlsaPort *> outputs;

				PBD::Signal0<void> UpdateLatency;
				PBD::ScopedConnection latency_connection;

			protected:
				void update_latencies (uint32_t, uint32_t);

			private:
				PBD::ScopedConnection _halted_connection;
				void halted ();
		};

		typedef std::vector<AudioSlave*> AudioSlaves;
		AudioSlaves _slaves;

}; // class AlsaAudioBackend

} // namespace

#endif /* __libbackend_alsa_audiobackend_h__ */
