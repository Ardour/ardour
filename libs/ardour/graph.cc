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
        , _quit_threads (false)
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

        reset_thread_list ();

        Config->ParameterChanged.connect_same_thread (processor_usage_connection, boost::bind (&Graph::parameter_changed, this, _1));

#ifdef DEBUG_RT_ALLOC
	graph = this;
	pbd_alloc_allowed = &::alloc_allowed;
#endif
}

void
Graph::parameter_changed (std::string param)
{
        if (param == X_("processor-usage")) {
                reset_thread_list ();
        }
}

void
Graph::reset_thread_list ()
{
        uint32_t num_threads = how_many_dsp_threads ();

        /* don't bother doing anything here if we already have the right
           number of threads.
        */

        if (_thread_list.size() == num_threads) {
                return;
        }

        Glib::Mutex::Lock lm (_session.engine().process_lock());
	pthread_t a_thread;

        if (!_thread_list.empty()) {
                drop_threads ();
        }

#if 0
        /* XXX this only makes sense when we can use just the AudioEngine thread
           and still keep the graph current with the route list
        */
        if (num_threads <= 1) {
                /* no point creating 1 thread - the AudioEngine already gives us one
                 */
                return;
        }
#endif
	if (AudioEngine::instance()->create_process_thread (boost::bind (&Graph::main_thread, this), &a_thread, 100000) == 0) {
		_thread_list.push_back (a_thread);
	}

        for (uint32_t i = 1; i < num_threads; ++i) {
		if (AudioEngine::instance()->create_process_thread (boost::bind (&Graph::helper_thread, this), &a_thread, 100000) == 0) {
			_thread_list.push_back (a_thread);
		}
        }
}

void
Graph::session_going_away()
{
        drop_threads ();

        // now drop all references on the nodes.
        _nodes_rt[0].clear();
        _nodes_rt[1].clear();
        _init_trigger_list[0].clear();
        _init_trigger_list[1].clear();
        _trigger_queue.clear();
}

void
Graph::drop_threads ()
{
        _quit_threads = true;

        for (unsigned int i=0; i< _thread_list.size(); i++) {
		_execution_sem.signal ();
        }

        _callback_start_sem.signal ();

        for (list<pthread_t>::iterator i = _thread_list.begin(); i != _thread_list.end(); ++i) {
                void* status;
                pthread_join (*i, &status);
        }

        _thread_list.clear ();

	_execution_tokens = 0;

        _quit_threads = false;
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
        _trigger_queue.push_back (n);
        pthread_mutex_unlock (&_trigger_mutex);
}

void
Graph::dec_ref()
{
        if (g_atomic_int_dec_and_test (&_finished_refcount)) {

		/* We have run all the nodes that are at the `output' end of
		   the graph, so there is nothing more to do this time around.
		*/

                this->restart_cycle();
        }
}

