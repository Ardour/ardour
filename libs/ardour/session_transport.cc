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

#include "midi++/mmc.h"
#include "midi++/port.h"
#include "midi++/manager.h"

#include "ardour/audioengine.h"
#include "ardour/auditioner.h"
#include "ardour/butler.h"
#include "ardour/click.h"
#include "ardour/debug.h"
#include "ardour/location.h"
#include "ardour/session.h"
#include "ardour/slave.h"
#include "ardour/operations.h"

#include "i18n.h"

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
Session::request_input_change_handling ()
{
	if (!(_state_of_the_state & (InitialConnecting|Deletion))) {
		SessionEvent* ev = new SessionEvent (SessionEvent::InputConfigurationChange, SessionEvent::Add, SessionEvent::Immediate, 0, 0.0);
		queue_event (ev);
	}
}

void
Session::request_sync_source (Slave* new_slave)
{
	SessionEvent* ev = new SessionEvent (SessionEvent::SetSyncSource, SessionEvent::Add, SessionEvent::Immediate, 0, 0.0);
	bool seamless;

	seamless = Config->get_seamless_loop ();

	if (dynamic_cast<JACK_Slave*>(new_slave)) {
		/* JACK cannot support seamless looping at present */
		Config->set_seamless_loop (false);
	} else {
		/* reset to whatever the value was before we last switched slaves */
		Config->set_seamless_loop (_was_seamless);
	}

	/* save value of seamless from before the switch */
	_was_seamless = seamless;

	ev->slave = new_slave;
	queue_event (ev);
}

void
Session::request_transport_speed (double speed)
{
	SessionEvent* ev = new SessionEvent (SessionEvent::SetTransportSpeed, SessionEvent::Add, SessionEvent::Immediate, 0, speed);
	DEBUG_TRACE (DEBUG::Transport, string_compose ("Request transport speed = %1\n", speed));
	queue_event (ev);
}

/** Request a new transport speed, but if the speed parameter is exactly zero then use
 *  a very small +ve value to prevent the transport actually stopping.  This method should
 *  be used by callers who are varying transport speed but don't ever want to stop it.
 */
void
Session::request_transport_speed_nonzero (double speed)
{
	if (speed == 0) {
		speed = DBL_EPSILON;
	}

	request_transport_speed (speed);
}

void
Session::request_track_speed (Track* tr, double speed)
{
	SessionEvent* ev = new SessionEvent (SessionEvent::SetTrackSpeed, SessionEvent::Add, SessionEvent::Immediate, 0, speed);
	ev->set_ptr (tr);
	queue_event (ev);
}

void
Session::request_stop (bool abort, bool clear_state)
{
	SessionEvent* ev = new SessionEvent (SessionEvent::SetTransportSpeed, SessionEvent::Add, SessionEvent::Immediate, 0, 0.0, abort, clear_state);
	DEBUG_TRACE (DEBUG::Transport, string_compose ("Request transport stop, abort = %1, clear state = %2\n", abort, clear_state));
	queue_event (ev);
}

void
Session::request_locate (framepos_t target_frame, bool with_roll)
{
	SessionEvent *ev = new SessionEvent (with_roll ? SessionEvent::LocateRoll : SessionEvent::Locate, SessionEvent::Add, SessionEvent::Immediate, target_frame, 0, false);
	DEBUG_TRACE (DEBUG::Transport, string_compose ("Request locate to %1\n", target_frame));
	queue_event (ev);
}

void
Session::force_locate (framepos_t target_frame, bool with_roll)
{
	SessionEvent *ev = new SessionEvent (with_roll ? SessionEvent::LocateRoll : SessionEvent::Locate, SessionEvent::Add, SessionEvent::Immediate, target_frame, 0, true);
	DEBUG_TRACE (DEBUG::Transport, string_compose ("Request forced locate to %1\n", target_frame));
	queue_event (ev);
}

