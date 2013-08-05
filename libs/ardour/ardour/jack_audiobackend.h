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
#ifdef HAVE_JACK_SESSION
#include <jack/session.h>
#endif

#include "ardour/audio_backend.h"

namespace ARDOUR {

class JackConnection;

class JACKAudioBackend : public AudioBackend {
  public:
    JACKAudioBackend (AudioEngine& e, boost::shared_ptr<JackConnection>);
    ~JACKAudioBackend ();

    std::string name() const;
    void* private_handle() const;
    bool connected() const;
    bool is_realtime () const;

    bool requires_driver_selection() const;
    std::vector<std::string> enumerate_drivers () const;
    int set_driver (const std::string&);

    std::vector<DeviceStatus> enumerate_devices () const;

    std::vector<float> available_sample_rates (const std::string& device) const;
    std::vector<uint32_t> available_buffer_sizes (const std::string& device) const;
    uint32_t available_input_channel_count (const std::string& device) const;
    uint32_t available_output_channel_count (const std::string& device) const;

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

    int start ();
    int stop ();
    int pause ();
    int freewheel (bool);

    float cpu_load() const;

    pframes_t sample_time ();
    pframes_t sample_time_at_cycle_start ();
    pframes_t samples_since_cycle_start ();

    size_t raw_buffer_size (DataType t);

    int create_process_thread (boost::function<void()> func, pthread_t*, size_t stacksize);

    void transport_start ();
    void transport_stop ();
    void transport_locate (framepos_t /*pos*/);
    TransportState transport_state () const;
    framepos_t transport_frame() const;

    int set_time_master (bool /*yn*/);
    bool get_sync_offset (pframes_t& /*offset*/) const;

    void update_latencies ();

    static bool already_configured();

  private:
    boost::shared_ptr<JackConnection>  _jack_connection; //< shared with JACKPortEngine
    bool            _running;
    bool            _freewheeling;
    std::map<DataType,size_t> _raw_buffer_sizes;

    static int  _xrun_callback (void *arg);
    static void* _process_thread (void *arg);
    static int  _sample_rate_callback (pframes_t nframes, void *arg);
    static int  _bufsize_callback (pframes_t nframes, void *arg);
    static void _jack_timebase_callback (jack_transport_state_t, pframes_t, jack_position_t*, int, void*);
    static int  _jack_sync_callback (jack_transport_state_t, jack_position_t*, void *arg);
    static void _freewheel_callback (int , void *arg);
    static void _latency_callback (jack_latency_callback_mode_t, void*);
#ifdef HAVE_JACK_SESSION
    static void _session_callback (jack_session_event_t *event, void *arg);
#endif
    
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

    ChanCount n_physical (unsigned long) const;
    
    void setup_jack_startup_command ();

    /* pffooo */

    std::string  _target_driver;
    std::string  _target_device;
    float        _target_sample_rate;
    uint32_t     _target_buffer_size;
    SampleFormat _target_sample_format;
    bool         _target_interleaved;
    uint32_t     _target_input_channels;
    uint32_t     _target_output_channels;
    uint32_t      _target_systemic_input_latency;
    uint32_t      _target_systemic_output_latency;

    uint32_t _current_sample_rate;
    uint32_t _current_buffer_size;
    uint32_t _current_usecs_per_cycle;
    uint32_t _current_systemic_input_latency;
    uint32_t _current_systemic_output_latency;

    typedef std::set<std::string> DeviceList;
    typedef std::map<std::string,DeviceList> DriverDeviceMap;
    
    mutable DriverDeviceMap all_devices;
};

} // namespace

#endif /* __ardour_audiobackend_h__ */
    
