/*
 * Copyright (C) 1999-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2006-2007 Jesse Chappell <jesse@essej.net>
 * Copyright (C) 2006-2009 Sampo Savolainen <v2@iki.fi>
 * Copyright (C) 2006-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2006-2016 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2008-2009 Hans Baier <hansfbaier@googlemail.com>
 * Copyright (C) 2012-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2013-2017 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2014-2019 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2015 GZharun <grygoriiz@wavesglobal.com>
 * Copyright (C) 2016-2018 Len Ovens <len@ovenwerks.net>
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

#include <stdint.h>

#include <algorithm>
#include <string>
#include <vector>
#include <sstream>
#include <cstdio> /* sprintf(3) ... grrr */
#include <cmath>
#include <cerrno>
#include <unistd.h>
#include <limits.h>

#include <glibmm/threads.h>
#include <glibmm/miscutils.h>
#include <glibmm/fileutils.h>

#include <boost/algorithm/string/erase.hpp>

#include "pbd/basename.h"
#include "pbd/convert.h"
#include "pbd/error.h"
#include "pbd/file_utils.h"
#include "pbd/i18n.h"
#include "pbd/md5.h"
#include "pbd/pthread_utils.h"
#include "pbd/search_path.h"
#include "pbd/stacktrace.h"
#include "pbd/stl_delete.h"
#include "pbd/replace_all.h"
#include "pbd/types_convert.h"
#include "pbd/unwind.h"

#include "ardour/amp.h"
#include "ardour/analyser.h"
#include "ardour/async_midi_port.h"
#include "ardour/audio_buffer.h"
#include "ardour/audio_port.h"
#include "ardour/audio_track.h"
#include "ardour/audioengine.h"
#include "ardour/audiofilesource.h"
#include "ardour/auditioner.h"
#include "ardour/boost_debug.h"
#include "ardour/buffer_manager.h"
#include "ardour/buffer_set.h"
#include "ardour/bundle.h"
#include "ardour/butler.h"
#include "ardour/click.h"
#include "ardour/control_protocol_manager.h"
#include "ardour/data_type.h"
#include "ardour/debug.h"
#include "ardour/disk_reader.h"
#include "ardour/directory_names.h"
#include "ardour/filename_extensions.h"
#include "ardour/gain_control.h"
#include "ardour/graph.h"
#include "ardour/luabindings.h"
#include "ardour/midiport_manager.h"
#include "ardour/scene_changer.h"
#include "ardour/midi_patch_manager.h"
#include "ardour/midi_track.h"
#include "ardour/midi_ui.h"
#include "ardour/operations.h"
#include "ardour/playlist.h"
#include "ardour/playlist_factory.h"
#include "ardour/plugin.h"
#include "ardour/plugin_insert.h"
#include "ardour/process_thread.h"
#include "ardour/profile.h"
#include "ardour/rc_configuration.h"
#include "ardour/recent_sessions.h"
#include "ardour/region.h"
#include "ardour/region_factory.h"
#include "ardour/revision.h"
#include "ardour/route_graph.h"
#include "ardour/route_group.h"
#include "ardour/rt_tasklist.h"
#include "ardour/silentfilesource.h"
#include "ardour/send.h"
#include "ardour/selection.h"
#include "ardour/session.h"
#include "ardour/session_directory.h"
#include "ardour/session_playlists.h"
#include "ardour/smf_source.h"
#include "ardour/solo_isolate_control.h"
#include "ardour/source_factory.h"
#include "ardour/speakers.h"
#include "ardour/tempo.h"
#include "ardour/ticker.h"
#include "ardour/transport_fsm.h"
#include "ardour/transport_master.h"
#include "ardour/transport_master_manager.h"
#include "ardour/track.h"
#include "ardour/types_convert.h"
#include "ardour/user_bundle.h"
#include "ardour/utils.h"
#include "ardour/vca_manager.h"
#include "ardour/vca.h"

#include "midi++/port.h"
#include "midi++/mmc.h"

#include "LuaBridge/LuaBridge.h"

#include <glibmm/checksum.h>

namespace ARDOUR {
class MidiSource;
class Processor;
class Speakers;
}

using namespace std;
using namespace ARDOUR;
using namespace PBD;

bool Session::_disable_all_loaded_plugins = false;
bool Session::_bypass_all_loaded_plugins = false;
guint Session::_name_id_counter = 0;

PBD::Signal1<void,std::string> Session::Dialog;
PBD::Signal0<int> Session::AskAboutPendingState;
PBD::Signal2<int, samplecnt_t, samplecnt_t> Session::AskAboutSampleRateMismatch;
PBD::Signal2<void, samplecnt_t, samplecnt_t> Session::NotifyAboutSampleRateMismatch;
PBD::Signal0<void> Session::SendFeedback;
PBD::Signal3<int,Session*,std::string,DataType> Session::MissingFile;

PBD::Signal1<void, samplepos_t> Session::StartTimeChanged;
PBD::Signal1<void, samplepos_t> Session::EndTimeChanged;
PBD::Signal2<void,std::string, std::string> Session::Exported;
PBD::Signal1<int,boost::shared_ptr<Playlist> > Session::AskAboutPlaylistDeletion;
PBD::Signal0<void> Session::Quit;
PBD::Signal0<void> Session::FeedbackDetected;
PBD::Signal0<void> Session::SuccessfulGraphSort;
PBD::Signal2<void,std::string,std::string> Session::VersionMismatch;

const samplecnt_t Session::bounce_chunk_size = 8192;
static void clean_up_session_event (SessionEvent* ev) { delete ev; }
const SessionEvent::RTeventCallback Session::rt_cleanup (clean_up_session_event);
const uint32_t Session::session_end_shift = 0;

/** @param snapshot_name Snapshot name, without .ardour suffix */
Session::Session (AudioEngine &eng,
                  const string& fullpath,
                  const string& snapshot_name,
                  BusProfile const * bus_profile,
                  string mix_template)
	: _playlists (new SessionPlaylists)
	, _engine (eng)
	, process_function (&Session::process_with_events)
	, _bounce_processing_active (false)
	, waiting_for_sync_offset (false)
	, _base_sample_rate (0)
	, _nominal_sample_rate (0)
	, _current_sample_rate (0)
	, _record_status (Disabled)
	, _transport_sample (0)
	, _seek_counter (0)
	, _session_range_location (0)
	, _session_range_is_free (true)
	, _silent (false)
	, _remaining_latency_preroll (0)
	, _engine_speed (1.0)
	, _transport_speed (0)
	, _default_transport_speed (1.0)
	, _signalled_varispeed (0)
	, _target_transport_speed (0.0)
	, auto_play_legal (false)
	, _requested_return_sample (-1)
	, current_block_size (0)
	, _worst_output_latency (0)
	, _worst_input_latency (0)
	, _worst_route_latency (0)
	, _send_latency_changes (0)
	, _have_captured (false)
	, _non_soloed_outs_muted (false)
	, _listening (false)
	, _listen_cnt (0)
	, _solo_isolated_cnt (0)
	, _writable (false)
	, _under_nsm_control (false)
	, _xrun_count (0)
	, master_wait_end (0)
	, post_export_sync (false)
	, post_export_position (0)
	, _exporting (false)
	, _export_rolling (false)
	, _realtime_export (false)
	, _region_export (false)
	, _export_preroll (0)
	, _pre_export_mmc_enabled (false)
	, _name (snapshot_name)
	, _is_new (true)
	, _send_qf_mtc (false)
	, _pframes_since_last_mtc (0)
	, play_loop (false)
	, loop_changing (false)
	, last_loopend (0)
	, _session_dir (new SessionDirectory (fullpath))
	, _current_snapshot_name (snapshot_name)
	, state_tree (0)
	, state_was_pending (false)
	, _state_of_the_state (StateOfTheState (CannotSave | InitialConnecting | Loading))
	, _suspend_save (0)
	, _save_queued (false)
	, _save_queued_pending (false)
	, _last_roll_location (0)
	, _last_roll_or_reversal_location (0)
	, _last_record_location (0)
	, pending_auto_loop (false)
	, _mempool ("Session", 3145728)
	, lua (lua_newstate (&PBD::ReallocPool::lalloc, &_mempool))
	, _n_lua_scripts (0)
	, _butler (new Butler (*this))
	, _transport_fsm (new TransportFSM (*this))
	, _post_transport_work (0)
	, _locations (new Locations (*this))
	, _ignore_skips_updates (false)
	, _rt_thread_active (false)
	, _rt_emit_pending (false)
	, _ac_thread_active (0)
	, _latency_recompute_pending (0)
	, step_speed (0)
	, outbound_mtc_timecode_frame (0)
	, next_quarter_frame_to_send (-1)
	, _samples_per_timecode_frame (0)
	, _frames_per_hour (0)
	, _timecode_frames_per_hour (0)
	, last_timecode_valid (false)
	, last_timecode_when (0)
	, _send_timecode_update (false)
	, ltc_encoder (0)
	, ltc_enc_buf(0)
	, ltc_buf_off (0)
	, ltc_buf_len (0)
	, ltc_speed (0)
	, ltc_enc_byte (0)
	, ltc_enc_pos (0)
	, ltc_enc_cnt (0)
	, ltc_enc_off (0)
	, restarting (false)
	, ltc_prev_cycle (0)
	, ltc_timecode_offset (0)
	, ltc_timecode_negative_offset (false)
	, midi_control_ui (0)
	, _punch_or_loop (NoConstraint)
	, _tempo_map (0)
	, _all_route_group (new RouteGroup (*this, "all"))
	, routes (new RouteList)
	, _adding_routes_in_progress (false)
	, _reconnecting_routes_in_progress (false)
	, _route_deletion_in_progress (false)
	, destructive_index (0)
	, _track_number_decimals(1)
	, default_fade_steepness (0)
	, default_fade_msecs (0)
	, _total_free_4k_blocks (0)
	, _total_free_4k_blocks_uncertain (false)
	, no_questions_about_missing_files (false)
	, _playback_load (0)
	, _capture_load (0)
	, _bundles (new BundleList)
	, _bundle_xml_node (0)
	, _current_trans (0)
	, _clicking (false)
	, _click_rec_only (false)
	, click_data (0)
	, click_emphasis_data (0)
	, click_length (0)
	, click_emphasis_length (0)
	, _clicks_cleared (0)
	, _count_in_samples (0)
	, _play_range (false)
	, _range_selection (-1,-1)
	, _object_selection (-1,-1)
	, _preroll_record_trim_len (0)
	, _count_in_once (false)
	, main_outs (0)
	, first_file_data_format_reset (true)
	, first_file_header_format_reset (true)
	, have_looped (false)
	, _have_rec_enabled_track (false)
	, _have_rec_disabled_track (true)
	, _step_editors (0)
	, _suspend_timecode_transmission (0)
	,  _speakers (new Speakers)
	, _ignore_route_processor_changes (0)
	, midi_clock (0)
	, _scene_changer (0)
	, _midi_ports (0)
	, _mmc (0)
	, _vca_manager (new VCAManager (*this))
	, _selection (new CoreSelection (*this))
	, _global_locate_pending (false)
{
	created_with = string_compose ("%1 %2", PROGRAM_NAME, revision);

	pthread_mutex_init (&_rt_emit_mutex, 0);
	pthread_cond_init (&_rt_emit_cond, 0);

	pthread_mutex_init (&_auto_connect_mutex, 0);
	pthread_cond_init (&_auto_connect_cond, 0);

	init_name_id_counter (1); // reset for new sessions, start at 1
	VCA::set_next_vca_number (1); // reset for new sessions, start at 1

	pre_engine_init (fullpath); // sets _is_new

	setup_lua ();

	assert (AudioEngine::instance()->running());
	immediately_post_engine ();

	if (_is_new) {

		Stateful::loading_state_version = CURRENT_SESSION_FILE_VERSION;

		if (create (mix_template, bus_profile)) {
			destroy ();
			throw SessionException (_("Session initialization failed"));
		}

		/* if a mix template was provided, then ::create() will
		 * have copied it into the session and we need to load it
		 * so that we have the state ready for ::set_state()
		 * after the engine is started.
		 *
		 * Note that we do NOT try to get the sample rate from
		 * the template at this time, though doing so would
		 * be easy if we decided this was an appropriate part
		 * of a template.
		 */

		if (!mix_template.empty()) {
			try {
				if (load_state (_current_snapshot_name)) {
					throw SessionException (_("Failed to load template/snapshot state"));
				}
			} catch (PBD::unknown_enumeration& e) {
				throw SessionException (_("Failed to parse template/snapshot state"));
			}
			store_recent_templates (mix_template);
		}

		/* load default session properties - if any */
		config.load_state();

	} else {

		if (load_state (_current_snapshot_name)) {
			throw SessionException (_("Failed to load state"));
		}
	}

	int err = post_engine_init ();

	if (err) {
		destroy ();
		switch (err) {
			case -1:
				throw SessionException (string_compose (_("Cannot initialize session/engine: %1"), _("Failed to create background threads.")));
				break;
			case -2:
			case -3:
				throw SessionException (string_compose (_("Cannot initialize session/engine: %1"), _("Invalid TempoMap in session-file.")));
				break;
			case -4:
				throw SessionException (string_compose (_("Cannot initialize session/engine: %1"), _("Invalid or corrupt session state.")));
				break;
			case -5:
				throw SessionException (string_compose (_("Cannot initialize session/engine: %1"), _("Port registration failed.")));
				break;
			default:
				throw SessionException (string_compose (_("Cannot initialize session/engine: %1"), _("Unexpected exception during session setup, possibly invalid audio/midi engine parameters. Please see stdout/stderr for details")));
				break;
		}
	}

	store_recent_sessions (_name, _path);

	bool was_dirty = dirty();
	unset_dirty ();

	PresentationInfo::Change.connect_same_thread (*this, boost::bind (&Session::notify_presentation_info_change, this));

	Config->ParameterChanged.connect_same_thread (*this, boost::bind (&Session::config_changed, this, _1, false));
	config.ParameterChanged.connect_same_thread (*this, boost::bind (&Session::config_changed, this, _1, true));

	if (was_dirty) {
		DirtyChanged (); /* EMIT SIGNAL */
	}

	StartTimeChanged.connect_same_thread (*this, boost::bind (&Session::start_time_changed, this, _1));
	EndTimeChanged.connect_same_thread (*this, boost::bind (&Session::end_time_changed, this, _1));

	LatentSend::ChangedLatency.connect_same_thread (*this, boost::bind (&Session::send_latency_compensation_change, this));
	Latent::DisableSwitchChanged.connect_same_thread (*this, boost::bind (&Session::queue_latency_recompute, this));

	emit_thread_start ();
	auto_connect_thread_start ();

	/* hook us up to the engine since we are now completely constructed */

	BootMessage (_("Connect to engine"));

	_engine.set_session (this);
	_engine.reset_timebase ();

	ensure_subdirs (); // archived or zipped sessions may lack peaks/ analysis/ etc

	if (!mix_template.empty ()) {
		/* ::create() unsets _is_new after creating the session.
		 * But for templated sessions, the sample-rate is initially unset
		 * (not read from template), so we need to save it (again).
		 */
		_is_new = true;
	}

	session_loaded ();
	_is_new = false;

	BootMessage (_("Session loading complete"));
}

Session::~Session ()
{
#ifdef PT_TIMING
	ST.dump ("ST.dump");
#endif
	destroy ();
}

unsigned int
Session::next_name_id ()
{
	return g_atomic_int_add (&_name_id_counter, 1);
}

unsigned int
Session::name_id_counter ()
{
	return g_atomic_int_get (&_name_id_counter);
}

void
Session::init_name_id_counter (guint n)
{
	g_atomic_int_set (&_name_id_counter, n);
}

int
Session::immediately_post_engine ()
{
	/* Do various initializations that should take place directly after we
	 * know that the engine is running, but before we either create a
	 * session or set state for an existing one.
	 */

	_rt_tasklist.reset (new RTTaskList ());

	if (how_many_dsp_threads () > 1) {
		/* For now, only create the graph if we are using >1 DSP threads, as
		   it is a bit slower than the old code with 1 thread.
		*/
		_process_graph.reset (new Graph (*this));
	}

	/* every time we reconnect, recompute worst case output latencies */

	_engine.Running.connect_same_thread (*this, boost::bind (&Session::initialize_latencies, this));

	/* Restart transport FSM */

	_transport_fsm->start ();

	/* every time we reconnect, do stuff ... */

	_engine.Running.connect_same_thread (*this, boost::bind (&Session::engine_running, this));

	try {
		BootMessage (_("Set up LTC"));
		setup_ltc ();
		BootMessage (_("Set up Click"));
		setup_click ();
		BootMessage (_("Set up standard connections"));
		setup_bundles ();
	}

	catch (failed_constructor& err) {
		return -1;
	}

	/* TODO, connect in different thread. (PortRegisteredOrUnregistered may be in RT context)
	 * can we do that? */
	 _engine.PortRegisteredOrUnregistered.connect_same_thread (*this, boost::bind (&Session::setup_bundles, this));

	// set samplerate for plugins added early
	// e.g from templates or MB channelstrip
	set_block_size (_engine.samples_per_cycle());
	set_sample_rate (_engine.sample_rate());

	return 0;
}

void
Session::destroy ()
{
	vector<void*> debug_pointers;

	/* if we got to here, leaving pending capture state around
	   is a mistake.
	*/

	remove_pending_capture_state ();

	Analyser::flush ();

	_state_of_the_state = StateOfTheState (CannotSave | Deletion);

	{
		Glib::Threads::Mutex::Lock lm (AudioEngine::instance()->process_lock ());
		ltc_tx_cleanup();
	}

	/* disconnect from any and all signals that we are connected to */

	Port::PortSignalDrop (); /* EMIT SIGNAL */
	drop_connections ();

	/* shutdown control surface protocols while we still have ports
	 * and the engine to move data to any devices.
	 */
	ControlProtocolManager::instance().drop_protocols ();

	/* stop auto dis/connecting */
	auto_connect_thread_terminate ();

	_engine.remove_session ();

	/* deregister all ports - there will be no process or any other
	 * callbacks from the engine any more.
	 */

	Port::PortDrop (); /* EMIT SIGNAL */

	/* remove I/O objects that we (the session) own */
	_click_io.reset ();
	_ltc_output.reset ();

	{
		Glib::Threads::Mutex::Lock lm (controllables_lock);
		for (Controllables::iterator i = controllables.begin(); i != controllables.end(); ++i) {
			(*i)->DropReferences (); /* EMIT SIGNAL */
		}
		controllables.clear ();
	}

	/* clear history so that no references to objects are held any more */

	_history.clear ();

	/* clear state tree so that no references to objects are held any more */

	delete state_tree;
	state_tree = 0;

	{
		/* unregister all lua functions, drop held references (if any) */
		Glib::Threads::Mutex::Lock tm (lua_lock, Glib::Threads::TRY_LOCK);
		(*_lua_cleanup)();
		lua.do_command ("Session = nil");
		delete _lua_run;
		delete _lua_add;
		delete _lua_del;
		delete _lua_list;
		delete _lua_save;
		delete _lua_load;
		delete _lua_cleanup;
		lua.collect_garbage ();
	}

	/* reset dynamic state version back to default */
	Stateful::loading_state_version = 0;

	_butler->drop_references ();
	delete _butler;
	_butler = 0;

	delete _all_route_group;

	DEBUG_TRACE (DEBUG::Destruction, "delete route groups\n");
	for (list<RouteGroup *>::iterator i = _route_groups.begin(); i != _route_groups.end(); ++i) {
		delete *i;
	}

	if (click_data != default_click) {
		delete [] click_data;
	}

	if (click_emphasis_data != default_click_emphasis) {
		delete [] click_emphasis_data;
	}

	clear_clicks ();

	/* need to remove auditioner before monitoring section
	 * otherwise it is re-connected.
	 * Note: If a session was never successfully loaded, there
	 * may not yet be an auditioner.
	 */
	if (auditioner) {
		auditioner->drop_references ();
	}
	auditioner.reset ();

	/* drop references to routes held by the monitoring section
	 * specifically _monitor_out aux/listen references */
	remove_monitor_section();

	/* clear out any pending dead wood from RCU managed objects */

	routes.flush ();
	_bundles.flush ();

	DiskReader::free_working_buffers();

	/* tell everyone who is still standing that we're about to die */
	drop_references ();

	/* tell everyone to drop references and delete objects as we go */

	DEBUG_TRACE (DEBUG::Destruction, "delete regions\n");
	RegionFactory::delete_all_regions ();

	/* Do this early so that VCAs no longer hold references to routes */

	DEBUG_TRACE (DEBUG::Destruction, "delete vcas\n");
	delete _vca_manager;

	DEBUG_TRACE (DEBUG::Destruction, "delete routes\n");

	/* reset these three references to special routes before we do the usual route delete thing */

	_master_out.reset ();
	_monitor_out.reset ();

	{
		RCUWriter<RouteList> writer (routes);
		boost::shared_ptr<RouteList> r = writer.get_copy ();

		for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
			DEBUG_TRACE(DEBUG::Destruction, string_compose ("Dropping for route %1 ; pre-ref = %2\n", (*i)->name(), (*i).use_count()));
			(*i)->drop_references ();
		}

		r->clear ();
		/* writer goes out of scope and updates master */
	}
	routes.flush ();

	{
		DEBUG_TRACE (DEBUG::Destruction, "delete sources\n");
		Glib::Threads::Mutex::Lock lm (source_lock);
		for (SourceMap::iterator i = sources.begin(); i != sources.end(); ++i) {
			DEBUG_TRACE(DEBUG::Destruction, string_compose ("Dropping for source %1 ; pre-ref = %2\n", i->second->name(), i->second.use_count()));
			i->second->drop_references ();
		}

		sources.clear ();
	}

	/* not strictly necessary, but doing it here allows the shared_ptr debugging to work */
	_playlists.reset ();

	emit_thread_terminate ();

	pthread_cond_destroy (&_rt_emit_cond);
	pthread_mutex_destroy (&_rt_emit_mutex);

	pthread_cond_destroy (&_auto_connect_cond);
	pthread_mutex_destroy (&_auto_connect_mutex);

	delete _scene_changer; _scene_changer = 0;
	delete midi_control_ui; midi_control_ui = 0;

	delete _mmc; _mmc = 0;
	delete _midi_ports; _midi_ports = 0;
	delete _locations; _locations = 0;

	delete midi_clock;
	delete _tempo_map;

	/* clear event queue, the session is gone, nobody is interested in
	 * those anymore, but they do leak memory if not removed
	 */
	while (!immediate_events.empty ()) {
		Glib::Threads::Mutex::Lock lm (AudioEngine::instance()->process_lock ());
		SessionEvent *ev = immediate_events.front ();
		DEBUG_TRACE (DEBUG::SessionEvents, string_compose ("Drop event: %1\n", enum_2_string (ev->type)));
		immediate_events.pop_front ();
		bool remove = true;
		bool del = true;
		switch (ev->type) {
			case SessionEvent::AutoLoop:
			case SessionEvent::Skip:
			case SessionEvent::PunchIn:
			case SessionEvent::PunchOut:
			case SessionEvent::RangeStop:
			case SessionEvent::RangeLocate:
				remove = false;
				del = false;
				break;
			case SessionEvent::RealTimeOperation:
				process_rtop (ev);
				del = false;
			default:
				break;
		}
		if (remove) {
			del = del && !_remove_event (ev);
		}
		if (del) {
			delete ev;
		}
	}

	{
		/* unregister all dropped ports, process pending port deletion. */
		// this may call ARDOUR::Port::drop ... jack_port_unregister ()
		// jack1 cannot cope with removing ports while processing
		Glib::Threads::Mutex::Lock lm (AudioEngine::instance()->process_lock ());
		AudioEngine::instance()->clear_pending_port_deletions ();
	}

	DEBUG_TRACE (DEBUG::Destruction, "delete selection\n");
	delete _selection;
	_selection = 0;

	_transport_fsm->stop ();

	DEBUG_TRACE (DEBUG::Destruction, "Session::destroy() done\n");

#ifndef NDEBUG
	Controllable::dump_registry ();
#endif

	BOOST_SHOW_POINTERS ();
}

void
Session::setup_ltc ()
{
	XMLNode* child = 0;

	_ltc_output.reset (new IO (*this, X_("LTC Out"), IO::Output));

	if (state_tree && (child = find_named_node (*state_tree->root(), X_("LTC Out"))) != 0) {
		_ltc_output->set_state (*(child->children().front()), Stateful::loading_state_version);
	} else {
		{
			Glib::Threads::Mutex::Lock lm (AudioEngine::instance()->process_lock ());
			_ltc_output->ensure_io (ChanCount (DataType::AUDIO, 1), true, this);
			// TODO use auto-connect thread
			reconnect_ltc_output ();
		}
	}

	/* fix up names of LTC ports because we don't want the normal
	 * IO style of NAME/TYPE-{in,out}N
	 */

	_ltc_output->nth (0)->set_name (X_("LTC-out"));
}

