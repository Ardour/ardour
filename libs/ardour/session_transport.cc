/*
    Copyright (C) 1999-2009 Paul Davis 

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
#include <unistd.h>

#include <sigc++/bind.h>
#include <sigc++/retype.h>

#include <pbd/undo.h>
#include <pbd/error.h>
#include <glibmm/thread.h>
#include <pbd/pthread_utils.h>
#include <pbd/memento_command.h>
#include <pbd/stacktrace.h>

#include <midi++/mmc.h>
#include <midi++/port.h>

#include <ardour/ardour.h>
#include <ardour/audioengine.h>
#include <ardour/session.h>
#include <ardour/audio_diskstream.h>
#include <ardour/auditioner.h>
#include <ardour/slave.h>
#include <ardour/location.h>

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace sigc;
using namespace PBD;

void
Session::request_input_change_handling ()
{
	if (!(_state_of_the_state & (InitialConnecting|Deletion))) {
		Event* ev = new Event (Event::InputConfigurationChange, Event::Add, Event::Immediate, 0, 0.0);
		queue_event (ev);
	} 
}

void
Session::request_slave_source (SlaveSource src)
{
	Event* ev = new Event (Event::SetSlaveSource, Event::Add, Event::Immediate, 0, 0.0);
	bool seamless;

	seamless = Config->get_seamless_loop ();

	if (src == JACK) {
		/* JACK cannot support seamless looping at present */
		Config->set_seamless_loop (false);
	} else {
		/* reset to whatever the value was before we last switched slaves */
		Config->set_seamless_loop (_was_seamless);
	}

	/* save value of seamless from before the switch */
	_was_seamless = seamless;

	ev->slave = src;
	queue_event (ev);
}

void
Session::request_transport_speed (float speed)
{
	Event* ev = new Event (Event::SetTransportSpeed, Event::Add, Event::Immediate, 0, speed);
	queue_event (ev);
}

void
Session::request_diskstream_speed (Diskstream& ds, float speed)
{
	Event* ev = new Event (Event::SetDiskstreamSpeed, Event::Add, Event::Immediate, 0, speed);
	ev->set_ptr (&ds);
	queue_event (ev);
}

void
Session::request_stop (bool abort, bool clear_state)
{
	Event* ev = new Event (Event::SetTransportSpeed, Event::Add, Event::Immediate, 0, 0.0, abort, clear_state);
	queue_event (ev);
}

void
Session::request_locate (nframes_t target_frame, bool with_roll)
{
	Event *ev = new Event (with_roll ? Event::LocateRoll : Event::Locate, Event::Add, Event::Immediate, target_frame, 0, false);
	queue_event (ev);
}

void
Session::force_locate (nframes_t target_frame, bool with_roll)
{
	Event *ev = new Event (with_roll ? Event::LocateRoll : Event::Locate, Event::Add, Event::Immediate, target_frame, 0, true);
	queue_event (ev);
}

void
Session::request_play_loop (bool yn, bool leave_rolling)
{
	Event* ev;	
	Location *location = _locations.auto_loop_location();

	if (location == 0 && yn) {
		error << _("Cannot loop - no loop range defined")
		      << endmsg;
		return;
	}

	ev = new Event (Event::SetLoop, Event::Add, Event::Immediate, 0, (leave_rolling ? 1.0 : 0.0), yn);
	queue_event (ev);

	if (!leave_rolling && !yn && Config->get_seamless_loop() && transport_rolling()) {
		// request an immediate locate to refresh the diskstreams
		// after disabling looping
		request_locate (_transport_frame-1, false);
	} 
}

void
Session::request_play_range (list<AudioRange>* range, bool leave_rolling)
{
	Event* ev = new Event (Event::SetPlayAudioRange, Event::Add, Event::Immediate, 0, (leave_rolling ? 1.0 : 0.0));
	if (range) {
		ev->audio_range = *range;
	} else {
		ev->audio_range.clear ();
	}
	queue_event (ev);
}

