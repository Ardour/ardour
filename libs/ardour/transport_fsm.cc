/*
 * Copyright (C) 2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2019 Paul Davis <paul@linuxaudiosystems.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <sstream>

#include <boost/none.hpp>

#include "pbd/error.h"
#include "pbd/pthread_utils.h"
#include "pbd/stacktrace.h"

#include "ardour/debug.h"
#include "ardour/disk_reader.h"
#include "ardour/session.h"
#include "ardour/transport_fsm.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;

Pool* TransportFSM::Event::pool = 0;

void
TransportFSM::Event::init_pool ()
{
	pool = new Pool (X_("Events"), sizeof (Event), 128);
}

void*
TransportFSM::Event::operator new (size_t)
{
	return pool->alloc();
 }

void
TransportFSM::Event::operator delete (void *ptr, size_t /*size*/)
{
	return pool->release (ptr);
}

TransportFSM::TransportFSM (TransportAPI& tapi)
	: _last_locate (Locate, 0, MustRoll, false, false, false) /* all but first argument don't matter */
	, last_speed_request (SetSpeed, 0, false, false, false) /* ditto */
	, api (&tapi)
	, processing (0)
	, most_recently_requested_speed (std::numeric_limits<double>::max())
{
	init ();
}

void
TransportFSM::init ()
{
	_motion_state = Stopped;
	_butler_state = NotWaitingForButler;
	_direction_state = Forwards;
	_last_locate.target = max_samplepos;
}

void
TransportFSM::process_events ()
{
	processing++;

	while (!queued_events.empty()) {

		MotionState oms = _motion_state;
		ButlerState obs = _butler_state;

		Event* ev = &queued_events.front();
		bool deferred;

		/* must remove from the queued_events list now, because
		 * process_event() may defer the event. This will lead to
		 * insertion into the deferred_events list, and its not possible
		 * with intrusive lists to be present in two lists at once
		 * (without additional hooks).
		 */

		queued_events.pop_front ();

		if (process_event (*ev, false, deferred)) { /* event processed successfully */

			if (oms != _motion_state || obs != _butler_state) {

				/* state changed, so now check deferred events
				 * to see if they can be processed now
				 */

				if (!deferred_events.empty() ){
					DEBUG_TRACE (DEBUG::TFSMEvents, string_compose ("processing %1 deferred events\n", deferred_events.size()));

					for (EventList::iterator e = deferred_events.begin(); e != deferred_events.end(); ) {
						Event* deferred_ev = &(*e);
						bool deferred2;
						if (process_event (*e, true, deferred2)) { /* event processed, remove from deferred */
							e = deferred_events.erase (e);
							delete deferred_ev;
						} else {
							++e;
						}
					}
				}
			}
		}

		if (!deferred) {
			delete ev;
		}
	}

	processing--;
}

/* This is the transition table from the original boost::msm
 * implementation of this FSM. It is more easily readable and
 * consultable. Please keep it updated as the FSM changes.
 *
 * Here's a hint about how to read each line of this table:
 *
 * "if the current state is Start and event Event arrives, new state is Next and we execute Action()"
 *
 * with a variant:
 *
 * "if the current state is Start and event Event arrives, new state is Next and we execute Action() ***IF*** Guard() returns true"
 *
 * This new implementation, however, does not use metaprogramming to achieve all this,
 * but just uses a large-ish switch() block.
 *
 */

