/*
 * Copyright (C) 2000-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005-2009 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006-2007 Jesse Chappell <jesse@essej.net>
 * Copyright (C) 2006-2009 Sampo Savolainen <v2@iki.fi>
 * Copyright (C) 2006-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2007-2015 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2008-2009 Hans Baier <hansfbaier@googlemail.com>
 * Copyright (C) 2008-2009 Sakari Bergen <sakari.bergen@beatwaves.net>
 * Copyright (C) 2012-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2013-2017 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2013-2018 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2014-2018 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2015-2018 Len Ovens <len@ovenwerks.net>
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

#ifndef __ardour_session_h__
#define __ardour_session_h__

#ifdef WAF_BUILD
#include "libardour-config.h"
#endif

#include <exception>
#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <queue>
#include <stdint.h>

#include <boost/dynamic_bitset.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/utility.hpp>

#include <glibmm/threads.h>

#include <ltc.h>

#include "pbd/error.h"
#include "pbd/event_loop.h"
#include "pbd/file_archive.h"
#include "pbd/rcu.h"
#include "pbd/reallocpool.h"
#include "pbd/statefuldestructible.h"
#include "pbd/signals.h"
#include "pbd/undo.h"

#include "lua/luastate.h"

#include "evoral/Range.h"

#include "midi++/types.h"
#include "midi++/mmc.h"

#include "temporal/time.h"

#include "ardour/ardour.h"
#include "ardour/chan_count.h"
#include "ardour/delivery.h"
#include "ardour/interthread_info.h"
#include "ardour/luascripting.h"
#include "ardour/location.h"
#include "ardour/monitor_processor.h"
#include "ardour/presentation_info.h"
#include "ardour/rc_configuration.h"
#include "ardour/session_configuration.h"
#include "ardour/session_event.h"
#include "ardour/interpolation.h"
#include "ardour/plugin.h"
#include "ardour/presentation_info.h"
#include "ardour/route.h"
#include "ardour/route_graph.h"
#include "ardour/transport_api.h"

class XMLTree;
class XMLNode;
struct _AEffect;
typedef struct _AEffect AEffect;

class PTFFormat;

namespace MIDI {
class Port;
class MachineControl;
class Parser;
}

namespace PBD {
class Controllable;
}

namespace luabridge {
	class LuaRef;
}

namespace Evoral {
class Curve;
}

namespace ARDOUR {

class Amp;
class AsyncMIDIPort;
class AudioEngine;
class AudioFileSource;
class AudioRegion;
class AudioSource;
class AudioTrack;
class Auditioner;
class AutomationList;
class AuxInput;
class BufferSet;
class Bundle;
class Butler;
class Click;
class CoreSelection;
class ExportHandler;
class ExportStatus;
class Graph;
class IO;
class IOProcessor;
class ImportStatus;
class MidiClockTicker;
class MidiControlUI;
class MidiPortManager;
class MidiPort;
class MidiRegion;
class MidiSource;
class MidiTrack;
class Playlist;
class PluginInsert;
class PluginInfo;
class Port;
class PortInsert;
class ProcessThread;
class Progress;
class Processor;
class Region;
class Return;
class Route;
class RouteGroup;
class RTTaskList;
class SMFSource;
class Send;
class SceneChanger;
class SessionDirectory;
class SessionMetadata;
class SessionPlaylists;
class Source;
class Speakers;
class TempoMap;
class TransportMaster;
struct TransportFSM;
class Track;
class UI_TransportMaster;
class VCAManager;
class WindowsVSTPlugin;

extern void setup_enum_writer ();

class LIBARDOUR_API SessionException: public std::exception {
public:
	explicit SessionException(const std::string msg) : _message(msg) {}
	virtual ~SessionException() throw() {}

	virtual const char* what() const throw() { return _message.c_str(); }

private:
	std::string _message;
};

/** Ardour Session */
class LIBARDOUR_API Session : public PBD::StatefulDestructible, public PBD::ScopedConnectionList, public SessionEventManager, public TransportAPI
{
private:

public:
	enum RecordState {
		Disabled = 0,
		Enabled = 1,
		Recording = 2
	};

	/* a new session might have non-empty mix_template, an existing session should always have an empty one.
	   the bus profile can be null if no master out bus is required.
	*/

	Session (AudioEngine&,
	         const std::string& fullpath,
	         const std::string& snapshot_name,
	         BusProfile const * bus_profile = 0,
	         std::string mix_template = "");

	virtual ~Session ();

	static int get_info_from_path (const std::string& xmlpath, float& sample_rate, SampleFormat& data_format, std::string& program_version);
	static std::string get_snapshot_from_instant (const std::string& session_dir);

	/** a monotonic counter used for naming user-visible things uniquely
	 * (curently the sidechain port).
	 * Use sparingly to keep the numbers low, prefer PBD::ID for all
	 * internal, not user-visible IDs */
	static unsigned int next_name_id ();

	std::string path() const { return _path; }
	std::string name() const { return _name; }
	std::string snap_name() const { return _current_snapshot_name; }
	std::string raid_path () const;
	bool path_is_within_session (const std::string&);

	bool writable() const { return _writable; }
	void set_clean ();   // == Clean and emit DirtyChanged IFF session was Dirty
	void set_dirty ();   // |= Dirty and emit DirtyChanged (unless already dirty or Loading, Deletion)
	void unset_dirty (bool emit_dirty_changed = false); // &= ~Dirty
	void set_deletion_in_progress ();   // |= Deletion
	void clear_deletion_in_progress (); // &= ~Deletion

	bool reconnection_in_progress () const         { return _reconnecting_routes_in_progress; }
	bool routes_deletion_in_progress () const      { return _route_deletion_in_progress; }
	bool dirty () const                            { return _state_of_the_state & Dirty; }
	bool deletion_in_progress () const             { return _state_of_the_state & Deletion; }
	bool peaks_cleanup_in_progres () const         { return _state_of_the_state & PeakCleanup; }
	bool loading () const                          { return _state_of_the_state & Loading; }
	bool cannot_save () const                      { return _state_of_the_state & CannotSave; }
	bool in_cleanup () const                       { return _state_of_the_state & InCleanup; }
	bool inital_connect_or_deletion_in_progress () { return _state_of_the_state & (InitialConnecting | Deletion); }

	PBD::Signal0<void> DirtyChanged;

	const SessionDirectory& session_directory () const { return *(_session_dir.get()); }

	static PBD::Signal1<void,std::string> Dialog;

	PBD::Signal0<void> BatchUpdateStart;
	PBD::Signal0<void> BatchUpdateEnd;

	int ensure_subdirs ();

	std::string automation_dir () const;  ///< Automation data
	std::string analysis_dir () const;    ///< Analysis data
	std::string plugins_dir () const;     ///< Plugin state
	std::string externals_dir () const;   ///< Links to external files

	std::string construct_peak_filepath (const std::string& audio_path, const bool in_session = false, const bool old_peak_name = false) const;

	bool audio_source_name_is_unique (const std::string& name);
	std::string format_audio_source_name (const std::string& legalized_base, uint32_t nchan, uint32_t chan, bool destructive, bool take_required, uint32_t cnt, bool related_exists);
	std::string new_audio_source_path_for_embedded (const std::string& existing_path);
	std::string new_audio_source_path (const std::string&, uint32_t nchans, uint32_t chan, bool destructive, bool take_required);
	std::string new_midi_source_path (const std::string&, bool need_source_lock = true);

	/** create a new track or bus from a template (XML path)
	 * @param how_many how many tracks or busses to create
	 * @param template_path path to xml template file
	 * @param insert_at position where to add new track, use PresentationInfo::max_order to append at the end.
	 * @param name name (prefix) of the route to create
	 * @param pd Playlist disposition
	 * @return list of newly created routes
	 */
	RouteList new_route_from_template (uint32_t how_many, PresentationInfo::order_t insert_at, const std::string& template_path, const std::string& name, PlaylistDisposition pd = NewPlaylist);
	RouteList new_route_from_template (uint32_t how_many, PresentationInfo::order_t insert_at, XMLNode&, const std::string& name, PlaylistDisposition pd = NewPlaylist);
	std::vector<std::string> get_paths_for_new_sources (bool allow_replacing, const std::string& import_file_path,
	                                                    uint32_t channels, std::vector<std::string> const & smf_track_names);

	int bring_all_sources_into_session (boost::function<void(uint32_t,uint32_t,std::string)> callback);

	void process (pframes_t nframes);

	BufferSet& get_silent_buffers (ChanCount count = ChanCount::ZERO);
	BufferSet& get_noinplace_buffers (ChanCount count = ChanCount::ZERO);
	BufferSet& get_scratch_buffers (ChanCount count = ChanCount::ZERO, bool silence = true);
	BufferSet& get_route_buffers (ChanCount count = ChanCount::ZERO, bool silence = true);
	BufferSet& get_mix_buffers (ChanCount count = ChanCount::ZERO);

	bool have_rec_enabled_track () const;
    bool have_rec_disabled_track () const;

	bool have_captured() const { return _have_captured; }

	void refill_all_track_buffers ();
	Butler* butler() { return _butler; }
	void butler_transport_work ();

	void refresh_disk_space ();

	int load_routes (const XMLNode&, int);
	boost::shared_ptr<RouteList> get_routes() const {
		return routes.reader ();
	}

	boost::shared_ptr<RTTaskList> rt_tasklist () { return _rt_tasklist; }

	RouteList get_routelist (bool mixer_order = false, PresentationInfo::Flag fl = PresentationInfo::MixerRoutes) const;

	CoreSelection& selection () { return *_selection; }

