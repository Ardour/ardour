/*
    Copyright (C) 2000 Paul Davis 

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

    $Id$
*/

#ifndef __ardour_session_h__
#define __ardour_session_h__


#include <string>
#include <list>
#include <map>
#include <vector>
#include <set>
#include <stack>

#include <boost/weak_ptr.hpp>

#include <stdint.h>

#include <sndfile.h>

#include <glibmm/thread.h>

#include <pbd/error.h>
#include <pbd/undo.h>
#include <pbd/pool.h>
#include <pbd/rcu.h>
#include <pbd/statefuldestructible.h>

#include <midi++/types.h>
#include <midi++/mmc.h>

#include <pbd/stateful.h> 
#include <pbd/destructible.h> 

#include <ardour/ardour.h>
#include <ardour/configuration.h>
#include <ardour/location.h>
#include <ardour/gain.h>
#include <ardour/io.h>

class XMLTree;
class XMLNode;
class AEffect;

namespace MIDI {
	class Port;
}

namespace PBD {
	class Controllable;
}

namespace ARDOUR {

class Port;
class AudioEngine;
class Slave;
class Diskstream;	
class AudioDiskstream;	
class Route;
class AuxInput;
class Source;
class AudioSource;
class AudioFileSource;
class Auditioner;
class Insert;
class Send;
class Redirect;
class PortInsert;
class PluginInsert;
class Connection;
class TempoMap;
class AudioTrack;
class NamedSelection;
class AudioRegion;
class Region;
class Playlist;
class VSTPlugin;
class ControlProtocolManager;

struct AudioExportSpecification;
struct RouteGroup;

using std::vector;
using std::string;
using std::map;
using std::set;

class Session : public sigc::trackable, public PBD::StatefulDestructible

{
  private:
	typedef std::pair<boost::shared_ptr<Route>,bool> RouteBooleanState;
	typedef vector<RouteBooleanState> GlobalRouteBooleanState;
	typedef std::pair<boost::shared_ptr<Route>,MeterPoint> RouteMeterState;
	typedef vector<RouteMeterState> GlobalRouteMeterState;

  public:
	enum RecordState {
		Disabled = 0,
		Enabled = 1,
		Recording = 2
	};

	struct Event {
	    enum Type {
		    SetTransportSpeed,
		    SetDiskstreamSpeed,
		    Locate,
		    LocateRoll,
		    SetLoop,
		    PunchIn,
		    PunchOut,
		    RangeStop,
		    RangeLocate,
		    Overwrite,
		    SetSlaveSource,
		    Audition,
		    InputConfigurationChange,
		    SetAudioRange,
		    SetPlayRange,
		    
		    /* only one of each of these events
		       can be queued at any one time
		    */

		    StopOnce,
		    AutoLoop
	    };

	    enum Action {
		    Add,
		    Remove,
		    Replace,
		    Clear
	    };

	    Type           type;
	    Action         action;
	    nframes_t action_frame;
	    nframes_t target_frame;
	    float          speed;

	    union {
			void*                ptr;
			bool                 yes_or_no;
			SlaveSource slave;
			Route*               route;
	    };

	    boost::shared_ptr<Region>   region;

	    list<AudioRange>     audio_range;
	    list<MusicRange>     music_range;

	    Event(Type t, Action a, nframes_t when, nframes_t where, float spd, bool yn = false)
		    : type (t), 
		      action (a),
		      action_frame (when),
		      target_frame (where),
		      speed (spd),
		      yes_or_no (yn) {}

	    void set_ptr (void* p) { 
		    ptr = p;
	    }

	    bool before (const Event& other) const {
		    return action_frame < other.action_frame;
	    }

	    bool after (const Event& other) const {
		    return action_frame > other.action_frame;
	    }

	    static bool compare (const Event *e1, const Event *e2) {
		    return e1->before (*e2);
	    }

	    void *operator new (size_t ignored) {
		    return pool.alloc ();
	    }

	    void operator delete(void *ptr, size_t size) {
		    pool.release (ptr);
	    }

	    static const nframes_t Immediate = 0;

	 private:
	    static MultiAllocSingleReleasePool pool;
	};

	/* creating from an XML file */

	Session (AudioEngine&,
		 string fullpath,
		 string snapshot_name,
		 string* mix_template = 0);

	/* creating a new Session */

	Session (AudioEngine&,
		 string fullpath,
		 string snapshot_name,
		 AutoConnectOption input_auto_connect,
		 AutoConnectOption output_auto_connect,
		 uint32_t control_out_channels,
		 uint32_t master_out_channels,
		 uint32_t n_physical_in,
		 uint32_t n_physical_out,
		 nframes_t initial_length);
	
	virtual ~Session ();


	static int find_session (string str, string& path, string& snapshot, bool& isnew);
	
	string path() const { return _path; }
	string name() const { return _name; }
	string snap_name() const { return _current_snapshot_name; }
	string raid_path () const;

	void set_snap_name ();

	void set_dirty ();
	void set_clean ();
	bool dirty() const { return _state_of_the_state & Dirty; }
	sigc::signal<void> DirtyChanged;

	std::string sound_dir (bool with_path = true) const;
	std::string peak_dir () const;
	std::string dead_sound_dir () const;
	std::string automation_dir () const;

	string peak_path_from_audio_path (string) const;

	static string suffixed_search_path (std::string suffix, bool data);
	static string control_protocol_path ();
	static string template_path ();
	static string template_dir ();
	static void get_template_list (list<string>&);
	
	static string change_audio_path_by_name (string oldpath, string oldname, string newname, bool destructive);
	string audio_path_from_name (string, uint32_t nchans, uint32_t chan, bool destructive);

	void process (nframes_t nframes);

	vector<Sample*>& get_passthru_buffers() { return _passthru_buffers; }
	vector<Sample*>& get_silent_buffers (uint32_t howmany);
	vector<Sample*>& get_send_buffers () { return _send_buffers; }

	void add_diskstream (boost::shared_ptr<Diskstream>);
	boost::shared_ptr<Diskstream> diskstream_by_id (const PBD::ID& id);
	boost::shared_ptr<Diskstream> diskstream_by_name (string name);

	bool have_captured() const { return _have_captured; }

	void refill_all_diskstream_buffers ();
	uint32_t diskstream_buffer_size() const { return dstream_buffer_size; }
	
	uint32_t get_next_diskstream_id() const { return n_diskstreams(); }
	uint32_t n_diskstreams() const;
	