/*
        Start                Event            Next               Action                Guard
      +----------------------+----------------+------------------+---------------------+---------------------------------+
a_row < Stopped,             start_transport, Rolling,           &T::start_playback                                      >,
_row  < Stopped,             stop_transport,  Stopped                                                                    >,
a_row < Stopped,             locate,          WaitingForLocate,  &T::start_locate_while_stopped                          >,
g_row < WaitingForLocate,    locate_done,     Stopped,                                  &T::should_not_roll_after_locate >,
_row  < Rolling,             butler_done,     Rolling                                                                    >,
_row  < Rolling,             start_transport, Rolling                                                                    >,
a_row < Rolling,             stop_transport,  DeclickToStop,     &T::stop_playback                                       >,
a_row < DeclickToStop,       declick_done,    Stopped,                                                                   >,
a_row < DeclickToStop,       stop_transport,  DeclickToStop                                                              >,
a_row < Rolling,             locate,          DeclickToLocate,   &T::start_declick_for_locate                            >,
a_row < DeclickToLocate,     declick_done,    WaitingForLocate,  &T::start_locate_after_declick                          >,
row   < WaitingForLocate,    locate_done,     Rolling,           &T::roll_after_locate, &T::should_roll_after_locate     >,
a_row < NotWaitingForButler, butler_required, WaitingForButler,  &T::schedule_butler_for_transport_work                  >,
a_row < WaitingForButler,    butler_required, WaitingForButler,  &T::schedule_butler_for_transport_work                  >,
_row  < WaitingForButler,    butler_done,     NotWaitingForButler                                                        >,
a_row < WaitingForLocate,    locate,          WaitingForLocate,  &T::interrupt_locate                                    >,
a_row < DeclickToLocate,     locate,          DeclickToLocate,   &T::interrupt_locate                                    >,

// Deferrals

#define defer(start_state,ev) boost::msm::front::Row<start_state, ev, start_state, boost::msm::front::Defer, boost::msm::front::none >

defer (DeclickToLocate, start_transport),
defer (DeclickToLocate, stop_transport),
defer (DeclickToStop, start_transport),
defer (WaitingForLocate, start_transport),
defer (WaitingForLocate, stop_transport)

#undef defer
*/

std::string
TransportFSM::current_state () const
{
	std::stringstream s;
	s << enum_2_string (_motion_state) << '/' << enum_2_string (_butler_state) << '/' << enum_2_string (_direction_state);
	return s.str();
}

void
TransportFSM::bad_transition (Event const & ev)
{
	error << "bad transition, current state = " << current_state() << " event = " << enum_2_string (ev.type) << endmsg;
	std::cerr << "bad transition, current state = " << current_state() << " event = " << enum_2_string (ev.type) << std::endl;
	PBD::stacktrace (std::cerr, 30);
}

