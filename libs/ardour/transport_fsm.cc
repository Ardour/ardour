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

#include "pbd/error.h"
#include "pbd/i18n.h"

#include "ardour/debug.h"
#include "ardour/session.h"
#include "ardour/transport_fsm.h"

using namespace ARDOUR;
using namespace PBD;

Pool* TransportFSM::FSMEvent::pool = 0;

void
TransportFSM::FSMEvent::init_pool ()
{
	pool = new Pool (X_("FSMEvents"), sizeof (FSMEvent), 128);
}

void*
TransportFSM::FSMEvent::operator new (size_t)
{
	return pool->alloc();
 }

void
TransportFSM::FSMEvent::operator delete (void *ptr, size_t /*size*/)
{
	return pool->release (ptr);
}

TransportFSM::TransportFSM (TransportAPI& tapi)
	: _last_locate (Locate)
	, _last_stop (StopTransport)
	, api (&tapi)
	, processing (0)
{
	init ();
}

void
TransportFSM::init ()
{
	_motion_state = Stopped;
	_butler_state = NotWaitingForButler;
}

void
TransportFSM::process_events ()
{
	processing++;

	while (!queued_events.empty()) {

		MotionState oms = _motion_state;
		ButlerState obs = _butler_state;

		if (process_event (queued_events.front())) { /* event processed successfully */

			if (oms != _motion_state || obs != _butler_state) {

				/* state changed, so now check deferred events
				 * to see if they can be processed now
				 */

				if (!deferred_events.empty() ){
					DEBUG_TRACE (DEBUG::TFSMEvents, string_compose ("processing %1 deferred events\n", deferred_events.size()));

					for (EventList::iterator e = deferred_events.begin(); e != deferred_events.end(); ) {
						FSMEvent* deferred_ev = &(*e);
						if (process_event (*e)) { /* event processed, remove from deferred */
							e = deferred_events.erase (e);
							delete deferred_ev;
						} else {
							++e;
						}
					}
				}
			}
		}

		FSMEvent* ev = &queued_events.front();
		queued_events.pop_front ();
		delete ev;
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
a_row < Stopped,             locate,          WaitingForLocate,  &T::start_locate                                        >,
g_row < WaitingForLocate,    locate_done,     Stopped,                                  &T::should_not_roll_after_locate >,
_row  < Rolling,             butler_done,     Rolling                                                                    >,
_row  < Rolling,             start_transport, Rolling                                                                    >,
a_row < Rolling,             stop_transport,  DeclickToStop,     &T::start_declick                                       >,
a_row < DeclickToStop,       declick_done,    Stopped,           &T::stop_playback                                       >,
a_row < Rolling,             locate,          DeclickToLocate,   &T::save_locate_and_start_declick                       >,
a_row < DeclickToLocate,     declick_done,    WaitingForLocate,  &T::start_saved_locate                                  >,
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
	s << enum_2_string (_motion_state) << '/' << enum_2_string (_butler_state);
	return s.str();
}

void
TransportFSM::bad_transition (FSMEvent const & ev)
{
	error << "bad transition, current state = " << current_state() << " event = " << enum_2_string (ev.type) << endmsg;
	std::cerr << "bad transition, current state = " << current_state() << " event = " << enum_2_string (ev.type) << std::endl;
}

bool
TransportFSM::process_event (FSMEvent& ev)
{
	DEBUG_TRACE (DEBUG::TFSMEvents, string_compose ("process %1\n", enum_2_string (ev.type)));

	switch (ev.type) {

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
			defer (ev);
			break;
		case DeclickToStop:
			defer (ev);
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
			start_declick (ev);
			break;
		case Stopped:
			break;
		case DeclickToLocate:
		case WaitingForLocate:
			defer (ev);
			break;
		default:
			bad_transition (ev); return false;
			break;
		}
		break;

	case Locate:
		switch (_motion_state) {
		case Stopped:
			transition (WaitingForLocate);
			start_locate (ev);
			break;
		case Rolling:
			transition (DeclickToLocate);
			save_locate_and_start_declick (ev);
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
			if (should_not_roll_after_locate()) {
				transition (Stopped);
			} else {
				transition (Rolling);
				roll_after_locate ();
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
			start_saved_locate ();
			break;
		case DeclickToStop:
			transition (Stopped);
			stop_playback ();
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
	DEBUG_TRACE (DEBUG::TFSMEvents, "tfsm::start_playback\n");
	api->start_transport();
}

void
TransportFSM::start_declick (FSMEvent const & s)
{
	assert (s.type == StopTransport);
	DEBUG_TRACE (DEBUG::TFSMEvents, "tfsm::start_declick\n");
	_last_stop = s;
}

void
TransportFSM::stop_playback ()
{
	DEBUG_TRACE (DEBUG::TFSMEvents, "tfsm::stop_playback\n");
	api->stop_transport (_last_stop.abort, _last_stop.clear_state);
}

void
TransportFSM::save_locate_and_start_declick (FSMEvent const & l)
{
	assert (l.type == Locate);
	DEBUG_TRACE (DEBUG::TFSMEvents, "tfsm::save_locate_and_stop\n");
	_last_locate = l;
	_last_stop = FSMEvent (StopTransport, false, false);
}

void
TransportFSM::start_locate (FSMEvent const & l)
{
	assert (l.type == Locate);
	DEBUG_TRACE (DEBUG::TFSMEvents, "tfsm::start_locate\n");
	api->locate (l.target, l.with_roll, l.with_flush, l.with_loop, l.force);
}

void
TransportFSM::start_saved_locate ()
{
	DEBUG_TRACE (DEBUG::TFSMEvents, "tfsm::start_save\n");
	api->locate (_last_locate.target, _last_locate.with_roll, _last_locate.with_flush, _last_locate.with_loop, _last_locate.force);
}

void
TransportFSM::interrupt_locate (FSMEvent const & l)
{
	assert (l.type == Locate);
	DEBUG_TRACE (DEBUG::TFSMEvents, "tfsm::interrupt\n");
	/* maintain original "with-roll" choice of initial locate, even though
	 * we are interrupting the locate to start a new one.
	 */
	api->locate (l.target, _last_locate.with_roll, l.with_flush, l.with_loop, l.force);
}

void
TransportFSM::schedule_butler_for_transport_work ()
{
	api->schedule_butler_for_transport_work ();
}

bool
TransportFSM::should_roll_after_locate ()
{
	bool ret = api->should_roll_after_locate ();
	DEBUG_TRACE (DEBUG::TFSMEvents, string_compose ("tfsm::should_roll_after_locate() ? %1\n", ret));
	return ret;
}

void
TransportFSM::roll_after_locate ()
{
	DEBUG_TRACE (DEBUG::TFSMEvents, "rolling after locate\n");
	api->start_transport ();
}

void
TransportFSM::defer (FSMEvent& ev)
{
	DEBUG_TRACE (DEBUG::TFSMEvents, string_compose ("Defer %1 during %2\n", enum_2_string (ev.type), current_state()));
	deferred_events.push_back (ev);
}

void
TransportFSM::transition (MotionState ms)
{
	DEBUG_TRACE (DEBUG::TFSMState, string_compose ("Leave %1, enter %2\n", enum_2_string (_motion_state), enum_2_string (ms)));
	_motion_state = ms;
}

void
TransportFSM::transition (ButlerState bs)
{
	DEBUG_TRACE (DEBUG::TFSMState, string_compose ("Leave %1, enter %2\n", enum_2_string (_butler_state), enum_2_string (bs)));
	_butler_state = bs;
}