	/* because the set of Stripables consists of objects managed
	 * independently, in multiple containers within the Session (or objects
	 * owned by the session), we fill out a list in-place rather than
	 * return a pointer to a copy of the (RCU) managed list, as happens
	 * with get_routes()
	 */

	void get_stripables (StripableList&, PresentationInfo::Flag fl = PresentationInfo::MixerStripables) const;
	StripableList get_stripables () const;
	boost::shared_ptr<RouteList> get_tracks() const;
	boost::shared_ptr<RouteList> get_routes_with_internal_returns() const;
	boost::shared_ptr<RouteList> get_routes_with_regions_at (samplepos_t const) const;

	boost::shared_ptr<AudioTrack> get_nth_audio_track (int nth) const;

	uint32_t nstripables (bool with_monitor = false) const;
	uint32_t nroutes() const { return routes.reader()->size(); }
	uint32_t ntracks () const;
	uint32_t nbusses () const;

	bool plot_process_graph (std::string const& file_name) const;

	boost::shared_ptr<BundleList> bundles () {
		return _bundles.reader ();
	}

	void notify_presentation_info_change ();

	template<class T> void foreach_route (T *obj, void (T::*func)(Route&), bool sort = true);
	template<class T> void foreach_route (T *obj, void (T::*func)(boost::shared_ptr<Route>), bool sort = true);
	template<class T, class A> void foreach_route (T *obj, void (T::*func)(Route&, A), A arg, bool sort = true);

	static char session_name_is_legal (const std::string&);
	bool io_name_is_legal (const std::string&) const;
	boost::shared_ptr<Route> route_by_name (std::string) const;
	boost::shared_ptr<Route> route_by_id (PBD::ID) const;
	boost::shared_ptr<Stripable> stripable_by_id (PBD::ID) const;
	boost::shared_ptr<Stripable> get_remote_nth_stripable (PresentationInfo::order_t n, PresentationInfo::Flag) const;
	boost::shared_ptr<Route> get_remote_nth_route (PresentationInfo::order_t n) const;
	boost::shared_ptr<Route> route_by_selected_count (uint32_t cnt) const;
	void routes_using_input_from (const std::string& str, RouteList& rl);

	bool route_name_unique (std::string) const;
	bool route_name_internal (std::string) const;

	uint32_t track_number_decimals () const {
		return _track_number_decimals;
	}

	bool get_record_enabled() const {
		return (record_status () >= Enabled);
	}

	RecordState record_status() const {
		return (RecordState) g_atomic_int_get (&_record_status);
	}

	bool actively_recording () const {
		return record_status() == Recording;
	}

	bool record_enabling_legal () const;
	void maybe_enable_record (bool rt_context = false);
	void disable_record (bool rt_context, bool force = false);
	void step_back_from_record ();

	void set_all_tracks_record_enabled(bool);

	void maybe_write_autosave ();

	/* Emitted when all i/o connections are complete */

	PBD::Signal0<void> IOConnectionsComplete;

	/* Record status signals */

	PBD::Signal0<void> RecordStateChanged; /* signals changes in recording state (i.e. are we recording) */
	/* XXX may 2015: paul says: it isn't clear to me that this has semantics that cannot be inferrred
	   from the previous signal and session state.
	*/
	PBD::Signal0<void> RecordArmStateChanged; /* signals changes in recording arming */

	/* Emited when session is loaded */
	PBD::Signal0<void> SessionLoaded;

	/* Transport mechanism signals */

	/** Emitted on the following changes in transport state:
	 *  - stop (from the butler thread)
	 *  - change in whether or not we are looping (from the process thread)
	 *  - change in the play range (from the process thread)
	 *  - start (from the process thread)
	 *  - engine halted
	 */
	PBD::Signal0<void> TransportStateChange;

	PBD::Signal1<void,samplepos_t> PositionChanged; /* sent after any non-sequential motion */
	PBD::Signal1<void,samplepos_t> Xrun;
	PBD::Signal0<void> TransportLooped;

	/** emitted when a locate has occurred */
	PBD::Signal0<void> Located;

	PBD::Signal1<void,RouteList&> RouteAdded;
	/** Emitted when a property of one of our route groups changes.
	 *  The parameter is the RouteGroup that has changed.
	 */
	PBD::Signal1<void, RouteGroup *> RouteGroupPropertyChanged;
	/** Emitted when a route is added to one of our route groups.
	 *  First parameter is the RouteGroup, second is the route.
	 */
	PBD::Signal2<void, RouteGroup *, boost::weak_ptr<Route> > RouteAddedToRouteGroup;
	/** Emitted when a route is removed from one of our route groups.
	 *  First parameter is the RouteGroup, second is the route.
	 */
	PBD::Signal2<void, RouteGroup *, boost::weak_ptr<Route> > RouteRemovedFromRouteGroup;

	/** Emitted when a foldback send is created or deleted
	 */
	PBD::Signal0<void> FBSendsChanged;

	/* Step Editing status changed */
	PBD::Signal1<void,bool> StepEditStatusChange;

	/* Timecode state signals */
	PBD::Signal0<void> MtcOrLtcInputPortChanged;

	void queue_event (SessionEvent*);

	void request_roll_at_and_return (samplepos_t start, samplepos_t return_to);
	void request_bounded_roll (samplepos_t start, samplepos_t end);
	void request_stop (bool abort = false, bool clear_state = false, TransportRequestSource origin = TRS_UI);
	void request_locate (samplepos_t sample, LocateTransportDisposition ltd = RollIfAppropriate, TransportRequestSource origin = TRS_UI);

	void request_play_loop (bool yn, bool leave_rolling = false);
	bool get_play_loop () const { return play_loop; }

	samplepos_t last_transport_start () const { return _last_roll_location; }
	void goto_end ();
	void goto_start (bool and_roll = false);
	void use_rf_shuttle_speed ();
	void allow_auto_play (bool yn);
	void request_transport_speed (double speed, bool as_default = true, TransportRequestSource origin = TRS_UI);
	void request_transport_speed_nonzero (double, bool as_default = true, TransportRequestSource origin = TRS_UI);
	void request_overwrite_buffer (boost::shared_ptr<Track>, OverwriteReason);
	void adjust_playback_buffering();
	void adjust_capture_buffering();

	bool global_locate_pending() const { return _global_locate_pending; }
	bool locate_pending() const;
	bool locate_initiated() const;
	bool declick_in_progress () const;
	bool transport_locked () const;

	int wipe ();

	samplepos_t current_end_sample () const;
	samplepos_t current_start_sample () const;
	/** "actual" sample rate of session, set by current audioengine rate, pullup/down etc. */
	samplecnt_t sample_rate () const { return _current_sample_rate; }
	/** "native" sample rate of session, regardless of current audioengine rate, pullup/down etc */
	samplecnt_t nominal_sample_rate () const { return _nominal_sample_rate; }
	samplecnt_t frames_per_hour () const { return _frames_per_hour; }

	double samples_per_timecode_frame() const { return _samples_per_timecode_frame; }
	samplecnt_t timecode_frames_per_hour() const { return _timecode_frames_per_hour; }

	MIDI::byte get_mtc_timecode_bits() const {
		return mtc_timecode_bits;   /* encoding of SMTPE type for MTC */
	}

	double timecode_frames_per_second() const;
	bool timecode_drop_frames() const;

	/* Locations */

	Locations *locations() { return _locations; }

	PBD::Signal1<void,Location*>    auto_loop_location_changed;
	PBD::Signal1<void,Location*>    auto_punch_location_changed;
	PBD::Signal0<void>              locations_modified;

	void set_auto_punch_location (Location *);
	void set_auto_loop_location (Location *);
	void set_session_extents (samplepos_t start, samplepos_t end);
	bool session_range_is_free () const { return _session_range_is_free; }
	void set_session_range_is_free (bool);

	pframes_t get_block_size () const         { return current_block_size; }
	samplecnt_t worst_output_latency () const { return _worst_output_latency; }
	samplecnt_t worst_input_latency () const  { return _worst_input_latency; }
	samplecnt_t worst_route_latency () const  { return _worst_route_latency; }
	samplecnt_t worst_latency_preroll () const;
	samplecnt_t worst_latency_preroll_buffer_size_ceil () const;

	PBD::Signal0<void> LatencyUpdated;

	struct SaveAs {
		std::string new_parent_folder;  /* parent folder where new session folder will be created */
		std::string new_name;           /* name of newly saved session */
		bool        switch_to;     /* true if we should be working on newly saved session after save-as; false otherwise */
		bool        include_media; /* true if the newly saved session should contain references to media */
		bool        copy_media;    /* true if media files (audio, media, etc) should be copied into newly saved session; false otherwise */
		bool        copy_external; /* true if external media should be consolidated into the newly saved session; false otherwise */

		std::string final_session_folder_name; /* filled in by * Session::save_as(), provides full path to newly saved session */

		/* emitted as we make progress. 3 arguments passed to signal
		 * handler:
		 *
		 *  1: percentage complete measured as a fraction (0-1.0) of
		 *     total data copying done.
		 *  2: number of files copied so far
		 *  3: total number of files to copy
		 *
		 * Handler should return true for save-as to continue, or false
		 * to stop (and remove all evidence of partial save-as).
		 */
		PBD::Signal3<bool,float,int64_t,int64_t> Progress;

		/* if save_as() returns non-zero, this string will indicate the reason why.
		 */
		std::string failure_message;
	};

	int save_as (SaveAs&);

