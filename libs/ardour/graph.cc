/*
 * Copyright (C) 2010-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2010-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2013 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2013 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2015-2019 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <cmath>
#include <stdio.h>

#include <sys/syscall.h>
#include <sys/types.h>

#include "pbd/compose.h"
#include "pbd/debug_rt_alloc.h"
#include "pbd/pthread_utils.h"

#include "temporal/superclock.h"
#include "temporal/tempo.h"

#include "ardour/audioengine.h"
#include "ardour/debug.h"
#include "ardour/graph.h"
#include "ardour/process_thread.h"
#include "ardour/route.h"
#include "ardour/session.h"
#include "ardour/types.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;

#ifdef DEBUG_RT_ALLOC
static Graph* graph = 0;

extern "C" {

int
alloc_allowed ()
{
	return !graph->in_process_thread ();
}
}
#endif

#define g_atomic_uint_get(x) static_cast<guint> (g_atomic_int_get (x))

Graph::Graph (Session& session)
	: SessionHandleRef (session)
	, _execution_sem ("graph_execution", 0)
	, _callback_start_sem ("graph_start", 0)
	, _callback_done_sem ("graph_done", 0)
	, _graph_empty (true)
	, _current_chain (0)
	, _pending_chain (0)
	, _setup_chain (1)
{
	g_atomic_int_set (&_terminal_refcnt, 0);
	g_atomic_int_set (&_terminate, 0);
	g_atomic_int_set (&_n_workers, 0);
	g_atomic_int_set (&_idle_thread_cnt, 0);
	g_atomic_int_set (&_trigger_queue_size, 0);

	_n_terminal_nodes[0] = 0;
	_n_terminal_nodes[1] = 0;

	/* pre-allocate memory */
	_trigger_queue.reserve (1024);

	ARDOUR::AudioEngine::instance ()->Running.connect_same_thread (engine_connections, boost::bind (&Graph::reset_thread_list, this));
	ARDOUR::AudioEngine::instance ()->Stopped.connect_same_thread (engine_connections, boost::bind (&Graph::engine_stopped, this));
	ARDOUR::AudioEngine::instance ()->Halted.connect_same_thread (engine_connections, boost::bind (&Graph::engine_stopped, this));

	reset_thread_list ();

#ifdef DEBUG_RT_ALLOC
	graph             = this;
	pbd_alloc_allowed = &::alloc_allowed;
#endif
}

void
Graph::engine_stopped ()
{
#ifndef NDEBUG
	cerr << "Graph::engine_stopped. n_thread: " << AudioEngine::instance ()->process_thread_count () << endl;
#endif
	if (AudioEngine::instance ()->process_thread_count () != 0) {
		drop_threads ();
	}
}

/** Set up threads for running the graph */
void
Graph::reset_thread_list ()
{
	uint32_t num_threads = how_many_dsp_threads ();
	guint    n_workers   = g_atomic_uint_get (&_n_workers);

	/* For now, we shouldn't be using the graph code if we only have 1 DSP thread */
	assert (num_threads > 1);

	/* don't bother doing anything here if we already have the right
	 * number of threads.
	 */

	if (AudioEngine::instance ()->process_thread_count () == num_threads) {
		return;
	}

	Glib::Threads::Mutex::Lock lm (_session.engine ().process_lock ());

	if (n_workers > 0) {
		drop_threads ();
	}

	/* Allow threads to run */
	g_atomic_int_set (&_terminate, 0);

	if (AudioEngine::instance ()->create_process_thread (boost::bind (&Graph::main_thread, this)) != 0) {
		throw failed_constructor ();
	}

	for (uint32_t i = 1; i < num_threads; ++i) {
		if (AudioEngine::instance ()->create_process_thread (boost::bind (&Graph::helper_thread, this))) {
			throw failed_constructor ();
		}
	}

	while (g_atomic_uint_get (&_n_workers) + 1 != num_threads) {
		sched_yield ();
	}
}

void
Graph::session_going_away ()
{
	drop_threads ();

	// now drop all references on the nodes.
	_nodes_rt[0].clear ();
	_nodes_rt[1].clear ();
	_init_trigger_list[0].clear ();
	_init_trigger_list[1].clear ();
	g_atomic_int_set (&_trigger_queue_size, 0);
	_trigger_queue.clear ();
}