void
Session::setup_click ()
{
	_clicking = false;

	boost::shared_ptr<AutomationList> gl (new AutomationList (Evoral::Parameter (GainAutomation)));
	boost::shared_ptr<GainControl> gain_control = boost::shared_ptr<GainControl> (new GainControl (*this, Evoral::Parameter(GainAutomation), gl));

	_click_io.reset (new ClickIO (*this, X_("Click")));
	_click_gain.reset (new Amp (*this, _("Fader"), gain_control, true));
	_click_gain->activate ();
	if (state_tree) {
		setup_click_state (state_tree->root());
	} else {
		setup_click_state (0);
	}
}

void
Session::setup_click_state (const XMLNode* node)
{
	const XMLNode* child = 0;

	if (node && (child = find_named_node (*node, "Click")) != 0) {

		/* existing state for Click */
		int c = 0;

		if (Stateful::loading_state_version < 3000) {
			c = _click_io->set_state_2X (*child->children().front(), Stateful::loading_state_version, false);
		} else {
			const XMLNodeList& children (child->children());
			XMLNodeList::const_iterator i = children.begin();
			if ((c = _click_io->set_state (**i, Stateful::loading_state_version)) == 0) {
				++i;
				if (i != children.end()) {
					c = _click_gain->set_state (**i, Stateful::loading_state_version);
				}
			}
		}

		if (c == 0) {
			_clicking = Config->get_clicking ();

		} else {

			error << _("could not setup Click I/O") << endmsg;
			_clicking = false;
		}


	} else {

		/* default state for Click: dual-mono to first 2 physical outputs */

		vector<string> outs;
		_engine.get_physical_outputs (DataType::AUDIO, outs);

		for (uint32_t physport = 0; physport < 2; ++physport) {
			if (outs.size() > physport) {
				if (_click_io->add_port (outs[physport], this)) {
					// relax, even though its an error
				}
			}
		}

		if (_click_io->n_ports () > ChanCount::ZERO) {
			_clicking = Config->get_clicking ();
		}
	}
}

void
Session::get_physical_ports (vector<string>& inputs, vector<string>& outputs, DataType type,
                             MidiPortFlags include, MidiPortFlags exclude)
{
	_engine.get_physical_inputs (type, inputs, include, exclude);
	_engine.get_physical_outputs (type, outputs, include, exclude);
}


void
Session::auto_connect_master_bus ()
{
	if (!_master_out || !Config->get_auto_connect_standard_busses() || _monitor_out) {
		return;
	}

	/* if requested auto-connect the outputs to the first N physical ports.
	 */

	uint32_t limit = _master_out->n_outputs().n_total();
	vector<string> outputs[DataType::num_types];

	for (uint32_t i = 0; i < DataType::num_types; ++i) {
		_engine.get_physical_outputs (DataType (DataType::Symbol (i)), outputs[i]);
	}

	for (uint32_t n = 0; n < limit; ++n) {
		boost::shared_ptr<Port> p = _master_out->output()->nth (n);
		string connect_to;
		if (outputs[p->type()].size() > n) {
			connect_to = outputs[p->type()][n];
		}

		if (!connect_to.empty() && p->connected_to (connect_to) == false) {
			if (_master_out->output()->connect (p, connect_to, this)) {
				error << string_compose (_("cannot connect master output %1 to %2"), n, connect_to)
				      << endmsg;
				break;
			}
		}
	}
}

void
Session::remove_monitor_section ()
{
	if (!_monitor_out) {
		return;
	}

	/* allow deletion when session is unloaded */
	if (!_engine.running() && !deletion_in_progress ()) {
		error << _("Cannot remove monitor section while the engine is offline.") << endmsg;
		return;
	}

	/* force reversion to Solo-In-Place */
	Config->set_solo_control_is_listen_control (false);

	/* if we are auditioning, cancel it ... this is a workaround
	   to a problem (auditioning does not execute the process graph,
	   which is needed to remove routes when using >1 core for processing)
	*/
	cancel_audition ();

	if (!deletion_in_progress ()) {
		/* Hold process lock while doing this so that we don't hear bits and
		 * pieces of audio as we work on each route.
		 */

		Glib::Threads::Mutex::Lock lm (AudioEngine::instance()->process_lock ());

		/* Connect tracks to monitor section. Note that in an
		   existing session, the internal sends will already exist, but we want the
		   routes to notice that they connect to the control out specifically.
		*/


		boost::shared_ptr<RouteList> r = routes.reader ();
		ProcessorChangeBlocker  pcb (this, false);

		for (RouteList::iterator x = r->begin(); x != r->end(); ++x) {

			if ((*x)->is_monitor()) {
				/* relax */
			} else if ((*x)->is_master()) {
				/* relax */
			} else {
				(*x)->remove_aux_or_listen (_monitor_out);
			}
		}
	}

	remove_route (_monitor_out);
	if (deletion_in_progress ()) {
		return;
	}

	auto_connect_master_bus ();

	if (auditioner) {
		auditioner->connect ();
	}

	MonitorBusAddedOrRemoved (); /* EMIT SIGNAL */
}

void
Session::add_monitor_section ()
{
	RouteList rl;

	if (!_engine.running()) {
		error << _("Cannot create monitor section while the engine is offline.") << endmsg;
		return;
	}

	if (_monitor_out || !_master_out) {
		return;
	}

	boost::shared_ptr<Route> r (new Route (*this, _("Monitor"), PresentationInfo::MonitorOut, DataType::AUDIO));

	if (r->init ()) {
		return;
	}

	BOOST_MARK_ROUTE(r);

	try {
		Glib::Threads::Mutex::Lock lm (AudioEngine::instance()->process_lock ());
		r->input()->ensure_io (_master_out->output()->n_ports(), false, this);
		r->output()->ensure_io (_master_out->output()->n_ports(), false, this);
	} catch (...) {
		error << _("Cannot create monitor section. 'Monitor' Port name is not unique.") << endmsg;
		return;
	}

	rl.push_back (r);
	add_routes (rl, false, false, false, 0);

	assert (_monitor_out);

	/* AUDIO ONLY as of june 29th 2009, because listen semantics for anything else
	   are undefined, at best.
	*/

	uint32_t limit = _monitor_out->n_inputs().n_audio();

	if (_master_out) {

		/* connect the inputs to the master bus outputs. this
		 * represents a separate data feed from the internal sends from
		 * each route. as of jan 2011, it allows the monitor section to
		 * conditionally ignore either the internal sends or the normal
		 * input feed, but we should really find a better way to do
		 * this, i think.
		 */

		_master_out->output()->disconnect (this);

		for (uint32_t n = 0; n < limit; ++n) {
			boost::shared_ptr<AudioPort> p = _monitor_out->input()->ports().nth_audio_port (n);
			boost::shared_ptr<AudioPort> o = _master_out->output()->ports().nth_audio_port (n);

			if (o) {
				string connect_to = o->name();
				if (_monitor_out->input()->connect (p, connect_to, this)) {
					error << string_compose (_("cannot connect control input %1 to %2"), n, connect_to)
					      << endmsg;
					break;
				}
			}
		}
	}

	/* if monitor section is not connected, connect it to physical outs
	 */

	if ((Config->get_auto_connect_standard_busses () || Profile->get_mixbus ()) && !_monitor_out->output()->connected ()) {

		if (!Config->get_monitor_bus_preferred_bundle().empty()) {

			boost::shared_ptr<Bundle> b = bundle_by_name (Config->get_monitor_bus_preferred_bundle());

			if (b) {
				_monitor_out->output()->connect_ports_to_bundle (b, true, this);
			} else {
				warning << string_compose (_("The preferred I/O for the monitor bus (%1) cannot be found"),
							   Config->get_monitor_bus_preferred_bundle())
					<< endmsg;
			}

		} else {

			/* Monitor bus is audio only */

			vector<string> outputs[DataType::num_types];

			for (uint32_t i = 0; i < DataType::num_types; ++i) {
				_engine.get_physical_outputs (DataType (DataType::Symbol (i)), outputs[i]);
			}

			uint32_t mod = outputs[DataType::AUDIO].size();
			uint32_t limit = _monitor_out->n_outputs().get (DataType::AUDIO);

			if (mod != 0) {

				for (uint32_t n = 0; n < limit; ++n) {

					boost::shared_ptr<Port> p = _monitor_out->output()->ports().port(DataType::AUDIO, n);
					string connect_to;
					if (outputs[DataType::AUDIO].size() > (n % mod)) {
						connect_to = outputs[DataType::AUDIO][n % mod];
					}

					if (!connect_to.empty()) {
						if (_monitor_out->output()->connect (p, connect_to, this)) {
							error << string_compose (
								_("cannot connect control output %1 to %2"),
								n, connect_to)
							      << endmsg;
							break;
						}
					}
				}
			}
		}
	}

	/* Hold process lock while doing this so that we don't hear bits and
	 * pieces of audio as we work on each route.
	 */

	Glib::Threads::Mutex::Lock lm (AudioEngine::instance()->process_lock ());

	/* Connect tracks to monitor section. Note that in an
	   existing session, the internal sends will already exist, but we want the
	   routes to notice that they connect to the control out specifically.
	*/


	boost::shared_ptr<RouteList> rls = routes.reader ();

	ProcessorChangeBlocker  pcb (this, false /* XXX */);

	for (RouteList::iterator x = rls->begin(); x != rls->end(); ++x) {

		if ((*x)->is_monitor()) {
			/* relax */
		} else if ((*x)->is_master()) {
			/* relax */
		} else {
			(*x)->enable_monitor_send ();
		}
	}

	if (auditioner) {
		auditioner->connect ();
	}

	MonitorBusAddedOrRemoved (); /* EMIT SIGNAL */
}

void
Session::reset_monitor_section ()
{
	/* Process lock should be held by the caller.*/

	if (!_monitor_out) {
		return;
	}

	uint32_t limit = _master_out->n_outputs().n_audio();

	/* connect the inputs to the master bus outputs. this
	 * represents a separate data feed from the internal sends from
	 * each route. as of jan 2011, it allows the monitor section to
	 * conditionally ignore either the internal sends or the normal
	 * input feed, but we should really find a better way to do
	 * this, i think.
	 */

	_master_out->output()->disconnect (this);
	_monitor_out->output()->disconnect (this);

	// monitor section follow master bus - except midi
	ChanCount mon_chn (_master_out->output()->n_ports());
	mon_chn.set_midi (0);

	_monitor_out->input()->ensure_io (mon_chn, false, this);
	_monitor_out->output()->ensure_io (mon_chn, false, this);

	for (uint32_t n = 0; n < limit; ++n) {
		boost::shared_ptr<AudioPort> p = _monitor_out->input()->ports().nth_audio_port (n);
		boost::shared_ptr<AudioPort> o = _master_out->output()->ports().nth_audio_port (n);

		if (o) {
			string connect_to = o->name();
			if (_monitor_out->input()->connect (p, connect_to, this)) {
				error << string_compose (_("cannot connect control input %1 to %2"), n, connect_to)
				      << endmsg;
				break;
			}
		}
	}

	/* connect monitor section to physical outs
	 */

	if (Config->get_auto_connect_standard_busses()) {

		if (!Config->get_monitor_bus_preferred_bundle().empty()) {

			boost::shared_ptr<Bundle> b = bundle_by_name (Config->get_monitor_bus_preferred_bundle());

			if (b) {
				_monitor_out->output()->connect_ports_to_bundle (b, true, this);
			} else {
				warning << string_compose (_("The preferred I/O for the monitor bus (%1) cannot be found"),
							   Config->get_monitor_bus_preferred_bundle())
					<< endmsg;
			}

		} else {

			/* Monitor bus is audio only */

			vector<string> outputs[DataType::num_types];

			for (uint32_t i = 0; i < DataType::num_types; ++i) {
				_engine.get_physical_outputs (DataType (DataType::Symbol (i)), outputs[i]);
			}

			uint32_t mod = outputs[DataType::AUDIO].size();
			uint32_t limit = _monitor_out->n_outputs().get (DataType::AUDIO);

			if (mod != 0) {

				for (uint32_t n = 0; n < limit; ++n) {

					boost::shared_ptr<Port> p = _monitor_out->output()->ports().port(DataType::AUDIO, n);
					string connect_to;
					if (outputs[DataType::AUDIO].size() > (n % mod)) {
						connect_to = outputs[DataType::AUDIO][n % mod];
					}

					if (!connect_to.empty()) {
						if (_monitor_out->output()->connect (p, connect_to, this)) {
							error << string_compose (
								_("cannot connect control output %1 to %2"),
								n, connect_to)
							      << endmsg;
							break;
						}
					}
				}
			}
		}
	}

	/* Connect tracks to monitor section. Note that in an
	   existing session, the internal sends will already exist, but we want the
	   routes to notice that they connect to the control out specifically.
	*/


	boost::shared_ptr<RouteList> rls = routes.reader ();

	ProcessorChangeBlocker pcb (this, false);

	for (RouteList::iterator x = rls->begin(); x != rls->end(); ++x) {

		if ((*x)->is_monitor()) {
			/* relax */
		} else if ((*x)->is_master()) {
			/* relax */
		} else {
			(*x)->enable_monitor_send ();
		}
	}
}

int
Session::add_master_bus (ChanCount const& count)
{
	if (master_out ()) {
		return -1;
	}

	RouteList rl;

	boost::shared_ptr<Route> r (new Route (*this, _("Master"), PresentationInfo::MasterOut, DataType::AUDIO));
	if (r->init ()) {
		return -1;
	}

	BOOST_MARK_ROUTE(r);

	{
		Glib::Threads::Mutex::Lock lm (AudioEngine::instance()->process_lock ());
		r->input()->ensure_io (count, false, this);
		r->output()->ensure_io (count, false, this);
	}

	rl.push_back (r);
	add_routes (rl, false, false, false, PresentationInfo::max_order);
	return 0;
}

void
Session::hookup_io ()
{
	/* stop graph reordering notifications from
	   causing resorts, etc.
	*/

	_state_of_the_state = StateOfTheState (_state_of_the_state | InitialConnecting);

	if (!auditioner) {

		/* we delay creating the auditioner till now because
		   it makes its own connections to ports.
		*/

		try {
			boost::shared_ptr<Auditioner> a (new Auditioner (*this));
			if (a->init()) {
				throw failed_constructor ();
			}
			auditioner = a;
		}

		catch (failed_constructor& err) {
			warning << _("cannot create Auditioner: no auditioning of regions possible") << endmsg;
		}
	}

	/* load bundles, which we may have postponed earlier on */
	if (_bundle_xml_node) {
		load_bundles (*_bundle_xml_node);
		delete _bundle_xml_node;
	}

	/* Tell all IO objects to connect themselves together */

	IO::enable_connecting ();

	/* Now tell all "floating" ports to connect to whatever
	   they should be connected to.
	*/

	AudioEngine::instance()->reconnect_ports ();
	TransportMasterManager::instance().reconnect_ports ();

	/* Anyone who cares about input state, wake up and do something */

	IOConnectionsComplete (); /* EMIT SIGNAL */

	_state_of_the_state = StateOfTheState (_state_of_the_state & ~InitialConnecting);

	/* now handle the whole enchilada as if it was one
	   graph reorder event.
	*/

	graph_reordered (false);

	/* update the full solo state, which can't be
	   correctly determined on a per-route basis, but
	   needs the global overview that only the session
	   has.
	*/

	update_route_solo_state ();
}

void
Session::track_playlist_changed (boost::weak_ptr<Track> wp)
{
	boost::shared_ptr<Track> track = wp.lock ();
	if (!track) {
		return;
	}

	boost::shared_ptr<Playlist> playlist;

	if ((playlist = track->playlist()) != 0) {
		playlist->RegionAdded.connect_same_thread (*this, boost::bind (&Session::playlist_region_added, this, _1));
		playlist->RangesMoved.connect_same_thread (*this, boost::bind (&Session::playlist_ranges_moved, this, _1));
		playlist->RegionsExtended.connect_same_thread (*this, boost::bind (&Session::playlist_regions_extended, this, _1));
	}
}

bool
Session::record_enabling_legal () const
{
	if (Config->get_all_safe()) {
		return false;
	}
	return true;
}

void
Session::set_track_monitor_input_status (bool yn)
{
	boost::shared_ptr<RouteList> rl = routes.reader ();
	for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
		boost::shared_ptr<AudioTrack> tr = boost::dynamic_pointer_cast<AudioTrack> (*i);
		if (tr && tr->rec_enable_control()->get_value()) {
			//cerr << "switching to input = " << !auto_input << __FILE__ << __LINE__ << endl << endl;
			tr->request_input_monitoring (yn);
		}
	}
}

void
Session::auto_punch_start_changed (Location* location)
{
	replace_event (SessionEvent::PunchIn, location->start());

	if (get_record_enabled() && config.get_punch_in() && !actively_recording ()) {
		/* capture start has been changed, so save new pending state */
		save_state ("", true);
	}
}

bool
Session::punch_active () const
{
	if (!get_record_enabled ()) {
		return false;
	}
	if (!_locations->auto_punch_location ()) {
		return false;
	}
	return config.get_punch_in () || config.get_punch_out ();
}

bool
Session::punch_is_possible () const
{
	return g_atomic_int_get (&_punch_or_loop) != OnlyLoop;
}

bool
Session::loop_is_possible () const
{
#if 0 /* maybe prevent looping even when not rolling ? */
	if (get_record_enabled () && punch_active ()) {
			return false;
		}
	}
#endif
	return g_atomic_int_get(&_punch_or_loop) != OnlyPunch;
}

void
Session::reset_punch_loop_constraint ()
{
	if (g_atomic_int_get (&_punch_or_loop) == NoConstraint) {
		return;
	}
	g_atomic_int_set (&_punch_or_loop, NoConstraint);
	PunchLoopConstraintChange (); /* EMIT SIGNAL */
}

bool
Session::maybe_allow_only_loop (bool play_loop) {
	if (!(get_play_loop () || play_loop)) {
		return false;
	}
	bool rv = g_atomic_int_compare_and_exchange (&_punch_or_loop, NoConstraint, OnlyLoop);
	if (rv) {
		PunchLoopConstraintChange (); /* EMIT SIGNAL */
	}
	if (rv || loop_is_possible ()) {
		unset_punch ();
		return true;
	}
	return false;
}

bool
Session::maybe_allow_only_punch () {
	if (!punch_active ()) {
		return false;
	}
	bool rv = g_atomic_int_compare_and_exchange (&_punch_or_loop, NoConstraint, OnlyPunch);
	if (rv) {
		PunchLoopConstraintChange (); /* EMIT SIGNAL */
	}
	return rv || punch_is_possible ();
}

void
Session::unset_punch ()
{
	/* used when enabling looping
	 * -> _punch_or_loop = OnlyLoop;
	 */
	if (config.get_punch_in ()) {
		config.set_punch_in (false);
	}
	if (config.get_punch_out ()) {
		config.set_punch_out (false);
	}
}

void
Session::auto_punch_end_changed (Location* location)
{
	replace_event (SessionEvent::PunchOut, location->end());
}

void
Session::auto_punch_changed (Location* location)
{
	auto_punch_start_changed (location);
	auto_punch_end_changed (location);
}

void
Session::auto_loop_changed (Location* location)
{
	if (!location) {
		return;
	}

	replace_event (SessionEvent::AutoLoop, location->end(), location->start());

	const bool rolling = transport_rolling ();

	boost::shared_ptr<RouteList> r = routes.reader ();

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		(*i)->reload_loop ();
	}


	if (rolling) {

		if (get_play_loop ()) {

			if (_transport_sample < location->start() || _transport_sample > location->end()) {

				/* new loop range excludes current transport
				 * sample => relocate to beginning of loop and roll.
				 */

				/* Set this so that when/if we have to stop the
				 * transport for a locate, we know that it is triggered
				 * by loop-changing, and we do not cancel play loop
				 */

				loop_changing = true;
				request_locate (location->start(), MustRoll);

			} else {

				// schedule a buffer overwrite to refill buffers with the new loop.

				request_overwrite_buffer (boost::shared_ptr<Track>(), LoopChanged);
			}
		}

	} else {

		/* possibly move playhead if not rolling; if we are rolling we'll move
		   to the loop start on stop if that is appropriate.
		*/

		samplepos_t pos;

		if (select_playhead_priority_target (pos)) {
			if (pos == location->start()) {
				request_locate (pos);
			}
		}
	}

	last_loopend = location->end();
	set_dirty ();
}

void
Session::set_auto_punch_location (Location* location)
{
	Location* existing;

	if ((existing = _locations->auto_punch_location()) != 0 && existing != location) {
		punch_connections.drop_connections();
		existing->set_auto_punch (false, this);
		clear_events (SessionEvent::PunchIn);
		clear_events (SessionEvent::PunchOut);
		auto_punch_location_changed (0);
	}

	set_dirty();

	if (location == 0) {
		return;
	}

	if (location->end() <= location->start()) {
		error << _("Session: you can't use that location for auto punch (start <= end)") << endmsg;
		return;
	}

	punch_connections.drop_connections ();

	location->StartChanged.connect_same_thread (punch_connections, boost::bind (&Session::auto_punch_start_changed, this, location));
	location->EndChanged.connect_same_thread (punch_connections, boost::bind (&Session::auto_punch_end_changed, this, location));
	location->Changed.connect_same_thread (punch_connections, boost::bind (&Session::auto_punch_changed, this, location));

	location->set_auto_punch (true, this);

	auto_punch_changed (location);

	auto_punch_location_changed (location);
}

void
Session::set_session_extents (samplepos_t start, samplepos_t end)
{
	if (end <= start) {
		error << _("Session: you can't use that location for session start/end)") << endmsg;
		return;
	}

	Location* existing;
	if ((existing = _locations->session_range_location()) == 0) {
		_session_range_location = new Location (*this, start, end, _("session"), Location::IsSessionRange, 0);
		_locations->add (_session_range_location);
	} else {
		existing->set( start, end );
	}

	set_dirty();
}

void
Session::set_auto_loop_location (Location* location)
{
	Location* existing;

	if ((existing = _locations->auto_loop_location()) != 0 && existing != location) {
		loop_connections.drop_connections ();
		existing->set_auto_loop (false, this);
		remove_event (existing->end(), SessionEvent::AutoLoop);
		auto_loop_location_changed (0);
	}

	set_dirty();

	if (location == 0) {
		return;
	}

	if (location->end() <= location->start()) {
		error << _("You cannot use this location for auto-loop because it has zero or negative length") << endmsg;
		return;
	}

	last_loopend = location->end();

	loop_connections.drop_connections ();

	location->StartChanged.connect_same_thread (loop_connections, boost::bind (&Session::auto_loop_changed, this, location));
	location->EndChanged.connect_same_thread (loop_connections, boost::bind (&Session::auto_loop_changed, this, location));
	location->Changed.connect_same_thread (loop_connections, boost::bind (&Session::auto_loop_changed, this, location));
	location->FlagsChanged.connect_same_thread (loop_connections, boost::bind (&Session::auto_loop_changed, this, location));

	location->set_auto_loop (true, this);

	if (Config->get_loop_is_mode() && get_play_loop ()) {
		/* set all tracks to use internal looping */
		set_track_loop (true);
	}

	/* take care of our stuff first */

	auto_loop_changed (location);

	/* now tell everyone else */

	auto_loop_location_changed (location);
}

void
Session::update_marks (Location*)
{
	set_dirty ();
}

void
Session::update_skips (Location* loc, bool consolidate)
{
	if (_ignore_skips_updates) {
		return;
	}

	Locations::LocationList skips;

	if (consolidate) {
		PBD::Unwinder<bool> uw (_ignore_skips_updates, true);
		consolidate_skips (loc);
	}

	sync_locations_to_skips ();

	set_dirty ();
}

void
Session::consolidate_skips (Location* loc)
{
	Locations::LocationList all_locations = _locations->list ();

	for (Locations::LocationList::iterator l = all_locations.begin(); l != all_locations.end(); ) {

		if (!(*l)->is_skip ()) {
			++l;
			continue;
		}

		/* don't test against self */

		if (*l == loc) {
			++l;
			continue;
		}

		switch (Evoral::coverage ((*l)->start(), (*l)->end(), loc->start(), loc->end())) {
			case Evoral::OverlapInternal:
			case Evoral::OverlapExternal:
			case Evoral::OverlapStart:
			case Evoral::OverlapEnd:
				/* adjust new location to cover existing one */
				loc->set_start (min (loc->start(), (*l)->start()));
				loc->set_end (max (loc->end(), (*l)->end()));
				/* we don't need this one any more */
				_locations->remove (*l);
				/* the location has been deleted, so remove reference to it in our local list */
				l = all_locations.erase (l);
				break;

			case Evoral::OverlapNone:
				++l;
				break;
		}
	}
}

void
Session::sync_locations_to_skips ()
{
	/* This happens asynchronously (in the audioengine thread). After the clear is done, we will call
	 * Session::_sync_locations_to_skips() from the audioengine thread.
	 */
	clear_events (SessionEvent::Skip, boost::bind (&Session::_sync_locations_to_skips, this));
}

void
Session::_sync_locations_to_skips ()
{
	/* called as a callback after existing Skip events have been cleared from a realtime audioengine thread */

	Locations::LocationList const & locs (_locations->list());

	for (Locations::LocationList::const_iterator i = locs.begin(); i != locs.end(); ++i) {

		Location* location = *i;

		if (location->is_skip() && location->is_skipping()) {
			SessionEvent* ev = new SessionEvent (SessionEvent::Skip, SessionEvent::Add, location->start(), location->end(), 1.0);
			queue_event (ev);
		}
	}
}