void
Session::request_play_loop (bool yn, bool leave_rolling)
{
	SessionEvent* ev;
	Location *location = _locations->auto_loop_location();

	if (location == 0 && yn) {
		error << _("Cannot loop - no loop range defined")
		      << endmsg;
		return;
	}

	ev = new SessionEvent (SessionEvent::SetLoop, SessionEvent::Add, SessionEvent::Immediate, 0, (leave_rolling ? 1.0 : 0.0), yn);
	DEBUG_TRACE (DEBUG::Transport, string_compose ("Request set loop = %1, leave rolling ? %2\n", yn, leave_rolling));
	queue_event (ev);

	if (!leave_rolling && !yn && Config->get_seamless_loop() && transport_rolling()) {
		// request an immediate locate to refresh the tracks
		// after disabling looping
		request_locate (_transport_frame-1, false);
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
Session::realtime_stop (bool abort, bool clear_state)
{
	DEBUG_TRACE (DEBUG::Transport, string_compose ("realtime stop @ %1\n", _transport_frame));
	PostTransportWork todo = PostTransportWork (0);

	/* assume that when we start, we'll be moving forwards */

	if (_transport_speed < 0.0f) {
		todo = (PostTransportWork (todo | PostTransportStop | PostTransportReverse));
	} else {
		todo = PostTransportWork (todo | PostTransportStop);
	}

	/* call routes */

	boost::shared_ptr<RouteList> r = routes.reader ();

	for (RouteList::iterator i = r->begin (); i != r->end(); ++i) {
		(*i)->realtime_handle_transport_stopped ();
	}

	if (actively_recording()) {

		/* move the transport position back to where the
		   request for a stop was noticed. we rolled
		   past that point to pick up delayed input (and/or to delick)
		*/

		if (worst_playback_latency() > current_block_size) {
			/* we rolled past the stop point to pick up data that had
			   not yet arrived. move back to where the stop occured.
			*/
			decrement_transport_position (current_block_size + (worst_input_latency() - current_block_size));
		} else {
			decrement_transport_position (current_block_size);
		}

		/* the duration change is not guaranteed to have happened, but is likely */

		todo = PostTransportWork (todo | PostTransportDuration);
	}

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

	/* if we're going to clear loop state, then force disabling record BUT only if we're not doing latched rec-enable */
	disable_record (true, (!Config->get_latched_record_enable() && clear_state));

	reset_slave_state ();

	_transport_speed = 0;
	_target_transport_speed = 0;

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
  restart:
	bool finished;
	PostTransportWork ptw;
	boost::shared_ptr<RouteList> r = routes.reader ();

	int on_entry = g_atomic_int_get (&_butler->should_do_transport_work);
	finished = true;
	ptw = post_transport_work();

	DEBUG_TRACE (DEBUG::Transport, string_compose ("Butler transport work, todo = %1\n", enum_2_string (ptw)));

	if (ptw & PostTransportAdjustPlaybackBuffering) {
		for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
			boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);
			if (tr) {
				tr->adjust_playback_buffering ();
				/* and refill those buffers ... */
				tr->non_realtime_locate (_transport_frame);
			}
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

	if (ptw & PostTransportInputChange) {
		for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
			boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);
			if (tr) {
				tr->non_realtime_input_change ();
			}
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
				boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);
				if (tr && !tr->hidden()) {
					tr->non_realtime_locate (_transport_frame);
				}
				if (on_entry != g_atomic_int_get (&_butler->should_do_transport_work)) {
					/* new request, stop seeking, and start again */
					g_atomic_int_dec_and_test (&_butler->should_do_transport_work);
					goto restart;
				}
			}
		}
	}

	if (ptw & PostTransportLocate) {
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

	DEBUG_TRACE (DEBUG::Transport, X_("Butler transport work all done\n"));
}