void
Graph::drop_threads ()
{
	Glib::Threads::Mutex::Lock ls (_swap_mutex);

	/* Flag threads to terminate */
	g_atomic_int_set (&_terminate, 1);

	/* Wake-up sleeping threads */
	guint tc = g_atomic_uint_get (&_idle_thread_cnt);
	assert (tc == g_atomic_uint_get (&_n_workers));
	for (guint i = 0; i < tc; ++i) {
		_execution_sem.signal ();
	}

	/* and the main thread */
	_callback_start_sem.signal ();

	/* join process threads */
	AudioEngine::instance ()->join_process_threads ();

	g_atomic_int_set (&_n_workers, 0);
	g_atomic_int_set (&_idle_thread_cnt, 0);

	/* signal main process thread if it's waiting for an already terminated thread */
	_callback_done_sem.signal ();

	/* reset semaphores.
	 * This is somewhat ugly, yet if a thread is killed (e.g jackd terminates
	 * abnormally), some semaphores are still unlocked.
	 */
#ifndef NDEBUG
	int d1 = _execution_sem.reset ();
	int d2 = _callback_start_sem.reset ();
	int d3 = _callback_done_sem.reset ();
	cerr << "Graph::drop_threads() sema-counts: " << d1 << ", " << d2 << ", " << d3 << endl;
#else
	_execution_sem.reset ();
	_callback_start_sem.reset ();
	_callback_done_sem.reset ();
#endif
}

