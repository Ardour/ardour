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

#include <pbd/error.h>

#include <glibmm/thread.h>

#include <ardour/ardour.h>
#include <ardour/session.h>
#include <ardour/timestamps.h>
#include <ardour/audio_diskstream.h>
#include <ardour/audioengine.h>
#include <ardour/slave.h>
#include <ardour/auditioner.h>
#include <ardour/cycles.h>
#include <ardour/cycle_timer.h>

#include <midi++/manager.h>

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;

/** Called by the audio engine when there is work to be done with JACK.
 * @param nframes Number of frames to process.
 */
void
Session::process (nframes_t nframes)
{
	MIDI::Manager::instance()->cycle_start(nframes);

	_silent = false;

	if (synced_to_jack() && waiting_to_start) {
		if ( _engine.transport_state() == AudioEngine::TransportRolling) {
			actually_start_transport ();
		}
	}

	if (non_realtime_work_pending()) {
		if (!transport_work_requested ()) {
			post_transport ();
		} 
	} 
	
	(this->*process_function) (nframes);
	
	MIDI::Manager::instance()->cycle_end();

	SendFeedback (); /* EMIT SIGNAL */
}

void
Session::prepare_diskstreams ()
{
	boost::shared_ptr<DiskstreamList> dsl = diskstreams.reader();
	for (DiskstreamList::iterator i = dsl->begin(); i != dsl->end(); ++i) {
		(*i)->prepare ();
	}
}

int
Session::no_roll (nframes_t nframes, nframes_t offset)
{
	nframes_t end_frame = _transport_frame + nframes; // FIXME: varispeed + no_roll ??
	int ret = 0;
	bool declick = get_transport_declick_required();
	boost::shared_ptr<RouteList> r = routes.reader ();

	if (_click_io) {
		_click_io->silence (nframes, offset);
	}

	if (g_atomic_int_get (&processing_prohibited)) {
		for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
			(*i)->silence (nframes, offset);
		}
		return 0;
	}

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		
		(*i)->prepare_inputs (nframes, offset);

		if ((*i)->is_hidden()) {
			continue;
		}
		
		(*i)->set_pending_declick (declick);
		
		if ((*i)->no_roll (nframes, _transport_frame, end_frame, offset, non_realtime_work_pending(), 
				   actively_recording(), declick)) {
			error << string_compose(_("Session: error in no roll for %1"), (*i)->name()) << endmsg;
			ret = -1;
			break;
		}
	}

	return ret;
}

int
Session::process_routes (nframes_t nframes, nframes_t offset)
{
	bool record_active;
	int  declick = get_transport_declick_required();
	bool rec_monitors = get_rec_monitors_input();
	boost::shared_ptr<RouteList> r = routes.reader ();

	if (transport_sub_state & StopPendingCapture) {
		/* force a declick out */
		declick = -1;
	}

	record_active = actively_recording(); // || (get_record_enabled() && get_punch_in());

	const nframes_t start_frame = _transport_frame;
	const nframes_t end_frame = _transport_frame + (nframes_t)floor(nframes * _transport_speed);

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {

		int ret;

		(*i)->prepare_inputs (nframes, offset);

		if ((*i)->is_hidden()) {
			continue;
		}

		(*i)->set_pending_declick (declick);

		if ((ret = (*i)->roll (nframes, start_frame, end_frame, offset, declick, record_active, rec_monitors)) < 0) {

			/* we have to do this here. Route::roll() for an AudioTrack will have called AudioDiskstream::process(),
			   and the DS will expect AudioDiskstream::commit() to be called. but we're aborting from that
			   call path, so make sure we release any outstanding locks here before we return failure.
			*/

			boost::shared_ptr<DiskstreamList> dsl = diskstreams.reader();
			for (DiskstreamList::iterator ids = dsl->begin(); ids != dsl->end(); ++ids) {
				(*ids)->recover ();
			}

			stop_transport ();
			return -1;
		} 
	}

	return 0;
}