void
Session::location_added (Location *location)
{
	if (location->is_auto_punch()) {
		set_auto_punch_location (location);
	}

	if (location->is_auto_loop()) {
		set_auto_loop_location (location);
	}

	if (location->is_session_range()) {
		/* no need for any signal handling or event setting with the session range,
			 because we keep a direct reference to it and use its start/end directly.
			 */
		_session_range_location = location;
	}

	if (location->is_mark()) {
		/* listen for per-location signals that require us to do any * global updates for marks */

		location->StartChanged.connect_same_thread (skip_update_connections, boost::bind (&Session::update_marks, this, location));
		location->EndChanged.connect_same_thread (skip_update_connections, boost::bind (&Session::update_marks, this, location));
		location->Changed.connect_same_thread (skip_update_connections, boost::bind (&Session::update_marks, this, location));
		location->FlagsChanged.connect_same_thread (skip_update_connections, boost::bind (&Session::update_marks, this, location));
		location->PositionLockStyleChanged.connect_same_thread (skip_update_connections, boost::bind (&Session::update_marks, this, location));
	}

	if (location->is_range_marker()) {
		/* listen for per-location signals that require us to do any * global updates for marks */

		location->StartChanged.connect_same_thread (skip_update_connections, boost::bind (&Session::update_marks, this, location));
		location->EndChanged.connect_same_thread (skip_update_connections, boost::bind (&Session::update_marks, this, location));
		location->Changed.connect_same_thread (skip_update_connections, boost::bind (&Session::update_marks, this, location));
		location->FlagsChanged.connect_same_thread (skip_update_connections, boost::bind (&Session::update_marks, this, location));
		location->PositionLockStyleChanged.connect_same_thread (skip_update_connections, boost::bind (&Session::update_marks, this, location));
	}

	if (location->is_skip()) {
		/* listen for per-location signals that require us to update skip-locate events */

		location->StartChanged.connect_same_thread (skip_update_connections, boost::bind (&Session::update_skips, this, location, true));
		location->EndChanged.connect_same_thread (skip_update_connections, boost::bind (&Session::update_skips, this, location, true));
		location->Changed.connect_same_thread (skip_update_connections, boost::bind (&Session::update_skips, this, location, true));
		location->FlagsChanged.connect_same_thread (skip_update_connections, boost::bind (&Session::update_skips, this, location, false));
		location->PositionLockStyleChanged.connect_same_thread (skip_update_connections, boost::bind (&Session::update_marks, this, location));

		update_skips (location, true);
	}

	set_dirty ();
}

void
Session::location_removed (Location *location)
{
	if (location->is_auto_loop()) {
		set_auto_loop_location (0);
		if (!get_play_loop ()) {
			set_track_loop (false);
		}
		unset_play_loop ();
	}

	if (location->is_auto_punch()) {
		set_auto_punch_location (0);
	}

	if (location->is_session_range()) {
		/* this is never supposed to happen */
		error << _("programming error: session range removed!") << endl;
	}

	if (location->is_skip()) {

		update_skips (location, false);
	}

	set_dirty ();
}

void
Session::locations_changed ()
{
	_locations->apply (*this, &Session::_locations_changed);
}

void
Session::_locations_changed (const Locations::LocationList& locations)
{
	/* There was some mass-change in the Locations object.
	 *
	 * We might be re-adding a location here but it doesn't actually matter
	 * for all the locations that the Session takes an interest in.
	 */

	{
		PBD::Unwinder<bool> protect_ignore_skip_updates (_ignore_skips_updates, true);
		for (Locations::LocationList::const_iterator i = locations.begin(); i != locations.end(); ++i) {
			location_added (*i);
		}
	}

	update_skips (NULL, false);
}

void
Session::enable_record ()
{
	if (_transport_speed != 0.0 && _transport_speed != 1.0) {
		/* no recording at anything except normal speed */
		return;
	}

	while (1) {
		RecordState rs = (RecordState) g_atomic_int_get (&_record_status);

		if (rs == Recording) {
			break;
		}

		if (g_atomic_int_compare_and_exchange (&_record_status, rs, Recording)) {

			_last_record_location = _transport_sample;
			send_immediate_mmc (MIDI::MachineControlCommand (MIDI::MachineControl::cmdRecordStrobe));

			if (Config->get_monitoring_model() == HardwareMonitoring && config.get_auto_input()) {
				set_track_monitor_input_status (true);
			}

			RecordStateChanged ();
			break;
		}
	}
}

void
Session::set_all_tracks_record_enabled (bool enable )
{
	boost::shared_ptr<RouteList> rl = routes.reader();
	set_controls (route_list_to_control_list (rl, &Stripable::rec_enable_control), enable, Controllable::NoGroup);
}

void
Session::disable_record (bool rt_context, bool force)
{
	RecordState rs;

	if ((rs = (RecordState) g_atomic_int_get (&_record_status)) != Disabled) {

		if (!Config->get_latched_record_enable () || force) {
			g_atomic_int_set (&_record_status, Disabled);
			send_immediate_mmc (MIDI::MachineControlCommand (MIDI::MachineControl::cmdRecordExit));
		} else {
			if (rs == Recording) {
				g_atomic_int_set (&_record_status, Enabled);
			}
		}

		if (Config->get_monitoring_model() == HardwareMonitoring && config.get_auto_input()) {
			set_track_monitor_input_status (false);
		}

		RecordStateChanged (); /* emit signal */
	}
}

void
Session::step_back_from_record ()
{
	if (g_atomic_int_compare_and_exchange (&_record_status, Recording, Enabled)) {

		if (Config->get_monitoring_model() == HardwareMonitoring && config.get_auto_input()) {
			set_track_monitor_input_status (false);
		}

		RecordStateChanged (); /* emit signal */
	}
}

void
Session::maybe_enable_record (bool rt_context)
{
	if (_step_editors > 0) {
		return;
	}

	g_atomic_int_set (&_record_status, Enabled);

	/* This function is currently called from somewhere other than an RT thread.
	 * (except maybe lua scripts, which can use rt_context = true)
	 * This save_state() call therefore doesn't impact anything.  Doing it here
	 * means that we save pending state of which sources the next record will use,
	 * which gives us some chance of recovering from a crash during the record.
	 */

	if (!rt_context) {
		save_state ("", true);
	}

	if (_transport_speed) {
		maybe_allow_only_punch ();
		if (!config.get_punch_in()) {
			enable_record ();
		}
	} else {
		send_immediate_mmc (MIDI::MachineControlCommand (MIDI::MachineControl::cmdRecordPause));
		RecordStateChanged (); /* EMIT SIGNAL */
	}

	set_dirty();
}

samplepos_t
Session::audible_sample (bool* latent_locate) const
{
	if (latent_locate) {
		*latent_locate = false;
	}

	samplepos_t ret;

	if (synced_to_engine()) {
		/* Note: this is basically just sync-to-JACK */
		ret = _engine.transport_sample();
	} else {
		ret = _transport_sample;
	}

	assert (ret >= 0);

	if (!transport_rolling()) {
		return ret;
	}

#if 0 // TODO looping
	if (_transport_speed > 0.0f) {
		if (play_loop && have_looped) {
			/* the play-position wrapped at the loop-point
			 * ardour is already playing the beginning of the loop,
			 * but due to playback latency, the "audible frame"
			 * is still at the end of the loop.
			 */
			Location *location = _locations->auto_loop_location();
			sampleoffset_t lo = location->start() - ret;
			if (lo > 0) {
				ret = location->end () - lo;
				if (latent_locate) {
					*latent_locate = true;
				}
			}
		}
	} else if (_transport_speed < 0.0f) {
		/* XXX wot? no backward looping? */
	}
#endif

	return std::max ((samplepos_t)0, ret);
}

samplecnt_t
Session::preroll_samples (samplepos_t pos) const
{
	const float pr = Config->get_preroll_seconds();
	if (pos >= 0 && pr < 0) {
		const Tempo& tempo = _tempo_map->tempo_at_sample (pos);
		const Meter& meter = _tempo_map->meter_at_sample (pos);
		return meter.samples_per_bar (tempo, sample_rate()) * -pr;
	}
	if (pr < 0) {
		return 0;
	}
	return pr * sample_rate();
}

void
Session::set_sample_rate (samplecnt_t frames_per_second)
{
	/** \fn void Session::set_sample_size(samplecnt_t)
		the AudioEngine object that calls this guarantees
		that it will not be called while we are also in
		::process(). Its fine to do things that block
		here.
	*/

	if (_base_sample_rate == 0) {
		_base_sample_rate = frames_per_second;
	}
	else if (_base_sample_rate != frames_per_second && frames_per_second != _nominal_sample_rate) {
		NotifyAboutSampleRateMismatch (_base_sample_rate, frames_per_second);
	}
	_nominal_sample_rate = frames_per_second;

	sync_time_vars();

	clear_clicks ();
	reset_write_sources (false);

	DiskReader::alloc_loop_declick (nominal_sample_rate());
	Location* loc = _locations->auto_loop_location ();
	DiskReader::reset_loop_declick (loc, nominal_sample_rate());

	// XXX we need some equivalent to this, somehow
	// SndFileSource::setup_standard_crossfades (frames_per_second);

	set_dirty();

	/* XXX need to reset/reinstantiate all LADSPA plugins */
}

void
Session::set_block_size (pframes_t nframes)
{
	/* the AudioEngine guarantees
	   that it will not be called while we are also in
	   ::process(). It is therefore fine to do things that block
	   here.
	*/

	{
		current_block_size = nframes;

		ensure_buffers ();

		boost::shared_ptr<RouteList> r = routes.reader ();

		for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
			(*i)->set_block_size (nframes);
		}

		boost::shared_ptr<RouteList> rl = routes.reader ();
		for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
			boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);
			if (tr) {
				tr->set_block_size (nframes);
			}
		}

		set_worst_io_latencies ();
	}
}


static void
trace_terminal (boost::shared_ptr<Route> r1, boost::shared_ptr<Route> rbase, bool sends_only)
{
	boost::shared_ptr<Route> r2;

	if (r1->feeds (rbase) && rbase->feeds (r1)) {
		info << string_compose(_("feedback loop setup between %1 and %2"), r1->name(), rbase->name()) << endmsg;
		return;
	}

	/* make a copy of the existing list of routes that feed r1 */

	Route::FedBy existing (r1->fed_by());

	/* for each route that feeds r1, recurse, marking it as feeding
	   rbase as well.
	*/

	for (Route::FedBy::iterator i = existing.begin(); i != existing.end(); ++i) {
		if (!(r2 = i->r.lock ())) {
			/* (*i) went away, ignore it */
			continue;
		}

		/* r2 is a route that feeds r1 which somehow feeds base. mark
		   base as being fed by r2
		*/

		rbase->add_fed_by (r2, i->sends_only || sends_only);

		if (r2 != rbase) {

			/* 2nd level feedback loop detection. if r1 feeds or is fed by r2,
			   stop here.
			*/

			if (r1->feeds (r2) && r2->feeds (r1)) {
				continue;
			}

			/* now recurse, so that we can mark base as being fed by
			   all routes that feed r2
			*/

			trace_terminal (r2, rbase, i->sends_only || sends_only);
		}

	}
}

void
Session::resort_routes ()
{
	/* don't do anything here with signals emitted
	   by Routes during initial setup or while we
	   are being destroyed.
	*/

	if (inital_connect_or_deletion_in_progress ()) {
		return;
	}

	if (_route_deletion_in_progress) {
		return;
	}

	{
		RCUWriter<RouteList> writer (routes);
		boost::shared_ptr<RouteList> r = writer.get_copy ();
		resort_routes_using (r);
		/* writer goes out of scope and forces update */
	}

#ifndef NDEBUG
	if (DEBUG_ENABLED(DEBUG::Graph)) {
		boost::shared_ptr<RouteList> rl = routes.reader ();
		for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
			DEBUG_TRACE (DEBUG::Graph, string_compose ("%1 fed by ...\n", (*i)->name()));

			const Route::FedBy& fb ((*i)->fed_by());

			for (Route::FedBy::const_iterator f = fb.begin(); f != fb.end(); ++f) {
				boost::shared_ptr<Route> sf = f->r.lock();
				if (sf) {
					DEBUG_TRACE (DEBUG::Graph, string_compose ("\t%1 (sends only ? %2)\n", sf->name(), f->sends_only));
				}
			}
		}
	}
#endif

}

/** This is called whenever we need to rebuild the graph of how we will process
 *  routes.
 *  @param r List of routes, in any order.
 */

void
Session::resort_routes_using (boost::shared_ptr<RouteList> r)
{
	/* We are going to build a directed graph of our routes;
	   this is where the edges of that graph are put.
	*/

	GraphEdges edges;

	/* Go through all routes doing two things:
	 *
	 * 1. Collect the edges of the route graph.  Each of these edges
	 *    is a pair of routes, one of which directly feeds the other
	 *    either by a JACK connection or by an internal send.
	 *
	 * 2. Begin the process of making routes aware of which other
	 *    routes directly or indirectly feed them.  This information
	 *    is used by the solo code.
	 */

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {

		/* Clear out the route's list of direct or indirect feeds */
		(*i)->clear_fed_by ();

		for (RouteList::iterator j = r->begin(); j != r->end(); ++j) {

			bool via_sends_only = false;

			/* See if this *j feeds *i according to the current state of the JACK
			   connections and internal sends.
			*/
			if ((*j)->direct_feeds_according_to_reality (*i, &via_sends_only)) {
				/* add the edge to the graph (part #1) */
				edges.add (*j, *i, via_sends_only);
				/* tell the route (for part #2) */
				(*i)->add_fed_by (*j, via_sends_only);
			}
		}
	}

	/* Attempt a topological sort of the route graph */
	boost::shared_ptr<RouteList> sorted_routes = topological_sort (r, edges);

	if (sorted_routes) {
		/* We got a satisfactory topological sort, so there is no feedback;
		   use this new graph.

		   Note: the process graph rechain does not require a
		   topologically-sorted list, but hey ho.
		*/
		if (_process_graph) {
			_process_graph->rechain (sorted_routes, edges);
		}

		_current_route_graph = edges;

		/* Complete the building of the routes' lists of what directly
		   or indirectly feeds them.
		*/
		for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
			trace_terminal (*i, *i, false);
		}

		*r = *sorted_routes;

#ifndef NDEBUG
		DEBUG_TRACE (DEBUG::Graph, "Routes resorted, order follows:\n");
		for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
			DEBUG_TRACE (DEBUG::Graph, string_compose ("\t%1 presentation order %2\n", (*i)->name(), (*i)->presentation_info().order()));
		}
#endif

		SuccessfulGraphSort (); /* EMIT SIGNAL */

	} else {
		/* The topological sort failed, so we have a problem.  Tell everyone
		   and stick to the old graph; this will continue to be processed, so
		   until the feedback is fixed, what is played back will not quite
		   reflect what is actually connected.  Note also that we do not
		   do trace_terminal here, as it would fail due to an endless recursion,
		   so the solo code will think that everything is still connected
		   as it was before.
		*/

		FeedbackDetected (); /* EMIT SIGNAL */
	}

}

/** Find a route name starting with \a base, maybe followed by the
 *  lowest \a id.  \a id will always be added if \a definitely_add_number
 *  is true on entry; otherwise it will only be added if required
 *  to make the name unique.
 *
 *  Names are constructed like e.g. "Audio 3" for base="Audio" and id=3.
 *  The available route name with the lowest ID will be used, and \a id
 *  will be set to the ID.
 *
 *  \return false if a route name could not be found, and \a track_name
 *  and \a id do not reflect a free route name.
 */
bool
Session::find_route_name (string const & base, uint32_t& id, string& name, bool definitely_add_number)
{
	/* the base may conflict with ports that do not belong to existing
	   routes, but hidden objects like the click track. So check port names
	   before anything else.
	*/

	for (map<string,bool>::const_iterator reserved = reserved_io_names.begin(); reserved != reserved_io_names.end(); ++reserved) {
		if (base == reserved->first) {
			/* Check if this reserved name already exists, and if
			   so, disallow it without a numeric suffix.
			*/
			if (!reserved->second || route_by_name (reserved->first)) {
				definitely_add_number = true;
				if (id < 1) {
					id = 1;
				}
			}
			break;
		}
	}

	/* if we have "base 1" already, it doesn't make sense to add "base"
	 * if "base 1" has been deleted, adding "base" is no worse than "base 1"
	 */
	if (!definitely_add_number && route_by_name (base) == 0 && (route_by_name (string_compose("%1 1", base)) == 0)) {
		/* just use the base */
		name = base;
		return true;
	}

	do {
		name = string_compose ("%1 %2", base, id);

		if (route_by_name (name) == 0) {
			return true;
		}

		++id;

	} while (id < (UINT_MAX-1));

	return false;
}

/** Count the total ins and outs of all non-hidden tracks in the session and return them in in and out */
void
Session::count_existing_track_channels (ChanCount& in, ChanCount& out)
{
	in  = ChanCount::ZERO;
	out = ChanCount::ZERO;

	boost::shared_ptr<RouteList> r = routes.reader ();

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);
		if (!tr) {
			continue;
		}
		assert (!tr->is_auditioner()); // XXX remove me
		in  += tr->n_inputs();
		out += tr->n_outputs();
	}
}

string
Session::default_track_name_pattern (DataType t)
{
	switch (t) {
	case DataType::AUDIO:
		return _("Audio ");
		break;

	case DataType::MIDI:
		return _("MIDI ");
	}

	return "";
}

/** Caller must not hold process lock
 *  @param name_template string to use for the start of the name, or "" to use "MIDI".
 *  @param instrument plugin info for the instrument to insert pre-fader, if any
 */
list<boost::shared_ptr<MidiTrack> >
Session::new_midi_track (const ChanCount& input, const ChanCount& output, bool strict_io,
                         boost::shared_ptr<PluginInfo> instrument, Plugin::PresetRecord* pset,
                         RouteGroup* route_group, uint32_t how_many,
                         string name_template, PresentationInfo::order_t order,
                         TrackMode mode)
{
	string track_name;
	uint32_t track_id = 0;
	string port;
	RouteList new_routes;
	list<boost::shared_ptr<MidiTrack> > ret;

	const string name_pattern = default_track_name_pattern (DataType::MIDI);
	bool const use_number = (how_many != 1) || name_template.empty () || (name_template == name_pattern);

	while (how_many) {
		if (!find_route_name (name_template.empty() ? _("MIDI") : name_template, ++track_id, track_name, use_number)) {
			error << "cannot find name for new midi track" << endmsg;
			goto failed;
		}

		boost::shared_ptr<MidiTrack> track;

		try {
			track.reset (new MidiTrack (*this, track_name, mode));

			if (track->init ()) {
				goto failed;
			}

			if (strict_io) {
				track->set_strict_io (true);
			}

			BOOST_MARK_TRACK (track);

			{
				Glib::Threads::Mutex::Lock lm (AudioEngine::instance()->process_lock ());
				if (track->input()->ensure_io (input, false, this)) {
					error << "cannot configure " << input << " out configuration for new midi track" << endmsg;
					goto failed;
				}

				if (track->output()->ensure_io (output, false, this)) {
					error << "cannot configure " << output << " out configuration for new midi track" << endmsg;
					goto failed;
				}
			}

			if (route_group) {
				route_group->add (track);
			}

			new_routes.push_back (track);
			ret.push_back (track);
		}

		catch (failed_constructor &err) {
			error << _("Session: could not create new midi track.") << endmsg;
			goto failed;
		}

		catch (AudioEngine::PortRegistrationFailure& pfe) {

			error << string_compose (_("No more JACK ports are available. You will need to stop %1 and restart JACK with more ports if you need this many tracks."), PROGRAM_NAME) << endmsg;
			goto failed;
		}

		--how_many;
	}

	failed:
	if (!new_routes.empty()) {
		StateProtector sp (this);

		if (instrument) {
			for (RouteList::iterator r = new_routes.begin(); r != new_routes.end(); ++r) {
				PluginPtr plugin = instrument->load (*this);
				if (!plugin) {
					warning << "Failed to add Synth Plugin to newly created track." << endmsg;
					continue;
				}
				if (pset) {
					plugin->load_preset (*pset);
				}
				boost::shared_ptr<PluginInsert> pi (new PluginInsert (*this, plugin));
				if (strict_io) {
					pi->set_strict_io (true);
				}

				(*r)->add_processor (pi, PreFader);

				if (Profile->get_mixbus () && pi->configured () && pi->output_streams().n_audio() > 2) {
					(*r)->move_instrument_down (false);
				}
			}
		}

		add_routes (new_routes, true, true, false, order);
	}

	return ret;
}

RouteList
Session::new_midi_route (RouteGroup* route_group, uint32_t how_many, string name_template, bool strict_io,
                         boost::shared_ptr<PluginInfo> instrument, Plugin::PresetRecord* pset,
                         PresentationInfo::Flag flag, PresentationInfo::order_t order)
{
	string bus_name;
	uint32_t bus_id = 0;
	string port;
	RouteList ret;

	bool const use_number = (how_many != 1) || name_template.empty () || name_template == _("Midi Bus");

	while (how_many) {
		if (!find_route_name (name_template.empty () ? _("Midi Bus") : name_template, ++bus_id, bus_name, use_number)) {
			error << "cannot find name for new midi bus" << endmsg;
			goto failure;
		}

		try {
			boost::shared_ptr<Route> bus (new Route (*this, bus_name, flag, DataType::AUDIO)); // XXX Editor::add_routes is not ready for ARDOUR::DataType::MIDI

			if (bus->init ()) {
				goto failure;
			}

			if (strict_io) {
				bus->set_strict_io (true);
			}

			BOOST_MARK_ROUTE(bus);

			{
				Glib::Threads::Mutex::Lock lm (AudioEngine::instance()->process_lock ());

				if (bus->input()->ensure_io (ChanCount(DataType::MIDI, 1), false, this)) {
					error << _("cannot configure new midi bus input") << endmsg;
					goto failure;
				}


				if (bus->output()->ensure_io (ChanCount(DataType::MIDI, 1), false, this)) {
					error << _("cannot configure new midi bus output") << endmsg;
					goto failure;
				}
			}

			if (route_group) {
				route_group->add (bus);
			}

			bus->add_internal_return ();
			ret.push_back (bus);
		}

		catch (failed_constructor &err) {
			error << _("Session: could not create new audio route.") << endmsg;
			goto failure;
		}

		catch (AudioEngine::PortRegistrationFailure& pfe) {
			error << pfe.what() << endmsg;
			goto failure;
		}


		--how_many;
	}

	failure:
	if (!ret.empty()) {
		StateProtector sp (this);
		add_routes (ret, false, false, false, order);

		if (instrument) {
			for (RouteList::iterator r = ret.begin(); r != ret.end(); ++r) {
				PluginPtr plugin = instrument->load (*this);
				if (!plugin) {
					warning << "Failed to add Synth Plugin to newly created track." << endmsg;
					continue;
				}
				if (pset) {
					plugin->load_preset (*pset);
				}
				boost::shared_ptr<PluginInsert> pi (new PluginInsert (*this, plugin));
				if (strict_io) {
					pi->set_strict_io (true);
				}

				(*r)->add_processor (pi, PreFader);

				if (Profile->get_mixbus () && pi->configured () && pi->output_streams().n_audio() > 2) {
					(*r)->move_instrument_down (false);
				}
			}
		}
	}

	return ret;

}


void
Session::midi_output_change_handler (IOChange change, void * /*src*/, boost::weak_ptr<Route> wmt)
{
	boost::shared_ptr<Route> midi_track (wmt.lock());

	if (!midi_track) {
		return;
	}

	if ((change.type & IOChange::ConfigurationChanged) && Config->get_output_auto_connect() != ManualConnect) {

		if (change.after.n_audio() <= change.before.n_audio()) {
			return;
		}

		/* new audio ports: make sure the audio goes somewhere useful,
		 * unless the user has no-auto-connect selected.
		 *
		 * The existing ChanCounts don't matter for this call as they are only
		 * to do with matching input and output indices, and we are only changing
		 * outputs here.
		 */
		auto_connect_route (midi_track, false, ChanCount(), change.before);
	}
}

bool
Session::ensure_stripable_sort_order ()
{
	StripableList sl;
	get_stripables (sl);
	sl.sort (Stripable::Sorter ());

	bool change = false;
	PresentationInfo::order_t order = 0;

	for (StripableList::iterator si = sl.begin(); si != sl.end(); ++si) {
		boost::shared_ptr<Stripable> s (*si);
		assert (!s->is_auditioner ()); // XXX remove me
		if (s->is_monitor ()) {
			continue;
		}
		if (order != s->presentation_info().order()) {
			s->set_presentation_order (order);
			change = true;
		}
		++order;
	}
	return change;
}

