/*
 * Copyright (C) 1999-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005-2009 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006-2007 Jesse Chappell <jesse@essej.net>
 * Copyright (C) 2006-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2008-2009 Hans Baier <hansfbaier@googlemail.com>
 * Copyright (C) 2012-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2014-2018 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2015 GZharun <grygoriiz@wavesglobal.com>
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

#ifdef WAF_BUILD
#include "libardour-config.h"
#endif

#include <cmath>
#include <cerrno>
#include <unistd.h>

#include <boost/algorithm/string/erase.hpp>

#include "pbd/error.h"
#include "pbd/enumwriter.h"
#include "pbd/i18n.h"
#include "pbd/memento_command.h"
#include "pbd/pthread_utils.h"
#include "pbd/stacktrace.h"
#include "pbd/undo.h"

#include "midi++/mmc.h"
#include "midi++/port.h"

#include "ardour/audio_backend.h"
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
#include "ardour/transport_fsm.h"
#include "ardour/transport_master.h"
#include "ardour/transport_master_manager.h"
#include "ardour/tempo.h"
#include "ardour/operations.h"
#include "ardour/vca.h"
#include "ardour/vca_manager.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;


#ifdef NDEBUG
# define ENSURE_PROCESS_THREAD do {} while (0)
#else
# define ENSURE_PROCESS_THREAD                           \
  do {                                                   \
    if (!AudioEngine::instance()->in_process_thread()) { \
      PBD::stacktrace (std::cerr, 30);                   \
    }                                                    \
  } while (0)
#endif


#define TFSM_EVENT(evtype) { _transport_fsm->enqueue (new TransportFSM::Event (evtype)); }
#define TFSM_STOP(abort,clear) { _transport_fsm->enqueue (new TransportFSM::Event (TransportFSM::StopTransport,abort,clear)); }
#define TFSM_LOCATE(target,ltd,flush,loop,force) { _transport_fsm->enqueue (new TransportFSM::Event (TransportFSM::Locate,target,ltd,flush,loop,force)); }

/* *****************************************************************************
 * REALTIME ACTIONS (to be called on state transitions)
 * ****************************************************************************/

