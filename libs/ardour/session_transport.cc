/*
    Copyright (C) 1999-2003 Paul Davis

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

#ifdef WAF_BUILD
#include "libardour-config.h"
#endif

#include <cmath>
#include <cerrno>
#include <unistd.h>

#include "pbd/undo.h"
#include "pbd/error.h"
#include "pbd/enumwriter.h"
#include "pbd/pthread_utils.h"
#include "pbd/memento_command.h"
#include "pbd/stacktrace.h"

#include "midi++/mmc.h"
#include "midi++/port.h"

#include "ardour/audioengine.h"
#include "ardour/auditioner.h"
#include "ardour/automation_watch.h"
#include "ardour/butler.h"
#include "ardour/click.h"
#include "ardour/debug.h"
#include "ardour/disk_reader.h"
#include "ardour/location.h"
#include "ardour/playlist.h"
#include "ardour/profile.h"
#include "ardour/scene_changer.h"
#include "ardour/session.h"
#include "ardour/slave.h"
#include "ardour/tempo.h"
#include "ardour/operations.h"
#include "ardour/vca.h"
#include "ardour/vca_manager.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

void
Session::add_post_transport_work (PostTransportWork ptw)
{
	PostTransportWork oldval;
	PostTransportWork newval;
	int tries = 0;

	while (tries < 8) {
		oldval = (PostTransportWork) g_atomic_int_get (&_post_transport_work);
		newval = PostTransportWork (oldval | ptw);
		if (g_atomic_int_compare_and_exchange (&_post_transport_work, oldval, newval)) {
			/* success */
			return;
		}
	}

	error << "Could not set post transport work! Crazy thread madness, call the programmers" << endmsg;
}

void
Session::request_sync_source (Slave* new_slave)
{
	SessionEvent* ev = new SessionEvent (SessionEvent::SetSyncSource, SessionEvent::Add, SessionEvent::Immediate, 0, 0.0);
	bool seamless;

	seamless = Config->get_seamless_loop ();

	if (dynamic_cast<Engine_Slave*>(new_slave)) {
		/* JACK cannot support seamless looping at present */
		Config->set_seamless_loop (false);
	} else {
		/* reset to whatever the value was before we last switched slaves */
		Config->set_seamless_loop (_was_seamless);
	}

	/* save value of seamless from before the switch */
	_was_seamless = seamless;

	ev->slave = new_slave;
	DEBUG_TRACE (DEBUG::Slave, "sent request for new slave\n");
	queue_event (ev);
}

void
Session::request_transport_speed (double speed, bool as_default)
{
	SessionEvent* ev = new SessionEvent (SessionEvent::SetTransportSpeed, SessionEvent::Add, SessionEvent::Immediate, 0, speed);
	ev->third_yes_or_no = as_default; // as_default
	DEBUG_TRACE (DEBUG::Transport, string_compose ("Request transport speed = %1 as default = %2\n", speed, as_default));
	queue_event (ev);
}

/** Request a new transport speed, but if the speed parameter is exactly zero then use
 *  a very small +ve value to prevent the transport actually stopping.  This method should
 *  be used by callers who are varying transport speed but don't ever want to stop it.
 */
void
Session::request_transport_speed_nonzero (double speed, bool as_default)
{
	if (speed == 0) {
		speed = DBL_EPSILON;
	}

	request_transport_speed (speed, as_default);
}

void
Session::request_stop (bool abort, bool clear_state)
{
	SessionEvent* ev = new SessionEvent (SessionEvent::SetTransportSpeed, SessionEvent::Add, SessionEvent::Immediate, audible_sample(), 0.0, abort, clear_state);
	DEBUG_TRACE (DEBUG::Transport, string_compose ("Request transport stop, audible %3 transport %4 abort = %1, clear state = %2\n", abort, clear_state, audible_sample(), _transport_sample));
	queue_event (ev);
}

void
Session::request_locate (samplepos_t target_sample, bool with_roll)
{
	SessionEvent *ev = new SessionEvent (with_roll ? SessionEvent::LocateRoll : SessionEvent::Locate, SessionEvent::Add, SessionEvent::Immediate, target_sample, 0, false);
	DEBUG_TRACE (DEBUG::Transport, string_compose ("Request locate to %1\n", target_sample));
	queue_event (ev);
}

void
Session::force_locate (samplepos_t target_sample, bool with_roll)
{
	SessionEvent *ev = new SessionEvent (with_roll ? SessionEvent::LocateRoll : SessionEvent::Locate, SessionEvent::Add, SessionEvent::Immediate, target_sample, 0, true);
	DEBUG_TRACE (DEBUG::Transport, string_compose ("Request forced locate to %1\n", target_sample));
	queue_event (ev);
}

void
Session::unset_preroll_record_trim ()
{
	_preroll_record_trim_len = 0;
}

void
Session::request_preroll_record_trim (samplepos_t rec_in, samplecnt_t preroll)
{
	if (actively_recording ()) {
		return;
	}
	unset_preroll_record_trim ();

	config.set_punch_in (false);
	config.set_punch_out (false);

	samplepos_t pos = std::max ((samplepos_t)0, rec_in - preroll);
	_preroll_record_trim_len = preroll;
	maybe_enable_record ();
	request_locate (pos, true);
	set_requested_return_sample (rec_in);
}

void
Session::request_count_in_record ()
{
	if (actively_recording ()) {
		return;
	}
	if (transport_rolling()) {
		return;
	}
	maybe_enable_record ();
	_count_in_once = true;
	request_transport_speed (1.0, true);
}

void
Session::request_play_loop (bool yn, bool change_transport_roll)
{
	if (_slave && yn) {
		// don't attempt to loop when not using Internal Transport
		// see also gtk2_ardour/ardour_ui_options.cc parameter_changed()
		return;
	}

	SessionEvent* ev;
	Location *location = _locations->auto_loop_location();
	double target_speed;

	if (location == 0 && yn) {
		error << _("Cannot loop - no loop range defined")
		      << endmsg;
		return;
	}

	if (change_transport_roll) {
		if (transport_rolling()) {
			/* start looping at current speed */
			target_speed = transport_speed ();
		} else {
			/* currently stopped */
			if (yn) {
				/* start looping at normal speed */
				target_speed = 1.0;
			} else {
				target_speed = 0.0;
			}
		}
	} else {
		/* leave the speed alone */
		target_speed = transport_speed ();
	}

	ev = new SessionEvent (SessionEvent::SetLoop, SessionEvent::Add, SessionEvent::Immediate, 0, target_speed, yn);
	DEBUG_TRACE (DEBUG::Transport, string_compose ("Request set loop = %1, change roll state ? %2\n", yn, change_transport_roll));
	queue_event (ev);

	if (yn) {
		if (!change_transport_roll) {
			if (!transport_rolling()) {
				/* we're not changing transport state, but we do want
				   to set up position for the new loop. Don't
				   do this if we're rolling already.
				*/
				request_locate (location->start(), false);
			}
		}
	} else {
		if (!change_transport_roll && Config->get_seamless_loop() && transport_rolling()) {
			// request an immediate locate to refresh the tracks
			// after disabling looping
			request_locate (_transport_sample-1, false);
		}
	}
}