	/** save session
	 * @param snapshot_name name of the session (use an empty string for the current name)
	 * @param pending save a 'recovery', not full state (default: false)
	 * @param switch_to_snapshot switch to given snapshot after saving (default: false)
	 * @param template_only save a session template (default: false)
	 * @param for_archive save only data relevant for session-archive
	 * @param only_used_assets skip Sources that are not used, mainly useful with \p for_archive
	 * @return zero on success
	 */
	int save_state (std::string snapshot_name,
	                bool pending = false,
	                bool switch_to_snapshot = false,
	                bool template_only = false,
	                bool for_archive = false,
	                bool only_used_assets = false);

	enum ArchiveEncode {
		NO_ENCODE,
		FLAC_16BIT,
		FLAC_24BIT
	};

	int archive_session (const std::string&, const std::string&,
	                     ArchiveEncode compress_audio = FLAC_16BIT,
	                     PBD::FileArchive::CompressionLevel compression_level = PBD::FileArchive::CompressGood,
	                     bool only_used_sources = false,
	                     Progress* p = 0);

	int restore_state (std::string snapshot_name);
	int save_template (const std::string& template_name, const std::string& description = "", bool replace_existing = false);
	int save_history (std::string snapshot_name = "");
	int restore_history (std::string snapshot_name);
	void remove_state (std::string snapshot_name);
	void rename_state (std::string old_name, std::string new_name);
	void remove_pending_capture_state ();
	int rename (const std::string&);
	bool get_nsm_state () const { return _under_nsm_control; }
	void set_nsm_state (bool state) { _under_nsm_control = state; }
	bool save_default_options ();

	PBD::Signal1<void,std::string> StateSaved;
	PBD::Signal0<void> StateReady;

	/* emitted when session needs to be saved due to some internal
	 * event or condition (i.e. not in response to a user request).
	 *
	 * Only one object should
	 * connect to this signal and take responsibility.
	 *
	 * Argument is the snapshot name to use when saving.
	 */
	PBD::Signal1<void,std::string> SaveSessionRequested;

	/* emitted during a session save to allow other entities to add state, via
	 * extra XML, to the session state
	 */
	PBD::Signal0<void> SessionSaveUnderway;

	std::vector<std::string> possible_states() const;
	static std::vector<std::string> possible_states (std::string path);

	bool export_track_state (boost::shared_ptr<RouteList> rl, const std::string& path);

	/// The instant xml file is written to the session directory
	void add_instant_xml (XMLNode&, bool write_to_config = true);
	XMLNode* instant_xml (const std::string& str);

	enum StateOfTheState {
		Clean = 0x0,
		Dirty = 0x1,
		CannotSave = 0x2,
		Deletion = 0x4,
		InitialConnecting = 0x8,
		Loading = 0x10,
		InCleanup = 0x20,
		PeakCleanup = 0x40
	};

	class StateProtector {
		public:
			StateProtector (Session* s) : _session (s) {
				g_atomic_int_inc (&s->_suspend_save);
			}
			~StateProtector () {
				if (g_atomic_int_dec_and_test (&_session->_suspend_save)) {
					while (_session->_save_queued) {
						_session->_save_queued = false;
						_session->save_state ("");
					}
					while (_session->_save_queued_pending) {
						_session->_save_queued_pending = false;
						_session->save_state ("", true);
					}
				}
			}
		private:
			Session * _session;
	};

	class ProcessorChangeBlocker {
		public:
			ProcessorChangeBlocker (Session* s, bool rc = true)
				: _session (s)
				, _reconfigure_on_delete (rc)
			{
				g_atomic_int_inc (&s->_ignore_route_processor_changes);
			}
			~ProcessorChangeBlocker () {
				if (g_atomic_int_dec_and_test (&_session->_ignore_route_processor_changes)) {
					if (_reconfigure_on_delete) {
						_session->route_processors_changed (RouteProcessorChange ());
					}
				}
			}
		private:
			Session* _session;
			bool _reconfigure_on_delete;
	};

	RouteGroup* new_route_group (const std::string&);
	void add_route_group (RouteGroup *);
	void remove_route_group (RouteGroup* rg) { if (rg) remove_route_group (*rg); }
	void remove_route_group (RouteGroup&);
	void reorder_route_groups (std::list<RouteGroup*>);

	RouteGroup* route_group_by_name (std::string);
	RouteGroup& all_route_group() const;

	PBD::Signal1<void,RouteGroup*> route_group_added;
	PBD::Signal0<void>             route_group_removed;
	PBD::Signal0<void>             route_groups_reordered;

	void foreach_route_group (boost::function<void(RouteGroup*)> f) {
		for (std::list<RouteGroup *>::iterator i = _route_groups.begin(); i != _route_groups.end(); ++i) {
			f (*i);
		}
	}

	std::list<RouteGroup*> const & route_groups () const {
		return _route_groups;
	}

	/* fundamental operations. duh. */

	std::list<boost::shared_ptr<AudioTrack> > new_audio_track (
		int input_channels,
		int output_channels,
		RouteGroup* route_group,
		uint32_t how_many,
		std::string name_template,
		PresentationInfo::order_t order,
		TrackMode mode = Normal
		);

	std::list<boost::shared_ptr<MidiTrack> > new_midi_track (
		const ChanCount& input, const ChanCount& output, bool strict_io,
		boost::shared_ptr<PluginInfo> instrument,
		Plugin::PresetRecord* pset,
		RouteGroup* route_group, uint32_t how_many, std::string name_template,
		PresentationInfo::order_t,
		TrackMode mode = Normal
		);

	RouteList new_audio_route (int input_channels, int output_channels, RouteGroup* route_group, uint32_t how_many, std::string name_template, PresentationInfo::Flag, PresentationInfo::order_t);
	RouteList new_midi_route (RouteGroup* route_group, uint32_t how_many, std::string name_template, bool strict_io, boost::shared_ptr<PluginInfo> instrument, Plugin::PresetRecord*, PresentationInfo::Flag, PresentationInfo::order_t);

	void remove_routes (boost::shared_ptr<RouteList>);
	void remove_route (boost::shared_ptr<Route>);

	void resort_routes ();
	void resort_routes_using (boost::shared_ptr<RouteList>);

	AudioEngine & engine() { return _engine; }
	AudioEngine const & engine () const { return _engine; }

	static std::string default_track_name_pattern (DataType);

	/* Time */

	samplepos_t transport_sample () const { return _transport_sample; }
	samplepos_t record_location () const { return _last_record_location; }
	samplepos_t audible_sample (bool* latent_locate = NULL) const;
	samplepos_t requested_return_sample() const { return _requested_return_sample; }
	void set_requested_return_sample(samplepos_t return_to);
	boost::optional<samplepos_t> const & nominal_jack_transport_sample() { return _nominal_jack_transport_sample; }

	bool compute_audible_delta (samplepos_t& pos_and_delta) const;
	samplecnt_t remaining_latency_preroll () const { return _remaining_latency_preroll; }

	enum PullupFormat {
		pullup_Plus4Plus1,
		pullup_Plus4,
		pullup_Plus4Minus1,
		pullup_Plus1,
		pullup_None,
		pullup_Minus1,
		pullup_Minus4Plus1,
		pullup_Minus4,
		pullup_Minus4Minus1
	};

	void sync_time_vars();

	void bbt_time (samplepos_t when, Timecode::BBT_Time&);
	void timecode_to_sample(Timecode::Time& timecode, samplepos_t& sample, bool use_offset, bool use_subframes) const;
	void sample_to_timecode(samplepos_t sample, Timecode::Time& timecode, bool use_offset, bool use_subframes) const;
	void timecode_time (Timecode::Time &);
	void timecode_time (samplepos_t when, Timecode::Time&);
	void timecode_time_subframes (samplepos_t when, Timecode::Time&);

	void timecode_duration (samplecnt_t, Timecode::Time&) const;
	void timecode_duration_string (char *, size_t len, samplecnt_t) const;

	samplecnt_t convert_to_samples (AnyTime const & position);
	samplecnt_t any_duration_to_samples (samplepos_t position, AnyTime const & duration);

	static PBD::Signal1<void, samplepos_t> StartTimeChanged;
	static PBD::Signal1<void, samplepos_t> EndTimeChanged;

	void   request_sync_source (boost::shared_ptr<TransportMaster>);
	bool   synced_to_engine() const;

	double engine_speed() const { return _engine_speed; }
	double actual_speed() const {
		if (_transport_speed > 0) return _engine_speed;
		if (_transport_speed < 0) return - _engine_speed;
		return 0;
	}
	double transport_speed() const { return _count_in_samples > 0 ? 0. : _transport_speed; }
	bool   transport_stopped() const;
	bool   transport_stopped_or_stopping() const;
	bool   transport_rolling() const;
	bool   transport_will_roll_forwards() const;
	
	bool silent () { return _silent; }

	bool punch_is_possible () const;
	bool loop_is_possible () const;
	PBD::Signal0<void> PunchLoopConstraintChange;

	TempoMap&       tempo_map()       { return *_tempo_map; }
	const TempoMap& tempo_map() const { return *_tempo_map; }
	void maybe_update_tempo_from_midiclock_tempo (float bpm);

	unsigned int    get_xrun_count () const {return _xrun_count; }
	void            reset_xrun_count () {_xrun_count = 0; }

	/* region info  */

	boost::shared_ptr<Region> find_whole_file_parent (boost::shared_ptr<Region const>) const;

	boost::shared_ptr<Region>      XMLRegionFactory (const XMLNode&, bool full);
	boost::shared_ptr<AudioRegion> XMLAudioRegionFactory (const XMLNode&, bool full);
	boost::shared_ptr<MidiRegion>  XMLMidiRegionFactory (const XMLNode&, bool full);

	/* source management */

	void import_files (ImportStatus&);
	bool sample_rate_convert (ImportStatus&, std::string infile, std::string& outfile);
	std::string build_tmp_convert_name (std::string file);

