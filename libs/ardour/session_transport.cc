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
#include "ardour/tempo.h"
#include "ardour/transport_fsm.h"
#include "ardour/transport_master.h"
#include "ardour/transport_master_manager.h"
#include "ardour/triggerbox.h"
#include "ardour/operations.h"
#include "ardour/vca.h"
#include "ardour/vca_manager.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Temporal;

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
#define TFSM_ROLL() { _transport_fsm->enqueue (new TransportFSM::Event (TransportFSM::StartTransport)); }
#define TFSM_STOP(abort,clear) { _transport_fsm->enqueue (new TransportFSM::Event (TransportFSM::StopTransport,abort,clear)); }
#define TFSM_LOCATE(target,ltd,loop,force) { _transport_fsm->enqueue (new TransportFSM::Event (TransportFSM::Locate,target,ltd,loop,force)); }
#define TFSM_SPEED(speed) { _transport_fsm->enqueue (new TransportFSM::Event (speed)); }

/* *****************************************************************************
 * REALTIME ACTIONS (to be called on state transitions)
 * ****************************************************************************/

void
Session::realtime_stop (bool abort, bool clear_state)
{
	ENSURE_PROCESS_THREAD;

	DEBUG_TRACE (DEBUG::Transport, string_compose ("realtime stop @ %1 speed = %2\n", _transport_sample, _transport_fsm->transport_speed()));
	PostTransportWork todo = PostTransportStop;

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

	/* if we're going to clear loop state, then force disabling record BUT only if we're not doing latched rec-enable */
	disable_record (true, (!Config->get_latched_record_enable() && clear_state));

	if (clear_state && !Config->get_loop_is_mode()) {
		unset_play_loop ();
	}

	reset_punch_loop_constraint ();

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
Session::locate (samplepos_t target_sample, bool for_loop_end, bool force, bool with_mmc)
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

	DEBUG_TRACE (DEBUG::Transport, string_compose ("rt-locate to %1 ts = %7, for loop end %2 force %3 mmc %4\n",
	                                               target_sample, for_loop_end, force, with_mmc, _transport_sample));

	if (!force && (_transport_sample == target_sample) && !for_loop_end) {
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

	assert (_transport_fsm->locating() || _transport_fsm->declicking_for_locate());

	/* Tell all routes to do the RT part of locate */

	boost::shared_ptr<RouteList> r = routes.reader ();
	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		(*i)->realtime_locate (for_loop_end);
	}

	if (force || !for_loop_end) {

		PostTransportWork todo = PostTransportLocate;
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

	/* cancel looped playback if transport pos outside of loop range */
	if (get_play_loop ()) {

		Location* al = _locations->auto_loop_location();

		if (al) {
			if (_transport_sample < al->start_sample() || _transport_sample >= al->end_sample()) {

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

			} else if (_transport_sample == al->start_sample()) {

				// located to start of loop - this is looping, basically

				boost::shared_ptr<RouteList> rl = routes.reader();

				for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
					boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);

					if (tr && tr->rec_enable_control()->get_value()) {
						// tell it we've looped, so it can deal with the record state
						tr->transport_looped (_transport_sample);
					}
				}

				if (for_loop_end) {
					have_looped = true;
					TransportLooped(); // EMIT SIGNAL
				}
			}
		}
	}

	if (need_butler) {
		TFSM_EVENT (TransportFSM::ButlerRequired);
	} else {
		TFSM_EVENT (TransportFSM::LocateDone);
	}

	_send_timecode_update = true;

	if (with_mmc) {
		send_mmc_locate (_transport_sample);
	}

	_last_roll_location = _last_roll_or_reversal_location =  _transport_sample;

	Located (); /* EMIT SIGNAL */
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

double
Session::default_play_speed ()
{
	return _transport_fsm->default_speed();
}

/** Set the default speed that is used when we respond to a "play" action.
 *  @param speed New speed
 */