	typedef std::list<boost::shared_ptr<Diskstream> > DiskstreamList;
	typedef std::list<boost::shared_ptr<Route> >      RouteList; 

	boost::shared_ptr<RouteList> get_routes() const {
		return routes.reader ();
	}

	uint32_t nroutes() const { return routes.reader()->size(); }
	uint32_t ntracks () const;
	uint32_t nbusses () const;

	struct RoutePublicOrderSorter {
	    bool operator() (boost::shared_ptr<Route>, boost::shared_ptr<Route> b);
	};
	
	template<class T> void foreach_route (T *obj, void (T::*func)(Route&));
	template<class T> void foreach_route (T *obj, void (T::*func)(boost::shared_ptr<Route>));
	template<class T, class A> void foreach_route (T *obj, void (T::*func)(Route&, A), A arg);

	boost::shared_ptr<Route> route_by_name (string);
	boost::shared_ptr<Route> route_by_id (PBD::ID);
	boost::shared_ptr<Route> route_by_remote_id (uint32_t id);

	bool route_name_unique (string) const;

	bool get_record_enabled() const { 
		return (record_status () >= Enabled);
	}

	RecordState record_status() const {
		return (RecordState) g_atomic_int_get (&_record_status);
	}

	bool actively_recording () {
		return record_status() == Recording;
	}

	bool record_enabling_legal () const;
	void maybe_enable_record ();
	void disable_record (bool rt_context, bool force = false);
	void step_back_from_record ();
	
	/* Proxy signal for region hidden changes */

	sigc::signal<void,boost::shared_ptr<Region> > RegionHiddenChange;

	/* Emitted when all i/o connections are complete */
	
	sigc::signal<void> IOConnectionsComplete;
	
	/* Record status signals */

        sigc::signal<void> RecordStateChanged;

	/* Transport mechanism signals */

	sigc::signal<void> TransportStateChange; /* generic */
	sigc::signal<void,nframes_t> PositionChanged; /* sent after any non-sequential motion */
	sigc::signal<void> DurationChanged;
	sigc::signal<void> HaltOnXrun;

	sigc::signal<void,RouteList&> RouteAdded;

	void request_roll ();
	void request_bounded_roll (nframes_t start, nframes_t end);
	void request_stop (bool abort = false);
	void request_locate (nframes_t frame, bool with_roll = false);
	void request_play_loop (bool yn);
	nframes_t  last_transport_start() const { return _last_roll_location; }
	void goto_end ()   { request_locate (end_location->start(), false);}
	void goto_start () { request_locate (start_location->start(), false); }
	void set_session_start (nframes_t start) { start_location->set_start(start); }
	void set_session_end (nframes_t end) { end_location->set_start(end); _end_location_is_free = false; }
	void use_rf_shuttle_speed ();
	void request_transport_speed (float speed);
	void request_overwrite_buffer (Diskstream*);
	void request_diskstream_speed (Diskstream&, float speed);
	void request_input_change_handling ();

	bool locate_pending() const { return static_cast<bool>(post_transport_work&PostTransportLocate); }
	bool transport_locked () const;

	int wipe ();

	int remove_region_from_region_list (boost::shared_ptr<Region>);

	nframes_t get_maximum_extent () const;
	nframes_t current_end_frame() const { return end_location->start(); }
	nframes_t current_start_frame() const { return start_location->start(); }
	nframes_t frame_rate() const   { return _current_frame_rate; }
	nframes_t frames_per_hour() const { return _frames_per_hour; }

	double frames_per_smpte_frame() const { return _frames_per_smpte_frame; }
	nframes_t smpte_frames_per_hour() const { return _smpte_frames_per_hour; }

	/* Locations */

	Locations *locations() { return &_locations; }

	sigc::signal<void,Location*>    auto_loop_location_changed;
	sigc::signal<void,Location*>    auto_punch_location_changed;
	sigc::signal<void>              locations_modified;

	void set_auto_punch_location (Location *);
	void set_auto_loop_location (Location *);

	void reset_input_monitor_state ();

	void add_event (nframes_t action_frame, Event::Type type, nframes_t target_frame = 0);
	void remove_event (nframes_t frame, Event::Type type);
	void clear_events (Event::Type type);

	nframes_t get_block_size() const { return current_block_size; }
	nframes_t worst_output_latency () const { return _worst_output_latency; }
	nframes_t worst_input_latency () const { return _worst_input_latency; }
	nframes_t worst_track_latency () const { return _worst_track_latency; }

	int save_state (string snapshot_name, bool pending = false);
	int restore_state (string snapshot_name);
	int save_template (string template_name);
        int save_history (string snapshot_name = "");
        int restore_history (string snapshot_name);

	static int rename_template (string old_name, string new_name);

	static int delete_template (string name);
	
	sigc::signal<void,string> StateSaved;
	sigc::signal<void> StateReady;

	vector<string*>* possible_states() const;
	static vector<string*>* possible_states(string path);

	XMLNode& get_state();
	int      set_state(const XMLNode& node); // not idempotent
	XMLNode& get_template();

	void add_instant_xml (XMLNode&, const std::string& dir);

	enum StateOfTheState {
		Clean = 0x0,
		Dirty = 0x1,
		CannotSave = 0x2,
		Deletion = 0x4,
		InitialConnecting = 0x8,
		Loading = 0x10,
		InCleanup = 0x20
	};

	StateOfTheState state_of_the_state() const { return _state_of_the_state; }

	RouteGroup* add_edit_group (string);
	RouteGroup* add_mix_group (string);

	void remove_edit_group (RouteGroup&);
	void remove_mix_group (RouteGroup&);

	RouteGroup *mix_group_by_name (string);
	RouteGroup *edit_group_by_name (string);

	sigc::signal<void,RouteGroup*> edit_group_added;
	sigc::signal<void,RouteGroup*> mix_group_added;
	sigc::signal<void> edit_group_removed;
	sigc::signal<void> mix_group_removed;

	void foreach_edit_group (sigc::slot<void,RouteGroup*> sl) {
		for (list<RouteGroup *>::iterator i = edit_groups.begin(); i != edit_groups.end(); i++) {
			sl (*i);
		}
	}

	void foreach_mix_group (sigc::slot<void,RouteGroup*> sl) {
		for (list<RouteGroup *>::iterator i = mix_groups.begin(); i != mix_groups.end(); i++) {
			sl (*i);
		}
	}

