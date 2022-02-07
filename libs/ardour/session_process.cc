/*
 * Copyright (C) 1999-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2006-2007 Jesse Chappell <jesse@essej.net>
 * Copyright (C) 2006-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2008-2009 Hans Baier <hansfbaier@googlemail.com>
 * Copyright (C) 2012-2019 Robin Gareus <robin@gareus.org>
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

#include <cmath>
#include <cerrno>
#include <algorithm>
#include <unistd.h>

#include <boost/algorithm/string/erase.hpp>

#include "pbd/i18n.h"
#include "pbd/error.h"
#include "pbd/enumwriter.h"
#include "pbd/pthread_utils.h"

#include <glibmm/threads.h>

#include "temporal/tempo.h"

#include "ardour/audioengine.h"
#include "ardour/auditioner.h"
#include "ardour/butler.h"
#include "ardour/cycle_timer.h"
#include "ardour/debug.h"
#include "ardour/disk_reader.h"
#include "ardour/graph.h"
#include "ardour/port.h"
#include "ardour/process_thread.h"
#include "ardour/scene_changer.h"
#include "ardour/session.h"
#include "ardour/transport_fsm.h"
#include "ardour/transport_master.h"
#include "ardour/transport_master_manager.h"
#include "ardour/ticker.h"
#include "ardour/triggerbox.h"
#include "ardour/types.h"
#include "ardour/vca.h"
#include "ardour/vca_manager.h"

#include "midi++/mmc.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;

#define TFSM_EVENT(evtype) { _transport_fsm->enqueue (new TransportFSM::Event (evtype)); }
#define TFSM_ROLL() { _transport_fsm->enqueue (new TransportFSM::Event (TransportFSM::StartTransport)); }
#define TFSM_STOP(abort,clear) { _transport_fsm->enqueue (new TransportFSM::Event (TransportFSM::StopTransport,abort,clear)); }
#define TFSM_SPEED(speed) { _transport_fsm->enqueue (new TransportFSM::Event (speed)); }
#define TFSM_LOCATE(target,ltd,loop,force) { _transport_fsm->enqueue (new TransportFSM::Event (TransportFSM::Locate,target,ltd,loop,force)); }


void
Session::setup_thread_local_variables ()
{
	Temporal::TempoMap::fetch ();
}

/** Called by the audio engine when there is work to be done with JACK.
 * @param nframes Number of samples to process.
 */

void
Session::process (pframes_t nframes)
{
	TimerRAII tr (dsp_stats[OverallProcess]);

	if (processing_blocked()) {
		_silent = true;
		return;
	} else {
		_silent = false;
	}

	samplepos_t transport_at_start = _transport_sample;

	setup_thread_local_variables ();

	if (non_realtime_work_pending()) {
		DEBUG_TRACE (DEBUG::Butler, string_compose ("non-realtime work pending: %1 (%2%3%4)\n", enum_2_string (post_transport_work()), std::hex, post_transport_work(), std::dec));
		if (!_butler->transport_work_requested ()) {
			DEBUG_TRACE (DEBUG::Butler, string_compose ("done, waiting? %1\n", _transport_fsm->waiting_for_butler()));
			butler_completed_transport_work ();
		} else {
			DEBUG_TRACE (DEBUG::Butler, "doesn't seem to have finished yet (from view of RT thread)\n");
		}
	}

	_engine.main_thread()->get_buffers ();

	(this->*process_function) (nframes);

	/* realtime-safe meter-position and processor-order changes
	 *
	 * ideally this would be done in
	 * Route::process_output_buffers() but various functions
	 * callig it hold a _processor_lock reader-lock
	 */
	bool one_or_more_routes_declicking = false;
	{
		ProcessorChangeBlocker pcb (this);
		boost::shared_ptr<RouteList> r = routes.reader ();
		for (RouteList::const_iterator i = r->begin(); i != r->end(); ++i) {
			if ((*i)->apply_processor_changes_rt()) {
				_rt_emit_pending = true;
			}
			if ((*i)->declick_in_progress()) {
				one_or_more_routes_declicking = true;
			}
		}
	}

	if (_update_send_delaylines) {
		boost::shared_ptr<RouteList> r = routes.reader ();
		for (RouteList::const_iterator i = r->begin(); i != r->end(); ++i) {
			(*i)->update_send_delaylines ();
		}
	}

	if (_rt_emit_pending) {
		if (!_rt_thread_active) {
			emit_route_signals ();
		}
		if (pthread_mutex_trylock (&_rt_emit_mutex) == 0) {
			pthread_cond_signal (&_rt_emit_cond);
			pthread_mutex_unlock (&_rt_emit_mutex);
			_rt_emit_pending = false;
		}
	}

	/* We are checking two things here:
	 *
	 * 1) whether or not all tracks have finished a declick out.
	 * 2) is the transport FSM waiting to be told this
	 */

	if (!one_or_more_routes_declicking && declick_in_progress()) {
		/* end of the declick has been reached by all routes */
		TFSM_EVENT (TransportFSM::DeclickDone);
	}

	_engine.main_thread()->drop_buffers ();

	/* deliver MIDI clock. Note that we need to use the transport sample
	 * position at the start of process(), not the value at the end of
	 * it. We may already have ticked() because of a transport state
	 * change, for example.
	 */

	try {
		_scene_changer->run (transport_at_start, transport_at_start + nframes);
	} catch (...) {
		/* don't bother with a message */
	}

	SendFeedback (); /* EMIT SIGNAL */
}

int
Session::fail_roll (pframes_t nframes)
{
	return no_roll (nframes);
}

int
Session::no_roll (pframes_t nframes)
{
	PT_TIMING_CHECK (4);
	TimerRAII tr (dsp_stats[NoRoll]);

	samplepos_t end_sample = _transport_sample + floor (nframes * _transport_fsm->transport_speed());
	int ret = 0;
	boost::shared_ptr<RouteList> r = routes.reader ();

	if (_click_io) {
		_click_io->silence (nframes);
	}

	VCAList v = _vca_manager->vcas ();
	for (VCAList::const_iterator i = v.begin(); i != v.end(); ++i) {
		(*i)->automation_run (_transport_sample, nframes);
	}

	_global_locate_pending = locate_pending ();

	if (_process_graph) {
		DEBUG_TRACE(DEBUG::ProcessThreads,"calling graph/no-roll\n");
		_process_graph->routes_no_roll( nframes, _transport_sample, end_sample, non_realtime_work_pending());
	} else {
		PT_TIMING_CHECK (10);
		for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {

			if ((*i)->is_auditioner()) {
				continue;
			}

			if ((*i)->no_roll (nframes, _transport_sample, end_sample, non_realtime_work_pending())) {
				error << string_compose(_("Session: error in no roll for %1"), (*i)->name()) << endmsg;
				ret = -1;
				break;
			}
		}
		PT_TIMING_CHECK (11);
	}

	PT_TIMING_CHECK (5);
	return ret;
}