void
Session::request_play_range (list<AudioRange>* range, bool leave_rolling)
{
	SessionEvent* ev = new SessionEvent (SessionEvent::SetPlayAudioRange, SessionEvent::Add, SessionEvent::Immediate, 0, (leave_rolling ? 1.0 : 0.0));
	if (range) {
		ev->audio_range = *range;
	} else {
		ev->audio_range.clear ();
	}
	DEBUG_TRACE (DEBUG::Transport, string_compose ("Request play range, leave rolling ? %1\n", leave_rolling));
	queue_event (ev);
}

void
Session::request_cancel_play_range ()
{
	SessionEvent* ev = new SessionEvent (SessionEvent::CancelPlayAudioRange, SessionEvent::Add, SessionEvent::Immediate, 0, 0);
	queue_event (ev);
}


bool
Session::solo_selection_active ()
{
	if ( _soloSelection.empty() ) {
		return false;
	}
	return true;
}

void
Session::solo_selection ( StripableList &list, bool new_state  )
{
	boost::shared_ptr<ControlList> solo_list (new ControlList);
	boost::shared_ptr<ControlList> unsolo_list (new ControlList);

	if (new_state)
		_soloSelection = list;
	else
		_soloSelection.clear();
	
	boost::shared_ptr<RouteList> rl = get_routes();
 
	for (ARDOUR::RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {

		if ( !(*i)->is_track() ) {
			continue;
		}
		
		boost::shared_ptr<Stripable> s (*i);

		bool found = (std::find(list.begin(), list.end(), s) != list.end());
		if ( new_state && found ) {
			
			solo_list->push_back (s->solo_control());
			
			//must invalidate playlists on selected tracks, so only selected regions get heard
			boost::shared_ptr<Track> track = boost::dynamic_pointer_cast<Track> (*i);
			if (track) {
				boost::shared_ptr<Playlist> playlist = track->playlist();
				if (playlist) {
					playlist->ContentsChanged();
				}
			}
		} else {
			unsolo_list->push_back (s->solo_control());
		}
	}

	set_controls (solo_list, 1.0, Controllable::NoGroup);
	set_controls (unsolo_list, 0.0, Controllable::NoGroup);
}

void
Session::realtime_stop (bool abort, bool clear_state)
{
	DEBUG_TRACE (DEBUG::Transport, string_compose ("realtime stop @ %1\n", _transport_sample));
	PostTransportWork todo = PostTransportWork (0);

	/* assume that when we start, we'll be moving forwards */

	if (_transport_speed < 0.0f) {
		todo = (PostTransportWork (todo | PostTransportStop | PostTransportReverse));
		_default_transport_speed = 1.0;
	} else {
		todo = PostTransportWork (todo | PostTransportStop);
	}

	/* call routes */

	boost::shared_ptr<RouteList> r = routes.reader ();

	for (RouteList::iterator i = r->begin (); i != r->end(); ++i) {
		(*i)->realtime_handle_transport_stopped ();
	}

	DEBUG_TRACE (DEBUG::Transport, string_compose ("stop complete, auto-return scheduled for return to %1\n", _requested_return_sample));

	/* the duration change is not guaranteed to have happened, but is likely */

	todo = PostTransportWork (todo | PostTransportDuration);

	if (abort) {
		todo = PostTransportWork (todo | PostTransportAbort);
	}

	if (clear_state) {
		todo = PostTransportWork (todo | PostTransportClearSubstate);
	}

	if (todo) {
		add_post_transport_work (todo);
	}

	_clear_event_type (SessionEvent::StopOnce);
	_clear_event_type (SessionEvent::RangeStop);
	_clear_event_type (SessionEvent::RangeLocate);

	//clear our solo-selection, if there is one
	if ( solo_selection_active() ) {
		solo_selection ( _soloSelection, false );
	}
	
	/* if we're going to clear loop state, then force disabling record BUT only if we're not doing latched rec-enable */
	disable_record (true, (!Config->get_latched_record_enable() && clear_state));

	if (clear_state && !Config->get_loop_is_mode()) {
		unset_play_loop ();
	}

	reset_slave_state ();

	_transport_speed = 0;
	_target_transport_speed = 0;
	_engine_speed = 1.0;

	g_atomic_int_set (&_playback_load, 100);
	g_atomic_int_set (&_capture_load, 100);

	if (config.get_use_video_sync()) {
		waiting_for_sync_offset = true;
	}

	transport_sub_state = 0;
}

void
Session::realtime_locate ()
{
	boost::shared_ptr<RouteList> r = routes.reader ();
	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		(*i)->realtime_locate ();
	}
}

void
Session::butler_transport_work ()
{
	/* Note: this function executes in the butler thread context */

  restart:
	bool finished;
	PostTransportWork ptw;
	boost::shared_ptr<RouteList> r = routes.reader ();
	uint64_t before;

	int on_entry = g_atomic_int_get (&_butler->should_do_transport_work);
	finished = true;
	ptw = post_transport_work();

	DEBUG_TRACE (DEBUG::Transport, string_compose ("Butler transport work, todo = %1 at %2\n", enum_2_string (ptw), (before = g_get_monotonic_time())));


	if (ptw & PostTransportLocate) {

		if (get_play_loop() && !Config->get_seamless_loop()) {

			DEBUG_TRACE (DEBUG::Butler, "flush loop recording fragment to disk\n");

			/* this locate might be happening while we are
			 * loop recording.
			 *
			 * Non-seamless looping will require a locate (below) that
			 * will reset capture buffers and throw away data.
			 *
			 * Rather than first find all tracks and see if they
			 * have outstanding data, just do a flush anyway. It
			 * may be cheaper this way anyway, and is certainly
			 * more accurate.
			 */

			bool more_disk_io_to_do = false;
			uint32_t errors = 0;

			do {
				more_disk_io_to_do = _butler->flush_tracks_to_disk_after_locate (r, errors);

				if (errors) {
					break;
				}

				if (more_disk_io_to_do) {
					continue;
				}

			} while (false);

		}
	}

	if (ptw & PostTransportAdjustPlaybackBuffering) {
		for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
			boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);
			if (tr) {
				tr->adjust_playback_buffering ();
				/* and refill those buffers ... */
			}
			(*i)->non_realtime_locate (_transport_sample);
		}
		VCAList v = _vca_manager->vcas ();
		for (VCAList::const_iterator i = v.begin(); i != v.end(); ++i) {
			(*i)->non_realtime_locate (_transport_sample);
		}
	}

	if (ptw & PostTransportAdjustCaptureBuffering) {
		for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
			boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);
			if (tr) {
				tr->adjust_capture_buffering ();
			}
		}
	}

	if (ptw & PostTransportCurveRealloc) {
		for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
			(*i)->curve_reallocate();
		}
	}

	if (ptw & PostTransportSpeed) {
		non_realtime_set_speed ();
	}

	if (ptw & PostTransportReverse) {

		clear_clicks();
		cumulative_rf_motion = 0;
		reset_rf_scale (0);

		/* don't seek if locate will take care of that in non_realtime_stop() */

		if (!(ptw & PostTransportLocate)) {
			for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
				(*i)->non_realtime_locate (_transport_sample);

				if (on_entry != g_atomic_int_get (&_butler->should_do_transport_work)) {
					/* new request, stop seeking, and start again */
					g_atomic_int_dec_and_test (&_butler->should_do_transport_work);
					goto restart;
				}
			}
			VCAList v = _vca_manager->vcas ();
			for (VCAList::const_iterator i = v.begin(); i != v.end(); ++i) {
				(*i)->non_realtime_locate (_transport_sample);
			}
		}
	}

	if (ptw & PostTransportLocate) {
		DEBUG_TRACE (DEBUG::Transport, "nonrealtime locate invoked from BTW\n");
		non_realtime_locate ();
	}

	if (ptw & PostTransportStop) {
		non_realtime_stop (ptw & PostTransportAbort, on_entry, finished);
		if (!finished) {
			g_atomic_int_dec_and_test (&_butler->should_do_transport_work);
			goto restart;
		}
	}

	if (ptw & PostTransportOverWrite) {
		non_realtime_overwrite (on_entry, finished);
		if (!finished) {
			g_atomic_int_dec_and_test (&_butler->should_do_transport_work);
			goto restart;
		}
	}

	if (ptw & PostTransportAudition) {
		non_realtime_set_audition ();
	}

	g_atomic_int_dec_and_test (&_butler->should_do_transport_work);

	DEBUG_TRACE (DEBUG::Transport, string_compose (X_("Butler transport work all done after %1 usecs @ %2 trw = %3\n"), g_get_monotonic_time() - before, _transport_sample, _butler->transport_work_requested()));
}

