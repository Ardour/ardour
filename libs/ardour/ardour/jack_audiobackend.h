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

#include <stdint.h>

#include <jack/jack.h>
#ifdef HAVE_JACK_SESSION
#include <jack/session.h>
#endif

#include "ardour/audio_backend.h"

namespace ARDOUR {

class JACKAudioBackend : public AudioBackend {
  public:
    JACKAudioBackend (AudioEngine& e);
    ~JACKAudioBackend ();

    std::string name() const;
    bool connected() const;
    bool is_realtime () const;

    std::vector<std::string> enumerate_devices () const;
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

    std::string  get_device_name () const;
    float        get_sample_rate () const;
    uint32_t     get_buffer_size () const;
    SampleFormat get_sample_format () const;
    bool         get_interleaved () const;
    uint32_t     get_input_channels () const;
    uint32_t     get_output_channels () const;
    uint32_t     get_systemic_input_latency () const;
    uint32_t     get_systemic_output_latency () const;

    int start ();
    int stop ();
    int pause ();
    int freewheel (bool);

  private:
    jack_client_t* volatile   _jack; /* could be reset to null by SIGPIPE or another thread */
    std::string                jack_client_name;

    static int  _xrun_callback (void *arg);
    static int  _graph_order_callback (void *arg);
    static void* _process_thread (void *arg);
    static int  _sample_rate_callback (pframes_t nframes, void *arg);
    static int  _bufsize_callback (pframes_t nframes, void *arg);
    static void _jack_timebase_callback (jack_transport_state_t, pframes_t, jack_position_t*, int, void*);
    static int  _jack_sync_callback (jack_transport_state_t, jack_position_t*, void *arg);
    static void _freewheel_callback (int , void *arg);
    static void _registration_callback (jack_port_id_t, int, void *);
    static void _connect_callback (jack_port_id_t, jack_port_id_t, int, void *);
    static void _latency_callback (jack_latency_callback_mode_t, void*);
    static void  halted (void *);
    static void  halted_info (jack_status_t,const char*,void *);
#ifdef HAVE_JACK_SESSION
    static void _session_callback (jack_session_event_t *event, void *arg);
#endif
    
    void jack_timebase_callback (jack_transport_state_t, pframes_t, jack_position_t*, int);
    int  jack_sync_callback (jack_transport_state_t, jack_position_t*);
    int  jack_bufsize_callback (pframes_t);
    int  jack_sample_rate_callback (pframes_t);
    void freewheel_callback (int);
    void connect_callback (jack_port_id_t, jack_port_id_t, int);
    int  process_callback (pframes_t nframes);
    void jack_latency_callback (jack_latency_callback_mode_t);
    
    void set_jack_callbacks ();
    int connect_to_jack (std::string client_name, std::string session_uuid);
    
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
    void get_physical (DataType, unsigned long, std::vector<std::string> &);

    /* pffooo */

    std::string  _target_device;
    float        _target_sample_rate;
    uint32_t     _target_buffer_size;
    SampleFormat _target_sample_format;
    bool         _target_interleaved;
    uint32_t     _target_input_channels;
    uint32_t     _target_output_channels;
    uint32_t      _target_systemic_input_latency;
    uint32_t      _target_systemic_output_latency;
};

} // namespace

#endif /* __ardour_audiobackend_h__ */
    