void
Session::non_realtime_set_speed ()
{
	boost::shared_ptr<RouteList> rl = routes.reader();
	for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
		boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);
		if (tr) {
			tr->non_realtime_set_speed ();
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
	boost::shared_ptr<RouteList> rl = routes.reader();
	for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
		boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);
		if (tr) {
			tr->non_realtime_locate (_transport_frame);
		}
	}

	/* XXX: it would be nice to generate the new clicks here (in the non-RT thread)
	   rather than clearing them so that the RT thread has to spend time constructing
	   them (in Session::click).
	 */
	clear_clicks ();
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
		if (tr && tr->get_captured_frames () != 0) {
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

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		if (!(*i)->is_hidden()) {
			(*i)->set_pending_declick (0);
		}
	}

	if (did_record) {
		commit_reversible_command ();
	}

	if (_engine.running()) {
		PostTransportWork ptw = post_transport_work ();
		for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
			(*i)->nonrealtime_handle_transport_stopped (abort, (ptw & PostTransportLocate), (!(ptw & PostTransportLocate) || pending_locate_flush));
		}
		update_latency_compensation ();
	}

	bool const auto_return_enabled =
		(!config.get_external_sync() && config.get_auto_return());

	if (auto_return_enabled ||
	    (ptw & PostTransportLocate) ||
	    (_requested_return_frame >= 0) ||
	    synced_to_jack()) {

		if (pending_locate_flush) {
			flush_all_inserts ();
		}

		if ((auto_return_enabled || synced_to_jack() || _requested_return_frame >= 0) &&
		    !(ptw & PostTransportLocate)) {

			/* no explicit locate queued */

			bool do_locate = false;

			if (_requested_return_frame >= 0) {

				/* explicit return request pre-queued in event list. overrides everything else */

				cerr << "explicit auto-return to " << _requested_return_frame << endl;

				_transport_frame = _requested_return_frame;
				do_locate = true;

			} else {
				if (config.get_auto_return()) {

					if (play_loop) {

						/* don't try to handle loop play when synced to JACK */

						if (!synced_to_jack()) {

							Location *location = _locations->auto_loop_location();

							if (location != 0) {
								_transport_frame = location->start();
							} else {
								_transport_frame = _last_roll_location;
							}
							do_locate = true;
						}

					} else if (_play_range) {

						/* return to start of range */

						if (!current_audio_range.empty()) {
							_transport_frame = current_audio_range.front().start;
							do_locate = true;
						}

					} else {

						/* regular auto-return */

						_transport_frame = _last_roll_location;
						do_locate = true;
					}
				}
			}

			_requested_return_frame = -1;

			if (do_locate) {
				_engine.transport_locate (_transport_frame);
			}
		}

	}

	clear_clicks();

	/* do this before seeking, because otherwise the tracks will do the wrong thing in seamless loop mode.
	*/

	if (ptw & PostTransportClearSubstate) {
		_play_range = false;
		unset_play_loop ();
	}

	/* this for() block can be put inside the previous if() and has the effect of ... ??? what */

	DEBUG_TRACE (DEBUG::Transport, X_("Butler PTW: locate\n"));
	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		DEBUG_TRACE (DEBUG::Transport, string_compose ("Butler PTW: locate on %1\n", (*i)->name()));
		boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);
		if (tr && !tr->hidden()) {
			tr->non_realtime_locate (_transport_frame);
		}

		if (on_entry != g_atomic_int_get (&_butler->should_do_transport_work)) {
			finished = false;
			/* we will be back */
			return;
		}
	}

	have_looped = false;

	if (!_engine.freewheeling()) {
		send_full_time_code (_transport_frame);
		
		if (!dynamic_cast<MTC_Slave*>(_slave)) {
			MIDI::Manager::instance()->mmc()->send (MIDI::MachineControlCommand (MIDI::MachineControl::cmdStop));
			send_mmc_locate (_transport_frame);
		}
	}

	if ((ptw & PostTransportLocate) && get_record_enabled()) {
		/* capture start has been changed, so save pending state */
		save_state ("", true);
		saved = true;
	}

	/* always try to get rid of this */

	remove_pending_capture_state ();

	/* save the current state of things if appropriate */

	if (did_record && !saved) {
		save_state (_current_snapshot_name);
	}

	if (ptw & PostTransportStop) {
		_play_range = false;
		play_loop = false;
	}

	PositionChanged (_transport_frame); /* EMIT SIGNAL */
	TransportStateChange (); /* EMIT SIGNAL */

	/* and start it up again if relevant */

	if ((ptw & PostTransportLocate) && !config.get_external_sync() && pending_locate_roll) {
		request_transport_speed (1.0);
		pending_locate_roll = false;
	}
}

void
Session::check_declick_out ()
{
	bool locate_required = transport_sub_state & PendingLocate;

	/* this is called after a process() iteration. if PendingDeclickOut was set,
	   it means that we were waiting to declick the output (which has just been
	   done) before doing something else. this is where we do that "something else".

	   note: called from the audio thread.
	*/

	if (transport_sub_state & PendingDeclickOut) {

		if (locate_required) {
			start_locate (pending_locate_frame, pending_locate_roll, pending_locate_flush);
			transport_sub_state &= ~(PendingDeclickOut|PendingLocate);
		} else {
			stop_transport (pending_abort);
			transport_sub_state &= ~(PendingDeclickOut|PendingLocate);
		}
	}
}