void
Session::non_realtime_set_speed ()
{
	boost::shared_ptr<RouteList> rl = routes.reader();
	for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
		boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);
		if (tr) {
			tr->non_realtime_speed_change ();
		}
	}
}

void
Session::non_realtime_overwrite (int on_entry, bool& finished)
{
	boost::shared_ptr<RouteList> rl = routes.reader();
	for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
		boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);
		if (tr && tr->pending_overwrite ()) {
			tr->overwrite_existing_buffers ();
		}
		if (on_entry != g_atomic_int_get (&_butler->should_do_transport_work)) {
			finished = false;
			return;
		}
	}
}


void
Session::non_realtime_locate ()
{
	DEBUG_TRACE (DEBUG::Transport, string_compose ("locate tracks to %1\n", _transport_sample));

	if (Config->get_loop_is_mode() && get_play_loop()) {

		Location *loc  = _locations->auto_loop_location();

		if (!loc || (_transport_sample < loc->start() || _transport_sample >= loc->end())) {
			/* jumped out of loop range: stop tracks from looping,
			   but leave loop (mode) enabled.
			 */
			set_track_loop (false);

		} else if (loc && Config->get_seamless_loop() &&
                   ((loc->start() <= _transport_sample) ||
                   (loc->end() > _transport_sample) ) ) {

			/* jumping to start of loop. This  might have been done before but it is
			 * idempotent and cheap. Doing it here ensures that when we start playback
			 * outside the loop we still flip tracks into the magic seamless mode
			 * when needed.
			 */
			set_track_loop (true);

		} else if (loc) {
			set_track_loop (false);
		}

	} else {

		/* no more looping .. should have been noticed elsewhere */
	}


	samplepos_t tf;

	{
		boost::shared_ptr<RouteList> rl = routes.reader();

	  restart:
		gint sc = g_atomic_int_get (&_seek_counter);
		tf = _transport_sample;

		for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
			(*i)->non_realtime_locate (tf);
			if (sc != g_atomic_int_get (&_seek_counter)) {
				goto restart;
			}
		}
	}

	{
		/* VCAs are quick to locate because they have no data (except
		   automation) associated with them. Don't bother with a
		   restart mechanism here, but do use the same transport sample
		   that the Routes used.
		*/
		VCAList v = _vca_manager->vcas ();
		for (VCAList::const_iterator i = v.begin(); i != v.end(); ++i) {
			(*i)->non_realtime_locate (tf);
		}
	}

	_scene_changer->locate (_transport_sample);

	/* XXX: it would be nice to generate the new clicks here (in the non-RT thread)
	   rather than clearing them so that the RT thread has to spend time constructing
	   them (in Session::click).
	 */
	clear_clicks ();
}

#ifdef USE_TRACKS_CODE_FEATURES
bool
Session::select_playhead_priority_target (samplepos_t& jump_to)
{
	jump_to = -1;

	AutoReturnTarget autoreturn = Config->get_auto_return_target_list ();

	if (!autoreturn) {
		return false;
	}

	if (Profile->get_trx() && transport_rolling() ) {
		// We're playing, so do nothing.
		// Next stop will put us where we need to be.
		return false;
	}

	/* Note that the order of checking each AutoReturnTarget flag defines
	   the priority each flag.

	   Ardour/Mixbus: Last Locate
	                  Range Selection
	                  Loop Range
	                  Region Selection

	   Tracks:        Range Selection
                          Loop Range
                          Region Selection
                          Last Locate
	*/

	if (autoreturn & RangeSelectionStart) {
		if (!_range_selection.empty()) {
			jump_to = _range_selection.from;
		} else {
			if (transport_rolling ()) {
				/* Range selection no longer exists, but we're playing,
				   so do nothing. Next stop will put us where
				   we need to be.
				*/
				return false;
			}
		}
	}

	if (jump_to < 0 && (autoreturn & Loop) && get_play_loop()) {
		/* don't try to handle loop play when synced to JACK */

		if (!synced_to_engine()) {
			Location *location = _locations->auto_loop_location();

			if (location) {
				jump_to = location->start();

				if (Config->get_seamless_loop()) {
					/* need to get track buffers reloaded */
					set_track_loop (true);
				}
			}
		}
	}

	if (jump_to < 0 && (autoreturn & RegionSelectionStart)) {
		if (!_object_selection.empty()) {
			jump_to = _object_selection.from;
		}
	}

	if (jump_to < 0 && (autoreturn & LastLocate)) {
		jump_to = _last_roll_location;
	}

	return jump_to >= 0;
}
#else

bool
Session::select_playhead_priority_target (samplepos_t& jump_to)
{
	if (config.get_external_sync() || !config.get_auto_return()) {
		return false;
	}

	jump_to = _last_roll_location;
	return jump_to >= 0;
}

#endif

void
Session::follow_playhead_priority ()
{
	samplepos_t target;

	if (select_playhead_priority_target (target)) {
		request_locate (target);
	}
}

