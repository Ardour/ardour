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
#include "pbd/pthread_utils.h"

#include "ardour/debug.h"
#include "ardour/graph.h"
#include "ardour/types.h"
#include "ardour/session.h"
#include "ardour/route.h"
#include "ardour/process_thread.h"
#include "ardour/audioengine.h"

#include "pbd/i18n.h"

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
	, _threads_active (false)
	, _execution_sem ("graph_execution", 0)
	, _callback_start_sem ("graph_start", 0)
	, _callback_done_sem ("graph_done", 0)
{
	pthread_mutex_init( &_trigger_mutex, NULL);

	/* XXX: rather hacky `fix' to stop _trigger_queue.push_back() allocating
	 * memory in the RT thread.
	 */
	_trigger_queue.reserve (8192);

	_execution_tokens = 0;

	_current_chain = 0;
	_pending_chain = 0;
	_setup_chain   = 1;
	_graph_empty = true;


	ARDOUR::AudioEngine::instance()->Running.connect_same_thread (engine_connections, boost::bind (&Graph::reset_thread_list, this));
	ARDOUR::AudioEngine::instance()->Stopped.connect_same_thread (engine_connections, boost::bind (&Graph::engine_stopped, this));
	ARDOUR::AudioEngine::instance()->Halted.connect_same_thread (engine_connections, boost::bind (&Graph::engine_stopped, this));

	reset_thread_list ();

#ifdef DEBUG_RT_ALLOC
	graph = this;
	pbd_alloc_allowed = &::alloc_allowed;
#endif
}

void
Graph::engine_stopped ()
{
#ifndef NDEBUG
	cerr << "Graph::engine_stopped. n_thread: " << AudioEngine::instance()->process_thread_count() << endl;
#endif
	if (AudioEngine::instance()->process_thread_count() != 0) {
		drop_threads ();
	}
}