/** @param need_butler to be set to true by this method if it needs the butler,
 *  otherwise it must be left alone.
 */
int
Session::process_routes (pframes_t nframes, bool& need_butler)
{
	TimerRAII tr (dsp_stats[Roll]);
	boost::shared_ptr<RouteList> r = routes.reader ();

	const samplepos_t start_sample = _transport_sample;
	const samplepos_t end_sample = _transport_sample + floor (nframes * _transport_fsm->transport_speed());

	if (actively_recording ()) {
		_capture_duration += nframes;
	}

	VCAList v = _vca_manager->vcas ();
	for (VCAList::const_iterator i = v.begin(); i != v.end(); ++i) {
		(*i)->automation_run (start_sample, nframes);
	}

	_global_locate_pending = locate_pending();

	if (_process_graph) {
		DEBUG_TRACE(DEBUG::ProcessThreads,"calling graph/process-routes\n");
		if (_process_graph->process_routes (nframes, start_sample, end_sample, need_butler) < 0) {
			stop_transport ();
			return -1;
		}
	} else {

		for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {

			int ret;

			if ((*i)->is_auditioner()) {
				continue;
			}

			bool b = false;

			if ((ret = (*i)->roll (nframes, start_sample, end_sample, b)) < 0) {
				cerr << "ERR1 STOP\n";
				TFSM_STOP (false, false);
				return -1;
			}

			if (b) {
				DEBUG_TRACE (DEBUG::Butler, string_compose ("%1 rolled and needs butler\n", (*i)->name()));
				need_butler = true;
			}
		}
	}

	return 0;
}

void
Session::get_track_statistics ()
{
	float pworst = 1.0f;
	float cworst = 1.0f;

	boost::shared_ptr<RouteList> rl = routes.reader();
	for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {

		boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);

		if (!tr || tr->is_private_route()) {
			continue;
		}

		pworst = min (pworst, tr->playback_buffer_load());
		cworst = min (cworst, tr->capture_buffer_load());
	}

	g_atomic_int_set (&_playback_load, (uint32_t) floor (pworst * 100.0f));
	g_atomic_int_set (&_capture_load, (uint32_t) floor (cworst * 100.0f));

	if (actively_recording()) {
		set_dirty();
	}
}

bool
Session::compute_audible_delta (samplepos_t& pos_and_delta) const
{
	if (_transport_fsm->transport_speed() == 0.0 || _count_in_samples > 0 || _remaining_latency_preroll > 0) {
		/* cannot compute audible delta, because the session is
		   generating silence that does not correspond to the timeline,
		   but is instead filling playback buffers to manage latency
		   alignment.
		*/
		DEBUG_TRACE (DEBUG::Slave, string_compose ("still adjusting for latency (%1) and/or count-in (%2) or stopped %1\n", _remaining_latency_preroll, _count_in_samples, _transport_fsm->transport_speed()));
		return false;
	}

	pos_and_delta -= _transport_sample;
	return true;
}

samplecnt_t
Session::calc_preroll_subcycle (samplecnt_t ns) const
{
	boost::shared_ptr<RouteList> r = routes.reader ();
	for (RouteList::const_iterator i = r->begin(); i != r->end(); ++i) {
		samplecnt_t route_offset = (*i)->playback_latency ();
		if (_remaining_latency_preroll > route_offset + ns) {
			/* route will no-roll for complete pre-roll cycle */
			continue;
		}
		if (_remaining_latency_preroll > route_offset) {
			/* route may need partial no-roll and partial roll from
			 * (_transport_sample - _remaining_latency_preroll) ..  +ns.
			 * shorten and split the cycle.
			 */
			ns = std::min (ns, (_remaining_latency_preroll - route_offset));
		}
	}
	return ns;
}