void
Session::realtime_stop (bool abort, bool clear_state)
{
	ENSURE_PROCESS_THREAD;

	DEBUG_TRACE (DEBUG::Transport, string_compose ("realtime stop @ %1 speed = %2\n", _transport_sample, _transport_speed));
	PostTransportWork todo = PostTransportWork (0);

	if (_transport_speed < 0.0f) {
		todo = (PostTransportWork (todo | PostTransportStop));
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

	if (abort) {
		todo = PostTransportWork (todo | PostTransportAbort);
	}

	if (clear_state) {
		todo = PostTransportWork (todo | PostTransportClearSubstate);
	}

	if (todo) {
		add_post_transport_work (todo);
	}

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

	reset_punch_loop_constraint ();

	_transport_speed = 0;
	_target_transport_speed = 0;
	_engine_speed = 1.0;

	g_atomic_int_set (&_playback_load, 100);
	g_atomic_int_set (&_capture_load, 100);

	if (config.get_use_video_sync()) {
		waiting_for_sync_offset = true;
	}

	if (todo) {
		TFSM_EVENT (TransportFSM::ButlerRequired);
	}
}

/** @param with_mmc true to send a MMC locate command when the locate is done */
void
Session::locate (samplepos_t target_sample, bool with_roll, bool with_flush, bool for_loop_end, bool force, bool with_mmc)
{
	ENSURE_PROCESS_THREAD;

	if (target_sample < 0) {
		error << _("Locate called for negative sample position - ignored") << endmsg;
		return;
	}

	bool need_butler = false;

	/* Locates for seamless looping are fairly different from other
	 * locates. They assume that the diskstream buffers for each track
	 * already have the correct data in them, and thus there is no need to
	 * actually tell the tracks to locate. What does need to be done,
	 * though, is all the housekeeping that is associated with non-linear
	 * changes in the value of _transport_sample.
	 */

	DEBUG_TRACE (DEBUG::Transport, string_compose ("rt-locate to %1 ts = %7, roll %2 flush %3 for loop end %4 force %5 mmc %6\n",
	                                               target_sample, with_roll, with_flush, for_loop_end, force, with_mmc, _transport_sample));

	if (!force && _transport_sample == target_sample && !loop_changing && !for_loop_end) {

		/* already at the desired position. Not forced to locate,
		   the loop isn't changing, so unless we're told to
		   start rolling also, there's nothing to do but
		   tell the world where we are (again).
		*/

		if (with_roll) {
			set_transport_speed (1.0, false, false, false);
		}
		loop_changing = false;
		TFSM_EVENT (TransportFSM::LocateDone);
		Located (); /* EMIT SIGNAL */
		return;
	}

	// Update Timecode time
	_transport_sample = target_sample;
	_nominal_jack_transport_sample = boost::none;
	// Bump seek counter so that any in-process locate in the butler
	// thread(s?) can restart.
	g_atomic_int_inc (&_seek_counter);
	_last_roll_or_reversal_location = target_sample;
	if (!for_loop_end) {
		_remaining_latency_preroll = worst_latency_preroll_buffer_size_ceil ();
	}
	timecode_time(_transport_sample, transmitting_timecode_time); // XXX here?

	/* do "stopped" stuff if:
	 *
	 * we are rolling AND
	 * no autoplay in effect AND
	 * we're not going to keep rolling after the locate AND
	 * !(playing a loop with JACK sync) AND
	 * we're not synced to an external transport master
	 *
	 */


	/* it is important here that we use the internal state of the transport
	   FSM, not the public facing result of ::transport_rolling()
	*/
	bool transport_was_stopped = !_transport_fsm->rolling();

	if (!transport_was_stopped &&
	    (!auto_play_legal || !config.get_auto_play()) &&
	    !with_roll &&
	    !(synced_to_engine() && get_play_loop ()) &&
	    !(config.get_external_sync() && !synced_to_engine())) {

		realtime_stop (false, true); // XXX paul - check if the 2nd arg is really correct
		transport_was_stopped = true;

	} else {

		/* Tell all routes to do the RT part of locate */

		boost::shared_ptr<RouteList> r = routes.reader ();
		for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
			(*i)->realtime_locate (for_loop_end);
		}
	}

	if (force || !for_loop_end || loop_changing) {

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
	if (get_play_loop ()) {

		Location* al = _locations->auto_loop_location();

		if (al) {
			if (_transport_sample < al->start() || _transport_sample >= al->end()) {

				// located outside the loop: cancel looping directly, this is called from event handling context

				have_looped = false;

				if (!Config->get_loop_is_mode()) {
					set_play_loop (false, false);
				} else {
					/* this will make the non_realtime_locate() in the butler
					   which then causes seek() in tracks actually do the right
					   thing.
					*/
					set_track_loop (false);
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
		TFSM_EVENT (TransportFSM::ButlerRequired);
	} else {
		TFSM_EVENT (TransportFSM::LocateDone);
		loop_changing = false;
	}

	_send_timecode_update = true;

	if (with_mmc) {
		send_mmc_locate (_transport_sample);
	}

	_last_roll_location = _last_roll_or_reversal_location =  _transport_sample;
	if (!synced_to_engine () || _transport_sample == _engine.transport_sample ()) {
		Located (); /* EMIT SIGNAL */
	}
}

void
Session::post_locate ()
{
	if (transport_master_is_external() && !synced_to_engine()) {
		const samplepos_t current_master_position = TransportMasterManager::instance().get_current_position_in_process_context();
		if (abs (current_master_position - _transport_sample) > TransportMasterManager::instance().current()->resolution()) {
			_last_roll_location = _last_roll_or_reversal_location =  _transport_sample;
		}
	}
}

/** Set the transport speed.
 *  Called from the process thread.
 *  @param speed New speed
 */
void
Session::set_transport_speed (double speed, bool abort, bool clear_state, bool as_default)
{
	ENSURE_PROCESS_THREAD;
	DEBUG_TRACE (DEBUG::Transport, string_compose ("@ %5 Set transport speed to %1 from %4 (es = %7), abort = %2 clear_state = %3, as_default %6\n",
	                                               speed, abort, clear_state, _transport_speed, _transport_sample, as_default, _engine_speed));

	if ((_engine_speed != 1) && (_engine_speed == fabs (speed)) && (speed * _transport_speed) >= 0) {
		/* engine speed is not changing and no direction change, do nothing */
		DEBUG_TRACE (DEBUG::Transport, "no reason to change speed, do nothing\n");
		return;
	}

	/* max speed is somewhat arbitrary but based on guestimates regarding disk i/o capability
	   and user needs. XXX We really need CD-style "skip" playback for ffwd and rewind.
	*/

	if (speed > 0) {
		speed = min ((double) Config->get_max_transport_speed(), speed);
	} else if (speed < 0) {
		speed = max ((double) -Config->get_max_transport_speed(), speed);
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
		}

		TFSM_STOP (abort, false);

	} else if (transport_stopped() && speed == 1.0) {

		if (as_default) {
			_default_transport_speed = speed;
		}

		/* we are stopped and we want to start rolling at speed 1 */

		if (Config->get_loop_is_mode() && get_play_loop ()) {

			Location *location = _locations->auto_loop_location();

			if (location != 0) {
				if (_transport_sample != location->start()) {

					/* force tracks to do their thing */
					set_track_loop (true);

					/* jump to start and then roll from there */

					request_locate (location->start(), MustRoll);
					return;
				}
			}
		}

		if (Config->get_monitoring_model() == HardwareMonitoring && config.get_auto_input()) {
			set_track_monitor_input_status (false);
		}

		TFSM_EVENT (TransportFSM::StartTransport);

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

		_transport_speed = speed;

		if (as_default) {
			_default_transport_speed = speed;
		}

		DEBUG_TRACE (DEBUG::Transport, string_compose ("send TSC3 with speed = %1\n", _transport_speed));

		/* throttle signal emissions.
		 * when slaved [_last]_transport_speed
		 * usually changes every cycle (tiny amounts due to DLL).
		 * Emitting a signal every cycle is overkill and unwarranted.
		 *
		 * Using _transport_speed is not acceptable,
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
	ENSURE_PROCESS_THREAD;

	_count_in_once = false;

	DEBUG_TRACE (DEBUG::Transport, string_compose ("time to actually stop with TS @ %1\n", _transport_sample));

	realtime_stop (abort, clear_state);
}

/** Called from the process thread */
void
Session::start_transport ()
{
	ENSURE_PROCESS_THREAD;
	DEBUG_TRACE (DEBUG::Transport, "start_transport\n");

	_last_roll_location = _transport_sample;
	_last_roll_or_reversal_location = _transport_sample;
	if (!have_looped) {
		_remaining_latency_preroll = worst_latency_preroll_buffer_size_ceil ();
	}

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
		if (!get_play_loop ()) {
			disable_record (false);
		}
		break;

	default:
		break;
	}

	maybe_allow_only_loop ();
	maybe_allow_only_punch ();

	_transport_speed = _default_transport_speed;
	_target_transport_speed = _transport_speed;

	if (!_engine.freewheeling()) {
		Timecode::Time time;
		timecode_time_subframes (_transport_sample, time);
		if (transport_master()->type() != MTC) { // why not when slaved to MTC?
			send_immediate_mmc (MIDI::MachineControlCommand (MIDI::MachineControl::cmdDeferredPlay));
		}

		if ((actively_recording () || (config.get_punch_in () && get_record_enabled ()))
		    && click_data && (config.get_count_in () || _count_in_once)) {
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

bool
Session::should_roll_after_locate () const
{
	/* a locate must previously have been requested and completed before
	 * this answer can be considered correct
	 */

	return ((!config.get_external_sync() && (auto_play_legal && config.get_auto_play())) && !_exporting) || (post_transport_work() & PostTransportRoll);

}

/** Do any transport work in the audio thread that needs to be done after the
 * butler thread is finished.  Audio thread, realtime safe.
 */
void
Session::butler_completed_transport_work ()
{
	ENSURE_PROCESS_THREAD;
	PostTransportWork ptw = post_transport_work ();

	DEBUG_TRACE (DEBUG::Transport, string_compose ("Butler done, RT cleanup for %1\n", enum_2_string (ptw)));

	if (ptw & PostTransportAudition) {
		if (auditioner && auditioner->auditioning()) {
			process_function = &Session::process_audition;
		} else {
			process_function = &Session::process_with_events;
		}
		ptw = PostTransportWork (ptw & ~PostTransportAudition);
		set_post_transport_work (ptw);
	}

	if (ptw & PostTransportLocate) {
		post_locate ();
		ptw = PostTransportWork (ptw & ~PostTransportLocate);
		set_post_transport_work (ptw);
		loop_changing = false;
		TFSM_EVENT (TransportFSM::LocateDone);
	}

	bool start_after_butler_done_msg = false;

	if (ptw & PostTransportRoll) {
		start_after_butler_done_msg = true;
	}

	/* the butler finished its work so clear all PostTransportWork flags
	 */

	set_post_transport_work (PostTransportWork (0));

	set_next_event ();

	if (_transport_fsm->waiting_for_butler()) {
		TFSM_EVENT (TransportFSM::ButlerDone);
	}

	DiskReader::dec_no_disk_output ();

	if (start_after_butler_done_msg) {
		if (_transport_speed) {
			/* reversal is done ... tell TFSM that it is time to start*/
			TFSM_EVENT (TransportFSM::StartTransport);
		}
	}
}

void
Session::schedule_butler_for_transport_work ()
{
	assert (_transport_fsm->waiting_for_butler ());
	DEBUG_TRACE (DEBUG::Butler, "summon butler for transport work\n");
	_butler->schedule_transport_work ();
}

bool
Session::maybe_stop (samplepos_t limit)
{
	ENSURE_PROCESS_THREAD;
	if ((_transport_speed > 0.0f && _transport_sample >= limit) || (_transport_speed < 0.0f && _transport_sample == 0)) {
		if (synced_to_engine ()) {
			_engine.transport_stop ();
		} else {
			TFSM_STOP (false, false);
		}
		return true;
	}
	return false;
}

int
Session::micro_locate (samplecnt_t distance)
{
	ENSURE_PROCESS_THREAD;

	boost::shared_ptr<RouteList> rl = routes.reader();
	for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
		boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);
		if (tr && !tr->can_internal_playback_seek (distance)) {
			return -1;
		}
	}

	DEBUG_TRACE (DEBUG::Transport, string_compose ("micro-locate by %1\n", distance));

	for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
		boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);
		if (tr) {
			tr->internal_playback_seek (distance);
		}
	}

	_transport_sample += distance;
	return 0;
}

void
Session::flush_all_inserts ()
{
	ENSURE_PROCESS_THREAD;
	boost::shared_ptr<RouteList> r = routes.reader ();

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		(*i)->flush_processors ();
	}
}

/* *****************************************************************************
 * END REALTIME ACTIONS
 * ****************************************************************************/

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

bool
Session::should_ignore_transport_request (TransportRequestSource src, TransportRequestType type) const
{
	if (config.get_external_sync()) {
		if (TransportMasterManager::instance().current()->allow_request (src, type)) {
			return false;
		} else {
			return true;
		}
	}
	return false;
}

bool
Session::synced_to_engine() const
{
	return config.get_external_sync() && TransportMasterManager::instance().current()->type() == Engine;
}

void
Session::request_sync_source (boost::shared_ptr<TransportMaster> tm)
{
	SessionEvent* ev = new SessionEvent (SessionEvent::SetTransportMaster, SessionEvent::Add, SessionEvent::Immediate, 0, 0.0);
	ev->transport_master = tm;
	DEBUG_TRACE (DEBUG::Slave, "sent request for new transport master\n");
	queue_event (ev);
}

void
Session::request_transport_speed (double speed, bool as_default, TransportRequestSource origin)
{
	if (synced_to_engine()) {
		if (speed != 0) {
			_engine.transport_start ();
		} else {
			_engine.transport_stop ();
		}
		return;
	}

	if (should_ignore_transport_request (origin, TR_Speed)) {
		return;
	}

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
Session::request_transport_speed_nonzero (double speed, bool as_default, TransportRequestSource origin)
{
	if (should_ignore_transport_request (origin, TransportRequestType (TR_Speed|TR_Start))) {
		return;
	}

	if (speed == 0) {
		speed = DBL_EPSILON;
	}

	request_transport_speed (speed, as_default);
}

void
Session::request_stop (bool abort, bool clear_state, TransportRequestSource origin)
{
	if (synced_to_engine()) {
		_engine.transport_stop ();
		return;
	}

	if (should_ignore_transport_request (origin, TR_Stop)) {
		return;
	}

	SessionEvent* ev = new SessionEvent (SessionEvent::SetTransportSpeed, SessionEvent::Add, SessionEvent::Immediate, audible_sample(), 0.0, abort, clear_state);
	DEBUG_TRACE (DEBUG::Transport, string_compose ("Request transport stop, audible %3 transport %4 abort = %1, clear state = %2\n", abort, clear_state, audible_sample(), _transport_sample));
	queue_event (ev);
}

void
Session::request_locate (samplepos_t target_sample, LocateTransportDisposition ltd, TransportRequestSource origin)
{
	if (synced_to_engine()) {
		_engine.transport_locate (target_sample);
		return;
	}

	if (should_ignore_transport_request (origin, TR_Locate)) {
		return;
	}

	SessionEvent::Type type;

	switch (ltd) {
	case MustRoll:
		type = SessionEvent::LocateRoll;
		break;
	case MustStop:
		type = SessionEvent::Locate;
		break;
	case RollIfAppropriate:
		if (config.get_auto_play()) {
			type = SessionEvent::LocateRoll;
		} else {
			type = SessionEvent::Locate;
		}
		break;
	}

	SessionEvent *ev = new SessionEvent (type, SessionEvent::Add, SessionEvent::Immediate, target_sample, 0, false);
	ev->locate_transport_disposition = ltd;
	DEBUG_TRACE (DEBUG::Transport, string_compose ("Request locate to %1 ltd = %2\n", target_sample, enum_2_string (ltd)));
	queue_event (ev);
}

void
Session::force_locate (samplepos_t target_sample, LocateTransportDisposition ltd)
{
	SessionEvent *ev = new SessionEvent (SessionEvent::Locate, SessionEvent::Add, SessionEvent::Immediate, target_sample, 0, true);
	ev->locate_transport_disposition = ltd;
	DEBUG_TRACE (DEBUG::Transport, string_compose ("Request forced locate to %1 roll %2\n", target_sample, enum_2_string (ltd)));
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
	request_locate (pos, MustRoll);
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
	if (transport_master_is_external() && yn) {
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

	ev = new SessionEvent (SessionEvent::SetLoop, SessionEvent::Add, SessionEvent::Immediate, 0, target_speed, yn, change_transport_roll);
	DEBUG_TRACE (DEBUG::Transport, string_compose ("Request set loop = %1, change roll state ? %2\n", yn, change_transport_roll));
	queue_event (ev);
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
	if (_soloSelection.empty()) {
		return false;
	}
	return true;
}

void
Session::solo_selection (StripableList &list, bool new_state)
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
Session::butler_transport_work ()
{
	/* Note: this function executes in the butler thread context */

  restart:
	boost::shared_ptr<RouteList> r = routes.reader ();
	int on_entry = g_atomic_int_get (&_butler->should_do_transport_work);
	bool finished = true;
	PostTransportWork ptw = post_transport_work();
#ifndef NDEBUG
	uint64_t before;
#endif

	DEBUG_TRACE (DEBUG::Transport, string_compose ("Butler transport work, todo = [%1] (0x%3%4%5) at %2\n", enum_2_string (ptw), (before = g_get_monotonic_time()), std::hex, ptw, std::dec));

	if (ptw & PostTransportLocate) {

		if (get_play_loop()) {

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
		/* need to prevent concurrency with ARDOUR::Reader::run(),
		 * DiskWriter::adjust_buffering() re-allocates the ringbuffer */
		Glib::Threads::Mutex::Lock lx (AudioEngine::instance()->process_lock ());
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
		/* need to prevent concurrency with ARDOUR::DiskWriter::run(),
		 * DiskWriter::adjust_buffering() re-allocates the ringbuffer */
		Glib::Threads::Mutex::Lock lx (AudioEngine::instance()->process_lock ());
		for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
			boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);
			if (tr) {
				tr->adjust_capture_buffering ();
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

	DEBUG_TRACE (DEBUG::Transport, string_compose (X_("Butler transport work all done after %1 usecs @ %2 ptw %3 trw = %4\n"), g_get_monotonic_time() - before, _transport_sample, enum_2_string (post_transport_work()), _butler->transport_work_requested()));
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

		} else if (loc && ((loc->start() <= _transport_sample) || (loc->end() > _transport_sample))) {

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
				std::cerr << "\n\nLOCATE INTERRUPTED BY LOCATE!!!\n\n";
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

bool
Session::select_playhead_priority_target (samplepos_t& jump_to)
{
	if (!transport_master_no_external_or_using_engine() || !config.get_auto_return()) {
		return false;
	}

	jump_to = _last_roll_location;
	return jump_to >= 0;
}

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

	if (did_record) {
		begin_reversible_command (Operations::capture);
		_have_captured = true;
	}

	DEBUG_TRACE (DEBUG::Transport, X_("Butler post-transport-work, non realtime stop\n"));

	if (abort && did_record) {
		/* no reason to save the session file when we remove sources
		 */
		_state_of_the_state = StateOfTheState (_state_of_the_state | InCleanup);
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
	}

	/* If we are not synced to a "true" external master, and we're not
	 * handling an explicit locate, we should consider whether or not to
	 * "auto-return". This could mean going to a specifically requested
	 * location, or just back to the start of the last roll.
	 */

	if (transport_master_no_external_or_using_engine() && !locate_initiated()) {

		bool do_locate = false;

		if (_requested_return_sample >= 0) {

			/* explicit return request pre-queued in event list. overrides everything else */

			_transport_sample = _requested_return_sample;

			/* cancel this request */
			_requested_return_sample = -1;
			do_locate = true;

		} else if (Config->get_auto_return_target_list()) {

			samplepos_t jump_to;

			if (select_playhead_priority_target (jump_to)) {

				/* there's a valid target (we don't care how it
				 * was derived here)
				 */

				_transport_sample = jump_to;
				do_locate = true;

			} else if (abort) {

				/* roll aborted (typically capture) with
				 * auto-return enabled
				 */

				_transport_sample = _last_roll_location;
				do_locate = true;

			}
		}


		if (do_locate && synced_to_engine()) {

			/* We will unconditionally locate to _transport_sample
			 * below, which will refill playback buffers based on
			 * _transport_sample, and maximises the buffering they
			 * represent.
			 *
			 * But if we are synced to engine (JACK), we should
			 * locate the engine (JACK) as well. We would follow
			 * the engine (JACK) on the next process cycle, but
			 * since we're going to do a locate below anyway,
			 * it seems pointless to not use just do it ourselves
			 * right now, rather than wait for the engine (JACK) to
			 * provide the new position on the next cycle.
			 *
			 * Despite the generic name of the called method
			 * (::transport_locate()) this method only does
			 * anything if the audio/MIDI backend is JACK.
			 */

			_engine.transport_locate (_transport_sample);

		}
	}

	clear_clicks();
	unset_preroll_record_trim ();

	/* do this before seeking, because otherwise the tracks will do the wrong thing in seamless loop mode.
	*/

	if (ptw & (PostTransportClearSubstate|PostTransportStop)) {
		unset_play_range ();
		if (!loop_changing && !Config->get_loop_is_mode()) {
			unset_play_loop ();
		}
	}

	if (!_transport_fsm->declicking_for_locate()) {

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

		VCAList v = _vca_manager->vcas ();
		for (VCAList::const_iterator i = v.begin(); i != v.end(); ++i) {
			(*i)->non_realtime_locate (_transport_sample);
		}
	}

	have_looped = false;

	/* don't bother with this stuff if we're disconnected from the engine,
	   because there will be no process callbacks to deliver stuff from
	*/

	if (_engine.running() && !_engine.freewheeling()) {
		// need to queue this in the next RT cycle
		_send_timecode_update = true;

		if (transport_master()->type() != MTC) { // why?
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
		if (!transport_master_is_external() || !transport_master()->locked()) {
			DEBUG_TRACE (DEBUG::Transport, X_("Butler PTW: requests save\n"));
			SaveSessionRequested (_current_snapshot_name);
			saved = true;
		}
	}

	/* save the current state of things if appropriate */

	if (did_record && !saved) {
		SaveSessionRequested (_current_snapshot_name);
	}

	PositionChanged (_transport_sample); /* EMIT SIGNAL */
	DEBUG_TRACE (DEBUG::Transport, string_compose ("send TSC with speed = %1\n", _transport_speed));
	TransportStateChange (); /* EMIT SIGNAL */
	AutomationWatch::instance().transport_stop_automation_watches (_transport_sample);
}

void
Session::set_play_loop (bool yn, bool change_transport_state)
{
	ENSURE_PROCESS_THREAD;
	/* Called from event-handling context */

	DEBUG_TRACE (DEBUG::Transport, string_compose ("set_play_loop (%1)\n", yn));

	Location *loc;

	if (yn == get_play_loop () || (actively_recording() && yn) || (loc = _locations->auto_loop_location()) == 0) {
		/* nothing to do, or can't change loop status while recording */
		return;
	}

	if (yn && synced_to_engine()) {
		warning << string_compose (
			_("Looping cannot be supported while %1 is using JACK transport.\n"
			  "Recommend changing the configured options"), PROGRAM_NAME)
			<< endmsg;
		return;
	}

	if (yn && !maybe_allow_only_loop (true)) {
		return;
	}

	if (yn) {

		play_loop = true;
		have_looped = false;

		unset_play_range ();
		/* set all tracks to use internal looping */
		set_track_loop (true);

		merge_event (new SessionEvent (SessionEvent::AutoLoop, SessionEvent::Replace, loc->end(), loc->start(), 0.0f));

		if (!Config->get_loop_is_mode()) {
			/* args: positition, roll=true, flush=true, for_loop_end=false, force buffer, refill  looping */
			/* set this so that when/if we stop for locate,
				 we do not call unset_play_loop(). This is a
				 crude mechanism. Got a better idea?
				 */
			loop_changing = true;
			TFSM_LOCATE (loc->start(), MustRoll, true, false, true);
		} else if (!transport_rolling()) {
			/* loop-is-mode: not rolling, just locate to loop start */
			TFSM_LOCATE (loc->start(), MustStop, true, false, true);
		}
		TransportStateChange (); /* EMIT SIGNAL */
	} else {
		unset_play_loop ();
	}

	DEBUG_TRACE (DEBUG::Transport, string_compose ("send TSC2 with speed = %1\n", _transport_speed));
}

void
Session::unset_play_loop (bool change_transport_state)
{
	if (!get_play_loop()) {
		return;
	}

	play_loop = false;
	clear_events (SessionEvent::AutoLoop);
	set_track_loop (false);

	/* likely need to flush track buffers: this will locate us to wherever we are */

	if (change_transport_state && transport_rolling ()) {
		TFSM_STOP (false, false);
	}

	overwrite_some_buffers (boost::shared_ptr<Route>(), LoopDisabled);
	TransportStateChange (); /* EMIT SIGNAL */
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

	DiskReader::reset_loop_declick (loc, nominal_sample_rate());
}

samplecnt_t
Session::worst_latency_preroll () const
{
	return _worst_output_latency + _worst_input_latency;
}

samplecnt_t
Session::worst_latency_preroll_buffer_size_ceil () const
{
	return lrintf (ceil ((_worst_output_latency + _worst_input_latency) / (float) current_block_size) * current_block_size);
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
	TransportStateChange (); /* EMIT SIGNAL */
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
	/* there will be no more calls to process(), so
	   we'd better clean up for ourselves, right now.

	   We can't queue SessionEvents because they only get
	   handled from within a process callback.
	*/

	/* this just stops the FSM engine ... it doesn't change the state of
	 * the FSM directly or anything else ... but the FSM will be
	 * reinitialized when we call its ::start() method from
	 * ::engine_running() (if we ever get there)
	 */

	_transport_fsm->stop ();

	/* Synchronously do the realtime part of a transport stop.
	 *
	 * Calling this will cause the butler to asynchronously run
	 * ::non_realtime_stop() where the rest of the "stop" work will be
	 * done.
	 */

	realtime_stop (false, true);
}

void
Session::engine_running ()
{
	_transport_fsm->start ();
	reset_xrun_count ();
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

	resort_routes ();
	update_latency_compensation (false, false);

	set_dirty ();
}

void
Session::allow_auto_play (bool yn)
{
	auto_play_legal = yn;
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

boost::shared_ptr<TransportMaster>
Session::transport_master() const
{
	return TransportMasterManager::instance().current();
}

bool
Session::transport_master_is_external () const
{
	return TransportMasterManager::instance().current() && config.get_external_sync();
}

bool
Session::transport_master_no_external_or_using_engine () const
{
	return !TransportMasterManager::instance().current() || !config.get_external_sync() || (TransportMasterManager::instance().current()->type() == Engine);
}

void
Session::sync_source_changed (SyncSource type, samplepos_t pos, pframes_t cycle_nframes)
{
	/* Runs in process() context */

	boost::shared_ptr<TransportMaster> master = TransportMasterManager::instance().current();

	if (master->can_loop()) {
		request_play_loop (false);
	} else if (master->has_loop()) {
		request_play_loop (true);
	}

	/* slave change, reset any DiskIO block on disk output because it is no
	   longer valid with a new slave.
	*/

	DiskReader::dec_no_disk_output ();

#if 0
	we should not be treating specific transport masters as special cases because there maybe > 1 of a particular type

	boost::shared_ptr<MTC_TransportMaster> mtc_master = boost::dynamic_pointer_cast<MTC_TransportMaster> (master);

	if (mtc_master) {
		mtc_master->ActiveChanged.connect_same_thread (mtc_status_connection, boost::bind (&Session::mtc_status_changed, this, _1));
		MTCSyncStateChanged(mtc_master->locked() );
	} else {
		if (g_atomic_int_compare_and_exchange (&_mtc_active, 1, 0)) {
			MTCSyncStateChanged( false );
		}
		mtc_status_connection.disconnect ();
	}

	boost::shared_ptr<LTC_TransportMaster> ltc_master = boost::dynamic_pointer_cast<LTC_TransportMaster> (master);

	if (ltc_master) {
		ltc_master->ActiveChanged.connect_same_thread (ltc_status_connection, boost::bind (&Session::ltc_status_changed, this, _1));
		LTCSyncStateChanged (ltc_master->locked() );
	} else {
		if (g_atomic_int_compare_and_exchange (&_ltc_active, 1, 0)) {
			LTCSyncStateChanged( false );
		}
		ltc_status_connection.disconnect ();
	}
#endif

	DEBUG_TRACE (DEBUG::Slave, string_compose ("set new slave to %1\n", master));

	// need to queue this for next process() cycle
	_send_timecode_update = true;

	boost::shared_ptr<RouteList> rl = routes.reader();
	const bool externally_slaved = transport_master_is_external();

	for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
		boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);
		if (tr && !tr->is_private_route()) {
			tr->set_slaved (externally_slaved);
		}
	}

	set_dirty();
}

bool
Session::transport_stopped() const
{
	return _transport_fsm->stopped();
}

bool
Session::transport_stopped_or_stopping() const
{
	return _transport_fsm->stopped() || _transport_fsm->stopping();
}

bool
Session::transport_rolling() const
{
	return _transport_speed != 0.0 && _count_in_samples == 0 && _remaining_latency_preroll == 0;
}

bool
Session::locate_pending () const
{
	return _transport_fsm->locating();
}

bool
Session::locate_initiated() const
{
	return _transport_fsm->declicking_for_locate() || _transport_fsm->locating();
}

bool
Session::declick_in_progress () const
{
	return _transport_fsm->declick_in_progress();
}

bool
Session::transport_will_roll_forwards () const
{
	return _transport_fsm->will_roll_fowards ();
}