/** Set up threads for running the graph */
void
Graph::reset_thread_list ()
{
	uint32_t num_threads = how_many_dsp_threads ();

	/* For now, we shouldn't be using the graph code if we only have 1 DSP thread */
	assert (num_threads > 1);

	/* don't bother doing anything here if we already have the right
	 * number of threads.
	 */

	if (AudioEngine::instance()->process_thread_count() == num_threads) {
		return;
	}

	Glib::Threads::Mutex::Lock lm (_session.engine().process_lock());

	if (AudioEngine::instance()->process_thread_count() != 0) {
		drop_threads ();
	}

	_threads_active = true;

	if (AudioEngine::instance()->create_process_thread (boost::bind (&Graph::main_thread, this)) != 0) {
		throw failed_constructor ();
	}

	for (uint32_t i = 1; i < num_threads; ++i) {
		if (AudioEngine::instance()->create_process_thread (boost::bind (&Graph::helper_thread, this))) {
			throw failed_constructor ();
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
	Glib::Threads::Mutex::Lock ls (_swap_mutex);
	_threads_active = false;

	uint32_t thread_count = AudioEngine::instance()->process_thread_count ();

	for (unsigned int i=0; i < thread_count; i++) {
		pthread_mutex_lock (&_trigger_mutex);
		_execution_sem.signal ();
		pthread_mutex_unlock (&_trigger_mutex);
	}

	pthread_mutex_lock (&_trigger_mutex);
	_callback_start_sem.signal ();
	pthread_mutex_unlock (&_trigger_mutex);

	AudioEngine::instance()->join_process_threads ();

	/* signal main process thread if it's waiting for an already terminated thread */
	_callback_done_sem.signal ();
	_execution_tokens = 0;

	/* reset semaphores.
	 * This is somewhat ugly, yet if a thread is killed (e.g jackd terminates
	 * abnormally), some semaphores are still unlocked.
	 */
#ifndef NDEBUG
	int d1 = _execution_sem.reset ();
	int d2 = _callback_start_sem.reset ();
	int d3 = _callback_done_sem.reset ();
	cerr << "Graph::drop_threads() sema-counts: " << d1 << ", " << d2<< ", " << d3 << endl;
#else
	_execution_sem.reset ();
	_callback_start_sem.reset ();
	_callback_done_sem.reset ();
#endif
}

void
Graph::clear_other_chain ()
{
	Glib::Threads::Mutex::Lock ls (_swap_mutex);

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
		 * to wait till this is no longer true.
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

	/* Trigger the initial nodes for processing, which are the ones at the `input' end */
	pthread_mutex_lock (&_trigger_mutex);
	for (i=_init_trigger_list[chain].begin(); i!=_init_trigger_list[chain].end(); i++) {
		/* don't use ::trigger here, as we have already locked the mutex */
		_trigger_queue.push_back (i->get ());
	}
	pthread_mutex_unlock (&_trigger_mutex);
}

void
Graph::trigger (GraphNode* n)
{
	pthread_mutex_lock (&_trigger_mutex);
	_trigger_queue.push_back (n);
	pthread_mutex_unlock (&_trigger_mutex);
}

/** Called when a node at the `output' end of the chain (ie one that has no-one to feed)
 *  is finished.
 */
void
Graph::dec_ref()
{
	if (g_atomic_int_dec_and_test (const_cast<gint*> (&_finished_refcount))) {

		/* We have run all the nodes that are at the `output' end of
		 * the graph, so there is nothing more to do this time around.
		 */

		restart_cycle ();
	}
}

void
Graph::restart_cycle()
{
	// we are through. wakeup our caller.
	DEBUG_TRACE(DEBUG::ProcessThreads, string_compose ("%1 cycle done.\n", pthread_name()));

again:
	_callback_done_sem.signal ();

	/* Block until the a process callback triggers us */
	_callback_start_sem.wait();

	if (!_threads_active) {
		return;
	}

	DEBUG_TRACE(DEBUG::ProcessThreads, string_compose ("%1 prepare new cycle.\n", pthread_name()));
	prep ();

	if (_graph_empty && _threads_active) {
		goto again;
	}

	// returning will restart the cycle.
	// starting with waking up the others.
}

/** Rechain our stuff using a list of routes (which can be in any order) and
 *  a directed graph of their interconnections, which is guaranteed to be
 *  acyclic.
 */
void
Graph::rechain (boost::shared_ptr<RouteList> routelist, GraphEdges const & edges)
{
	Glib::Threads::Mutex::Lock ls (_swap_mutex);

	int chain = _setup_chain;
	DEBUG_TRACE (DEBUG::Graph, string_compose ("============== setup %1\n", chain));

	/* This will become the number of nodes that do not feed any other node;
	 * once we have processed this number of those nodes, we have finished.
	 */
	_init_finished_refcount[chain] = 0;

	/* This will become a list of nodes that are not fed by another node, ie
	 * those at the `input' end.
	 */
	_init_trigger_list[chain].clear();

	_nodes_rt[chain].clear();

	/* Clear things out, and make _nodes_rt[chain] a copy of routelist */
	for (RouteList::iterator ri=routelist->begin(); ri!=routelist->end(); ri++) {
		(*ri)->_init_refcount[chain] = 0;
		(*ri)->_activation_set[chain].clear();
		_nodes_rt[chain].push_back (*ri);
	}

	// now add refs for the connections.

	for (node_list_t::iterator ni = _nodes_rt[chain].begin(); ni != _nodes_rt[chain].end(); ni++) {

		boost::shared_ptr<Route> r = boost::dynamic_pointer_cast<Route> (*ni);

		/* The routes that are directly fed by r */
		set<GraphVertex> fed_from_r = edges.from (r);

		/* Hence whether r has an output */
		bool const has_output = !fed_from_r.empty ();

		/* Set up r's activation set */
		for (set<GraphVertex>::iterator i = fed_from_r.begin(); i != fed_from_r.end(); ++i) {
			r->_activation_set[chain].insert (*i);
		}

		/* r has an input if there are some incoming edges to r in the graph */
		bool const has_input = !edges.has_none_to (r);

		/* Increment the refcount of any route that we directly feed */
		for (node_set_t::iterator ai = r->_activation_set[chain].begin(); ai != r->_activation_set[chain].end(); ai++) {
			(*ai)->_init_refcount[chain] += 1;
		}

		if (!has_input) {
			/* no input, so this node needs to be triggered initially to get things going */
			_init_trigger_list[chain].push_back (*ni);
		}

		if (!has_output) {
			/* no output, so this is one of the nodes that we can count off to decide
			 * if we've finished
			 */
			_init_finished_refcount[chain] += 1;
		}
	}

	_pending_chain = chain;
	dump(chain);
}

/** Called by both the main thread and all helpers.
 *  @return true to quit, false to carry on.
 */
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

	/* the number of threads that are asleep */
	int et = _execution_tokens;
	/* the number of nodes that need to be run */
	int ts = _trigger_queue.size();

	/* hence how many threads to wake up */
	int wakeup = min (et, ts);
	/* update the number of threads that will still be sleeping */
	_execution_tokens -= wakeup;

	DEBUG_TRACE(DEBUG::ProcessThreads, string_compose ("%1 signals %2\n", pthread_name(), wakeup));

	for (int i = 0; i < wakeup; i++) {
		_execution_sem.signal ();
	}

	while (to_run == 0) {
		_execution_tokens += 1;
		pthread_mutex_unlock (&_trigger_mutex);
		DEBUG_TRACE (DEBUG::ProcessThreads, string_compose ("%1 goes to sleep\n", pthread_name()));
		_execution_sem.wait ();
		if (!_threads_active) {
			return true;
		}
		DEBUG_TRACE (DEBUG::ProcessThreads, string_compose ("%1 is awake\n", pthread_name()));
		pthread_mutex_lock (&_trigger_mutex);
		if (_trigger_queue.size()) {
			to_run = _trigger_queue.back();
			_trigger_queue.pop_back();
		}
	}
	pthread_mutex_unlock (&_trigger_mutex);

	to_run->process();
	to_run->finish (_current_chain);

	DEBUG_TRACE(DEBUG::ProcessThreads, string_compose ("%1 has finished run_one()\n", pthread_name()));

	return !_threads_active;
}

void
Graph::helper_thread()
{
	suspend_rt_malloc_checks ();
	ProcessThread* pt = new ProcessThread ();
	resume_rt_malloc_checks ();

	pt->get_buffers();

	while(1) {
		if (run_one()) {
			break;
		}
	}

	pt->drop_buffers();
	delete pt;
}

/** Here's the main graph thread */
void
Graph::main_thread()
{
	suspend_rt_malloc_checks ();
	ProcessThread* pt = new ProcessThread ();
	resume_rt_malloc_checks ();

	pt->get_buffers();

again:
	_callback_start_sem.wait ();

	DEBUG_TRACE(DEBUG::ProcessThreads, "main thread is awake\n");

	if (!_threads_active) {
		pt->drop_buffers();
		delete (pt);
		return;
	}

	prep ();

	if (_graph_empty && _threads_active) {
		_callback_done_sem.signal ();
		DEBUG_TRACE(DEBUG::ProcessThreads, "main thread sees graph done, goes back to sleep\n");
		goto again;
	}

	/* This loop will run forever */
	while (1) {
		DEBUG_TRACE(DEBUG::ProcessThreads, string_compose ("main thread (%1) runs one graph node\n", pthread_name ()));
		if (run_one()) {
			break;
		}
	}

	pt->drop_buffers();
	delete (pt);
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
Graph::process_routes (pframes_t nframes, samplepos_t start_sample, samplepos_t end_sample, bool& need_butler)
{
	DEBUG_TRACE (DEBUG::ProcessThreads, string_compose ("graph execution from %1 to %2 = %3\n", start_sample, end_sample, nframes));

	if (!_threads_active) return 0;

	_process_nframes = nframes;
	_process_start_sample = start_sample;
	_process_end_sample = end_sample;

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
Graph::routes_no_roll (pframes_t nframes, samplepos_t start_sample, samplepos_t end_sample, bool non_rt_pending)
{
	DEBUG_TRACE (DEBUG::ProcessThreads, string_compose ("no-roll graph execution from %1 to %2 = %3\n", start_sample, end_sample, nframes));

	if (!_threads_active) return 0;

	_process_nframes = nframes;
	_process_start_sample = start_sample;
	_process_end_sample = end_sample;
	_process_non_rt_pending = non_rt_pending;

	_process_noroll = true;
	_process_retval = 0;
	_process_need_butler = false;

	DEBUG_TRACE(DEBUG::ProcessThreads, "wake graph for no-roll process\n");
	_callback_start_sem.signal ();
	_callback_done_sem.wait ();
	DEBUG_TRACE (DEBUG::ProcessThreads, "graph execution complete\n");

	return _process_retval;
}
void
Graph::process_one_route (Route* route)
{
	bool need_butler = false;
	int retval;

	assert (route);

	DEBUG_TRACE (DEBUG::ProcessThreads, string_compose ("%1 runs route %2\n", pthread_name(), route->name()));

	if (_process_noroll) {
		retval = route->no_roll (_process_nframes, _process_start_sample, _process_end_sample, _process_non_rt_pending);
	} else {
		retval = route->roll (_process_nframes, _process_start_sample, _process_end_sample, need_butler);
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
	return AudioEngine::instance()->in_process_thread ();
}