void
Session::ensure_route_presentation_info_gap (PresentationInfo::order_t first_new_order, uint32_t how_many)
{
	DEBUG_TRACE (DEBUG::OrderKeys, string_compose ("ensure order gap starting at %1 for %2\n", first_new_order, how_many));

	if (first_new_order == PresentationInfo::max_order) {
		/* adding at end, no worries */
		return;
	}

	/* create a gap in the presentation info to accomodate @param how_many
	 * new objects.
	 */
	StripableList sl;
	get_stripables (sl);

	for (StripableList::iterator si = sl.begin(); si != sl.end(); ++si) {
		boost::shared_ptr<Stripable> s (*si);

		if (s->presentation_info().special (false)) {
			continue;
		}

		if (!s->presentation_info().order_set()) {
			continue;
		}

		if (s->presentation_info().order () >= first_new_order) {
			s->set_presentation_order (s->presentation_info().order () + how_many);
		}
	}
}

/** Caller must not hold process lock
 *  @param name_template string to use for the start of the name, or "" to use "Audio".
 */
list< boost::shared_ptr<AudioTrack> >
Session::new_audio_track (int input_channels, int output_channels, RouteGroup* route_group,
                          uint32_t how_many, string name_template, PresentationInfo::order_t order,
                          TrackMode mode)
{
	string track_name;
	uint32_t track_id = 0;
	string port;
	RouteList new_routes;
	list<boost::shared_ptr<AudioTrack> > ret;

	const string name_pattern = default_track_name_pattern (DataType::AUDIO);
	bool const use_number = (how_many != 1) || name_template.empty () || (name_template == name_pattern);

	while (how_many) {

		if (!find_route_name (name_template.empty() ? _(name_pattern.c_str()) : name_template, ++track_id, track_name, use_number)) {
			error << "cannot find name for new audio track" << endmsg;
			goto failed;
		}

		boost::shared_ptr<AudioTrack> track;

		try {
			track.reset (new AudioTrack (*this, track_name, mode));

			if (track->init ()) {
				goto failed;
			}

			if (Profile->get_mixbus ()) {
				track->set_strict_io (true);
			}

			BOOST_MARK_TRACK (track);

			{
				Glib::Threads::Mutex::Lock lm (AudioEngine::instance()->process_lock ());

				if (track->input()->ensure_io (ChanCount(DataType::AUDIO, input_channels), false, this)) {
					error << string_compose (
						_("cannot configure %1 in/%2 out configuration for new audio track"),
						input_channels, output_channels)
					      << endmsg;
					goto failed;
				}

				if (track->output()->ensure_io (ChanCount(DataType::AUDIO, output_channels), false, this)) {
					error << string_compose (
						_("cannot configure %1 in/%2 out configuration for new audio track"),
						input_channels, output_channels)
					      << endmsg;
					goto failed;
				}
			}

			if (route_group) {
				route_group->add (track);
			}

			new_routes.push_back (track);
			ret.push_back (track);
		}

		catch (failed_constructor &err) {
			error << _("Session: could not create new audio track.") << endmsg;
			goto failed;
		}

		catch (AudioEngine::PortRegistrationFailure& pfe) {

			error << pfe.what() << endmsg;
			goto failed;
		}

		--how_many;
	}

	failed:
	if (!new_routes.empty()) {
		StateProtector sp (this);
		add_routes (new_routes, true, true, false, order);
	}

	return ret;
}

/** Caller must not hold process lock.
 *  @param name_template string to use for the start of the name, or "" to use "Bus".
 */
RouteList
Session::new_audio_route (int input_channels, int output_channels, RouteGroup* route_group, uint32_t how_many, string name_template,
                          PresentationInfo::Flag flags, PresentationInfo::order_t order)
{
	string bus_name;
	uint32_t bus_id = 0;
	string port;
	RouteList ret;

	bool const use_number = (how_many != 1) || name_template.empty () || name_template == _("Bus");

	while (how_many) {
		if (!find_route_name (name_template.empty () ? _("Bus") : name_template, ++bus_id, bus_name, use_number)) {
			error << "cannot find name for new audio bus" << endmsg;
			goto failure;
		}

		try {
			boost::shared_ptr<Route> bus (new Route (*this, bus_name, flags, DataType::AUDIO));

			if (bus->init ()) {
				goto failure;
			}

			if (Profile->get_mixbus ()) {
				bus->set_strict_io (true);
			}

			BOOST_MARK_ROUTE(bus);

			{
				Glib::Threads::Mutex::Lock lm (AudioEngine::instance()->process_lock ());

				if (bus->input()->ensure_io (ChanCount(DataType::AUDIO, input_channels), false, this)) {
					error << string_compose (_("cannot configure %1 in/%2 out configuration for new audio track"),
								 input_channels, output_channels)
					      << endmsg;
					goto failure;
				}


				if (bus->output()->ensure_io (ChanCount(DataType::AUDIO, output_channels), false, this)) {
					error << string_compose (_("cannot configure %1 in/%2 out configuration for new audio track"),
								 input_channels, output_channels)
					      << endmsg;
					goto failure;
				}
			}

			if (route_group) {
				route_group->add (bus);
			}

			bus->add_internal_return ();
			ret.push_back (bus);
		}

		catch (failed_constructor &err) {
			error << _("Session: could not create new audio route.") << endmsg;
			goto failure;
		}

		catch (AudioEngine::PortRegistrationFailure& pfe) {
			error << pfe.what() << endmsg;
			goto failure;
		}


		--how_many;
	}

	failure:
	if (!ret.empty()) {
		StateProtector sp (this);

		if (flags == PresentationInfo::FoldbackBus) {
			add_routes (ret, false, false, true, order); // no autoconnect
		} else {
			add_routes (ret, false, true, true, order); // autoconnect // outputs only
		}
	}

	return ret;

}

RouteList
Session::new_route_from_template (uint32_t how_many, PresentationInfo::order_t insert_at, const std::string& template_path, const std::string& name_base,
                                  PlaylistDisposition pd)
{
	XMLTree tree;

	if (!tree.read (template_path.c_str())) {
		return RouteList();
	}

	return new_route_from_template (how_many, insert_at, *tree.root(), name_base, pd);
}

RouteList
Session::new_route_from_template (uint32_t how_many, PresentationInfo::order_t insert_at, XMLNode& node, const std::string& name_base, PlaylistDisposition pd)
{
	RouteList ret;
	uint32_t number = 0;
	const uint32_t being_added = how_many;
	/* This will prevent the use of any existing XML-provided PBD::ID
	   values by Stateful.
	*/
	Stateful::ForceIDRegeneration force_ids;
	IO::disable_connecting ();

	while (how_many) {

		/* We're going to modify the node contents a bit so take a
		 * copy. The node may be re-used when duplicating more than once.
		 */

		XMLNode node_copy (node);
		std::vector<boost::shared_ptr<Playlist> > shared_playlists;

		try {
			string name;

			if (!name_base.empty()) {

				/* if we're adding more than one routes, force
				 * all the names of the new routes to be
				 * numbered, via the final parameter.
				 */

				if (!find_route_name (name_base.c_str(), ++number, name, (being_added > 1))) {
					fatal << _("Session: UINT_MAX routes? impossible!") << endmsg;
					abort(); /*NOTREACHED*/
				}

			} else {

				string const route_name  = node_copy.property(X_("name"))->value ();

				/* generate a new name by adding a number to the end of the template name */
				if (!find_route_name (route_name.c_str(), ++number, name, true)) {
					fatal << _("Session: UINT_MAX routes? impossible!") << endmsg;
					abort(); /*NOTREACHED*/
				}
			}

			/* figure out the appropriate playlist setup. The track
			 * (if the Route we're creating is a track) will find
			 * playlists via ID.
			 */

			if (pd == CopyPlaylist) {

				PBD::ID playlist_id;

				if (node_copy.get_property (X_("audio-playlist"), playlist_id)) {
					boost::shared_ptr<Playlist> playlist = _playlists->by_id (playlist_id);
					playlist = PlaylistFactory::create (playlist, string_compose ("%1.1", name));
					playlist->reset_shares ();
					node_copy.set_property (X_("audio-playlist"), playlist->id());
				}

				if (node_copy.get_property (X_("midi-playlist"), playlist_id)) {
					boost::shared_ptr<Playlist> playlist = _playlists->by_id (playlist_id);
					playlist = PlaylistFactory::create (playlist, string_compose ("%1.1", name));
					playlist->reset_shares ();
					node_copy.set_property (X_("midi-playlist"), playlist->id());
				}

			} else if (pd == SharePlaylist) {
				PBD::ID playlist_id;

				if (node_copy.get_property (X_("audio-playlist"), playlist_id)) {
					boost::shared_ptr<Playlist> playlist = _playlists->by_id (playlist_id);
					shared_playlists.push_back (playlist);
				}

				if (node_copy.get_property (X_("midi-playlist"), playlist_id)) {
					boost::shared_ptr<Playlist> playlist = _playlists->by_id (playlist_id);
					shared_playlists.push_back (playlist);
				}

			} else { /* NewPlaylist */

				PBD::ID pid;

				if (node_copy.get_property (X_("audio-playlist"), pid)) {
					boost::shared_ptr<Playlist> playlist = PlaylistFactory::create (DataType::AUDIO, *this, name, false);
					node_copy.set_property (X_("audio-playlist"), playlist->id());
				}

				if (node_copy.get_property (X_("midi-playlist"), pid)) {
					boost::shared_ptr<Playlist> playlist = PlaylistFactory::create (DataType::MIDI, *this, name, false);
					node_copy.set_property (X_("midi-playlist"), playlist->id());
				}
			}

			/* Fix up new name in the XML node */

			Route::set_name_in_state (node_copy, name);

			/* trim bitslots from listen sends so that new ones are used */
			XMLNodeList children = node_copy.children ();
			for (XMLNodeList::iterator i = children.begin(); i != children.end(); ++i) {
				if ((*i)->name() == X_("Processor")) {
					/* ForceIDRegeneration does not catch the following */
					XMLProperty const * role = (*i)->property (X_("role"));
					XMLProperty const * type = (*i)->property (X_("type"));
					if (role && role->value() == X_("Aux")) {
						/* check if the target bus exists.
						 * we should not save aux-sends in templates.
						 */
						XMLProperty const * target = (*i)->property (X_("target"));
						if (!target) {
							(*i)->set_property ("type", "dangling-aux-send");
							continue;
						}
						boost::shared_ptr<Route> r = route_by_id (target->value());
						if (!r || boost::dynamic_pointer_cast<Track>(r)) {
							(*i)->set_property ("type", "dangling-aux-send");
							continue;
						}
					}
					if (role && role->value() == X_("Listen")) {
						(*i)->remove_property (X_("bitslot"));
					}
					else if (role && (role->value() == X_("Send") || role->value() == X_("Aux"))) {
						Delivery::Role xrole;
						uint32_t bitslot = 0;
						xrole = Delivery::Role (string_2_enum (role->value(), xrole));
						std::string name = Send::name_and_id_new_send(*this, xrole, bitslot, false);
						(*i)->remove_property (X_("bitslot"));
						(*i)->remove_property (X_("name"));
						(*i)->set_property ("bitslot", bitslot);
						(*i)->set_property ("name", name);
						XMLNodeList io_kids = (*i)->children ();
						for (XMLNodeList::iterator j = io_kids.begin(); j != io_kids.end(); ++j) {
							if ((*j)->name() != X_("IO")) {
								continue;
							}
							(*j)->remove_property (X_("name"));
							(*j)->set_property ("name", name);
						}
					}
					else if (type && type->value() == X_("intreturn")) {
						(*i)->remove_property (X_("bitslot"));
						(*i)->set_property ("ignore-bitslot", "1");
					}
					else if (type && type->value() == X_("return")) {
						// Return::set_state() generates a new one
						(*i)->remove_property (X_("bitslot"));
					}
					else if (type && type->value() == X_("port")) {
						IOProcessor::prepare_for_reset (**i, name);
					}
				}
			}

			/* new routes start off unsoloed to avoid issues related to
			   upstream / downstream buses.
			*/
			node_copy.remove_node_and_delete (X_("Controllable"), X_("name"), X_("solo"));

			/* New v6 templates do have a version in the Route-Template,
			 * we assume that all older, unversioned templates are
			 * from Ardour 5.x
			 * when Stateful::loading_state_version was 3002
			 */
			int version = 3002;
			node.get_property (X_("version"), version);

			boost::shared_ptr<Route> route;

			if (version < 3000) {
				route = XMLRouteFactory_2X (node_copy, version);
			} else if (version < 5000) {
				route = XMLRouteFactory_3X (node_copy, version);
			} else {
				route = XMLRouteFactory (node_copy, version);
			}

			if (route == 0) {
				error << _("Session: cannot create track/bus from template description") << endmsg;
				goto out;
			}

			/* Fix up sharing of playlists with the new Route/Track */

			for (vector<boost::shared_ptr<Playlist> >::iterator sp = shared_playlists.begin(); sp != shared_playlists.end(); ++sp) {
				(*sp)->share_with (route->id());
			}

			if (boost::dynamic_pointer_cast<Track>(route)) {
				/* force input/output change signals so that the new diskstream
				   picks up the configuration of the route. During session
				   loading this normally happens in a different way.
				*/

				Glib::Threads::Mutex::Lock lm (AudioEngine::instance()->process_lock ());

				IOChange change (IOChange::Type (IOChange::ConfigurationChanged | IOChange::ConnectionsChanged));
				change.after = route->input()->n_ports();
				route->input()->changed (change, this);
				change.after = route->output()->n_ports();
				route->output()->changed (change, this);
			}

			ret.push_back (route);
		}

		catch (failed_constructor &err) {
			error << _("Session: could not create new route from template") << endmsg;
			goto out;
		}

		catch (AudioEngine::PortRegistrationFailure& pfe) {
			error << pfe.what() << endmsg;
			goto out;
		}

		catch (...) {
			IO::enable_connecting ();
			throw;
		}

		--how_many;
	}

	out:
	if (!ret.empty()) {
		StateProtector sp (this);

		add_routes (ret, true, true, false, insert_at);
	}

	IO::enable_connecting ();

	return ret;
}

void
Session::add_routes (RouteList& new_routes, bool input_auto_connect, bool output_auto_connect, bool save, PresentationInfo::order_t order)
{
	try {
		PBD::Unwinder<bool> aip (_adding_routes_in_progress, true);
		add_routes_inner (new_routes, input_auto_connect, output_auto_connect, order);

	} catch (...) {
		error << _("Adding new tracks/busses failed") << endmsg;
	}

	/* During the route additions there will have been potentially several
	 * signals emitted to indicate the new graph. ::graph_reordered() will
	 * have ignored all of them because _adding_routes_in_progress was
	 * true.
	 *
	 * We still need the effects of ::graph_reordered(), but we didn't want
	 * it called multiple times during the addition of multiple routes. Now
	 * that the addition is done, call it explicitly.
	 */

	graph_reordered (false);

	set_dirty();

	if (save) {
		save_state (_current_snapshot_name);
	}

	update_route_record_state ();

	RouteAdded (new_routes); /* EMIT SIGNAL */
}

void
Session::add_routes_inner (RouteList& new_routes, bool input_auto_connect, bool output_auto_connect, PresentationInfo::order_t order)
{
	ChanCount existing_inputs;
	ChanCount existing_outputs;
	uint32_t n_routes;
	uint32_t added = 0;

	count_existing_track_channels (existing_inputs, existing_outputs);

	{
		RCUWriter<RouteList> writer (routes);
		boost::shared_ptr<RouteList> r = writer.get_copy ();
		n_routes = r->size();
		r->insert (r->end(), new_routes.begin(), new_routes.end());

		/* if there is no control out and we're not in the middle of loading,
		 * resort the graph here. if there is a control out, we will resort
		 * toward the end of this method. if we are in the middle of loading,
		 * we will resort when done.
		 */

		if (!_monitor_out && IO::connecting_legal) {
			resort_routes_using (r);
		}
	}

	/* monitor is not part of the order */
	if (_monitor_out) {
		assert (n_routes > 0);
		--n_routes;
	}

	{
		PresentationInfo::ChangeSuspender cs;
		ensure_route_presentation_info_gap (order, new_routes.size());

		for (RouteList::iterator x = new_routes.begin(); x != new_routes.end(); ++x, ++added) {

			boost::weak_ptr<Route> wpr (*x);
			boost::shared_ptr<Route> r (*x);

			r->solo_control()->Changed.connect_same_thread (*this, boost::bind (&Session::route_solo_changed, this, _1, _2,wpr));
			r->solo_isolate_control()->Changed.connect_same_thread (*this, boost::bind (&Session::route_solo_isolated_changed, this, wpr));
			r->mute_control()->Changed.connect_same_thread (*this, boost::bind (&Session::route_mute_changed, this));

			r->output()->changed.connect_same_thread (*this, boost::bind (&Session::set_worst_io_latencies_x, this, _1, _2));
			r->processors_changed.connect_same_thread (*this, boost::bind (&Session::route_processors_changed, this, _1));
			r->processor_latency_changed.connect_same_thread (*this, boost::bind (&Session::queue_latency_recompute, this));

			if (r->is_master()) {
				_master_out = r;
			}

			if (r->is_monitor()) {
				_monitor_out = r;
			}

			boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (r);
			if (tr) {
				tr->PlaylistChanged.connect_same_thread (*this, boost::bind (&Session::track_playlist_changed, this, boost::weak_ptr<Track> (tr)));
				track_playlist_changed (boost::weak_ptr<Track> (tr));
				tr->rec_enable_control()->Changed.connect_same_thread (*this, boost::bind (&Session::update_route_record_state, this));

				boost::shared_ptr<MidiTrack> mt = boost::dynamic_pointer_cast<MidiTrack> (tr);
				if (mt) {
					mt->StepEditStatusChange.connect_same_thread (*this, boost::bind (&Session::step_edit_status_change, this, _1));
					mt->output()->changed.connect_same_thread (*this, boost::bind (&Session::midi_output_change_handler, this, _1, _2, boost::weak_ptr<Route>(mt)));
					mt->presentation_info().PropertyChanged.connect_same_thread (*this, boost::bind (&Session::midi_track_presentation_info_changed, this, _1, boost::weak_ptr<MidiTrack>(mt)));
				}
			}

			if (!r->presentation_info().special (false)) {

				DEBUG_TRACE (DEBUG::OrderKeys, string_compose ("checking PI state for %1\n", r->name()));

				/* presentation info order may already have been set from XML */

				if (!r->presentation_info().order_set()) {
					if (order == PresentationInfo::max_order) {
						/* just add to the end */
						r->set_presentation_order (n_routes + added);
						DEBUG_TRACE (DEBUG::OrderKeys, string_compose ("group order not set, set to NR %1 + %2 = %3\n", n_routes, added, n_routes + added));
					} else {
						r->set_presentation_order (order + added);
						DEBUG_TRACE (DEBUG::OrderKeys, string_compose ("group order not set, set to %1 + %2 = %3\n", order, added, order + added));
					}
				} else {
					DEBUG_TRACE (DEBUG::OrderKeys, string_compose ("group order already set to %1\n", r->presentation_info().order()));
				}
			}

#if !defined(__APPLE__) && !defined(__FreeBSD__)
			/* clang complains: 'operator<<' should be declared prior to the call site or in an associated namespace of one of its
			 * arguments std::ostream& operator<<(std::ostream& o, ARDOUR::PresentationInfo const& rid)"
			 */
			DEBUG_TRACE (DEBUG::OrderKeys, string_compose ("added route %1, group order %2 type %3 (summary: %4)\n",
			                                               r->name(),
			                                               r->presentation_info().order(),
			                                               enum_2_string (r->presentation_info().flags()),
			                                               r->presentation_info()));
#endif


			if (input_auto_connect || output_auto_connect) {
				auto_connect_route (r, input_auto_connect, ChanCount (), ChanCount (), existing_inputs, existing_outputs);
				existing_inputs += r->n_inputs();
				existing_outputs += r->n_outputs();
			}

			ARDOUR::GUIIdle ();
		}
		ensure_stripable_sort_order ();
	}

	if (_monitor_out && IO::connecting_legal) {
		Glib::Threads::Mutex::Lock lm (_engine.process_lock());

		for (RouteList::iterator x = new_routes.begin(); x != new_routes.end(); ++x) {
			if ((*x)->is_monitor()) {
				/* relax */
			} else if ((*x)->is_master()) {
				/* relax */
			} else {
				(*x)->enable_monitor_send ();
			}
		}
	}

	reassign_track_numbers ();
}

void
Session::globally_set_send_gains_to_zero (boost::shared_ptr<Route> dest)
{
	boost::shared_ptr<RouteList> r = routes.reader ();
	boost::shared_ptr<Send> s;

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		if ((s = (*i)->internal_send_for (dest)) != 0) {
			s->amp()->gain_control()->set_value (GAIN_COEFF_ZERO, Controllable::NoGroup);
		}
	}
}

void
Session::globally_set_send_gains_to_unity (boost::shared_ptr<Route> dest)
{
	boost::shared_ptr<RouteList> r = routes.reader ();
	boost::shared_ptr<Send> s;

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		if ((s = (*i)->internal_send_for (dest)) != 0) {
			s->amp()->gain_control()->set_value (GAIN_COEFF_UNITY, Controllable::NoGroup);
		}
	}
}

void
Session::globally_set_send_gains_from_track(boost::shared_ptr<Route> dest)
{
	boost::shared_ptr<RouteList> r = routes.reader ();
	boost::shared_ptr<Send> s;

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		if ((s = (*i)->internal_send_for (dest)) != 0) {
			s->amp()->gain_control()->set_value ((*i)->gain_control()->get_value(), Controllable::NoGroup);
		}
	}
}

/** @param include_buses true to add sends to buses and tracks, false for just tracks */
void
Session::globally_add_internal_sends (boost::shared_ptr<Route> dest, Placement p, bool include_buses)
{
	boost::shared_ptr<RouteList> r = routes.reader ();
	boost::shared_ptr<RouteList> t (new RouteList);

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		/* no MIDI sends because there are no MIDI busses yet */
		if (include_buses || boost::dynamic_pointer_cast<AudioTrack>(*i)) {
			t->push_back (*i);
		}
	}

	add_internal_sends (dest, p, t);
}

void
Session::add_internal_sends (boost::shared_ptr<Route> dest, Placement p, boost::shared_ptr<RouteList> senders)
{
	for (RouteList::iterator i = senders->begin(); i != senders->end(); ++i) {
		add_internal_send (dest, (*i)->before_processor_for_placement (p), *i);
	}
}

void
Session::add_internal_send (boost::shared_ptr<Route> dest, int index, boost::shared_ptr<Route> sender)
{
	add_internal_send (dest, sender->before_processor_for_index (index), sender);
}

void
Session::add_internal_send (boost::shared_ptr<Route> dest, boost::shared_ptr<Processor> before, boost::shared_ptr<Route> sender)
{
	if (sender->is_monitor() || sender->is_master() || sender == dest || dest->is_monitor() || dest->is_master()) {
		return;
	}

	if (!dest->internal_return()) {
		dest->add_internal_return ();
	}

	sender->add_aux_send (dest, before);

	graph_reordered (false);
}

void
Session::remove_routes (boost::shared_ptr<RouteList> routes_to_remove)
{
	bool mute_changed = false;
	bool send_selected = false;

	{ // RCU Writer scope
		PBD::Unwinder<bool> uw_flag (_route_deletion_in_progress, true);
		RCUWriter<RouteList> writer (routes);
		boost::shared_ptr<RouteList> rs = writer.get_copy ();

		for (RouteList::iterator iter = routes_to_remove->begin(); iter != routes_to_remove->end(); ++iter) {

			if (_selection->selected (*iter)) {
				send_selected = true;
			}

			if (*iter == _master_out) {
				continue;
			}

			/* speed up session deletion, don't do the solo dance */
			if (!deletion_in_progress ()) {
				(*iter)->solo_control()->set_value (0.0, Controllable::NoGroup);
			}

			if ((*iter)->mute_control()->muted ()) {
				mute_changed = true;
			}

			rs->remove (*iter);

			/* deleting the master out seems like a dumb
			   idea, but its more of a UI policy issue
			   than our concern.
			*/

			if (*iter == _master_out) {
				_master_out = boost::shared_ptr<Route> ();
			}

			if (*iter == _monitor_out) {
				_monitor_out.reset ();
			}

			// We need to disconnect the route's inputs and outputs

			(*iter)->input()->disconnect (0);
			(*iter)->output()->disconnect (0);

			/* if the route had internal sends sending to it, remove them */

			if (!deletion_in_progress () && (*iter)->internal_return()) {

				boost::shared_ptr<RouteList> r = routes.reader ();
				for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
					boost::shared_ptr<Send> s = (*i)->internal_send_for (*iter);
					if (s) {
						(*i)->remove_processor (s);
					}
				}
			}

			/* if the monitoring section had a pointer to this route, remove it */
			if (_monitor_out && !(*iter)->is_master() && !(*iter)->is_monitor()) {
				Glib::Threads::Mutex::Lock lm (AudioEngine::instance()->process_lock ());
				ProcessorChangeBlocker pcb (this, false);
				(*iter)->remove_aux_or_listen (_monitor_out);
			}

			boost::shared_ptr<MidiTrack> mt = boost::dynamic_pointer_cast<MidiTrack> (*iter);
			if (mt && mt->step_editing()) {
				if (_step_editors > 0) {
					_step_editors--;
				}
			}
		}

		/* writer goes out of scope, forces route list update */

	} // end of RCU Writer scope

	if (mute_changed) {
		MuteChanged (); /* EMIT SIGNAL */
	}

	update_route_solo_state ();
	update_latency_compensation (false, false);
	set_dirty();

	/* Re-sort routes to remove the graph's current references to the one that is
	 * going away, then flush old references out of the graph.
	 */

	routes.flush (); // maybe unsafe, see below.
	resort_routes ();

	if (_process_graph && !deletion_in_progress() && _engine.running()) {
		_process_graph->clear_other_chain ();
	}

	/* get rid of it from the dead wood collection in the route list manager */
	/* XXX i think this is unsafe as it currently stands, but i am not sure. (pd, october 2nd, 2006) */

	routes.flush ();

	/* remove these routes from the selection if appropriate, and signal
	 * the change *before* we call DropReferences for them.
	 */

	if (send_selected && !deletion_in_progress()) {
		for (RouteList::iterator iter = routes_to_remove->begin(); iter != routes_to_remove->end(); ++iter) {
			_selection->remove_stripable_by_id ((*iter)->id());
		}
		PropertyChange pc;
		pc.add (Properties::selected);
		PresentationInfo::Change (pc);
	}

	/* try to cause everyone to drop their references
	 * and unregister ports from the backend
	 */

	for (RouteList::iterator iter = routes_to_remove->begin(); iter != routes_to_remove->end(); ++iter) {
		(*iter)->drop_references ();
	}

	if (deletion_in_progress()) {
		return;
	}

	PropertyChange pc;
	pc.add (Properties::order);
	PresentationInfo::Change (pc);

	/* save the new state of the world */

	if (save_state (_current_snapshot_name)) {
		save_history (_current_snapshot_name);
	}

	update_route_record_state ();
}