	/* fundamental operations. duh. */

	std::list<boost::shared_ptr<AudioTrack> > new_audio_track (int input_channels, int output_channels, TrackMode mode = Normal, uint32_t how_many = 1);
	RouteList new_audio_route (int input_channels, int output_channels, uint32_t how_many);

	void   remove_route (boost::shared_ptr<Route>);

	void   resort_routes ();
	void   resort_routes_using (boost::shared_ptr<RouteList>);

	AudioEngine &engine() { return _engine; };

	int32_t  max_level;
	int32_t  min_level;

	/* Time */

	nframes_t transport_frame () const {return _transport_frame; }
	nframes_t audible_frame () const;

	enum SmpteFormat {
		smpte_23976,
		smpte_24,
		smpte_24976,
		smpte_25,
		smpte_2997,
		smpte_2997drop,
		smpte_30,
		smpte_30drop,
		smpte_5994,
		smpte_60,
	};

	enum PullupFormat {
		pullup_Plus4Plus1,
		pullup_Plus4,
		pullup_Plus4Minus1,
		pullup_Plus1,
		pullup_None,
		pullup_Minus1,
		pullup_Minus4Plus1,
		pullup_Minus4,
		pullup_Minus4Minus1,
	};

	int  set_smpte_type (float fps, bool drop_frames);

	void sync_time_vars();

	void bbt_time (nframes_t when, BBT_Time&);
	void smpte_to_sample( SMPTE::Time& smpte, nframes_t& sample, bool use_offset, bool use_subframes ) const;
	void sample_to_smpte( nframes_t sample, SMPTE::Time& smpte, bool use_offset, bool use_subframes ) const;
	void smpte_time (SMPTE::Time &);
	void smpte_time (nframes_t when, SMPTE::Time&);
	void smpte_time_subframes (nframes_t when, SMPTE::Time&);

	void smpte_duration (nframes_t, SMPTE::Time&) const;
	void smpte_duration_string (char *, nframes_t) const;

	void           set_smpte_offset (nframes_t);
	nframes_t smpte_offset () const { return _smpte_offset; }
	void           set_smpte_offset_negative (bool);
	bool           smpte_offset_negative () const { return _smpte_offset_negative; }

	nframes_t convert_to_frames_at (nframes_t position, AnyTime&);

	static sigc::signal<void> StartTimeChanged;
	static sigc::signal<void> EndTimeChanged;
	static sigc::signal<void> SMPTEOffsetChanged;

	void        request_slave_source (SlaveSource);
	bool        synced_to_jack() const { return Config->get_slave_source() == JACK; }

   	float       transport_speed() const { return _transport_speed; }
	bool        transport_stopped() const { return _transport_speed == 0.0f; }
	bool        transport_rolling() const { return _transport_speed != 0.0f; }

	int jack_slave_sync (nframes_t);

	TempoMap& tempo_map() { return *_tempo_map; }
	
	/* region info  */

	sigc::signal<void,boost::shared_ptr<AudioRegion> > AudioRegionAdded;
	sigc::signal<void,boost::shared_ptr<AudioRegion> > AudioRegionRemoved;

	int region_name (string& result, string base = string(""), bool newlevel = false) const;
	string new_region_name (string);
	string path_from_region_name (string name, string identifier);

	boost::shared_ptr<AudioRegion> find_whole_file_parent (boost::shared_ptr<AudioRegion>);
	void find_equivalent_playlist_regions (boost::shared_ptr<Region>, std::vector<boost::shared_ptr<Region> >& result);

	boost::shared_ptr<AudioRegion> XMLRegionFactory (const XMLNode&, bool full);

	template<class T> void foreach_audio_region (T *obj, void (T::*func)(boost::shared_ptr<AudioRegion>));

	/* source management */

	struct import_status : public InterThreadInfo {
	    string doing_what;
	    
	    /* control info */
	    bool multichan;
	    bool sample_convert;
	    volatile bool freeze;
	    string pathname;
	    
	    /* result */
	    std::vector<boost::shared_ptr<AudioRegion> > new_regions;
	    
	};

	int import_audiofile (import_status&);
	bool sample_rate_convert (import_status&, string infile, string& outfile);
	string build_tmp_convert_name (string file);

	SlaveSource post_export_slave;
	nframes_t post_export_position;

	int start_audio_export (ARDOUR::AudioExportSpecification&);
	int stop_audio_export (ARDOUR::AudioExportSpecification&);
	
	void add_source (boost::shared_ptr<Source>);
	void remove_source (boost::weak_ptr<Source>);
	int  cleanup_audio_file_source (boost::shared_ptr<AudioFileSource>);

	struct cleanup_report {
	    vector<string> paths;
	    int32_t space;
	};

	int  cleanup_sources (cleanup_report&);
	int  cleanup_trash_sources (cleanup_report&);

	int destroy_region (boost::shared_ptr<Region>);
	int destroy_regions (std::list<boost::shared_ptr<Region> >);

	int remove_last_capture ();

	/* handlers should return -1 for "stop cleanup", 0 for
	   "yes, delete this playlist" and 1 for "no, don't delete
	   this playlist.
	*/
	
	sigc::signal<int,ARDOUR::Playlist*> AskAboutPlaylistDeletion;


	/* handlers should return !0 for use pending state, 0 for
	   ignore it.
	*/

	static sigc::signal<int> AskAboutPendingState;
	
	sigc::signal<void,boost::shared_ptr<Source> > SourceAdded;
	sigc::signal<void,boost::shared_ptr<Source> > SourceRemoved;

	boost::shared_ptr<AudioFileSource> create_audio_source_for_session (ARDOUR::AudioDiskstream&, uint32_t which_channel, bool destructive);

	boost::shared_ptr<Source> source_by_id (const PBD::ID&);

	/* playlist management */

	Playlist* playlist_by_name (string name);
	void add_playlist (Playlist *);
	sigc::signal<void,Playlist*> PlaylistAdded;
	sigc::signal<void,Playlist*> PlaylistRemoved;

	uint32_t n_playlists() const;

	template<class T> void foreach_playlist (T *obj, void (T::*func)(Playlist *));

	/* named selections */

	NamedSelection* named_selection_by_name (string name);
	void add_named_selection (NamedSelection *);
	void remove_named_selection (NamedSelection *);

	template<class T> void foreach_named_selection (T& obj, void (T::*func)(NamedSelection&));
	sigc::signal<void> NamedSelectionAdded;
	sigc::signal<void> NamedSelectionRemoved;