void
Session::unset_play_loop ()
{
	play_loop = false;
	clear_events (SessionEvent::AutoLoop);

	// set all tracks to NOT use internal looping
	boost::shared_ptr<RouteList> rl = routes.reader ();
	for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
		boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);
		if (tr && !tr->hidden()) {
			tr->set_loop (0);
		}
	}
}

void
Session::set_play_loop (bool yn)
{
	/* Called from event-handling context */

	Location *loc;

	if (yn == play_loop || (actively_recording() && yn) || (loc = _locations->auto_loop_location()) == 0) {
		/* nothing to do, or can't change loop status while recording */
		return;
	}

	if (yn && Config->get_seamless_loop() && synced_to_jack()) {
		warning << string_compose (
			_("Seamless looping cannot be supported while %1 is using JACK transport.\n"
			  "Recommend changing the configured options"), PROGRAM_NAME)
			<< endmsg;
		return;
	}

	if (yn) {

		play_loop = true;

		if (loc) {

			unset_play_range ();

			if (Config->get_seamless_loop()) {
				// set all tracks to use internal looping
				boost::shared_ptr<RouteList> rl = routes.reader ();
				for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
					boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);
					if (tr && !tr->hidden()) {
						tr->set_loop (loc);
					}
				}
			}
			else {
				// set all tracks to NOT use internal looping
				boost::shared_ptr<RouteList> rl = routes.reader ();
				for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
					boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);
					if (tr && !tr->hidden()) {
						tr->set_loop (0);
					}
				}
			}

			/* put the loop event into the event list */

			SessionEvent* event = new SessionEvent (SessionEvent::AutoLoop, SessionEvent::Replace, loc->end(), loc->start(), 0.0f);
			merge_event (event);

			/* locate to start of loop and roll. If doing seamless loop, force a
			   locate+buffer refill even if we are positioned there already.
			*/

			start_locate (loc->start(), true, true, false, Config->get_seamless_loop());
		}

	} else {

		unset_play_loop ();
	}

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
Session::start_locate (framepos_t target_frame, bool with_roll, bool with_flush, bool with_loop, bool force)
{
	if (synced_to_jack()) {

		double sp;
		framepos_t pos;

		_slave->speed_and_position (sp, pos);

		if (target_frame != pos) {

			/* tell JACK to change transport position, and we will
			   follow along later in ::follow_slave()
			*/

			_engine.transport_locate (target_frame);

			if (sp != 1.0f && with_roll) {
				_engine.transport_start ();
			}

		}

	} else {
		locate (target_frame, with_roll, with_flush, with_loop, force);
	}
}

int
Session::micro_locate (framecnt_t distance)
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

	_transport_frame += distance;
	return 0;
}