void
Session::realtime_stop (bool abort, bool clear_state)
{
	PostTransportWork todo = PostTransportWork (0);
	
	/* assume that when we start, we'll be moving forwards */

	if (_transport_speed < 0.0f) {
		todo = PostTransportWork (todo | PostTransportStop | PostTransportReverse);
	} else {
		todo = PostTransportWork (todo | PostTransportStop);
	}

	if (actively_recording()) {

		/* move the transport position back to where the
		   request for a stop was noticed. we rolled
		   past that point to pick up delayed input (and to declick).
		*/

                if (_worst_output_latency > current_block_size) {
                        /* we rolled past the stop point to pick up data that had
                           not yet arrived. move back to where the stop occured.
                        */
                        decrement_transport_position (current_block_size + (_worst_output_latency - current_block_size));
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
		post_transport_work = PostTransportWork (post_transport_work | todo);
	}
						  
	_clear_event_type (Event::StopOnce);
	_clear_event_type (Event::RangeStop);
	_clear_event_type (Event::RangeLocate);

	/* if we're going to clear loop state, then force disabling record BUT only if we're not doing latched rec-enable */

	disable_record (true, (!Config->get_latched_record_enable() && clear_state));

	reset_slave_state ();
		
	_transport_speed = 0;

	if (Config->get_use_video_sync()) {
		waiting_for_sync_offset = true;
	}

	transport_sub_state = ((Config->get_slave_source() == None && Config->get_auto_return()) ? AutoReturning : 0);
}

void
Session::butler_transport_work ()
{
  restart:
	bool finished;
	boost::shared_ptr<RouteList> r = routes.reader ();
	boost::shared_ptr<DiskstreamList> dsl = diskstreams.reader();

	int on_entry = g_atomic_int_get (&butler_should_do_transport_work);
	finished = true;

	if (post_transport_work & PostTransportCurveRealloc) {
		for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
			(*i)->curve_reallocate();
		}
	}

	if (post_transport_work & PostTransportInputChange) {
		for (DiskstreamList::iterator i = dsl->begin(); i != dsl->end(); ++i) {
			(*i)->non_realtime_input_change ();
		}
	}

	if (post_transport_work & PostTransportSpeed) {
		non_realtime_set_speed ();
	}

	if (post_transport_work & PostTransportReverse) {
		
		clear_clicks();
		cumulative_rf_motion = 0;
		reset_rf_scale (0);

		/* don't seek if locate will take care of that in non_realtime_stop() */

		if (!(post_transport_work & PostTransportLocate)) {
			
			for (DiskstreamList::iterator i = dsl->begin(); i != dsl->end(); ++i) {
				if (!(*i)->hidden()) {
					if ((*i)->speed() != 1.0f || (*i)->speed() != -1.0f) {
						(*i)->seek ((nframes_t) (_transport_frame * (double) (*i)->speed()));
					}
					else {
						(*i)->seek (_transport_frame);
					}
				}
				if (on_entry != g_atomic_int_get (&butler_should_do_transport_work)) {
					/* new request, stop seeking, and start again */
					g_atomic_int_dec_and_test (&butler_should_do_transport_work);
					goto restart;
				}
			}
		}
	}

	if (post_transport_work & (PostTransportStop|PostTransportLocate)) {
		non_realtime_stop (post_transport_work & PostTransportAbort, on_entry, finished);
		if (!finished) {
			g_atomic_int_dec_and_test (&butler_should_do_transport_work);
			goto restart;
		}
	}

	if (post_transport_work & PostTransportOverWrite) {
		non_realtime_overwrite (on_entry, finished);
		if (!finished) {
			g_atomic_int_dec_and_test (&butler_should_do_transport_work);
			goto restart;
		}
	}

	if (post_transport_work & PostTransportAudition) {
		non_realtime_set_audition ();
	}
	
	g_atomic_int_dec_and_test (&butler_should_do_transport_work);
}

void
Session::non_realtime_set_speed ()
{
	boost::shared_ptr<DiskstreamList> dsl = diskstreams.reader();

	for (DiskstreamList::iterator i = dsl->begin(); i != dsl->end(); ++i) {
		(*i)->non_realtime_set_speed ();
	}
}

void
Session::non_realtime_overwrite (int on_entry, bool& finished)
{
	boost::shared_ptr<DiskstreamList> dsl = diskstreams.reader();

	for (DiskstreamList::iterator i = dsl->begin(); i != dsl->end(); ++i) {
		if ((*i)->pending_overwrite) {
			(*i)->overwrite_existing_buffers ();
		}
		if (on_entry != g_atomic_int_get (&butler_should_do_transport_work)) {
			finished = false;
			return;
		}
	}
}