void
Session::non_realtime_stop (bool abort, int on_entry, bool& finished)
{
	struct tm* now;
	time_t     xnow;
	bool       did_record;
	bool       saved;
	PostTransportWork ptw = post_transport_work();

	did_record = false;
	saved = false;

	boost::shared_ptr<RouteList> rl = routes.reader();
	for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
		boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);
		if (tr && tr->get_captured_samples () != 0) {
			did_record = true;
			break;
		}
	}

	/* stop and locate are merged here because they share a lot of common stuff */

	time (&xnow);
	now = localtime (&xnow);

	if (auditioner) {
		auditioner->cancel_audition ();
	}

	cumulative_rf_motion = 0;
	reset_rf_scale (0);

	if (did_record) {
		begin_reversible_command (Operations::capture);
		_have_captured = true;
	}

	DEBUG_TRACE (DEBUG::Transport, X_("Butler PTW: DS stop\n"));

	if (abort && did_record) {
		/* no reason to save the session file when we remove sources
		 */
		_state_of_the_state = StateOfTheState (_state_of_the_state|InCleanup);
	}

	for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
		boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);
		if (tr) {
			tr->transport_stopped_wallclock (*now, xnow, abort);
		}
	}

	if (abort && did_record) {
		_state_of_the_state = StateOfTheState (_state_of_the_state & ~InCleanup);
	}

	boost::shared_ptr<RouteList> r = routes.reader ();

	if (did_record) {
		commit_reversible_command ();
		/* increase take name */
		if (config.get_track_name_take () && !config.get_take_name ().empty()) {
			string newname = config.get_take_name();
			config.set_take_name(bump_name_number (newname));
		}
	}

	if (_engine.running()) {
		PostTransportWork ptw = post_transport_work ();

		for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
			(*i)->non_realtime_transport_stop (_transport_sample, !(ptw & PostTransportLocate));
		}
		VCAList v = _vca_manager->vcas ();
		for (VCAList::const_iterator i = v.begin(); i != v.end(); ++i) {
			(*i)->non_realtime_transport_stop (_transport_sample, !(ptw & PostTransportLocate));
		}

		update_latency_compensation ();
	}

	bool const auto_return_enabled = (!config.get_external_sync() && (Config->get_auto_return_target_list() || abort));

	if (auto_return_enabled ||
	    (ptw & PostTransportLocate) ||
	    (_requested_return_sample >= 0) ||
	    synced_to_engine()) {

		// rg: what is the logic behind this case?
		// _requested_return_sample should be ignored when synced_to_engine/slaved.
		// currently worked around in MTC_Slave by forcing _requested_return_sample to -1
		// 2016-01-10
		if ((auto_return_enabled || synced_to_engine() || _requested_return_sample >= 0) &&
		    !(ptw & PostTransportLocate)) {

			/* no explicit locate queued */

			bool do_locate = false;

			if (_requested_return_sample >= 0) {

				/* explicit return request pre-queued in event list. overrides everything else */

				_transport_sample = _requested_return_sample;
				do_locate = true;

			} else {
				samplepos_t jump_to;

				if (select_playhead_priority_target (jump_to)) {

					_transport_sample = jump_to;
					do_locate = true;

				} else if (abort) {

					_transport_sample = _last_roll_location;
					do_locate = true;
				}
			}

			_requested_return_sample = -1;

			if (do_locate) {
				_engine.transport_locate (_transport_sample);
			}
		}

	}

	clear_clicks();
	unset_preroll_record_trim ();

	/* do this before seeking, because otherwise the tracks will do the wrong thing in seamless loop mode.
	*/

	if (ptw & PostTransportClearSubstate) {
		unset_play_range ();
		if (!Config->get_loop_is_mode()) {
			unset_play_loop ();
		}
	}

	/* this for() block can be put inside the previous if() and has the effect of ... ??? what */

	{
		DEBUG_TRACE (DEBUG::Transport, X_("Butler PTW: locate\n"));
		for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
			DEBUG_TRACE (DEBUG::Transport, string_compose ("Butler PTW: locate on %1\n", (*i)->name()));
			(*i)->non_realtime_locate (_transport_sample);

			if (on_entry != g_atomic_int_get (&_butler->should_do_transport_work)) {
				finished = false;
				/* we will be back */
				return;
			}
		}
	}

	{
		VCAList v = _vca_manager->vcas ();
		for (VCAList::const_iterator i = v.begin(); i != v.end(); ++i) {
			(*i)->non_realtime_locate (_transport_sample);
		}
	}

	have_looped = false;

	/* don't bother with this stuff if we're disconnected from the engine,
	   because there will be no process callbacks to deliver stuff from
	*/

	if (_engine.connected() && !_engine.freewheeling()) {
		// need to queue this in the next RT cycle
		_send_timecode_update = true;

		if (!dynamic_cast<MTC_Slave*>(_slave)) {
			send_immediate_mmc (MIDI::MachineControlCommand (MIDI::MachineControl::cmdStop));

			/* This (::non_realtime_stop()) gets called by main
			   process thread, which will lead to confusion
			   when calling AsyncMIDIPort::write().

			   Something must be done. XXX
			*/
			send_mmc_locate (_transport_sample);
		}
	}

	if ((ptw & PostTransportLocate) && get_record_enabled()) {
		/* This is scheduled by realtime_stop(), which is also done
		 * when a slave requests /locate/ for an initial sync.
		 * We can't hold up the slave for long with a save() here,
		 * without breaking its initial sync cycle.
		 *
		 * save state only if there's no slave or if it's not yet locked.
		 */
		if (!_slave || !_slave->locked()) {
			DEBUG_TRACE (DEBUG::Transport, X_("Butler PTW: requests save\n"));
			SaveSessionRequested (_current_snapshot_name);
			saved = true;
		}
	}

	/* always try to get rid of this */

	remove_pending_capture_state ();

	/* save the current state of things if appropriate */

	if (did_record && !saved) {
		SaveSessionRequested (_current_snapshot_name);
	}

	if (ptw & PostTransportStop) {
		unset_play_range ();
		if (!Config->get_loop_is_mode()) {
			unset_play_loop ();
		}
	}

	PositionChanged (_transport_sample); /* EMIT SIGNAL */
	DEBUG_TRACE (DEBUG::Transport, string_compose ("send TSC with speed = %1\n", _transport_speed));
	TransportStateChange (); /* EMIT SIGNAL */
	AutomationWatch::instance().transport_stop_automation_watches (_transport_sample);

	/* and start it up again if relevant */

	if ((ptw & PostTransportLocate) && !config.get_external_sync()) {
		request_transport_speed (1.0);
	}
}

void
Session::unset_play_loop ()
{
	if (play_loop) {
		play_loop = false;
		clear_events (SessionEvent::AutoLoop);
		set_track_loop (false);


		if (Config->get_seamless_loop()) {
			/* likely need to flush track buffers: this will locate us to wherever we are */
			add_post_transport_work (PostTransportLocate);
			_butler->schedule_transport_work ();
		}
	}
}

void
Session::set_track_loop (bool yn)
{
	Location* loc = _locations->auto_loop_location ();

	if (!loc) {
		yn = false;
	}

	boost::shared_ptr<RouteList> rl = routes.reader ();

	for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
		if (*i && !(*i)->is_private_route()) {
			(*i)->set_loop (yn ? loc : 0);
		}
	}
}