bool
TransportFSM::process_event (Event& ev, bool already_deferred, bool& deferred)
{
	DEBUG_TRACE (DEBUG::TFSMEvents, string_compose ("process %1\n", enum_2_string (ev.type)));

	deferred = false;

	switch (ev.type) {

	case SetSpeed:
		switch (_direction_state) {
		case Reversing:
			if (!already_deferred) {
				defer (ev);
				deferred = true;
			}
			break;
		default:
			switch (_motion_state) {
			case Stopped:
			case Rolling:
				set_speed (ev);
			break;
			default:
				if (!already_deferred) {
					defer (ev);
					deferred = true;
				}
			}
			break;
		}
		break;

	case StartTransport:
		switch (_motion_state) {
		case Stopped:
			transition (Rolling);
			start_playback ();
			break;
		case Rolling:
			break;
		case DeclickToLocate:
		case WaitingForLocate:
			if (!already_deferred) {
				defer (ev);
				deferred = true;
			}
			break;
		case DeclickToStop:
			if (!already_deferred) {
				defer (ev);
				deferred = true;
			}
			break;
		default:
			bad_transition (ev); return false;
			break;
		}
		break;

	case StopTransport:
		switch (_motion_state) {
		case Rolling:
			transition (DeclickToStop);
			stop_playback (ev);
			break;
		case Stopped:
			break;
		case DeclickToLocate:
		case WaitingForLocate:
			if (!already_deferred) {
				defer (ev);
				deferred = true;
			}
			break;
		case DeclickToStop:
			/* already doing it */
			break;
		default:
			bad_transition (ev); return false;
			break;
		}
		break;

	case Locate:
		DEBUG_TRACE (DEBUG::TFSMEvents, string_compose ("locate, ltd = %1 flush = %2 target = %3 loop %4 force %5\n",
		                                                enum_2_string (ev.ltd),
		                                                ev.with_flush,
		                                                ev.target,
		                                                ev.for_loop_end,
		                                                ev.force));
		switch (_motion_state) {
		case Stopped:
			transition (WaitingForLocate);
			start_locate_while_stopped (ev);
			break;
		case Rolling:
			if (ev.for_loop_end) {

				/* we will finish the locate synchronously, so
				 * that after returning from
				 * ::locate_for_loop() we will already have
				 * received (and re-entrantly handled)
				 * LocateDone and returned back to Rolling.
				 *
				 * This happens because we only need to do a
				 * realtime locate and continue rolling. No
				 * disk I/O is required - the loop is
				 * automically present in buffers already.
				 *
				 * Note that ev.ltd is ignored and
				 * assumed to be true because we're looping.
				 */
				transition (WaitingForLocate);
				locate_for_loop (ev);

			} else if (DiskReader::no_disk_output()) {

				/* separate clause to allow a comment that is
				   case specific. Logically this condition
				   could be bundled into first if() above.
				*/

				/* this can occur when locating to catch up
				   with a transport master. no_disk_output was
				   set to prevent playback until we're synced
				   and locked with the master. If we locate
				   during this process, we're not producing any
				   audio from disk, and so there is no need to
				   declick.
				*/
				transition (WaitingForLocate);
				locate_for_loop (ev);
			} else {

				if (api->need_declick_before_locate()) {
					transition (DeclickToLocate);
					start_declick_for_locate (ev);
				} else {
					transition (WaitingForLocate);
					locate_for_loop (ev);
				}
			}
			break;
		case WaitingForLocate:
		case DeclickToLocate:
			interrupt_locate (ev);
			break;
		default:
			bad_transition (ev); return false;
		}
		break;

	case LocateDone:
		switch (_motion_state) {
		case WaitingForLocate:

			if (reversing()) {

				if (most_recently_requested_speed >= 0.) {
					transition (Forwards);
				} else {
					transition (Backwards);
				}

				if (should_roll_after_locate()) {
					transition (Stopped);
				} else {
					transition (Rolling);
					roll_after_locate ();
				}

			} else {
				if (should_not_roll_after_locate()) {
					transition (Stopped);
					/* already stopped, nothing to do */
				} else {
					transition (Rolling);
					roll_after_locate ();
				}
				break;
			}
			break;
		default:
			bad_transition (ev); return false;
		}
		break;

	case DeclickDone:
		switch (_motion_state) {
		case DeclickToLocate:
			transition (WaitingForLocate);
			start_locate_after_declick ();
			break;
		case DeclickToStop:
			transition (Stopped);
			/* transport already stopped */
			break;
		default:
			bad_transition (ev); return false;
		}
		break;

	case ButlerRequired:
		switch (_butler_state) {
		case NotWaitingForButler:
			transition (WaitingForButler);
			schedule_butler_for_transport_work ();
			break;
		case WaitingForButler:
			schedule_butler_for_transport_work ();
			break;
		default:
			bad_transition (ev); return false;
		}
		break;

	case ButlerDone:
		switch (_butler_state) {
		case WaitingForButler:
			transition (NotWaitingForButler);
			break;
		default:
			bad_transition (ev); return false;
		}
		break;
	}

	return true;
}

/* transition actions */

void
TransportFSM::start_playback ()
{
	DEBUG_TRACE (DEBUG::TFSMEvents, "start_playback\n");

	_last_locate.target = max_samplepos;
	current_roll_after_locate_status = boost::none;

	api->start_transport();
}

