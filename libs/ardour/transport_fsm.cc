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

/*

autoplay off (hint: state remains the same):

[STOPPED]  -> LOCATE -> [STOPPED]
[ROLLING]  -> LOCATE -> [ROLLING]
[RWD|FFWD] -> LOCATE -> [RWD|FFWD]

autoplay on (hint: state is always rolling):

[STOPPED]  -> LOCATE -> [ROLLING]
[ROLLING]  -> LOCATE -> [ROLLING]
[RWD|FFWD] -> LOCATE -> [ROLLING]

the problem child is the last one. The final ROLLING state is intended to be
forwards at the default speed. But if we were rewinding, we need a reverse.

if autoplay is the determining factor in differentiating these two, we must
make it available via TransportAPI. but let's rename it as something slightly
more on-topic.

*/

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
	: _motion_state (Stopped)
	, _butler_state (NotWaitingForButler)
	, _direction_state (Forwards)
	, _transport_speed (0)
	, _last_locate (Locate, max_samplepos, MustRoll, false, false) /* all but first two argument don't matter */
	, api (&tapi)
	, processing (0)
	, most_recently_requested_speed (std::numeric_limits<double>::max())
	, _default_speed (1.0)
	, _reverse_after_declick (0)
{
	init ();
}

void
TransportFSM::init ()
{
}