void
Session::remove_route (boost::shared_ptr<Route> route)
{
	boost::shared_ptr<RouteList> rl (new RouteList);
	rl->push_back (route);
	remove_routes (rl);
}

void
Session::route_mute_changed ()
{
	MuteChanged (); /* EMIT SIGNAL */
	set_dirty ();
}

void
Session::route_listen_changed (Controllable::GroupControlDisposition group_override, boost::weak_ptr<Route> wpr)
{
	boost::shared_ptr<Route> route (wpr.lock());

	if (!route) {
		return;
	}

	assert (Config->get_solo_control_is_listen_control());

	if (route->solo_control()->soloed_by_self_or_masters()) {

		if (Config->get_exclusive_solo()) {

			RouteGroup* rg = route->route_group ();
			const bool group_already_accounted_for = (group_override == Controllable::ForGroup);

			boost::shared_ptr<RouteList> r = routes.reader ();

			for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
				if ((*i) == route) {
					/* already changed */
					continue;
				}

				if ((*i)->solo_isolate_control()->solo_isolated() || !(*i)->can_solo()) {
					/* route does not get solo propagated to it */
					continue;
				}

				if ((group_already_accounted_for && (*i)->route_group() && (*i)->route_group() == rg)) {
					/* this route is a part of the same solo group as the route
					 * that was changed. Changing that route did change or will
					 * change all group members appropriately, so we can ignore it
					 * here
					 */
					continue;
				}
				(*i)->solo_control()->set_value (0.0, Controllable::NoGroup);
			}
		}

		_listen_cnt++;

	} else if (_listen_cnt > 0) {

		_listen_cnt--;
	}
}

void
Session::route_solo_isolated_changed (boost::weak_ptr<Route> wpr)
{
	boost::shared_ptr<Route> route (wpr.lock());

	if (!route) {
		return;
	}

	bool send_changed = false;

	if (route->solo_isolate_control()->solo_isolated()) {
		if (_solo_isolated_cnt == 0) {
			send_changed = true;
		}
		_solo_isolated_cnt++;
	} else if (_solo_isolated_cnt > 0) {
		_solo_isolated_cnt--;
		if (_solo_isolated_cnt == 0) {
			send_changed = true;
		}
	}

	if (send_changed) {
		IsolatedChanged (); /* EMIT SIGNAL */
	}
}

void
Session::route_solo_changed (bool self_solo_changed, Controllable::GroupControlDisposition group_override,  boost::weak_ptr<Route> wpr)
{
	DEBUG_TRACE (DEBUG::Solo, string_compose ("route solo change, self = %1, update\n", self_solo_changed));

	boost::shared_ptr<Route> route (wpr.lock());

	if (!route) {
		return;
	}

	if (Config->get_solo_control_is_listen_control()) {
		route_listen_changed (group_override, wpr);
		return;
	}

	DEBUG_TRACE (DEBUG::Solo, string_compose ("%1: self %2 masters %3 transition %4\n", route->name(), route->self_soloed(), route->solo_control()->get_masters_value(), route->solo_control()->transitioned_into_solo()));

	if (route->solo_control()->transitioned_into_solo() == 0) {
		/* route solo changed by upstream/downstream or clear all solo state; not interesting
		   to Session.
		*/
		DEBUG_TRACE (DEBUG::Solo, string_compose ("%1 not self-soloed nor soloed by master (%2), ignoring\n", route->name(), route->solo_control()->get_masters_value()));
		return;
	}

	boost::shared_ptr<RouteList> r = routes.reader ();
	int32_t delta = route->solo_control()->transitioned_into_solo ();

	/* the route may be a member of a group that has shared-solo
	 * semantics. If so, then all members of that group should follow the
	 * solo of the changed route. But ... this is optional, controlled by a
	 * Controllable::GroupControlDisposition.
	 *
	 * The first argument to the signal that this method is connected to is the
	 * GroupControlDisposition value that was used to change solo.
	 *
	 * If the solo change was done with group semantics (either InverseGroup
	 * (force the entire group to change even if the group shared solo is
	 * disabled) or UseGroup (use the group, which may or may not have the
	 * shared solo property enabled)) then as we propagate the change to
	 * the entire session we should IGNORE THE GROUP that the changed route
	 * belongs to.
	 */

	RouteGroup* rg = route->route_group ();
	const bool group_already_accounted_for = (group_override == Controllable::ForGroup);

	DEBUG_TRACE (DEBUG::Solo, string_compose ("propagate to session, group accounted for ? %1\n", group_already_accounted_for));

	if (delta == 1 && Config->get_exclusive_solo()) {

		/* new solo: disable all other solos, but not the group if its solo-enabled */

		for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {

			if ((*i) == route) {
				/* already changed */
				continue;
			}

			if ((*i)->solo_isolate_control()->solo_isolated() || !(*i)->can_solo()) {
				/* route does not get solo propagated to it */
				continue;
			}

			if ((group_already_accounted_for && (*i)->route_group() && (*i)->route_group() == rg)) {
				/* this route is a part of the same solo group as the route
				 * that was changed. Changing that route did change or will
				 * change all group members appropriately, so we can ignore it
				 * here
				 */
				continue;
			}

			(*i)->solo_control()->set_value (0.0, group_override);
		}
	}

	DEBUG_TRACE (DEBUG::Solo, string_compose ("propagate solo change, delta = %1\n", delta));

	RouteList uninvolved;

	DEBUG_TRACE (DEBUG::Solo, string_compose ("%1\n", route->name()));

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		bool via_sends_only;
		bool in_signal_flow;

		if ((*i) == route) {
			/* already changed */
			continue;
		}

		if ((*i)->solo_isolate_control()->solo_isolated() || !(*i)->can_solo()) {
			/* route does not get solo propagated to it */
			DEBUG_TRACE (DEBUG::Solo, string_compose ("%1 excluded from solo because iso = %2 can_solo = %3\n", (*i)->name(), (*i)->solo_isolate_control()->solo_isolated(),
			                                          (*i)->can_solo()));
			continue;
		}

		if ((group_already_accounted_for && (*i)->route_group() && (*i)->route_group() == rg)) {
			/* this route is a part of the same solo group as the route
			 * that was changed. Changing that route did change or will
			 * change all group members appropriately, so we can ignore it
			 * here
			 */
			continue;
		}

		in_signal_flow = false;

		DEBUG_TRACE (DEBUG::Solo, string_compose ("check feed from %1\n", (*i)->name()));

		if ((*i)->feeds (route, &via_sends_only)) {
			DEBUG_TRACE (DEBUG::Solo, string_compose ("\tthere is a feed from %1\n", (*i)->name()));
			if (!via_sends_only) {
				if (!route->soloed_by_others_upstream()) {
					(*i)->solo_control()->mod_solo_by_others_downstream (delta);
				} else {
					DEBUG_TRACE (DEBUG::Solo, "\talready soloed by others upstream\n");
				}
			} else {
				DEBUG_TRACE (DEBUG::Solo, string_compose ("\tthere is a send-only feed from %1\n", (*i)->name()));
			}
			in_signal_flow = true;
		} else {
			DEBUG_TRACE (DEBUG::Solo, string_compose ("\tno feed from %1\n", (*i)->name()));
		}

		DEBUG_TRACE (DEBUG::Solo, string_compose ("check feed to %1\n", (*i)->name()));

		if (route->feeds (*i, &via_sends_only)) {
			/* propagate solo upstream only if routing other than
			   sends is involved, but do consider the other route
			   (*i) to be part of the signal flow even if only
			   sends are involved.
			*/
			DEBUG_TRACE (DEBUG::Solo, string_compose ("%1 feeds %2 via sends only %3 sboD %4 sboU %5\n",
			                                          route->name(),
			                                          (*i)->name(),
			                                          via_sends_only,
			                                          route->soloed_by_others_downstream(),
			                                          route->soloed_by_others_upstream()));
			if (!via_sends_only) {
				//NB. Triggers Invert Push, which handles soloed by downstream
				DEBUG_TRACE (DEBUG::Solo, string_compose ("\tmod %1 by %2\n", (*i)->name(), delta));
				(*i)->solo_control()->mod_solo_by_others_upstream (delta);
			} else {
				DEBUG_TRACE (DEBUG::Solo, string_compose ("\tfeed to %1 ignored, sends-only\n", (*i)->name()));
			}
			in_signal_flow = true;
		} else {
			DEBUG_TRACE (DEBUG::Solo, string_compose("\tno feed to %1\n", (*i)->name()) );
		}

		if (!in_signal_flow) {
			uninvolved.push_back (*i);
		}
	}

	DEBUG_TRACE (DEBUG::Solo, "propagation complete\n");

	/* now notify that the mute state of the routes not involved in the signal
	   pathway of the just-solo-changed route may have altered.
	*/

	for (RouteList::iterator i = uninvolved.begin(); i != uninvolved.end(); ++i) {
		DEBUG_TRACE (DEBUG::Solo, string_compose ("mute change for %1, which neither feeds or is fed by %2\n", (*i)->name(), route->name()));
		(*i)->act_on_mute ();
		/* Session will emit SoloChanged() after all solo changes are
		 * complete, which should be used by UIs to update mute status
		 */
	}
}

void
Session::update_route_solo_state (boost::shared_ptr<RouteList> r)
{
	/* now figure out if anything that matters is soloed (or is "listening")*/

	bool something_soloed = false;
	bool something_listening = false;
	uint32_t listeners = 0;
	uint32_t isolated = 0;

	if (!r) {
		r = routes.reader();
	}

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		if ((*i)->can_solo()) {
			if (Config->get_solo_control_is_listen_control()) {
				if ((*i)->solo_control()->soloed_by_self_or_masters()) {
					listeners++;
					something_listening = true;
				}
			} else {
				(*i)->set_listen (false);
				if ((*i)->can_solo() && (*i)->solo_control()->soloed_by_self_or_masters()) {
					something_soloed = true;
				}
			}
		}

		if ((*i)->solo_isolate_control()->solo_isolated()) {
			isolated++;
		}
	}

	if (something_soloed != _non_soloed_outs_muted) {
		_non_soloed_outs_muted = something_soloed;
		SoloActive (_non_soloed_outs_muted); /* EMIT SIGNAL */
	}

	if (something_listening != _listening) {
		_listening = something_listening;
		SoloActive (_listening);
	}

	_listen_cnt = listeners;

	if (isolated != _solo_isolated_cnt) {
		_solo_isolated_cnt = isolated;
		IsolatedChanged (); /* EMIT SIGNAL */
	}

	DEBUG_TRACE (DEBUG::Solo, string_compose ("solo state updated by session, soloed? %1 listeners %2 isolated %3\n",
						  something_soloed, listeners, isolated));


	SoloChanged (); /* EMIT SIGNAL */
	set_dirty();
}

bool
Session::muted () const
{
	// TODO consider caching the value on every MuteChanged signal,
	// Note that API users may also subscribe to MuteChanged and hence
	// this method needs to be called first.
	bool muted = false;
	StripableList all;
	get_stripables (all);
	for (StripableList::const_iterator i = all.begin(); i != all.end(); ++i) {
		assert (!(*i)->is_auditioner()); // XXX remove me
		if ((*i)->is_monitor()) {
			continue;
		}
		boost::shared_ptr<Route> r = boost::dynamic_pointer_cast<Route>(*i);
		if (r && !r->active()) {
			continue;
		}
		boost::shared_ptr<MuteControl> mc = (*i)->mute_control();
		if (mc && mc->muted ()) {
			muted = true;
			break;
		}
	}
	return muted;
}

std::vector<boost::weak_ptr<AutomationControl> >
Session::cancel_all_mute ()
{
	StripableList all;
	get_stripables (all);
	std::vector<boost::weak_ptr<AutomationControl> > muted;
	boost::shared_ptr<ControlList> cl (new ControlList);
	for (StripableList::const_iterator i = all.begin(); i != all.end(); ++i) {
		assert (!(*i)->is_auditioner());
		if ((*i)->is_monitor()) {
			continue;
		}
		boost::shared_ptr<Route> r = boost::dynamic_pointer_cast<Route> (*i);
		if (r && !r->active()) {
			continue;
		}
		boost::shared_ptr<AutomationControl> ac = (*i)->mute_control();
		if (ac && ac->get_value () > 0) {
			cl->push_back (ac);
			muted.push_back (boost::weak_ptr<AutomationControl>(ac));
		}
	}
	if (!cl->empty ()) {
		set_controls (cl, 0.0, PBD::Controllable::UseGroup);
	}
	return muted;
}

void
Session::get_stripables (StripableList& sl, PresentationInfo::Flag fl) const
{
	boost::shared_ptr<RouteList> r = routes.reader ();
	for (RouteList::iterator it = r->begin(); it != r->end(); ++it) {
		if ((*it)->presentation_info ().flags () & fl) {
			sl.push_back (*it);
		}
	}

	if (fl & PresentationInfo::VCA) {
		VCAList v = _vca_manager->vcas ();
		sl.insert (sl.end(), v.begin(), v.end());
	}
}

StripableList
Session::get_stripables () const
{
	PresentationInfo::Flag fl = PresentationInfo::AllStripables;
	StripableList rv;
	Session::get_stripables (rv, fl);
	rv.sort (Stripable::Sorter ());
	return rv;
}

RouteList
Session::get_routelist (bool mixer_order, PresentationInfo::Flag fl) const
{
	boost::shared_ptr<RouteList> r = routes.reader ();
	RouteList rv;
	for (RouteList::iterator it = r->begin(); it != r->end(); ++it) {
		if ((*it)->presentation_info ().flags () & fl) {
			rv.push_back (*it);
		}
	}
	rv.sort (Stripable::Sorter (mixer_order));
	return rv;
}

boost::shared_ptr<RouteList>
Session::get_routes_with_internal_returns() const
{
	boost::shared_ptr<RouteList> r = routes.reader ();
	boost::shared_ptr<RouteList> rl (new RouteList);

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		if ((*i)->internal_return ()) {
			rl->push_back (*i);
		}
	}
	return rl;
}

bool
Session::io_name_is_legal (const std::string& name) const
{
	boost::shared_ptr<RouteList> r = routes.reader ();

	for (map<string,bool>::const_iterator reserved = reserved_io_names.begin(); reserved != reserved_io_names.end(); ++reserved) {
		if (name == reserved->first) {
			if (!route_by_name (reserved->first)) {
				/* first instance of a reserved name is allowed for some */
				return reserved->second;
			}
			/* all other instances of a reserved name are not allowed */
			return false;
		}
	}

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		if ((*i)->name() == name) {
			return false;
		}

		if ((*i)->has_io_processor_named (name)) {
			return false;
		}
	}

	return true;
}

void
Session::set_exclusive_input_active (boost::shared_ptr<RouteList> rl, bool onoff, bool flip_others)
{
	RouteList rl2;
	vector<string> connections;

	/* if we are passed only a single route and we're not told to turn
	 * others off, then just do the simple thing.
	 */

	if (flip_others == false && rl->size() == 1) {
		boost::shared_ptr<MidiTrack> mt = boost::dynamic_pointer_cast<MidiTrack> (rl->front());
		if (mt) {
			mt->set_input_active (onoff);
			return;
		}
	}

	for (RouteList::iterator rt = rl->begin(); rt != rl->end(); ++rt) {

		PortSet& ps ((*rt)->input()->ports());

		for (PortSet::iterator p = ps.begin(); p != ps.end(); ++p) {
			p->get_connections (connections);
		}

		for (vector<string>::iterator s = connections.begin(); s != connections.end(); ++s) {
			routes_using_input_from (*s, rl2);
		}

		/* scan all relevant routes to see if others are on or off */

		bool others_are_already_on = false;

		for (RouteList::iterator r = rl2.begin(); r != rl2.end(); ++r) {

			boost::shared_ptr<MidiTrack> mt = boost::dynamic_pointer_cast<MidiTrack> (*r);

			if (!mt) {
				continue;
			}

			if ((*r) != (*rt)) {
				if (mt->input_active()) {
					others_are_already_on = true;
				}
			} else {
				/* this one needs changing */
				mt->set_input_active (onoff);
			}
		}

		if (flip_others) {

			/* globally reverse other routes */

			for (RouteList::iterator r = rl2.begin(); r != rl2.end(); ++r) {
				if ((*r) != (*rt)) {
					boost::shared_ptr<MidiTrack> mt = boost::dynamic_pointer_cast<MidiTrack> (*r);
					if (mt) {
						mt->set_input_active (!others_are_already_on);
					}
				}
			}
		}
	}
}

void
Session::routes_using_input_from (const string& str, RouteList& rl)
{
	boost::shared_ptr<RouteList> r = routes.reader();

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		if ((*i)->input()->connected_to (str)) {
			rl.push_back (*i);
		}
	}
}

boost::shared_ptr<Route>
Session::route_by_name (string name) const
{
	boost::shared_ptr<RouteList> r = routes.reader ();

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		if ((*i)->name() == name) {
			return *i;
		}
	}

	return boost::shared_ptr<Route> ((Route*) 0);
}

boost::shared_ptr<Route>
Session::route_by_id (PBD::ID id) const
{
	boost::shared_ptr<RouteList> r = routes.reader ();

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		if ((*i)->id() == id) {
			return *i;
		}
	}

	return boost::shared_ptr<Route> ((Route*) 0);
}


boost::shared_ptr<Stripable>
Session::stripable_by_id (PBD::ID id) const
{
	StripableList sl;
	get_stripables (sl);

	for (StripableList::const_iterator s = sl.begin(); s != sl.end(); ++s) {
		if ((*s)->id() == id) {
			return *s;
		}
	}

	return boost::shared_ptr<Stripable>();
}

boost::shared_ptr<Processor>
Session::processor_by_id (PBD::ID id) const
{
	boost::shared_ptr<RouteList> r = routes.reader ();

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		boost::shared_ptr<Processor> p = (*i)->Route::processor_by_id (id);
		if (p) {
			return p;
		}
	}

	return boost::shared_ptr<Processor> ();
}

boost::shared_ptr<Route>
Session::get_remote_nth_route (PresentationInfo::order_t n) const
{
	return boost::dynamic_pointer_cast<Route> (get_remote_nth_stripable (n, PresentationInfo::Route));
}

boost::shared_ptr<Stripable>
Session::get_remote_nth_stripable (PresentationInfo::order_t n, PresentationInfo::Flag flags) const
{
	StripableList sl;
	PresentationInfo::order_t match_cnt = 0;

	get_stripables (sl);
	sl.sort (Stripable::Sorter());

	for (StripableList::const_iterator s = sl.begin(); s != sl.end(); ++s) {

		if ((*s)->presentation_info().hidden()) {
			/* if the caller didn't explicitly ask for hidden
			   stripables, ignore hidden ones. This matches
			   the semantics of the pre-PresentationOrder
			   "get by RID" logic of Ardour 4.x and earlier.

			   XXX at some point we should likely reverse
			   the logic of the flags, because asking for "the
			   hidden stripables" is not going to be common,
			   whereas asking for visible ones is normal.
			*/

			if (! (flags & PresentationInfo::Hidden)) {
				continue;
			}
		}

		if ((*s)->presentation_info().flag_match (flags)) {
			if (match_cnt++ == n) {
				return *s;
			}
		}
	}

	/* there is no nth stripable that matches the given flags */
	return boost::shared_ptr<Stripable>();
}

boost::shared_ptr<Route>
Session::route_by_selected_count (uint32_t id) const
{
	RouteList r (*(routes.reader ()));
	r.sort (Stripable::Sorter());

	RouteList::iterator i;

	for (i = r.begin(); i != r.end(); ++i) {
		if ((*i)->is_selected()) {
			if (id == 0) {
				return *i;
			}
			--id;
		}
	}

	return boost::shared_ptr<Route> ();
}

void
Session::reassign_track_numbers ()
{
	int64_t tn = 0;
	int64_t bn = 0;
	RouteList r (*(routes.reader ()));
	r.sort (Stripable::Sorter());

	StateProtector sp (this);

	for (RouteList::iterator i = r.begin(); i != r.end(); ++i) {
		assert (!(*i)->is_auditioner());
		if (boost::dynamic_pointer_cast<Track> (*i)) {
			(*i)->set_track_number(++tn);
		}
		else if (!(*i)->is_master() && !(*i)->is_monitor()) {
			(*i)->set_track_number(--bn);
		}
	}
	const uint32_t decimals = ceilf (log10f (tn + 1));
	const bool decimals_changed = _track_number_decimals != decimals;
	_track_number_decimals = decimals;

	if (decimals_changed && config.get_track_name_number ()) {
		for (RouteList::iterator i = r.begin(); i != r.end(); ++i) {
			boost::shared_ptr<Track> t = boost::dynamic_pointer_cast<Track> (*i);
			if (t) {
				t->resync_track_name();
			}
		}
		// trigger GUI re-layout
		config.ParameterChanged("track-name-number");
	}

#ifndef NDEBUG
	if (DEBUG_ENABLED(DEBUG::OrderKeys)) {
		boost::shared_ptr<RouteList> rl = routes.reader ();
		for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
			DEBUG_TRACE (DEBUG::OrderKeys, string_compose ("%1 numbered %2\n", (*i)->name(), (*i)->track_number()));
		}
	}
#endif /* NDEBUG */

}

void
Session::playlist_region_added (boost::weak_ptr<Region> w)
{
	boost::shared_ptr<Region> r = w.lock ();
	if (!r) {
		return;
	}

	/* These are the operations that are currently in progress... */
	list<GQuark> curr = _current_trans_quarks;
	curr.sort ();

	/* ...and these are the operations during which we want to update
	   the session range location markers.
	*/
	list<GQuark> ops;
	ops.push_back (Operations::capture);
	ops.push_back (Operations::paste);
	ops.push_back (Operations::duplicate_region);
	ops.push_back (Operations::insert_file);
	ops.push_back (Operations::insert_region);
	ops.push_back (Operations::drag_region_brush);
	ops.push_back (Operations::region_drag);
	ops.push_back (Operations::selection_grab);
	ops.push_back (Operations::region_fill);
	ops.push_back (Operations::fill_selection);
	ops.push_back (Operations::create_region);
	ops.push_back (Operations::region_copy);
	ops.push_back (Operations::fixed_time_region_copy);
	ops.sort ();

	/* See if any of the current operations match the ones that we want */
	list<GQuark> in;
	set_intersection (_current_trans_quarks.begin(), _current_trans_quarks.end(), ops.begin(), ops.end(), back_inserter (in));

	/* If so, update the session range markers */
	if (!in.empty ()) {
		maybe_update_session_range (r->position (), r->last_sample ());
	}
}

/** Update the session range markers if a is before the current start or
 *  b is after the current end.
 */
void
Session::maybe_update_session_range (samplepos_t a, samplepos_t b)
{
	if (loading ()) {
		return;
	}

	samplepos_t session_end_marker_shift_samples = session_end_shift * _nominal_sample_rate;

	if (_session_range_location == 0) {

		set_session_extents (a, b + session_end_marker_shift_samples);

	} else {

		if (_session_range_is_free && (a < _session_range_location->start())) {
			_session_range_location->set_start (a);
		}

		if (_session_range_is_free && (b > _session_range_location->end())) {
			_session_range_location->set_end (b);
		}
	}
}

void
Session::set_session_range_is_free (bool yn)
{
	_session_range_is_free = yn;
}

void
Session::playlist_ranges_moved (list<Evoral::RangeMove<samplepos_t> > const & ranges)
{
	for (list<Evoral::RangeMove<samplepos_t> >::const_iterator i = ranges.begin(); i != ranges.end(); ++i) {
		maybe_update_session_range (i->to, i->to + i->length);
	}
}