void
Session::non_realtime_stop (bool abort, int on_entry, bool& finished)
{
	struct tm* now;
	time_t     xnow;
	bool       did_record;
	bool       saved;

	did_record = false;
	saved = false;

	boost::shared_ptr<DiskstreamList> dsl = diskstreams.reader();
	
	for (DiskstreamList::iterator i = dsl->begin(); i != dsl->end(); ++i) {
		if ((*i)->get_captured_frames () != 0) {
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

	clear_clicks();
	cumulative_rf_motion = 0;
	reset_rf_scale (0);

	if (did_record) {
		begin_reversible_command ("capture");
		
		Location* loc = _locations.end_location();
		bool change_end = false;
		
		if (_transport_frame < loc->end()) {

			/* stopped recording before current end */

			if (_end_location_is_free) {

				/* first capture for this session, move end back to where we are */

				change_end = true;
			} 

		} else if (_transport_frame > loc->end()) {
			
			/* stopped recording after the current end, extend it */

			change_end = true;
		}
		
		if (change_end) {
                        XMLNode &before = loc->get_state();
                        loc->set_end(_transport_frame);
                        XMLNode &after = loc->get_state();
                        add_command (new MementoCommand<Location>(*loc, &before, &after));
		}

		_end_location_is_free = false;
		_have_captured = true;
	}

	for (DiskstreamList::iterator i = dsl->begin(); i != dsl->end(); ++i) {
		(*i)->transport_stopped (*now, xnow, abort);
	}
	
	boost::shared_ptr<RouteList> r = routes.reader ();

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		if (!(*i)->hidden()) {
			(*i)->set_pending_declick (0);
		}
	}
	
	if (did_record) {
		commit_reversible_command ();
	}	
	
	if (_engine.running()) {
		update_latency_compensation (true, abort);
	}

	if ((Config->get_slave_source() == None && Config->get_auto_return()) || 
	    (post_transport_work & PostTransportLocate) || 
	    (_requested_return_frame >= 0) ||
	    synced_to_jack()) {
		
		if (pending_locate_flush) {
			flush_all_redirects ();
		}
		
		if (((Config->get_slave_source() == None && Config->get_auto_return()) || 
		     synced_to_jack() ||
		     _requested_return_frame >= 0) &&
		    !(post_transport_work & PostTransportLocate)) {

			/* no explicit locate queued */

			bool do_locate = false;

			if (_requested_return_frame >= 0) {

				/* explicit return request pre-queued in event list. overrides everything else */
				
				_transport_frame = _requested_return_frame;
				do_locate = true;

			} else {
				if (Config->get_auto_return()) {

					if (play_loop) {

						/* don't try to handle loop play when synced to JACK */

						if (!synced_to_jack()) {

							Location *location = _locations.auto_loop_location();
							
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

	/* do this before seeking, because otherwise the Diskstreams will do the wrong thing in seamless loop mode.
	*/

	if (post_transport_work & PostTransportClearSubstate) {
		_play_range = false;
		unset_play_loop ();
	}

	/* this for() block can be put inside the previous if() and has the effect of ... ??? what */

	for (DiskstreamList::iterator i = dsl->begin(); i != dsl->end(); ++i) {
		if (!(*i)->hidden()) {
			if ((*i)->speed() != 1.0f || (*i)->speed() != -1.0f) {
				(*i)->seek ((nframes_t) (_transport_frame * (double) (*i)->speed()));
			}
			else {
				(*i)->seek (_transport_frame);
			}
		}
		if (on_entry != g_atomic_int_get (&butler_should_do_transport_work)) {
			finished = false;
			/* we will be back */
			return;
		}
	}

        have_looped = false; 

        send_full_time_code ();
	deliver_mmc (MIDI::MachineControl::cmdStop, 0);
	deliver_mmc (MIDI::MachineControl::cmdLocate, _transport_frame);

	if ((post_transport_work & PostTransportLocate) && get_record_enabled()) {
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

	if (post_transport_work & PostTransportDuration) {
		DurationChanged (); /* EMIT SIGNAL */
	}

        nframes_t tf = _transport_frame;

        PositionChanged (tf); /* EMIT SIGNAL */
	TransportStateChange (); /* EMIT SIGNAL */

	/* and start it up again if relevant */

	if ((post_transport_work & PostTransportLocate) && Config->get_slave_source() == None && pending_locate_roll) {
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
			stop_transport (pending_abort, pending_clear_substate);
			transport_sub_state &= ~(PendingDeclickOut|PendingLocate);
		}
	}
}

void
Session::unset_play_loop ()
{
	play_loop = false;
	clear_events (Event::AutoLoop);
	
	// set all diskstreams to NOT use internal looping
	boost::shared_ptr<DiskstreamList> dsl = diskstreams.reader();
	for (DiskstreamList::iterator i = dsl->begin(); i != dsl->end(); ++i) {
		if (!(*i)->hidden()) {
			(*i)->set_loop (0);
		}
	}
}

void
Session::set_play_loop (bool yn)
{
	/* Called from event-handling context */

	Location *loc;

	if (yn == play_loop || (actively_recording() && yn) || (loc = _locations.auto_loop_location()) == 0) {
		/* nothing to do, or can't change loop status while recording */
		return;
	}
	
	set_dirty();
	
	if (yn && Config->get_seamless_loop() && synced_to_jack()) {
		warning << string_compose (_("Seamless looping cannot be supported while %1 is using JACK transport.\n"
					     "Recommend changing the configured options"), PROGRAM_NAME)
			<< endmsg;
		return;
	}
	
	if (yn) {

		play_loop = true;

		if (loc) {

			unset_play_range ();

			if (Config->get_seamless_loop()) {
				// set all diskstreams to use internal looping
				boost::shared_ptr<DiskstreamList> dsl = diskstreams.reader();
				for (DiskstreamList::iterator i = dsl->begin(); i != dsl->end(); ++i) {
					if (!(*i)->hidden()) {
						(*i)->set_loop (loc);
					}
				}
			}
			else {
				// set all diskstreams to NOT use internal looping
				boost::shared_ptr<DiskstreamList> dsl = diskstreams.reader();
				for (DiskstreamList::iterator i = dsl->begin(); i != dsl->end(); ++i) {
					if (!(*i)->hidden()) {
						(*i)->set_loop (0);
					}
				}
			}
			
			/* put the loop event into the event list */
			
			Event* event = new Event (Event::AutoLoop, Event::Replace, loc->end(), loc->start(), 0.0f);
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
Session::flush_all_redirects ()
{
	boost::shared_ptr<RouteList> r = routes.reader ();

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		(*i)->flush_redirects ();
	}
}

void
Session::start_locate (nframes_t target_frame, bool with_roll, bool with_flush, bool with_loop, bool force)
{
	if (synced_to_jack()) {

		float sp;
		nframes_t pos;

		_slave->speed_and_position (sp, pos);

		if (target_frame != pos) {

			/* tell JACK to change transport position, and we will
			   follow along later in ::follow_slave()
			*/

			_engine.transport_locate (target_frame);
		}

		if (sp != 1.0f && with_roll) {
			_engine.transport_start ();
		}

	} else {

		locate (target_frame, with_roll, with_flush, with_loop, force);
	}
}

int
Session::micro_locate (nframes_t distance)
{
	boost::shared_ptr<DiskstreamList> dsl = diskstreams.reader();
	
	for (DiskstreamList::iterator i = dsl->begin(); i != dsl->end(); ++i) {
		if (!(*i)->can_internal_playback_seek (distance)) {
			return -1;
		}
	}

	for (DiskstreamList::iterator i = dsl->begin(); i != dsl->end(); ++i) {
		(*i)->internal_playback_seek (distance);
	}
	
	_transport_frame += distance;
	return 0;
}

void
Session::locate (nframes_t target_frame, bool with_roll, bool with_flush, bool with_loop, bool force)
{
	if (actively_recording() && !with_loop) {
		return;
	}

	if (!force && _transport_frame == target_frame && !loop_changing && !with_loop) {
		if (with_roll) {
			set_transport_speed (1.0, false);
		}
		loop_changing = false;
		return;
	}

	_transport_frame = target_frame;

	if (_transport_speed && (!with_loop || loop_changing)) {
		/* schedule a declick. we'll be called again when its done */

		if (!(transport_sub_state & PendingDeclickOut)) {
			transport_sub_state |= (PendingDeclickOut|PendingLocate);
			pending_locate_frame = target_frame;
			pending_locate_roll = with_roll;
			pending_locate_flush = with_flush;
			return;
		} 
	}

	/* stop if we are rolling and we're not doing autoplay and we don't plan to roll when done and we not looping while synced to
	   jack
	*/

	if (transport_rolling() && (!auto_play_legal || !Config->get_auto_play()) && !with_roll && !(synced_to_jack() && play_loop)) {
		realtime_stop (false, true); // XXX paul - check if the 2nd arg is really correct
	} 

	if (force || !with_loop || loop_changing) {

		post_transport_work = PostTransportWork (post_transport_work | PostTransportLocate);
		
		if (with_roll) {
			post_transport_work = PostTransportWork (post_transport_work | PostTransportRoll);
		}

		schedule_butler_transport_work ();

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

			boost::shared_ptr<DiskstreamList> dsl = diskstreams.reader();

			for (DiskstreamList::iterator i = dsl->begin(); i != dsl->end(); ++i) {
				if ((*i)->record_enabled ()) {
					//cerr << "switching from input" << __FILE__ << __LINE__ << endl << endl;
					(*i)->monitor_input (!Config->get_auto_input());
				}
			}
		}
	} else {
		/* otherwise we're going to stop, so do the opposite */
		if (Config->get_monitoring_model() == HardwareMonitoring) {
			boost::shared_ptr<DiskstreamList> dsl = diskstreams.reader();

			for (DiskstreamList::iterator i = dsl->begin(); i != dsl->end(); ++i) {
				if ((*i)->record_enabled ()) {
					//cerr << "switching to input" << __FILE__ << __LINE__ << endl << endl;
					(*i)->monitor_input (true);
				}
			}
		}
	}

	/* cancel looped playback if transport pos outside of loop range */
	if (play_loop) {
		Location* al = _locations.auto_loop_location();
		
		if (al && (_transport_frame < al->start() || _transport_frame > al->end())) {
			// cancel looping directly, this is called from event handling context
			set_play_loop (false);
		}
		else if (al && _transport_frame == al->start()) {
			if (with_loop) {
				// this is only necessary for seamless looping

				boost::shared_ptr<DiskstreamList> dsl = diskstreams.reader();
				
				for (DiskstreamList::iterator i = dsl->begin(); i != dsl->end(); ++i) {
					if ((*i)->record_enabled ()) {
						// tell it we've looped, so it can deal with the record state
						(*i)->transport_looped(_transport_frame);
					}
				}
			}
			have_looped = true;
			TransportLooped(); // EMIT SIGNAL
		}
	}
	
	loop_changing = false;

	_send_smpte_update = true;
}

void
Session::set_transport_speed (float speed, bool abort, bool clear_state)
{
	if (_transport_speed == speed) {
		return;
	}

	if (speed > 0) {
		speed = min (8.0f, speed);
	} else if (speed < 0) {
		speed = max (-8.0f, speed);
	}

	if (transport_rolling() && speed == 0.0) {

		if (Config->get_monitoring_model() == HardwareMonitoring)
		{
			boost::shared_ptr<DiskstreamList> dsl = diskstreams.reader();

			for (DiskstreamList::iterator i = dsl->begin(); i != dsl->end(); ++i) {
				if ((*i)->record_enabled ()) {
					//cerr << "switching to input" << __FILE__ << __LINE__ << endl << endl;
					(*i)->monitor_input (true);	
				}
			}
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
			stop_transport (abort, clear_state);
		}
		
	} else if (transport_stopped() && speed == 1.0) {

		if (!get_record_enabled() && Config->get_stop_at_session_end() && _transport_frame >= current_end_frame()) {
			return;
		}

		if (Config->get_monitoring_model() == HardwareMonitoring) {

			boost::shared_ptr<DiskstreamList> dsl = diskstreams.reader();

			for (DiskstreamList::iterator i = dsl->begin(); i != dsl->end(); ++i) {
				if (Config->get_auto_input() && (*i)->record_enabled ()) {
					//cerr << "switching from input" << __FILE__ << __LINE__ << endl << endl;
					(*i)->monitor_input (false);	
				}
			}
		}

		if (synced_to_jack()) {
			_engine.transport_start ();
		} else {
			start_transport ();
		}

	} else {

		if (!get_record_enabled() && Config->get_stop_at_session_end() && _transport_frame >= current_end_frame()) {
			return;
		}

		if ((synced_to_jack()) && speed != 0.0 && speed != 1.0) {
			warning << string_compose (_("Global varispeed cannot be supported while %1 is connected to JACK transport control"),
						   PROGRAM_NAME)
				<< endmsg;
			return;
		}

		if (actively_recording()) {
			return;
		}

		if (speed > 0.0f && _transport_frame == current_end_frame()) {
			return;
		}

		if (speed < 0.0f && _transport_frame == 0) {
			return;
		}
		
		clear_clicks ();

		/* if we are reversing relative to the current speed, or relative to the speed
		   before the last stop, then we have to do extra work.
		*/

		PostTransportWork todo = PostTransportWork (0);

		if ((_transport_speed && speed * _transport_speed < 0.0f) || (_last_transport_speed * speed < 0.0f) || (_last_transport_speed == 0.0f && speed < 0.0f)) {
			todo = PostTransportWork (todo | PostTransportReverse);
			last_stop_frame = _transport_frame;
 			_last_roll_or_reversal_location = _transport_frame;
		}
		
		_last_transport_speed = _transport_speed;
		_transport_speed = speed;
		
		boost::shared_ptr<DiskstreamList> dsl = diskstreams.reader();
		for (DiskstreamList::iterator i = dsl->begin(); i != dsl->end(); ++i) {
			if ((*i)->realtime_set_speed ((*i)->speed(), true)) {
				todo = PostTransportWork (todo | PostTransportSpeed);
				break;
			}
		}
		
		if (todo) {
			post_transport_work = PostTransportWork (post_transport_work | todo);
			schedule_butler_transport_work ();
		}
	}
}

void
Session::stop_transport (bool abort, bool clear_state)
{
	if (_transport_speed == 0.0f) {
		return;
	}

	if (actively_recording() && !(transport_sub_state & StopPendingCapture) && (_worst_output_latency > current_block_size)) {

                boost::shared_ptr<DiskstreamList> dsl = diskstreams.reader();
                
                for (DiskstreamList::iterator i = dsl->begin(); i != dsl->end(); ++i) {
                        (*i)->prepare_to_stop (_transport_frame);
                }

		/* we need to capture the audio that has still not yet been received by the system
		   at the time the stop is requested, so we have to roll past that time.

		   we want to declick before stopping, so schedule the autostop for one
		   block before the actual end. we'll declick in the subsequent block,
		   and then we'll really be stopped.
		*/
		
		Event *ev = new Event (Event::StopOnce, Event::Replace, 
				       _transport_frame + _worst_output_latency - current_block_size,
				       0, 0, abort);
		
		merge_event (ev);
		transport_sub_state |= StopPendingCapture;
		pending_abort = abort;
		pending_clear_substate = clear_state;
		return;
	} 

	if ((transport_sub_state & PendingDeclickOut) == 0) {

                if (!(transport_sub_state & StopPendingCapture)) {
                        boost::shared_ptr<DiskstreamList> dsl = diskstreams.reader();
                        
                        for (DiskstreamList::iterator i = dsl->begin(); i != dsl->end(); ++i) {
                                (*i)->prepare_to_stop (_transport_frame);
                        }
                }

		transport_sub_state |= PendingDeclickOut;
		/* we'll be called again after the declick */
		pending_abort = abort;
		pending_clear_substate = clear_state;
		return;
	}

	realtime_stop (abort, clear_state);
	schedule_butler_transport_work ();
}

void
Session::start_transport ()
{
	_last_roll_location = _transport_frame;
	_last_roll_or_reversal_location = _transport_frame;
	
	have_looped = false;

	/* if record status is Enabled, move it to Recording. if its
	   already Recording, move it to Disabled. 
	*/

	switch (record_status()) {
	case Enabled:
		if (!Config->get_punch_in()) {
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
	
	boost::shared_ptr<DiskstreamList> dsl = diskstreams.reader();
	for (DiskstreamList::iterator i = dsl->begin(); i != dsl->end(); ++i) {
		(*i)->realtime_set_speed ((*i)->speed(), true);
	}

	boost::shared_ptr<RouteList> r = routes.reader ();

        /* force an automation snapshot as we start up */

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
                (*i)->automation_snapshot (transport_frame(), true);
        }

	send_mmc_in_another_thread (MIDI::MachineControl::cmdDeferredPlay, 0);
	
	TransportStateChange (); /* EMIT SIGNAL */
}

void
Session::post_transport ()
{
	if (post_transport_work & PostTransportAudition) {
		if (auditioner && auditioner->active()) {
			process_function = &Session::process_audition;
		} else {
			process_function = &Session::process_with_events;
		}
	}

	if (post_transport_work & PostTransportStop) {

		transport_sub_state = 0;
	}

	if (post_transport_work & PostTransportLocate) {

		if (((Config->get_slave_source() == None && (auto_play_legal && Config->get_auto_play())) && !_exporting) || (post_transport_work & PostTransportRoll)) {
			start_transport ();
			
		} else {
			transport_sub_state = 0;
		}
	}

	set_next_event ();

	post_transport_work = PostTransportWork (0);
}

void
Session::reset_rf_scale (nframes_t motion)
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
Session::set_slave_source (SlaveSource src, bool stop_the_transport)
{
	bool reverse = false;
	bool non_rt_required = false;

	if (_transport_speed) {
		error << _("please stop the transport before adjusting slave settings") << endmsg;
		return;
	}

// 	if (src == JACK && Config->get_jack_time_master()) {
// 		return;
// 	}
	
	if (_slave) {
		delete _slave;
		_slave = 0;
	}

	if (_transport_speed < 0.0) {
		reverse = true;
	}

	switch (src) {
	case None:
		if (stop_the_transport) {
			stop_transport ();
		}
		break;
		
	case MTC:
		if (_mtc_port) {
			try {
				_slave = new MTC_Slave (*this, *_mtc_port);
			}

			catch (failed_constructor& err) {
				return;
			}

		} else {
			error << _("No MTC port defined: MTC slaving is impossible.") << endmsg;
			return;
		}
		_desired_transport_speed = _transport_speed;
		break;
		
	case JACK:
		_slave = new JACK_Slave (_engine.jack());
		_desired_transport_speed = _transport_speed;
		break;
	};

	Config->set_slave_source (src);
	
	boost::shared_ptr<DiskstreamList> dsl = diskstreams.reader();
	for (DiskstreamList::iterator i = dsl->begin(); i != dsl->end(); ++i) {
		if (!(*i)->hidden()) {
			if ((*i)->realtime_set_speed ((*i)->speed(), true)) {
				non_rt_required = true;
			}
			(*i)->set_slaved (_slave);
		}
	}

	if (reverse) {
		reverse_diskstream_buffers ();
	}

	if (non_rt_required) {
		post_transport_work = PostTransportWork (post_transport_work | PostTransportSpeed);
		schedule_butler_transport_work ();
	}

	set_dirty();
}

void
Session::reverse_diskstream_buffers ()
{
	post_transport_work = PostTransportWork (post_transport_work | PostTransportReverse);
	schedule_butler_transport_work ();
}

void
Session::set_diskstream_speed (Diskstream* stream, float speed)
{
	if (stream->realtime_set_speed (speed, false)) {
		post_transport_work = PostTransportWork (post_transport_work | PostTransportSpeed);
		schedule_butler_transport_work ();
		set_dirty ();
	}
}

void
Session::unset_play_range ()
{
	_play_range = false;
	_clear_event_type (Event::RangeStop);
	_clear_event_type (Event::RangeLocate);
}

void
Session::set_play_range (list<AudioRange>& range, bool leave_rolling)
{
	Event* ev;

	/* Called from event-processing context */

	unset_play_range ();
	
	if (range.empty()) {
		/* _play_range set to false in unset_play_range()
		 */
		if (!leave_rolling) {
			/* stop transport */
			Event* ev = new Event (Event::SetTransportSpeed, Event::Add, Event::Immediate, 0, 0.0f, false);
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
			
			nframes_t requested_frame = (*i).end;
			
			if (requested_frame > current_block_size) {
				requested_frame -= current_block_size;
			} else {
				requested_frame = 0;
			}
			
			if (next == range.end()) {
				ev = new Event (Event::RangeStop, Event::Add, requested_frame, 0, 0.0f);
			} else {
				ev = new Event (Event::RangeLocate, Event::Add, requested_frame, (*next).start, 0.0f);
			}
			
			merge_event (ev);
			
			i = next;
		}
		
	} else if (sz == 1) {

		ev = new Event (Event::RangeStop, Event::Add, range.front().end, 0, 0.0f);
		merge_event (ev);
		
	} 

	/* save range so we can do auto-return etc. */

	current_audio_range = range;

	/* now start rolling at the right place */

	ev = new Event (Event::LocateRoll, Event::Add, Event::Immediate, range.front().start, 0.0f, false);
	merge_event (ev);
	
	TransportStateChange ();
}

void
Session::request_bounded_roll (nframes_t start, nframes_t end)
{
	AudioRange ar (start, end, 0);
	list<AudioRange> lar;

	lar.push_back (ar);
	request_play_range (&lar, true);
}

void
Session::request_roll_at_and_return (nframes_t start, nframes_t return_to)
{
 	Event *ev = new Event (Event::LocateRollLocate, Event::Add, Event::Immediate, return_to, 1.0);
	ev->target2_frame = start;
	queue_event (ev);
}

void
Session::engine_halted (const char* /* reason */)
{
	bool ignored;

	/* there will be no more calls to process(), so
	   we'd better clean up for ourselves, right now.

	   but first, make sure the butler is out of 
	   the picture.
	*/

	g_atomic_int_set (&butler_should_do_transport_work, 0);
	post_transport_work = PostTransportWork (0);
	stop_butler ();

	realtime_stop (false, true);
	non_realtime_stop (false, 0, ignored);
	transport_sub_state = 0;

	if (synced_to_jack()) {
		/* transport is already stopped, hence the second argument */
		set_slave_source (None, false);
	}

	TransportStateChange (); /* EMIT SIGNAL */
}


void
Session::xrun_recovery ()
{
	Xrun (transport_frame()); //EMIT SIGNAL

	if (Config->get_stop_recording_on_xrun() && actively_recording()) {

		/* it didn't actually halt, but we need
		   to handle things in the same way.
		*/

		engine_halted ("");
	} 
}

void
Session::update_latency_compensation (bool with_stop, bool abort)
{
	bool update_jack = false;

	if (_state_of_the_state & Deletion) {
		return;
	}

	_worst_track_latency = 0;

#undef DEBUG_LATENCY
#ifdef DEBUG_LATENCY
	cerr << "\n---------------------------------\nUPDATE LATENCY\n";
#endif

	boost::shared_ptr<RouteList> r = routes.reader ();

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		if (with_stop) {
			(*i)->handle_transport_stopped (abort, (post_transport_work & PostTransportLocate), 
							(!(post_transport_work & PostTransportLocate) || pending_locate_flush));
		}

		nframes_t old_latency = (*i)->signal_latency ();
		nframes_t track_latency = (*i)->update_total_latency ();

		if (old_latency != track_latency) {
			update_jack = true;
		}
		
		if (!(*i)->hidden() && ((*i)->active())) {
			_worst_track_latency = max (_worst_track_latency, track_latency);
		}
	}

#ifdef DEBUG_LATENCY
	cerr << "\tworst was " << _worst_track_latency << endl;
#endif

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		(*i)->set_latency_delay (_worst_track_latency);
	}

	/* tell JACK to play catch up */

	if (update_jack) {
		_engine.update_total_latencies ();
	}

	set_worst_io_latencies ();

	/* reflect any changes in latencies into capture offsets
	*/
	
	boost::shared_ptr<DiskstreamList> dsl = diskstreams.reader();

	for (DiskstreamList::iterator i = dsl->begin(); i != dsl->end(); ++i) {
		(*i)->set_capture_offset ();
	}
}

void
Session::route_redirects_changed (void* ignored)
{
	update_latency_compensation (false, false);
	resort_routes ();
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
Session::maybe_stop (nframes_t limit)
{
	if ((_transport_speed > 0.0f && _transport_frame >= limit) || (_transport_speed < 0.0f && _transport_frame == 0)) {
		if (synced_to_jack () && Config->get_jack_time_master ()) {
			_engine.transport_stop ();
		} else if (!synced_to_jack ()) {
			stop_transport ();
		}
		return true;
	}
	return false;
}