int
Session::silent_process_routes (nframes_t nframes, nframes_t offset)
{
	bool record_active = actively_recording();
	int  declick = get_transport_declick_required();
	bool rec_monitors = get_rec_monitors_input();
	boost::shared_ptr<RouteList> r = routes.reader ();

	if (transport_sub_state & StopPendingCapture) {
		/* force a declick out */
		declick = -1;
	}
	
	const nframes_t start_frame = _transport_frame;
	const nframes_t end_frame = _transport_frame + lrintf(nframes * _transport_speed);

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {

		int ret;

		if ((*i)->is_hidden()) {
			continue;
		}

		if ((ret = (*i)->silent_roll (nframes, start_frame, end_frame, offset, record_active, rec_monitors)) < 0) {
			
			/* we have to do this here. Route::roll() for an AudioTrack will have called AudioDiskstream::process(),
			   and the DS will expect AudioDiskstream::commit() to be called. but we're aborting from that
			   call path, so make sure we release any outstanding locks here before we return failure.
			*/

			boost::shared_ptr<DiskstreamList> dsl = diskstreams.reader();
			for (DiskstreamList::iterator ids = dsl->begin(); ids != dsl->end(); ++ids) {
				(*ids)->recover ();
			}

			stop_transport ();
			return -1;
		} 
	}

	return 0;
}

void
Session::commit_diskstreams (nframes_t nframes, bool &needs_butler)
{
	int dret;
	float pworst = 1.0f;
	float cworst = 1.0f;

	boost::shared_ptr<DiskstreamList> dsl = diskstreams.reader();
	for (DiskstreamList::iterator i = dsl->begin(); i != dsl->end(); ++i) {

		if ((*i)->hidden()) {
			continue;
		}
		
		/* force all diskstreams not handled by a Route to call do their stuff.
		   Note: the diskstreams that were handled by a route will just return zero
		   from this call, because they know they were processed. So in fact, this
		   also runs commit() for every diskstream.
		 */

		if ((dret = (*i)->process (_transport_frame, nframes, 0, actively_recording(), get_rec_monitors_input())) == 0) {
			if ((*i)->commit (nframes)) {
				needs_butler = true;
			}

		} else if (dret < 0) {
			(*i)->recover();
		}
		
		pworst = min (pworst, (*i)->playback_buffer_load());
		cworst = min (cworst, (*i)->capture_buffer_load());
	}

	uint32_t pmin = g_atomic_int_get (&_playback_load);
	uint32_t pminold = g_atomic_int_get (&_playback_load_min);
	uint32_t cmin = g_atomic_int_get (&_capture_load);
	uint32_t cminold = g_atomic_int_get (&_capture_load_min);

	g_atomic_int_set (&_playback_load, (uint32_t) floor (pworst * 100.0f));
	g_atomic_int_set (&_capture_load, (uint32_t) floor (cworst * 100.0f));
	g_atomic_int_set (&_playback_load_min, min (pmin, pminold));
	g_atomic_int_set (&_capture_load_min, min (cmin, cminold));

	if (actively_recording()) {
		set_dirty();
	}
}