void
TransportFSM::stop_playback (Event const & s)
{
	DEBUG_TRACE (DEBUG::TFSMEvents, "stop_playback\n");

	_last_locate.target = max_samplepos;
	current_roll_after_locate_status = boost::none;

	api->stop_transport (s.abort_capture, s.clear_state);
}

void
TransportFSM::set_roll_after (bool with_roll) const
{
	current_roll_after_locate_status = with_roll;
}

void
TransportFSM::start_declick_for_locate (Event const & l)
{
	assert (l.type == Locate);
	DEBUG_TRACE (DEBUG::TFSMEvents, string_compose ("start_declick_for_locate, crals %1 ltd %2 speed %3 sral %4\n", (bool) current_roll_after_locate_status,
	                                                enum_2_string (l.ltd), api->speed(), api->should_roll_after_locate()));
	_last_locate = l;

	if (!current_roll_after_locate_status) {
		set_roll_after (compute_should_roll (l.ltd));
	}
	api->stop_transport (false, false);
}

void
TransportFSM::start_locate_while_stopped (Event const & l) const
{
	assert (l.type == Locate);
	DEBUG_TRACE (DEBUG::TFSMEvents, "start_locate_while_stopped\n");

	set_roll_after (compute_should_roll (l.ltd));

	api->locate (l.target, current_roll_after_locate_status.get(), l.with_flush, l.for_loop_end, l.force);
}

bool
TransportFSM::compute_should_roll (LocateTransportDisposition ltd) const
{
	switch (ltd) {
	case MustRoll:
		return true;
	case MustStop:
		return false;
	case RollIfAppropriate:
		/* by the time we call this, if we were rolling before the
		   locate, we've already transitioned into DeclickToLocate
		*/
		if (_motion_state == DeclickToLocate) {
			return true;
		} else {
			return api->should_roll_after_locate ();
		}
		break;
	}
	/*NOTREACHED*/
	return true;
}

void
TransportFSM::locate_for_loop (Event const & l)
{
	assert (l.type == Locate);
	DEBUG_TRACE (DEBUG::TFSMEvents, string_compose ("locate_for_loop, wl = %1\n", l.for_loop_end));

	const bool should_roll = compute_should_roll (l.ltd);
	current_roll_after_locate_status = should_roll;
	_last_locate = l;
	api->locate (l.target, should_roll, l.with_flush, l.for_loop_end, l.force);
}

void
TransportFSM::start_locate_after_declick () const
{
	DEBUG_TRACE (DEBUG::TFSMEvents, string_compose ("start_locate_after_declick, have crals ? %1 roll will be %2\n", (bool) current_roll_after_locate_status,
	                                                current_roll_after_locate_status ? current_roll_after_locate_status.get() : compute_should_roll (_last_locate.ltd)));

	const bool roll = current_roll_after_locate_status ? current_roll_after_locate_status.get() : compute_should_roll (_last_locate.ltd);
	api->locate (_last_locate.target, roll, _last_locate.with_flush, _last_locate.for_loop_end, _last_locate.force);
}

void
TransportFSM::interrupt_locate (Event const & l) const
{
	assert (l.type == Locate);
	DEBUG_TRACE (DEBUG::TFSMEvents, string_compose ("interrupt to %1 versus %2\n", l.target, _last_locate.target));

	/* Because of snapping (e.g. of mouse position) we could be
	 * interrupting an existing locate to the same position. If we go ahead
	 * with this, the code in Session::do_locate() will notice that it's a
	 * repeat position, will do nothing, will queue a "locate_done" event
	 * that will arrive in the next process cycle. But this event may be
	 * processed before the original (real) locate has completed in the
	 * butler thread, and processing it may transition us back to Rolling
	 * before some (or even all) tracks are actually ready.
	 *
	 * So, we must avoid this from happening, and this seems like the
	 * simplest way.
	 */

	if (l.target == _last_locate.target && !l.force) {
		return;
	}
	/* maintain original "with-roll" choice of initial locate, even though
	 * we are interrupting the locate to start a new one.
	 */
	_last_locate = l;
	api->locate (l.target, false, l.with_flush, l.for_loop_end, l.force);
}