	boost::shared_ptr<ExportHandler> get_export_handler ();
	boost::shared_ptr<ExportStatus> get_export_status ();

	int start_audio_export (samplepos_t position, bool realtime = false, bool region_export = false);

	PBD::Signal1<int, samplecnt_t> ProcessExport;
	static PBD::Signal2<void,std::string, std::string> Exported;

	void add_source (boost::shared_ptr<Source>);
	void remove_source (boost::weak_ptr<Source>);

	void cleanup_regions();
	bool can_cleanup_peakfiles () const;
	int  cleanup_peakfiles ();
	int  cleanup_sources (CleanupReport&);
	int  cleanup_trash_sources (CleanupReport&);

	int destroy_sources (std::list<boost::shared_ptr<Source> > const&);

	int remove_last_capture ();
	void get_last_capture_sources (std::list<boost::shared_ptr<Source> >&);

	/** handlers should return -1 for "stop cleanup",
	    0 for "yes, delete this playlist",
	    1 for "no, don't delete this playlist".
	*/
	static PBD::Signal1<int,boost::shared_ptr<Playlist> >  AskAboutPlaylistDeletion;

	/** handlers should return 0 for "ignore the rate mismatch",
	    !0 for "do not use this session"
	*/
	static PBD::Signal2<int, samplecnt_t, samplecnt_t> AskAboutSampleRateMismatch;

	/** non interactive message */
	static PBD::Signal2<void, samplecnt_t, samplecnt_t> NotifyAboutSampleRateMismatch;

	/** handlers should return !0 for use pending state, 0 for ignore it.
	 */
	static PBD::Signal0<int> AskAboutPendingState;

	boost::shared_ptr<AudioFileSource> create_audio_source_for_session (
		size_t, std::string const &, uint32_t, bool destructive);

	boost::shared_ptr<MidiSource> create_midi_source_for_session (std::string const &);
	boost::shared_ptr<MidiSource> create_midi_source_by_stealing_name (boost::shared_ptr<Track>);

	boost::shared_ptr<Source> source_by_id (const PBD::ID&);
	boost::shared_ptr<AudioFileSource> audio_source_by_path_and_channel (const std::string&, uint16_t) const;
	boost::shared_ptr<MidiSource> midi_source_by_path (const std::string&, bool need_source_lock) const;
	uint32_t count_sources_by_origin (const std::string&);

	void add_playlist (boost::shared_ptr<Playlist>, bool unused = false);

	/* Curves and AutomationLists (TODO when they go away) */
	void add_automation_list(AutomationList*);

	/* auditioning */

	boost::shared_ptr<Auditioner> the_auditioner() { return auditioner; }
	void audition_playlist ();
	void audition_region (boost::shared_ptr<Region>);
	void cancel_audition ();
	bool is_auditioning () const;

	PBD::Signal1<void,bool> AuditionActive;

	/* session script */
	void register_lua_function (const std::string&, const std::string&, const LuaScriptParamList&);
	void unregister_lua_function (const std::string& name);
	std::vector<std::string> registered_lua_functions ();
	uint32_t registered_lua_function_count () const { return _n_lua_scripts; }
	void scripts_changed (); // called from lua, updates _n_lua_scripts

	PBD::Signal0<void> LuaScriptsChanged;

	/* flattening stuff */

	boost::shared_ptr<Region> write_one_track (Track&, samplepos_t start, samplepos_t end,
	                                           bool overwrite, std::vector<boost::shared_ptr<Source> >&, InterThreadInfo& wot,
	                                           boost::shared_ptr<Processor> endpoint,
	                                           bool include_endpoint, bool for_export, bool for_freeze);
	int freeze_all (InterThreadInfo&);

	/* session-wide solo/mute/rec-enable */

	bool muted() const;
	std::vector<boost::weak_ptr<AutomationControl> > cancel_all_mute ();

	bool soloing() const { return _non_soloed_outs_muted; }
	bool listening() const { return _listen_cnt > 0; }
	bool solo_isolated() const { return _solo_isolated_cnt > 0; }
	void cancel_all_solo ();

	bool solo_selection_active();
	void solo_selection( StripableList&, bool );

	static const SessionEvent::RTeventCallback rt_cleanup;

	void clear_all_solo_state (boost::shared_ptr<RouteList>);

	/* Control-based methods */

	void set_controls (boost::shared_ptr<ControlList>, double val, PBD::Controllable::GroupControlDisposition);
	void set_control (boost::shared_ptr<AutomationControl>, double val, PBD::Controllable::GroupControlDisposition);

	void set_exclusive_input_active (boost::shared_ptr<RouteList> rt, bool onoff, bool flip_others = false);

	PBD::Signal1<void,bool> SoloActive;
	PBD::Signal0<void> SoloChanged;
	PBD::Signal0<void> MuteChanged;
	PBD::Signal0<void> IsolatedChanged;
	PBD::Signal0<void> MonitorChanged;
	PBD::Signal0<void> MonitorBusAddedOrRemoved;

	PBD::Signal0<void> session_routes_reconnected;

	/* monitor/master out */
	int add_master_bus (ChanCount const&);

	void reset_monitor_section ();
	bool monitor_active() const { return (_monitor_out && _monitor_out->monitor_control () && _monitor_out->monitor_control ()->monitor_active()); }

	boost::shared_ptr<Route> monitor_out() const { return _monitor_out; }
	boost::shared_ptr<Route> master_out() const { return _master_out; }

	PresentationInfo::order_t master_order_key () const { return _master_out ? _master_out->presentation_info ().order () : -1; }
	bool ensure_stripable_sort_order ();

	void globally_add_internal_sends (boost::shared_ptr<Route> dest, Placement p, bool);
	void globally_set_send_gains_from_track (boost::shared_ptr<Route> dest);
	void globally_set_send_gains_to_zero (boost::shared_ptr<Route> dest);
	void globally_set_send_gains_to_unity (boost::shared_ptr<Route> dest);
	void add_internal_sends (boost::shared_ptr<Route> dest, Placement p, boost::shared_ptr<RouteList> senders);
	void add_internal_send (boost::shared_ptr<Route>, int, boost::shared_ptr<Route>);
	void add_internal_send (boost::shared_ptr<Route>, boost::shared_ptr<Processor>, boost::shared_ptr<Route>);

	static void set_disable_all_loaded_plugins (bool yn) {
		_disable_all_loaded_plugins = yn;
	}
	static bool get_disable_all_loaded_plugins() {
		return _disable_all_loaded_plugins;
	}
	static void set_bypass_all_loaded_plugins (bool yn) {
		_bypass_all_loaded_plugins = yn;
	}
	static bool get_bypass_all_loaded_plugins() {
		return _bypass_all_loaded_plugins;
	}

	uint32_t next_send_id();
	uint32_t next_aux_send_id();
	uint32_t next_return_id();
	uint32_t next_insert_id();
	void mark_send_id (uint32_t);
	void mark_aux_send_id (uint32_t);
	void mark_return_id (uint32_t);
	void mark_insert_id (uint32_t);
	void unmark_send_id (uint32_t);
	void unmark_aux_send_id (uint32_t);
	void unmark_return_id (uint32_t);
	void unmark_insert_id (uint32_t);

	/* s/w "RAID" management */

	boost::optional<samplecnt_t> available_capture_duration();

	/* I/O bundles */

	void add_bundle (boost::shared_ptr<Bundle>, bool emit_signal = true);
	void remove_bundle (boost::shared_ptr<Bundle>);
	boost::shared_ptr<Bundle> bundle_by_name (std::string) const;

	PBD::Signal0<void> BundleAddedOrRemoved;

	void midi_panic ();

	/* History (for editors, mixers, UIs etc.) */

	/** Undo some transactions.
	 * @param n Number of transactions to undo.
	 */
	void undo (uint32_t n);
	/** Redo some transactions.
	 * @param n Number of transactions to undo.
	 */
	void redo (uint32_t n);

	UndoHistory& history() { return _history; }

	uint32_t undo_depth() const { return _history.undo_depth(); }
	uint32_t redo_depth() const { return _history.redo_depth(); }
	std::string next_undo() const { return _history.next_undo(); }
	std::string next_redo() const { return _history.next_redo(); }

	/** begin collecting undo information
	 *
	 * This call must always be followed by either
	 * begin_reversible_command() or commit_reversible_command()
	 *
	 * @param cmd_name human readable name for the undo operation
	 */
	void begin_reversible_command (const std::string& cmd_name);
	void begin_reversible_command (GQuark);
	/** abort an open undo command
	 * This must only be called after begin_reversible_command ()
	 */
	void abort_reversible_command ();
	/** finalize an undo command and commit pending transactions
	 *
	 * This must only be called after begin_reversible_command ()
	 * @param cmd (additional) command to add
	 */
	void commit_reversible_command (Command* cmd = 0);

	void add_command (Command *const cmd);

	/** create an StatefulDiffCommand from the given object and add it to the stack.
	 *
	 * This function must only be called after  begin_reversible_command.
	 * Failing to do so may lead to a crash.
	 *
	 * @param sfd the object to diff
	 * @returns the allocated StatefulDiffCommand (already added via add_command)
	 */
	PBD::StatefulDiffCommand* add_stateful_diff_command (boost::shared_ptr<PBD::StatefulDestructible> sfd);

	/** @return The list of operations that are currently in progress */
	std::list<GQuark> const & current_operations () {
		return _current_trans_quarks;
	}

	bool operation_in_progress (GQuark) const;

	void add_commands (std::vector<Command*> const & cmds);

	std::map<PBD::ID,PBD::StatefulDestructible*> registry;