/** Process callback used when the auditioner is not active */
void
Session::process_with_events (nframes_t nframes)
{
	Event*         ev;
	nframes_t this_nframes;
	nframes_t end_frame;
	nframes_t offset;
	bool session_needs_butler = false;
	nframes_t stop_limit;
	long           frames_moved;
	
	/* make sure the auditioner is silent */

	if (auditioner) {
		auditioner->silence (nframes, 0);
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
		Event *ev = immediate_events.front ();
		immediate_events.pop_front ();
		process_event (ev);
	}

	/* Events caused a transport change, send an MTC Full Frame (SMPTE) message.
	 * This is sent whether rolling or not, to give slaves an idea of ardour time
	 * on locates (and allow slow slaves to position and prepare for rolling)
	 */
	if (_send_smpte_update) {
		send_full_time_code(nframes);
	}

	if (!process_can_proceed()) {
		_silent = true;
		return;
	}

	if (events.empty() || next_event == events.end()) {
		process_without_events (nframes);
		return;
	}

	end_frame = _transport_frame + (nframes_t)abs(floor(nframes * _transport_speed));

	{
		Event* this_event;
		Events::iterator the_next_one;
		
		if (!process_can_proceed()) {
			_silent = true;
			return;
		}
		
		if (!_exporting && _slave) {
			if (!follow_slave (nframes, 0)) {
				return;
			}
		} 

		if (_transport_speed == 0) {
			no_roll (nframes, 0);
			return;
		}
	
		if (!_exporting) {
			send_midi_time_code_for_cycle (nframes);
		}

		if (actively_recording()) {
			stop_limit = max_frames;
		} else {

			if (Config->get_stop_at_session_end()) {
				stop_limit = current_end_frame();
			} else {
				stop_limit = max_frames;
			}
		}

		if (maybe_stop (stop_limit)) {
			no_roll (nframes, 0);
			return;
		} 

		this_event = *next_event;
		the_next_one = next_event;
		++the_next_one;

		offset = 0;

		/* yes folks, here it is, the actual loop where we really truly
		   process some audio */
		while (nframes) {

			this_nframes = nframes; /* real (jack) time relative */
			frames_moved = (long) floor (_transport_speed * nframes); /* transport relative */

			/* running an event, position transport precisely to its time */
			if (this_event && this_event->action_frame <= end_frame && this_event->action_frame >= _transport_frame) {
				/* this isn't quite right for reverse play */
				frames_moved = (long) (this_event->action_frame - _transport_frame);
				this_nframes = (nframes_t) abs( floor(frames_moved / _transport_speed) );
			} 

			if (this_nframes) {
				
				click (_transport_frame, nframes, offset);
				
				/* now process frames between now and the first event in this block */
				prepare_diskstreams ();

				if (process_routes (this_nframes, offset)) {
					no_roll (nframes, 0);
					return;
				}
				
				commit_diskstreams (this_nframes, session_needs_butler);

				nframes -= this_nframes;
				offset += this_nframes;
				
				if (frames_moved < 0) {
					decrement_transport_position (-frames_moved);
				} else {
					increment_transport_position (frames_moved);
				}

				maybe_stop (stop_limit);
				check_declick_out ();
			}

			/* now handle this event and all others scheduled for the same time */
			
			while (this_event && this_event->action_frame == _transport_frame) {
				process_event (this_event);

				if (the_next_one == events.end()) {
					this_event = 0;
				} else {
					this_event = *the_next_one;
					++the_next_one;
				}
			} 

			/* if an event left our state changing, do the right thing */

			if (non_realtime_work_pending()) {
				no_roll (nframes, offset);
				break;
			}

			/* this is necessary to handle the case of seamless looping */
			end_frame = _transport_frame + (nframes_t) floor (nframes * _transport_speed);
			
		}

		set_next_event ();

	} /* implicit release of route lock */

	if (session_needs_butler)
		summon_butler ();
}

void
Session::reset_slave_state ()
{
	average_slave_delta = 1800;
	delta_accumulator_cnt = 0;
	have_first_delta_accumulator = false;
	slave_state = Stopped;
}

bool
Session::transport_locked () const
{
	Slave* sl = _slave;

	if (!locate_pending() && ((Config->get_slave_source() == None) || (sl && sl->ok() && sl->locked()))) {
		return true;
	}

	return false;
}