        /* Curves and AutomationLists (TODO when they go away) */
        void add_curve(Curve*);
        void add_automation_list(AutomationList*);

	/* fade curves */

	float get_default_fade_length () const { return default_fade_msecs; }
	float get_default_fade_steepness () const { return default_fade_steepness; }
	void set_default_fade (float steepness, float msecs);

	/* auditioning */

	boost::shared_ptr<Auditioner> the_auditioner() { return auditioner; }
	void audition_playlist ();
	void audition_region (boost::shared_ptr<Region>);
	void cancel_audition ();
	bool is_auditioning () const;
	
	sigc::signal<void,bool> AuditionActive;

	/* flattening stuff */

	int write_one_audio_track (AudioTrack&, nframes_t start, nframes_t cnt, bool overwrite, vector<boost::shared_ptr<AudioSource> >&,
				   InterThreadInfo& wot);
	int freeze (InterThreadInfo&);

	/* session-wide solo/mute/rec-enable */
	
	bool soloing() const { return currently_soloing; }

	void set_all_solo (bool);
	void set_all_mute (bool);

	sigc::signal<void,bool> SoloActive;
	
	void record_disenable_all ();
	void record_enable_all ();

	/* control/master out */

	boost::shared_ptr<IO> control_out() const { return _control_out; }
	boost::shared_ptr<IO> master_out() const { return _master_out; }

	/* insert/send management */
	
	uint32_t n_port_inserts() const { return _port_inserts.size(); }
	uint32_t n_plugin_inserts() const { return _plugin_inserts.size(); }
	uint32_t n_sends() const { return _sends.size(); }

	string next_send_name();
	string next_insert_name();
	
	/* s/w "RAID" management */
	
	nframes_t available_capture_duration();

	/* I/O Connections */

	template<class T> void foreach_connection (T *obj, void (T::*func)(Connection *));
	void add_connection (Connection *);
	void remove_connection (Connection *);
	Connection *connection_by_name (string) const;

	sigc::signal<void,Connection *> ConnectionAdded;
	sigc::signal<void,Connection *> ConnectionRemoved;

	/* MIDI */

	int set_mtc_port (string port_tag);
	int set_mmc_port (string port_tag);
	int set_midi_port (string port_tag);
	MIDI::Port *mtc_port() const { return _mtc_port; }
	MIDI::Port *mmc_port() const { return _mmc_port; }
	MIDI::Port *midi_port() const { return _midi_port; }

	sigc::signal<void> MTC_PortChanged;
	sigc::signal<void> MMC_PortChanged;
	sigc::signal<void> MIDI_PortChanged;

	void set_trace_midi_input (bool, MIDI::Port* port = 0);
	void set_trace_midi_output (bool, MIDI::Port* port = 0);

	bool get_trace_midi_input(MIDI::Port *port = 0);
	bool get_trace_midi_output(MIDI::Port *port = 0);
	
	void send_midi_message (MIDI::Port * port, MIDI::eventType ev, MIDI::channel_t, MIDI::EventTwoBytes);

	void deliver_midi (MIDI::Port*, MIDI::byte*, int32_t size);

	/* Scrubbing */

	void start_scrub (nframes_t where);
	void stop_scrub ();
	void set_scrub_speed (float);
	nframes_t scrub_buffer_size() const;
	sigc::signal<void> ScrubReady;

	/* History (for editors, mixers, UIs etc.) */

	void undo (uint32_t n) {
		history.undo (n);
	}
	void redo (uint32_t n) {
		history.redo (n);
	}

	uint32_t undo_depth() const { return history.undo_depth(); }
	uint32_t redo_depth() const { return history.redo_depth(); }
	string next_undo() const { return history.next_undo(); }
	string next_redo() const { return history.next_redo(); }

	void begin_reversible_command (string cmd_name);
	void commit_reversible_command (Command* cmd = 0);

	void add_command (Command *const cmd) {
		current_trans->add_command (cmd);
	}

	std::map<PBD::ID, PBD::StatefulDestructible*> registry;

        // these commands are implemented in libs/ardour/session_command.cc
	Command *memento_command_factory(XMLNode *n);
        void register_with_memento_command_factory(PBD::ID, PBD::StatefulDestructible *);

        class GlobalSoloStateCommand : public Command
        {
            GlobalRouteBooleanState before, after;
            Session &sess;
            void *src;
        public:
            GlobalSoloStateCommand(Session &, void *src);
            void operator()();
            void undo();
            XMLNode &get_state();
            void mark();
        };

        class GlobalMuteStateCommand : public Command
        {
            GlobalRouteBooleanState before, after;
            Session &sess;
            void *src;
        public:
            GlobalMuteStateCommand(Session &, void *src);
            void operator()();
            void undo();
            XMLNode &get_state();
            void mark();
        };

        class GlobalRecordEnableStateCommand : public Command
        {
            GlobalRouteBooleanState before, after;
            Session &sess;
            void *src;
        public:
            GlobalRecordEnableStateCommand(Session &, void *src);
            void operator()();
            void undo();
            XMLNode &get_state();
            void mark();
        };

        class GlobalMeteringStateCommand : public Command
        {
            GlobalRouteMeterState before, after;
            Session &sess;
            void *src;
        public:
            GlobalMeteringStateCommand(Session &, void *src);
            void operator()();
            void undo();
            XMLNode &get_state();
            void mark();
        };

	/* clicking */

	boost::shared_ptr<IO>  click_io() { return _click_io; }
		
	/* tempo FX */

	struct TimeStretchRequest {
	    boost::shared_ptr<ARDOUR::AudioRegion> region;
	    float                fraction; /* session: read ; GUI: write */
	    float                progress; /* session: write ; GUI: read */
	    bool                 running;  /* read/write */
	    bool                 quick_seek; /* GUI: write */
	    bool                 antialias;  /* GUI: write */

	    TimeStretchRequest () {} 
	};

	boost::shared_ptr<AudioRegion> tempoize_region (TimeStretchRequest&);

	/* disk, buffer loads */

	uint32_t playback_load ();
	uint32_t capture_load ();
	uint32_t playback_load_min ();
	uint32_t capture_load_min ();

	void reset_playback_load_min ();
	void reset_capture_load_min ();
	
	float read_data_rate () const;
	float write_data_rate () const;

	/* ranges */

	void set_audio_range (list<AudioRange>&);
	void set_music_range (list<MusicRange>&);

