#ifndef _ardour_transport_fsm_h_
#define _ardour_transport_fsm_h_

#include <list>
#include <queue>

#include <boost/intrusive/list.hpp>
#include <boost/optional.hpp>

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
		/* for stop */
		bool abort;
		bool clear_state;
		/* for locate */
		LocateTransportDisposition ltd;
		bool with_flush;
		samplepos_t target;
		bool for_loop_end;
		bool force;

		Event (EventType t)
			: type (t)
			, abort (false)
			, clear_state (false)
			, ltd (MustStop)
			, with_flush (false)
			, target (0)
			, for_loop_end (false)
			, force (false)
		{
			assert (t != StopTransport);
			assert (t != Locate);
		}
		Event (EventType t, bool ab, bool cl)
			: type (t)
			, abort (ab)
			, clear_state (cl)
			, ltd (MustStop)
			, with_flush (false)
			, target (0)
			, for_loop_end (false)
			, force (false)
		{
			assert (t == StopTransport);
		}
		Event (EventType t, samplepos_t pos, LocateTransportDisposition l, bool fl, bool lp, bool f4c)
			: type (t)
			, abort (false)
			, clear_state (false)
			, ltd (l)
			, with_flush (fl)
			, target (pos)
			, for_loop_end (lp)
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

	void schedule_butler_for_transport_work () const;
	void start_playback ();
	void stop_playback (Event const &);
	void start_locate_after_declick () const;
	void locate_for_loop (Event const &);
	void roll_after_locate () const;
	void start_locate_while_stopped (Event const &) const;
	void interrupt_locate (Event const &) const;
	void start_declick_for_locate (Event const &);

	/* guards */

	bool should_roll_after_locate () const;
	bool should_not_roll_after_locate ()  const { return !should_roll_after_locate (); }

  public:
	bool locating () const           { return _motion_state == WaitingForLocate; }
	bool rolling () const            { return _motion_state == Rolling; }
	bool stopped () const            { return _motion_state == Stopped; }
	bool stopping () const           { return _motion_state == DeclickToStop; }
	bool waiting_for_butler() const  { return _butler_state == WaitingForButler; }
	bool declick_in_progress() const { return _motion_state == DeclickToLocate || _motion_state == DeclickToStop; }
	bool declicking_for_locate() const { return _motion_state == DeclickToLocate; }

	void enqueue (Event* ev);

  private:

	void transition (MotionState ms);
	void transition (ButlerState bs);

	void process_events ();
	bool process_event (Event&, bool was_deferred, bool& deferred);

	Event _last_locate;

	TransportAPI* api;
	typedef boost::intrusive::list<Event> EventList;
	EventList queued_events;
	EventList deferred_events;
	int processing;
	mutable boost::optional<bool> current_roll_after_locate_status;

	void defer (Event& ev);
	void bad_transition (Event const &);
	void set_roll_after (bool) const;
	bool compute_should_roll (LocateTransportDisposition) const;
};

} /* end namespace ARDOUR */

#endif
