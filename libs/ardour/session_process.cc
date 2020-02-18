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

#include <glibmm/threads.h>

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
#include "ardour/types.h"
#include "ardour/vca.h"
#include "ardour/vca_manager.h"

#include "midi++/mmc.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;

#define TFSM_EVENT(evtype) { _transport_fsm->enqueue (new TransportFSM::Event (evtype)); }
#define TFSM_STOP(abort,clear) { _transport_fsm->enqueue (new TransportFSM::Event (TransportFSM::StopTransport,abort,clear)); }
#define TFSM_LOCATE(target,ltd,flush,loop,force) { _transport_fsm->enqueue (new TransportFSM::Event (TransportFSM::Locate,target,ltd,flush,loop,force)); }


/** Called by the audio engine when there is work to be done with JACK.
 * @param nframes Number of samples to process.
 */

void
Session::process (pframes_t nframes)
{
	samplepos_t transport_at_start = _transport_sample;

	_silent = false;

	if (processing_blocked()) {
		_silent = true;
		return;
	}

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
	boost::shared_ptr<RouteList> r = routes.reader ();
	bool one_or_more_routes_declicking = false;
	for (RouteList::const_iterator i = r->begin(); i != r->end(); ++i) {
		if ((*i)->apply_processor_changes_rt()) {
			_rt_emit_pending = true;
		}
		if ((*i)->declick_in_progress()) {
			one_or_more_routes_declicking = true;
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
		if (!_silent && !_engine.freewheeling() && Config->get_send_midi_clock() && (transport_speed() == 1.0f || transport_speed() == 0.0f) && midi_clock->has_midi_port()) {
			midi_clock->tick (transport_at_start, nframes);
		}

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

	samplepos_t end_sample = _transport_sample + floor (nframes * _transport_speed);
	int ret = 0;
	boost::shared_ptr<RouteList> r = routes.reader ();

	if (_click_io) {
		_click_io->silence (nframes);
	}

	ltc_tx_send_time_code_for_cycle (_transport_sample, end_sample, _target_transport_speed, _transport_speed, nframes);

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
	boost::shared_ptr<RouteList> r = routes.reader ();

	const samplepos_t start_sample = _transport_sample;
	const samplepos_t end_sample = _transport_sample + floor (nframes * _transport_speed);

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
	if (_transport_speed == 0.0 || _count_in_samples > 0 || _remaining_latency_preroll > 0) {
		/* cannot compute audible delta, because the session is
		   generating silence that does not correspond to the timeline,
		   but is instead filling playback buffers to manage latency
		   alignment.
		*/
		DEBUG_TRACE (DEBUG::Slave, string_compose ("still adjusting for latency (%1) and/or count-in (%2) or stopped %1\n", _remaining_latency_preroll, _count_in_samples, _transport_speed));
		return false;
	}

	pos_and_delta -= _transport_sample;
	return true;
}

/** Process callback used when the auditioner is not active */
void
Session::process_with_events (pframes_t nframes)
{
	PT_TIMING_CHECK (3);

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
	if (_transport_speed != 1.0 && _count_in_samples > 0) {
		_count_in_samples = 0;
	}
	if (_transport_speed == 0.0) {
		_remaining_latency_preroll = 0;
	}

	assert (_count_in_samples == 0 || _remaining_latency_preroll == 0 || _count_in_samples == _remaining_latency_preroll);

	// DEBUG_TRACE (DEBUG::Transport, string_compose ("Running count in/latency preroll of %1 & %2\n", _count_in_samples, _remaining_latency_preroll));

	while (_count_in_samples > 0 || _remaining_latency_preroll > 0) {
		samplecnt_t ns;

		if (_remaining_latency_preroll > 0) {
			ns = std::min ((samplecnt_t)nframes, _remaining_latency_preroll);
		} else {
			ns = std::min ((samplecnt_t)nframes, _count_in_samples);
		}

		boost::shared_ptr<RouteList> r = routes.reader ();
		for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
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

	if (_transport_speed != 0) {
		_send_qf_mtc = (
			Config->get_send_mtc () &&
			_transport_speed >= (1 - tolerance) &&
			_transport_speed <= (1 + tolerance)
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

	assert (_transport_speed == 0 || _transport_speed == 1.0 || _transport_speed == -1.0);

	samples_moved = (samplecnt_t) nframes * _transport_speed;
	// DEBUG_TRACE (DEBUG::Transport, string_compose ("plan to move transport by %1 (%2 @ %3)\n", samples_moved, nframes, _transport_speed));

	end_sample = _transport_sample + samples_moved;

	{
		SessionEvent* this_event;
		Events::iterator the_next_one;

		if (!process_can_proceed()) {
			_silent = true;
			return;
		}

		if (!_exporting && config.get_external_sync()) {
			if (!follow_transport_master (nframes)) {
				ltc_tx_send_time_code_for_cycle (_transport_sample, end_sample, _target_transport_speed, _transport_speed, nframes);
				return;
			}
		}

		if (_transport_speed == 0) {
			no_roll (nframes);
			return;
		}


		samplepos_t stop_limit = compute_stop_limit ();

		if (maybe_stop (stop_limit)) {
			if (!_exporting && !timecode_transmission_suspended()) {
				send_midi_time_code_for_cycle (_transport_sample, end_sample, nframes);
			}
			ltc_tx_send_time_code_for_cycle (_transport_sample, end_sample, _target_transport_speed, _transport_speed, nframes);

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
			samples_moved = (samplecnt_t) floor (_transport_speed * nframes); /* transport relative */
			// DEBUG_TRACE (DEBUG::Transport, string_compose ("sub-loop plan to move transport by %1 (%2 @ %3)\n", samples_moved, nframes, _transport_speed));

			/* running an event, position transport precisely to its time */
			if (this_event && this_event->action_sample <= end_sample && this_event->action_sample >= _transport_sample) {
				/* this isn't quite right for reverse play */
				samples_moved = (samplecnt_t) (this_event->action_sample - _transport_sample);
				// DEBUG_TRACE (DEBUG::Transport, string_compose ("sub-loop2 (for %4)plan to move transport by %1 (%2 @ %3)\n", samples_moved, nframes, _transport_speed, enum_2_string (this_event->type)));
				this_nframes = abs (floor(samples_moved / _transport_speed));
			}

			try_run_lua (this_nframes);

			if (this_nframes) {

				if (!_exporting && !timecode_transmission_suspended()) {
					send_midi_time_code_for_cycle (_transport_sample, _transport_sample + samples_moved, this_nframes);
				}

				ltc_tx_send_time_code_for_cycle (_transport_sample,  _transport_sample + samples_moved, _target_transport_speed, _transport_speed, this_nframes);

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
			end_sample = _transport_sample + floor (nframes * _transport_speed);
		}

		set_next_event ();

	} /* implicit release of route lock */

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
	bool session_needs_butler = false;
	samplecnt_t samples_moved;

	if (!process_can_proceed()) {
		_silent = true;
		return;
	}

	if (!_exporting && config.get_external_sync()) {
		if (!follow_transport_master (nframes)) {
			ltc_tx_send_time_code_for_cycle (_transport_sample, _transport_sample, 0, 0 , nframes);
			return;
		}
	}

	assert (_transport_speed == 0 || _transport_speed == 1.0 || _transport_speed == -1.0);

	if (_transport_speed == 0) {
		no_roll (nframes);
		return;
	} else {
		samples_moved = (samplecnt_t) nframes * _transport_speed;
	}

	if (!_exporting && !timecode_transmission_suspended()) {
		send_midi_time_code_for_cycle (_transport_sample, _transport_sample + samples_moved, nframes);
	}

	ltc_tx_send_time_code_for_cycle (_transport_sample, _transport_sample + samples_moved, _target_transport_speed, _transport_speed, nframes);

	samplepos_t const stop_limit = compute_stop_limit ();

	if (maybe_stop (stop_limit)) {
		no_roll (nframes);
		return;
	}

	if (maybe_sync_start (nframes)) {
		return;
	}

	click (_transport_sample, nframes);

	if (process_routes (nframes, session_needs_butler)) {
		fail_roll (nframes);
		return;
	}

	get_track_statistics ();

	if (samples_moved < 0) {
		decrement_transport_position (-samples_moved);
		//DEBUG_TRACE (DEBUG::Transport, string_compose ("DEcrement transport by %1 to %2\n", samples_moved, _transport_sample));
	} else if (samples_moved) {
		increment_transport_position (samples_moved);
		//DEBUG_TRACE (DEBUG::Transport, string_compose ("INcrement transport by %1 to %2\n", samples_moved, _transport_sample));
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

	/* run the auditioner, and if it says we need butler service, ask for it */

	if (auditioner->play_audition (nframes) > 0) {
		DEBUG_TRACE (DEBUG::Butler, "auditioner needs butler, call it\n");
		_butler->summon ();
	}

	/* if using a monitor section, run it because otherwise we don't hear anything */

	if (_monitor_out && auditioner->needs_monitor()) {
		_monitor_out->monitor_run (_transport_sample, _transport_sample + nframes, nframes);
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
		set_play_loop (ev->yes_or_no, true);
		break;

	case SessionEvent::AutoLoop:
		if (play_loop) {
			/* roll after locate, do not flush, set "for loop end" true
			*/
			TFSM_LOCATE (ev->target_sample, MustRoll, false, true, false);
		}
		remove = false;
		del = false;
		break;

	case SessionEvent::Locate:
		/* args: do not roll after locate, clear state, not for loop, force */
		TFSM_LOCATE (ev->target_sample, ev->locate_transport_disposition, true, false, ev->yes_or_no);
		_send_timecode_update = true;
		break;

	case SessionEvent::LocateRoll:
		/* args: roll after locate, clear state if not looping, not for loop, force */
		TFSM_LOCATE (ev->target_sample, ev->locate_transport_disposition, !play_loop, false, ev->yes_or_no);
		_send_timecode_update = true;
		break;

	case SessionEvent::Skip:
		if (Config->get_skip_playback()) {
			TFSM_LOCATE (ev->target_sample, MustRoll, true, false, false);
			_send_timecode_update = true;
		}
		remove = false;
		del = false;
		break;

	case SessionEvent::LocateRollLocate:
		// locate is handled by ::request_roll_at_and_return()
		_requested_return_sample = ev->target_sample;
		TFSM_LOCATE (ev->target2_sample, MustRoll, true, false, false);
		_send_timecode_update = true;
		break;


	case SessionEvent::SetTransportSpeed:
		set_transport_speed (ev->speed, ev->target_sample, ev->yes_or_no, ev->second_yes_or_no, ev->third_yes_or_no);
		break;

	case SessionEvent::SetTransportMaster:
		TransportMasterManager::instance().set_current (ev->transport_master);
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
		TFSM_STOP (ev->yes_or_no, false);
		remove = false;
		del = false;
		break;

	case SessionEvent::RangeLocate:
		/* args: roll after locate, do flush, not with loop */
		TFSM_LOCATE (ev->target_sample, MustRoll, true, false, false);
		remove = false;
		del = false;
		break;

	case SessionEvent::Overwrite:
		overwrite_some_buffers (ev->track, ev->overwrite);
		break;

	case SessionEvent::Audition:
		set_audition (ev->region);
		// drop reference to region
		ev->region.reset ();
		break;

	case SessionEvent::SetPlayAudioRange:
		set_play_range (ev->audio_range, (ev->speed == 1.0f));
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

	default:
	  fatal << string_compose(_("Programming error: illegal event type in process_event (%1)"), ev->type) << endmsg;
		abort(); /*NOTREACHED*/
		break;
	};

	if (remove) {
		del = del && !_remove_event (ev);
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

bool
Session::follow_transport_master (pframes_t nframes)
{
	TransportMasterManager& tmm (TransportMasterManager::instance());

	double master_speed;
	samplepos_t master_transport_sample;
	sampleoffset_t delta;

	if (tmm.master_invalid_this_cycle()) {
		DEBUG_TRACE (DEBUG::Slave, "session told not to use the transport master this cycle\n");
		goto noroll;
	}

	master_speed = tmm.get_current_speed_in_process_context();
	master_transport_sample = tmm.get_current_position_in_process_context ();
	delta = _transport_sample - master_transport_sample;

	DEBUG_TRACE (DEBUG::Slave, string_compose ("session at %1, master at %2, delta: %3 res: %4 TFSM state %5\n", _transport_sample, master_transport_sample, delta, tmm.current()->resolution(), _transport_fsm->current_state()));

	if (tmm.current()->type() == Engine) {

		/* JACK Transport. */

		if (master_speed == 0) {

			if (!actively_recording()) {

				const samplecnt_t wlp = worst_latency_preroll_buffer_size_ceil ();

				if (delta != wlp) {

					/* if we're not aligned with the current JACK * time, then jump to it */

					if (!locate_pending() && !declick_in_progress() && !tmm.current()->starting()) {

						const samplepos_t locate_target = master_transport_sample + wlp;
						DEBUG_TRACE (DEBUG::Slave, string_compose ("JACK transport: jump to master position %1 by locating to %2\n", master_transport_sample, locate_target));
						/* for JACK transport always stop after the locate (2nd argument == false) */
						TFSM_LOCATE (locate_target, MustStop, true, false, false);

					} else {
						DEBUG_TRACE (DEBUG::Slave, string_compose ("JACK Transport: locate already in process, sts = %1\n", master_transport_sample));
					}
				}
			}

		} else {

			if (_transport_speed) {
				/* master is rolling, and we're rolling ... with JACK we should always be perfectly in sync, so ... WTF? */
				if (delta) {
					if (remaining_latency_preroll() && worst_latency_preroll()) {
						/* our transport position is not moving because we're doing latency alignment. Nothing in particular to do */
					} else {
						cerr << "\n\n\n IMPOSSIBLE! OUT OF SYNC WITH JACK TRANSPORT (rlp = " << remaining_latency_preroll() << " wlp " << worst_latency_preroll() << ")\n\n\n";
					}
				}
			}
		}

	} else {

		/* This is a heuristic rather than a strictly provable rule. The idea
		 * is that if we're "far away" from the master, we should locate to its
		 * current position, and then varispeed to sync with it.
		 *
		 * On the other hand, if we're close to it, just varispeed.
		 */

		if (!actively_recording() && abs (delta) > (5 * current_block_size)) {

			DiskReader::inc_no_disk_output ();

			if (!locate_pending() && !declick_in_progress()) {
				DEBUG_TRACE (DEBUG::Slave, string_compose ("request locate to master position %1\n", master_transport_sample));
				/* note that for non-JACK transport masters, we assume that the transport state (rolling,stopped) after the locate
				 * remains unchanged (2nd argument, "roll-after-locate")
				 */
				TFSM_LOCATE (master_transport_sample, (master_speed != 0) ? MustRoll : MustStop, true, false, false);
			}

			return true;
		}
	}

	if (master_speed != 0.0) {
		if (_transport_speed == 0.0f) {
			DEBUG_TRACE (DEBUG::Slave, string_compose ("slave starts transport: %1 sample %2 tf %3\n", master_speed, master_transport_sample, _transport_sample));
			TFSM_EVENT (TransportFSM::StartTransport);
		}

	} else if (!tmm.current()->starting()) { /* master stopped, not in "starting" state */

		if (_transport_speed != 0.0f) {
			DEBUG_TRACE (DEBUG::Slave, string_compose ("slave stops transport: %1 sample %2 tf %3\n", master_speed, master_transport_sample, _transport_sample));
			TFSM_STOP (false, false);
		}
	}

	/* This is the second part of the "we're not synced yet" code. If we're
	 * close, but not within the resolution of the master, silence disk
	 * output but continue to varispeed to get in sync.
	 */

	if ((tmm.current()->type() != Engine) && !actively_recording() && abs (delta) > tmm.current()->resolution()) {
		/* just varispeed to chase the master, and be silent till we're synced */
		DiskReader::inc_no_disk_output ();
		return true;
	}

	/* speed is set, we're locked, and good to go */
	DiskReader::dec_no_disk_output ();
	return true;

  noroll:
	/* don't move at all */
	DEBUG_TRACE (DEBUG::Slave, "no roll\n")
	no_roll (nframes);
	return false;
}

void
Session::reset_slave_state ()
{
	DiskReader::dec_no_disk_output ();
}
