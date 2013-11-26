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

#ifndef __libardour_jack_audiobackend_h__
#define __libardour_jack_audiobackend_h__

#include <string>
#include <vector>
#include <map>
#include <set>

#include <stdint.h>

#include <boost/shared_ptr.hpp>

#include <jack/jack.h>
#include <jack/session.h>

#include "ardour/audio_backend.h"

namespace ARDOUR {

class JackConnection;
class JACKSession;

class JACKAudioBackend : public AudioBackend {
  public:
    JACKAudioBackend (AudioEngine& e, boost::shared_ptr<JackConnection>);
    ~JACKAudioBackend ();
    
    /* AUDIOBACKEND API */

    std::string name() const;
    void* private_handle() const;
    bool available() const;
    bool is_realtime () const;

    bool requires_driver_selection() const;
    std::vector<std::string> enumerate_drivers () const;
    int set_driver (const std::string&);

    std::vector<DeviceStatus> enumerate_devices () const;

    std::vector<float> available_sample_rates (const std::string& device) const;
    std::vector<uint32_t> available_buffer_sizes (const std::string& device) const;
    uint32_t available_input_channel_count (const std::string& device) const;
    uint32_t available_output_channel_count (const std::string& device) const;

    bool can_change_sample_rate_when_running() const;
    bool can_change_buffer_size_when_running() const;

    int set_device_name (const std::string&);
    int set_sample_rate (float);
    int set_buffer_size (uint32_t);
    int set_sample_format (SampleFormat);
    int set_interleaved (bool yn);
    int set_input_channels (uint32_t);
    int set_output_channels (uint32_t);
    int set_systemic_input_latency (uint32_t);
    int set_systemic_output_latency (uint32_t);

    std::string  device_name () const;
    float        sample_rate () const;
    uint32_t     buffer_size () const;
    SampleFormat sample_format () const;
    bool         interleaved () const;
    uint32_t     input_channels () const;
    uint32_t     output_channels () const;
    uint32_t     systemic_input_latency () const;
    uint32_t     systemic_output_latency () const;
    std::string  driver_name() const;

    std::string control_app_name () const;
    void launch_control_app ();

    int _start (bool for_latency_measurement);
    int stop ();
    int freewheel (bool);

    float cpu_load() const;

    pframes_t sample_time ();
    pframes_t sample_time_at_cycle_start ();
    pframes_t samples_since_cycle_start ();

    size_t raw_buffer_size (DataType t);

    int create_process_thread (boost::function<void()> func);
    int join_process_threads ();
    bool in_process_thread ();
    uint32_t process_thread_count ();

    void transport_start ();
    void transport_stop ();
    void transport_locate (framepos_t /*pos*/);
    TransportState transport_state () const;
    framepos_t transport_frame() const;

    int set_time_master (bool /*yn*/);
    bool get_sync_offset (pframes_t& /*offset*/) const;

    void update_latencies ();

    static bool already_configured();
    
    /* PORTENGINE API */

    const std::string& my_name() const;
    uint32_t port_name_size() const;

    int         set_port_name (PortHandle, const std::string&);
    std::string get_port_name (PortHandle) const;
    PortHandle  get_port_by_name (const std::string&) const;

    int get_ports (const std::string& port_name_pattern, DataType type, PortFlags flags, std::vector<std::string>&) const;

    DataType port_data_type (PortHandle) const;

    PortHandle register_port (const std::string& shortname, ARDOUR::DataType, ARDOUR::PortFlags);
    void  unregister_port (PortHandle);

    bool  connected (PortHandle, bool process_callback_safe);
    bool  connected_to (PortHandle, const std::string&, bool process_callback_safe);
    bool  physically_connected (PortHandle, bool process_callback_safe);
    int   get_connections (PortHandle, std::vector<std::string>&, bool process_callback_safe);
    int   connect (PortHandle, const std::string&);

    int   disconnect (PortHandle, const std::string&);
    int   disconnect_all (PortHandle);
    int   connect (const std::string& src, const std::string& dst);
    int   disconnect (const std::string& src, const std::string& dst);
    
    /* MIDI */

    std::vector<std::string> enumerate_midi_options () const;
    int set_midi_option (const std::string&);
    std::string midi_option () const;

    int      midi_event_get (pframes_t& timestamp, size_t& size, uint8_t** buf, void* port_buffer, uint32_t event_index);
    int      midi_event_put (void* port_buffer, pframes_t timestamp, const uint8_t* buffer, size_t size);
    uint32_t get_midi_event_count (void* port_buffer);
    void     midi_clear (void* port_buffer);