/** Process callback used when the auditioner is not active */
void
Session::process_with_events (pframes_t nframes)
{
	PT_TIMING_CHECK (3);
	TimerRAII tr (dsp_stats[ProcessFunction]);

	SessionEvent*  ev;
	pframes_t      this_nframes;
	samplepos_t     end_sample;
	bool           session_needs_butler = false;
	samplecnt_t     samples_moved;

	/* make sure the auditioner is silent */

	if (auditioner) {
		auditioner->silence (nframes);
	}

	/* handle any pending events */

	while (pending_events.read (&ev, 1) == 1) {
		merge_event (ev);
	}

	/* if we are not in the middle of a state change,
	   and there are immediate events queued up,
	   process them.
	*/

	while (!non_realtime_work_pending() && !immediate_events.empty()) {
		SessionEvent *ev = immediate_events.front ();
		immediate_events.pop_front ();
		process_event (ev);
	}
	/* only count-in when going to roll at speed 1.0 */
	if (_transport_fsm->transport_speed() != 1.0 && _count_in_samples > 0) {
		_count_in_samples = 0;
	}
	if (_transport_fsm->transport_speed() == 0.0) {
		_remaining_latency_preroll = 0;
	}

	assert (_count_in_samples == 0 || _remaining_latency_preroll == 0 || _count_in_samples == _remaining_latency_preroll);

	// DEBUG_TRACE (DEBUG::Transport, string_compose ("Running count in/latency preroll of %1 & %2\n", _count_in_samples, _remaining_latency_preroll));

	maybe_find_pending_cue ();

	while (_count_in_samples > 0 || _remaining_latency_preroll > 0) {
		samplecnt_t ns;

		if (_remaining_latency_preroll > 0) {
			ns = std::min ((samplecnt_t)nframes, _remaining_latency_preroll);
		} else {
			ns = std::min ((samplecnt_t)nframes, _count_in_samples);
		}

		/* process until next route in-point */
		ns = calc_preroll_subcycle (ns);

		if (_count_in_samples > 0) {
			run_click (_transport_sample - _count_in_samples, ns);
			assert (_count_in_samples >= ns);
			_count_in_samples -= ns;
		}

		if (_remaining_latency_preroll > 0) {
			if (_count_in_samples == 0) {
				click (_transport_sample - _remaining_latency_preroll, ns);
			}
			if (process_routes (ns, session_needs_butler)) {
				fail_roll (ns);
			}
		} else {
			no_roll (ns);
		}

		if (_remaining_latency_preroll > 0) {
			assert (_remaining_latency_preroll >= ns);
			_remaining_latency_preroll -= ns;
		}

		nframes -= ns;

		/* process events.. */
		if (!events.empty() && next_event != events.end()) {
			SessionEvent* this_event = *next_event;
			Events::iterator the_next_one = next_event;
			++the_next_one;

			while (this_event && this_event->action_sample == _transport_sample) {
				process_event (this_event);
				if (the_next_one == events.end()) {
					this_event = 0;
				} else {
					this_event = *the_next_one;
					++the_next_one;
				}
			}
			set_next_event ();
		}

		if (nframes == 0) {
			return;
		} else {
			_engine.split_cycle (ns);
		}
	}

	/* Decide on what to do with quarter-frame MTC during this cycle */

	bool const was_sending_qf_mtc = _send_qf_mtc;
	double const tolerance = Config->get_mtc_qf_speed_tolerance() / 100.0;

	if (_transport_fsm->transport_speed() != 0) {
		_send_qf_mtc = (
			Config->get_send_mtc () &&
			_transport_fsm->transport_speed() >= (1 - tolerance) &&
			_transport_fsm->transport_speed() <= (1 + tolerance)
			);

		if (_send_qf_mtc && !was_sending_qf_mtc) {
			/* we will re-start quarter-frame MTC this cycle, so send a full update to set things up */
			_send_timecode_update = true;
		}

		if (Config->get_send_mtc() && !_send_qf_mtc && _pframes_since_last_mtc > (sample_rate () / 4)) {
			/* we're sending MTC, but we're not sending QF MTC at the moment, and it's been
			   a quarter of a second since we sent anything at all, so send a full MTC update
			   this cycle.
			*/
			_send_timecode_update = true;
		}

		_pframes_since_last_mtc += nframes;
	}

	/* Events caused a transport change (or we re-started sending
	 * MTC), so send an MTC Full Frame (Timecode) message.  This
	 * is sent whether rolling or not, to give slaves an idea of
	 * ardour time on locates (and allow slow slaves to position
	 * and prepare for rolling)
	 */
	if (_send_timecode_update) {
		send_full_time_code (_transport_sample, nframes);
	}

	if (!process_can_proceed()) {
		_silent = true;
		return;
	}

	if (events.empty() || next_event == events.end()) {
		try_run_lua (nframes); // also during export ?? ->move to process_without_events()
		/* lua scripts may inject events */
		while (_n_lua_scripts > 0 && pending_events.read (&ev, 1) == 1) {
			merge_event (ev);
		}
		if (events.empty() || next_event == events.end()) {
			process_without_events (nframes);
			return;
		}
	}

	assert (_transport_fsm->transport_speed() == 0 || _transport_fsm->transport_speed() == 1.0 || _transport_fsm->transport_speed() == -1.0);

	samples_moved = (samplecnt_t) nframes * _transport_fsm->transport_speed();
	// DEBUG_TRACE (DEBUG::Transport, string_compose ("plan to move transport by %1 (%2 @ %3)\n", samples_moved, nframes, _transport_fsm->transport_speed()));

	end_sample = _transport_sample + samples_moved;

	{
		SessionEvent* this_event;
		Events::iterator the_next_one;

		if (!process_can_proceed()) {
			_silent = true;
			return;
		}

		if (!_exporting && config.get_external_sync()) {
			if (!implement_master_strategy ()) {
				no_roll (nframes);
				return;
			}
		}

		if (_transport_fsm->transport_speed() == 0) {
			no_roll (nframes);
			return;
		}


		samplepos_t stop_limit = compute_stop_limit ();

		if (maybe_stop (stop_limit)) {
			if (!_exporting && !timecode_transmission_suspended()) {
				send_midi_time_code_for_cycle (_transport_sample, end_sample, nframes);
			}

			no_roll (nframes);
			return;
		}

		this_event = *next_event;
		the_next_one = next_event;
		++the_next_one;

		/* yes folks, here it is, the actual loop where we really truly
		   process some audio
		*/

		while (nframes) {

			this_nframes = nframes; /* real (jack) time relative */
			samples_moved = (samplecnt_t) floor (_transport_fsm->transport_speed() * nframes); /* transport relative */
			// DEBUG_TRACE (DEBUG::Transport, string_compose ("sub-loop plan to move transport by %1 (%2 @ %3)\n", samples_moved, nframes, _transport_fsm->transport_speed()));

			/* running an event, position transport precisely to its time */
			if (this_event && this_event->action_sample <= end_sample && this_event->action_sample >= _transport_sample) {
				/* this isn't quite right for reverse play */
				samples_moved = (samplecnt_t) (this_event->action_sample - _transport_sample);
				// DEBUG_TRACE (DEBUG::Transport, string_compose ("sub-loop2 (for %4)plan to move transport by %1 (%2 @ %3)\n", samples_moved, nframes, _transport_fsm->transport_speed(), enum_2_string (this_event->type)));
				this_nframes = abs (floor(samples_moved / _transport_fsm->transport_speed()));
			}

			try_run_lua (this_nframes);

			if (this_nframes) {

				if (!_exporting && !timecode_transmission_suspended()) {
					send_midi_time_code_for_cycle (_transport_sample, _transport_sample + samples_moved, this_nframes);
				}

				click (_transport_sample, this_nframes);

				if (process_routes (this_nframes, session_needs_butler)) {
					fail_roll (nframes);
					return;
				}

				get_track_statistics ();

				nframes -= this_nframes;

				if (samples_moved < 0) {
					decrement_transport_position (-samples_moved);
				} else if (samples_moved) {
					increment_transport_position (samples_moved);
				} else {
					DEBUG_TRACE (DEBUG::Transport, "no transport motion\n");
				}

				maybe_stop (stop_limit);
			}

			if (nframes > 0) {
				_engine.split_cycle (this_nframes);
			}

			/* now handle this event and all others scheduled for the same time */

			while (this_event && this_event->action_sample == _transport_sample) {
				process_event (this_event);

				if (the_next_one == events.end()) {
					this_event = 0;
				} else {
					this_event = *the_next_one;
					++the_next_one;
				}
			}

			/* if an event left our state changing, do the right thing */

			if (nframes && non_realtime_work_pending()) {
				no_roll (nframes);
				break;
			}

			/* this is necessary to handle the case of seamless looping */
			end_sample = _transport_sample + floor (nframes * _transport_fsm->transport_speed());
		}

		set_next_event ();

	} /* implicit release of route lock */

	clear_active_cue ();

	if (session_needs_butler) {
		DEBUG_TRACE (DEBUG::Butler, "p-with-events: session needs butler, call it\n");
		_butler->summon ();
	}
}

