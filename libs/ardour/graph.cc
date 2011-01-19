/*
  Copyright (C) 2010 Paul Davis
  Author: Torben Hohn

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
#include <stdio.h>
#include <cmath>

#include "pbd/compose.h"
#include "pbd/cpus.h"
#include "pbd/debug_rt_alloc.h"

#include "ardour/debug.h"
#include "ardour/graph.h"
#include "ardour/types.h"
#include "ardour/session.h"
#include "ardour/route.h"
#include "ardour/process_thread.h"
#include "ardour/audioengine.h"

#include <jack/thread.h>

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;

#ifdef DEBUG_RT_ALLOC
static Graph* graph = 0;
extern "C" {

int alloc_allowed ()
{
	return !graph->in_process_thread ();
}

}
#endif

Graph::Graph (Session & session) 
        : SessionHandleRef (session) 
	, _execution_sem ("graph_execution", 0)
	, _callback_start_sem ("graph_start", 0)
	, _callback_done_sem ("graph_done", 0)
	, _cleanup_sem ("graph_cleanup", 0)
{
        pthread_mutex_init( &_trigger_mutex, NULL);

	/* XXX: rather hacky `fix' to stop _trigger_queue.push_back() allocating
	   memory in the RT thread.
	*/
	_trigger_queue.reserve (8192);

        _execution_tokens = 0;

        _current_chain = 0;
        _pending_chain = 0;
        _setup_chain   = 1;
        _quit_threads = false;
        _graph_empty = true;

        int num_cpu = hardware_concurrency();
        int num_threads = num_cpu;
        int pu = Config->get_processor_usage ();

        if (pu < 0) {
                /* pu is negative: use "pu" less cores for DSP than appear to be available
                 */

                if (-pu < num_threads) {
                        num_threads += pu; 
                } else {
                        num_threads = 1;
                }
        } else {
                /* use "pu" cores, if available
                 */

                if (pu <= num_threads) {
                        num_threads = pu;
                } 
        }

        info << string_compose (_("Using %2 threads on %1 CPUs"), num_cpu, num_threads) << endmsg;

	pthread_t a_thread;

	if (AudioEngine::instance()->create_process_thread (boost::bind (&Graph::main_thread, this), &a_thread, 100000) == 0) {
		_thread_list.push_back (a_thread);
	}

        for (int i = 1; i < num_threads; ++i) {
		if (AudioEngine::instance()->create_process_thread (boost::bind (&Graph::helper_thread, this), &a_thread, 100000) == 0) {
			_thread_list.push_back (a_thread);
		}
        }

#ifdef DEBUG_RT_ALLOC	
	graph = this;
	pbd_alloc_allowed = &::alloc_allowed;
#endif	
}

void
Graph::session_going_away()
{
        _quit_threads = true;

        for (unsigned int i=0; i<_thread_list.size(); i++) {
		_execution_sem.signal ();
        }

        _callback_start_sem.signal ();

        for (list<pthread_t>::iterator i = _thread_list.begin(); i != _thread_list.end(); i++) {
                void* status;
                pthread_join (*i, &status);
        }

        // now drop all references on the nodes.
        _nodes_rt[0].clear();
        _nodes_rt[1].clear();
        _init_trigger_list[0].clear();
        _init_trigger_list[1].clear();
        _trigger_queue.clear();
}

void
Graph::clear_other_chain ()
{
        Glib::Mutex::Lock ls (_swap_mutex);

        while (1) {
                if (_setup_chain != _pending_chain) {

                        for (node_list_t::iterator ni=_nodes_rt[_setup_chain].begin(); ni!=_nodes_rt[_setup_chain].end(); ni++) {
                                (*ni)->_activation_set[_setup_chain].clear();
                        }

                        _nodes_rt[_setup_chain].clear ();
                        _init_trigger_list[_setup_chain].clear ();
                        break;
                }
                /* setup chain == pending chain - we have
                   to wait till this is no longer true.
                */
                _cleanup_cond.wait (_swap_mutex);                
        }
}