/** @param with_mmc true to send a MMC locate command when the locate is done */
void
Session::locate (framepos_t target_frame, bool with_roll, bool with_flush, bool with_loop, bool force, bool with_mmc)
{
	if (actively_recording() && !with_loop) {
		return;
	}

	if (!force && _transport_frame == target_frame && !loop_changing && !with_loop) {
		if (with_roll) {
			set_transport_speed (1.0, false);
		}
		loop_changing = false;
		Located (); /* EMIT SIGNAL */
		return;
	}

	if (_transport_speed) {
		/* schedule a declick. we'll be called again when its done */

		if (!(transport_sub_state & PendingDeclickOut)) {
			transport_sub_state |= (PendingDeclickOut|PendingLocate);
			pending_locate_frame = target_frame;
			pending_locate_roll = with_roll;
			pending_locate_flush = with_flush;
			return;
		}
	}

	// Update Timecode time
	// [DR] FIXME: find out exactly where this should go below
	_transport_frame = target_frame;
	timecode_time(_transport_frame, transmitting_timecode_time);
	outbound_mtc_timecode_frame = _transport_frame;
	next_quarter_frame_to_send = 0;

	/* do "stopped" stuff if:
	 *
	 * we are rolling AND
	 *    no autoplay in effect AND
         *       we're not going to keep rolling after the locate AND
         *           !(playing a loop with JACK sync)
         *
	 */

	if (transport_rolling() && (!auto_play_legal || !config.get_auto_play()) && !with_roll && !(synced_to_jack() && play_loop)) {
		realtime_stop (false, true); // XXX paul - check if the 2nd arg is really correct
	} else {
		/* otherwise tell the world that we located */
		realtime_locate ();
	}

	if (force || !with_loop || loop_changing) {

		PostTransportWork todo = PostTransportLocate;

		if (with_roll) {
			todo = PostTransportWork (todo | PostTransportRoll);
		}

		add_post_transport_work (todo);
		_butler->schedule_transport_work ();

	} else {

		/* this is functionally what clear_clicks() does but with a tentative lock */

		Glib::RWLock::WriterLock clickm (click_lock, Glib::TRY_LOCK);

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

		if (al && (_transport_frame < al->start() || _transport_frame > al->end())) {
			// cancel looping directly, this is called from event handling context
			set_play_loop (false);
		}
		else if (al && _transport_frame == al->start()) {
			if (with_loop) {
				// this is only necessary for seamless looping

				boost::shared_ptr<RouteList> rl = routes.reader();
				for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
					boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);
					if (tr && tr->record_enabled ()) {
						// tell it we've looped, so it can deal with the record state
						tr->transport_looped(_transport_frame);
					}
				}
			}
			have_looped = true;
			TransportLooped(); // EMIT SIGNAL
		}
	}

	loop_changing = false;

	_send_timecode_update = true;

	if (with_mmc) {
		send_mmc_locate (_transport_frame);
	}

	Located (); /* EMIT SIGNAL */
}

/** Set the transport speed.
 *  Called from the process thread.
 *  @param speed New speed
 */
void
Session::set_transport_speed (double speed, bool abort, bool clear_state)
{
	DEBUG_TRACE (DEBUG::Transport, string_compose ("@ %5 Set transport speed to %1, abort = %2 clear_state = %3, current = %4\n", 
						       speed, abort, clear_state, _transport_speed, _transport_frame));

	if (_transport_speed == speed) {
		return;
	}

	if (actively_recording() && speed != 1.0 && speed != 0.0) {
		/* no varispeed during recording */
		return;
	}

	_target_transport_speed = fabs(speed);

	/* 8.0 max speed is somewhat arbitrary but based on guestimates regarding disk i/o capability
	   and user needs. We really need CD-style "skip" playback for ffwd and rewind.
	*/

	if (speed > 0) {
		speed = min (8.0, speed);
	} else if (speed < 0) {
		speed = max (-8.0, speed);
	}

	if (transport_rolling() && speed == 0.0) {

		/* we are rolling and we want to stop */

		if (Config->get_monitoring_model() == HardwareMonitoring) {
			set_track_monitor_input_status (true);
		}

		if (synced_to_jack ()) {
			if (clear_state) {
				/* do this here because our response to the slave won't
				   take care of it.
				*/
				_play_range = false;
				unset_play_loop ();
			}
			_engine.transport_stop ();
		} else {
			stop_transport (abort);
		}

		unset_play_loop ();

	} else if (transport_stopped() && speed == 1.0) {

		/* we are stopped and we want to start rolling at speed 1 */

		if (Config->get_monitoring_model() == HardwareMonitoring && config.get_auto_input()) {
			set_track_monitor_input_status (false);
		}

		if (synced_to_jack()) {
			_engine.transport_start ();
		} else {
			start_transport ();
		}

	} else {

		/* not zero, not 1.0 ... varispeed */

		if ((synced_to_jack()) && speed != 0.0 && speed != 1.0) {
			warning << string_compose (
				_("Global varispeed cannot be supported while %1 is connected to JACK transport control"),
				PROGRAM_NAME)
				<< endmsg;
			return;
		}

		if (actively_recording()) {
			return;
		}

		if (speed > 0.0 && _transport_frame == current_end_frame()) {
			return;
		}

		if (speed < 0.0 && _transport_frame == 0) {
			return;
		}

		clear_clicks ();

		/* if we are reversing relative to the current speed, or relative to the speed
		   before the last stop, then we have to do extra work.
		*/

		PostTransportWork todo = PostTransportWork (0);

		if ((_transport_speed && speed * _transport_speed < 0.0) || (_last_transport_speed * speed < 0.0) || (_last_transport_speed == 0.0f && speed < 0.0f)) {
			todo = PostTransportWork (todo | PostTransportReverse);
			_last_roll_or_reversal_location = _transport_frame;
		}

		_last_transport_speed = _transport_speed;
		_transport_speed = speed;

		boost::shared_ptr<RouteList> rl = routes.reader();
		for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
			boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);
			if (tr && tr->realtime_set_speed (tr->speed(), true)) {
				todo = PostTransportWork (todo | PostTransportSpeed);
			}
		}

		if (todo) {
			add_post_transport_work (todo);
			_butler->schedule_transport_work ();
		}

		TransportStateChange (); /* EMIT SIGNAL */
	}
}