	// these commands are implemented in libs/ardour/session_command.cc
	Command* memento_command_factory(XMLNode* n);
	Command* stateful_diff_command_factory (XMLNode *);
	void register_with_memento_command_factory(PBD::ID, PBD::StatefulDestructible*);

	/* clicking */

	boost::shared_ptr<IO> click_io() { return _click_io; }
	boost::shared_ptr<Amp> click_gain() { return _click_gain; }

	/* disk, buffer loads */

	uint32_t playback_load ();
	uint32_t capture_load ();

	/* ranges */

	void request_play_range (std::list<AudioRange>*, bool leave_rolling = false);
	void request_cancel_play_range ();
	bool get_play_range () const { return _play_range; }

	void maybe_update_session_range (samplepos_t, samplepos_t);

	/* preroll */
	samplecnt_t preroll_samples (samplepos_t) const;

	void request_preroll_record_trim (samplepos_t start, samplecnt_t preroll);
	void request_count_in_record ();

	samplecnt_t preroll_record_trim_len () const { return _preroll_record_trim_len; }

	/* temporary hacks to allow selection to be pushed from GUI into backend.
	   Whenever we move the selection object into libardour, these will go away.
	 */
	void set_range_selection (samplepos_t start, samplepos_t end);
	void set_object_selection (samplepos_t start, samplepos_t end);
	void clear_range_selection ();
	void clear_object_selection ();

	/* buffers for gain and pan */

	gain_t* gain_automation_buffer () const;
	gain_t* trim_automation_buffer () const;
	gain_t* send_gain_automation_buffer () const;
	gain_t* scratch_automation_buffer () const;
	pan_t** pan_automation_buffer () const;

	void ensure_buffer_set (BufferSet& buffers, const ChanCount& howmany);

	/* VST support */

	static int  vst_current_loading_id;
	static const char* vst_can_do_strings[];
	static const int vst_can_do_string_count;

	static intptr_t vst_callback (
		AEffect* effect,
		int32_t opcode,
		int32_t index,
		intptr_t value,
		void* ptr,
		float opt
		);

	static PBD::Signal0<void> SendFeedback;

	/* Speakers */

	boost::shared_ptr<Speakers> get_speakers ();

	/* Controllables */

	boost::shared_ptr<Processor> processor_by_id (PBD::ID) const;

	boost::shared_ptr<PBD::Controllable> controllable_by_id (const PBD::ID&);
	boost::shared_ptr<AutomationControl> automation_control_by_id (const PBD::ID&);

	void add_controllable (boost::shared_ptr<PBD::Controllable>);

	boost::shared_ptr<PBD::Controllable> solo_cut_control() const;

	SessionConfiguration config;

	SessionConfiguration* cfg () { return &config; }

	bool exporting () const {
		return _exporting;
	}

	bool bounce_processing() const {
		return _bounce_processing_active;
	}

	/* this is a private enum, but setup_enum_writer() needs it,
	   and i can't find a way to give that function
	   friend access. sigh.
	*/

	enum PostTransportWork {
		PostTransportStop               = 0x1,
		PostTransportLocate             = 0x2,
		PostTransportRoll               = 0x4,
		PostTransportAbort              = 0x8,
		PostTransportOverWrite          = 0x10,
		PostTransportAudition           = 0x20,
		PostTransportReverse            = 0x40,
		PostTransportClearSubstate      = 0x80,
		PostTransportAdjustPlaybackBuffering  = 0x100,
		PostTransportAdjustCaptureBuffering   = 0x200
	};

	boost::shared_ptr<SessionPlaylists> playlists () const { return _playlists; }

	void send_mmc_locate (samplepos_t);
	void queue_full_time_code () { _send_timecode_update = true; }
	void queue_song_position_pointer () { /* currently does nothing */ }

	bool step_editing() const { return (_step_editors > 0); }

	void request_suspend_timecode_transmission ();
	void request_resume_timecode_transmission ();
	bool timecode_transmission_suspended () const;

	std::vector<std::string> source_search_path(DataType) const;
	void ensure_search_path_includes (const std::string& path, DataType type);
	void remove_dir_from_search_path (const std::string& path, DataType type);

	std::list<std::string> unknown_processors () const;

	std::list<std::string> missing_filesources (DataType) const;

	/** Emitted when a feedback cycle has been detected within Ardour's signal
	    processing path.  Until it is fixed (by the user) some (unspecified)
	    routes will not be run.
	*/
	static PBD::Signal0<void> FeedbackDetected;

	/** Emitted when a graph sort has successfully completed, which means
	    that it has no feedback cycles.
	*/
	static PBD::Signal0<void> SuccessfulGraphSort;

	/* handlers can return an integer value:
	   0: config.set_audio_search_path() or config.set_midi_search_path() was used
	   to modify the search path and we should try to find it again.
	   1: quit entire session load
	   2: as 0, but don't ask about other missing files
	   3: don't ask about other missing files, and just mark this one missing
	   -1: just mark this one missing
	   any other value: as -1
	*/
	static PBD::Signal3<int,Session*,std::string,DataType> MissingFile;

	void set_missing_file_replacement (const std::string& mfr) {
		_missing_file_replacement = mfr;
	}

	/** Emitted when the session wants Ardour to quit */
	static PBD::Signal0<void> Quit;

	/** Emitted when Ardour is asked to load a session in an older session
	 * format, and makes a backup copy.
	 */
	static PBD::Signal2<void,std::string,std::string> VersionMismatch;

	SceneChanger* scene_changer() const { return _scene_changer; }

	/* asynchronous MIDI control ports */

	boost::shared_ptr<Port> mmc_output_port () const;
	boost::shared_ptr<Port> mmc_input_port () const;
	boost::shared_ptr<Port> scene_input_port () const;
	boost::shared_ptr<Port> scene_output_port () const;

	boost::shared_ptr<AsyncMIDIPort> vkbd_output_port () const;

	/* synchronous MIDI ports used for synchronization */

	boost::shared_ptr<MidiPort> midi_clock_output_port () const;
	boost::shared_ptr<MidiPort> mtc_output_port () const;
	boost::shared_ptr<Port> ltc_output_port() const;

	boost::shared_ptr<IO> ltc_output_io() { return _ltc_output; }

	MIDI::MachineControl& mmc() { return *_mmc; }

	void reconnect_midi_scene_ports (bool);
	void reconnect_mmc_ports (bool);

	void reconnect_ltc_output ();

	VCAManager& vca_manager() { return *_vca_manager; }
	VCAManager* vca_manager_ptr() { return _vca_manager; }

	void auto_connect_thread_wakeup ();

	double compute_speed_from_master (pframes_t nframes);
	bool   transport_master_is_external() const;
	bool   transport_master_no_external_or_using_engine() const;
	boost::shared_ptr<TransportMaster> transport_master() const;

	void import_pt (PTFFormat& ptf, ImportStatus& status);
	bool import_sndfile_as_region (std::string path, SrcQuality quality, samplepos_t& pos, SourceList& sources, ImportStatus& status);

protected:
	friend class AudioEngine;
	void set_block_size (pframes_t nframes);
	void set_sample_rate (samplecnt_t nframes);

	friend class Route;
	void update_latency_compensation (bool force, bool called_from_backend);

	/* transport API */

	void locate (samplepos_t, bool with_roll, bool with_flush, bool for_loop_end=false, bool force=false, bool with_mmc=true);
	void stop_transport (bool abort = false, bool clear_state = false);
	void start_transport ();
	void butler_completed_transport_work ();
	void post_locate ();
	void schedule_butler_for_transport_work ();
	bool should_roll_after_locate () const;
	double speed() const { return _transport_speed; }
	samplepos_t position() const { return _transport_sample; }
	void set_transport_speed (double speed, bool abort, bool clear_state, bool as_default);

private:
	int  create (const std::string& mix_template, BusProfile const *);
	void destroy ();

	static guint _name_id_counter;
	static void init_name_id_counter (guint n);
	static unsigned int name_id_counter ();

	boost::shared_ptr<SessionPlaylists> _playlists;

	/* stuff used in process() should be close together to
	   maximise cache hits
	*/

	typedef void (Session::*process_function_type)(pframes_t);

	AudioEngine&            _engine;
	mutable gint             processing_prohibited;
	process_function_type    process_function;
	process_function_type    last_process_function;
	bool                    _bounce_processing_active;
	bool                     waiting_for_sync_offset;
	samplecnt_t             _base_sample_rate;     // sample-rate of the session at creation time, "native" SR
	samplecnt_t             _nominal_sample_rate;  // overridden by audioengine setting
	samplecnt_t             _current_sample_rate;  // this includes video pullup offset
	mutable gint            _record_status;
	samplepos_t             _transport_sample;
	gint                    _seek_counter;
	Location*               _session_range_location; ///< session range, or 0 if there is nothing in the session yet
	bool                    _session_range_is_free;
	bool                    _silent;
	samplecnt_t             _remaining_latency_preroll;

	// varispeed playback -- TODO: move out of session to backend.
	double                  _engine_speed;
	double                  _transport_speed; // only: -1, 0, +1
	double                  _default_transport_speed;
	double                  _last_transport_speed;
	double                  _signalled_varispeed;
	double                  _target_transport_speed;

	bool                     auto_play_legal;
	samplepos_t             _requested_return_sample;
	pframes_t                current_block_size;
	samplecnt_t             _worst_output_latency;
	samplecnt_t             _worst_input_latency;
	samplecnt_t             _worst_route_latency;
	uint32_t                _send_latency_changes;
	bool                    _have_captured;
	bool                    _non_soloed_outs_muted;
	bool                    _listening;
	uint32_t                _listen_cnt;
	uint32_t                _solo_isolated_cnt;
	bool                    _writable;
	bool                    _was_seamless;
	bool                    _under_nsm_control;
	unsigned int            _xrun_count;