bool
Session::transport_locked () const
{
	if (!locate_pending() && (!config.get_external_sync() || (transport_master()->ok() && transport_master()->locked()))) {
		return true;
	}

	return false;
}

void
Session::process_without_events (pframes_t nframes)
{
	TimerRAII tr (dsp_stats[ProcessFunction]);
	bool session_needs_butler = false;
	samplecnt_t samples_moved;

	if (!process_can_proceed()) {
		_silent = true;
		return;
	}

	if (!_exporting && config.get_external_sync()) {
		if (!implement_master_strategy ()) {
			no_roll (nframes);
			return;
		}
	}

	assert (_transport_fsm->transport_speed() == 0 || _transport_fsm->transport_speed() == 1.0 || _transport_fsm->transport_speed() == -1.0);

	if (_transport_fsm->transport_speed() == 0) {
		// DEBUG_TRACE (DEBUG::Transport, string_compose ("transport not moving @ %1\n", _transport_sample));
		no_roll (nframes);
		return;
	} else {
		samples_moved = (samplecnt_t) nframes * _transport_fsm->transport_speed();
		// DEBUG_TRACE (DEBUG::Transport, string_compose ("plan to move transport by %1 (%2 @ %3)\n", samples_moved, nframes, _transport_fsm->transport_speed()));
	}

	if (!_exporting && !timecode_transmission_suspended()) {
		send_midi_time_code_for_cycle (_transport_sample, _transport_sample + samples_moved, nframes);
	}

	samplepos_t const stop_limit = compute_stop_limit ();

	if (maybe_stop (stop_limit)) {
		no_roll (nframes);
		return;
	}

	if (maybe_sync_start (nframes)) {
		return;
	}

	click (_transport_sample, nframes);

	maybe_find_pending_cue ();

	if (process_routes (nframes, session_needs_butler)) {
		fail_roll (nframes);
		return;
	}

	clear_active_cue ();

	get_track_statistics ();

	if (samples_moved < 0) {
		decrement_transport_position (-samples_moved);
		// DEBUG_TRACE (DEBUG::Transport, string_compose ("DEcrement transport by %1 to %2\n", samples_moved, _transport_sample));
	} else if (samples_moved) {
		increment_transport_position (samples_moved);
		// DEBUG_TRACE (DEBUG::Transport, string_compose ("INcrement transport by %1 to %2\n", samples_moved, _transport_sample));
	} else {
		DEBUG_TRACE (DEBUG::Transport, "no transport motion\n");
	}

	maybe_stop (stop_limit);

	if (session_needs_butler) {
		DEBUG_TRACE (DEBUG::Butler, "p-without-events: session needs butler, call it\n");
		_butler->summon ();
	}
}

/** Process callback used when the auditioner is active.
 * @param nframes number of samples to process.
 */
void
Session::process_audition (pframes_t nframes)
{
	SessionEvent* ev;
	boost::shared_ptr<RouteList> r = routes.reader ();

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		if (!(*i)->is_auditioner()) {
			(*i)->silence (nframes);
		}
	}

	if (_process_graph) {
		_process_graph->swap_process_chain ();
	}

	/* handle pending events */

	while (pending_events.read (&ev, 1) == 1) {
		merge_event (ev);
	}

	/* if we are not in the middle of a state change,
	   and there are immediate events queued up,
	   process them.
	*/

	while (!non_realtime_work_pending() && !immediate_events.empty()) {
		SessionEvent *ev = immediate_events.front ();
		immediate_events.pop_front ();
		process_event (ev);
	}

	/* run the auditioner, and if it says we need butler service, ask for it */

	if (auditioner->play_audition (nframes) > 0) {
		DEBUG_TRACE (DEBUG::Butler, "auditioner needs butler, call it\n");
		_butler->summon ();
	}

	/* if using a monitor section, run it because otherwise we don't hear anything */

	if (_monitor_out && auditioner->needs_monitor()) {
		_monitor_out->monitor_run (_transport_sample, _transport_sample + nframes, nframes);
	}

	if (!auditioner->auditioning()) {
		/* auditioner no longer active, so go back to the normal process callback */
		process_function = &Session::process_with_events;
	}
}

bool
Session::maybe_sync_start (pframes_t & nframes)
{
	pframes_t sync_offset;

	if (!waiting_for_sync_offset) {
		return false;
	}

	if (_engine.get_sync_offset (sync_offset) && sync_offset < nframes) {

		/* generate silence up to the sync point, then
		   adjust nframes + offset to reflect whatever
		   is left to do.
		*/

		no_roll (sync_offset);
		nframes -= sync_offset;
		Port::increment_global_port_buffer_offset (sync_offset);
		waiting_for_sync_offset = false;

		if (nframes == 0) {
			return true; // done, nothing left to process
		}

	} else {

		/* sync offset point is not within this process()
		   cycle, so just generate silence. and don't bother
		   with any fancy stuff here, just the minimal silence.
		*/

		_silent = true;

		if (Config->get_locate_while_waiting_for_sync()) {
			DEBUG_TRACE (DEBUG::Transport, "micro-locate while waiting for sync\n");
			if (micro_locate (nframes)) {
				/* XXX ERROR !!! XXX */
			}
		}

		return true; // done, nothing left to process
	}

	return false;
}

void
Session::queue_event (SessionEvent* ev)
{
	if (deletion_in_progress ()) {
		return;
	} else if (loading ()) {
		merge_event (ev);
	} else {
		Glib::Threads::Mutex::Lock lm (rb_write_lock);
		pending_events.write (&ev, 1);
	}
}

void
Session::set_next_event ()
{
	if (events.empty()) {
		next_event = events.end();
		return;
	}

	if (next_event == events.end()) {
		next_event = events.begin();
	}

	if ((*next_event)->action_sample > _transport_sample) {
		next_event = events.begin();
	}

	for (; next_event != events.end(); ++next_event) {
		if ((*next_event)->action_sample >= _transport_sample) {
			break;
		}
	}
	if (next_event != events.end()) {
		DEBUG_TRACE (DEBUG::SessionEvents, string_compose ("@ %1 next event set to %2 @ %3\n", _transport_sample, enum_2_string ((*next_event)->type), (*next_event)->action_sample));
	} else {
		DEBUG_TRACE (DEBUG::SessionEvents, string_compose ("no next event for %1\n", _transport_sample));
	}
}