void
Session::set_play_loop (bool yn, double speed)
{
	/* Called from event-handling context */

	Location *loc;

	if (yn == play_loop || (actively_recording() && yn) || (loc = _locations->auto_loop_location()) == 0) {
		/* nothing to do, or can't change loop status while recording */
		return;
	}

	if (yn && Config->get_seamless_loop() && synced_to_engine()) {
		warning << string_compose (
			_("Seamless looping cannot be supported while %1 is using JACK transport.\n"
			  "Recommend changing the configured options"), PROGRAM_NAME)
			<< endmsg;
		return;
	}

	if (yn) {

		play_loop = true;
		have_looped = false;

		if (loc) {

			unset_play_range ();

			if (Config->get_seamless_loop()) {
				if (!Config->get_loop_is_mode()) {
					/* set all tracks to use internal looping */
					set_track_loop (true);
				} else {
					/* we will do this in the locate to the start OR when we hit the end
					 * of the loop for the first time
					 */
				}
			} else {
				/* set all tracks to NOT use internal looping */
				set_track_loop (false);
			}

			/* Put the delick and loop events in into the event list.  The declick event will
			   cause a de-clicking fade-out just before the end of the loop, and it will also result
			   in a fade-in when the loop restarts.  The AutoLoop event will peform the actual loop.
			*/

			samplepos_t dcp;
			samplecnt_t dcl;
			auto_loop_declick_range (loc, dcp, dcl);
			merge_event (new SessionEvent (SessionEvent::AutoLoop, SessionEvent::Replace, loc->end(), loc->start(), 0.0f));

			/* if requested to roll, locate to start of loop and
			 * roll but ONLY if we're not already rolling.

			   args: positition, roll=true, flush=true, with_loop=false, force buffer refill if seamless looping
			*/

			if (Config->get_loop_is_mode()) {
				/* loop IS a transport mode: if already
				   rolling, do not locate to loop start.
				*/
				if (!transport_rolling() && (speed != 0.0)) {
					start_locate (loc->start(), true, true, false, true);
				}
			} else {
				if (speed != 0.0) {
					start_locate (loc->start(), true, true, false, true);
				}
			}
		}

	} else {

		unset_play_loop ();
	}

	DEBUG_TRACE (DEBUG::Transport, string_compose ("send TSC2 with speed = %1\n", _transport_speed));
	TransportStateChange ();
}
void
Session::flush_all_inserts ()
{
	boost::shared_ptr<RouteList> r = routes.reader ();

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		(*i)->flush_processors ();
	}
}

void
Session::start_locate (samplepos_t target_sample, bool with_roll, bool with_flush, bool for_loop_enabled, bool force)
{
	if (target_sample < 0) {
		error << _("Locate called for negative sample position - ignored") << endmsg;
		return;
	}

	if (synced_to_engine()) {

		double sp;
		samplepos_t pos;

		_slave->speed_and_position (sp, pos);

		if (target_sample != pos) {

			if (config.get_jack_time_master()) {
				/* actually locate now, since otherwise jack_timebase_callback
				   will use the incorrect _transport_sample and report an old
				   and incorrect time to Jack transport
				*/
				locate (target_sample, with_roll, with_flush, for_loop_enabled, force);
			}

			/* tell JACK to change transport position, and we will
			   follow along later in ::follow_slave()
			*/

			_engine.transport_locate (target_sample);

			if (sp != 1.0f && with_roll) {
				_engine.transport_start ();
			}

		}

	} else {
		locate (target_sample, with_roll, with_flush, for_loop_enabled, force);
	}
}

samplecnt_t
Session::worst_latency_preroll () const
{
	return _worst_output_latency + _worst_input_latency;
}

int
Session::micro_locate (samplecnt_t distance)
{
	boost::shared_ptr<RouteList> rl = routes.reader();
	for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
		boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);
		if (tr && !tr->can_internal_playback_seek (distance)) {
			return -1;
		}
	}

	for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
		boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);
		if (tr) {
			tr->internal_playback_seek (distance);
		}
	}

	_transport_sample += distance;
	return 0;
}

/** @param with_mmc true to send a MMC locate command when the locate is done */
void
Session::locate (samplepos_t target_sample, bool with_roll, bool with_flush, bool for_loop_enabled, bool force, bool with_mmc)
{
	bool need_butler = false;

	/* Locates for seamless looping are fairly different from other
	 * locates. They assume that the diskstream buffers for each track
	 * already have the correct data in them, and thus there is no need to
	 * actually tell the tracks to locate. What does need to be done,
	 * though, is all the housekeeping that is associated with non-linear
	 * changes in the value of _transport_sample.
	 */

	DEBUG_TRACE (DEBUG::Transport, string_compose ("rt-locate to %1, roll %2 flush %3 loop-enabled %4 force %5 mmc %6\n",
	                                               target_sample, with_roll, with_flush, for_loop_enabled, force, with_mmc));

	if (!force && _transport_sample == target_sample && !loop_changing && !for_loop_enabled) {

		/* already at the desired position. Not forced to locate,
		   the loop isn't changing, so unless we're told to
		   start rolling also, there's nothing to do but
		   tell the world where we are (again).
		*/

		if (with_roll) {
			set_transport_speed (1.0, 0, false);
		}
		loop_changing = false;
		Located (); /* EMIT SIGNAL */
		return;
	}

	cerr << "... now doing the actual locate\n";

	// Update Timecode time
	_transport_sample = target_sample;
	// Bump seek counter so that any in-process locate in the butler
	// thread(s?) can restart.
	g_atomic_int_inc (&_seek_counter);
	_last_roll_or_reversal_location = target_sample;
	_remaining_latency_preroll = worst_latency_preroll ();
	timecode_time(_transport_sample, transmitting_timecode_time); // XXX here?

	/* do "stopped" stuff if:
	 *
	 * we are rolling AND
	 * no autoplay in effect AND
	 * we're not going to keep rolling after the locate AND
	 * !(playing a loop with JACK sync)
	 *
	 */

	bool transport_was_stopped = !transport_rolling();

	if (!transport_was_stopped && (!auto_play_legal || !config.get_auto_play()) && !with_roll && !(synced_to_engine() && play_loop) &&
	    (!Profile->get_trx() || !(config.get_external_sync() && !synced_to_engine()))) {
		realtime_stop (false, true); // XXX paul - check if the 2nd arg is really correct
		transport_was_stopped = true;
	} else {
		/* otherwise tell the world that we located */
		realtime_locate ();
	}

	if (force || !for_loop_enabled || loop_changing) {

		PostTransportWork todo = PostTransportLocate;

		if (with_roll && transport_was_stopped) {
			todo = PostTransportWork (todo | PostTransportRoll);
		}

		add_post_transport_work (todo);
		need_butler = true;

	} else {

		/* this is functionally what clear_clicks() does but with a tentative lock */

		Glib::Threads::RWLock::WriterLock clickm (click_lock, Glib::Threads::TRY_LOCK);

		if (clickm.locked()) {

			for (Clicks::iterator i = clicks.begin(); i != clicks.end(); ++i) {
				delete *i;
			}

			clicks.clear ();
		}
	}

	if (with_roll) {
		/* switch from input if we're going to roll */
		if (Config->get_monitoring_model() == HardwareMonitoring) {
			set_track_monitor_input_status (!config.get_auto_input());
		}
	} else {
		/* otherwise we're going to stop, so do the opposite */
		if (Config->get_monitoring_model() == HardwareMonitoring) {
			set_track_monitor_input_status (true);
		}
	}

	/* cancel looped playback if transport pos outside of loop range */
	if (play_loop) {

		Location* al = _locations->auto_loop_location();

		if (al) {
			if (_transport_sample < al->start() || _transport_sample >= al->end()) {

				// located outside the loop: cancel looping directly, this is called from event handling context

				have_looped = false;

				if (!Config->get_loop_is_mode()) {
					set_play_loop (false, _transport_speed);
				} else {
					if (Config->get_seamless_loop()) {
						/* this will make the non_realtime_locate() in the butler
						   which then causes seek() in tracks actually do the right
						   thing.
						*/
						set_track_loop (false);
					}
				}

			} else if (_transport_sample == al->start()) {

				// located to start of loop - this is looping, basically

				if (!have_looped) {
					/* first time */
					if (_last_roll_location != al->start()) {
						/* didn't start at loop start - playback must have
						 * started before loop since we've now hit the loop
						 * end.
						 */
						add_post_transport_work (PostTransportLocate);
						need_butler = true;
					}

				}

				boost::shared_ptr<RouteList> rl = routes.reader();

				for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
					boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);

					if (tr && tr->rec_enable_control()->get_value()) {
						// tell it we've looped, so it can deal with the record state
						tr->transport_looped (_transport_sample);
					}
				}

				have_looped = true;
				TransportLooped(); // EMIT SIGNAL
			}
		}
	}

	if (need_butler) {
		_butler->schedule_transport_work ();
	}

	loop_changing = false;

	_send_timecode_update = true;

	if (with_mmc) {
		send_mmc_locate (_transport_sample);
	}

	_last_roll_location = _last_roll_or_reversal_location =  _transport_sample;
	if (!synced_to_engine () || _transport_sample == _engine.transport_sample ()) {
		Located (); /* EMIT SIGNAL */
	}
}