void
Graph::prep()
{
        node_list_t::iterator i;
        int chain;

        if (_swap_mutex.trylock()) {
                // we got the swap mutex.
                if (_current_chain != _pending_chain)
                {
                        // printf ("chain swap ! %d -> %d\n", _current_chain, _pending_chain);
                        _setup_chain = _current_chain;
                        _current_chain = _pending_chain;
                        _cleanup_cond.signal ();
                }
                _swap_mutex.unlock ();
        }

        chain = _current_chain;

        _graph_empty = true;
        for (i=_nodes_rt[chain].begin(); i!=_nodes_rt[chain].end(); i++) {
                (*i)->prep( chain);
                _graph_empty = false;
        }
        _finished_refcount = _init_finished_refcount[chain];

        for (i=_init_trigger_list[chain].begin(); i!=_init_trigger_list[chain].end(); i++) {
                this->trigger( i->get() );
        }
}

void
Graph::trigger (GraphNode* n)
{
        pthread_mutex_lock (&_trigger_mutex);
        _trigger_queue.push_back( n);
        pthread_mutex_unlock (&_trigger_mutex);
}

void
Graph::dec_ref()
{
        if (g_atomic_int_dec_and_test (&_finished_refcount)) {

                // ok... this cycle is finished now.
                // we are the only thread alive.
	
                this->restart_cycle();
        }
}

void
Graph::restart_cycle()
{
        //printf( "cycle_done chain: %d\n", _current_chain);

        // we are through. wakeup our caller.
  again:
        _callback_done_sem.signal ();

        // block until we are triggered.
        _callback_start_sem.wait();
        if (_quit_threads)
                return;

        //printf( "cycle_start\n" );

        this->prep();
        if (_graph_empty)
                goto again;
        //printf( "cycle_start chain: %d\n", _current_chain);

        // returning will restart the cycle.
        //  starting with waking up the others.
}

static bool
is_feedback (boost::shared_ptr<RouteList> routelist, Route* from, boost::shared_ptr<Route> to)
{
        for (RouteList::iterator ri=routelist->begin(); ri!=routelist->end(); ri++) {
                if ((*ri).get() == from)
                        return false;
                if ((*ri) == to)
                        return true;
        }
        assert(0);
        return false;
}

static bool
is_feedback (boost::shared_ptr<RouteList> routelist, boost::shared_ptr<Route> from, Route* to)
{
        for (RouteList::iterator ri=routelist->begin(); ri!=routelist->end(); ri++) {
                if ((*ri).get() == to)
                        return true;
                if ((*ri) == from)
                        return false;
        }
        assert(0);
        return false;
}

void
Graph::rechain (boost::shared_ptr<RouteList> routelist)
{
        node_list_t::iterator ni;
        Glib::Mutex::Lock ls (_swap_mutex);

        int chain = _setup_chain;
        DEBUG_TRACE (DEBUG::Graph, string_compose ("============== setup %1\n", chain));
        // set all refcounts to 0;

        _init_finished_refcount[chain] = 0;
        _init_trigger_list[chain].clear();

        _nodes_rt[chain].clear();

        for (RouteList::iterator ri=routelist->begin(); ri!=routelist->end(); ri++) {
                node_ptr_t n = boost::dynamic_pointer_cast<GraphNode> (*ri);

                n->_init_refcount[chain] = 0;
                n->_activation_set[chain].clear();
                _nodes_rt[chain].push_back(n);
        }

        // now add refs for the connections.

        for (ni=_nodes_rt[chain].begin(); ni!=_nodes_rt[chain].end(); ni++) {
                bool has_input  = false;
                bool has_output = false;

                boost::shared_ptr<Route> rp = boost::dynamic_pointer_cast<Route>( *ni);

                for (RouteList::iterator ri=routelist->begin(); ri!=routelist->end(); ri++) {
                        if (rp->direct_feeds (*ri)) {
                                if (is_feedback (routelist, rp.get(), *ri)) {
                                        continue; 
                                }

                                has_output = true;
                                (*ni)->_activation_set[chain].insert (boost::dynamic_pointer_cast<GraphNode> (*ri) );
                        }
                }

                for (Route::FedBy::iterator fi=rp->fed_by().begin(); fi!=rp->fed_by().end(); fi++) {
                        if (boost::shared_ptr<Route> r = fi->r.lock()) {
                                if (!is_feedback (routelist, r, rp.get())) {
                                        has_input = true;
                                }
                        }
                }

                for (node_set_t::iterator ai=(*ni)->_activation_set[chain].begin(); ai!=(*ni)->_activation_set[chain].end(); ai++) {
                        (*ai)->_init_refcount[chain] += 1;
                }

                if (!has_input)
                        _init_trigger_list[chain].push_back (*ni);

                if (!has_output)
                        _init_finished_refcount[chain] += 1;
        } 

        _pending_chain = chain;
        dump(chain);
}