void
Session::set_default_play_speed (double spd)
{
	ENSURE_PROCESS_THREAD;
	/* see also Port::set_speed_ratio and
	 * VMResampler::set_rratio() for min/max range.
	 * speed must be > +/- 100 / 16 %
	 */
	if (spd > 0.0) {
		spd = std::min<double> (Config->get_max_transport_speed(), std::max (0.0625, spd));
	} else if (spd < 0.0) {
		spd = std::max<double> (- Config->get_max_transport_speed(), std::min (-0.0625, spd));
	}
	_transport_fsm->set_default_speed(spd);
	TFSM_SPEED(spd);
	TransportStateChange (); /* EMIT SIGNAL */
}

/** Set the transport speed.
 *  Called from the process thread.
 *  @param speed New speed
 */
void
Session::set_transport_speed (double speed)
{
	ENSURE_PROCESS_THREAD;
	DEBUG_TRACE (DEBUG::Transport, string_compose ("@ %1 Set transport speed to %2 from %3 (es = %4)\n", _transport_sample, speed, _transport_fsm->transport_speed(), _engine_speed));

	double default_speed = _transport_fsm->default_speed();

	assert (speed != 0.0);

	/* the logic:

	   a) engine speed is not 1.0 (normal speed)
	   b) engine speed matches the requested speed (sign ignored)
	   c) speed and transport speed have the same sign (no direction change)

	   For (c) the correct arithmetical test is >= 0, but we care about the
	   case where at least one of them is zero. That would generate an
	   equality with zero, but if only one of them is zero, we still need
	   to change speed. So we check that the product is > 0, which implies
	   that neither of them are zero, and they have the same sign.

	*/

	if ((_engine_speed != default_speed) && (_engine_speed == fabs (speed)) && ((speed * _transport_fsm->transport_speed()) > 0)) {
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

	double new_engine_speed = fabs (speed);
	// double new_transport_speed = (speed < 0) ? -1 : 1;

	if ((synced_to_engine()) && speed != 0.0 && speed != 1.0) {
		warning << string_compose (
			_("Global varispeed cannot be supported while %1 is connected to JACK transport control"),
			PROGRAM_NAME)
		        << endmsg;
		return;
	}

	clear_clicks ();
	_engine_speed = new_engine_speed;

	if (!Config->get_auto_return_after_rewind_ffwd() && fabs (speed) > 2.0) {
		/* fast-wind of any sort should cancel auto-return */
		/* since we don't have an actual ffwd/rew state yet, just trigger on a 'fast' varispeed */
		_requested_return_sample = -1;
		_last_roll_location = -1;
		_last_roll_or_reversal_location = -1;
	}


	/* throttle signal emissions.
	 * when slaved [_last]_transport_fsm->transport_speed()
	 * usually changes every cycle (tiny amounts due to DLL).
	 * Emitting a signal every cycle is overkill and unwarranted.
	 *
	 * Using _transport_fsm->transport_speed() is not acceptable,
	 * since it allows for large changes over a long period
	 * of time. Hence we introduce a dedicated variable to keep track
	 *
	 * The 0.2% dead-zone is somewhat arbitrary. Main use-case
	 * for TransportStateChange() here is the ShuttleControl display.
	 */
	const double act_speed = actual_speed ();

	if (fabs (_signalled_varispeed - act_speed) > .002
	    // still, signal hard changes to 1.0 and 0.0:
	    || (act_speed == default_speed && _signalled_varispeed != default_speed)
	    || (act_speed == 0.0 && _signalled_varispeed != 0.0)
		)
	{
		DEBUG_TRACE (DEBUG::Transport, string_compose ("send TSC3 with speed = %1\n", _transport_fsm->transport_speed()));
		TransportStateChange (); /* EMIT SIGNAL */
		_signalled_varispeed = act_speed;
	}

}

/** Called from the gui thread */
void
Session::stop_all_triggers (bool now)
{
	boost::shared_ptr<RouteList> rl = routes.reader();
	for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
		(*i)->stop_triggers (now);
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
Session::start_transport_from_trigger ()
{
	ENSURE_PROCESS_THREAD;
	TFSM_ROLL();
}

/** Called from the process thread */
void
Session::start_transport (bool after_loop)
{
	ENSURE_PROCESS_THREAD;
	DEBUG_TRACE (DEBUG::Transport, "start_transport\n");

	if (Config->get_loop_is_mode() && get_play_loop ()) {

		Location *location = _locations->auto_loop_location();

		if (location != 0) {
			if (_transport_sample != location->start_sample()) {

				/* force tracks to do their thing */
				set_track_loop (true);

				/* jump to start and then roll from there */

				request_locate (location->start_sample(), MustRoll);
				return;
			}
		}
	}

	if (Config->get_monitoring_model() == HardwareMonitoring) {
		set_track_monitor_input_status (!config.get_auto_input());
	}

	_last_roll_location = _transport_sample;
	_last_roll_or_reversal_location = _transport_sample;
	if (!have_looped && !_exporting) {
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

	clear_clicks ();

	if (!_engine.freewheeling()) {
		Timecode::Time time;
		timecode_time_subframes (_transport_sample, time);
		if (transport_master()->type() != MTC) { // why not when slaved to MTC?
			send_immediate_mmc (MIDI::MachineControlCommand (MIDI::MachineControl::cmdDeferredPlay));
		}

		if ((actively_recording () || (config.get_punch_in () && get_record_enabled ()))
		    && click_data && (config.get_count_in () || _count_in_once)) {
			TempoMap::SharedPtr tmap (TempoMap::use());

			_count_in_once = false;
			/* calculate count-in duration (in audio samples)
			 * - use [fixed] tempo/meter at _transport_sample
			 * - calc duration of 1 bar + time-to-beat before or at transport_sample
			 */
			TempoMetric const & tempometric = tmap->metric_at (_transport_sample);

			const double num = tempometric.divisions_per_bar ();
			/* XXX possible optimization: get meter and BBT time in one call */
			const Temporal::BBT_Time bbt = tmap->bbt_at (timepos_t (_transport_sample));
			const double bar_fract = (double) bbt.beats / tempometric.divisions_per_bar();

			_count_in_samples = tempometric.samples_per_bar (_current_sample_rate);

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
			clear_clicks ();
			_clicks_cleared = cf;
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

	DEBUG_TRACE (DEBUG::Transport, string_compose ("send TSC4 with speed = %1\n", transport_speed()));

	if (!after_loop) {
		/* emit TransportStateChange signal only when transport is actually rolling */
		SessionEvent* ev = new SessionEvent (SessionEvent::TransportStateChange, SessionEvent::Add, _transport_sample, _transport_sample, 1.0);
		queue_event (ev);

		samplepos_t roll_pos = _transport_sample + std::max (_count_in_samples, _remaining_latency_preroll) * (_transport_fsm->will_roll_fowards () ? 1 : -1);
		if (roll_pos > 0 && roll_pos != _transport_sample) {
			/* and when transport_rolling () == true */
			SessionEvent* ev = new SessionEvent (SessionEvent::TransportStateChange, SessionEvent::Add, roll_pos, roll_pos, 1.0);
			queue_event (ev);
		}
	}
}

bool
Session::need_declick_before_locate () const
{
	/* At this time (July 2020) only audio playback from disk readers is
	   de-clicked. MIDI tracks with audio output really need it too.
	*/
	return naudiotracks() > 0;
}

bool
Session::should_stop_before_locate () const
{
	/* do "stopped" stuff if:
	 *
	 * we are rolling AND
	 * no autoplay in effect AND
	 * we're not synced to an external transport master
	 *
	 */

	if ((!auto_play_legal || !config.get_auto_play()) &&
	    !(config.get_external_sync() && !synced_to_engine())) {

		return true;
	}
	return false;
}

bool
Session::user_roll_after_locate () const
{
	return auto_play_legal && config.get_auto_play();
}

bool
Session::should_roll_after_locate () const
{
	/* a locate must previously have been requested and completed before
	 * this answer can be considered correct
	 */

	return ((!config.get_external_sync() && (auto_play_legal && config.get_auto_play())) && !_exporting);

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
			_remaining_latency_preroll = 0;
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
		TFSM_EVENT (TransportFSM::LocateDone);
	}

	/* the butler finished its work so clear all PostTransportWork flags
	 */

	set_post_transport_work (PostTransportWork (0));

	set_next_event ();

	if (_transport_fsm->waiting_for_butler()) {
		TFSM_EVENT (TransportFSM::ButlerDone);
	}
}

void
Session::schedule_butler_for_transport_work ()
{
	assert (_transport_fsm->waiting_for_butler ());
	DEBUG_TRACE (DEBUG::Butler, string_compose ("summon butler for transport work (%1)\n", enum_2_string (post_transport_work())));
	_butler->schedule_transport_work ();
}

bool
Session::maybe_stop (samplepos_t limit)
{
	ENSURE_PROCESS_THREAD;
	if ((_transport_fsm->transport_speed() > 0.0f && _transport_sample >= limit) || (_transport_fsm->transport_speed() < 0.0f && _transport_sample == 0)) {
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
Session::should_ignore_transport_request (TransportRequestSource src, TransportRequestType type)
{
	if (config.get_external_sync()) {
		if (TransportMasterManager::instance().current()->allow_request (src, type)) {
			/* accepting a command means dropping external sync first */
			config.set_external_sync (false);
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
Session::reset_transport_speed (TransportRequestSource origin)
{
	request_transport_speed (_transport_fsm->default_speed(), origin);
}

void
Session::request_transport_speed (double speed, TransportRequestSource origin)
{
	if (synced_to_engine()) {
		if (speed != 0) {
			_engine.transport_start ();
		} else {
			_engine.transport_stop ();
		}
		return;
	}

	if (speed == 1. || speed == 0. || speed == -1.) {
		if (should_ignore_transport_request (origin, TR_StartStop)) {
			return;
		}
	} else {
		if (should_ignore_transport_request (origin, TR_Speed)) {
			return;
		}
	}

	SessionEvent* ev = new SessionEvent (SessionEvent::SetTransportSpeed, SessionEvent::Add, SessionEvent::Immediate, 0, speed);
	DEBUG_TRACE (DEBUG::Transport, string_compose ("Request transport speed = %1 as default = %2\n", speed));
	queue_event (ev);
}

void
Session::request_default_play_speed (double speed, TransportRequestSource origin)
{
	if (synced_to_engine()) {
		return;
	}

	SessionEvent* ev = new SessionEvent (SessionEvent::SetDefaultPlaySpeed, SessionEvent::Add, SessionEvent::Immediate, 0, speed);
	DEBUG_TRACE (DEBUG::Transport, string_compose ("Request default transport speed = %1 as default = %2\n", speed));
	queue_event (ev);
}

/** Request a new transport speed, but if the speed parameter is exactly zero then use
 *  a very small +ve value to prevent the transport actually stopping.  This method should
 *  be used by callers who are varying transport speed but don't ever want to stop it.
 */
void
Session::request_transport_speed_nonzero (double speed, TransportRequestSource origin)
{
	if (speed == 0) {
		speed = DBL_EPSILON;
	}

	request_transport_speed (speed);
}

void
Session::request_roll (TransportRequestSource origin)
{
	if (synced_to_engine()) {
		_engine.transport_start ();
		return;
	}

	if (should_ignore_transport_request (origin, TR_StartStop)) {
		return;
	}

	SessionEvent* ev = new SessionEvent (SessionEvent::StartRoll, SessionEvent::Add, SessionEvent::Immediate, 0, false); /* final 2 argumment do not matter */
	DEBUG_TRACE (DEBUG::Transport, "Request transport roll\n");
	queue_event (ev);
}

void
Session::request_stop (bool abort, bool clear_state, TransportRequestSource origin)
{
	if (synced_to_engine()) {
		_engine.transport_stop ();
		return;
	}

	if (should_ignore_transport_request (origin, TR_StartStop)) {
		return;
	}

	/* clear our solo-selection, if there is one */
	if ( solo_selection_active() ) {
		solo_selection ( _soloSelection, false );
	}

	SessionEvent* ev = new SessionEvent (SessionEvent::EndRoll, SessionEvent::Add, SessionEvent::Immediate, audible_sample(), 0.0, abort, clear_state);
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
	_preroll_record_trim_len = rec_in - pos;
	maybe_enable_record ();
	request_locate (pos, MustRoll);
	set_requested_return_sample (rec_in);

	if (pos < rec_in) {
		/* Notify GUI to update monitor state display */
		SessionEvent* ev = new SessionEvent (SessionEvent::TransportStateChange, SessionEvent::Add, rec_in, rec_in, 1.0);
		queue_event (ev);
	}
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
	request_transport_speed(_transport_fsm->default_speed());
	request_roll ();
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
				target_speed = _transport_fsm->default_speed();
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
Session::request_play_range (list<TimelineRange>* range, bool leave_rolling)
{
	SessionEvent* ev = new SessionEvent (SessionEvent::SetPlayAudioRange, SessionEvent::Add, SessionEvent::Immediate, 0, (leave_rolling ? _transport_fsm->default_speed() : 0.0));
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

	boost::shared_ptr<RouteList> rl = get_routes();

	for (ARDOUR::RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {

		if ( !(*i)->is_track() ) {
			continue;
		}

		boost::shared_ptr<Stripable> s (*i);

		bool found = (std::find(list.begin(), list.end(), s) != list.end());
		if ( found ) {
			/* must invalidate playlists on selected track, so disk reader
			 * will re-fill with the new selection state for solo_selection */
			boost::shared_ptr<Track> track = boost::dynamic_pointer_cast<Track> (*i);
			if (track) {
				boost::shared_ptr<Playlist> playlist = track->playlist();
				if (playlist) {
					playlist->ContentsChanged();
				}
			}
		}

		if ( found & new_state ) {
			solo_list->push_back (s->solo_control());
		} else {
			unsolo_list->push_back (s->solo_control());
		}
	}

	/* set/unset solos of the associated tracks */
	set_controls (solo_list, 1.0, Controllable::NoGroup);
	set_controls (unsolo_list, 0.0, Controllable::NoGroup);

	if (new_state)
		_soloSelection = list;
	else
		_soloSelection.clear();
}


void
Session::butler_transport_work (bool have_process_lock)
{
	/* Note: this function executes in the butler thread context */

  restart:
	boost::shared_ptr<RouteList> r = routes.reader ();
	int on_entry = g_atomic_int_get (&_butler->should_do_transport_work);
	bool finished = true;
	PostTransportWork ptw = post_transport_work();
#ifndef NDEBUG
	uint64_t before = g_get_monotonic_time();
#endif

	DEBUG_TRACE (DEBUG::Transport, string_compose ("Butler transport work, todo = [%1] (0x%3%4%5) at %2\n", enum_2_string (ptw), before, std::hex, ptw, std::dec));

	if (ptw & PostTransportAdjustPlaybackBuffering) {
		/* need to prevent concurrency with ARDOUR::Reader::run(),
		 * DiskWriter::adjust_buffering() re-allocates the ringbuffer */
		Glib::Threads::Mutex::Lock lx (AudioEngine::instance()->process_lock (), Glib::Threads::NOT_LOCK);
		if (!have_process_lock) {
			lx.acquire ();
		}
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
		Glib::Threads::Mutex::Lock lx (AudioEngine::instance()->process_lock (), Glib::Threads::NOT_LOCK);
		if (!have_process_lock) {
			lx.acquire ();
		}
		for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
			boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);
			if (tr) {
				tr->adjust_capture_buffering ();
			}
		}
	}

	if (ptw & PostTransportStop) {
		non_realtime_stop (ptw & PostTransportAbort, on_entry, finished);
		if (!finished) {
			g_atomic_int_dec_and_test (&_butler->should_do_transport_work);
			goto restart;
		}
	}

	const int butler = g_atomic_int_get (&_butler_seek_counter);
	const int rtlocates = g_atomic_int_get (&_seek_counter);

	if (butler != rtlocates) {
		DEBUG_TRACE (DEBUG::Transport, string_compose ("nonrealtime locate invoked from BTW (butler has done %1, rtlocs %2)\n", butler, rtlocates));
		non_realtime_locate ();
	}

	if (ptw & PostTransportOverWrite) {
		non_realtime_overwrite (on_entry, finished, (ptw & PostTransportLoopChanged));
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
Session::non_realtime_overwrite (int on_entry, bool& finished, bool update_loop_declicks)
{
	if (update_loop_declicks) {
		DiskReader::reset_loop_declick (_locations->auto_loop_location(), sample_rate());
	}

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

		if (!loc || (_transport_sample < loc->start().samples() || _transport_sample >= loc->end().samples())) {
			/* jumped out of loop range: stop tracks from looping,
			   but leave loop (mode) enabled.
			 */
			set_track_loop (false);

		} else if (loc && ((loc->start().samples() <= _transport_sample) || (loc->end().samples() > _transport_sample))) {

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

	microseconds_t start;
	uint32_t nt = 0;

	samplepos_t tf;
	gint sc;

	{
		boost::shared_ptr<RouteList> rl = routes.reader();

	  restart:
		sc = g_atomic_int_get (&_seek_counter);
		tf = _transport_sample;
		start = get_microseconds ();

		for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i, ++nt) {
			(*i)->non_realtime_locate (tf);
			if (sc != g_atomic_int_get (&_seek_counter)) {
				goto restart;
			}
		}

		microseconds_t end = get_microseconds ();
		int usecs_per_track = lrintf ((end - start) / (double) nt);
#ifndef NDEBUG
		std::cerr << "locate to " << tf << " took " << (end - start) << " usecs for " << nt << " tracks = " << usecs_per_track << " per track\n";
#endif
		if (usecs_per_track > g_atomic_int_get (&_current_usecs_per_track)) {
			g_atomic_int_set (&_current_usecs_per_track, usecs_per_track);
		}
	}

	/* we've caught up with whatever the _seek_counter was when we did the
	   non-realtime locates.
	*/
	g_atomic_int_set (&_butler_seek_counter, sc);

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

	/* This must be called while _transport_sample still reflects where we stopped
	 */

	flush_cue_recording ();

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

				if (_last_roll_location >= 0) {
					_transport_sample = _last_roll_location;
					do_locate = true;
				}
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
		if (!Config->get_loop_is_mode() && get_play_loop() && !loop_changing) {
			unset_play_loop ();
		}
	}

	/* reset loop_changing so it does not affect next transport action */
	loop_changing = false;

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
	DEBUG_TRACE (DEBUG::Transport, string_compose ("send TSC with speed = %1\n", _transport_fsm->transport_speed()));
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

		merge_event (new SessionEvent (SessionEvent::AutoLoop, SessionEvent::Replace, loc->end().samples(), loc->start().samples(), 0.0f));

		if (!Config->get_loop_is_mode()) {
			if (transport_rolling()) {
				/* set loop_changing to ensure that non_realtime_stop does not unset_play_loop */
				loop_changing = true;
			}
			/* args: position, disposition, for_loop_end=false, force=true */
			TFSM_LOCATE (loc->start().samples(), MustRoll, false, true);
		} else {
			if (!transport_rolling()) {
				/* loop-is-mode: not rolling, just locate to loop start */
				TFSM_LOCATE (loc->start().samples(), MustStop, false, true);
			}
		}
		TransportStateChange (); /* EMIT SIGNAL */
	} else {
		unset_play_loop ();
	}

	DEBUG_TRACE (DEBUG::Transport, string_compose ("send TSC2 with speed = %1\n", _transport_fsm->transport_speed()));
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
Session::set_play_range (list<TimelineRange>& range, bool leave_rolling)
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

	list<TimelineRange>::size_type sz = range.size();

	if (sz > 1) {

		list<TimelineRange>::iterator i = range.begin();
		list<TimelineRange>::iterator next;

		while (i != range.end()) {

			next = i;
			++next;

			/* locating/stopping is subject to delays for declicking.
			 */

			samplepos_t requested_sample = i->end().samples();

			if (requested_sample > current_block_size) {
				requested_sample -= current_block_size;
			} else {
				requested_sample = 0;
			}

			if (next == range.end()) {
				ev = new SessionEvent (SessionEvent::RangeStop, SessionEvent::Add, requested_sample, 0, 0.0f);
			} else {
				ev = new SessionEvent (SessionEvent::RangeLocate, SessionEvent::Add, requested_sample, (*next).start().samples(), 0.0f);
			}

			merge_event (ev);

			i = next;
		}

	} else if (sz == 1) {

		ev = new SessionEvent (SessionEvent::RangeStop, SessionEvent::Add, range.front().end().samples(), 0, 0.0f);
		merge_event (ev);

	}

	/* save range so we can do auto-return etc. */

	current_audio_range = range;

	/* now start rolling at the right place */

	ev = new SessionEvent (SessionEvent::LocateRoll, SessionEvent::Add, SessionEvent::Immediate, range.front().start().samples(), 0.0f, false);
	merge_event (ev);

	DEBUG_TRACE (DEBUG::Transport, string_compose ("send TSC5 with speed = %1\n", _transport_fsm->transport_speed()));
	TransportStateChange (); /* EMIT SIGNAL */
}

void
Session::request_bounded_roll (samplepos_t start, samplepos_t end)
{
	TimelineRange ar (timepos_t (start), timepos_t (end), 0);
	list<TimelineRange> lar;

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
	SessionEvent *ev = new SessionEvent (SessionEvent::LocateRollLocate, SessionEvent::Add, SessionEvent::Immediate, return_to, _transport_fsm->default_speed());
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
	cancel_audition ();

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

	if (actively_recording ()) {
		++_capture_xruns;

		if (Config->get_stop_recording_on_xrun()) {

			/* it didn't actually halt, but we need
			 * to handle things in the same way.
			 */

			engine_halted();

			/* ..and start the FSM engine again */
			_transport_fsm->start ();
		} else {
			boost::shared_ptr<RouteList> rl = routes.reader();
			for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
				boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);
				if (tr) {
					tr->mark_capture_xrun ();
				}
			}

		}
	}
	else if (_exporting && _realtime_export && _export_rolling) {
		++_export_xruns;
	}
}

void
Session::reset_xrun_count ()
{
	_xrun_count = 0;
	ARDOUR::reset_performance_meters (this);
	Xrun (-1); /* EMIT SIGNAL */
}

void
Session::route_processors_changed (RouteProcessorChange c)
{
	if (g_atomic_int_get (&_ignore_route_processor_changes) > 0) {
		g_atomic_int_set (&_ignored_a_processor_change, 1);
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

	TransportMasterManager::instance().unblock_disk_output ();

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
Session::transport_state_rolling() const
{
	return _transport_fsm->rolling();
}

bool
Session::transport_rolling() const
{
	return _transport_fsm->transport_speed() != 0.0 && _count_in_samples == 0 && _remaining_latency_preroll == 0;
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

double
Session::transport_speed() const
{
	if (_transport_fsm->transport_speed() != _transport_fsm->transport_speed()) {
		// cerr << "\n\n!!TS " << _transport_fsm->transport_speed() << " TFSM::speed " << _transport_fsm->transport_speed() << " via " << _transport_fsm->current_state() << endl;
	}
	return _count_in_samples > 0 ? 0. : _transport_fsm->transport_speed();
}

double
Session::actual_speed() const
{
	if (_transport_fsm->transport_speed() > 0) return _engine_speed;
	if (_transport_fsm->transport_speed() < 0) return - _engine_speed;
	return 0;
}

void
Session::flush_cue_recording ()
{
	/* if the user canceled cue recording before stopping *and* didn't record any cues, leave cues unchanged */
	if (!TriggerBox::cue_recording() && !TriggerBox::cue_records.read_space()) {
		return;
	}

	CueRecord cr;
	TempoMap::SharedPtr tmap (TempoMap::use());

	/* we will delete the cues we rolled over, even if the user never wrote any new cues (??)*/
	_locations->clear_cue_markers (_last_roll_location, _transport_sample);

	while (TriggerBox::cue_records.read (&cr, 1) == 1) {
		BBT_Time bbt = tmap->bbt_at (timepos_t (cr.when));
		bbt = bbt.round_up_to_bar ();

		timepos_t when;

		if (tmap->time_domain() == Temporal::AudioTime) {
			when = timepos_t (tmap->sample_at (bbt));
		} else {
			when = timepos_t (tmap->quarters_at (bbt));
		}

		Location* l = new Location (*this, when, when, std::string(), Location::Flags (Location::IsMark|Location::IsCueMarker), cr.cue_number);
		_locations->add (l);
	}

	/* scheduled sync of cue markers in RT thread */
	cue_marker_change (0);

	/* disarm the cues from recording when we finish our pass */
	TriggerBox::set_cue_recording(false);
}