	void request_play_range (bool yn);
	bool get_play_range () const { return _play_range; }

	/* favorite dirs */
	typedef vector<string> FavoriteDirs;

	static int read_favorite_dirs (FavoriteDirs&);

	static int write_favorite_dirs (FavoriteDirs&);
	
	/* file suffixes */

	static const char* template_suffix() { return _template_suffix; }
	static const char* statefile_suffix() { return _statefile_suffix; }
	static const char* pending_suffix() { return _pending_suffix; }

	/* buffers for gain and pan */

	gain_t* gain_automation_buffer () const { return _gain_automation_buffer; }
	pan_t** pan_automation_buffer() const { return _pan_automation_buffer; }

	/* buffers for conversion */
	enum RunContext {
		ButlerContext = 0,
		TransportContext,
		ExportContext
	};
	
	/* VST support */

	static long vst_callback (AEffect* effect,
				  long opcode,
				  long index,
				  long value,
				  void* ptr,
				  float opt);

	typedef float (*compute_peak_t)				(Sample *, nframes_t, float);
	typedef void  (*apply_gain_to_buffer_t)		(Sample *, nframes_t, float);
	typedef void  (*mix_buffers_with_gain_t)	(Sample *, Sample *, nframes_t, float);
	typedef void  (*mix_buffers_no_gain_t)		(Sample *, Sample *, nframes_t);

	static compute_peak_t			compute_peak;
	static apply_gain_to_buffer_t	apply_gain_to_buffer;
	static mix_buffers_with_gain_t	mix_buffers_with_gain;
	static mix_buffers_no_gain_t	mix_buffers_no_gain;

	static sigc::signal<void> SendFeedback;

	/* Controllables */

	PBD::Controllable* controllable_by_id (const PBD::ID&);

  protected:
	friend class AudioEngine;
	void set_block_size (nframes_t nframes);
	void set_frame_rate (nframes_t nframes);

  protected:
	friend class Diskstream;
	void stop_butler ();
	void wait_till_butler_finished();

  protected:
	friend class Route;
	void schedule_curve_reallocation ();
	void update_latency_compensation (bool, bool);
	
  private:
	int  create (bool& new_session, string* mix_template, nframes_t initial_length);

	static const char* _template_suffix;
	static const char* _statefile_suffix;
	static const char* _pending_suffix;

	enum SubState {
		PendingDeclickIn   = 0x1,
		PendingDeclickOut  = 0x2,
		StopPendingCapture = 0x4,
		AutoReturning      = 0x10,
		PendingLocate      = 0x20,
		PendingSetLoop     = 0x40
	};

	/* stuff used in process() should be close together to
	   maximise cache hits
	*/

	typedef void (Session::*process_function_type)(nframes_t);

	AudioEngine            &_engine;
	mutable gint            processing_prohibited;
	process_function_type    process_function;
	process_function_type    last_process_function;
	bool                     waiting_for_sync_offset;
	nframes_t          _base_frame_rate;
	nframes_t          _current_frame_rate;  //this includes video pullup offset
	int                      transport_sub_state;
	mutable gint           _record_status;
	nframes_t          _transport_frame;
	Location*                end_location;
	Location*                start_location;
	Slave                  *_slave;
	volatile float          _transport_speed;
	volatile float          _desired_transport_speed;
	float                   _last_transport_speed;
	nframes_t          _last_slave_transport_frame;
	nframes_t           maximum_output_latency;
	nframes_t           last_stop_frame;
	vector<Sample *>        _passthru_buffers;
	vector<Sample *>        _silent_buffers;
	vector<Sample *>        _send_buffers;
	nframes_t           current_block_size;
	nframes_t          _worst_output_latency;
	nframes_t          _worst_input_latency;
	nframes_t          _worst_track_latency;
	bool                    _have_captured;
	float                   _meter_hold;
	float                   _meter_falloff;
	bool                    _end_location_is_free;

	void set_worst_io_latencies ();
	void set_worst_io_latencies_x (IOChange asifwecare, void *ignored) {
		set_worst_io_latencies ();
	}

	void update_latency_compensation_proxy (void* ignored);

	void ensure_passthru_buffers (uint32_t howmany);
	
	void process_scrub          (nframes_t);
	void process_without_events (nframes_t);
	void process_with_events    (nframes_t);
	void process_audition       (nframes_t);
	int  process_export         (nframes_t, ARDOUR::AudioExportSpecification*);
	
	/* slave tracking */

	static const int delta_accumulator_size = 25;
	int delta_accumulator_cnt;
	long delta_accumulator[delta_accumulator_size];
	long average_slave_delta;
	int  average_dir;
	bool have_first_delta_accumulator;
	
	enum SlaveState {
		Stopped,
		Waiting,
		Running
	};
	
	SlaveState slave_state;
	nframes_t slave_wait_end;

	void reset_slave_state ();
	bool follow_slave (nframes_t, nframes_t);
	void set_slave_source (SlaveSource);

	bool _exporting;
	int prepare_to_export (ARDOUR::AudioExportSpecification&);

	void prepare_diskstreams ();
	void commit_diskstreams (nframes_t, bool& session_requires_butler);
	int  process_routes (nframes_t, nframes_t);
	int  silent_process_routes (nframes_t, nframes_t);

	bool get_rec_monitors_input () {
		if (actively_recording()) {
			return true;
		} else {
			if (Config->get_auto_input()) {
				return false;
			} else {
				return true;
			}
		}
	}

	int get_transport_declick_required () {

		if (transport_sub_state & PendingDeclickIn) {
			transport_sub_state &= ~PendingDeclickIn;
			return 1;
		} else if (transport_sub_state & PendingDeclickOut) {
			return -1;
		} else {
			return 0;
		}
	}

	bool maybe_stop (nframes_t limit) {
		if ((_transport_speed > 0.0f && _transport_frame >= limit) || (_transport_speed < 0.0f && _transport_frame == 0)) {
			stop_transport ();
			return true;
		}
		return false;
	}

	bool maybe_sync_start (nframes_t&, nframes_t&);

	void check_declick_out ();

	MIDI::MachineControl*    mmc;
	MIDI::Port*             _mmc_port;
	MIDI::Port*             _mtc_port;
	MIDI::Port*             _midi_port;
	string                  _path;
	string                  _name;
	bool                     session_send_mmc;
	bool                     session_send_mtc;
	bool                     session_midi_feedback;
	bool                     play_loop;
	bool                     loop_changing;
	nframes_t           last_loopend;