void
Session::playlist_regions_extended (list<Evoral::Range<samplepos_t> > const & ranges)
{
	for (list<Evoral::Range<samplepos_t> >::const_iterator i = ranges.begin(); i != ranges.end(); ++i) {
		maybe_update_session_range (i->from, i->to);
	}
}

/* Region management */

boost::shared_ptr<Region>
Session::find_whole_file_parent (boost::shared_ptr<Region const> child) const
{
	const RegionFactory::RegionMap& regions (RegionFactory::regions());
	RegionFactory::RegionMap::const_iterator i;
	boost::shared_ptr<Region> region;

	Glib::Threads::Mutex::Lock lm (region_lock);

	for (i = regions.begin(); i != regions.end(); ++i) {

		region = i->second;

		if (region->whole_file()) {

			if (child->source_equivalent (region)) {
				return region;
			}
		}
	}

	return boost::shared_ptr<Region> ();
}

int
Session::destroy_sources (list<boost::shared_ptr<Source> > const& srcs)
{
	set<boost::shared_ptr<Region> > relevant_regions;

	for (list<boost::shared_ptr<Source> >::const_iterator s = srcs.begin(); s != srcs.end(); ++s) {
		RegionFactory::get_regions_using_source (*s, relevant_regions);
	}

	for (set<boost::shared_ptr<Region> >::iterator r = relevant_regions.begin(); r != relevant_regions.end(); ) {
		set<boost::shared_ptr<Region> >::iterator tmp;

		tmp = r;
		++tmp;

		_playlists->destroy_region (*r);
		RegionFactory::map_remove (*r);

		(*r)->drop_sources ();
		(*r)->drop_references ();

		relevant_regions.erase (r);

		r = tmp;
	}

	for (list<boost::shared_ptr<Source> >::const_iterator s = srcs.begin(); s != srcs.end(); ++s) {

		{
			Glib::Threads::Mutex::Lock ls (source_lock);
			/* remove from the main source list */
			sources.erase ((*s)->id());
		}

		(*s)->mark_for_remove ();
		(*s)->drop_references ();
		SourceRemoved (boost::weak_ptr<Source> (*s)); /* EMIT SIGNAL */
	}

	return 0;
}

int
Session::remove_last_capture ()
{
	list<boost::shared_ptr<Source> > srcs;

	boost::shared_ptr<RouteList> rl = routes.reader ();
	for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
		boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);
		if (!tr) {
			continue;
		}

		list<boost::shared_ptr<Source> >& l = tr->last_capture_sources();

		if (!l.empty()) {
			srcs.insert (srcs.end(), l.begin(), l.end());
			l.clear ();
		}
	}

	destroy_sources (srcs);

	save_state (_current_snapshot_name);

	return 0;
}

void
Session::get_last_capture_sources (std::list<boost::shared_ptr<Source> >& srcs)
{
	boost::shared_ptr<RouteList> rl = routes.reader ();
	for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
		boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);
		if (!tr) {
			continue;
		}

		list<boost::shared_ptr<Source> >& l = tr->last_capture_sources();

		if (!l.empty()) {
			srcs.insert (srcs.end(), l.begin(), l.end());
			l.clear ();
		}
	}
}

/* Source Management */

void
Session::add_source (boost::shared_ptr<Source> source)
{
	pair<SourceMap::key_type, SourceMap::mapped_type> entry;
	pair<SourceMap::iterator,bool> result;

	entry.first = source->id();
	entry.second = source;

	{
		Glib::Threads::Mutex::Lock lm (source_lock);
		result = sources.insert (entry);
	}

	if (result.second) {

		/* yay, new source */

		boost::shared_ptr<FileSource> fs = boost::dynamic_pointer_cast<FileSource> (source);

		if (fs) {
			if (!fs->within_session()) {
				ensure_search_path_includes (Glib::path_get_dirname (fs->path()), fs->type());
			}
		}

		set_dirty();

		boost::shared_ptr<AudioFileSource> afs;

		if ((afs = boost::dynamic_pointer_cast<AudioFileSource>(source)) != 0) {
			if (Config->get_auto_analyse_audio()) {
				Analyser::queue_source_for_analysis (source, false);
			}
		}

		source->DropReferences.connect_same_thread (*this, boost::bind (&Session::remove_source, this, boost::weak_ptr<Source> (source)));

		SourceAdded (boost::weak_ptr<Source> (source)); /* EMIT SIGNAL */
	}
}

void
Session::remove_source (boost::weak_ptr<Source> src)
{
	if (deletion_in_progress ()) {
		return;
	}

	SourceMap::iterator i;
	boost::shared_ptr<Source> source = src.lock();

	if (!source) {
		return;
	}

	{
		Glib::Threads::Mutex::Lock lm (source_lock);

		if ((i = sources.find (source->id())) != sources.end()) {
			sources.erase (i);
			SourceRemoved (src); /* EMIT SIGNAL */
		}
	}

	if (!in_cleanup () && !loading ()) {

		/* save state so we don't end up with a session file
		 * referring to non-existent sources.
		 */

		save_state (_current_snapshot_name);
	}
}

boost::shared_ptr<Source>
Session::source_by_id (const PBD::ID& id)
{
	Glib::Threads::Mutex::Lock lm (source_lock);
	SourceMap::iterator i;
	boost::shared_ptr<Source> source;

	if ((i = sources.find (id)) != sources.end()) {
		source = i->second;
	}

	return source;
}

boost::shared_ptr<AudioFileSource>
Session::audio_source_by_path_and_channel (const string& path, uint16_t chn) const
{
	/* Restricted to audio files because only audio sources have channel
	   as a property.
	*/

	Glib::Threads::Mutex::Lock lm (source_lock);

	for (SourceMap::const_iterator i = sources.begin(); i != sources.end(); ++i) {
		boost::shared_ptr<AudioFileSource> afs
			= boost::dynamic_pointer_cast<AudioFileSource>(i->second);

		if (afs && afs->path() == path && chn == afs->channel()) {
			return afs;
		}
	}

	return boost::shared_ptr<AudioFileSource>();
}

boost::shared_ptr<MidiSource>
Session::midi_source_by_path (const std::string& path, bool need_source_lock) const
{
	/* Restricted to MIDI files because audio sources require a channel
	   for unique identification, in addition to a path.
	*/

	Glib::Threads::Mutex::Lock lm (source_lock, Glib::Threads::NOT_LOCK);
	if (need_source_lock) {
		lm.acquire ();
	}

	for (SourceMap::const_iterator s = sources.begin(); s != sources.end(); ++s) {
		boost::shared_ptr<MidiSource> ms
			= boost::dynamic_pointer_cast<MidiSource>(s->second);
		boost::shared_ptr<FileSource> fs
			= boost::dynamic_pointer_cast<FileSource>(s->second);

		if (ms && fs && fs->path() == path) {
			return ms;
		}
	}

	return boost::shared_ptr<MidiSource>();
}

uint32_t
Session::count_sources_by_origin (const string& path)
{
	uint32_t cnt = 0;
	Glib::Threads::Mutex::Lock lm (source_lock);

	for (SourceMap::const_iterator i = sources.begin(); i != sources.end(); ++i) {
		boost::shared_ptr<FileSource> fs
			= boost::dynamic_pointer_cast<FileSource>(i->second);

		if (fs && fs->origin() == path) {
			++cnt;
		}
	}

	return cnt;
}

static string
peak_file_helper (const string& peak_path, const string& file_path, const string& file_base, bool hash) {
	if (hash) {
		std::string checksum = Glib::Checksum::compute_checksum(Glib::Checksum::CHECKSUM_SHA1, file_path + G_DIR_SEPARATOR + file_base);
		return Glib::build_filename (peak_path, checksum + peakfile_suffix);
	} else {
		return Glib::build_filename (peak_path, file_base + peakfile_suffix);
	}
}

string
Session::construct_peak_filepath (const string& filepath, const bool in_session, const bool old_peak_name) const
{
	string interchange_dir_string = string (interchange_dir_name) + G_DIR_SEPARATOR;

	if (Glib::path_is_absolute (filepath)) {

		/* rip the session dir from the audiofile source */

		string session_path;
		bool in_another_session = true;

		if (filepath.find (interchange_dir_string) != string::npos) {

			session_path = Glib::path_get_dirname (filepath); /* now ends in audiofiles */
			session_path = Glib::path_get_dirname (session_path); /* now ends in session name */
			session_path = Glib::path_get_dirname (session_path); /* now ends in interchange */
			session_path = Glib::path_get_dirname (session_path); /* now has session path */

			/* see if it is within our session */

			for (vector<space_and_path>::const_iterator i = session_dirs.begin(); i != session_dirs.end(); ++i) {
				if (i->path == session_path) {
					in_another_session = false;
					break;
				}
			}
		} else {
			in_another_session = false;
		}


		if (in_another_session) {
			SessionDirectory sd (session_path);
			return peak_file_helper (sd.peak_path(), "", Glib::path_get_basename (filepath), !old_peak_name);
		}
	}

	/* 1) if file belongs to this session
	 * it may be a relative path (interchange/...)
	 * or just basename (session_state, remove source)
	 * -> just use the basename
	 */
	std::string filename = Glib::path_get_basename (filepath);
	std::string path;

	/* 2) if the file is outside our session dir:
	 * (imported but not copied) add the path for check-summming */
	if (!in_session) {
		path = Glib::path_get_dirname (filepath);
	}

	return peak_file_helper (_session_dir->peak_path(), path, Glib::path_get_basename (filepath), !old_peak_name);
}

string
Session::new_audio_source_path_for_embedded (const std::string& path)
{
	/* embedded source:
	 *
	 * we know that the filename is already unique because it exists
	 * out in the filesystem.
	 *
	 * However, when we bring it into the session, we could get a
	 * collision.
	 *
	 * Eg. two embedded files:
	 *
	 *          /foo/bar/baz.wav
	 *          /frob/nic/baz.wav
	 *
	 * When merged into session, these collide.
	 *
	 * There will not be a conflict with in-memory sources
	 * because when the source was created we already picked
	 * a unique name for it.
	 *
	 * This collision is not likely to be common, but we have to guard
	 * against it.  So, if there is a collision, take the md5 hash of the
	 * the path, and use that as the filename instead.
	 */

	SessionDirectory sdir (get_best_session_directory_for_new_audio());
	string base = Glib::path_get_basename (path);
	string newpath = Glib::build_filename (sdir.sound_path(), base);

	if (Glib::file_test (newpath, Glib::FILE_TEST_EXISTS)) {

		MD5 md5;

		md5.digestString (path.c_str());
		md5.writeToString ();
		base = md5.digestChars;

		string ext = get_suffix (path);

		if (!ext.empty()) {
			base += '.';
			base += ext;
		}

		newpath = Glib::build_filename (sdir.sound_path(), base);

		/* if this collides, we're screwed */

		if (Glib::file_test (newpath, Glib::FILE_TEST_EXISTS)) {
			error << string_compose (_("Merging embedded file %1: name collision AND md5 hash collision!"), path) << endmsg;
			return string();
		}

	}

	return newpath;
}

/** Return true if there are no audio file sources that use @param name as
 * the filename component of their path.
 *
 * Return false otherwise.
 *
 * This method MUST ONLY be used to check in-session, mono files since it
 * hard-codes the channel of the audio file source we are looking for as zero.
 *
 * If/when Ardour supports native files in non-mono formats, the logic here
 * will need to be revisited.
 */
bool
Session::audio_source_name_is_unique (const string& name)
{
	std::vector<string> sdirs = source_search_path (DataType::AUDIO);
	vector<space_and_path>::iterator i;
	uint32_t existing = 0;

	for (vector<string>::const_iterator i = sdirs.begin(); i != sdirs.end(); ++i) {

		/* note that we search *without* the extension so that
		   we don't end up both "Audio 1-1.wav" and "Audio 1-1.caf"
		   in the event that this new name is required for
		   a file format change.
		*/

		const string spath = *i;

		if (matching_unsuffixed_filename_exists_in (spath, name)) {
			existing++;
			break;
		}

		/* it is possible that we have the path already
		 * assigned to a source that has not yet been written
		 * (ie. the write source for a diskstream). we have to
		 * check this in order to make sure that our candidate
		 * path isn't used again, because that can lead to
		 * two Sources point to the same file with different
		 * notions of their removability.
		 */


		string possible_path = Glib::build_filename (spath, name);

		if (audio_source_by_path_and_channel (possible_path, 0)) {
			existing++;
			break;
		}
	}

	return (existing == 0);
}

string
Session::format_audio_source_name (const string& legalized_base, uint32_t nchan, uint32_t chan, bool destructive, bool take_required, uint32_t cnt, bool related_exists)
{
	ostringstream sstr;
	const string ext = native_header_format_extension (config.get_native_file_header_format(), DataType::AUDIO);

	sstr << legalized_base;

	if (take_required || related_exists) {
		sstr << '-';
		sstr << cnt;
	}

	if (nchan == 2) {
		if (chan == 0) {
			sstr << "%L";
		} else {
			sstr << "%R";
		}
	} else if (nchan > 2) {
		if (nchan < 26) {
			sstr << '%';
			sstr << 'a' + chan;
		} else {
			/* XXX what? more than 26 channels! */
			sstr << '%';
			sstr << chan+1;
		}
	}

	sstr << ext;

	return sstr.str();
}

/** Return a unique name based on \a base for a new internal audio source */
string
Session::new_audio_source_path (const string& base, uint32_t nchan, uint32_t chan, bool destructive, bool take_required)
{
	uint32_t cnt;
	string possible_name;
	const uint32_t limit = 9999; // arbitrary limit on number of files with the same basic name
	string legalized;
	bool some_related_source_name_exists = false;

	legalized = legalize_for_path (base);

	// Find a "version" of the base name that doesn't exist in any of the possible directories.

	for (cnt = (destructive ? ++destructive_index : 1); cnt <= limit; ++cnt) {

		possible_name = format_audio_source_name (legalized, nchan, chan, destructive, take_required, cnt, some_related_source_name_exists);

		if (audio_source_name_is_unique (possible_name)) {
			break;
		}

		some_related_source_name_exists = true;

		if (cnt > limit) {
			error << string_compose(
					_("There are already %1 recordings for %2, which I consider too many."),
					limit, base) << endmsg;
			destroy ();
			throw failed_constructor();
		}
	}

	/* We've established that the new name does not exist in any session
	 * directory, so now find out which one we should use for this new
	 * audio source.
	 */

	SessionDirectory sdir (get_best_session_directory_for_new_audio());

	std::string s = Glib::build_filename (sdir.sound_path(), possible_name);

	return s;
}

/** Return a unique name based on `base` for a new internal MIDI source */
string
Session::new_midi_source_path (const string& base, bool need_lock)
{
	string possible_path;
	string possible_name;

	possible_name = legalize_for_path (base);

	// Find a "version" of the file name that doesn't exist in any of the possible directories.
	std::vector<string> sdirs = source_search_path(DataType::MIDI);

	/* - the main session folder is the first in the vector.
	 * - after checking all locations for file-name uniqueness,
	 *   we keep the one from the last iteration as new file name
	 * - midi files are small and should just be kept in the main session-folder
	 *
	 * -> reverse the array, check main session folder last and use that as location
	 *    for MIDI files.
	 */
	std::reverse(sdirs.begin(), sdirs.end());

	while (true) {
		possible_name = bump_name_once (possible_name, '-');

		vector<space_and_path>::iterator i;
		uint32_t existing = 0;

		for (vector<string>::const_iterator i = sdirs.begin(); i != sdirs.end(); ++i) {

			possible_path = Glib::build_filename (*i, possible_name + ".mid");

			if (Glib::file_test (possible_path, Glib::FILE_TEST_EXISTS)) {
				existing++;
			}

			if (midi_source_by_path (possible_path, need_lock)) {
				existing++;
			}
		}

		if (possible_path.size () >= PATH_MAX) {
			error << string_compose(
					_("There are already many recordings for %1, resulting in a too long file-path %2."),
					base, possible_path) << endmsg;
			destroy ();
			return 0;
		}

		if (existing == 0) {
			break;
		}
	}

	/* No need to "find best location" for software/app-based RAID, because
	   MIDI is so small that we always put it in the same place.
	*/

	return possible_path;
}


/** Create a new within-session audio source */
boost::shared_ptr<AudioFileSource>
Session::create_audio_source_for_session (size_t n_chans, string const & base, uint32_t chan, bool destructive)
{
	const string path = new_audio_source_path (base, n_chans, chan, destructive, true);

	if (!path.empty()) {
		return boost::dynamic_pointer_cast<AudioFileSource> (
			SourceFactory::createWritable (DataType::AUDIO, *this, path, destructive, sample_rate(), true, true));
	} else {
		throw failed_constructor ();
	}
}

/** Create a new within-session MIDI source */
boost::shared_ptr<MidiSource>
Session::create_midi_source_for_session (string const & basic_name)
{
	const string path = new_midi_source_path (basic_name);

	if (!path.empty()) {
		return boost::dynamic_pointer_cast<SMFSource> (
			SourceFactory::createWritable (
				DataType::MIDI, *this, path, false, sample_rate()));
	} else {
		throw failed_constructor ();
	}
}

/** Create a new within-session MIDI source */
boost::shared_ptr<MidiSource>
Session::create_midi_source_by_stealing_name (boost::shared_ptr<Track> track)
{
	/* the caller passes in the track the source will be used in,
	   so that we can keep the numbering sane.

	   Rationale: a track with the name "Foo" that has had N
	   captures carried out so far will ALREADY have a write source
	   named "Foo-N+1.mid" waiting to be used for the next capture.

	   If we call new_midi_source_name() we will get "Foo-N+2". But
	   there is no region corresponding to "Foo-N+1", so when
	   "Foo-N+2" appears in the track, the gap presents the user
	   with odd behaviour - why did it skip past Foo-N+1?

	   We could explain this to the user in some odd way, but
	   instead we rename "Foo-N+1.mid" as "Foo-N+2.mid", and then
	   use "Foo-N+1" here.

	   If that attempted rename fails, we get "Foo-N+2.mid" anyway.
	*/

	boost::shared_ptr<MidiTrack> mt = boost::dynamic_pointer_cast<MidiTrack> (track);
	assert (mt);
	std::string name = track->steal_write_source_name ();

	if (name.empty()) {
		return boost::shared_ptr<MidiSource>();
	}

	/* MIDI files are small, just put them in the first location of the
	   session source search path.
	*/

	const string path = Glib::build_filename (source_search_path (DataType::MIDI).front(), name);

	return boost::dynamic_pointer_cast<SMFSource> (
		SourceFactory::createWritable (
			DataType::MIDI, *this, path, false, sample_rate()));
}

bool
Session::playlist_is_active (boost::shared_ptr<Playlist> playlist)
{
	Glib::Threads::Mutex::Lock lm (_playlists->lock);
	for (SessionPlaylists::List::iterator i = _playlists->playlists.begin(); i != _playlists->playlists.end(); i++) {
		if ( (*i) == playlist ) {
			return true;
		}
	}
	return false;
}

void
Session::add_playlist (boost::shared_ptr<Playlist> playlist, bool unused)
{
	if (playlist->hidden()) {
		return;
	}

	_playlists->add (playlist);

	if (unused) {
		playlist->release();
	}

	set_dirty();
}

void
Session::remove_playlist (boost::weak_ptr<Playlist> weak_playlist)
{
	if (deletion_in_progress ()) {
		return;
	}

	boost::shared_ptr<Playlist> playlist (weak_playlist.lock());

	if (!playlist) {
		return;
	}

	_playlists->remove (playlist);

	set_dirty();
}

void
Session::set_audition (boost::shared_ptr<Region> r)
{
	pending_audition_region = r;
	add_post_transport_work (PostTransportAudition);
	_butler->schedule_transport_work ();
}

void
Session::audition_playlist ()
{
	SessionEvent* ev = new SessionEvent (SessionEvent::Audition, SessionEvent::Add, SessionEvent::Immediate, 0, 0.0);
	ev->region.reset ();
	queue_event (ev);
}


void
Session::register_lua_function (
		const std::string& name,
		const std::string& script,
		const LuaScriptParamList& args
		)
{
	Glib::Threads::Mutex::Lock lm (lua_lock);

	lua_State* L = lua.getState();

	const std::string& bytecode = LuaScripting::get_factory_bytecode (script);
	luabridge::LuaRef tbl_arg (luabridge::newTable(L));
	for (LuaScriptParamList::const_iterator i = args.begin(); i != args.end(); ++i) {
		if ((*i)->optional && !(*i)->is_set) { continue; }
		tbl_arg[(*i)->name] = (*i)->value;
	}
	(*_lua_add)(name, bytecode, tbl_arg); // throws luabridge::LuaException
	lm.release();

	LuaScriptsChanged (); /* EMIT SIGNAL */
	set_dirty();
}

void
Session::unregister_lua_function (const std::string& name)
{
	Glib::Threads::Mutex::Lock lm (lua_lock);
	(*_lua_del)(name); // throws luabridge::LuaException
	lua.collect_garbage ();
	lm.release();

	LuaScriptsChanged (); /* EMIT SIGNAL */
	set_dirty();
}

std::vector<std::string>
Session::registered_lua_functions ()
{
	Glib::Threads::Mutex::Lock lm (lua_lock);
	std::vector<std::string> rv;

	try {
		luabridge::LuaRef list ((*_lua_list)());
		for (luabridge::Iterator i (list); !i.isNil (); ++i) {
			if (!i.key ().isString ()) { assert(0); continue; }
			rv.push_back (i.key ().cast<std::string> ());
		}
	} catch (...) { }
	return rv;
}

#ifndef NDEBUG
static void _lua_print (std::string s) {
	std::cout << "SessionLua: " << s << "\n";
}
#endif

void
Session::try_run_lua (pframes_t nframes)
{
	if (_n_lua_scripts == 0) return;
	Glib::Threads::Mutex::Lock tm (lua_lock, Glib::Threads::TRY_LOCK);
	if (tm.locked ()) {
		try { (*_lua_run)(nframes); } catch (...) { }
		lua.collect_garbage_step ();
	}
}

void
Session::setup_lua ()
{
#ifndef NDEBUG
	lua.Print.connect (&_lua_print);
#endif
	lua.sandbox (true);
	lua.do_command (
			"function ArdourSession ()"
			"  local self = { scripts = {}, instances = {} }"
			""
			"  local remove = function (n)"
			"   self.scripts[n] = nil"
			"   self.instances[n] = nil"
			"   Session:scripts_changed()" // call back
			"  end"
			""
			"  local addinternal = function (n, f, a)"
			"   assert(type(n) == 'string', 'function-name must be string')"
			"   assert(type(f) == 'function', 'Given script is a not a function')"
			"   assert(type(a) == 'table' or type(a) == 'nil', 'Given argument is invalid')"
			"   assert(self.scripts[n] == nil, 'Callback \"'.. n ..'\" already exists.')"
			"   self.scripts[n] = { ['f'] = f, ['a'] = a }"
			"   local env = { print = print, tostring = tostring, assert = assert, ipairs = ipairs, error = error, select = select, string = string, type = type, tonumber = tonumber, collectgarbage = collectgarbage, pairs = pairs, math = math, table = table, pcall = pcall, bit32=bit32, Session = Session, PBD = PBD, Timecode = Timecode, Evoral = Evoral, C = C, ARDOUR = ARDOUR }"
			"   self.instances[n] = load (string.dump(f, true), nil, nil, env)(a)"
			"   Session:scripts_changed()" // call back
			"  end"
			""
			"  local add = function (n, b, a)"
			"   assert(type(b) == 'string', 'ByteCode must be string')"
			"   load (b)()" // assigns f
			"   assert(type(f) == 'string', 'Assigned ByteCode must be string')"
			"   addinternal (n, load(f), a)"
			"  end"
			""
			"  local run = function (...)"
			"   for n, s in pairs (self.instances) do"
			"     local status, err = pcall (s, ...)"
			"     if not status then"
			"       print ('fn \"'.. n .. '\": ', err)"
			"       remove (n)"
			"      end"
			"   end"
			"   collectgarbage(\"step\")"
			"  end"
			""
			"  local cleanup = function ()"
			"   self.scripts = nil"
			"   self.instances = nil"
			"  end"
			""
			"  local list = function ()"
			"   local rv = {}"
			"   for n, _ in pairs (self.scripts) do"
			"     rv[n] = true"
			"   end"
			"   return rv"
			"  end"
			""
			"  local function basic_serialize (o)"
			"    if type(o) == \"number\" then"
			"     return tostring(o)"
			"    else"
			"     return string.format(\"%q\", o)"
			"    end"
			"  end"
			""
			"  local function serialize (name, value)"
			"   local rv = name .. ' = '"
			"   collectgarbage()"
			"   if type(value) == \"number\" or type(value) == \"string\" or type(value) == \"nil\" then"
			"    return rv .. basic_serialize(value) .. ' '"
			"   elseif type(value) == \"table\" then"
			"    rv = rv .. '{} '"
			"    for k,v in pairs(value) do"
			"     local fieldname = string.format(\"%s[%s]\", name, basic_serialize(k))"
			"     rv = rv .. serialize(fieldname, v) .. ' '"
			"     collectgarbage()" // string concatenation allocates a new string :(
			"    end"
			"    return rv;"
			"   elseif type(value) == \"function\" then"
			"     return rv .. string.format(\"%q\", string.dump(value, true))"
			"   else"
			"    error('cannot save a ' .. type(value))"
			"   end"
			"  end"
			""
			""
			"  local save = function ()"
			"   return (serialize('scripts', self.scripts))"
			"  end"
			""
			"  local restore = function (state)"
			"   self.scripts = {}"
			"   load (state)()"
			"   for n, s in pairs (scripts) do"
			"    addinternal (n, load(s['f']), s['a'])"
			"   end"
			"  end"
			""
			" return { run = run, add = add, remove = remove,"
		  "          list = list, restore = restore, save = save, cleanup = cleanup}"
			" end"
			" "
			" sess = ArdourSession ()"
			" ArdourSession = nil"
			" "
			"function ardour () end"
			);

	lua_State* L = lua.getState();

	try {
		luabridge::LuaRef lua_sess = luabridge::getGlobal (L, "sess");
		lua.do_command ("sess = nil"); // hide it.
		lua.do_command ("collectgarbage()");

		_lua_run = new luabridge::LuaRef(lua_sess["run"]);
		_lua_add = new luabridge::LuaRef(lua_sess["add"]);
		_lua_del = new luabridge::LuaRef(lua_sess["remove"]);
		_lua_list = new luabridge::LuaRef(lua_sess["list"]);
		_lua_save = new luabridge::LuaRef(lua_sess["save"]);
		_lua_load = new luabridge::LuaRef(lua_sess["restore"]);
		_lua_cleanup = new luabridge::LuaRef(lua_sess["cleanup"]);
	} catch (luabridge::LuaException const& e) {
		fatal << string_compose (_("programming error: %1"),
				std::string ("Failed to setup session Lua interpreter") + e.what ())
			<< endmsg;
		abort(); /*NOTREACHED*/
	} catch (...) {
		fatal << string_compose (_("programming error: %1"),
				X_("Failed to setup session Lua interpreter"))
			<< endmsg;
		abort(); /*NOTREACHED*/
	}

	lua_mlock (L, 1);
	LuaBindings::stddef (L);
	LuaBindings::common (L);
	LuaBindings::dsp (L);
	lua_mlock (L, 0);
	luabridge::push <Session *> (L, this);
	lua_setglobal (L, "Session");
}