/** Stop the transport.  */
void
Session::stop_transport (bool abort, bool clear_state)
{
	if (_transport_speed == 0.0f) {
		return;
	}

	if (actively_recording() && !(transport_sub_state & StopPendingCapture) && worst_input_latency() > current_block_size) {

		boost::shared_ptr<RouteList> rl = routes.reader();
		for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
			boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);
			if (tr) {
				tr->prepare_to_stop (_transport_frame);
			}
		}

		/* we need to capture the audio that has still not yet been received by the system
		   at the time the stop is requested, so we have to roll past that time.

		   we want to declick before stopping, so schedule the autostop for one
		   block before the actual end. we'll declick in the subsequent block,
		   and then we'll really be stopped.
		*/

		DEBUG_TRACE (DEBUG::Transport, string_compose ("stop transport requested @ %1, scheduled for + %2 - %3 = %4, abort = %5\n",
							       _transport_frame, _worst_input_latency, current_block_size,
							       _transport_frame - _worst_input_latency - current_block_size,
							       abort));

		SessionEvent *ev = new SessionEvent (SessionEvent::StopOnce, SessionEvent::Replace,
		                                     _transport_frame + _worst_input_latency - current_block_size,
		                                     0, 0, abort);

		merge_event (ev);
		transport_sub_state |= StopPendingCapture;
		pending_abort = abort;
		return;
	}

	if ((transport_sub_state & PendingDeclickOut) == 0) {

		if (!(transport_sub_state & StopPendingCapture)) {
			boost::shared_ptr<RouteList> rl = routes.reader();
			for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
				boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);
				if (tr) {
					tr->prepare_to_stop (_transport_frame);
				}
			}
		}

		transport_sub_state |= PendingDeclickOut;
		/* we'll be called again after the declick */
		pending_abort = abort;
		return;
	}

	realtime_stop (abort, clear_state);
	_butler->schedule_transport_work ();
}

