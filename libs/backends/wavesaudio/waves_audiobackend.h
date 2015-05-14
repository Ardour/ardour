/*
    Copyright (C) 2013 Waves Audio Ltd.

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

#ifndef __libardour_waves_audiobackend_h__
#define __libardour_waves_audiobackend_h__

#include <string>
#include <vector>    
#include <list>

#include <stdint.h>
#include <stdlib.h>

#include <boost/function.hpp>

#include "ardour/types.h"
#include "ardour/audio_backend.h"

#include "waves_midi_device_manager.h"

#ifdef __APPLE__

#include <WCMRCoreAudioDeviceManager.h>

class ArdourAudioDeviceManager : public WCMRCoreAudioDeviceManager
{
  public:
    ArdourAudioDeviceManager (WCMRAudioDeviceManagerClient *client) : WCMRCoreAudioDeviceManager (client, eAllDevices) {};
};

#elif defined (PLATFORM_WINDOWS)

#include <WCMRPortAudioDeviceManager.h>

class ArdourAudioDeviceManager : public WCMRPortAudioDeviceManager
{
  public:
    ArdourAudioDeviceManager (WCMRAudioDeviceManagerClient *client) : WCMRPortAudioDeviceManager (client, eAllDevices) {};
};

#endif

namespace ARDOUR {

class AudioEngine;
class PortEngine;
class PortManager;
class WavesAudioBackend;
class WavesDataPort;
class WavesAudioPort;
class WavesMidiPort;


    class WavesAudioBackend : public AudioBackend, WCMRAudioDeviceManagerClient
{
  public:
    WavesAudioBackend (AudioEngine& e);
    virtual ~WavesAudioBackend ();

    /* AUDIOBACKEND API */

    virtual std::string name () const;
    
    virtual bool is_realtime () const;
    
    virtual bool requires_driver_selection () const;
    
    virtual std::vector<std::string> enumerate_drivers () const;
    
    virtual int set_driver (const std::string& /*drivername*/);
    
    virtual std::vector<DeviceStatus> enumerate_devices () const;
    
    virtual std::vector<float> available_sample_rates (const std::string& device) const;

    virtual float default_sample_rate () const;

    virtual std::vector<uint32_t> available_buffer_sizes (const std::string& device) const;

    virtual uint32_t available_input_channel_count (const std::string& device) const;

    virtual uint32_t available_output_channel_count (const std::string& device) const;

    virtual bool can_change_sample_rate_when_running () const;

    virtual bool can_change_buffer_size_when_running () const;

    virtual int set_device_name (const std::string& name);

	virtual int drop_device();

    virtual int set_sample_rate (float);

    virtual int set_buffer_size (uint32_t);

    virtual int set_sample_format (SampleFormat);

    virtual int set_interleaved (bool yn);
    
    virtual int set_input_channels (uint32_t);
    
    virtual int set_output_channels (uint32_t);

    virtual int set_systemic_input_latency (uint32_t);

    virtual int set_systemic_output_latency (uint32_t);

    int set_systemic_midi_input_latency (std::string const, uint32_t) { return 0; }
    int set_systemic_midi_output_latency (std::string const, uint32_t) { return 0; }

    virtual int reset_device ();

    virtual std::string device_name () const;
    
    virtual float sample_rate () const;
    
    virtual uint32_t buffer_size () const;
    
    virtual SampleFormat sample_format () const;
    
    virtual bool interleaved () const;
    
    virtual uint32_t input_channels () const;
    
    virtual uint32_t output_channels () const;
    
    virtual uint32_t systemic_input_latency () const;
    
    virtual uint32_t systemic_output_latency () const;

    uint32_t systemic_midi_input_latency (std::string const) const { return 0; }

    uint32_t systemic_midi_output_latency (std::string const) const { return 0; }

    virtual std::string control_app_name () const;

    virtual void launch_control_app ();

    virtual std::vector<std::string> enumerate_midi_options () const;

    virtual int set_midi_option (const std::string& option);

    virtual std::string midi_option () const;

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

    virtual int _start (bool for_latency_measurement);

    virtual int stop ();

    virtual int freewheel (bool start_stop);

    virtual float dsp_load () const ;

    virtual void transport_start ();

    virtual void transport_stop ();

    virtual TransportState transport_state () const;

    virtual void transport_locate (framepos_t pos);

    virtual framepos_t transport_frame () const;

    virtual int set_time_master (bool yn);

    virtual int usecs_per_cycle () const;

    virtual size_t raw_buffer_size (DataType data_type);
    
    virtual framepos_t sample_time ();

    virtual framepos_t sample_time_at_cycle_start ();

    virtual pframes_t samples_since_cycle_start ();

    virtual bool get_sync_offset (pframes_t& offset) const;

    virtual int create_process_thread (boost::function<void ()> func);

    virtual int join_process_threads ();

    virtual bool in_process_thread ();

    virtual uint32_t process_thread_count ();

    virtual void update_latencies ();

    virtual bool speed_and_position (double& speed, framepos_t& position) {
        speed = 0.0;
        position = 0;
        return false;
    }

    /* PORTENGINE API */

    virtual void* private_handle () const;
 
    virtual const std::string& my_name () const;
    
    virtual bool available () const;

    virtual uint32_t port_name_size () const;

    virtual int set_port_name (PortHandle port_handle, const std::string& port_name);

    virtual std::string get_port_name (PortHandle port_handle ) const;

    virtual PortHandle get_port_by_name (const std::string& port_name) const;

    virtual int get_ports (const std::string& port_name_pattern, DataType type, PortFlags flags, std::vector<std::string>& port_handles) const;

    virtual DataType port_data_type (PortHandle port_handle) const;

    virtual PortHandle register_port (const std::string& shortname, ARDOUR::DataType type, ARDOUR::PortFlags flags);

    virtual void unregister_port (PortHandle port_handle);

    virtual int connect (const std::string& src, const std::string& dst);

    virtual int disconnect (const std::string& src, const std::string& dst);
    
    virtual int connect (PortHandle port_handle, const std::string& port_name);

    virtual int disconnect (PortHandle port_handle, const std::string& port_name);

    virtual int disconnect_all (PortHandle port_handle);

    virtual bool connected (PortHandle port_handle, bool process_callback_safe);

    virtual bool connected_to (PortHandle port_handle, const std::string& port_name, bool process_callback_safe);

    virtual bool physically_connected (PortHandle port_handle, bool process_callback_safe);

    virtual int get_connections (PortHandle port_handle, std::vector<std::string>&, bool process_callback_safe);

    virtual int midi_event_get (pframes_t& timestamp, size_t& size, uint8_t** buf, void* port_buffer, uint32_t event_index);
    
    virtual int midi_event_put (void* port_buffer, pframes_t timestamp, const uint8_t* buffer, size_t size);
    
    virtual uint32_t get_midi_event_count (void* port_buffer);
    
    virtual void midi_clear (void* port_buffer);

    virtual bool can_monitor_input () const;
    
    virtual int request_input_monitoring (PortHandle port_handle, bool);
    
    virtual int ensure_input_monitoring (PortHandle port_handle, bool);
    
    virtual bool monitoring_input (PortHandle port_handle);

    virtual void set_latency_range (PortHandle port_handle, bool for_playback, LatencyRange);
    
    virtual LatencyRange get_latency_range (PortHandle port_handle, bool for_playback);

    virtual bool port_is_physical (PortHandle port_handle) const;

    virtual void get_physical_outputs (DataType type, std::vector<std::string>& port_names);

    virtual void get_physical_inputs (DataType type, std::vector<std::string>& port_names);

    virtual ChanCount n_physical_outputs () const;

    virtual ChanCount n_physical_inputs () const;

    virtual void* get_buffer (PortHandle port_handle, pframes_t frames);

    static AudioBackendInfo& backend_info () { return __backend_info; }

    virtual void AudioDeviceManagerNotification (NotificationReason reason, void* pParam);

  private:
    //ArdourAudioDeviceManagerClient _audio_device_manager_client;
    ArdourAudioDeviceManager _audio_device_manager;
    WavesMidiDeviceManager _midi_device_manager;

    WCMRAudioDevice *_device;
    SampleFormat _sample_format;
    bool _interleaved;
    static std::string __instantiated_name;
    uint32_t _input_channels;
    uint32_t _max_input_channels;
    uint32_t _output_channels;
    uint32_t _max_output_channels;
    float _sample_rate;
    uint32_t _buffer_size;
    uint32_t _systemic_input_latency;
    uint32_t _systemic_output_latency;
    bool _call_thread_init_callback;
    std::vector<pthread_t> _backend_threads;
    pthread_t _main_thread;
    static const size_t __max_raw_midi_buffer_size;

    static const std::vector<std::string> __available_midi_options;
    bool  _use_midi;

    struct ThreadData {
        WavesAudioBackend* engine;
        boost::function<void ()> f;
        size_t stacksize;
    
        ThreadData (WavesAudioBackend* e, boost::function<void ()> fp, size_t stacksz)
            : engine (e) , f (fp) , stacksize (stacksz) {}
    };

    static boost::shared_ptr<AudioBackend>    __waves_backend_factory (AudioEngine& e);
    static int __instantiate (const std::string& arg1, const std::string& arg2);
    static int __deinstantiate ();
    static bool __already_configured ();
    static bool __available ();

    static void* __start_process_thread (void*);
    static uint64_t __get_time_nanos ();
    
    static size_t __thread_stack_size ();

    void _audio_device_callback (const float* input_audio_buffer, 
                                 float* output_buffer, 
                                 unsigned long nframes,
                                 framepos_t sample_time,
                                 uint64_t cycle_start_time_nanos);

    void _changed_midi_devices ();

	// DO change sample rate and buffer size
	int _buffer_size_change(uint32_t new_buffer_size);
	int _sample_rate_change(float new_sample_rate);
    
    int _register_system_audio_ports ();
    int _register_system_midi_ports ();

    int _read_midi_data_from_devices ();
    int _write_midi_data_to_devices (pframes_t);

    pframes_t _ms_to_sample_time (int32_t time_ms) const;
    int32_t _sample_time_to_ms (pframes_t sample_time) const ;

    void _read_audio_data_from_device (const float* input_buffer, pframes_t nframes);
    void _write_audio_data_to_device (float* output_buffer, pframes_t nframes);

    void _unregister_system_audio_ports ();
    void _unregister_system_midi_ports ();

    WavesDataPort* _register_port (const std::string& port_name, ARDOUR::DataType type, ARDOUR::PortFlags flags);
    inline bool _registered (PortHandle  port_handle) const
    {
        return std::find (_ports.begin (), _ports.end (), (WavesDataPort*)port_handle) != _ports.end ();
    }
    
    WavesDataPort* _find_port (const std::string& port_name) const;
    void _freewheel_thread ();

    std::vector<WavesAudioPort*> _physical_audio_inputs;
    std::vector<WavesAudioPort*> _physical_audio_outputs;
    std::vector<WavesMidiPort*> _physical_midi_inputs;
    std::vector<WavesMidiPort*> _physical_midi_outputs;
    std::vector<WavesDataPort*> _ports;
    static AudioBackendInfo __backend_info;

#if defined (PLATFORM_WINDOWS)
	static uint64_t __performance_counter_frequency;
#endif
	uint64_t _cycle_start_time_nanos;
    framepos_t _sample_time_at_cycle_start;

    bool _freewheeling;
    bool _freewheel_thread_active;

    friend class WavesMidiDeviceManager;
    
    std::list<uint64_t> _dsp_load_history;
    size_t _dsp_load_history_length;
    uint64_t _dsp_load_accumulator;
    float _audio_cycle_period_nanos;
    void _init_dsp_load_history();
};

} // namespace

#endif /* __libardour_waves_audiobackend_h__ */
    