void
Session::scripts_changed ()
{
	assert (!lua_lock.trylock()); // must hold lua_lock

	try {
		luabridge::LuaRef list ((*_lua_list)());
		int cnt = 0;
		for (luabridge::Iterator i (list); !i.isNil (); ++i) {
			if (!i.key ().isString ()) { assert(0); continue; }
			++cnt;
		}
		_n_lua_scripts = cnt;
	} catch (luabridge::LuaException const& e) {
		fatal << string_compose (_("programming error: %1"),
				std::string ("Indexing Lua Session Scripts failed.") + e.what ())
			<< endmsg;
		abort(); /*NOTREACHED*/
	} catch (...) {
		fatal << string_compose (_("programming error: %1"),
				X_("Indexing Lua Session Scripts failed."))
			<< endmsg;
		abort(); /*NOTREACHED*/
	}
}

void
Session::non_realtime_set_audition ()
{
	assert (pending_audition_region);
	auditioner->audition_region (pending_audition_region);
	pending_audition_region.reset ();
	AuditionActive (true); /* EMIT SIGNAL */
}

void
Session::audition_region (boost::shared_ptr<Region> r)
{
	SessionEvent* ev = new SessionEvent (SessionEvent::Audition, SessionEvent::Add, SessionEvent::Immediate, 0, 0.0);
	ev->region = r;
	queue_event (ev);
}

void
Session::cancel_audition ()
{
	if (!auditioner) {
		return;
	}
	if (auditioner->auditioning()) {
		auditioner->cancel_audition ();
		AuditionActive (false); /* EMIT SIGNAL */
	}
}

bool
Session::is_auditioning () const
{
	/* can be called before we have an auditioner object */
	if (auditioner) {
		return auditioner->auditioning();
	} else {
		return false;
	}
}

void
Session::graph_reordered (bool called_from_backend)
{
	/* don't do this stuff if we are setting up connections
	   from a set_state() call or creating new tracks. Ditto for deletion.
	*/

	if (inital_connect_or_deletion_in_progress () || _adding_routes_in_progress || _reconnecting_routes_in_progress || _route_deletion_in_progress) {
		return;
	}

	resort_routes ();

	/* force all diskstreams to update their capture offset values to
	 * reflect any changes in latencies within the graph.
	 */
	update_latency_compensation (true, called_from_backend);
}

/** @return Number of samples that there is disk space available to write,
 *  if known.
 */
boost::optional<samplecnt_t>
Session::available_capture_duration ()
{
	Glib::Threads::Mutex::Lock lm (space_lock);

	if (_total_free_4k_blocks_uncertain) {
		return boost::optional<samplecnt_t> ();
	}

	float sample_bytes_on_disk = 4.0; // keep gcc happy

	switch (config.get_native_file_data_format()) {
	case FormatFloat:
		sample_bytes_on_disk = 4.0;
		break;

	case FormatInt24:
		sample_bytes_on_disk = 3.0;
		break;

	case FormatInt16:
		sample_bytes_on_disk = 2.0;
		break;

	default:
		/* impossible, but keep some gcc versions happy */
		fatal << string_compose (_("programming error: %1"),
					 X_("illegal native file data format"))
		      << endmsg;
		abort(); /*NOTREACHED*/
	}

	double scale = 4096.0 / sample_bytes_on_disk;

	if (_total_free_4k_blocks * scale > (double) max_samplecnt) {
		return max_samplecnt;
	}

	return (samplecnt_t) floor (_total_free_4k_blocks * scale);
}

void
Session::tempo_map_changed (const PropertyChange&)
{
	clear_clicks ();

	_playlists->update_after_tempo_map_change ();

	_locations->apply (*this, &Session::update_locations_after_tempo_map_change);

	set_dirty ();
}

void
Session::update_locations_after_tempo_map_change (const Locations::LocationList& loc)
{
	for (Locations::LocationList::const_iterator i = loc.begin(); i != loc.end(); ++i) {
		(*i)->recompute_samples_from_beat ();
	}
}

/** Ensures that all buffers (scratch, send, silent, etc) are allocated for
 * the given count with the current block size.
 */
void
Session::ensure_buffers (ChanCount howmany)
{
	BufferManager::ensure_buffers (howmany, bounce_processing() ? bounce_chunk_size : 0);
}

void
Session::ensure_buffer_set(BufferSet& buffers, const ChanCount& count)
{
	for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
		buffers.ensure_buffers(*t, count.get(*t), _engine.raw_buffer_size(*t));
	}
}

uint32_t
Session::next_insert_id ()
{
	/* this doesn't really loop forever. just think about it */

	while (true) {
		for (boost::dynamic_bitset<uint32_t>::size_type n = 1; n < insert_bitset.size(); ++n) {
			if (!insert_bitset[n]) {
				insert_bitset[n] = true;
				return n;

			}
		}

		/* none available, so resize and try again */

		insert_bitset.resize (insert_bitset.size() + 16, false);
	}
}

uint32_t
Session::next_send_id ()
{
	/* this doesn't really loop forever. just think about it */

	while (true) {
		for (boost::dynamic_bitset<uint32_t>::size_type n = 1; n < send_bitset.size(); ++n) {
			if (!send_bitset[n]) {
				send_bitset[n] = true;
				return n;

			}
		}

		/* none available, so resize and try again */

		send_bitset.resize (send_bitset.size() + 16, false);
	}
}

uint32_t
Session::next_aux_send_id ()
{
	/* this doesn't really loop forever. just think about it */

	while (true) {
		for (boost::dynamic_bitset<uint32_t>::size_type n = 1; n < aux_send_bitset.size(); ++n) {
			if (!aux_send_bitset[n]) {
				aux_send_bitset[n] = true;
				return n;

			}
		}

		/* none available, so resize and try again */

		aux_send_bitset.resize (aux_send_bitset.size() + 16, false);
	}
}

uint32_t
Session::next_return_id ()
{
	/* this doesn't really loop forever. just think about it */

	while (true) {
		for (boost::dynamic_bitset<uint32_t>::size_type n = 1; n < return_bitset.size(); ++n) {
			if (!return_bitset[n]) {
				return_bitset[n] = true;
				return n;

			}
		}

		/* none available, so resize and try again */

		return_bitset.resize (return_bitset.size() + 16, false);
	}
}

void
Session::mark_send_id (uint32_t id)
{
	if (id >= send_bitset.size()) {
		send_bitset.resize (id+16, false);
	}
	if (send_bitset[id]) {
		warning << string_compose (_("send ID %1 appears to be in use already"), id) << endmsg;
	}
	send_bitset[id] = true;
}

void
Session::mark_aux_send_id (uint32_t id)
{
	if (id >= aux_send_bitset.size()) {
		aux_send_bitset.resize (id+16, false);
	}
	if (aux_send_bitset[id]) {
		warning << string_compose (_("aux send ID %1 appears to be in use already"), id) << endmsg;
	}
	aux_send_bitset[id] = true;
}

void
Session::mark_return_id (uint32_t id)
{
	if (id >= return_bitset.size()) {
		return_bitset.resize (id+16, false);
	}
	if (return_bitset[id]) {
		warning << string_compose (_("return ID %1 appears to be in use already"), id) << endmsg;
	}
	return_bitset[id] = true;
}

void
Session::mark_insert_id (uint32_t id)
{
	if (id >= insert_bitset.size()) {
		insert_bitset.resize (id+16, false);
	}
	if (insert_bitset[id]) {
		warning << string_compose (_("insert ID %1 appears to be in use already"), id) << endmsg;
	}
	insert_bitset[id] = true;
}

void
Session::unmark_send_id (uint32_t id)
{
	if (deletion_in_progress ()) {
		return;
	}
	if (id < send_bitset.size()) {
		send_bitset[id] = false;
	}
}

void
Session::unmark_aux_send_id (uint32_t id)
{
	if (deletion_in_progress ()) {
		return;
	}
	if (id < aux_send_bitset.size()) {
		aux_send_bitset[id] = false;
	}
}

void
Session::unmark_return_id (uint32_t id)
{
	if (deletion_in_progress ()) {
		return;
	}
	if (id < return_bitset.size()) {
		return_bitset[id] = false;
	}
}

void
Session::unmark_insert_id (uint32_t id)
{
	if (deletion_in_progress ()) {
		return;
	}
	if (id < insert_bitset.size()) {
		insert_bitset[id] = false;
	}
}

void
Session::reset_native_file_format ()
{
	boost::shared_ptr<RouteList> rl = routes.reader ();

	for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
		boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);
		if (tr) {
			/* don't save state as we do this, there's no point */
			_state_of_the_state = StateOfTheState (_state_of_the_state | InCleanup);
			tr->reset_write_sources (false);
			_state_of_the_state = StateOfTheState (_state_of_the_state & ~InCleanup);
		}
	}
}

bool
Session::route_name_unique (string n) const
{
	boost::shared_ptr<RouteList> r = routes.reader ();

	for (RouteList::const_iterator i = r->begin(); i != r->end(); ++i) {
		if ((*i)->name() == n) {
			return false;
		}
	}

	return true;
}

bool
Session::route_name_internal (string n) const
{
	if (auditioner && auditioner->name() == n) {
		return true;
	}

	if (_click_io && _click_io->name() == n) {
		return true;
	}

	return false;
}

int
Session::freeze_all (InterThreadInfo& itt)
{
	boost::shared_ptr<RouteList> r = routes.reader ();

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {

		boost::shared_ptr<Track> t;

		if ((t = boost::dynamic_pointer_cast<Track>(*i)) != 0) {
			/* XXX this is wrong because itt.progress will keep returning to zero at the start
			   of every track.
			*/
			t->freeze_me (itt);
		}
	}

	return 0;
}

boost::shared_ptr<Region>
Session::write_one_track (Track& track, samplepos_t start, samplepos_t end,
			  bool /*overwrite*/, vector<boost::shared_ptr<Source> >& srcs,
			  InterThreadInfo& itt,
			  boost::shared_ptr<Processor> endpoint, bool include_endpoint,
			  bool for_export, bool for_freeze)
{
	boost::shared_ptr<Region> result;
	boost::shared_ptr<Playlist> playlist;
	boost::shared_ptr<Source> source;
	ChanCount diskstream_channels (track.n_channels());
	samplepos_t position;
	samplecnt_t this_chunk;
	samplepos_t to_do;
	samplepos_t latency_skip;
	BufferSet buffers;
	samplepos_t len = end - start;
	bool need_block_size_reset = false;
	ChanCount const max_proc = track.max_processor_streams ();
	string legal_playlist_name;
	string possible_path;

	DataType data_type = track.data_type();

	if (end <= start) {
		error << string_compose (_("Cannot write a range where end <= start (e.g. %1 <= %2)"),
					 end, start) << endmsg;
		return result;
	}

	diskstream_channels = track.bounce_get_output_streams (diskstream_channels, endpoint,
			include_endpoint, for_export, for_freeze);

	if (data_type == DataType::MIDI && endpoint && !for_export && !for_freeze && diskstream_channels.n(DataType::AUDIO) > 0) {
		data_type = DataType::AUDIO;
	}

	if (diskstream_channels.n(data_type) < 1) {
		error << _("Cannot write a range with no data.") << endmsg;
		return result;
	}

	// block all process callback handling

	block_processing ();

	{
		// synchronize with AudioEngine::process_callback()
		// make sure processing is not currently running
		// and processing_blocked() is honored before
		// acquiring thread buffers
		Glib::Threads::Mutex::Lock lm (_engine.process_lock());
	}

	_bounce_processing_active = true;

	/* call tree *MUST* hold route_lock */

	if ((playlist = track.playlist()) == 0) {
		goto out;
	}

	legal_playlist_name = "(bounce)" + legalize_for_path (playlist->name());

	for (uint32_t chan_n = 0; chan_n < diskstream_channels.n(data_type); ++chan_n) {

		string base_name = string_compose ("%1-%2-bounce", playlist->name(), chan_n);
		string path = ((data_type == DataType::AUDIO)
		               ? new_audio_source_path (legal_playlist_name, diskstream_channels.n_audio(), chan_n, false, true)
		               : new_midi_source_path (legal_playlist_name));

		if (path.empty()) {
			goto out;
		}

		try {
			source = SourceFactory::createWritable (data_type, *this, path, false, sample_rate());
		}

		catch (failed_constructor& err) {
			error << string_compose (_("cannot create new file \"%1\" for %2"), path, track.name()) << endmsg;
			goto out;
		}

		srcs.push_back (source);
	}

	/* tell redirects that care that we are about to use a much larger
	 * blocksize. this will flush all plugins too, so that they are ready
	 * to be used for this process.
	 */

	need_block_size_reset = true;
	track.set_block_size (bounce_chunk_size);
	_engine.main_thread()->get_buffers ();

	position = start;
	to_do = len;
	latency_skip = track.bounce_get_latency (endpoint, include_endpoint, for_export, for_freeze);

	/* create a set of reasonably-sized buffers */
	for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
		buffers.ensure_buffers(*t, max_proc.get(*t), bounce_chunk_size);
	}
	buffers.set_count (max_proc);

	for (vector<boost::shared_ptr<Source> >::iterator src = srcs.begin(); src != srcs.end(); ++src) {
		boost::shared_ptr<AudioFileSource> afs = boost::dynamic_pointer_cast<AudioFileSource>(*src);
		boost::shared_ptr<MidiSource> ms;
		if (afs) {
			afs->prepare_for_peakfile_writes ();
		} else if ((ms = boost::dynamic_pointer_cast<MidiSource>(*src))) {
			Source::Lock lock(ms->mutex());
			ms->mark_streaming_write_started(lock);
		}
	}

	while (to_do && !itt.cancel) {

		this_chunk = min (to_do, bounce_chunk_size);

		if (track.export_stuff (buffers, start, this_chunk, endpoint, include_endpoint, for_export, for_freeze)) {
			goto out;
		}

		start += this_chunk;
		to_do -= this_chunk;
		itt.progress = (float) (1.0 - ((double) to_do / len));

		if (latency_skip >= bounce_chunk_size) {
			latency_skip -= bounce_chunk_size;
			continue;
		}

		const samplecnt_t current_chunk = this_chunk - latency_skip;

		uint32_t n = 0;
		for (vector<boost::shared_ptr<Source> >::iterator src=srcs.begin(); src != srcs.end(); ++src, ++n) {
			boost::shared_ptr<AudioFileSource> afs = boost::dynamic_pointer_cast<AudioFileSource>(*src);
			boost::shared_ptr<MidiSource> ms;

			if (afs) {
				if (afs->write (buffers.get_audio(n).data(latency_skip), current_chunk) != current_chunk) {
					goto out;
				}
			} else if ((ms = boost::dynamic_pointer_cast<MidiSource>(*src))) {
				Source::Lock lock(ms->mutex());

				const MidiBuffer& buf = buffers.get_midi(0);
				for (MidiBuffer::const_iterator i = buf.begin(); i != buf.end(); ++i) {
					Evoral::Event<samplepos_t> ev = *i;
					if (!endpoint || for_export) {
						ev.set_time(ev.time() - position);
					}
					ms->append_event_samples(lock, ev, ms->natural_position());
				}
			}
		}
		latency_skip = 0;
	}

	/* post-roll, pick up delayed processor output */
	latency_skip = track.bounce_get_latency (endpoint, include_endpoint, for_export, for_freeze);

	while (latency_skip && !itt.cancel) {
		this_chunk = min (latency_skip, bounce_chunk_size);
		latency_skip -= this_chunk;

		buffers.silence (this_chunk, 0);
		track.bounce_process (buffers, start, this_chunk, endpoint, include_endpoint, for_export, for_freeze);

		uint32_t n = 0;
		for (vector<boost::shared_ptr<Source> >::iterator src=srcs.begin(); src != srcs.end(); ++src, ++n) {
			boost::shared_ptr<AudioFileSource> afs = boost::dynamic_pointer_cast<AudioFileSource>(*src);

			if (afs) {
				if (afs->write (buffers.get_audio(n).data(), this_chunk) != this_chunk) {
					goto out;
				}
			}
		}
	}

	if (!itt.cancel) {

		time_t now;
		struct tm* xnow;
		time (&now);
		xnow = localtime (&now);

		for (vector<boost::shared_ptr<Source> >::iterator src=srcs.begin(); src != srcs.end(); ++src) {
			boost::shared_ptr<AudioFileSource> afs = boost::dynamic_pointer_cast<AudioFileSource>(*src);
			boost::shared_ptr<MidiSource> ms;

			if (afs) {
				afs->update_header (position, *xnow, now);
				afs->flush_header ();
			} else if ((ms = boost::dynamic_pointer_cast<MidiSource>(*src))) {
				Source::Lock lock(ms->mutex());
				ms->mark_streaming_write_completed(lock);
			}
		}

		/* construct a region to represent the bounced material */

		PropertyList plist;

		plist.add (Properties::start, 0);
		plist.add (Properties::whole_file, true);
		plist.add (Properties::length, srcs.front()->length(srcs.front()->natural_position()));
		plist.add (Properties::name, region_name_from_path (srcs.front()->name(), true));

		result = RegionFactory::create (srcs, plist, true);

	}

	out:
	if (!result) {
		for (vector<boost::shared_ptr<Source> >::iterator src = srcs.begin(); src != srcs.end(); ++src) {
			(*src)->mark_for_remove ();
			(*src)->drop_references ();
		}

	} else {
		for (vector<boost::shared_ptr<Source> >::iterator src = srcs.begin(); src != srcs.end(); ++src) {
			boost::shared_ptr<AudioFileSource> afs = boost::dynamic_pointer_cast<AudioFileSource>(*src);

			if (afs)
				afs->done_with_peakfile_writes ();
		}
	}

	_bounce_processing_active = false;

	if (need_block_size_reset) {
		_engine.main_thread()->drop_buffers ();
		track.set_block_size (get_block_size());
	}

	unblock_processing ();

	return result;
}

gain_t*
Session::gain_automation_buffer() const
{
	return ProcessThread::gain_automation_buffer ();
}

gain_t*
Session::trim_automation_buffer() const
{
	return ProcessThread::trim_automation_buffer ();
}

gain_t*
Session::send_gain_automation_buffer() const
{
	return ProcessThread::send_gain_automation_buffer ();
}

gain_t*
Session::scratch_automation_buffer() const
{
	return ProcessThread::scratch_automation_buffer ();
}

pan_t**
Session::pan_automation_buffer() const
{
	return ProcessThread::pan_automation_buffer ();
}

BufferSet&
Session::get_silent_buffers (ChanCount count)
{
	return ProcessThread::get_silent_buffers (count);
}

BufferSet&
Session::get_scratch_buffers (ChanCount count, bool silence)
{
	return ProcessThread::get_scratch_buffers (count, silence);
}

BufferSet&
Session::get_noinplace_buffers (ChanCount count)
{
	return ProcessThread::get_noinplace_buffers (count);
}

BufferSet&
Session::get_route_buffers (ChanCount count, bool silence)
{
	return ProcessThread::get_route_buffers (count, silence);
}


BufferSet&
Session::get_mix_buffers (ChanCount count)
{
	return ProcessThread::get_mix_buffers (count);
}

uint32_t
Session::ntracks () const
{
	uint32_t n = 0;
	boost::shared_ptr<RouteList> r = routes.reader ();

	for (RouteList::const_iterator i = r->begin(); i != r->end(); ++i) {
		if (boost::dynamic_pointer_cast<Track> (*i)) {
			++n;
		}
	}

	return n;
}

uint32_t
Session::nbusses () const
{
	uint32_t n = 0;
	boost::shared_ptr<RouteList> r = routes.reader ();

	for (RouteList::const_iterator i = r->begin(); i != r->end(); ++i) {
		if (boost::dynamic_pointer_cast<Track>(*i) == 0) {
			++n;
		}
	}

	return n;
}

uint32_t
Session::nstripables (bool with_monitor) const
{
	uint32_t rv = routes.reader()->size ();
	rv += _vca_manager->vcas ().size ();

	if (with_monitor) {
		return rv;
	}

	if (_monitor_out) {
		assert (rv > 0);
		--rv;
	}
	return rv;
}

bool
Session::plot_process_graph (std::string const& file_name) const {
	return _process_graph ? _process_graph->plot (file_name) : false;
}

void
Session::add_automation_list(AutomationList *al)
{
	automation_lists[al->id()] = al;
}

/** @return true if there is at least one record-enabled track, otherwise false */
bool
Session::have_rec_enabled_track () const
{
	return g_atomic_int_get (const_cast<gint*>(&_have_rec_enabled_track)) == 1;
}

bool
Session::have_rec_disabled_track () const
{
	return g_atomic_int_get (const_cast<gint*>(&_have_rec_disabled_track)) == 1;
}

/** Update the state of our rec-enabled tracks flag */
void
Session::update_route_record_state ()
{
	boost::shared_ptr<RouteList> rl = routes.reader ();
	RouteList::iterator i = rl->begin();
	while (i != rl->end ()) {

		boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);
	                            if (tr && tr->rec_enable_control()->get_value()) {
			break;
		}

		++i;
	}

	int const old = g_atomic_int_get (&_have_rec_enabled_track);

	g_atomic_int_set (&_have_rec_enabled_track, i != rl->end () ? 1 : 0);

	if (g_atomic_int_get (&_have_rec_enabled_track) != old) {
		RecordStateChanged (); /* EMIT SIGNAL */
	}

	for (i = rl->begin(); i != rl->end (); ++i) {
		boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);
		if (tr && !tr->rec_enable_control()->get_value()) {
			break;
		}
	}

	g_atomic_int_set (&_have_rec_disabled_track, i != rl->end () ? 1 : 0);

	bool record_arm_state_changed = (old != g_atomic_int_get (&_have_rec_enabled_track) );

	if (record_status() == Recording && record_arm_state_changed ) {
		RecordArmStateChanged ();
	}

}

void
Session::listen_position_changed ()
{
	if (loading ()) {
		/* skip duing session restore (already taken care of) */
		return;
	}
	ProcessorChangeBlocker pcb (this);
	boost::shared_ptr<RouteList> r = routes.reader ();
	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		(*i)->listen_position_changed ();
	}
}

void
Session::solo_control_mode_changed ()
{
	if (soloing() || listening()) {
		if (loading()) {
			/* We can't use ::clear_all_solo_state() here because during
			   session loading at program startup, that will queue a call
			   to rt_clear_all_solo_state() that will not execute until
			   AFTER solo states have been established (thus throwing away
			   the session's saved solo state). So just explicitly turn
			   them all off.
			*/
			set_controls (route_list_to_control_list (get_routes(), &Stripable::solo_control), 0.0, Controllable::NoGroup);
		} else {
			clear_all_solo_state (get_routes());
		}
	}
}

/** Called when a property of one of our route groups changes */
void
Session::route_group_property_changed (RouteGroup* rg)
{
	RouteGroupPropertyChanged (rg); /* EMIT SIGNAL */
}