void
Session::process_event (SessionEvent* ev)
{
	bool remove = true;
	bool del = true;

	/* if we're in the middle of a state change (i.e. waiting
	   for the butler thread to complete the non-realtime
	   part of the change), we'll just have to queue this
	   event for a time when the change is complete.
	*/

	if (non_realtime_work_pending()) {

		/* except locates, which we have the capability to handle */

		if (ev->type != SessionEvent::Locate) {
			immediate_events.insert (immediate_events.end(), ev);
			_remove_event (ev);
			return;
		}
	}

	DEBUG_TRACE (DEBUG::SessionEvents, string_compose ("Processing event: %1 @ %2\n", enum_2_string (ev->type), _transport_sample));

	switch (ev->type) {
	case SessionEvent::SetLoop:
		/* this is the event sent by a UI to define whether or not we
		   use loop range playback or not.
		*/
		set_play_loop (ev->yes_or_no, true);
		break;

	case SessionEvent::AutoLoop:
		/* this is the event created by the Session that marks
		   the end of the loop range and if we're loop playing,
		   triggers a special kind of locate back to the start of the
		   loop range.
		*/
		if (play_loop) {
			/* roll after locate, set "for loop end" true
			*/
			TFSM_LOCATE (ev->target_sample, MustRoll, true, false);
		}
		remove = false;
		del = false;
		break;

	case SessionEvent::Locate:
		/* args: do not roll after locate, clear state, not for loop, force */
		DEBUG_TRACE (DEBUG::Transport, string_compose ("sending locate to %1 to tfsm\n", ev->target_sample));
		TFSM_LOCATE (ev->target_sample, ev->locate_transport_disposition, false, ev->yes_or_no);
		_send_timecode_update = true;
		break;

	case SessionEvent::LocateRoll:
		/* args: roll after locate, clear state if not looping, not for loop, force */
		TFSM_LOCATE (ev->target_sample, MustRoll, false, ev->yes_or_no);
		_send_timecode_update = true;
		break;

	case SessionEvent::Skip:
		if (Config->get_skip_playback()) {
			TFSM_LOCATE (ev->target_sample, MustRoll, false, false);
			_send_timecode_update = true;
		}
		remove = false;
		del = false;
		break;

	case SessionEvent::LocateRollLocate:
		// locate is handled by ::request_roll_at_and_return()
		_requested_return_sample = ev->target_sample;
		TFSM_LOCATE (ev->target2_sample, MustRoll, false, false);
		_send_timecode_update = true;
		break;


	case SessionEvent::SetTransportSpeed:
		TFSM_SPEED (ev->speed);
		break;

	case SessionEvent::SetDefaultPlaySpeed:
		set_default_play_speed (ev->speed);
		break;

	case SessionEvent::StartRoll:
		TFSM_ROLL ();
		break;

	case SessionEvent::EndRoll:
		TFSM_STOP (ev->yes_or_no, ev->second_yes_or_no);
		break;

	case SessionEvent::SetTransportMaster:
		/* do not allow changing the transport master if we're already
		   using one.
		*/
		if (!config.get_external_sync()) {
			TransportMasterManager::instance().set_current (ev->transport_master);
		}
		break;

	case SessionEvent::PunchIn:
		// cerr << "PunchIN at " << transport_sample() << endl;
		if (config.get_punch_in() && record_status() == Enabled) {
			enable_record ();
		}
		remove = false;
		del = false;
		break;

	case SessionEvent::PunchOut:
		// cerr << "PunchOUT at " << transport_sample() << endl;
		if (config.get_punch_out()) {
			step_back_from_record ();
		}
		remove = false;
		del = false;
		break;

	case SessionEvent::RangeStop:
		cerr << "RANGE STOP\n";
		TFSM_STOP (ev->yes_or_no, false);
		remove = false;
		del = false;
		break;

	case SessionEvent::RangeLocate:
		/* args: roll after locate, not with loop */
		TFSM_LOCATE (ev->target_sample, MustRoll, false, false);
		remove = false;
		del = false;
		break;

	case SessionEvent::Overwrite:
		if (boost::shared_ptr<Track> track = ev->track.lock()) {
			overwrite_some_buffers (track, ev->overwrite);
		}
		break;

	case SessionEvent::OverwriteAll:
			overwrite_some_buffers (boost::shared_ptr<Track>(), ev->overwrite);
		break;

	case SessionEvent::TransportStateChange:
		TransportStateChange (); /* EMIT SIGNAL */
		break;

	case SessionEvent::Audition:
		set_audition (ev->region);
		// drop reference to region
		ev->region.reset ();
		break;

	case SessionEvent::SetPlayAudioRange:
		set_play_range (ev->audio_range, (ev->speed == _transport_fsm->default_speed()));  //an explicit PLAY state would be nicer here
		break;

	case SessionEvent::CancelPlayAudioRange:
		unset_play_range();
		break;

	case SessionEvent::RealTimeOperation:
		process_rtop (ev);
		del = false; // other side of RT request needs to clean up
		break;

	case SessionEvent::AdjustPlaybackBuffering:
		schedule_playback_buffering_adjustment ();
		break;

	case SessionEvent::AdjustCaptureBuffering:
		schedule_capture_buffering_adjustment ();
		break;

	case SessionEvent::SetTimecodeTransmission:
		g_atomic_int_set (&_suspend_timecode_transmission, ev->yes_or_no ? 0 : 1);
		break;

	case SessionEvent::SyncCues:
		sync_cues ();
		break;

	default:
	  fatal << string_compose(_("Programming error: illegal event type in process_event (%1)"), ev->type) << endmsg;
		abort(); /*NOTREACHED*/
		break;
	};

	if (remove) {
		del = (del && !_remove_event (ev));
	}

	if (del) {
		delete ev;
	}
}

samplepos_t
Session::compute_stop_limit () const
{
	if (!Config->get_stop_at_session_end ()) {
		return max_samplepos;
	}

	if (config.get_external_sync()) {
		return max_samplepos;
	}

	bool const punching_in = (config.get_punch_in () && _locations->auto_punch_location());
	bool const punching_out = (config.get_punch_out () && _locations->auto_punch_location());

	if (actively_recording ()) {
		/* permanently recording */
		return max_samplepos;
	} else if (punching_in && !punching_out) {
		/* punching in but never out */
		return max_samplepos;
	} else if (punching_in && punching_out && _locations->auto_punch_location()->end() > current_end_sample()) {
		/* punching in and punching out after session end */
		return max_samplepos;
	}

	return current_end_sample ();
}



/* dedicated thread for signal emission.
 *
 * while sending cross-thread signals from the process thread
 * is fine in general, PBD::Signal's use of boost::function and
 * boost:bind can produce a vast overhead which is not
 * acceptable for low latency.
 *
 * This works around the issue by moving the boost overhead
 * out of the RT thread. The overall load is probably higher but
 * the realtime thread remains unaffected.
 */