void
TransportFSM::process_events ()
{
	processing++;

	while (!queued_events.empty()) {

		MotionState oms = _motion_state;
		ButlerState obs = _butler_state;
		DirectionState ods = _direction_state;

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

			if (oms != _motion_state || obs != _butler_state || ods != _direction_state) {

				/* state changed, so now check deferred events
				 * to see if they can be processed now
				 */

				if (!deferred_events.empty() ){

					DEBUG_TRACE (DEBUG::TFSMEvents, string_compose ("processing %1 deferred events\n", deferred_events.size()));

					for (EventList::iterator e = deferred_events.begin(); e != deferred_events.end(); ) {
						Event* deferred_ev = &(*e);
						bool deferred2;

						if (process_event (*e, true, deferred2)) {
							if (!deferred2) { /* event processed and not deferred again, remove from deferred */
								DEBUG_TRACE (DEBUG::TFSMEvents, string_compose ("processed deferred event %1, now deleting\n", enum_2_string (deferred_ev->type)));
								e = deferred_events.erase (e);
								delete deferred_ev;
							} else {
								DEBUG_TRACE (DEBUG::TFSMEvents, string_compose ("Re-Defer deferred event %1\n", enum_2_string (deferred_ev->type)));
								++e;
							}
						} else { /* process error */
							DEBUG_TRACE (DEBUG::TFSMEvents, string_compose ("deferred event %1 failed, deleting\n", enum_2_string (deferred_ev->type)));
							++e;
							delete deferred_ev;
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
			}
			deferred = true;
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
				}
				deferred = true;
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
			}
			deferred = true;
			break;
		case DeclickToStop:
			if (!already_deferred) {
				defer (ev);
			}
			deferred = true;
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
			}
			deferred = true;
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
		DEBUG_TRACE (DEBUG::TFSMEvents, string_compose ("locate, ltd = %1 target = %2 for loop end %3 force %4\n",
		                                                enum_2_string (ev.ltd),
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

				if (should_not_roll_after_locate()) {
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
			if (_reverse_after_declick) {
				transition (Reversing);
			}
			transition (WaitingForLocate);
			start_locate_after_declick ();
			break;
		case DeclickToStop:
			if (!maybe_reset_speed ()) {
				transition (Stopped);
				/* transport already stopped */
			}
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

	if (most_recently_requested_speed == std::numeric_limits<double>::max()) {
		/* we started rolling without ever setting speed; that's an implicit
		 * call to set_speed (1.0)
		 */
		most_recently_requested_speed = 1.0;
	} else {
		api->set_transport_speed (most_recently_requested_speed);
	}

	api->start_transport (false);
}

void
TransportFSM::stop_playback (Event const & s)
{
	DEBUG_TRACE (DEBUG::TFSMEvents, "stop_playback\n");

	_last_locate.target = max_samplepos;
	current_roll_after_locate_status = boost::none;

	api->stop_transport (s.abort_capture, s.clear_state);
}

bool
TransportFSM::maybe_reset_speed ()
{
	bool state_changed = false;

	if (Config->get_reset_default_speed_on_stop()) {

		if (most_recently_requested_speed != 1.0 || default_speed() != 1.0) {
			set_default_speed(1.0);
			set_speed (Event (1.0));
			state_changed = true;
		}

	} else {

		/* We're not resetting back to 1.0, but we may need to handle a
		 * speed change from whatever we have been rolling at to
		 * whatever the current default is. We could have been
		 * rewinding at -4.5 ... when we restart, we need to play at
		 * the current _default_speed
		 */

		if (most_recently_requested_speed != _default_speed) {
			state_changed = set_speed (Event (_default_speed));
		}
	}

	return state_changed;
}

void
TransportFSM::start_declick_for_locate (Event const & l)
{
	assert (l.type == Locate);
	DEBUG_TRACE (DEBUG::TFSMEvents, string_compose ("start_declick_for_locate, crals %1 ltd %2 sral %3\n", (bool) current_roll_after_locate_status,
	                                                enum_2_string (l.ltd), api->should_roll_after_locate()));
	_last_locate = l;

	if (!current_roll_after_locate_status) {
		set_roll_after (compute_should_roll (l.ltd));
	}
	api->stop_transport (false, false);
}

bool
TransportFSM::compute_should_roll (LocateTransportDisposition ltd) const
{
	switch (ltd) {
	case MustRoll:
		return true;
	case MustStop:
		return false;
	default:
		break;
	}

	/* case RollIfAppropriate */

	/* by the time we call this, if we were rolling before the
	   locate, we've already transitioned into DeclickToLocate,
	   so DeclickToLocate is essentially a synonym for "was Rolling".
	*/
	if (_motion_state == DeclickToLocate) {
		return true;
	}

	return api->should_roll_after_locate ();
}

void
TransportFSM::set_roll_after (bool with_roll) const
{
	current_roll_after_locate_status = with_roll;
}

void
TransportFSM::start_locate_while_stopped (Event const & l) const
{
	assert (l.type == Locate);
	DEBUG_TRACE (DEBUG::TFSMEvents, "start_locate_while_stopped\n");

	set_roll_after (compute_should_roll (l.ltd));
	api->locate (l.target, l.for_loop_end, l.force);
}

void
TransportFSM::locate_for_loop (Event const & l)
{
	assert (l.type == Locate);
	DEBUG_TRACE (DEBUG::TFSMEvents, string_compose ("locate_for_loop, wl = %1\n", l.for_loop_end));

	_last_locate = l;
	set_roll_after (compute_should_roll (l.ltd));
	api->locate (l.target, l.for_loop_end, l.force);
}

void
TransportFSM::start_locate_after_declick ()
{
	DEBUG_TRACE (DEBUG::TFSMEvents, string_compose ("start_locate_after_declick, have crals ? %1 roll will be %2\n", (bool) current_roll_after_locate_status,
	                                                current_roll_after_locate_status ? current_roll_after_locate_status.get() : compute_should_roll (_last_locate.ltd)));

	/* we only get here because a locate request arrived while we were rolling. We declicked and that is now finished */

	/* Special case: we were rolling. If the user has set auto-play, then
	   post-locate, we should roll at the default speed, which may involve
	   a reversal and that needs to be setup before we actually locate.
	*/

	double post_locate_speed;

	if (api->user_roll_after_locate() && !_reverse_after_declick) {
		post_locate_speed = _default_speed;
	} else {
		post_locate_speed = most_recently_requested_speed;
	}

	if (post_locate_speed * most_recently_requested_speed < 0) {
		/* different directions */
		transition (Reversing);
	}

	DEBUG_TRACE (DEBUG::TFSMEvents, string_compose ("post-locate speed will be %1 based on user-roll-after %2 and r-a-dc %3\n", post_locate_speed, api->user_roll_after_locate(), _reverse_after_declick));

	if (_reverse_after_declick) {
		_reverse_after_declick--;
	}

	if (api->user_roll_after_locate()) {
		most_recently_requested_speed = post_locate_speed;
	}


	api->locate (_last_locate.target, _last_locate.for_loop_end, _last_locate.force);
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
	api->locate (l.target, l.for_loop_end, l.force);
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
	bool for_loop = _last_locate.for_loop_end;
	DEBUG_TRACE (DEBUG::TFSMEvents, string_compose ("rolling after locate, was for_loop ? %1\n", for_loop));
	current_roll_after_locate_status = boost::none;

	if (most_recently_requested_speed == std::numeric_limits<double>::max()) {
		/* we started rolling without ever setting speed; that's an implicit
		 * call to set_speed (1.0)
		 */
		most_recently_requested_speed = 1.0;
	}

	api->set_transport_speed (most_recently_requested_speed);
	api->start_transport (for_loop);
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
	_transport_speed = compute_transport_speed ();
	DEBUG_TRACE (DEBUG::TFSMState, string_compose ("Leave %1, enter %2\n", enum_2_string (old), current_state()));
}

void
TransportFSM::transition (ButlerState bs)
{
	const ButlerState old = _butler_state;
	_butler_state = bs;
	_transport_speed = compute_transport_speed ();
	DEBUG_TRACE (DEBUG::TFSMState, string_compose ("Leave %1, enter %2\n", enum_2_string (old), current_state()));
}

void
TransportFSM::transition (DirectionState ds)
{
	const DirectionState old = _direction_state;
	_direction_state = ds;
	_transport_speed = compute_transport_speed ();
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

int
TransportFSM::compute_transport_speed () const
{
	if (_motion_state != Rolling || _direction_state == Reversing) {
		return 0;
	}

	if (_direction_state == Backwards) {
		return -1;
	}

	return 1;
}

bool
TransportFSM::set_speed (Event const & ev)
{
	assert (ev.speed != 0.0);

	bool initial_reverse = false;
	bool must_reverse = false;

	DEBUG_TRACE (DEBUG::TFSMState, string_compose ("::set_speed(): target speed %1 MRRS %2 state %3\n", ev.speed, most_recently_requested_speed, current_state()));

	/* This "must_roll" value is a bit complicated to explain. If we were moving at anything other than normal speed+direction,
	   and then stop, and reset-default-speed-on-stop is enabled, Session::stop_transport() will queue a SetSpeed
	   request to restore the default speed to 1.0. However, because of the stop in progress, the SetSpeed event
	   will be deferred during the stop. Eventually, we will handle it, and call this method. If we need to reverse
	   direction, the locate will end up using compute_should_roll(), with rolling state already set to DeclickToLocate.

	   If we pass in RollIfAppropriate as the after-locate roll disposition, the logic there will conclude that we should roll.
	   This logic is correct in cases where we do a normal locate, but not for a locate-for-reverse when stopping.

	   So, instead of using RollIfAppropriate, determine here is our after-locate state should be rolling or stopped, and pass
	   MustRoll or MustStop to the locate request. This will get compute_should_roll() to do the right thing, and once the locate
	   is complete, we will be in the correct state.
	*/


	const bool must_roll = rolling();

	if (most_recently_requested_speed == std::numeric_limits<double>::max()) {
		/* have never rolled yet */
		initial_reverse = true;
	}

	if (ev.speed * most_recently_requested_speed < 0.0 || initial_reverse) {
		must_reverse = true;
	}

	api->set_transport_speed (ev.speed);

	/* corner case: first call to ::set_speed() has a negative
	 * speed
	 */

	most_recently_requested_speed = ev.speed;

	if (must_reverse) {

		/* direction change */

		DEBUG_TRACE (DEBUG::TFSMState, string_compose ("switch-directions, target speed %1 state %2 IR %3\n", ev.speed, current_state(), initial_reverse));

		Event lev (Locate, api->position(), must_roll ? MustRoll : MustStop, false, true);

		if (_transport_speed) {
			_reverse_after_declick++;
			transition (DeclickToLocate);
			start_declick_for_locate (lev);
		} else {
			transition (Reversing);
			transition (WaitingForLocate);
			start_locate_while_stopped (lev);
		}

		return true;
	}

	return false;
}

bool
TransportFSM::will_roll_fowards () const
{
	if (reversing() || _reverse_after_declick) {
		return most_recently_requested_speed >= 0; /* note: future speed of zero is equivalent to Forwards */
	}
	return (_direction_state == Forwards);
}