    /* Monitoring */

    bool  can_monitor_input() const;
    int   request_input_monitoring (PortHandle, bool);
    int   ensure_input_monitoring (PortHandle, bool);
    bool  monitoring_input (PortHandle);

    /* Latency management
     */
    
    void          set_latency_range (PortHandle, bool for_playback, LatencyRange);
    LatencyRange  get_latency_range (PortHandle, bool for_playback);

    /* Physical ports */

    bool      port_is_physical (PortHandle) const;
    void      get_physical_outputs (DataType type, std::vector<std::string>&);
    void      get_physical_inputs (DataType type, std::vector<std::string>&);
    ChanCount n_physical_outputs () const;
    ChanCount n_physical_inputs () const;

    /* Getting access to the data buffer for a port */

    void* get_buffer (PortHandle, pframes_t);

    /* transport sync */

    bool speed_and_position (double& sp, framepos_t& pos);

  private:
    boost::shared_ptr<JackConnection>  _jack_connection;
    bool            _running;
    bool            _freewheeling;
    std::map<DataType,size_t> _raw_buffer_sizes;

    std::vector<jack_native_thread_t> _jack_threads;

    static int  _xrun_callback (void *arg);
    static void* _process_thread (void *arg);
    static int  _sample_rate_callback (pframes_t nframes, void *arg);
    static int  _bufsize_callback (pframes_t nframes, void *arg);
    static void _jack_timebase_callback (jack_transport_state_t, pframes_t, jack_position_t*, int, void*);
    static int  _jack_sync_callback (jack_transport_state_t, jack_position_t*, void *arg);
    static void _freewheel_callback (int , void *arg);
    static void _latency_callback (jack_latency_callback_mode_t, void*);
    static void _session_callback (jack_session_event_t *event, void *arg);
    
    void jack_timebase_callback (jack_transport_state_t, pframes_t, jack_position_t*, int);
    int  jack_sync_callback (jack_transport_state_t, jack_position_t*);
    int  jack_bufsize_callback (pframes_t);
    int  jack_sample_rate_callback (pframes_t);
    void freewheel_callback (int);
    int  process_callback (pframes_t nframes);
    void jack_latency_callback (jack_latency_callback_mode_t);
    void disconnected (const char*);

    void set_jack_callbacks ();
    int reconnect_to_jack ();
    
    struct ThreadData {
	JACKAudioBackend* engine;
	boost::function<void()> f;
	size_t stacksize;
	
	ThreadData (JACKAudioBackend* e, boost::function<void()> fp, size_t stacksz)
		: engine (e) , f (fp) , stacksize (stacksz) {}
    };
    
    void*  process_thread ();
    static void* _start_process_thread (void*);

    void setup_jack_startup_command (bool for_latency_measurement);

    /* pffooo */

    std::string  _target_driver;
    std::string  _target_device;
    float        _target_sample_rate;
    uint32_t     _target_buffer_size;
    SampleFormat _target_sample_format;
    bool         _target_interleaved;
    uint32_t     _target_input_channels;
    uint32_t     _target_output_channels;
    uint32_t     _target_systemic_input_latency;
    uint32_t     _target_systemic_output_latency;
    uint32_t     _current_sample_rate;
    uint32_t     _current_buffer_size;
    std::string  _target_midi_option;

    typedef std::set<std::string> DeviceList;
    typedef std::map<std::string,DeviceList> DriverDeviceMap;
    
    mutable DriverDeviceMap all_devices;

    PBD::ScopedConnection disconnect_connection;

    /* PORTENGINE RELATED */

    static int  _graph_order_callback (void *arg);
    static void _registration_callback (jack_port_id_t, int, void *);
    static void _connect_callback (jack_port_id_t, jack_port_id_t, int, void *);

    void connect_callback (jack_port_id_t, jack_port_id_t, int);

    ChanCount n_physical (unsigned long flags) const;
    void get_physical (DataType type, unsigned long flags, std::vector<std::string>& phy) const;

    void when_connected_to_jack ();
    PBD::ScopedConnection jack_connection_connection;

    /* Object to manage interactions with Session in a way that 
       keeps JACK out of libardour directly
    */

    JACKSession* _session;
};

} // namespace

#endif /* __ardour_audiobackend_h__ */
    