void
Session::emit_route_signals ()
{
	// TODO use RAII to allow using these signals in other places
	BatchUpdateStart(); /* EMIT SIGNAL */
	ProcessorChangeBlocker pcb (this);
	boost::shared_ptr<RouteList> r = routes.reader ();
	for (RouteList::const_iterator ci = r->begin(); ci != r->end(); ++ci) {
		(*ci)->emit_pending_signals ();
	}
	BatchUpdateEnd(); /* EMIT SIGNAL */
}

void
Session::emit_thread_start ()
{
	if (_rt_thread_active) {
		return;
	}
	_rt_thread_active = true;

	if (pthread_create (&_rt_emit_thread, NULL, emit_thread, this)) {
		_rt_thread_active = false;
	}
}

void
Session::emit_thread_terminate ()
{
	if (!_rt_thread_active) {
		return;
	}
	_rt_thread_active = false;

	if (pthread_mutex_lock (&_rt_emit_mutex) == 0) {
		pthread_cond_signal (&_rt_emit_cond);
		pthread_mutex_unlock (&_rt_emit_mutex);
	}

	void *status;
	pthread_join (_rt_emit_thread, &status);
}

void *
Session::emit_thread (void *arg)
{
	Session *s = static_cast<Session *>(arg);
	pthread_set_name ("SessionSignals");
	s->emit_thread_run ();
	pthread_exit (0);
	return 0;
}

void
Session::emit_thread_run ()
{
	pthread_mutex_lock (&_rt_emit_mutex);
	while (_rt_thread_active) {
		emit_route_signals();
		pthread_cond_wait (&_rt_emit_cond, &_rt_emit_mutex);
	}
	pthread_mutex_unlock (&_rt_emit_mutex);
}

double
Session::plan_master_strategy_engine (pframes_t nframes, double master_speed, samplepos_t master_transport_sample, double /* catch_speed */)
{
	/* JACK Transport. */

	TransportMasterManager& tmm (TransportMasterManager::instance());
	sampleoffset_t delta = _transport_sample - master_transport_sample;
	const bool interesting_transport_state_change_underway = (locate_pending() || declick_in_progress());

	DEBUG_TRACE (DEBUG::Slave, string_compose ("JACK Transport: delta = %1 transport change underway %2 master speed %3\n", delta, interesting_transport_state_change_underway, master_speed));

	if (master_speed == 0) {

		DEBUG_TRACE (DEBUG::Slave, "JACK transport: not moving\n");

		const samplecnt_t wlp = worst_latency_preroll_buffer_size_ceil ();

		if (delta != wlp) {

			DEBUG_TRACE (DEBUG::Slave, string_compose ("JACK transport: need to locate to reduce delta %1 vs %2\n", delta, wlp));

			/* if we're not aligned with the current JACK * time, then jump to it */

			if (!interesting_transport_state_change_underway) {

				const samplepos_t locate_target = master_transport_sample + wlp;
				DEBUG_TRACE (DEBUG::Slave, string_compose ("JACK transport: jump to master position %1 by locating to %2\n", master_transport_sample, locate_target));
				/* for JACK transport always stop after the locate (2nd argument == false) */

				transport_master_strategy.action = TransportMasterLocate;
				transport_master_strategy.target = locate_target;
				transport_master_strategy.roll_disposition = MustStop;

				return 1.0;

			} else {
				DEBUG_TRACE (DEBUG::Slave, string_compose ("JACK Transport: locate already in process, master @ %1, locating %2 declick %3\n",
				                                           master_transport_sample, locate_pending(), declick_in_progress()));
				transport_master_strategy.action = TransportMasterRelax;
				return 1.0;
			}
		}

	} else {

		DEBUG_TRACE (DEBUG::Slave, string_compose ("JACK transport: MOVING at %1\n", master_speed));

		if (_transport_fsm->rolling()) {
			/* master is rolling, and we're rolling ... with JACK we should always be perfectly in sync, so ... WTF? */
			if (delta) {
				if (remaining_latency_preroll() && worst_latency_preroll()) {
					/* our transport position is not moving because we're doing latency alignment. Nothing in particular to do */
					DEBUG_TRACE (DEBUG::Slave, "JACK transport: waiting for latency alignment\n");
					transport_master_strategy.action = TransportMasterRelax;
					return 1.0;
				} else {
					cerr << "\n\n\n IMPOSSIBLE! OUT OF SYNC (delta = " << delta << ") WITH JACK TRANSPORT (rlp = " << remaining_latency_preroll() << " wlp " << worst_latency_preroll() << ")\n\n\n";
				}
			}
		}
	}

	if (!interesting_transport_state_change_underway) {

		if (master_speed != 0.0) {

			/* master rolling, we should be too */

			if (_transport_fsm->transport_speed() == 0.0f) {
				DEBUG_TRACE (DEBUG::Slave, string_compose ("slave starts transport: %1 sample %2 tf %3\n", master_speed, master_transport_sample, _transport_sample));
				transport_master_strategy.action = TransportMasterStart;
				return 1.0;
			}

		} else if (!tmm.current()->starting()) { /* master stopped, not in "starting" state */

			if (_transport_fsm->transport_speed() != 0.0f) {
				DEBUG_TRACE (DEBUG::Slave, string_compose ("slave stops transport: %1 sample %2 tf %3\n", master_speed, master_transport_sample, _transport_sample));
				transport_master_strategy.action = TransportMasterStop;
				return 1.0;
			}
		}
	}

	/* No varispeed with JACK */
	transport_master_strategy.action = TransportMasterRelax;
	return 1.0;
}