/** Called when a route is added to one of our route groups */
void
Session::route_added_to_route_group (RouteGroup* rg, boost::weak_ptr<Route> r)
{
	RouteAddedToRouteGroup (rg, r);
}

/** Called when a route is removed from one of our route groups */
void
Session::route_removed_from_route_group (RouteGroup* rg, boost::weak_ptr<Route> r)
{
	update_route_record_state ();
	RouteRemovedFromRouteGroup (rg, r); /* EMIT SIGNAL */

	if (!rg->has_control_master () && !rg->has_subgroup () && rg->empty()) {
		remove_route_group (*rg);
	}
}

boost::shared_ptr<AudioTrack>
Session::get_nth_audio_track (int nth) const
{
	boost::shared_ptr<RouteList> rl = routes.reader ();
	rl->sort (Stripable::Sorter ());

	for (RouteList::const_iterator r = rl->begin(); r != rl->end(); ++r) {
		if (!boost::dynamic_pointer_cast<AudioTrack> (*r)) {
			continue;
		}

		if (--nth > 0) {
			continue;
		}
		return boost::dynamic_pointer_cast<AudioTrack> (*r);
	}
	return boost::shared_ptr<AudioTrack> ();
}

boost::shared_ptr<RouteList>
Session::get_tracks () const
{
	boost::shared_ptr<RouteList> rl = routes.reader ();
	boost::shared_ptr<RouteList> tl (new RouteList);

	for (RouteList::const_iterator r = rl->begin(); r != rl->end(); ++r) {
		if (boost::dynamic_pointer_cast<Track> (*r)) {
			assert (!(*r)->is_auditioner()); // XXX remove me
			tl->push_back (*r);
		}
	}
	return tl;
}

boost::shared_ptr<RouteList>
Session::get_routes_with_regions_at (samplepos_t const p) const
{
	boost::shared_ptr<RouteList> r = routes.reader ();
	boost::shared_ptr<RouteList> rl (new RouteList);

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);
		if (!tr) {
			continue;
		}

		boost::shared_ptr<Playlist> pl = tr->playlist ();
		if (!pl) {
			continue;
		}

		if (pl->has_region_at (p)) {
			rl->push_back (*i);
		}
	}

	return rl;
}

void
Session::goto_end ()
{
	if (_session_range_location) {
		request_locate (_session_range_location->end(), MustStop);
	} else {
		request_locate (0, MustStop);
	}
}

void
Session::goto_start (bool and_roll)
{
	if (_session_range_location) {
		request_locate (_session_range_location->start(), and_roll ? MustRoll : RollIfAppropriate);
	} else {
		request_locate (0, and_roll ? MustRoll : RollIfAppropriate);
	}
}

samplepos_t
Session::current_start_sample () const
{
	return _session_range_location ? _session_range_location->start() : 0;
}

samplepos_t
Session::current_end_sample () const
{
	return _session_range_location ? _session_range_location->end() : 0;
}

void
Session::step_edit_status_change (bool yn)
{
	bool send = false;

	bool val = false;
	if (yn) {
		send = (_step_editors == 0);
		val = true;

		_step_editors++;
	} else {
		send = (_step_editors == 1);
		val = false;

		if (_step_editors > 0) {
			_step_editors--;
		}
	}

	if (send) {
		StepEditStatusChange (val);
	}
}


void
Session::start_time_changed (samplepos_t old)
{
	/* Update the auto loop range to match the session range
	   (unless the auto loop range has been changed by the user)
	*/

	Location* s = _locations->session_range_location ();
	if (s == 0) {
		return;
	}

	Location* l = _locations->auto_loop_location ();

	if (l && l->start() == old) {
		l->set_start (s->start(), true);
	}
	set_dirty ();
}

void
Session::end_time_changed (samplepos_t old)
{
	/* Update the auto loop range to match the session range
	   (unless the auto loop range has been changed by the user)
	*/

	Location* s = _locations->session_range_location ();
	if (s == 0) {
		return;
	}

	Location* l = _locations->auto_loop_location ();

	if (l && l->end() == old) {
		l->set_end (s->end(), true);
	}
	set_dirty ();
}

std::vector<std::string>
Session::source_search_path (DataType type) const
{
	Searchpath sp;

	if (session_dirs.size() == 1) {
		switch (type) {
		case DataType::AUDIO:
			sp.push_back (_session_dir->sound_path());
			break;
		case DataType::MIDI:
			sp.push_back (_session_dir->midi_path());
			break;
		}
	} else {
		for (vector<space_and_path>::const_iterator i = session_dirs.begin(); i != session_dirs.end(); ++i) {
			SessionDirectory sdir (i->path);
			switch (type) {
			case DataType::AUDIO:
				sp.push_back (sdir.sound_path());
				break;
			case DataType::MIDI:
				sp.push_back (sdir.midi_path());
				break;
			}
		}
	}

	if (type == DataType::AUDIO) {
		const string sound_path_2X = _session_dir->sound_path_2X();
		if (Glib::file_test (sound_path_2X, Glib::FILE_TEST_EXISTS|Glib::FILE_TEST_IS_DIR)) {
			if (find (sp.begin(), sp.end(), sound_path_2X) == sp.end()) {
				sp.push_back (sound_path_2X);
			}
		}
	}

	// now check the explicit (possibly user-specified) search path

	switch (type) {
	case DataType::AUDIO:
		sp += Searchpath(config.get_audio_search_path ());
		break;
	case DataType::MIDI:
		sp += Searchpath(config.get_midi_search_path ());
		break;
	}

	return sp;
}

void
Session::ensure_search_path_includes (const string& path, DataType type)
{
	Searchpath sp;

	if (path == ".") {
		return;
	}

	switch (type) {
	case DataType::AUDIO:
		sp += Searchpath(config.get_audio_search_path ());
		break;
	case DataType::MIDI:
		sp += Searchpath (config.get_midi_search_path ());
		break;
	}

	for (vector<std::string>::iterator i = sp.begin(); i != sp.end(); ++i) {
		/* No need to add this new directory if it has the same inode as
		   an existing one; checking inode rather than name prevents duplicated
		   directories when we are using symlinks.

		   On Windows, I think we could just do if (*i == path) here.
		*/
		if (PBD::equivalent_paths (*i, path)) {
			return;
		}
	}

	sp += path;

	switch (type) {
	case DataType::AUDIO:
		config.set_audio_search_path (sp.to_string());
		break;
	case DataType::MIDI:
		config.set_midi_search_path (sp.to_string());
		break;
	}
}

void
Session::remove_dir_from_search_path (const string& dir, DataType type)
{
	Searchpath sp;

	switch (type) {
	case DataType::AUDIO:
		sp = Searchpath(config.get_audio_search_path ());
		break;
	case DataType::MIDI:
		sp = Searchpath (config.get_midi_search_path ());
		break;
	}

	sp -= dir;

	switch (type) {
	case DataType::AUDIO:
		config.set_audio_search_path (sp.to_string());
		break;
	case DataType::MIDI:
		config.set_midi_search_path (sp.to_string());
		break;
	}

}

boost::shared_ptr<Speakers>
Session::get_speakers()
{
	return _speakers;
}

list<string>
Session::unknown_processors () const
{
	list<string> p;

	boost::shared_ptr<RouteList> r = routes.reader ();
	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		list<string> t = (*i)->unknown_processors ();
		copy (t.begin(), t.end(), back_inserter (p));
	}

	p.sort ();
	p.unique ();

	return p;
}

list<string>
Session::missing_filesources (DataType dt) const
{
	list<string> p;
	for (SourceMap::const_iterator i = sources.begin(); i != sources.end(); ++i) {
		if (dt == DataType::AUDIO && 0 != boost::dynamic_pointer_cast<SilentFileSource> (i->second)) {
			p.push_back (i->second->name());
		}
		else if (dt == DataType::MIDI && 0 != boost::dynamic_pointer_cast<SMFSource> (i->second) && (i->second->flags() & Source::Missing) != 0) {
			p.push_back (i->second->name());
		}
	}
	p.sort ();
	return p;
}

void
Session::initialize_latencies ()
{
        {
                Glib::Threads::Mutex::Lock lm (_engine.process_lock());
                update_latency (false);
                update_latency (true);
        }

        set_worst_io_latencies ();
}

void
Session::set_worst_io_latencies_x (IOChange, void *)
{
		set_worst_io_latencies ();
}

void
Session::send_latency_compensation_change ()
{
	/* As a result of Send::set_output_latency()
	 * or InternalReturn::set_playback_offset ()
	 * the send's own latency can change (source track
	 * is aligned with target bus).
	 *
	 * This can only happen be triggered by
	 * Route::update_signal_latency ()
	 * when updating the processor latency.
	 *
	 * We need to walk the graph again to take those changes into account
	 * (we should probably recurse or process the graph in a 2 step process).
	 */
	++_send_latency_changes;
}

bool
Session::update_route_latency (bool playback, bool apply_to_delayline)
{
	/* Note: RouteList is process-graph sorted */
	boost::shared_ptr<RouteList> r = routes.reader ();

	if (playback) {
		/* reverse the list so that we work backwards from the last route to run to the first,
		 * this is not needed, but can help to reduce the iterations for aux-sends.
		 */
		RouteList* rl = routes.reader().get();
		r.reset (new RouteList (*rl));
		reverse (r->begin(), r->end());
	}

	bool changed = false;
	int bailout = 0;
restart:
	_send_latency_changes = 0;
	_worst_route_latency = 0;

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		// if (!(*i)->active()) { continue ; } // TODO
		samplecnt_t l;
		if ((*i)->signal_latency () != (l = (*i)->update_signal_latency (apply_to_delayline))) {
			changed = true;
		}
		_worst_route_latency = std::max (l, _worst_route_latency);
	}

	if (_send_latency_changes > 0) {
		// only 1 extra iteration is needed (we allow only 1 level of aux-sends)
		// BUT..  jack'n'sends'n'bugs
		if (++bailout < 5) {
			cerr << "restarting Session::update_latency. # of send changes: " << _send_latency_changes << " iteration: " << bailout << endl;
			goto restart;
		}
	}

	DEBUG_TRACE (DEBUG::Latency, string_compose ("worst signal processing latency: %1 (changed ? %2)\n", _worst_route_latency, (changed ? "yes" : "no")));

	return changed;
}

void
Session::update_latency (bool playback)
{
	/* called only from AudioEngine::latency_callback.
	 * but may indirectly be triggered from
	 * Session::update_latency_compensation -> _engine.update_latencies
	 */
	DEBUG_TRACE (DEBUG::LatencyCompensation, string_compose ("Engine latency callback: %1 (initial/deletion? %2)\n", (playback ? "PLAYBACK" : "CAPTURE"), inital_connect_or_deletion_in_progress()));

	if (inital_connect_or_deletion_in_progress () || _adding_routes_in_progress || _route_deletion_in_progress) {
		return;
	}
	if (!_engine.running()) {
		return;
	}

	/* Note; RouteList is sorted as process-graph */
	boost::shared_ptr<RouteList> r = routes.reader ();

	if (playback) {
		/* reverse the list so that we work backwards from the last route to run to the first */
		RouteList* rl = routes.reader().get();
		r.reset (new RouteList (*rl));
		reverse (r->begin(), r->end());
	}

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		samplecnt_t latency = (*i)->set_private_port_latencies (playback);
		(*i)->set_public_port_latencies (latency, playback);
	}

	if (playback) {
		Glib::Threads::Mutex::Lock lx (_update_latency_lock);
		set_worst_output_latency ();
		update_route_latency (true, true);
	} else {
		Glib::Threads::Mutex::Lock lx (_update_latency_lock);
		set_worst_input_latency ();
		update_route_latency (false, false);
	}

	DEBUG_TRACE (DEBUG::LatencyCompensation, "Engine latency callback: DONE\n");
	LatencyUpdated (); /* EMIT SIGNAL */
}

void
Session::set_worst_io_latencies ()
{
	DEBUG_TRACE (DEBUG::LatencyCompensation, "Session::set_worst_io_latencies\n");
	Glib::Threads::Mutex::Lock lx (_update_latency_lock);
	set_worst_output_latency ();
	set_worst_input_latency ();
}

void
Session::set_worst_output_latency ()
{
	if (inital_connect_or_deletion_in_progress ()) {
		return;
	}

	_worst_output_latency = 0;

	if (!_engine.running()) {
		return;
	}

	boost::shared_ptr<RouteList> r = routes.reader ();

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		_worst_output_latency = max (_worst_output_latency, (*i)->output()->latency());
	}

	_worst_output_latency = max (_worst_output_latency, _click_io->latency());

	DEBUG_TRACE (DEBUG::LatencyCompensation, string_compose ("Worst output latency: %1\n", _worst_output_latency));
}

void
Session::set_worst_input_latency ()
{
	if (inital_connect_or_deletion_in_progress ()) {
		return;
	}

	_worst_input_latency = 0;

	if (!_engine.running()) {
		return;
	}

	boost::shared_ptr<RouteList> r = routes.reader ();

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		_worst_input_latency = max (_worst_input_latency, (*i)->input()->latency());
	}

	DEBUG_TRACE (DEBUG::LatencyCompensation, string_compose ("Worst input latency: %1\n", _worst_input_latency));
}

void
Session::update_latency_compensation (bool force_whole_graph, bool called_from_backend)
{
	/* Called to update Ardour's internal latency values and compensation
	 * planning. Typically case is from within ::graph_reordered()
	 */

	if (inital_connect_or_deletion_in_progress ()) {
		return;
	}

	/* this lock is not usually contended, but under certain conditions,
	 * update_latency_compensation may be called concurrently.
	 * e.g. drag/drop copy a latent plugin while rolling.
	 * GUI thread (via route_processors_changed) and
	 * auto_connect_thread_run may race.
	 */
	Glib::Threads::Mutex::Lock lx (_update_latency_lock, Glib::Threads::TRY_LOCK);
	if (!lx.locked()) {
		/* no need to do this twice */
		return;
	}

	DEBUG_TRACE (DEBUG::LatencyCompensation, string_compose ("update_latency_compensation %1\n", (force_whole_graph ? "of whole graph" : "")));

	bool some_track_latency_changed = update_route_latency (false, false);

	if (some_track_latency_changed || force_whole_graph)  {

		/* cannot hold lock while engine initiates a full latency callback */

		lx.release ();

		/* next call will ask the backend up update its latencies.
		 *
		 * The semantics of how the backend does this are not well
		 * defined (Oct 2019).
		 *
		 * In all cases, eventually AudioEngine::latency_callback() is
		 * invoked, which will call Session::update_latency().
		 *
		 * Some backends will do that asynchronously with respect to
		 * this call. Others (JACK1) will do so synchronously, and in
		 * those cases this call will return until the backend latency
		 * callback is complete.
		 *
		 * Further, if this is called as part of a backend callback,
		 * then we have to follow the JACK1 rule that we cannot call
		 * back into the backend during such a callback (otherwise
		 * deadlock ensues).
		 */

		if (!called_from_backend) {
			DEBUG_TRACE (DEBUG::LatencyCompensation, "update_latency_compensation: delegate to engine\n");
			_engine.update_latencies ();
		} else {
			DEBUG_TRACE (DEBUG::LatencyCompensation, "update_latency_compensation called from engine, don't call back into engine\n");
		}
	} else {
		DEBUG_TRACE (DEBUG::LatencyCompensation, "update_latency_compensation: directly apply to routes\n");
		boost::shared_ptr<RouteList> r = routes.reader ();
		for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
			(*i)->apply_latency_compensation ();
		}
	}
	DEBUG_TRACE (DEBUG::LatencyCompensation, "update_latency_compensation: DONE\n");
}

char
Session::session_name_is_legal (const string& path)
{
	char illegal_chars[] = { '/', '\\', ':', ';', '\0' };

	for (int i = 0; illegal_chars[i]; ++i) {
		if (path.find (illegal_chars[i]) != string::npos) {
			return illegal_chars[i];
		}
	}

	return 0;
}

void
Session::notify_presentation_info_change ()
{
	if (deletion_in_progress()) {
		return;
	}

	reassign_track_numbers();
}

bool
Session::operation_in_progress (GQuark op) const
{
	return (find (_current_trans_quarks.begin(), _current_trans_quarks.end(), op) != _current_trans_quarks.end());
}

boost::shared_ptr<Port>
Session::ltc_output_port () const
{
	return _ltc_output ? _ltc_output->nth (0) : boost::shared_ptr<Port> ();
}

void
Session::reconnect_ltc_output ()
{
	if (_ltc_output) {

		string src = Config->get_ltc_output_port();

		_ltc_output->disconnect (this);

		if (src != _("None") && !src.empty())  {
			_ltc_output->nth (0)->connect (src);
		}
	}
}

void
Session::set_range_selection (samplepos_t start, samplepos_t end)
{
	_range_selection = Evoral::Range<samplepos_t> (start, end);
}

void
Session::set_object_selection (samplepos_t start, samplepos_t end)
{
	_object_selection = Evoral::Range<samplepos_t> (start, end);
}

void
Session::clear_range_selection ()
{
	_range_selection = Evoral::Range<samplepos_t> (-1,-1);
}

void
Session::clear_object_selection ()
{
	_object_selection = Evoral::Range<samplepos_t> (-1,-1);
}

void
Session::auto_connect_route (boost::shared_ptr<Route> route, bool connect_inputs,
		const ChanCount& input_start,
		const ChanCount& output_start,
		const ChanCount& input_offset,
		const ChanCount& output_offset)
{
	Glib::Threads::Mutex::Lock lx (_auto_connect_queue_lock);
	_auto_connect_queue.push (AutoConnectRequest (route, connect_inputs,
				input_start, output_start,
				input_offset, output_offset));

	auto_connect_thread_wakeup ();
}

void
Session::auto_connect_thread_wakeup ()
{
	if (pthread_mutex_trylock (&_auto_connect_mutex) == 0) {
		pthread_cond_signal (&_auto_connect_cond);
		pthread_mutex_unlock (&_auto_connect_mutex);
	}
}

void
Session::queue_latency_recompute ()
{
	g_atomic_int_inc (&_latency_recompute_pending);
	auto_connect_thread_wakeup ();
}

void
Session::auto_connect (const AutoConnectRequest& ar)
{
	boost::shared_ptr<Route> route = ar.route.lock();

	if (!route) { return; }

	if (!IO::connecting_legal) {
		return;
	}

	/* If both inputs and outputs are auto-connected to physical ports,
	 * use the max of input and output offsets to ensure auto-connected
	 * port numbers always match up (e.g. the first audio input and the
	 * first audio output of the route will have the same physical
	 * port number).  Otherwise just use the lowest input or output
	 * offset possible.
	 */

	const bool in_out_physical =
		(Config->get_input_auto_connect() & AutoConnectPhysical)
		&& (Config->get_output_auto_connect() & AutoConnectPhysical)
		&& ar.connect_inputs;

	const ChanCount in_offset = in_out_physical
		? ChanCount::max(ar.input_offset, ar.output_offset)
		: ar.input_offset;

	const ChanCount out_offset = in_out_physical
		? ChanCount::max(ar.input_offset, ar.output_offset)
		: ar.output_offset;

	for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
		vector<string> physinputs;
		vector<string> physoutputs;


		/* for connecting track inputs we only want MIDI ports marked
		 * for "music".
		 */

		get_physical_ports (physinputs, physoutputs, *t, MidiPortMusic);

		if (!physinputs.empty() && ar.connect_inputs) {
			uint32_t nphysical_in = physinputs.size();

			for (uint32_t i = ar.input_start.get(*t); i < route->n_inputs().get(*t) && i < nphysical_in; ++i) {
				string port;

				if (Config->get_input_auto_connect() & AutoConnectPhysical) {
					port = physinputs[(in_offset.get(*t) + i) % nphysical_in];
				}

				if (!port.empty() && route->input()->connect (route->input()->ports().port(*t, i), port, this)) {
					break;
				}
			}
		}

		if (!physoutputs.empty()) {
			uint32_t nphysical_out = physoutputs.size();
			for (uint32_t i = ar.output_start.get(*t); i < route->n_outputs().get(*t); ++i) {
				string port;

				if ((*t) == DataType::MIDI && (Config->get_output_auto_connect() & AutoConnectPhysical)) {
					port = physoutputs[(out_offset.get(*t) + i) % nphysical_out];
				} else if ((*t) == DataType::AUDIO && (Config->get_output_auto_connect() & AutoConnectMaster)) {
					/* master bus is audio only */
					if (_master_out && _master_out->n_inputs().get(*t) > 0) {
						port = _master_out->input()->ports().port(*t,
								i % _master_out->input()->n_ports().get(*t))->name();
					}
				}

				if (!port.empty() && route->output()->connect (route->output()->ports().port(*t, i), port, this)) {
					break;
				}
			}
		}
	}
}

void
Session::auto_connect_thread_start ()
{
	if (g_atomic_int_get (&_ac_thread_active)) {
		return;
	}

	while (!_auto_connect_queue.empty ()) {
		_auto_connect_queue.pop ();
	}

	g_atomic_int_set (&_ac_thread_active, 1);
	if (pthread_create (&_auto_connect_thread, NULL, auto_connect_thread, this)) {
		g_atomic_int_set (&_ac_thread_active, 0);
	}
}

void
Session::auto_connect_thread_terminate ()
{
	if (!g_atomic_int_get (&_ac_thread_active)) {
		return;
	}

	{
		Glib::Threads::Mutex::Lock lx (_auto_connect_queue_lock);
		while (!_auto_connect_queue.empty ()) {
			_auto_connect_queue.pop ();
		}
	}

	/* cannot use auto_connect_thread_wakeup() because that is allowed to
	 * fail to wakeup the thread.
	 */

	pthread_mutex_lock (&_auto_connect_mutex);
	g_atomic_int_set (&_ac_thread_active, 0);
	pthread_cond_signal (&_auto_connect_cond);
	pthread_mutex_unlock (&_auto_connect_mutex);

	void *status;
	pthread_join (_auto_connect_thread, &status);
}

void *
Session::auto_connect_thread (void *arg)
{
	Session *s = static_cast<Session *>(arg);
	s->auto_connect_thread_run ();
	pthread_exit (0);
	return 0;
}

void
Session::auto_connect_thread_run ()
{
	pthread_set_name (X_("autoconnect"));
	SessionEvent::create_per_thread_pool (X_("autoconnect"), 1024);
	PBD::notify_event_loops_about_thread_creation (pthread_self(), X_("autoconnect"), 1024);
	pthread_mutex_lock (&_auto_connect_mutex);
	while (g_atomic_int_get (&_ac_thread_active)) {

		if (!_auto_connect_queue.empty ()) {
			// Why would we need the process lock ??
			// A: if ports are added while we're connecting, the backend's iterator may be invalidated:
			//   graph_order_callback() -> resort_routes() -> direct_feeds_according_to_reality () -> backend::connected_to()
			//   All ardour-internal backends use a std::vector   xxxAudioBackend::find_port()
			//   We have control over those, but what does jack do?
			Glib::Threads::Mutex::Lock lm (AudioEngine::instance()->process_lock ());

			Glib::Threads::Mutex::Lock lx (_auto_connect_queue_lock);
			while (!_auto_connect_queue.empty ()) {
				const AutoConnectRequest ar (_auto_connect_queue.front());
				_auto_connect_queue.pop ();
				lx.release ();
				auto_connect (ar);
				lx.acquire ();
			}
		}

		if (!actively_recording ()) { // might not be needed,
			/* this is only used for updating plugin latencies, the
			 * graph does not change. so it's safe in general.
			 * BUT..
			 * update_latency_compensation ()
			 * calls DiskWriter::set_capture_offset () which
			 * modifies the capture-offset, which can be a problem.
			 */
			while (g_atomic_int_and (&_latency_recompute_pending, 0)) {
				Glib::Threads::Mutex::Lock lm (AudioEngine::instance()->process_lock ());
				update_latency_compensation (false, false);
			}
		}

		{
			// this may call ARDOUR::Port::drop ... jack_port_unregister ()
			// jack1 cannot cope with removing ports while processing
			Glib::Threads::Mutex::Lock lm (AudioEngine::instance()->process_lock ());
			AudioEngine::instance()->clear_pending_port_deletions ();
		}

		pthread_cond_wait (&_auto_connect_cond, &_auto_connect_mutex);
	}
	pthread_mutex_unlock (&_auto_connect_mutex);
}

void
Session::cancel_all_solo ()
{
	StripableList sl;

	get_stripables (sl);

	set_controls (stripable_list_to_control_list (sl, &Stripable::solo_control), 0.0, Controllable::NoGroup);
	clear_all_solo_state (routes.reader());
}

void
Session::maybe_update_tempo_from_midiclock_tempo (float bpm)
{
	if (_tempo_map->n_tempos() == 1) {
		TempoSection& ts (_tempo_map->tempo_section_at_sample (0));
		if (fabs (ts.note_types_per_minute() - bpm) > (0.01 * ts.note_types_per_minute())) {
			const Tempo tempo (bpm, 4.0, bpm);
			std::cerr << "new tempo " << bpm << " old " << ts.note_types_per_minute() << std::endl;
			_tempo_map->replace_tempo (ts, tempo, 0.0, 0.0, AudioTime);
		}
	}
}