	RingBuffer<Event*> pending_events;

	void hookup_io ();
	void when_engine_running ();
	sigc::connection first_time_running;
	void graph_reordered ();

	string _current_snapshot_name;

	XMLTree* state_tree;
	bool     state_was_pending;
	StateOfTheState _state_of_the_state;

	void     auto_save();
	int      load_options (const XMLNode&);
	XMLNode& get_options () const;
	int      load_state (string snapshot_name);
	bool     save_config_options_predicate (ConfigVariableBase::Owner owner) const;

	nframes_t   _last_roll_location;
	nframes_t   _last_record_location;
	bool              pending_locate_roll;
	nframes_t    pending_locate_frame;

	bool              pending_locate_flush;
	bool              pending_abort;
	bool              pending_auto_loop;
	
	Sample*           butler_mixdown_buffer;
	float*            butler_gain_buffer;
	pthread_t         butler_thread;
	Glib::Mutex       butler_request_lock;
        Glib::Cond        butler_paused;
	bool              butler_should_run;
	mutable gint      butler_should_do_transport_work;
	int               butler_request_pipe[2];
	
	struct ButlerRequest {
	    enum Type {
		    Wake,
		    Run,
		    Pause,
		    Quit
	    };
	};

	enum PostTransportWork {
		PostTransportStop               = 0x1,
		PostTransportDisableRecord      = 0x2,
		PostTransportPosition           = 0x8,
		PostTransportDidRecord          = 0x20,
		PostTransportDuration           = 0x40,
		PostTransportLocate             = 0x80,
		PostTransportRoll               = 0x200,
		PostTransportAbort              = 0x800,
		PostTransportOverWrite          = 0x1000,
		PostTransportSpeed              = 0x2000,
		PostTransportAudition           = 0x4000,
		PostTransportScrub              = 0x8000,
		PostTransportReverse            = 0x10000,
		PostTransportInputChange        = 0x20000,
		PostTransportCurveRealloc       = 0x40000
	};
	
	static const PostTransportWork ProcessCannotProceedMask = 
		PostTransportWork (PostTransportInputChange|
				   PostTransportSpeed|
				   PostTransportReverse|
				   PostTransportCurveRealloc|
				   PostTransportScrub|
				   PostTransportAudition|
				   PostTransportLocate|
				   PostTransportStop);
	
	PostTransportWork post_transport_work;

	void             summon_butler ();
	void             schedule_butler_transport_work ();
	int              start_butler_thread ();
	void             terminate_butler_thread ();
	static void    *_butler_thread_work (void *arg);
	void*            butler_thread_work ();

	uint32_t    cumulative_rf_motion;
	uint32_t    rf_scale;

	void set_rf_speed (float speed);
	void reset_rf_scale (nframes_t frames_moved);

	Locations        _locations;
	void              locations_changed ();
	void              locations_added (Location*);
	void              handle_locations_changed (Locations::LocationList&);

	sigc::connection auto_punch_start_changed_connection;
	sigc::connection auto_punch_end_changed_connection;
	sigc::connection auto_punch_changed_connection;
	void             auto_punch_start_changed (Location *);
	void             auto_punch_end_changed (Location *);
	void             auto_punch_changed (Location *);

	sigc::connection auto_loop_start_changed_connection;
	sigc::connection auto_loop_end_changed_connection;
	sigc::connection auto_loop_changed_connection;
	void             auto_loop_changed (Location *);

	typedef list<Event *> Events;
	Events           events;
	Events           immediate_events;
	Events::iterator next_event;

	/* there can only ever be one of each of these */

	Event *auto_loop_event;
	Event *punch_out_event;
	Event *punch_in_event;

	/* events */

	void dump_events () const;
	void queue_event (Event *ev);
	void merge_event (Event*);
	void replace_event (Event::Type, nframes_t action_frame, nframes_t target = 0);
	bool _replace_event (Event*);
	bool _remove_event (Event *);
	void _clear_event_type (Event::Type);

	void first_stage_init (string path, string snapshot_name);
	int  second_stage_init (bool new_tracks);
	void find_current_end ();
	void remove_empty_sounds ();

	void setup_midi_control ();
	int  midi_read (MIDI::Port *);

	void enable_record ();
	
	void increment_transport_position (uint32_t val) {
		if (max_frames - val < _transport_frame) {
			_transport_frame = max_frames;
		} else {
			_transport_frame += val;
		}
	}

	void decrement_transport_position (uint32_t val) {
		if (val < _transport_frame) {
			_transport_frame -= val;
		} else {
			_transport_frame = 0;
		}
	}

	void post_transport_motion ();
	static void *session_loader_thread (void *arg);

	void *do_work();

	void set_next_event ();
	void process_event (Event *);

	/* MIDI Machine Control */

	void deliver_mmc (MIDI::MachineControl::Command, nframes_t);
	void deliver_midi_message (MIDI::Port * port, MIDI::eventType ev, MIDI::channel_t, MIDI::EventTwoBytes);
	void deliver_data (MIDI::Port* port, MIDI::byte*, int32_t size);

	void spp_start (MIDI::Parser&);
	void spp_continue (MIDI::Parser&);
	void spp_stop (MIDI::Parser&);

	void mmc_deferred_play (MIDI::MachineControl &);
	void mmc_stop (MIDI::MachineControl &);
	void mmc_step (MIDI::MachineControl &, int);
	void mmc_pause (MIDI::MachineControl &);
	void mmc_record_pause (MIDI::MachineControl &);
	void mmc_record_strobe (MIDI::MachineControl &);
	void mmc_record_exit (MIDI::MachineControl &);
	void mmc_track_record_status (MIDI::MachineControl &, 
				      uint32_t track, bool enabled);
	void mmc_fast_forward (MIDI::MachineControl &);
	void mmc_rewind (MIDI::MachineControl &);
	void mmc_locate (MIDI::MachineControl &, const MIDI::byte *);
	void mmc_shuttle (MIDI::MachineControl &mmc, float speed, bool forw);
	void mmc_record_enable (MIDI::MachineControl &mmc, size_t track, bool enabled);

	struct timeval last_mmc_step;
	double step_speed;

	typedef sigc::slot<bool> MidiTimeoutCallback;
	typedef list<MidiTimeoutCallback> MidiTimeoutList;

	MidiTimeoutList midi_timeouts;
	bool mmc_step_timeout ();