bool
Session::follow_slave (nframes_t nframes, nframes_t offset)
{
	float slave_speed;
	nframes_t slave_transport_frame;
	nframes_t this_delta;
	int dir;
	bool starting;

	if (!_slave->ok()) {
		stop_transport ();
		Config->set_slave_source (None);
		goto noroll;
	}
	
	_slave->speed_and_position (slave_speed, slave_transport_frame);

	if (!_slave->locked()) {
		goto noroll;
	}

	if (slave_transport_frame > _transport_frame) {
		this_delta = slave_transport_frame - _transport_frame;
		dir = 1;
	} else {
		this_delta = _transport_frame - slave_transport_frame;
		dir = -1;
	}

	if ((starting = _slave->starting())) {
		slave_speed = 0.0f;
	}

#if 0
	cerr << "delta = " << (int) (dir * this_delta)
	     << " speed = " << slave_speed 
	     << " ts = " << _transport_speed 
	     << " M@ "<< slave_transport_frame << " S@ " << _transport_frame 
	     << " avgdelta = " << average_slave_delta 
	     << endl;
#endif	

	if (_slave->is_always_synced() || Config->get_timecode_source_is_synced()) {

		/* if the TC source is synced, then we assume that its 
		   speed is binary: 0.0 or 1.0
		*/

		if (slave_speed != 0.0f) {
			slave_speed = 1.0f;
		} 

	} else {

		/* TC source is able to drift relative to us (slave)
		   so we need to keep track of the drift and adjust
		   our speed to remain locked.
		*/

		if (delta_accumulator_cnt >= delta_accumulator_size) {
			have_first_delta_accumulator = true;
			delta_accumulator_cnt = 0;
		}

		if (delta_accumulator_cnt != 0 || this_delta < _current_frame_rate) {
			delta_accumulator[delta_accumulator_cnt++] = dir*this_delta;
		}
		
		if (have_first_delta_accumulator) {
			average_slave_delta = 0;
			for (int i = 0; i < delta_accumulator_size; ++i) {
				average_slave_delta += delta_accumulator[i];
			}
			average_slave_delta /= delta_accumulator_size;
			if (average_slave_delta < 0) {
				average_dir = -1;
				average_slave_delta = -average_slave_delta;
			} else {
				average_dir = 1;
			}
			// cerr << "avgdelta = " << average_slave_delta*average_dir << endl;
		}
	}

	if (slave_speed != 0.0f) {

		/* slave is running */

		switch (slave_state) {
		case Stopped:
			if (_slave->requires_seekahead()) {
				slave_wait_end = slave_transport_frame + _current_frame_rate;
				locate (slave_wait_end, false, false);
				slave_state = Waiting;
				starting = true;

			} else {

				slave_state = Running;

				Location* al = _locations.auto_loop_location();

				if (al && play_loop && (slave_transport_frame < al->start() || slave_transport_frame > al->end())) {
					// cancel looping
					request_play_loop(false);
				}

				if (slave_transport_frame != _transport_frame) {
					locate (slave_transport_frame, false, false);
				}
			}
			break;

		case Waiting:
			break;

		default:
			break;

		}

		if (slave_state == Waiting) {

			// cerr << "waiting at " << slave_transport_frame << endl;
			if (slave_transport_frame >= slave_wait_end) {
				// cerr << "\tstart at " << _transport_frame << endl;

				slave_state = Running;

				bool ok = true;
				nframes_t frame_delta = slave_transport_frame - _transport_frame;

				boost::shared_ptr<DiskstreamList> dsl = diskstreams.reader();
				
				for (DiskstreamList::iterator i = dsl->begin(); i != dsl->end(); ++i) {
					if (!(*i)->can_internal_playback_seek (frame_delta)) {
						ok = false;
						break;
					}
				}

				if (ok) {
					for (DiskstreamList::iterator i = dsl->begin(); i != dsl->end(); ++i) {
						(*i)->internal_playback_seek (frame_delta);
					}
					_transport_frame += frame_delta;
				       
				} else {
					// cerr << "cannot micro-seek\n";
					/* XXX what? */
				}

				memset (delta_accumulator, 0, sizeof (nframes_t) * delta_accumulator_size);
				average_slave_delta = 0;
				this_delta = 0;
			}
		}
		
		if (slave_state == Running && _transport_speed == 0.0f) {
			
			// cerr << "slave starts transport\n";
			
			start_transport ();
		} 

	} else {

		/* slave has stopped */

		if (_transport_speed != 0.0f) {

			// cerr << "slave stops transport: " << slave_speed << " frame: " << slave_transport_frame 
			// << " tf = " << _transport_frame
			// << endl;
			
			if (Config->get_slave_source() == JACK) {
				last_stop_frame = _transport_frame;
			}

			stop_transport();
		}

		if (slave_transport_frame != _transport_frame) {
			// cerr << "slave stopped, move to " << slave_transport_frame << endl;
			force_locate (slave_transport_frame, false);
		}

		slave_state = Stopped;
	}

	if (slave_state == Running && !_slave->is_always_synced() && !Config->get_timecode_source_is_synced()) {


		if (_transport_speed != 0.0f) {
			
			/* 
			   note that average_dir is +1 or -1 
			*/
			
			const float adjust_seconds = 1.0f;
			float delta;

			//if (average_slave_delta == 0) {
				delta = this_delta;
				delta *= dir;
//			} else {
//				delta = average_slave_delta;
//				delta *= average_dir;
//			}

			float adjusted_speed = slave_speed +
				(delta / (adjust_seconds * _current_frame_rate));
			
			// cerr << "adjust using " << delta
			// << " towards " << adjusted_speed
			// << " ratio = " << adjusted_speed / slave_speed
			// << " current = " << _transport_speed
			// << " slave @ " << slave_speed
			// << endl;
			
			request_transport_speed (adjusted_speed);
			
#if 1
			if ((nframes_t) average_slave_delta > _slave->resolution()) {
				// cerr << "not locked\n";
				goto silent_motion;
			}
#endif
		}
	} 

	if (!starting && !non_realtime_work_pending()) {
		/* speed is set, we're locked, and good to go */
		return true;
	}

  silent_motion:

	if (slave_speed && _transport_speed) {

		/* something isn't right, but we should move with the master
		   for now.
		*/

		bool need_butler;
		
		prepare_diskstreams ();
		silent_process_routes (nframes, offset);
		commit_diskstreams (nframes, need_butler);

		if (need_butler) {
			summon_butler ();
		}
		
		int32_t frames_moved = (int32_t) floor (_transport_speed * nframes);
		
		if (frames_moved < 0) {
			decrement_transport_position (-frames_moved);
		} else {
			increment_transport_position (frames_moved);
		}
		
		nframes_t stop_limit;
		
		if (actively_recording()) {
			stop_limit = max_frames;
		} else {
			if (Config->get_stop_at_session_end()) {
				stop_limit = current_end_frame();
			} else {
				stop_limit = max_frames;
			}
		}

		maybe_stop (stop_limit);
	}

  noroll:
	/* don't move at all */
	no_roll (nframes, 0);
	return false;
}