/** Set the transport speed.
 *  Called from the process thread.
 *  @param speed New speed
 */
void
Session::set_transport_speed (double speed, samplepos_t destination_sample, bool abort, bool clear_state, bool as_default)
{
	DEBUG_TRACE (DEBUG::Transport, string_compose ("@ %5 Set transport speed to %1, abort = %2 clear_state = %3, current = %4 as_default %6\n",
						       speed, abort, clear_state, _transport_speed, _transport_sample, as_default));

	/* max speed is somewhat arbitrary but based on guestimates regarding disk i/o capability
	   and user needs. We really need CD-style "skip" playback for ffwd and rewind.
	*/

	if (speed > 0) {
		speed = min (8.0, speed);
	} else if (speed < 0) {
		speed = max (-8.0, speed);
	}

	double new_engine_speed = 1.0;
	if (speed != 0) {
		new_engine_speed = fabs (speed);
		if (speed < 0) speed = -1;
		if (speed > 0) speed = 1;
	}

	if (_transport_speed == speed && new_engine_speed == _engine_speed) {
		if (as_default && speed == 0.0) { // => reset default transport speed. hacky or what?
			_default_transport_speed = 1.0;
		}
		return;
	}

#if 0 // TODO pref: allow vari-speed recording
	if (actively_recording() && speed != 1.0 && speed != 0.0) {
		/* no varispeed during recording */
		DEBUG_TRACE (DEBUG::Transport, string_compose ("No varispeed during recording cur_speed %1, sample %2\n",
						       _transport_speed, _transport_sample));
		return;
	}
#endif

	_target_transport_speed = fabs(speed);
	_engine_speed = new_engine_speed;

	if (transport_rolling() && speed == 0.0) {

		/* we are rolling and we want to stop */

		if (Config->get_monitoring_model() == HardwareMonitoring) {
			set_track_monitor_input_status (true);
		}

		if (synced_to_engine ()) {
			if (clear_state) {
				/* do this here because our response to the slave won't
				   take care of it.
				*/
				_play_range = false;
				_count_in_once = false;
				unset_play_loop ();
			}
			_engine.transport_stop ();
		} else {
			bool const auto_return_enabled = (!config.get_external_sync() && (Config->get_auto_return_target_list() || abort));

			if (!auto_return_enabled) {
				_requested_return_sample = destination_sample;
			}

			stop_transport (abort);
		}

	} else if (transport_stopped() && speed == 1.0) {
		if (as_default) {
			_default_transport_speed = speed;
		}
		/* we are stopped and we want to start rolling at speed 1 */

		if (Config->get_loop_is_mode() && play_loop) {

			Location *location = _locations->auto_loop_location();

			if (location != 0) {
				if (_transport_sample != location->start()) {

					if (Config->get_seamless_loop()) {
						/* force tracks to do their thing */
						set_track_loop (true);
					}

					/* jump to start and then roll from there */

					request_locate (location->start(), true);
					return;
				}
			}
		}

		if (Config->get_monitoring_model() == HardwareMonitoring && config.get_auto_input()) {
			set_track_monitor_input_status (false);
		}

		if (synced_to_engine()) {
			_engine.transport_start ();
			_count_in_once = false;
		} else {
			start_transport ();
		}

	} else {

		/* not zero, not 1.0 ... varispeed */

		// TODO handled transport start..  _remaining_latency_preroll
		// and reversal of playback direction.

		if ((synced_to_engine()) && speed != 0.0 && speed != 1.0) {
			warning << string_compose (
				_("Global varispeed cannot be supported while %1 is connected to JACK transport control"),
				PROGRAM_NAME)
				<< endmsg;
			return;
		}

#if 0
		if (actively_recording()) {
			return;
		}
#endif

		if (speed > 0.0 && _transport_sample == current_end_sample()) {
			return;
		}

		if (speed < 0.0 && _transport_sample == 0) {
			return;
		}

		clear_clicks ();

		/* if we are reversing relative to the current speed, or relative to the speed
		   before the last stop, then we have to do extra work.
		*/

		PostTransportWork todo = PostTransportWork (0);

		if ((_transport_speed && speed * _transport_speed < 0.0) || (_last_transport_speed * speed < 0.0) || (_last_transport_speed == 0.0 && speed < 0.0)) {
			todo = PostTransportWork (todo | PostTransportReverse);
			_last_roll_or_reversal_location = _transport_sample;
		}

		_last_transport_speed = _transport_speed;
		_transport_speed = speed;

		if (as_default) {
			_default_transport_speed = speed;
		}

		if (todo) {
			add_post_transport_work (todo);
			_butler->schedule_transport_work ();
		}

		DEBUG_TRACE (DEBUG::Transport, string_compose ("send TSC3 with speed = %1\n", _transport_speed));

		/* throttle signal emissions.
		 * when slaved [_last]_transport_speed
		 * usually changes every cycle (tiny amounts due to DLL).
		 * Emitting a signal every cycle is overkill and unwarranted.
		 *
		 * Using _last_transport_speed is not acceptable,
		 * since it allows for large changes over a long period
		 * of time. Hence we introduce a dedicated variable to keep track
		 *
		 * The 0.2% dead-zone is somewhat arbitrary. Main use-case
		 * for TransportStateChange() here is the ShuttleControl display.
		 */
		if (fabs (_signalled_varispeed - actual_speed ()) > .002
		    // still, signal hard changes to 1.0 and 0.0:
		    || (actual_speed () == 1.0 && _signalled_varispeed != 1.0)
		    || (actual_speed () == 0.0 && _signalled_varispeed != 0.0)
		   )
		{
			TransportStateChange (); /* EMIT SIGNAL */
			_signalled_varispeed = actual_speed ();
		}
	}
}


/** Stop the transport.  */
void
Session::stop_transport (bool abort, bool clear_state)
{
	_count_in_once = false;
	if (_transport_speed == 0.0f) {
		return;
	}

	DEBUG_TRACE (DEBUG::Transport, "time to actually stop\n");

	realtime_stop (abort, clear_state);
	_butler->schedule_transport_work ();
}