/** Called from the process thread */
void
Session::start_transport ()
{
	DEBUG_TRACE (DEBUG::Transport, "start_transport\n");

	_last_roll_location = _transport_frame;
	_last_roll_or_reversal_location = _transport_frame;

	have_looped = false;

	/* if record status is Enabled, move it to Recording. if its
	   already Recording, move it to Disabled.
	*/

	switch (record_status()) {
	case Enabled:
		if (!config.get_punch_in()) {
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

	transport_sub_state |= PendingDeclickIn;

	_transport_speed = 1.0;
	_target_transport_speed = 1.0;

	boost::shared_ptr<RouteList> rl = routes.reader();
	for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
		boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);
		if (tr) {
			tr->realtime_set_speed (tr->speed(), true);
		}
		(*i)->automation_snapshot (_transport_frame, true);
	}

	if (!_engine.freewheeling()) {
		Timecode::Time time;
		timecode_time_subframes (_transport_frame, time);
		if (!dynamic_cast<MTC_Slave*>(_slave)) {
			MIDI::Manager::instance()->mmc()->send (MIDI::MachineControlCommand (MIDI::MachineControl::cmdDeferredPlay));
		}
	}

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
Session::reset_rf_scale (framecnt_t motion)
{
	cumulative_rf_motion += motion;

	if (cumulative_rf_motion < 4 * _current_frame_rate) {
		rf_scale = 1;
	} else if (cumulative_rf_motion < 8 * _current_frame_rate) {
		rf_scale = 4;
	} else if (cumulative_rf_motion < 16 * _current_frame_rate) {
		rf_scale = 10;
	} else {
		rf_scale = 100;
	}

	if (motion != 0) {
		set_dirty();
	}
}

void
Session::use_sync_source (Slave* new_slave)
{
	/* Runs in process() context */

	bool non_rt_required = false;

	/* XXX this deletion is problematic because we're in RT context */

	delete _slave;
	_slave = new_slave;

	send_full_time_code (_transport_frame);

	boost::shared_ptr<RouteList> rl = routes.reader();
	for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
		boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);
		if (tr && !tr->hidden()) {
			if (tr->realtime_set_speed (tr->speed(), true)) {
				non_rt_required = true;
			}
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
			new_slave = new MTC_Slave (*this, *MIDI::Manager::instance()->mtc_input_port());
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
			new_slave = new MIDIClock_Slave (*this, *MIDI::Manager::instance()->midi_clock_input_port(), 24);
		}

		catch (failed_constructor& err) {
			return;
		}
		break;

	case JACK:
		if (_slave && dynamic_cast<JACK_Slave*>(_slave)) {
			return;
		}

		if (config.get_video_pullup() != 0.0f) {
			return;
		}

		new_slave = new JACK_Slave (_engine.jack());
		break;

	default:
		new_slave = 0;
		break;
	};

	request_sync_source (new_slave);
}

void
Session::set_track_speed (Track* track, double speed)
{
	if (track->realtime_set_speed (speed, false)) {
		add_post_transport_work (PostTransportSpeed);
		_butler->schedule_transport_work ();
		set_dirty ();
	}
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

			framepos_t requested_frame = i->end;

			if (requested_frame > current_block_size) {
				requested_frame -= current_block_size;
			} else {
				requested_frame = 0;
			}

			if (next == range.end()) {
				ev = new SessionEvent (SessionEvent::RangeStop, SessionEvent::Add, requested_frame, 0, 0.0f);
			} else {
				ev = new SessionEvent (SessionEvent::RangeLocate, SessionEvent::Add, requested_frame, (*next).start, 0.0f);
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

	TransportStateChange ();
}

void
Session::request_bounded_roll (framepos_t start, framepos_t end)
{
	AudioRange ar (start, end, 0);
	list<AudioRange> lar;

	lar.push_back (ar);
	request_play_range (&lar, true);
}
void
Session::request_roll_at_and_return (framepos_t start, framepos_t return_to)
{
	SessionEvent *ev = new SessionEvent (SessionEvent::LocateRollLocate, SessionEvent::Add, SessionEvent::Immediate, return_to, 1.0);
	ev->target2_frame = start;
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

	g_atomic_int_set (&_butler->should_do_transport_work, 0);
	set_post_transport_work (PostTransportWork (0));
	_butler->stop ();

	realtime_stop (false, true);
	non_realtime_stop (false, 0, ignored);
	transport_sub_state = 0;

	TransportStateChange (); /* EMIT SIGNAL */
}


void
Session::xrun_recovery ()
{
	Xrun (_transport_frame); /* EMIT SIGNAL */

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
	if (ignore_route_processor_changes) {
		return;
	}

	if (c.type == RouteProcessorChange::MeterPointChange) {
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

void
Session::reset_jack_connection (jack_client_t* jack)
{
	JACK_Slave* js;

	if (_slave && ((js = dynamic_cast<JACK_Slave*> (_slave)) != 0)) {
		js->reset_client (jack);
	}
}

bool
Session::maybe_stop (framepos_t limit)
{
	if ((_transport_speed > 0.0f && _transport_frame >= limit) || (_transport_speed < 0.0f && _transport_frame == 0)) {
		if (synced_to_jack () && config.get_jack_time_master ()) {
			_engine.transport_stop ();
		} else if (!synced_to_jack ()) {
			stop_transport ();
		}
		return true;
	}
	return false;
}

void
Session::send_mmc_locate (framepos_t t)
{
	if (!_engine.freewheeling()) {
		Timecode::Time time;
		timecode_time_subframes (t, time);
		MIDI::Manager::instance()->mmc()->send (MIDI::MachineControlCommand (time));
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