void
Graph::restart_cycle()
{
        // we are through. wakeup our caller.

  again:
        _callback_done_sem.signal ();

        // block until we are triggered.
        _callback_start_sem.wait();

        if (_quit_threads) {
                return;
        }

        this->prep();

        if (_graph_empty) {
                goto again;
        }

        // returning will restart the cycle.
        // starting with waking up the others.
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

	/* This will become the number of nodes that do not feed any other node;
	   once we have processed this number of those nodes, we have finished.
	*/
        _init_finished_refcount[chain] = 0;

	/* This will become a list of nodes that are not fed by another node, ie
	   those at the `input' end.
	*/
        _init_trigger_list[chain].clear();

        _nodes_rt[chain].clear();

	/* Clear things out, and make _nodes_rt[chain] a copy of routelist */
        for (RouteList::iterator ri=routelist->begin(); ri!=routelist->end(); ri++) {
                node_ptr_t n = boost::dynamic_pointer_cast<GraphNode> (*ri);

                n->_init_refcount[chain] = 0;
                n->_activation_set[chain].clear();
                _nodes_rt[chain].push_back(n);
        }

        // now add refs for the connections.

        for (ni=_nodes_rt[chain].begin(); ni!=_nodes_rt[chain].end(); ni++) {

		/* We will set this to true if the node *ni is directly or
		   indirectly fed by anything (without feedback)
		*/
                bool has_input  = false;

		/* We will set this to true if the node *ni directly feeds
		   anything (without feedback)
		*/
                bool has_output = false;

		/* We will also set up *ni's _activation_set to contain any nodes
		   that it directly feeds.
		*/

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

		/* Increment the refcount of any route that we directly feed */
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

        DEBUG_TRACE(DEBUG::ProcessThreads, string_compose ("%1 signals %2\n", pthread_self(), wakeup));

        for (int i = 0; i < wakeup; i++) {
                _execution_sem.signal ();
        }

        while (to_run == 0) {
                _execution_tokens += 1;
                pthread_mutex_unlock (&_trigger_mutex);
                DEBUG_TRACE (DEBUG::ProcessThreads, string_compose ("%1 goes to sleep\n", pthread_self()));
                _execution_sem.wait ();
                if (_quit_threads) {
                        return true;
                }
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

        DEBUG_TRACE(DEBUG::ProcessThreads, string_compose ("%1 has finished run_one()\n", pthread_self()));

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
	suspend_rt_malloc_checks ();
	ProcessThread* pt = new ProcessThread ();
	resume_rt_malloc_checks ();

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
	suspend_rt_malloc_checks ();
	ProcessThread* pt = new ProcessThread ();
	resume_rt_malloc_checks ();

        pt->get_buffers();
        get_rt();

  again:
        _callback_start_sem.wait ();
	DEBUG_TRACE(DEBUG::ProcessThreads, "main thread is awake\n");

        if (_quit_threads) {
                return;
        }

        this->prep();

        if (_graph_empty && !_quit_threads) {
                _callback_done_sem.signal ();
                DEBUG_TRACE(DEBUG::ProcessThreads, "main thread sees graph done, goes back to slee\n");
                goto again;
        }

        while (1) {
		DEBUG_TRACE(DEBUG::ProcessThreads, "main thread runs one graph node\n");
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
Graph::silent_process_routes (pframes_t nframes, framepos_t start_frame, framepos_t end_frame, bool& need_butler)
{
        _process_nframes = nframes;
        _process_start_frame = start_frame;
        _process_end_frame = end_frame;

        _process_silent = true;
        _process_noroll = false;
        _process_retval = 0;
        _process_need_butler = false;

        if (!_graph_empty) {
		DEBUG_TRACE(DEBUG::ProcessThreads, "wake graph for silent process\n");
                _callback_start_sem.signal ();
                _callback_done_sem.wait ();
        }

        need_butler = _process_need_butler;

        return _process_retval;
}

int
Graph::process_routes (pframes_t nframes, framepos_t start_frame, framepos_t end_frame, int declick, bool& need_butler)
{
	DEBUG_TRACE (DEBUG::ProcessThreads, string_compose ("graph execution from %1 to %2 = %3\n", start_frame, end_frame, nframes));

        _process_nframes = nframes;
        _process_start_frame = start_frame;
        _process_end_frame = end_frame;
        _process_declick = declick;

        _process_silent = false;
        _process_noroll = false;
        _process_retval = 0;
        _process_need_butler = false;

	DEBUG_TRACE(DEBUG::ProcessThreads, "wake graph for non-silent process\n");
        _callback_start_sem.signal ();
	_callback_done_sem.wait ();

	DEBUG_TRACE (DEBUG::ProcessThreads, "graph execution complete\n");

        need_butler = _process_need_butler;

        return _process_retval;
}

int
Graph::routes_no_roll (pframes_t nframes, framepos_t start_frame, framepos_t end_frame,
                       bool non_rt_pending, int declick)
{
	DEBUG_TRACE (DEBUG::ProcessThreads, string_compose ("no-roll graph execution from %1 to %2 = %3\n", start_frame, end_frame, nframes));

        _process_nframes = nframes;
        _process_start_frame = start_frame;
        _process_end_frame = end_frame;
        _process_declick = declick;
        _process_non_rt_pending = non_rt_pending;

        _process_silent = false;
        _process_noroll = true;
        _process_retval = 0;
        _process_need_butler = false;

	DEBUG_TRACE(DEBUG::ProcessThreads, "wake graph for no-roll process\n");
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
                retval = route->silent_roll (_process_nframes, _process_start_frame, _process_end_frame, need_butler);
        } else if (_process_noroll) {
                route->set_pending_declick (_process_declick);
                retval = route->no_roll (_process_nframes, _process_start_frame, _process_end_frame, _process_non_rt_pending);
        } else {
                route->set_pending_declick (_process_declick);
                retval = route->roll (_process_nframes, _process_start_frame, _process_end_frame, _process_declick, need_butler);
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