/** Called from the process thread */
void
Session::start_transport ()
{
	DEBUG_TRACE (DEBUG::Transport, "start_transport\n");

	_last_roll_location = _transport_sample;
	_last_roll_or_reversal_location = _transport_sample;
	_remaining_latency_preroll = worst_latency_preroll ();

	have_looped = false;

	/* if record status is Enabled, move it to Recording. if its
	   already Recording, move it to Disabled.
	*/

	switch (record_status()) {
	case Enabled:
		if (!config.get_punch_in()) {
			/* This is only for UIs (keep blinking rec-en before
			 * punch-in, don't show rec-region etc). The UI still
			 * depends on SessionEvent::PunchIn and ensuing signals.
			 *
			 * The disk-writers handle punch in/out internally
			 * in their local delay-compensated timeframe.
			 */
			enable_record ();
		}
		break;

	case Recording:
		if (!play_loop) {
			disable_record (false);
		}
		break;

	default:
		break;
	}

	_transport_speed = _default_transport_speed;
	_target_transport_speed = _transport_speed;

	if (!_engine.freewheeling()) {
		Timecode::Time time;
		timecode_time_subframes (_transport_sample, time);
		if (!dynamic_cast<MTC_Slave*>(_slave)) {
			send_immediate_mmc (MIDI::MachineControlCommand (MIDI::MachineControl::cmdDeferredPlay));
		}

		if (actively_recording() && click_data && (config.get_count_in () || _count_in_once)) {
			_count_in_once = false;
			/* calculate count-in duration (in audio samples)
			 * - use [fixed] tempo/meter at _transport_sample
			 * - calc duration of 1 bar + time-to-beat before or at transport_sample
			 */
			const Tempo& tempo = _tempo_map->tempo_at_sample (_transport_sample);
			const Meter& meter = _tempo_map->meter_at_sample (_transport_sample);

			const double num = meter.divisions_per_bar ();
			const double den = meter.note_divisor ();
			const double barbeat = _tempo_map->exact_qn_at_sample (_transport_sample, 0) * den / (4. * num);
			const double bar_fract = fmod (barbeat, 1.0); // fraction of bar elapsed.

			_count_in_samples = meter.samples_per_bar (tempo, _current_sample_rate);

			double dt = _count_in_samples / num;
			if (bar_fract == 0) {
				/* at bar boundary, count-in 2 bars before start. */
				_count_in_samples *= 2;
			} else {
				/* beats left after full bar until roll position */
				_count_in_samples *= 1. + bar_fract;
			}

			if (_count_in_samples > _remaining_latency_preroll) {
				_remaining_latency_preroll = _count_in_samples;
			}

			int clickbeat = 0;
			samplepos_t cf = _transport_sample - _count_in_samples;
			samplecnt_t offset = _click_io->connected_latency (true);
			while (cf < _transport_sample + offset) {
				add_click (cf, clickbeat == 0);
				cf += dt;
				clickbeat = fmod (clickbeat + 1, num);
			}

			if (_count_in_samples < _remaining_latency_preroll) {
				_count_in_samples = _remaining_latency_preroll;
			}
		}
	}

	DEBUG_TRACE (DEBUG::Transport, string_compose ("send TSC4 with speed = %1\n", _transport_speed));
	TransportStateChange (); /* EMIT SIGNAL */
}

/** Do any transport work in the audio thread that needs to be done after the
 * transport thread is finished.  Audio thread, realtime safe.
 */
void
Session::post_transport ()
{
	PostTransportWork ptw = post_transport_work ();

	if (ptw & PostTransportAudition) {
		if (auditioner && auditioner->auditioning()) {
			process_function = &Session::process_audition;
		} else {
			process_function = &Session::process_with_events;
		}
	}

	if (ptw & PostTransportStop) {

		transport_sub_state = 0;
	}

	if (ptw & PostTransportLocate) {

		if (((!config.get_external_sync() && (auto_play_legal && config.get_auto_play())) && !_exporting) || (ptw & PostTransportRoll)) {
			_count_in_once = false;
			start_transport ();
		} else {
			transport_sub_state = 0;
		}
	}

	set_next_event ();
	/* XXX is this really safe? shouldn't we just be unsetting the bits that we actually
	   know were handled ?
	*/
	set_post_transport_work (PostTransportWork (0));
}

void
Session::reset_rf_scale (samplecnt_t motion)
{
	cumulative_rf_motion += motion;

	if (cumulative_rf_motion < 4 * _current_sample_rate) {
		rf_scale = 1;
	} else if (cumulative_rf_motion < 8 * _current_sample_rate) {
		rf_scale = 4;
	} else if (cumulative_rf_motion < 16 * _current_sample_rate) {
		rf_scale = 10;
	} else {
		rf_scale = 100;
	}

	if (motion != 0) {
		set_dirty();
	}
}

void
Session::mtc_status_changed (bool yn)
{
	g_atomic_int_set (&_mtc_active, yn);
	MTCSyncStateChanged( yn );
}

void
Session::ltc_status_changed (bool yn)
{
	g_atomic_int_set (&_ltc_active, yn);
	LTCSyncStateChanged( yn );
}

void
Session::use_sync_source (Slave* new_slave)
{
	/* Runs in process() context */

	bool non_rt_required = false;

	/* XXX this deletion is problematic because we're in RT context */

	delete _slave;
	_slave = new_slave;


	/* slave change, reset any DiskIO block on disk output because it is no
	   longer valid with a new slave.
	*/
	DiskReader::set_no_disk_output (false);

	MTC_Slave* mtc_slave = dynamic_cast<MTC_Slave*>(_slave);
	if (mtc_slave) {
		mtc_slave->ActiveChanged.connect_same_thread (mtc_status_connection, boost::bind (&Session::mtc_status_changed, this, _1));
		MTCSyncStateChanged(mtc_slave->locked() );
	} else {
		if (g_atomic_int_get (&_mtc_active) ){
			g_atomic_int_set (&_mtc_active, 0);
			MTCSyncStateChanged( false );
		}
		mtc_status_connection.disconnect ();
	}

	LTC_Slave* ltc_slave = dynamic_cast<LTC_Slave*> (_slave);
	if (ltc_slave) {
		ltc_slave->ActiveChanged.connect_same_thread (ltc_status_connection, boost::bind (&Session::ltc_status_changed, this, _1));
		LTCSyncStateChanged (ltc_slave->locked() );
	} else {
		if (g_atomic_int_get (&_ltc_active) ){
			g_atomic_int_set (&_ltc_active, 0);
			LTCSyncStateChanged( false );
		}
		ltc_status_connection.disconnect ();
	}

	DEBUG_TRACE (DEBUG::Slave, string_compose ("set new slave to %1\n", _slave));

	// need to queue this for next process() cycle
	_send_timecode_update = true;

	boost::shared_ptr<RouteList> rl = routes.reader();
	for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
		boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);
		if (tr && !tr->is_private_route()) {
			tr->set_slaved (_slave != 0);
		}
	}

	if (non_rt_required) {
		add_post_transport_work (PostTransportSpeed);
		_butler->schedule_transport_work ();
	}

	set_dirty();
}

void
Session::drop_sync_source ()
{
	request_sync_source (0);
}