void
Session::process_without_events (nframes_t nframes)
{
	bool session_needs_butler = false;
	nframes_t stop_limit;
	long frames_moved;
	nframes_t offset = 0;

	if (!process_can_proceed()) {
		_silent = true;
		return;
	}

	if (!_exporting && _slave) {
		if (!follow_slave (nframes, 0)) {
			return;
		}
	} 

	if (_transport_speed == 0) {
		no_roll (nframes, 0);
		return;
	}
		
	if (!_exporting) {
		send_midi_time_code_for_cycle (nframes);
	}

	if (actively_recording()) {
		stop_limit = max_frames;
	} else {
		if (Config->get_stop_at_session_end()) {
			stop_limit = current_end_frame();
		} else {
			stop_limit = max_frames;
		}
	}
		
	if (maybe_stop (stop_limit)) {
		no_roll (nframes, 0);
		return;
	} 

	if (maybe_sync_start (nframes, offset)) {
		return;
	}

	click (_transport_frame, nframes, offset);

	prepare_diskstreams ();
	
	frames_moved = (long) floor (_transport_speed * nframes);

	if (process_routes (nframes, offset)) {
		no_roll (nframes, offset);
		return;
	}

	commit_diskstreams (nframes, session_needs_butler);

	if (frames_moved < 0) {
		decrement_transport_position (-frames_moved);
	} else {
		increment_transport_position (frames_moved);
	}

	maybe_stop (stop_limit);
	check_declick_out ();

	if (session_needs_butler)
		summon_butler ();
}

/** Process callback used when the auditioner is active.
 * @param nframes number of frames to process.
 */
void
Session::process_audition (nframes_t nframes)
{
	Event* ev;
	boost::shared_ptr<RouteList> r = routes.reader ();

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		if (!(*i)->is_hidden()) {
			(*i)->silence (nframes, 0);
		}
	}

	/* run the auditioner, and if it says we need butler service, ask for it */
	
	if (auditioner->play_audition (nframes) > 0) {
		summon_butler ();
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
		Event *ev = immediate_events.front ();
		immediate_events.pop_front ();
		process_event (ev);
	}

	if (!auditioner->active()) {
		/* auditioner no longer active, so go back to the normal process callback */
		process_function = &Session::process_with_events;
	}
}

bool
Session::maybe_sync_start (nframes_t& nframes, nframes_t& offset)
{
	nframes_t sync_offset;

	if (!waiting_for_sync_offset) {
		return false;
	}

	if (_engine.get_sync_offset (sync_offset) && sync_offset < nframes) {

		/* generate silence up to the sync point, then
		   adjust nframes + offset to reflect whatever
		   is left to do.
		*/

		no_roll (sync_offset, 0);
		nframes -= sync_offset;
		offset += sync_offset;
		waiting_for_sync_offset = false;
		
		if (nframes == 0) {
			return true; // done, nothing left to process
		}
		
	} else {

		/* sync offset point is not within this process()
		   cycle, so just generate silence. and don't bother 
		   with any fancy stuff here, just the minimal silence.
		*/

		g_atomic_int_inc (&processing_prohibited);
		no_roll (nframes, 0);
		g_atomic_int_dec_and_test (&processing_prohibited);

		if (Config->get_locate_while_waiting_for_sync()) {
			if (micro_locate (nframes)) {
				/* XXX ERROR !!! XXX */
			}
		}

		return true; // done, nothing left to process
	}

	return false;
}