	std::string             _missing_file_replacement;

	void add_monitor_section ();
	void remove_monitor_section ();

	void update_latency (bool playback);
	bool update_route_latency (bool reverse, bool apply_to_delayline);

	void initialize_latencies ();
	void set_worst_io_latencies ();
	void set_worst_output_latency ();
	void set_worst_input_latency ();

	void send_latency_compensation_change ();
	void set_worst_io_latencies_x (IOChange, void *);

	void ensure_buffers (ChanCount howmany = ChanCount::ZERO);

	void process_scrub          (pframes_t);
	void process_without_events (pframes_t);
	void process_with_events    (pframes_t);
	void process_audition       (pframes_t);
	void process_export         (pframes_t);
	void process_export_fw      (pframes_t);

	void block_processing() { g_atomic_int_set (&processing_prohibited, 1); }
	void unblock_processing() { g_atomic_int_set (&processing_prohibited, 0); }
	bool processing_blocked() const { return g_atomic_int_get (&processing_prohibited); }

	static const samplecnt_t bounce_chunk_size;

	/* Transport master DLL */

	enum TransportMasterState {
		Stopped, /* no incoming or invalid signal/data for master to run with */
		Waiting, /* waiting to get full lock on incoming signal/data */
		Running  /* lock achieved, master is generating meaningful speed & position */
	};

	samplepos_t master_wait_end;
	void track_transport_master (float slave_speed, samplepos_t slave_transport_sample);
	bool follow_transport_master (pframes_t nframes);

	void sync_source_changed (SyncSource, samplepos_t pos, pframes_t cycle_nframes);
	void reset_slave_state ();

	bool post_export_sync;
	samplepos_t post_export_position;

	bool _exporting;
	bool _export_rolling;
	bool _realtime_export;
	bool _region_export;
	samplepos_t _export_preroll;

	boost::shared_ptr<ExportHandler> export_handler;
	boost::shared_ptr<ExportStatus>  export_status;

	int  pre_export ();
	int  stop_audio_export ();
	void finalize_audio_export (TransportRequestSource trs);
	void finalize_export_internal (bool stop_freewheel);
	bool _pre_export_mmc_enabled;

	PBD::ScopedConnection export_freewheel_connection;

	void get_track_statistics ();
	int  process_routes (pframes_t, bool& need_butler);
	int  silent_process_routes (pframes_t, bool& need_butler);

	/** @return 1 if there is a pending declick fade-in,
	    -1 if there is a pending declick fade-out,
	    0 if there is no pending declick.
	*/
	bool maybe_stop (samplepos_t limit);
	bool maybe_sync_start (pframes_t &);

	std::string             _path;
	std::string             _name;
	bool                    _is_new;
	bool                    _send_qf_mtc;
	/** Number of process samples since the last MTC output (when sending MTC); used to
	 *  know when to send full MTC messages every so often.
	 */
	pframes_t               _pframes_since_last_mtc;
	bool                     play_loop;
	bool                     loop_changing;
	samplepos_t              last_loopend;

	boost::scoped_ptr<SessionDirectory> _session_dir;

	void hookup_io ();
	void graph_reordered (bool called_from_backend);

	/** current snapshot name, without the .ardour suffix */
	void set_snapshot_name (const std::string &);
	void save_snapshot_name (const std::string &);
	std::string _current_snapshot_name;

	XMLTree*         state_tree;
	bool             state_was_pending;
	StateOfTheState _state_of_the_state;

	friend class    StateProtector;
	gint            _suspend_save; /* atomic */
	volatile bool   _save_queued;
	volatile bool   _save_queued_pending;

	Glib::Threads::Mutex save_state_lock;
	Glib::Threads::Mutex save_source_lock;
	Glib::Threads::Mutex peak_cleanup_lock;

	int        load_options (const XMLNode&);
	int        load_state (std::string snapshot_name);
	static int parse_stateful_loading_version (const std::string&);

	samplepos_t _last_roll_location;
	/** the session sample time at which we last rolled, located, or changed transport direction */
	samplepos_t _last_roll_or_reversal_location;
	samplepos_t _last_record_location;

	bool              pending_abort;
	bool              pending_auto_loop;

	PBD::ReallocPool _mempool;
	LuaState lua;
	Glib::Threads::Mutex lua_lock;
	luabridge::LuaRef * _lua_run;
	luabridge::LuaRef * _lua_add;
	luabridge::LuaRef * _lua_del;
	luabridge::LuaRef * _lua_list;
	luabridge::LuaRef * _lua_load;
	luabridge::LuaRef * _lua_save;
	luabridge::LuaRef * _lua_cleanup;
	uint32_t            _n_lua_scripts;

	void setup_lua ();
	void try_run_lua (pframes_t);

	Butler* _butler;

	TransportFSM* _transport_fsm;

	static const PostTransportWork ProcessCannotProceedMask = PostTransportWork (PostTransportAudition| PostTransportClearSubstate);

	gint _post_transport_work; /* accessed only atomic ops */
	PostTransportWork post_transport_work() const        { return (PostTransportWork) g_atomic_int_get (const_cast<gint*>(&_post_transport_work)); }
	void set_post_transport_work (PostTransportWork ptw) { g_atomic_int_set (&_post_transport_work, (gint) ptw); }
	void add_post_transport_work (PostTransportWork ptw);

	void schedule_playback_buffering_adjustment ();
	void schedule_capture_buffering_adjustment ();

	Locations*       _locations;
	void location_added (Location*);
	void location_removed (Location*);
	void locations_changed ();
	void _locations_changed (const Locations::LocationList&);

	void update_skips (Location*, bool consolidate);
	void update_marks (Location* loc);
	void consolidate_skips (Location*);
	void sync_locations_to_skips ();
	void _sync_locations_to_skips ();

	PBD::ScopedConnectionList skip_update_connections;
	bool _ignore_skips_updates;

	PBD::ScopedConnectionList punch_connections;
	void             auto_punch_start_changed (Location *);
	void             auto_punch_end_changed (Location *);
	void             auto_punch_changed (Location *);

	PBD::ScopedConnectionList loop_connections;
	void             auto_loop_changed (Location *);

	void pre_engine_init (std::string path);
	int  post_engine_init ();
	int  immediately_post_engine ();
	void remove_empty_sounds ();

	void session_loaded ();

	void setup_midi_control ();
	int  midi_read (MIDI::Port *);

	void enable_record ();

	void increment_transport_position (samplecnt_t val) {
		if (max_samplepos - val < _transport_sample) {
			_transport_sample = max_samplepos;
		} else {
			_transport_sample += val;
		}
	}

	void decrement_transport_position (samplecnt_t val) {
		if (val < _transport_sample) {
			_transport_sample -= val;
		} else {
			_transport_sample = 0;
		}
	}

	void post_transport_motion ();
	static void *session_loader_thread (void *arg);

	void *do_work();

	/* Signal Forwarding */
	void emit_route_signals ();
	void emit_thread_run ();
	static void *emit_thread (void *);
	void emit_thread_start ();
	void emit_thread_terminate ();

	pthread_t       _rt_emit_thread;
	bool            _rt_thread_active;

	pthread_mutex_t _rt_emit_mutex;
	pthread_cond_t  _rt_emit_cond;
	bool            _rt_emit_pending;

	/* Auto Connect Thread */
	static void *auto_connect_thread (void *);
	void auto_connect_thread_run ();
	void auto_connect_thread_start ();
	void auto_connect_thread_terminate ();

	pthread_t       _auto_connect_thread;
	gint            _ac_thread_active;
	pthread_mutex_t _auto_connect_mutex;
	pthread_cond_t  _auto_connect_cond;

	struct AutoConnectRequest {
		public:
		AutoConnectRequest (boost::shared_ptr <Route> r, bool ci,
				const ChanCount& is,
				const ChanCount& os,
				const ChanCount& io,
				const ChanCount& oo)
			: route (boost::weak_ptr<Route> (r))
			, connect_inputs (ci)
			, input_start (is)
			, output_start (os)
			, input_offset (io)
			, output_offset (oo)
		{}

		boost::weak_ptr <Route> route;
		bool connect_inputs;
		ChanCount input_start;
		ChanCount output_start;
		ChanCount input_offset;
		ChanCount output_offset;
	};

	Glib::Threads::Mutex  _update_latency_lock;

	typedef std::queue<AutoConnectRequest> AutoConnectQueue;
	Glib::Threads::Mutex  _auto_connect_queue_lock;
	AutoConnectQueue _auto_connect_queue;
	guint _latency_recompute_pending;

	void get_physical_ports (std::vector<std::string>& inputs, std::vector<std::string>& outputs, DataType type,
	                         MidiPortFlags include = MidiPortFlags (0),
	                         MidiPortFlags exclude = MidiPortFlags (0));

	void auto_connect (const AutoConnectRequest&);
	void queue_latency_recompute ();

	/* SessionEventManager interface */

	void process_event (SessionEvent*);
	void set_next_event ();
	void cleanup_event (SessionEvent*,int);

	/* MIDI Machine Control */

	void spp_start ();
	void spp_continue ();
	void spp_stop ();