bool
Graph::run_one()
{
        GraphNode* to_run;

        pthread_mutex_lock (&_trigger_mutex);
        if (_trigger_queue.size()) {
                to_run = _trigger_queue.back();
                _trigger_queue.pop_back();
        } else {
                to_run = 0;
        }

	int et = _execution_tokens;
	int ts = _trigger_queue.size();

        int wakeup = min (et, ts);
        _execution_tokens -= wakeup;

        for (int i=0; i<wakeup; i++ ) {
                _execution_sem.signal ();
        }

        while (to_run == 0) {
                _execution_tokens += 1;
                pthread_mutex_unlock (&_trigger_mutex);
                DEBUG_TRACE (DEBUG::ProcessThreads, string_compose ("%1 goes to sleep\n", pthread_self()));
                _execution_sem.wait ();
                if (_quit_threads)
                        return true;
                DEBUG_TRACE (DEBUG::ProcessThreads, string_compose ("%1 is awake\n", pthread_self()));
                pthread_mutex_lock (&_trigger_mutex);
                if (_trigger_queue.size()) {
                        to_run = _trigger_queue.back();
                        _trigger_queue.pop_back();
                }
        }
        pthread_mutex_unlock (&_trigger_mutex);

        to_run->process();
        to_run->finish (_current_chain);

        return false;
}

static void get_rt()
{
        if (!jack_is_realtime (AudioEngine::instance()->jack())) {
                return;
        }

        int priority = jack_client_real_time_priority (AudioEngine::instance()->jack());

        if (priority) {
                struct sched_param rtparam;
	
                memset (&rtparam, 0, sizeof (rtparam));
                rtparam.sched_priority = priority;
	
                pthread_setschedparam (pthread_self(), SCHED_FIFO, &rtparam);
        }
}

void
Graph::helper_thread()
{
	ProcessThread* pt = new ProcessThread ();

        pt->get_buffers();
        get_rt();

        while(1) {
                if (run_one()) {
                        break;
                }
        }

        pt->drop_buffers();
}

void
Graph::main_thread()
{
	ProcessThread* pt = new ProcessThread ();

        pt->get_buffers();
        get_rt();

  again:
        _callback_start_sem.wait ();
	DEBUG_TRACE(DEBUG::Graph, "main thread is awake\n");
        this->prep();

        if (_graph_empty && !_quit_threads) {
                _callback_done_sem.signal ();
                goto again;
        }

        while (1) {
		DEBUG_TRACE(DEBUG::Graph, "main thread runs one graph node\n");
                if (run_one()) {
                        break;
                }
        }

        pt->drop_buffers();
}

void
Graph::dump (int chain)
{
#ifndef NDEBUG
        node_list_t::iterator ni;
        node_set_t::iterator ai;

        chain = _pending_chain;

        DEBUG_TRACE (DEBUG::Graph, "--------------------------------------------Graph dump:\n");
        for (ni=_nodes_rt[chain].begin(); ni!=_nodes_rt[chain].end(); ni++) {
                boost::shared_ptr<Route> rp = boost::dynamic_pointer_cast<Route>( *ni);
                DEBUG_TRACE (DEBUG::Graph, string_compose ("GraphNode: %1  refcount: %2\n", rp->name().c_str(), (*ni)->_init_refcount[chain]));
                for (ai=(*ni)->_activation_set[chain].begin(); ai!=(*ni)->_activation_set[chain].end(); ai++) {
                        DEBUG_TRACE (DEBUG::Graph, string_compose ("  triggers: %1\n", boost::dynamic_pointer_cast<Route>(*ai)->name().c_str()));
                }
        }

        DEBUG_TRACE (DEBUG::Graph, "------------- trigger list:\n");
        for (ni=_init_trigger_list[chain].begin(); ni!=_init_trigger_list[chain].end(); ni++) {
                DEBUG_TRACE (DEBUG::Graph, string_compose ("GraphNode: %1  refcount: %2\n", boost::dynamic_pointer_cast<Route>(*ni)->name().c_str(), (*ni)->_init_refcount[chain]));
        }

        DEBUG_TRACE (DEBUG::Graph, string_compose ("final activation refcount: %1\n", _init_finished_refcount[chain]));
#endif
}