/* special case route removal -- called from Session::remove_routes */
void
Graph::clear_other_chain ()
{
	Glib::Threads::Mutex::Lock ls (_swap_mutex);

	while (1) {
		if (_setup_chain != _pending_chain) {
			for (node_list_t::iterator ni = _nodes_rt[_setup_chain].begin (); ni != _nodes_rt[_setup_chain].end (); ++ni) {
				(*ni)->_activation_set[_setup_chain].clear ();
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
Graph::prep ()
{
	if (_swap_mutex.trylock ()) {
		/* swap mutex acquired */
		if (_current_chain != _pending_chain) {
			/* use new chain */
			_setup_chain   = _current_chain;
			_current_chain = _pending_chain;
			/* ensure that all nodes can be queued */
			_trigger_queue.reserve (_nodes_rt[_current_chain].size ());
			assert (g_atomic_uint_get (&_trigger_queue_size) == 0);
			_cleanup_cond.signal ();
		}
		_swap_mutex.unlock ();
	}

	_graph_empty = true;

	int chain = _current_chain;

	node_list_t::iterator i;
	for (i = _nodes_rt[chain].begin (); i != _nodes_rt[chain].end (); ++i) {
		(*i)->prep (chain);
		_graph_empty = false;
	}

	assert (_graph_empty != (_n_terminal_nodes[chain] > 0));

	g_atomic_int_set (&_terminal_refcnt, _n_terminal_nodes[chain]);

	/* Trigger the initial nodes for processing, which are the ones at the `input' end */
	for (i = _init_trigger_list[chain].begin (); i != _init_trigger_list[chain].end (); i++) {
		g_atomic_int_inc (&_trigger_queue_size);
		_trigger_queue.push_back (i->get ());
	}
}

void
Graph::trigger (GraphNode* n)
{
	g_atomic_int_inc (&_trigger_queue_size);
	_trigger_queue.push_back (n);
}

/** Called when a node at the `output' end of the chain (ie one that has no-one to feed)
 *  is finished.
 */
void
Graph::reached_terminal_node ()
{
	if (g_atomic_int_dec_and_test (&_terminal_refcnt)) {
	again:

		/* We have run all the nodes that are at the `output' end of
		 * the graph, so there is nothing more to do this time around.
		 */
		assert (g_atomic_uint_get (&_trigger_queue_size) == 0);

		/* Notify caller */
		DEBUG_TRACE (DEBUG::ProcessThreads, string_compose ("%1 cycle done.\n", pthread_name ()));

		_callback_done_sem.signal ();

		/* Ensure that all background threads are idle.
		 * When freewheeling there may be an immediate restart:
		 * If there are more threads than CPU cores, some worker-
		 * threads may only be "on the way" to become idle.
		 */
		guint n_workers = g_atomic_uint_get (&_n_workers);
		while (g_atomic_uint_get (&_idle_thread_cnt) != n_workers) {
			sched_yield ();
		}

		/* Block until the a process callback */
		_callback_start_sem.wait ();

		if (g_atomic_int_get (&_terminate)) {
			return;
		}

		DEBUG_TRACE (DEBUG::ProcessThreads, string_compose ("%1 prepare new cycle.\n", pthread_name ()));

		/* Prepare next cycle:
		 *  - Reset terminal reference count
		 *  - queue initial nodes
		 */
		prep ();

		if (_graph_empty && !g_atomic_int_get (&_terminate)) {
			goto again;
		}
		/* .. continue in worker-thread */
	}
}

/** Rechain our stuff using a list of routes (which can be in any order) and
 *  a directed graph of their interconnections, which is guaranteed to be
 *  acyclic.
 */
void
Graph::rechain (boost::shared_ptr<RouteList> routelist, GraphEdges const& edges)
{
	Glib::Threads::Mutex::Lock ls (_swap_mutex);

	int chain = _setup_chain;
	DEBUG_TRACE (DEBUG::Graph, string_compose ("============== setup %1\n", chain));

	/* This will become the number of nodes that do not feed any other node;
	 * once we have processed this number of those nodes, we have finished.
	 */
	_n_terminal_nodes[chain] = 0;

	/* This will become a list of nodes that are not fed by another node, ie
	 * those at the `input' end.
	 */
	_init_trigger_list[chain].clear ();

	_nodes_rt[chain].clear ();

	/* Clear things out, and make _nodes_rt[chain] a copy of routelist */
	for (RouteList::iterator ri = routelist->begin (); ri != routelist->end (); ri++) {
		(*ri)->_init_refcount[chain] = 0;
		(*ri)->_activation_set[chain].clear ();
		_nodes_rt[chain].push_back (*ri);
	}

	// now add refs for the connections.

	for (node_list_t::iterator ni = _nodes_rt[chain].begin (); ni != _nodes_rt[chain].end (); ni++) {
		boost::shared_ptr<Route> r = boost::dynamic_pointer_cast<Route> (*ni);

		/* The routes that are directly fed by r */
		set<GraphVertex> fed_from_r = edges.from (r);

		/* Hence whether r has an output */
		bool const has_output = !fed_from_r.empty ();

		/* Set up r's activation set */
		for (set<GraphVertex>::iterator i = fed_from_r.begin (); i != fed_from_r.end (); ++i) {
			r->_activation_set[chain].insert (*i);
		}

		/* r has an input if there are some incoming edges to r in the graph */
		bool const has_input = !edges.has_none_to (r);

		/* Increment the refcount of any route that we directly feed */
		for (node_set_t::iterator ai = r->_activation_set[chain].begin (); ai != r->_activation_set[chain].end (); ai++) {
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
			_n_terminal_nodes[chain] += 1;
		}
	}

	_pending_chain = chain;
	dump (chain);
}

/** Called by both the main thread and all helpers. */
void
Graph::run_one ()
{
	GraphNode* to_run = NULL;

	if (g_atomic_int_get (&_terminate)) {
		return;
	}

	if (_trigger_queue.pop_front (to_run)) {
		/* Wake up idle threads, but at most as many as there's
		 * work in the trigger queue that can be processed by
		 * other threads.
		 * This thread as not yet decreased _trigger_queue_size.
		 */
		guint idle_cnt   = g_atomic_uint_get (&_idle_thread_cnt);
		guint work_avail = g_atomic_uint_get (&_trigger_queue_size);
		guint wakeup     = std::min (idle_cnt + 1, work_avail);

		DEBUG_TRACE (DEBUG::ProcessThreads, string_compose ("%1 signals %2 threads\n", pthread_name (), wakeup));
		for (guint i = 1; i < wakeup; ++i) {
			_execution_sem.signal ();
		}
	}

	while (!to_run) {
		/* Wait for work, fall asleep */
		g_atomic_int_inc (&_idle_thread_cnt);
		assert (g_atomic_uint_get (&_idle_thread_cnt) <= g_atomic_uint_get (&_n_workers));

		DEBUG_TRACE (DEBUG::ProcessThreads, string_compose ("%1 goes to sleep\n", pthread_name ()));
		_execution_sem.wait ();

		if (g_atomic_int_get (&_terminate)) {
			return;
		}

		DEBUG_TRACE (DEBUG::ProcessThreads, string_compose ("%1 is awake\n", pthread_name ()));

		g_atomic_int_dec_and_test (&_idle_thread_cnt);

		/* Try to find some work to do */
		_trigger_queue.pop_front (to_run);
	}

	/* Process the graph-node */
	g_atomic_int_dec_and_test (&_trigger_queue_size);
	to_run->run (_current_chain);

	DEBUG_TRACE (DEBUG::ProcessThreads, string_compose ("%1 has finished run_one()\n", pthread_name ()));
}

void
Graph::helper_thread ()
{
	g_atomic_int_inc (&_n_workers);
	guint id = g_atomic_uint_get (&_n_workers);

	/* This is needed for ARDOUR::Session requests called from rt-processors
	 * in particular Lua scripts may do cross-thread calls */
	if (!SessionEvent::has_per_thread_pool ()) {
		char name[64];
		snprintf (name, 64, "RT-%u-%p", id, (void*)DEBUG_THREAD_SELF);
		pthread_set_name (name);
		SessionEvent::create_per_thread_pool (name, 64);
		PBD::notify_event_loops_about_thread_creation (pthread_self (), name, 64);
	}

	suspend_rt_malloc_checks ();
	ProcessThread* pt = new ProcessThread ();
	resume_rt_malloc_checks ();

	pt->get_buffers ();

	while (!g_atomic_int_get (&_terminate)) {
		setup_thread_local_variables ();
		run_one ();
	}

	pt->drop_buffers ();
	delete pt;
}

/** Here's the main graph thread */
void
Graph::main_thread ()
{
	/* first time setup */

	suspend_rt_malloc_checks ();
	ProcessThread* pt = new ProcessThread ();

	/* This is needed for ARDOUR::Session requests called from rt-processors
	 * in particular Lua scripts may do cross-thread calls */
	if (!SessionEvent::has_per_thread_pool ()) {
		char name[64];
		snprintf (name, 64, "RT-main-%p", (void*)DEBUG_THREAD_SELF);
		pthread_set_name (name);
		SessionEvent::create_per_thread_pool (name, 64);
		PBD::notify_event_loops_about_thread_creation (pthread_self (), name, 64);
	}
	resume_rt_malloc_checks ();

	pt->get_buffers ();

	/* Wait for initial process callback */
again:
	_callback_start_sem.wait ();

	DEBUG_TRACE (DEBUG::ProcessThreads, "main thread is awake\n");

	if (g_atomic_int_get (&_terminate)) {
		pt->drop_buffers ();
		delete (pt);
		return;
	}

	/* Bootstrap the trigger-list
	 * (later this is done by Graph_reached_terminal_node) */
	prep ();

	if (_graph_empty && !g_atomic_int_get (&_terminate)) {
		_callback_done_sem.signal ();
		DEBUG_TRACE (DEBUG::ProcessThreads, "main thread sees graph done, goes back to sleep\n");
		goto again;
	}

	/* After setup, the main-thread just becomes a normal worker */
	while (!g_atomic_int_get (&_terminate)) {
		setup_thread_local_variables ();
		run_one ();
	}

	pt->drop_buffers ();
	delete (pt);
}

void
Graph::setup_thread_local_variables ()
{
	Temporal::set_thread_sample_rate (AudioEngine::instance()->sample_rate());
	Temporal::TempoMap::fetch ();
}

void
Graph::dump (int chain) const
{
#ifndef NDEBUG
	node_list_t::const_iterator ni;
	node_set_t::const_iterator  ai;

	chain = _pending_chain;

	DEBUG_TRACE (DEBUG::Graph, "--------------------------------------------Graph dump:\n");
	for (ni = _nodes_rt[chain].begin (); ni != _nodes_rt[chain].end (); ni++) {
		boost::shared_ptr<Route> rp = boost::dynamic_pointer_cast<Route> (*ni);
		DEBUG_TRACE (DEBUG::Graph, string_compose ("GraphNode: %1  refcount: %2\n", rp->name ().c_str (), (*ni)->_init_refcount[chain]));
		for (ai = (*ni)->_activation_set[chain].begin (); ai != (*ni)->_activation_set[chain].end (); ai++) {
			DEBUG_TRACE (DEBUG::Graph, string_compose ("  triggers: %1\n", boost::dynamic_pointer_cast<Route> (*ai)->name ().c_str ()));
		}
	}

	DEBUG_TRACE (DEBUG::Graph, "------------- trigger list:\n");
	for (ni = _init_trigger_list[chain].begin (); ni != _init_trigger_list[chain].end (); ni++) {
		DEBUG_TRACE (DEBUG::Graph, string_compose ("GraphNode: %1  refcount: %2\n", boost::dynamic_pointer_cast<Route> (*ni)->name ().c_str (), (*ni)->_init_refcount[chain]));
	}

	DEBUG_TRACE (DEBUG::Graph, string_compose ("final activation refcount: %1\n", _n_terminal_nodes[chain]));
#endif
}

bool
Graph::plot (std::string const& file_name) const
{
	Glib::Threads::Mutex::Lock ls (_swap_mutex);
	int chain = _current_chain;

	node_list_t::const_iterator ni;
	node_set_t::const_iterator  ai;
	stringstream ss;

	ss << "digraph {\n";
	ss << "  node [shape = ellipse];\n";

	for (ni = _nodes_rt[chain].begin (); ni != _nodes_rt[chain].end (); ni++) {
		boost::shared_ptr<Route> sr = boost::dynamic_pointer_cast<Route> (*ni);
		std::string sn = string_compose ("%1 (%2)", sr->name (), (*ni)->_init_refcount[chain]);
		if ((*ni)->_init_refcount[chain] == 0 && (*ni)->_activation_set[chain].size() == 0) {
				ss << "  \"" << sn << "\"[style=filled,fillcolor=gold1];\n";
		} else if ((*ni)->_init_refcount[chain] == 0) {
				ss << "  \"" << sn << "\"[style=filled,fillcolor=lightskyblue1];\n";
		} else if ((*ni)->_activation_set[chain].size() == 0) {
				ss << "  \"" << sn << "\"[style=filled,fillcolor=aquamarine2];\n";
		}
		for (ai = (*ni)->_activation_set[chain].begin (); ai != (*ni)->_activation_set[chain].end (); ai++) {
			boost::shared_ptr<Route> dr = boost::dynamic_pointer_cast<Route> (*ai);
			std::string dn = string_compose ("%1 (%2)", dr->name (), (*ai)->_init_refcount[chain]);
			bool sends_only = false;
			sr->direct_feeds_according_to_reality (dr, &sends_only);
			if (sends_only) {
				ss << "  edge [style=dashed];\n";
			}
			ss << "  \"" << sn << "\" -> \"" << dn << "\"\n";
			if (sends_only) {
				ss << "  edge [style=solid];\n";
			}
		}
	}
	ss << "}\n";

	GError *err = NULL;
	if (!g_file_set_contents (file_name.c_str(), ss.str().c_str(), -1, &err)) {
		if (err) {
			error << string_compose (_("Could not graph to file (%1)"), err->message) << endmsg;
			g_error_free (err);
		}
		return false;
	}
	return true;
}

int
Graph::process_routes (pframes_t nframes, samplepos_t start_sample, samplepos_t end_sample, bool& need_butler)
{
	DEBUG_TRACE (DEBUG::ProcessThreads, string_compose ("graph execution from %1 to %2 = %3\n", start_sample, end_sample, nframes));

	if (g_atomic_int_get (&_terminate)) {
		return 0;
	}

	_process_nframes      = nframes;
	_process_start_sample = start_sample;
	_process_end_sample   = end_sample;

	_process_noroll      = false;
	_process_retval      = 0;
	_process_need_butler = false;

	DEBUG_TRACE (DEBUG::ProcessThreads, "wake graph for non-silent process\n");
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

	if (g_atomic_int_get (&_terminate)) {
		return 0;
	}

	_process_nframes        = nframes;
	_process_start_sample   = start_sample;
	_process_end_sample     = end_sample;
	_process_non_rt_pending = non_rt_pending;

	_process_noroll      = true;
	_process_retval      = 0;
	_process_need_butler = false;

	DEBUG_TRACE (DEBUG::ProcessThreads, "wake graph for no-roll process\n");
	_callback_start_sem.signal ();
	_callback_done_sem.wait ();
	DEBUG_TRACE (DEBUG::ProcessThreads, "graph execution complete\n");

	return _process_retval;
}
void
Graph::process_one_route (Route* route)
{
	bool need_butler = false;
	int  retval;

	assert (route);

	DEBUG_TRACE (DEBUG::ProcessThreads, string_compose ("%1 runs route %2\n", pthread_name (), route->name ()));

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
	return AudioEngine::instance ()->in_process_thread ();
}