	MIDI::byte mmc_buffer[32];
	MIDI::byte mtc_msg[16];
	MIDI::byte mtc_smpte_bits;   /* encoding of SMTPE type for MTC */
	MIDI::byte midi_msg[16];
	nframes_t  outbound_mtc_smpte_frame;
	SMPTE::Time transmitting_smpte_time;
	int next_quarter_frame_to_send;
	
	double _frames_per_smpte_frame; /* has to be floating point because of drop frame */
	nframes_t _frames_per_hour;
	nframes_t _smpte_frames_per_hour;
	nframes_t _smpte_offset;
	bool _smpte_offset_negative;
	
	/* cache the most-recently requested time conversions.
	   this helps when we have multiple clocks showing the
	   same time (e.g. the transport frame)
	*/

	bool       last_smpte_valid;
	nframes_t  last_smpte_when;
	SMPTE::Time last_smpte;

	int send_full_time_code ();
	int send_midi_time_code ();

	void send_full_time_code_in_another_thread ();
	void send_midi_time_code_in_another_thread ();
	void send_time_code_in_another_thread (bool full);
	void send_mmc_in_another_thread (MIDI::MachineControl::Command, nframes_t frame = 0);

	nframes_t adjust_apparent_position (nframes_t frames);
	
	void reset_record_status ();
	
	int no_roll (nframes_t nframes, nframes_t offset);
	
	bool non_realtime_work_pending() const { return static_cast<bool>(post_transport_work); }
	bool process_can_proceed() const { return !(post_transport_work & ProcessCannotProceedMask); }

	struct MIDIRequest {
	    
	    enum Type {
		    SendFullMTC,
		    SendMTC,
		    SendMMC,
		    PortChange,
		    SendMessage,
		    Deliver,
		    Quit
	    };
	    
	    Type type;
	    MIDI::MachineControl::Command mmc_cmd;
	    nframes_t locate_frame;

	    // for SendMessage type

	    MIDI::Port * port;
	    MIDI::channel_t chan;
	    union {
		MIDI::EventTwoBytes data;
		MIDI::byte* buf;
	    };

	    union { 
		MIDI::eventType ev;
		int32_t size;
	    };

	    MIDIRequest () {}
	    
	    void *operator new(size_t ignored) {
		    return pool.alloc ();
	    };

	    void operator delete(void *ptr, size_t size) {
		    pool.release (ptr);
	    }

	  private:
	    static MultiAllocSingleReleasePool pool;
	};

	Glib::Mutex       midi_lock;
	pthread_t       midi_thread;
	int             midi_request_pipe[2];
	mutable  gint   butler_active;
	RingBuffer<MIDIRequest*> midi_requests;

	int           start_midi_thread ();
	void          terminate_midi_thread ();
	void          poke_midi_thread ();
	static void *_midi_thread_work (void *arg);
	void          midi_thread_work ();
	void          change_midi_ports ();
	int           use_config_midi_ports ();

	bool waiting_to_start;

	void set_play_loop (bool yn);
	void overwrite_some_buffers (Diskstream*);
	void flush_all_redirects ();
	void locate (nframes_t, bool with_roll, bool with_flush, bool with_loop=false);
	void start_locate (nframes_t, bool with_roll, bool with_flush, bool with_loop=false);
	void force_locate (nframes_t frame, bool with_roll = false);
	void set_diskstream_speed (Diskstream*, float speed);
	void set_transport_speed (float speed, bool abort = false);
	void stop_transport (bool abort = false);
	void start_transport ();
	void actually_start_transport ();
	void realtime_stop (bool abort);
	void non_realtime_start_scrub ();
	void non_realtime_set_speed ();
	void non_realtime_stop (bool abort);
	void non_realtime_overwrite ();
	void non_realtime_buffer_fill ();
	void butler_transport_work ();
	void post_transport ();
	void engine_halted ();
	void xrun_recovery ();

	TempoMap    *_tempo_map;
	void          tempo_map_changed (Change);

	/* edit/mix groups */

	int load_route_groups (const XMLNode&, bool is_edit);
	int load_edit_groups (const XMLNode&);
	int load_mix_groups (const XMLNode&);


	list<RouteGroup *> edit_groups;
	list<RouteGroup *> mix_groups;

	/* disk-streams */

	SerializedRCUManager<DiskstreamList>  diskstreams; 

	uint32_t dstream_buffer_size;
	int  load_diskstreams (const XMLNode&);

	/* routes stuff */

	SerializedRCUManager<RouteList>  routes;

	void   add_routes (RouteList&, bool save = true);
	uint32_t destructive_index;

	int load_routes (const XMLNode&);
	boost::shared_ptr<Route> XMLRouteFactory (const XMLNode&);

	/* mixer stuff */

	bool       solo_update_disabled;
	bool       currently_soloing;
	
	void route_mute_changed (void *src);
	void route_solo_changed (void *src, boost::shared_ptr<Route>);
	void catch_up_on_solo ();
	void update_route_solo_state ();
	void modify_solo_mute (bool, bool);
	void strip_portname_for_solo (string& portname);

	/* REGION MANAGEMENT */

	mutable Glib::Mutex region_lock;
	typedef map<PBD::ID,boost::shared_ptr<AudioRegion> > AudioRegionList;
	AudioRegionList audio_regions;
	
	void region_renamed (boost::shared_ptr<Region>);
	void region_changed (Change, boost::shared_ptr<Region>);
	void add_region (boost::shared_ptr<Region>);
	void remove_region (boost::shared_ptr<Region>);

	int load_regions (const XMLNode& node);

	/* SOURCES */
	
	mutable Glib::Mutex audio_source_lock;
	typedef std::map<PBD::ID,boost::shared_ptr<AudioSource> > AudioSourceList;

	AudioSourceList audio_sources;

	int load_sources (const XMLNode& node);
	XMLNode& get_sources_as_xml ();

	boost::shared_ptr<Source> XMLSourceFactory (const XMLNode&);

	/* PLAYLISTS */
	
	mutable Glib::Mutex playlist_lock;
	typedef set<Playlist *> PlaylistList;
	PlaylistList playlists;
	PlaylistList unused_playlists;

	int load_playlists (const XMLNode&);
	int load_unused_playlists (const XMLNode&);
	void remove_playlist (Playlist *);
	void track_playlist (Playlist *, bool);

	Playlist *playlist_factory (string name);
	Playlist *XMLPlaylistFactory (const XMLNode&);