int
Graph::silent_process_routes (pframes_t nframes, framepos_t start_frame, framepos_t end_frame,
                              bool can_record, bool rec_monitors_input, bool& need_butler)
{
        _process_nframes = nframes;
        _process_start_frame = start_frame;
        _process_end_frame = end_frame;
        _process_can_record = can_record;
        _process_rec_monitors_input = rec_monitors_input;

        _process_silent = true;
        _process_noroll = false;
        _process_retval = 0;
        _process_need_butler = false;

        if (!_graph_empty) {
		DEBUG_TRACE(DEBUG::Graph, "wake graph for silent process\n");
                _callback_start_sem.signal ();
                _callback_done_sem.wait ();
        }

        need_butler = _process_need_butler;

        return _process_retval;
}

int
Graph::process_routes (pframes_t nframes, framepos_t start_frame, framepos_t end_frame, int declick,
                       bool can_record, bool rec_monitors_input, bool& need_butler)
{
	DEBUG_TRACE (DEBUG::Graph, string_compose ("graph execution from %1 to %2 = %3\n", start_frame, end_frame, nframes));

        _process_nframes = nframes;
        _process_start_frame = start_frame;
        _process_end_frame = end_frame;
        _process_can_record = can_record;
        _process_rec_monitors_input = rec_monitors_input;
        _process_declick = declick;

        _process_silent = false;
        _process_noroll = false;
        _process_retval = 0;
        _process_need_butler = false;

	DEBUG_TRACE(DEBUG::Graph, "wake graph for non-silent process\n");
        _callback_start_sem.signal ();
	_callback_done_sem.wait ();

	DEBUG_TRACE (DEBUG::Graph, "graph execution complete\n");

        need_butler = _process_need_butler;

        return _process_retval;
}

int
Graph::routes_no_roll (pframes_t nframes, framepos_t start_frame, framepos_t end_frame, 
                       bool non_rt_pending, bool can_record, int declick)
{
	DEBUG_TRACE (DEBUG::Graph, string_compose ("no-roll graph execution from %1 to %2 = %3\n", start_frame, end_frame, nframes));

        _process_nframes = nframes;
        _process_start_frame = start_frame;
        _process_end_frame = end_frame;
        _process_can_record = can_record;
        _process_declick = declick;
        _process_non_rt_pending = non_rt_pending;

        _process_silent = false;
        _process_noroll = true;
        _process_retval = 0;
        _process_need_butler = false;

	DEBUG_TRACE(DEBUG::Graph, "wake graph for no-roll process\n");
        _callback_start_sem.signal ();
        _callback_done_sem.wait ();

        return _process_retval;
}
void
Graph::process_one_route (Route* route)
{
        bool need_butler = false;
        int retval;

        assert (route);

        DEBUG_TRACE (DEBUG::ProcessThreads, string_compose ("%1 runs route %2\n", pthread_self(), route->name()));

        if (_process_silent) {
                retval = route->silent_roll (_process_nframes, _process_start_frame, _process_end_frame, _process_can_record, _process_rec_monitors_input, need_butler);
        } else if (_process_noroll) {
                route->set_pending_declick (_process_declick);
                retval = route->no_roll (_process_nframes, _process_start_frame, _process_end_frame, _process_non_rt_pending, _process_can_record, _process_declick);
        } else {
                route->set_pending_declick (_process_declick);
                retval = route->roll (_process_nframes, _process_start_frame, _process_end_frame, _process_declick, _process_can_record, _process_rec_monitors_input, need_butler);
        }

        if (retval) {
                _process_retval = retval;
        }
    
        if (need_butler) {
                _process_need_butler = true;
        }
}

bool
Graph::in_process_thread () const
{
	list<pthread_t>::const_iterator i = _thread_list.begin ();
	while (i != _thread_list.end() && *i != pthread_self ()) {
		++i;
	}

	return i != _thread_list.end ();
}