	void mmc_deferred_play (MIDI::MachineControl &);
	void mmc_stop (MIDI::MachineControl &);
	void mmc_step (MIDI::MachineControl &, int);
	void mmc_pause (MIDI::MachineControl &);
	void mmc_record_pause (MIDI::MachineControl &);
	void mmc_record_strobe (MIDI::MachineControl &);
	void mmc_record_exit (MIDI::MachineControl &);
	void mmc_track_record_status (MIDI::MachineControl &, uint32_t track, bool enabled);
	void mmc_fast_forward (MIDI::MachineControl &);
	void mmc_rewind (MIDI::MachineControl &);
	void mmc_locate (MIDI::MachineControl &, const MIDI::byte *);
	void mmc_shuttle (MIDI::MachineControl &mmc, float speed, bool forw);
	void mmc_record_enable (MIDI::MachineControl &mmc, size_t track, bool enabled);

	struct timeval last_mmc_step;
	double step_speed;

	typedef boost::function<bool()> MidiTimeoutCallback;
	typedef std::list<MidiTimeoutCallback> MidiTimeoutList;

	MidiTimeoutList midi_timeouts;
	bool mmc_step_timeout ();
	void send_immediate_mmc (MIDI::MachineControlCommand);

	MIDI::byte mtc_msg[16];
	MIDI::byte mtc_timecode_bits;   /* encoding of SMTPE type for MTC */
	MIDI::byte midi_msg[16];
	double outbound_mtc_timecode_frame;
	Timecode::Time transmitting_timecode_time;
	int next_quarter_frame_to_send;

	double _samples_per_timecode_frame; /* has to be floating point because of drop sample */
	samplecnt_t _frames_per_hour;
	samplecnt_t _timecode_frames_per_hour;

	/* cache the most-recently requested time conversions. This helps when we
	 * have multiple clocks showing the same time (e.g. the transport sample) */
	bool last_timecode_valid;
	samplepos_t last_timecode_when;
	Timecode::Time last_timecode;

	bool _send_timecode_update; ///< Flag to send a full sample (Timecode) MTC message this cycle

	int send_midi_time_code_for_cycle (samplepos_t, samplepos_t, pframes_t nframes);

	LTCEncoder*       ltc_encoder;
	ltcsnd_sample_t*  ltc_enc_buf;

	Timecode::TimecodeFormat ltc_enc_tcformat;
	int32_t           ltc_buf_off;
	int32_t           ltc_buf_len;

	double            ltc_speed;
	int32_t           ltc_enc_byte;
	samplepos_t       ltc_enc_pos;
	double            ltc_enc_cnt;
	samplepos_t       ltc_enc_off;
	bool              restarting;
	samplepos_t       ltc_prev_cycle;

	samplepos_t       ltc_timecode_offset;
	bool              ltc_timecode_negative_offset;

	LatencyRange      ltc_out_latency;

	void ltc_tx_initialize();
	void ltc_tx_cleanup();
	void ltc_tx_reset();
	void ltc_tx_resync_latency();
	void ltc_tx_recalculate_position();
	void ltc_tx_parse_offset();
	void ltc_tx_send_time_code_for_cycle (samplepos_t, samplepos_t, double, double, pframes_t nframes);
	PBD::ScopedConnectionList ltc_tx_connections;


	void reset_record_status ();

	int no_roll (pframes_t nframes);
	int fail_roll (pframes_t nframes);

	bool non_realtime_work_pending() const { return static_cast<bool>(post_transport_work()); }
	bool process_can_proceed() const { return !(post_transport_work() & ProcessCannotProceedMask); }

	MidiControlUI* midi_control_ui;

	int           start_midi_thread ();

	bool should_ignore_transport_request (TransportRequestSource, TransportRequestType) const;

	void set_play_loop (bool yn, bool change_transport_state);
	void unset_play_loop (bool change_transport_state = false);
	void overwrite_some_buffers (boost::shared_ptr<Route>, OverwriteReason);
	void flush_all_inserts ();
	int  micro_locate (samplecnt_t distance);

	enum PunchLoopLock {
		NoConstraint,
		OnlyPunch,
		OnlyLoop,
	};

	volatile guint _punch_or_loop; // enum PunchLoopLock

	bool punch_active () const;
	void unset_punch ();
	void reset_punch_loop_constraint ();
	bool maybe_allow_only_loop (bool play_loop = false);
	bool maybe_allow_only_punch ();

	void force_locate (samplepos_t sample, LocateTransportDisposition);
	void realtime_stop (bool abort, bool clear_state);
	void realtime_locate (bool);
	void non_realtime_start_scrub ();
	void non_realtime_set_speed ();
	void non_realtime_locate ();
	void non_realtime_stop (bool abort, int entry_request_count, bool& finished);
	void non_realtime_overwrite (int entry_request_count, bool& finished);
	void engine_halted ();
	void engine_running ();
	void xrun_recovery ();
	void set_track_loop (bool);
	bool select_playhead_priority_target (samplepos_t&);
	void follow_playhead_priority ();

	/* These are synchronous and so can only be called from within the process
	 * cycle
	 */

	int  send_full_time_code (samplepos_t, pframes_t nframes);
	void send_song_position_pointer (samplepos_t);

	TempoMap    *_tempo_map;
	void          tempo_map_changed (const PBD::PropertyChange&);

	/* edit/mix groups */

	int load_route_groups (const XMLNode&, int);

	std::list<RouteGroup *> _route_groups;
	RouteGroup*             _all_route_group;

	/* routes stuff */

	boost::shared_ptr<Graph> _process_graph;

	SerializedRCUManager<RouteList>  routes;

	void add_routes (RouteList&, bool input_auto_connect, bool output_auto_connect, bool save, PresentationInfo::order_t);
	void add_routes_inner (RouteList&, bool input_auto_connect, bool output_auto_connect, PresentationInfo::order_t);
	bool _adding_routes_in_progress;
	bool _reconnecting_routes_in_progress;
	bool _route_deletion_in_progress;

	uint32_t destructive_index;

	boost::shared_ptr<Route> XMLRouteFactory (const XMLNode&, int);
	boost::shared_ptr<Route> XMLRouteFactory_2X (const XMLNode&, int);
	boost::shared_ptr<Route> XMLRouteFactory_3X (const XMLNode&, int);

	void route_processors_changed (RouteProcessorChange);

	bool find_route_name (std::string const &, uint32_t& id, std::string& name, bool);
	void count_existing_track_channels (ChanCount& in, ChanCount& out);
	void auto_connect_route (boost::shared_ptr<Route>, bool, const ChanCount&, const ChanCount&, const ChanCount& io = ChanCount(), const ChanCount& oo = ChanCount());
	void midi_output_change_handler (IOChange change, void* /*src*/, boost::weak_ptr<Route> midi_track);

	/* track numbering */

	void reassign_track_numbers ();
	uint32_t _track_number_decimals;

	/* solo/mute/notifications */

	void route_listen_changed (PBD::Controllable::GroupControlDisposition, boost::weak_ptr<Route>);
	void route_mute_changed ();
	void route_solo_changed (bool self_solo_change, PBD::Controllable::GroupControlDisposition group_override, boost::weak_ptr<Route>);
	void route_solo_isolated_changed (boost::weak_ptr<Route>);

	void update_route_solo_state (boost::shared_ptr<RouteList> r = boost::shared_ptr<RouteList>());

	void listen_position_changed ();
	void solo_control_mode_changed ();

	/* REGION MANAGEMENT */

	mutable Glib::Threads::Mutex region_lock;

	int load_regions (const XMLNode& node);
	int load_compounds (const XMLNode& node);

	void route_added_to_route_group (RouteGroup *, boost::weak_ptr<Route>);
	void route_removed_from_route_group (RouteGroup *, boost::weak_ptr<Route>);
	void route_group_property_changed (RouteGroup *);

	/* SOURCES */

	mutable Glib::Threads::Mutex source_lock;

public:

	/* Emited when a new source is added to the session */
	PBD::Signal1< void, boost::weak_ptr<Source> > SourceAdded;
	PBD::Signal1< void, boost::weak_ptr<Source> > SourceRemoved;

	typedef std::map<PBD::ID,boost::shared_ptr<Source> > SourceMap;

	void foreach_source (boost::function<void( boost::shared_ptr<Source> )> f) {
		Glib::Threads::Mutex::Lock ls (source_lock);
		for (SourceMap::iterator i = sources.begin(); i != sources.end(); ++i) {
			f ( (*i).second );
		}
	}

	bool playlist_is_active( boost::shared_ptr<Playlist>);

private:
	void reset_write_sources (bool mark_write_complete, bool force = false);
	SourceMap sources;

	int load_sources (const XMLNode& node);
	XMLNode& get_sources_as_xml ();

	boost::shared_ptr<Source> XMLSourceFactory (const XMLNode&);

	/* PLAYLISTS */

	void remove_playlist (boost::weak_ptr<Playlist>);
	void track_playlist_changed (boost::weak_ptr<Track>);
	void playlist_region_added (boost::weak_ptr<Region>);
	void playlist_ranges_moved (std::list<Evoral::RangeMove<samplepos_t> > const &);
	void playlist_regions_extended (std::list<Evoral::Range<samplepos_t> > const &);

	/* CURVES and AUTOMATION LISTS */
	std::map<PBD::ID, AutomationList*> automation_lists;

	/** load 2.X Sessions. Diskstream-ID to playlist-name mapping */
	std::map<PBD::ID, std::string> _diskstreams_2X;

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

	int flatten_one_track (AudioTrack&, samplepos_t start, samplecnt_t cnt);

	/* INSERT AND SEND MANAGEMENT */

	boost::dynamic_bitset<uint32_t> send_bitset;
	boost::dynamic_bitset<uint32_t> aux_send_bitset;
	boost::dynamic_bitset<uint32_t> return_bitset;
	boost::dynamic_bitset<uint32_t> insert_bitset;

	/* S/W RAID */

	struct space_and_path {
		uint32_t blocks;     ///< 4kB blocks
		bool blocks_unknown; ///< true if blocks is unknown
		std::string path;