void
Session::switch_to_sync_source (SyncSource src)
{
	Slave* new_slave;

	DEBUG_TRACE (DEBUG::Slave, string_compose ("Setting up sync source %1\n", enum_2_string (src)));

	switch (src) {
	case MTC:
		if (_slave && dynamic_cast<MTC_Slave*>(_slave)) {
			return;
		}

		try {
			new_slave = new MTC_Slave (*this, *_midi_ports->mtc_input_port());
		}

		catch (failed_constructor& err) {
			return;
		}
		break;

	case LTC:
		if (_slave && dynamic_cast<LTC_Slave*>(_slave)) {
			return;
		}

		try {
			new_slave = new LTC_Slave (*this);
		}

		catch (failed_constructor& err) {
			return;
		}

		break;

	case MIDIClock:
		if (_slave && dynamic_cast<MIDIClock_Slave*>(_slave)) {
			return;
		}

		try {
			new_slave = new MIDIClock_Slave (*this, *_midi_ports->midi_clock_input_port(), 24);
		}

		catch (failed_constructor& err) {
			return;
		}
		break;

	case Engine:
		if (_slave && dynamic_cast<Engine_Slave*>(_slave)) {
			return;
		}

		if (config.get_video_pullup() != 0.0f) {
			return;
		}

		new_slave = new Engine_Slave (*AudioEngine::instance());
		break;

	default:
		new_slave = 0;
		break;
	};

	request_sync_source (new_slave);
}

void
Session::unset_play_range ()
{
	_play_range = false;
	_clear_event_type (SessionEvent::RangeStop);
	_clear_event_type (SessionEvent::RangeLocate);
}

void
Session::set_play_range (list<AudioRange>& range, bool leave_rolling)
{
	SessionEvent* ev;

	/* Called from event-processing context */

	unset_play_range ();

	if (range.empty()) {
		/* _play_range set to false in unset_play_range()
		 */
		if (!leave_rolling) {
			/* stop transport */
			SessionEvent* ev = new SessionEvent (SessionEvent::SetTransportSpeed, SessionEvent::Add, SessionEvent::Immediate, 0, 0.0f, false);
			merge_event (ev);
		}
		return;
	}

	_play_range = true;

	/* cancel loop play */
	unset_play_loop ();

	list<AudioRange>::size_type sz = range.size();

	if (sz > 1) {

		list<AudioRange>::iterator i = range.begin();
		list<AudioRange>::iterator next;

		while (i != range.end()) {

			next = i;
			++next;

			/* locating/stopping is subject to delays for declicking.
			 */

			samplepos_t requested_sample = i->end;

			if (requested_sample > current_block_size) {
				requested_sample -= current_block_size;
			} else {
				requested_sample = 0;
			}

			if (next == range.end()) {
				ev = new SessionEvent (SessionEvent::RangeStop, SessionEvent::Add, requested_sample, 0, 0.0f);
			} else {
				ev = new SessionEvent (SessionEvent::RangeLocate, SessionEvent::Add, requested_sample, (*next).start, 0.0f);
			}

			merge_event (ev);

			i = next;
		}

	} else if (sz == 1) {

		ev = new SessionEvent (SessionEvent::RangeStop, SessionEvent::Add, range.front().end, 0, 0.0f);
		merge_event (ev);

	}

	/* save range so we can do auto-return etc. */

	current_audio_range = range;

	/* now start rolling at the right place */

	ev = new SessionEvent (SessionEvent::LocateRoll, SessionEvent::Add, SessionEvent::Immediate, range.front().start, 0.0f, false);
	merge_event (ev);

	DEBUG_TRACE (DEBUG::Transport, string_compose ("send TSC5 with speed = %1\n", _transport_speed));
	TransportStateChange ();
}

void
Session::request_bounded_roll (samplepos_t start, samplepos_t end)
{
	AudioRange ar (start, end, 0);
	list<AudioRange> lar;

	lar.push_back (ar);
	request_play_range (&lar, true);
}

void
Session::set_requested_return_sample (samplepos_t return_to)
{
	_requested_return_sample = return_to;
}

void
Session::request_roll_at_and_return (samplepos_t start, samplepos_t return_to)
{
	SessionEvent *ev = new SessionEvent (SessionEvent::LocateRollLocate, SessionEvent::Add, SessionEvent::Immediate, return_to, 1.0);
	ev->target2_sample = start;
	queue_event (ev);
}

void
Session::engine_halted ()
{
	bool ignored;

	/* there will be no more calls to process(), so
	   we'd better clean up for ourselves, right now.

	   but first, make sure the butler is out of
	   the picture.
	*/

	if (_butler) {
		_butler->stop ();
	}

	realtime_stop (false, true);
	non_realtime_stop (false, 0, ignored);
	transport_sub_state = 0;

	DEBUG_TRACE (DEBUG::Transport, string_compose ("send TSC6 with speed = %1\n", _transport_speed));
	TransportStateChange (); /* EMIT SIGNAL */
}


void
Session::xrun_recovery ()
{
	++_xrun_count;

	Xrun (_transport_sample); /* EMIT SIGNAL */

	if (Config->get_stop_recording_on_xrun() && actively_recording()) {

		/* it didn't actually halt, but we need
		   to handle things in the same way.
		*/

		engine_halted();
	}
}

void
Session::route_processors_changed (RouteProcessorChange c)
{
	if (g_atomic_int_get (&_ignore_route_processor_changes) > 0) {
		return;
	}

	if (c.type == RouteProcessorChange::MeterPointChange) {
		set_dirty ();
		return;
	}

	if (c.type == RouteProcessorChange::RealTimeChange) {
		set_dirty ();
		return;
	}

	update_latency_compensation ();
	resort_routes ();

	set_dirty ();
}

void
Session::allow_auto_play (bool yn)
{
	auto_play_legal = yn;
}

bool
Session::maybe_stop (samplepos_t limit)
{
	if ((_transport_speed > 0.0f && _transport_sample >= limit) || (_transport_speed < 0.0f && _transport_sample == 0)) {
		if (synced_to_engine () && config.get_jack_time_master ()) {
			_engine.transport_stop ();
		} else if (!synced_to_engine ()) {
			stop_transport ();
		}
		return true;
	}
	return false;
}

void
Session::send_mmc_locate (samplepos_t t)
{
	if (t < 0) {
		return;
	}

	if (!_engine.freewheeling()) {
		Timecode::Time time;
		timecode_time_subframes (t, time);
		send_immediate_mmc (MIDI::MachineControlCommand (time));
	}
}

/** Ask the transport to not send timecode until further notice.  The suspension
 *  will come into effect some finite time after this call, and timecode_transmission_suspended()
 *  should be checked by the caller to find out when.
 */
void
Session::request_suspend_timecode_transmission ()
{
	SessionEvent* ev = new SessionEvent (SessionEvent::SetTimecodeTransmission, SessionEvent::Add, SessionEvent::Immediate, 0, 0, false);
	queue_event (ev);
}

void
Session::request_resume_timecode_transmission ()
{
	SessionEvent* ev = new SessionEvent (SessionEvent::SetTimecodeTransmission, SessionEvent::Add, SessionEvent::Immediate, 0, 0, true);
	queue_event (ev);
}

bool
Session::timecode_transmission_suspended () const
{
	return g_atomic_int_get (&_suspend_timecode_transmission) == 1;
}
