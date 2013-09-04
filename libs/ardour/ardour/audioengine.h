/*
    Copyright (C) 2002-2004 Paul Davis

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

#ifndef __ardour_audioengine_h__
#define __ardour_audioengine_h__

#ifdef WAF_BUILD
#include "libardour-config.h"
#endif

#include <iostream>
#include <list>
#include <set>
#include <cmath>
#include <exception>
#include <string>

#include <glibmm/threads.h>

#include "pbd/signals.h"
#include "pbd/stacktrace.h"

#include "midi++/mmc.h"

#include "ardour/ardour.h"
#include "ardour/data_type.h"
#include "ardour/session_handle.h"
#include "ardour/types.h"
#include "ardour/chan_count.h"
#include "ardour/port_manager.h"

#ifdef HAVE_JACK_SESSION
#include <jack/session.h>
#endif

namespace ARDOUR {

class InternalPort;
class MidiPort;
class Port;
class Session;
class ProcessThread;
class AudioBackend;
class AudioBackendInfo;

class AudioEngine : public SessionHandlePtr, public PortManager
{
public:

    static AudioEngine* create ();

    virtual ~AudioEngine ();

    int discover_backends();
    std::vector<const AudioBackendInfo*> available_backends() const;
    std::string current_backend_name () const;
    boost::shared_ptr<AudioBackend> set_backend (const std::string&, const std::string& arg1, const std::string& arg2);
    boost::shared_ptr<AudioBackend> current_backend() const { return _backend; }
    bool setup_required () const;

    ProcessThread* main_thread() const { return _main_thread; }
    
    /* START BACKEND PROXY API 
     *
     * See audio_backend.h for full documentation and semantics. These wrappers
     * just forward to a backend implementation.
     */

    int            start ();
    int            stop ();
    int            pause ();
    int            freewheel (bool start_stop);
    float          get_cpu_load() const ;
    void           transport_start ();
    void           transport_stop ();
    TransportState transport_state ();
    void           transport_locate (framepos_t pos);
    framepos_t     transport_frame();
    framecnt_t     sample_rate () const;
    pframes_t      samples_per_cycle () const;
    int            usecs_per_cycle () const;
    size_t         raw_buffer_size (DataType t);
    pframes_t      sample_time ();
    pframes_t      sample_time_at_cycle_start ();
    pframes_t      samples_since_cycle_start ();
    bool           get_sync_offset (pframes_t& offset) const;
    int            create_process_thread (boost::function<void()> func, pthread_t*, size_t stacksize);
    bool           is_realtime() const;
    bool           connected() const;

    int set_device_name (const std::string&);
    int set_sample_rate (float);
    int set_buffer_size (uint32_t);
    int set_sample_format (SampleFormat);
    int set_interleaved (bool yn);
    int set_input_channels (uint32_t);
    int set_output_channels (uint32_t);
    int set_systemic_input_latency (uint32_t);
    int set_systemic_output_latency (uint32_t);

    /* END BACKEND PROXY API */

    bool freewheeling() const { return _freewheeling; }
    bool running() const { return _running; }

    Glib::Threads::Mutex& process_lock() { return _process_lock; }

    int request_buffer_size (pframes_t samples) {
	    return set_buffer_size (samples);
    }

    framecnt_t processed_frames() const { return _processed_frames; }
    
    void set_session (Session *);
    void remove_session (); // not a replacement for SessionHandle::session_going_away()
    Session* session() const { return _session; }

    class NoBackendAvailable : public std::exception {
      public:
	virtual const char *what() const throw() { return "could not connect to engine backend"; }
    };
    
    void split_cycle (pframes_t offset);
    
    int  reset_timebase ();
    
    void update_latencies ();
    
    /* this signal is sent for every process() cycle while freewheeling.
       (the regular process() call to session->process() is not made)
    */
    
    PBD::Signal1<int, pframes_t> Freewheel;
    
    PBD::Signal0<void> Xrun;

    /* this signal is emitted if the sample rate changes */
    
    PBD::Signal1<void, framecnt_t> SampleRateChanged;
    
    /* this signal is sent if the backend ever disconnects us */
    
    PBD::Signal1<void,const char*> Halted;
    
    /* these two are emitted when the engine itself is
       started and stopped
    */
    
    PBD::Signal0<void> Running;
    PBD::Signal0<void> Stopped;

    static AudioEngine* instance() { return _instance; }
    static void destroy();
    void died ();
    
    /* The backend will cause these at the appropriate time(s)
     */
    int  process_callback (pframes_t nframes);
    int  buffer_size_change (pframes_t nframes);
    int  sample_rate_change (pframes_t nframes);
    void freewheel_callback (bool);
    void timebase_callback (TransportState state, pframes_t nframes, framepos_t pos, int new_position);
    int  sync_callback (TransportState state, framepos_t position);
    int  port_registration_callback ();
    void latency_callback (bool for_playback);
    void halted_callback (const char* reason);

    /* sets up the process callback thread */
    static void thread_init_callback (void *);

    MIDI::MachineControl& mmc() { return _mmc; }

  private:
    AudioEngine ();

    static AudioEngine*       _instance;

    boost::shared_ptr<AudioBackend> _backend;

    Glib::Threads::Mutex      _process_lock;
    Glib::Threads::Cond        session_removed;
    bool                       session_remove_pending;
    frameoffset_t              session_removal_countdown;
    gain_t                     session_removal_gain;
    gain_t                     session_removal_gain_step;
    bool                      _running;
    bool                      _freewheeling;
    /// number of frames between each check for changes in monitor input
    framecnt_t                 monitor_check_interval;
    /// time of the last monitor check in frames
    framecnt_t                 last_monitor_check;
    /// the number of frames processed since start() was called
    framecnt_t                _processed_frames;
    bool                      _pre_freewheel_mmc_enabled;
    Glib::Threads::Thread*     m_meter_thread;
    ProcessThread*            _main_thread;
    MIDI::MachineControl      _mmc;
    
    void meter_thread ();
    void start_metering_thread ();
    void stop_metering_thread ();
    
    static gint      m_meter_exit;
    
    void parameter_changed (const std::string&);
    PBD::ScopedConnection config_connection;

    typedef std::map<std::string,AudioBackendInfo*> BackendMap;
    BackendMap _backends;
    AudioBackendInfo* backend_discover (const std::string&);
    void drop_backend ();
};
	
} // namespace ARDOUR

#endif /* __ardour_audioengine_h__ */