double
Session::plan_master_strategy (pframes_t nframes, double master_speed, samplepos_t master_transport_sample, double catch_speed)
{
	/* This is called from inside AudioEngine::process_callback(),
	 * immediately after the TransportMasterManager has run its
	 * ::pre_process_transport_masters() method to allow all transport
	 * masters to update their information on the speed and position
	 * indicated by their data sources.
	 *
	 * Our task here is to determine what the Session should do during its
	 * process() call in order to respond to the transport master (or to
	 * not respond at all, if we're not using external sync). We want to
	 * set transport_master_strategy.action, which will be used from within
	 * the Session process() callback (via ::implement_master_strategy())
	 * to determine what, if anything to do there.
	 *
	 * The return value is the speed (aka "ratio") to be used by the port
	 * resampler. If we're not chasing the master, the correct answer will
	 * be 1.0. This can occur in a number of scenarios. If we are synced
	 * and locked to the master, we want to use the "catch speed" given to
	 * us as a parameter. This was determined by the
	 * TransportMasterManager as the correct speed to use in order to
	 * reduce the delta between the master's position and the session
	 * transport position.
	 *
	 * In situations where we are not synced+locked, either temporarily or
	 * longer term, we return 1.0, which leads to no resampling, and the
	 * session will run at normal speed.
	 */

	if (!config.get_external_sync()) {
		float desired = actual_speed ();
		if (desired==0.0) {
			return _transport_fsm->default_speed();
		}
		return desired;
	}

	/* When calling TransportMasterStart, sould aim for
	 *   delta >= _remaining_latency_preroll
	 * This way there can be silent pre-roll of exactly the delta time.
	 *
	 * In order to meet this condition, TransportMasterStart needs be set
	 * if the *end* of the current cycle can reach _remaining_latency_preroll.
	 * So current_block_size needs to be added here.
	 */
	const samplecnt_t wlp = worst_latency_preroll_buffer_size_ceil () + current_block_size;

	TransportMasterManager& tmm (TransportMasterManager::instance());
	const samplecnt_t locate_threshold = 5 * current_block_size;

	if (tmm.master_invalid_this_cycle()) {
		DEBUG_TRACE (DEBUG::Slave, "session told not to use the transport master this cycle\n");
		if (_transport_fsm->rolling() && Config->get_transport_masters_just_roll_when_sync_lost()) {
			transport_master_strategy.action = TransportMasterRelax;
		} else {
			transport_master_strategy.action = TransportMasterNoRoll;
		}
		return 1.0;
	}

	if (tmm.current()->type() == Engine) {
		/* JACK is fundamentally different */
		return plan_master_strategy_engine (nframes, master_speed, master_transport_sample, catch_speed);
	}

	const sampleoffset_t delta = _transport_sample - master_transport_sample;

	DEBUG_TRACE (DEBUG::Slave, string_compose ("\n\n\n\nsession at %1, master at %2, delta: %3 res: %4 TFSM state %5 action %6\n", _transport_sample, master_transport_sample, delta, tmm.current()->resolution(), _transport_fsm->current_state(), transport_master_strategy.action));

	const bool interesting_transport_state_change_underway = (locate_pending() || declick_in_progress());

	if ((transport_master_strategy.action == TransportMasterWait) || (transport_master_strategy.action == TransportMasterNoRoll)) {

		/* We've either been:
		 *
		 * 1) waiting for the master to catch up with a position that
		 * we located to (Wait)
		 * 2) waiting to be able to use the master's speed & position
		 *
		 * The two cases are very similar, but differ in the conditions
		 * under which we need to initiate a (possibly successive)
		 * locate in response to the master's position
		 *
		 * This code is very similar to the non-wait case (the "else"
		 * that ends this scope). The big difference is that here we
		 * know that we've just finished a locate specifically in order
		 * to catch the master. This changes the logic a little bit.
		 */

		DEBUG_TRACE (DEBUG::Slave, "had been waiting for locate-to-catch-master to finish\n");

		if (interesting_transport_state_change_underway) {
			/* still waiting for the declick and/or locate to
			   finish ... nothing to do for now.
			*/
			DEBUG_TRACE (DEBUG::Slave, "still waiting for the locate to finish\n");
			return 1.0;
		}

		bool should_locate;

		if (transport_master_strategy.action == TransportMasterNoRoll) {

			/* We've been waiting to be able to use the master's
			 * position (i.e to get a lock on the incoming data
			 * stream). We need to locate if we're either ahead or
			 * behind the master by <threshold>.
			 */

			should_locate = abs (delta) > locate_threshold;
		} else {

			/* we located to be ahead of the master's position (see
			 * the locate call in the next "else" scope where we
			 * jump ahead by a significant distance).
			 *
			 * So, we should be ahead (or behind) the master's
			 * position, and waiting for it to get close to us.
			 *
			 * We only need to locate again if we are actually
			 * behind (or ahead, for reverse motion) of the master
			 * by more than <threshold>.
			 */

			should_locate = delta < 0 && (abs (delta) > locate_threshold);
		}

		if (should_locate) {

			/* we're too far from the master to catch it via
			 * varispeed ... need to locate ahead of it, wait for
			 * it to get cose to us, then varispeed to sync.
			 *
			 * We assume that the transport state after the locate
			 * is always Stopped - we don't restart the transport
			 * until the master catches us, or at least gets close
			 * to our new position.
			 *
			 * Any time we locate, we need to reset the DLL used by
			 * the TransportMasterManager. Do that here, since the
			 * TMM will not need that again until after we start
			 * the locate (and hence the apparent transport
			 * position of the Session will reflect the target we
			 * set here). That is because the locate will be
			 * initiated in the Session::process() callback that is
			 * about to happen right after we return.
			 */

			tmm.reinit (master_speed, master_transport_sample);

			samplepos_t locate_target = master_transport_sample;

			/* locate to a position "worst_latency_preroll" head of
			 * the master, but also add in a generous estimate to
			 * cover the time it will take to locate to that
			 * position, based on our worst-case estimate for this
			 * session (so far).
			 */

			locate_target += wlp + lrintf (ntracks() * sample_rate() * (1.5 * (g_atomic_int_get (&_current_usecs_per_track) / 1000000.0)));

			DEBUG_TRACE (DEBUG::Slave, string_compose ("After locate-to-catch-master, still too far off (%1). Locate again to %2\n", delta, locate_target));

			transport_master_strategy.action = TransportMasterLocate;
			transport_master_strategy.target = locate_target;
			transport_master_strategy.roll_disposition = MustStop;
			transport_master_strategy.catch_speed = catch_speed;

			return 1.0;
		}

		if (delta > wlp) {

			/* We're close, but haven't reached the point where we
			 * need to start rolling for preroll latency yet.
			 */

			DEBUG_TRACE (DEBUG::Slave, string_compose ("master @ %1 is not yet within %2 of our position %3 (delta is %4)\n", master_transport_sample, wlp, _transport_sample, delta));
			return 1.0;
		}

		/* case #3: we should start rolling */

		DEBUG_TRACE (DEBUG::Slave, string_compose ("master @ %1 is WITHIN %2 of our position %3 (delta is %4), so start\n", master_transport_sample, wlp, _transport_sample, delta));

		if (delta > _remaining_latency_preroll) {
			/* increase pre-roll to match delta. this allows
			 * to directly catch the transport w/o vari-speed */
			_remaining_latency_preroll = delta;
		}

		transport_master_strategy.action = TransportMasterStart;
		transport_master_strategy.catch_speed = catch_speed;
		return catch_speed;

	}

	/* currently we're not waiting to sync with the master. So
	 * check if we're way out of alignment (case #1) or just a bit
	 * out of alignment (case #2)
	 */

	if (abs (delta) > locate_threshold) {

		/* CASE ONE
		 *
		 * This is a heuristic rather than a strictly provable rule. The idea
		 * is that if we're "far away" from the master, we should locate to its
		 * current position, and then varispeed to sync with it.
		 *
		 * On the other hand, if we're close to it, just varispeed.
		 */

		tmm.reinit (master_speed, master_transport_sample);

		samplepos_t locate_target = master_transport_sample;

		locate_target += wlp + lrintf (ntracks() * sample_rate() * (1.5 * (g_atomic_int_get (&_current_usecs_per_track) / 1000000.0)));

		DEBUG_TRACE (DEBUG::Slave, string_compose ("request locate to master position %1\n", locate_target));

		transport_master_strategy.action = TransportMasterLocate;
		transport_master_strategy.target = locate_target;
		transport_master_strategy.roll_disposition = (master_speed != 0) ? MustRoll : MustStop;
		transport_master_strategy.catch_speed = catch_speed;

		/* Session::process_with(out)_events() will take this
		 * up when called.
		 */

		return 1.0;

	} else if (abs (delta) > tmm.current()->resolution()) {

		/* CASE TWO
		 *
		 * If we're close, but not within the resolution of the
		 * master, just varispeed to chase the master, and be
		 * silent till we're synced
		 */

		tmm.block_disk_output ();

	} else {

		/* speed is set, we're locked and synced and good to go */

		if (!locate_pending() && !declick_in_progress()) {
			DEBUG_TRACE (DEBUG::Slave, "master/slave synced & locked\n");
			tmm.unblock_disk_output ();
		}
	}

	if (master_speed != 0.0) {

		/* master rolling, we should be too */

		if (_transport_fsm->transport_speed() == 0.0f) {
			DEBUG_TRACE (DEBUG::Slave, string_compose ("slave starts transport: %1 sample %2 tf %3\n", master_speed, master_transport_sample, _transport_sample));
			transport_master_strategy.action = TransportMasterStart;
			transport_master_strategy.catch_speed = catch_speed;
			return catch_speed;
		}

	} else if (!tmm.current()->starting()) { /* master stopped, not in "starting" state */

		if (_transport_fsm->transport_speed() != 0.0f) {
			DEBUG_TRACE (DEBUG::Slave, string_compose ("slave stops transport: %1 sample %2 tf %3\n", master_speed, master_transport_sample, _transport_sample));
			transport_master_strategy.action = TransportMasterStop;
			return catch_speed;
		}
	}

	/* we were not waiting for the master, we're close enough to
	 * it, and our transport state already matched the master
	 * (stopped or rolling). We should just continue
	 * resampling/varispeeding at "catch_speed" in order to remain
	 * synced with the master.
	 */

	transport_master_strategy.action = TransportMasterRelax;
	return catch_speed;
}