		space_and_path ()
			: blocks (0)
			, blocks_unknown (true)
		{}
	};

	struct space_and_path_ascending_cmp {
		bool operator() (space_and_path a, space_and_path b) {
			if (a.blocks_unknown != b.blocks_unknown) {
				return !a.blocks_unknown;
			}
			return a.blocks > b.blocks;
		}
	};

	void setup_raid_path (std::string path);

	std::vector<space_and_path> session_dirs;
	std::vector<space_and_path>::iterator last_rr_session_dir;
	uint32_t _total_free_4k_blocks;
	/** If this is true, _total_free_4k_blocks is not definite,
	    as one or more of the session directories' filesystems
	    could not report free space.
	*/
	bool _total_free_4k_blocks_uncertain;
	Glib::Threads::Mutex space_lock;

	bool no_questions_about_missing_files;

	std::string get_best_session_directory_for_new_audio ();

	mutable gint _playback_load;
	mutable gint _capture_load;

	/* I/O bundles */

	SerializedRCUManager<BundleList> _bundles;
	XMLNode* _bundle_xml_node;
	int load_bundles (XMLNode const &);

	UndoHistory      _history;
	/** current undo transaction, or 0 */
	UndoTransaction* _current_trans;
	/** GQuarks to describe the reversible commands that are currently in progress.
	 *  These may be nested, in which case more recently-started commands are toward
	 *  the front of the list.
	 */
	std::list<GQuark> _current_trans_quarks;

	int  backend_sync_callback (TransportState, samplepos_t);

	void process_rtop (SessionEvent*);

	enum snapshot_t {
		NormalSave,
		SnapshotKeep,
		SwitchToSnapshot
	};

	XMLNode& state (bool save_template,
	                snapshot_t snapshot_type = NormalSave,
	                bool only_used_assets = false);

	XMLNode& get_state ();
	int      set_state (const XMLNode& node, int version); // not idempotent
	XMLNode& get_template ();

	/* click track */
	typedef std::list<Click*>     Clicks;
	Clicks                        clicks;
	bool                         _clicking;
	bool                         _click_rec_only;
	boost::shared_ptr<IO>        _click_io;
	boost::shared_ptr<Amp>       _click_gain;
	Sample*                       click_data;
	Sample*                       click_emphasis_data;
	samplecnt_t                   click_length;
	samplecnt_t                   click_emphasis_length;
	mutable Glib::Threads::RWLock click_lock;

	static const Sample     default_click[];
	static const samplecnt_t default_click_length;
	static const Sample     default_click_emphasis[];
	static const samplecnt_t default_click_emphasis_length;

	Click* get_click();
	samplepos_t _clicks_cleared;
	samplecnt_t _count_in_samples;
	void setup_click_sounds (int which);
	void setup_click_sounds (Sample**, Sample const *, samplecnt_t*, samplecnt_t, std::string const &);
	void clear_clicks ();
	void click (samplepos_t start, samplecnt_t nframes);
	void run_click (samplepos_t start, samplecnt_t nframes);
	void add_click (samplepos_t pos, bool emphasis);

	/* range playback */

	std::list<AudioRange> current_audio_range;
	bool _play_range;
	void set_play_range (std::list<AudioRange>&, bool leave_rolling);
	void unset_play_range ();

	/* temporary hacks to allow selection to be pushed from GUI into backend
	   Whenever we move the selection object into libardour, these will go away.
	*/
	Evoral::Range<samplepos_t> _range_selection;
	Evoral::Range<samplepos_t> _object_selection;

	void unset_preroll_record_trim ();

	samplecnt_t _preroll_record_trim_len;
	bool _count_in_once;

	/* main outs */
	uint32_t main_outs;

	boost::shared_ptr<Route> _master_out;
	boost::shared_ptr<Route> _monitor_out;

	void auto_connect_master_bus ();

	int find_all_sources (std::string path, std::set<std::string>& result);
	int find_all_sources_across_snapshots (std::set<std::string>& result, bool exclude_this_snapshot);

	typedef std::set<boost::shared_ptr<PBD::Controllable> > Controllables;
	Glib::Threads::Mutex controllables_lock;
	Controllables controllables;

	boost::shared_ptr<PBD::Controllable> _solo_cut_control;

	void reset_native_file_format();
	bool first_file_data_format_reset;
	bool first_file_header_format_reset;

	void config_changed (std::string, bool);

	XMLNode& get_control_protocol_state ();

	void set_history_depth (uint32_t depth);

	static bool _disable_all_loaded_plugins;
	static bool _bypass_all_loaded_plugins;

	mutable bool have_looped; ///< Used in \ref audible_sample

	void update_route_record_state ();
	gint _have_rec_enabled_track;
	gint _have_rec_disabled_track;

	static int ask_about_playlist_deletion (boost::shared_ptr<Playlist>);

	/* realtime "apply to set of routes" operations */
	template<typename T> SessionEvent*
		get_rt_event (boost::shared_ptr<RouteList> rl, T targ, SessionEvent::RTeventCallback after, PBD::Controllable::GroupControlDisposition group_override,
		              void (Session::*method) (boost::shared_ptr<RouteList>, T, PBD::Controllable::GroupControlDisposition)) {
		SessionEvent* ev = new SessionEvent (SessionEvent::RealTimeOperation, SessionEvent::Add, SessionEvent::Immediate, 0, 0.0);
		ev->rt_slot = boost::bind (method, this, rl, targ, group_override);
		ev->rt_return = after;
		ev->event_loop = PBD::EventLoop::get_event_loop_for_thread ();

		return ev;
	}

	/* specialized version realtime "apply to set of routes" operations */
	template<typename T1, typename T2> SessionEvent*
		get_rt_event (boost::shared_ptr<RouteList> rl, T1 t1arg, T2 t2arg, SessionEvent::RTeventCallback after, PBD::Controllable::GroupControlDisposition group_override,
		              void (Session::*method) (boost::shared_ptr<RouteList>, T1, T2, PBD::Controllable::GroupControlDisposition)) {
		SessionEvent* ev = new SessionEvent (SessionEvent::RealTimeOperation, SessionEvent::Add, SessionEvent::Immediate, 0, 0.0);
		ev->rt_slot = boost::bind (method, this, rl, t1arg, t2arg, group_override);
		ev->rt_return = after;
		ev->event_loop = PBD::EventLoop::get_event_loop_for_thread ();

		return ev;
	}

	/* specialized version realtime "apply to set of controls" operations */
	SessionEvent* get_rt_event (boost::shared_ptr<ControlList> cl, double arg, PBD::Controllable::GroupControlDisposition group_override) {
		SessionEvent* ev = new SessionEvent (SessionEvent::RealTimeOperation, SessionEvent::Add, SessionEvent::Immediate, 0, 0.0);
		ev->rt_slot = boost::bind (&Session::rt_set_controls, this, cl, arg, group_override);
		ev->rt_return = Session::rt_cleanup;
		ev->event_loop = PBD::EventLoop::get_event_loop_for_thread ();

		return ev;
	}

	void rt_set_controls (boost::shared_ptr<ControlList>, double val, PBD::Controllable::GroupControlDisposition group_override);
	void rt_clear_all_solo_state (boost::shared_ptr<RouteList>, bool yn, PBD::Controllable::GroupControlDisposition group_override);

	void setup_midi_machine_control ();

	void step_edit_status_change (bool);
	uint32_t _step_editors;

	/** true if timecode transmission by the transport is suspended, otherwise false */
	mutable gint _suspend_timecode_transmission;

	void update_locations_after_tempo_map_change (const Locations::LocationList &);

	void start_time_changed (samplepos_t);
	void end_time_changed (samplepos_t);

	void set_track_monitor_input_status (bool);
	samplepos_t compute_stop_limit () const;

	boost::shared_ptr<Speakers> _speakers;
	void load_nested_sources (const XMLNode& node);

	/** The directed graph of routes that is currently being used for audio processing
	    and solo/mute computations.
	*/
	GraphEdges _current_route_graph;

	void ensure_route_presentation_info_gap (PresentationInfo::order_t, uint32_t gap_size);

	friend class    ProcessorChangeBlocker;
	gint            _ignore_route_processor_changes; /* atomic */

	MidiClockTicker* midi_clock;

	boost::shared_ptr<IO>   _ltc_output;

	boost::shared_ptr<RTTaskList> _rt_tasklist;

	/* Scene Changing */
	SceneChanger* _scene_changer;

	/* persistent, non-track related MIDI ports */
	MidiPortManager* _midi_ports;
	MIDI::MachineControl* _mmc;

	void setup_ltc ();
	void setup_click ();
	void setup_click_state (const XMLNode*);
	void setup_bundles ();

	void save_as_bring_callback (uint32_t, uint32_t, std::string);

	static const uint32_t session_end_shift;

	std::string _template_state_dir;

	VCAManager* _vca_manager;

	boost::shared_ptr<Route> get_midi_nth_route_by_id (PresentationInfo::order_t n) const;

	std::string created_with;

	void midi_track_presentation_info_changed (PBD::PropertyChange const &, boost::weak_ptr<MidiTrack>);
	void rewire_selected_midi (boost::shared_ptr<MidiTrack>);
	void rewire_midi_selection_ports ();
	boost::weak_ptr<MidiTrack> current_midi_target;

	StripableList _soloSelection;  //the items that are soloe'd during a solo-selection operation; need to unsolo after the roll

	CoreSelection* _selection;

	bool _global_locate_pending;
	boost::optional<samplepos_t> _nominal_jack_transport_sample;
};


} // namespace ARDOUR

#endif /* __ardour_session_h__ */
