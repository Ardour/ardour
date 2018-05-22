/*
    Copyright (C) 1999-2002 Paul Davis

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

#include <cmath>
#include <cerrno>
#include <algorithm>
#include <unistd.h>

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
#include "ardour/slave.h"
#include "ardour/ticker.h"
#include "ardour/types.h"
#include "ardour/vca.h"
#include "ardour/vca_manager.h"

#include "midi++/mmc.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;

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
		if (!_butler->transport_work_requested ()) {
			post_transport ();
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
	for (RouteList::const_iterator i = r->begin(); i != r->end(); ++i) {
		if ((*i)->apply_processor_changes_rt()) {
			_rt_emit_pending = true;
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

	samplepos_t end_sample = _transport_sample + nframes; // FIXME: varispeed + no_roll ??
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

	_global_locate_pending = locate_pending ();

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
				stop_transport ();
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

	end_sample = _transport_sample + samples_moved;

	{
		SessionEvent* this_event;
		Events::iterator the_next_one;

		if (!process_can_proceed()) {
			_silent = true;
			return;
		}

		if (!_exporting && _slave) {
			if (!follow_slave (nframes)) {
				return;
			}
		}

		if (_transport_speed == 0) {
			no_roll (nframes);
			return;
		}

		if (!_exporting && !timecode_transmission_suspended()) {
			send_midi_time_code_for_cycle (_transport_sample, end_sample, nframes);
		}

		ltc_tx_send_time_code_for_cycle (_transport_sample, end_sample, _target_transport_speed, _transport_speed, nframes);

		samplepos_t stop_limit = compute_stop_limit ();

		if (maybe_stop (stop_limit)) {
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

			/* running an event, position transport precisely to its time */
			if (this_event && this_event->action_sample <= end_sample && this_event->action_sample >= _transport_sample) {
				/* this isn't quite right for reverse play */
				samples_moved = (samplecnt_t) (this_event->action_sample - _transport_sample);
				this_nframes = abs (floor(samples_moved / _transport_speed));
			}

			try_run_lua (this_nframes);

			if (this_nframes) {

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

void
Session::reset_slave_state ()
{
	average_slave_delta = 1800;
	delta_accumulator_cnt = 0;
	have_first_delta_accumulator = false;
	_slave_state = Stopped;
	DiskReader::set_no_disk_output (false);
}

bool
Session::transport_locked () const
{
	Slave* sl = _slave;

	if (!locate_pending() && (!config.get_external_sync() || (sl && sl->ok() && sl->locked()))) {
		return true;
	}

	return false;
}

bool
Session::follow_slave (pframes_t nframes)
{
	double slave_speed;
	samplepos_t slave_transport_sample;
	samplecnt_t this_delta;
	int dir;

	if (!_slave->ok()) {
		stop_transport ();
		config.set_external_sync (false);
		goto noroll;
	}

	_slave->speed_and_position (slave_speed, slave_transport_sample);

	DEBUG_TRACE (DEBUG::Slave, string_compose ("Slave position %1 speed %2\n", slave_transport_sample, slave_speed));

	if (!_slave->locked()) {
		DEBUG_TRACE (DEBUG::Slave, "slave not locked\n");
		goto noroll;
	}

	if (slave_transport_sample > _transport_sample) {
		this_delta = slave_transport_sample - _transport_sample;
		dir = 1;
	} else {
		this_delta = _transport_sample - slave_transport_sample;
		dir = -1;
	}

	if (_slave->starting()) {
		slave_speed = 0.0f;
	}

	if (_slave->is_always_synced() ||
			(Config->get_timecode_source_is_synced() && (dynamic_cast<TimecodeSlave*>(_slave)) != 0)
			) {

		/* if the TC source is synced, then we assume that its
		   speed is binary: 0.0 or 1.0
		*/

		if (slave_speed != 0.0f) {
			slave_speed = 1.0f;
		}

	} else {

		/* if we are chasing and the average delta between us and the
		   master gets too big, we want to switch to silent
		   motion. so keep track of that here.
		*/

		if (_slave_state == Running) {
			calculate_moving_average_of_slave_delta(dir, abs(this_delta));
		}
	}

	track_slave_state (slave_speed, slave_transport_sample, this_delta);

	DEBUG_TRACE (DEBUG::Slave, string_compose ("slave state %1 @ %2 speed %3 cur delta %4 avg delta %5\n",
						   _slave_state, slave_transport_sample, slave_speed, this_delta, average_slave_delta));


	if (_slave_state == Running && !_slave->is_always_synced() && !(Config->get_timecode_source_is_synced() && (dynamic_cast<TimecodeSlave*>(_slave)) != 0)) {

		/* may need to varispeed to sync with slave */

		if (_transport_speed != 0.0f) {

			/*
			   note that average_dir is +1 or -1
			*/

			float delta;

			if (average_slave_delta == 0) {
				delta = this_delta;
				delta *= dir;
			} else {
				delta = average_slave_delta;
				delta *= average_dir;
			}

#ifndef NDEBUG
			if (slave_speed != 0.0) {
				DEBUG_TRACE (DEBUG::Slave, string_compose ("delta = %1 speed = %2 ts = %3 M@%4 S@%5 avgdelta %6\n",
									   (int) (dir * this_delta),
									   slave_speed,
									   _transport_speed,
									   _transport_sample,
									   slave_transport_sample,
									   average_slave_delta));
			}
#endif

			if (_slave->give_slave_full_control_over_transport_speed()) {
				set_transport_speed (slave_speed, 0, false, false);
				//std::cout << "set speed = " << slave_speed << "\n";
			} else {
				float adjusted_speed = slave_speed + (1.5 * (delta /  float(_current_sample_rate)));
				request_transport_speed (adjusted_speed);
				DEBUG_TRACE (DEBUG::Slave, string_compose ("adjust using %1 towards %2 ratio %3 current %4 slave @ %5\n",
									   delta, adjusted_speed, adjusted_speed/slave_speed, _transport_speed,
									   slave_speed));
			}

			if (!actively_recording() && (samplecnt_t) average_slave_delta > _slave->resolution()) {
				DEBUG_TRACE (DEBUG::Slave, string_compose ("average slave delta %1 greater than slave resolution %2 => no disk output\n", average_slave_delta, _slave->resolution()));
				/* run routes as normal, but no disk output */
				DiskReader::set_no_disk_output (true);
				return true;
			}

			if (!have_first_delta_accumulator) {
				DEBUG_TRACE (DEBUG::Slave, "waiting for first slave delta accumulator to be ready, no disk output\n");
				/* run routes as normal, but no disk output */
				DiskReader::set_no_disk_output (true);
				return true;
			}
		}
	}


	if (!have_first_delta_accumulator) {
		DEBUG_TRACE (DEBUG::Slave, "still waiting to compute slave delta, no disk output\n");
		DiskReader::set_no_disk_output (true);
	} else {
		DiskReader::set_no_disk_output (false);
	}

	if ((_slave_state == Running) && (0 == (post_transport_work () & ~PostTransportSpeed))) {
		/* speed is set, we're locked, and good to go */
		return true;
	}

  noroll:
	/* don't move at all */
	DEBUG_TRACE (DEBUG::Slave, "no roll\n")
	no_roll (nframes);
	return false;
}

void
Session::calculate_moving_average_of_slave_delta (int dir, samplecnt_t this_delta)
{
	if (delta_accumulator_cnt >= delta_accumulator_size) {
		have_first_delta_accumulator = true;
		delta_accumulator_cnt = 0;
	}

	if (delta_accumulator_cnt != 0 || this_delta < _current_sample_rate) {
		delta_accumulator[delta_accumulator_cnt++] = (samplecnt_t) dir *  (samplecnt_t) this_delta;
	}

	if (have_first_delta_accumulator) {
		average_slave_delta = 0L;
		for (int i = 0; i < delta_accumulator_size; ++i) {
			average_slave_delta += delta_accumulator[i];
		}
		average_slave_delta /= (int32_t) delta_accumulator_size;
		if (average_slave_delta < 0L) {
			average_dir = -1;
			average_slave_delta = average_slave_delta;
		} else {
			average_dir = 1;
		}
	}
}

void
Session::track_slave_state (float slave_speed, samplepos_t slave_transport_sample, samplecnt_t /*this_delta*/)
{
	if (slave_speed != 0.0f) {

		/* slave is running */

		switch (_slave_state) {
		case Stopped:
			if (_slave->requires_seekahead()) {
				slave_wait_end = slave_transport_sample + _slave->seekahead_distance ();
				DEBUG_TRACE (DEBUG::Slave, string_compose ("slave stopped, but running, requires seekahead to %1\n", slave_wait_end));
				/* we can call locate() here because we are in process context */
				locate (slave_wait_end, false, false);
				_slave_state = Waiting;

			} else {

				DEBUG_TRACE (DEBUG::Slave, string_compose ("slave stopped -> running at %1\n", slave_transport_sample));

				memset (delta_accumulator, 0, sizeof (int32_t) * delta_accumulator_size);
				average_slave_delta = 0L;

				Location* al = _locations->auto_loop_location();

				if (al && play_loop && (slave_transport_sample < al->start() || slave_transport_sample > al->end())) {
					// cancel looping
					request_play_loop(false);
				}

				if (slave_transport_sample != _transport_sample) {
					DEBUG_TRACE (DEBUG::Slave, string_compose ("require locate to run. eng: %1 -> sl: %2\n", _transport_sample, slave_transport_sample));
					locate (slave_transport_sample, false, false);
				}
				_slave_state = Running;
			}
			break;

		case Waiting:
		default:
			break;
		}

		if (_slave_state == Waiting) {

			DEBUG_TRACE (DEBUG::Slave, string_compose ("slave waiting at %1\n", slave_transport_sample));

			if (slave_transport_sample >= slave_wait_end) {

				DEBUG_TRACE (DEBUG::Slave, string_compose ("slave start at %1 vs %2\n", slave_transport_sample, _transport_sample));

				_slave_state = Running;

				/* now perform a "micro-seek" within the disk buffers to realign ourselves
				   precisely with the master.
				*/


				bool ok = true;
				samplecnt_t sample_delta = slave_transport_sample - _transport_sample;

				boost::shared_ptr<RouteList> rl = routes.reader();
				for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
					boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);
					if (tr && !tr->can_internal_playback_seek (sample_delta)) {
						ok = false;
						break;
					}
				}

				if (ok) {
					for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
						boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);
						if (tr) {
							tr->internal_playback_seek (sample_delta);
						}
					}
					_transport_sample += sample_delta;

				} else {
					cerr << "cannot micro-seek\n";
					/* XXX what? */
				}
			}
		}

		if (_slave_state == Running && _transport_speed == 0.0f) {
			DEBUG_TRACE (DEBUG::Slave, "slave starts transport\n");
			start_transport ();
		}

	} else { // slave_speed is 0

		/* slave has stopped */

		if (_transport_speed != 0.0f) {
			DEBUG_TRACE (DEBUG::Slave, string_compose ("slave stops transport: %1 sample %2 tf %3\n", slave_speed, slave_transport_sample, _transport_sample));
			stop_transport ();
		}

		if (slave_transport_sample != _transport_sample) {
			DEBUG_TRACE (DEBUG::Slave, string_compose ("slave stopped, move to %1\n", slave_transport_sample));
			force_locate (slave_transport_sample, false);
		}

		reset_slave_state();
	}
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

	if (!_exporting && _slave) {
		if (!follow_slave (nframes)) {
			ltc_tx_send_time_code_for_cycle (_transport_sample, _transport_sample, 0, 0 , nframes);
			return;
		}
	}

	if (_transport_speed == 0) {
		no_roll (nframes);
		return;
	}

	if (_transport_speed == 1.0) {
		samples_moved = (samplecnt_t) nframes;
	} else {
		interpolation.set_speed (_transport_speed);
		samples_moved = interpolation.distance (nframes);
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
	} else if (samples_moved) {
		increment_transport_position (samples_moved);
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
	if (_state_of_the_state & Deletion) {
		return;
	} else if (_state_of_the_state & Loading) {
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
		set_play_loop (ev->yes_or_no, ev->speed);
		break;

	case SessionEvent::AutoLoop:
		if (play_loop) {
			/* roll after locate, do not flush, set "with loop"
			   true only if we are seamless looping
			*/
			start_locate (ev->target_sample, true, false, Config->get_seamless_loop());
		}
		remove = false;
		del = false;
		break;

	case SessionEvent::Locate:
		if (ev->yes_or_no) { /* force locate */
			/* args: do not roll after locate, do flush, not with loop */
			locate (ev->target_sample, false, true, false);
		} else {
			/* args: do not roll after locate, do flush, not with loop */
			start_locate (ev->target_sample, false, true, false);
		}
		_send_timecode_update = true;
		break;

	case SessionEvent::LocateRoll:
		if (ev->yes_or_no) {
			/* args: roll after locate, do flush, not with loop */
			locate (ev->target_sample, true, true, false);
		} else {
			/* args: roll after locate, do flush, not with loop */
			start_locate (ev->target_sample, true, true, false);
		}
		_send_timecode_update = true;
		break;

	case SessionEvent::Skip:
		if (Config->get_skip_playback()) {
			start_locate (ev->target_sample, true, true, false);
			_send_timecode_update = true;
		}
		remove = false;
		del = false;
		break;

	case SessionEvent::LocateRollLocate:
		// locate is handled by ::request_roll_at_and_return()
		_requested_return_sample = ev->target_sample;
		request_locate (ev->target2_sample, true);
		break;


	case SessionEvent::SetTransportSpeed:
		set_transport_speed (ev->speed, ev->target_sample, ev->yes_or_no, ev->second_yes_or_no, ev->third_yes_or_no);
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

	case SessionEvent::StopOnce:
		if (!non_realtime_work_pending()) {
			_clear_event_type (SessionEvent::StopOnce);
			stop_transport (ev->yes_or_no);
		}
		remove = false;
		del = false;
		break;

	case SessionEvent::RangeStop:
		if (!non_realtime_work_pending()) {
			stop_transport (ev->yes_or_no);
		}
		remove = false;
		del = false;
		break;

	case SessionEvent::RangeLocate:
		/* args: roll after locate, do flush, not with loop */
		start_locate (ev->target_sample, true, true, false);
		remove = false;
		del = false;
		break;

	case SessionEvent::Overwrite:
		overwrite_some_buffers (static_cast<Track*>(ev->ptr));
		break;

	case SessionEvent::SetSyncSource:
		DEBUG_TRACE (DEBUG::Slave, "seen request for new slave\n");
		use_sync_source (ev->slave);
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

	if (_slave) {
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