void
TransportFSM::schedule_butler_for_transport_work () const
{
	api->schedule_butler_for_transport_work ();
}

bool
TransportFSM::should_roll_after_locate () const
{
	bool roll;

	if (current_roll_after_locate_status) {
		roll = current_roll_after_locate_status.get();
		current_roll_after_locate_status = boost::none; // used it
	} else {
		roll = api->should_roll_after_locate ();
	}

	DEBUG_TRACE (DEBUG::TFSMEvents, string_compose ("should_roll_after_locate() ? %1\n", roll));
	return roll;
}

void
TransportFSM::roll_after_locate () const
{
	DEBUG_TRACE (DEBUG::TFSMEvents, string_compose ("rolling after locate, was for_loop ? %1\n", _last_locate.for_loop_end));
	current_roll_after_locate_status = boost::none;
	api->start_transport ();
}

void
TransportFSM::defer (Event& ev)
{
	DEBUG_TRACE (DEBUG::TFSMEvents, string_compose ("Defer %1 during %2\n", enum_2_string (ev.type), current_state()));
	deferred_events.push_back (ev);
}

void
TransportFSM::transition (MotionState ms)
{
	const MotionState old = _motion_state;
	_motion_state = ms;
	DEBUG_TRACE (DEBUG::TFSMState, string_compose ("Leave %1, enter %2\n", enum_2_string (old), current_state()));
}

void
TransportFSM::transition (ButlerState bs)
{
	const ButlerState old = _butler_state;
	_butler_state = bs;
	DEBUG_TRACE (DEBUG::TFSMState, string_compose ("Leave %1, enter %2\n", enum_2_string (old), current_state()));
}

void
TransportFSM::transition (DirectionState ds)
{
	const DirectionState old = _direction_state;
	_direction_state = ds;
	DEBUG_TRACE (DEBUG::TFSMState, string_compose ("Leave %1, enter %2\n", enum_2_string (old), current_state()));
}

void
TransportFSM::enqueue (Event* ev)
{
	DEBUG_TRACE (DEBUG::TFSMState, string_compose ("queue tfsm event %1\n", enum_2_string (ev->type)));
	queued_events.push_back (*ev);
	if (!processing) {
		process_events ();
	}
}

void
TransportFSM::set_speed (Event const & ev)
{
	bool initial_reverse = false;

	assert (ev.speed != 0.0);

	DEBUG_TRACE (DEBUG::TFSMState, string_compose ("%1, target speed %2 MRRS %3 state %4\n", (ev.speed == 0.0 ? "stopping" : "continue"), ev.speed, most_recently_requested_speed, current_state()));
	api->set_transport_speed (ev.speed, ev.as_default);

	const double mrrs = most_recently_requested_speed;

	/* corner case: first call to ::set_speed() has a negative
	 * speed
	 */

	if (most_recently_requested_speed == std::numeric_limits<double>::max()) {
		/* have never rolled yet */
		initial_reverse = true;
	}

	most_recently_requested_speed = ev.speed;

	if (ev.speed * mrrs < 0.0 || initial_reverse) {

		/* direction change */

		DEBUG_TRACE (DEBUG::TFSMState, string_compose ("switch-directions, target speed %1 MRRS %2 state %3 IR %4\n", ev.speed, mrrs, current_state(), initial_reverse));

		last_speed_request = ev;
		transition (Reversing);

		Event lev (Locate, api->position(), RollIfAppropriate, false, false, true);

		transition (DeclickToLocate);
		start_declick_for_locate (lev);
	}
}

bool
TransportFSM::will_roll_fowards () const
{
	if (reversing()) {
		return most_recently_requested_speed >= 0; /* note: future speed of zero is equivalent to Forwards */
	}
	return (_direction_state == Forwards);
}