	void playlist_length_changed (Playlist *);
	void diskstream_playlist_changed (boost::shared_ptr<Diskstream>);

	/* NAMED SELECTIONS */

	mutable Glib::Mutex named_selection_lock;
	typedef set<NamedSelection *> NamedSelectionList;
	NamedSelectionList named_selections;

	int load_named_selections (const XMLNode&);

	NamedSelection *named_selection_factory (string name);
	NamedSelection *XMLNamedSelectionFactory (const XMLNode&);

        /* CURVES and AUTOMATION LISTS */
        std::map<PBD::ID, Curve*> curves;
        std::map<PBD::ID, AutomationList*> automation_lists;

	/* DEFAULT FADE CURVES */

	float default_fade_steepness;
	float default_fade_msecs;

	/* AUDITIONING */

	boost::shared_ptr<Auditioner> auditioner;
	void set_audition (boost::shared_ptr<Region>);
	void non_realtime_set_audition ();
	boost::shared_ptr<Region> pending_audition_region;

	/* EXPORT */

	/* FLATTEN */

	int flatten_one_track (AudioTrack&, nframes_t start, nframes_t cnt);

	/* INSERT AND SEND MANAGEMENT */
	
	list<PortInsert *>   _port_inserts;
	list<PluginInsert *> _plugin_inserts;
	list<Send *>         _sends;
	uint32_t          send_cnt;
	uint32_t          insert_cnt;

	void add_redirect (Redirect *);
	void remove_redirect (Redirect *);

	/* S/W RAID */

	struct space_and_path {
	    uint32_t blocks; /* 4kB blocks */
	    string path;
	    
	    space_and_path() { 
		    blocks = 0;
	    }
	};

	struct space_and_path_ascending_cmp {
	    bool operator() (space_and_path a, space_and_path b) {
		    return a.blocks > b.blocks;
	    }
	};
	
	void setup_raid_path (string path);

	vector<space_and_path> session_dirs;
	vector<space_and_path>::iterator last_rr_session_dir;
	uint32_t _total_free_4k_blocks;
	Glib::Mutex space_lock;

	static const char* old_sound_dir_name;
	static const char* sound_dir_name;
	static const char* dead_sound_dir_name;
	static const char* interchange_dir_name;
	static const char* peak_dir_name;

	string discover_best_sound_dir (bool destructive = false);
	int ensure_sound_dir (string, string&);
	void refresh_disk_space ();

	mutable gint _playback_load;
	mutable gint _capture_load;
	mutable gint _playback_load_min;
	mutable gint _capture_load_min;

	/* I/O Connections */

	typedef list<Connection *> ConnectionList;
	mutable Glib::Mutex connection_lock;
	ConnectionList _connections;
	int load_connections (const XMLNode&);

	void reverse_diskstream_buffers ();

	UndoHistory history;
	UndoTransaction* current_trans;

	GlobalRouteBooleanState get_global_route_boolean (bool (Route::*method)(void) const);
	GlobalRouteMeterState get_global_route_metering ();

	void set_global_route_boolean (GlobalRouteBooleanState s, void (Route::*method)(bool, void*), void *arg);
	void set_global_route_metering (GlobalRouteMeterState s, void *arg);

	void set_global_mute (GlobalRouteBooleanState s, void *src);
	void set_global_solo (GlobalRouteBooleanState s, void *src);
	void set_global_record_enable (GlobalRouteBooleanState s, void *src);

	void jack_timebase_callback (jack_transport_state_t, nframes_t, jack_position_t*, int);
	int  jack_sync_callback (jack_transport_state_t, jack_position_t*);
	void record_enable_change_all (bool yn);

	XMLNode& state(bool);

	/* click track */

	struct Click {
	    nframes_t start;
	    nframes_t duration;
	    nframes_t offset;
	    const Sample *data;

	    Click (nframes_t s, nframes_t d, const Sample *b) 
		    : start (s), duration (d), data (b) { offset = 0; }
	    
	    void *operator new(size_t ignored) {
		    return pool.alloc ();
	    };

	    void operator delete(void *ptr, size_t size) {
		    pool.release (ptr);
	    }

          private:
	    static Pool pool;
	};
 
	typedef list<Click*> Clicks;

	Clicks          clicks;
	bool           _clicking;
	boost::shared_ptr<IO> _click_io;
	Sample*         click_data;
	Sample*         click_emphasis_data;
	nframes_t  click_length;
	nframes_t  click_emphasis_length;
	mutable Glib::RWLock click_lock;

	static const Sample         default_click[];
	static const nframes_t default_click_length;
	static const Sample         default_click_emphasis[];
	static const nframes_t default_click_emphasis_length;

	Click *get_click();
	void   setup_click_sounds (int which);
	void   clear_clicks ();
	void   click (nframes_t start, nframes_t nframes, nframes_t offset);

	vector<Route*> master_outs;
	
	/* range playback */

	list<AudioRange> current_audio_range;
	bool _play_range;
	void set_play_range (bool yn);
	void setup_auto_play ();

	/* main outs */
	uint32_t main_outs;
	
	boost::shared_ptr<IO> _master_out;
	boost::shared_ptr<IO> _control_out;

	gain_t* _gain_automation_buffer;
	pan_t** _pan_automation_buffer;
	void allocate_pan_automation_buffers (nframes_t nframes, uint32_t howmany, bool force);
	uint32_t _npan_buffers;

	/* VST support */

	long _vst_callback (VSTPlugin*,
			    long opcode,
			    long index,
			    long value,
			    void* ptr,
			    float opt);

	/* number of hardware audio ports we're using,
	   based on max (requested,available)
	*/

	uint32_t n_physical_outputs;
	uint32_t n_physical_inputs;

	void remove_pending_capture_state ();

	int find_all_sources (std::string path, std::set<std::string>& result);
	int find_all_sources_across_snapshots (std::set<std::string>& result, bool exclude_this_snapshot);

	LayerModel layer_model;
	CrossfadeModel xfade_model;

	typedef std::list<PBD::Controllable*> Controllables;
	Glib::Mutex controllables_lock;
	Controllables controllables;

	void add_controllable (PBD::Controllable*);
	void remove_controllable (PBD::Controllable*);


	void reset_native_file_format();
	bool first_file_data_format_reset;
	bool first_file_header_format_reset;

	void config_changed (const char*);
};

} // namespace ARDOUR

#endif /* __ardour_session_h__ */
