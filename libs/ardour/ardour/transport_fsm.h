#ifndef _ardour_transport_fsm_h_
#define _ardour_transport_fsm_h_

#ifdef nil
#undef nil
#endif

#include <boost/weak_ptr.hpp>
#include <boost/msm/back/state_machine.hpp>
#include <boost/msm/back/tools.hpp>
#include <boost/msm/front/state_machine_def.hpp>
#include <boost/msm/front/functor_row.hpp>

#include "pbd/demangle.h"
#include "pbd/stacktrace.h"

#include "ardour/debug.h"

/* state machine */
namespace msm = boost::msm;
namespace mpl = boost::mpl;

namespace ARDOUR
{

class TransportAPI;

struct TransportFSM : public msm::front::state_machine_def<TransportFSM>
{
	/* events to be delivered to the FSM */

	struct butler_done {};
	struct butler_required {};
	struct declick_done {};
	struct start_transport {};

	struct stop_transport {
		stop_transport (bool ab = false, bool cl = false)
			: abort (ab)
			, clear_state (cl) {}

		bool abort;
		bool clear_state;
	};

	struct locate {
		locate ()
			: target (0)
			, with_roll (false)
			, with_flush (false)
			, with_loop (false)
			, force (false) {}

		locate (samplepos_t target, bool roll, bool flush, bool loop, bool f4c)
			: target (target)
			, with_roll (roll)
			, with_flush (flush)
			, with_loop (loop)
			, force (f4c) {}

		samplepos_t target;
		bool with_roll;
		bool with_flush;
		bool with_loop;
		bool force;
	};

	struct locate_done {};

	/* Flags */

	struct DeclickInProgress {};
	struct LocateInProgress {};
	struct IsRolling {};
	struct IsStopped {};
	struct IsWaitingForButler {};

	typedef msm::active_state_switch_before_transition active_state_switch_policy;

	/* transition actions */

	void start_playback (start_transport const& p);
	void roll_after_locate (locate_done const& p);
	void stop_playback (declick_done const& s);
	void start_locate (locate const& s);
	void start_saved_locate (declick_done const& s);
	void interrupt_locate (locate const& s);
	void schedule_butler_for_transport_work (butler_required const&);
	void save_locate_and_start_declick (locate const &);
	void start_declick (stop_transport const &);

	/* guards */

	bool should_roll_after_locate (locate_done const &);
	bool should_not_roll_after_locate (locate_done const & e)  { return !should_roll_after_locate (e); }

#define define_state(State) \
	struct State : public msm::front::state<> \
	{ \
		template <class Event,class FSM> void on_entry (Event const&, FSM&) { DEBUG_TRACE (PBD::DEBUG::TFSMState, "entering: " # State "\n"); } \
		template <class Event,class FSM> void on_exit (Event const&, FSM&) { DEBUG_TRACE (PBD::DEBUG::TFSMState, "leaving: " # State "\n"); } \
	}

#define define_state_flag(State,Flag) \
	struct State : public msm::front::state<> \
	{ \
		template <class Event,class FSM> void on_entry (Event const&, FSM&) { DEBUG_TRACE (PBD::DEBUG::TFSMState, "entering: " # State "\n"); } \
		template <class Event,class FSM> void on_exit (Event const&, FSM&) { DEBUG_TRACE (PBD::DEBUG::TFSMState, "leaving: " # State "\n"); } \
		typedef mpl::vector1<Flag> flag_list; \
	}

#define define_state_flag2(State,Flag1,Flag2) \
	struct State : public msm::front::state<> \
	{ \
		template <class Event,class FSM> void on_entry (Event const&, FSM&) { DEBUG_TRACE (PBD::DEBUG::TFSMState, "entering: " # State "\n"); } \
		template <class Event,class FSM> void on_exit (Event const&, FSM&) { DEBUG_TRACE (PBD::DEBUG::TFSMState, "leaving: " # State "\n"); } \
		typedef mpl::vector2<Flag1,Flag2> flag_list; \
	}

	/* FSM states */

	define_state_flag (WaitingForButler, IsWaitingForButler);
	define_state (NotWaitingForButler);
	define_state_flag (Stopped,IsStopped);
	define_state_flag (Rolling,IsRolling);
	define_state_flag (DeclickToLocate,DeclickInProgress);
	define_state_flag (WaitingForLocate,LocateInProgress);
	define_state_flag (DeclickToStop,DeclickInProgress);

	// Pick a back-end
	typedef msm::back::state_machine<TransportFSM> back;

	boost::weak_ptr<back> wp;

	bool locating ()                     { return backend()->is_flag_active<LocateInProgress>(); }
	bool locating (declick_done const &) { return locating(); }
	bool rolling ()                      { return backend()->is_flag_active<IsRolling>(); }
	bool stopped ()                      { return backend()->is_flag_active<IsStopped>(); }
	bool waiting_for_butler()            { return backend()->is_flag_active<IsWaitingForButler>(); }
	bool declick_in_progress()           { return backend()->is_flag_active<DeclickInProgress>(); }

	static boost::shared_ptr<back> create(TransportAPI& api) {

		boost::shared_ptr<back> p (new back ());

		p->wp = p;
		p->api = &api;
		return p;
	}

	boost::shared_ptr<back> backend() { return wp.lock(); }

	template<typename Event> void enqueue (Event const & e) {
		backend()->process_event (e);
	}

	/* the initial state */
	typedef boost::mpl::vector<Stopped,NotWaitingForButler> initial_state;

	/* transition table */
	typedef TransportFSM T; // makes transition table cleaner

	struct transition_table : mpl::vector<
		//      Start                Event            Next               Action                Guard
		//    +----------------------+----------------+------------------+---------------------+----------------------+
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
		> {};

	typedef int activate_deferred_events;

	locate         _last_locate;
	stop_transport _last_stop;

	TransportAPI* api;

	// Replaces the default no-transition response.
	template <class FSM,class Event>
	void no_transition(Event const& e, FSM&,int state)
	{
		typedef typename boost::msm::back::recursive_get_transition_table<FSM>::type recursive_stt;
		typedef typename boost::msm::back::generate_state_set<recursive_stt>::type all_states;
		std::string stateName;
		boost::mpl::for_each<all_states,boost::msm::wrap<boost::mpl::placeholders::_1> >(boost::msm::back::get_state_name<recursive_stt>(stateName, state));
		std::cout << "No transition from state: " << PBD::demangle (stateName) << " on event " << typeid(e).name() << std::endl;
	}
};

} /* end namespace ARDOUR */

#endif
