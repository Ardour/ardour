#ifndef _ardour_transport_fsm_h_
#define _ardour_transport_fsm_h_

#include <list>
#include <queue>

#include <boost/intrusive/list.hpp>
#include <string>
#include <utility>
#include <iostream>

#include "pbd/demangle.h"
#include "pbd/stacktrace.h"

#include "ardour/debug.h"
#include "ardour/types.h"

namespace ARDOUR
{

class TransportAPI;

struct TransportFSM
{
	/* All code related to this object is expected to be run synchronously
	 * and single-threaded from the process callback. It can be re-entrant
	 * if handling one transport state change queues another state change,
	 * but that is handled explicitly (see the @param processing member and
	 * its usage).
	 */

  public:
	enum EventType {
		ButlerDone,
		ButlerRequired,
		DeclickDone,
		StartTransport,
		StopTransport,
		Locate,
		LocateDone
	};

	struct Event : public boost::intrusive::list_base_hook<> {
		EventType type;
		union {
			bool abort; /* for stop */
			bool with_roll; /* for locate */
		};
		union {
			bool clear_state; /* for stop */
			bool with_flush; /* for locate */
		};
		/* for locate */
		samplepos_t target;
		bool with_loop;
		bool force;

		Event (EventType t)
			: type (t)
			, with_roll (false)
			, with_flush (false)
			, target (0)
			, with_loop (false)
			, force (false)
		{}
		Event (EventType t, bool ab, bool cl)
			: type (t)
			, abort (ab)
			, clear_state (cl)
		{
			assert (t == StopTransport);
		}
		Event (EventType t, samplepos_t pos, bool r, bool fl, bool lp, bool f4c)
			: type (t)
			, with_roll (r)
			, with_flush (fl)
			, target (pos)
			, with_loop (lp)
			, force (f4c)
		{
			assert (t == Locate);
		}

		void* operator new (size_t);
		void  operator delete (void *ptr, size_t /*size*/);

		static void init_pool ();

          private:
		static Pool* pool;

	};

	TransportFSM (TransportAPI& tapi);

	void start () {
		init ();
	}

	void stop () {
		/* should we do anything here? this method is modelled on the
		   boost::msm design, but its not clear that we ever need to
		   do anything like this.
		*/
	}

	enum MotionState {
		Stopped,
		Rolling,
		DeclickToStop,
		DeclickToLocate,
		WaitingForLocate
	};

	enum ButlerState {
		NotWaitingForButler,
		WaitingForButler
	};

	std::string current_state () const;

  private:
	MotionState _motion_state;
	ButlerState _butler_state;

	void init();

	/* transition actions */

	void schedule_butler_for_transport_work ();
	void start_playback ();
	void stop_playback ();
	void start_saved_locate ();
	void roll_after_locate ();
	void start_locate (Event const &);
	void interrupt_locate (Event const &);
	void save_locate_and_start_declick (Event const &);
	void start_declick (Event const &);

	/* guards */

	bool should_roll_after_locate ();
	bool should_not_roll_after_locate ()  { return !should_roll_after_locate (); }

  public:
	bool locating ()                     { return _motion_state == WaitingForLocate; }
	bool rolling ()                      { return _motion_state == Rolling; }
	bool stopped ()                      { return _motion_state == Stopped; }
	bool waiting_for_butler()            { return _butler_state == WaitingForButler; }
	bool declick_in_progress()           { return _motion_state == DeclickToLocate || _motion_state == DeclickToStop; }

	void enqueue (Event* ev) {
		queued_events.push_back (*ev);
		if (!processing) {
			process_events ();
		}
	}

  private:

	void transition (MotionState ms);
	void transition (ButlerState bs);

	void process_events ();
	bool process_event (Event&);

	Event _last_locate;
	Event _last_stop;

	TransportAPI* api;
	typedef boost::intrusive::list<Event> EventList;
	EventList queued_events;
	EventList deferred_events;
	int processing;

	void defer (Event& ev);
	void bad_transition (Event const &);
};

} /* end namespace ARDOUR */

#endif