bool
Session::implement_master_strategy ()
{
	/* This is called from within Session::process(), only if we are using
	 * external sync. The task here is simply to implement whatever actions
	 * where decided by ::plan_master_strategy (), from within the
	 * ::process() callback (the planning step is executed before
	 * Session::process() begins.
	 */

	DEBUG_TRACE (DEBUG::Slave, string_compose ("Implementing master strategy: %1\n", transport_master_strategy.action));

	switch (transport_master_strategy.action) {
	case TransportMasterNoRoll:
		/* This is the one case where we do not want the session to
		   call ::roll() under any circumstances. Returning false here
		   will do that.
		*/
		return false;
	case TransportMasterRelax:
		break;
	case TransportMasterWait:
		break;
	case TransportMasterLocate:
		transport_master_strategy.action = TransportMasterWait;
		TFSM_LOCATE(transport_master_strategy.target, transport_master_strategy.roll_disposition, false, false);
		break;
	case TransportMasterStart:
		TFSM_EVENT (TransportFSM::StartTransport);
		break;
	case TransportMasterStop:
		cerr << "MASTER STOP\n";
		TFSM_STOP (false, false);
		break;
	}

	return true;
}

void
Session::sync_cues ()
{
	std::cerr << "Need to sync cues!\n";

	_locations->apply (*this, &Session::sync_cues_from_list);

}

struct LocationByTime
{
	bool operator() (Location const *a, Location const * b) {
		return a->start() < b->start();
	}
};

void
Session::sync_cues_from_list (Locations::LocationList const & locs)
{
	Locations::LocationList sorted (locs);
	LocationByTime cmp;

	sorted.sort (cmp);

	CueEvents::size_type n = 0;

	/* this leaves the capacity unchanged */
	_cue_events.clear ();

	for (auto const & loc : sorted) {

		if (loc->is_cue_marker()) {
			_cue_events.push_back (CueEvent (loc->cue_id(), loc->start_sample()));
		}

		if (++n >= _cue_events.capacity()) {
			break;
		}
	}
}

int32_t
Session::first_cue_within (samplepos_t s, samplepos_t e, bool& was_recorded)
{
	int32_t active_cue = _active_cue.load ();

	was_recorded = false;

	if (active_cue >= 0) {
		return active_cue;
	}

	if (!(Config->get_cue_behavior() & FollowCues)) {
		return -1;
	}

	CueEventTimeComparator cmp;
	CueEvents::iterator si = lower_bound (_cue_events.begin(), _cue_events.end(), s, cmp);

	if (si != _cue_events.end()) {
		if (si->time < e) {
			was_recorded = true;
			return si->cue;
		}
	}

	return -1;
}

void
Session::cue_marker_change (Location* /* ignored */)
{
	SessionEvent* ev = new SessionEvent (SessionEvent::SyncCues, SessionEvent::Add, SessionEvent::Immediate, 0, 0.0);
	queue_event (ev);
}

void
Session::cue_bang (int32_t cue)
{
	_pending_cue.store (cue);
}

void
Session::maybe_find_pending_cue ()
{
	int32_t ac = _pending_cue.exchange (-1);
	if (ac >= 0) {
		_active_cue.store (ac);

		if (TriggerBox::cue_recording()) {
			CueRecord cr (ac, _transport_sample);
			TriggerBox::cue_records.write (&cr, 1);
			/* failure is acceptable, but unlikely */
		}
	}
}

void
Session::clear_active_cue ()
{
	_active_cue.store (-1);
}
