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

#include "pbd/compose.h"
#include "pbd/debug_rt_alloc.h"
#include "pbd/pthread_utils.h"

#include "temporal/superclock.h"
#include "temporal/tempo.h"

#include "ardour/audioengine.h"
#include "ardour/debug.h"
#include "ardour/graph.h"
#include "ardour/io_plug.h"
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
	, _graph_chain (0)
{
	g_atomic_int_set (&_terminal_refcnt, 0);
	g_atomic_int_set (&_terminate, 0);
	g_atomic_int_set (&_n_workers, 0);
	g_atomic_int_set (&_idle_thread_cnt, 0);
	g_atomic_int_set (&_trigger_queue_size, 0);

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

uint32_t
Graph::n_threads () const
{
	return 1 + g_atomic_uint_get (&_n_workers);
}

void
Graph::session_going_away ()
{
	drop_threads ();

	/* now drop all references on the nodes. */
	g_atomic_int_set (&_trigger_queue_size, 0);
	_trigger_queue.clear ();
	_graph_chain = 0;
}

void
Graph::drop_threads ()
{
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

void
Graph::prep ()
{
	assert (_graph_chain);
	_graph_empty = true;

	node_list_t::iterator i;
	for (auto const& i : _graph_chain->_nodes_rt) {
		i->prep (_graph_chain);
		_graph_empty = false;
	}

	assert (g_atomic_uint_get (&_trigger_queue_size) == 0);
	assert (_graph_empty != (_graph_chain->_n_terminal_nodes > 0));

	if (_trigger_queue.capacity () < _graph_chain->_nodes_rt.size ()) {
		_trigger_queue.reserve (_graph_chain->_nodes_rt.size ());
	}

	g_atomic_int_set (&_terminal_refcnt, _graph_chain->_n_terminal_nodes);

	/* Trigger the initial nodes for processing, which are the ones at the `input' end */
	for (auto const& i : _graph_chain->_init_trigger_list) {
		g_atomic_int_inc (&_trigger_queue_size);
		_trigger_queue.push_back (i.get ());
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
		prep (); // XXX

		if (_graph_empty && !g_atomic_int_get (&_terminate)) {
			goto again;
		}
		/* .. continue in worker-thread */
	}
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

		/* update the thread-local tempo map ptr */

		Temporal::TempoMap::fetch ();

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
	to_run->run (_graph_chain);

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

	/* just in case we need the thread local tempo map ptr before anything else */
	Temporal::TempoMap::fetch ();

	while (!g_atomic_int_get (&_terminate)) {
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
	Temporal::TempoMap::fetch ();

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
		run_one ();
	}

	pt->drop_buffers ();
	delete (pt);
}

int
Graph::process_routes (boost::shared_ptr<GraphChain> chain, pframes_t nframes, samplepos_t start_sample, samplepos_t end_sample, bool& need_butler)
{
	DEBUG_TRACE (DEBUG::ProcessThreads, string_compose ("graph execution from %1 to %2 = %3\n", start_sample, end_sample, nframes));

	if (g_atomic_int_get (&_terminate)) {
		return 0;
	}

	_graph_chain          = chain.get ();
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
Graph::routes_no_roll (boost::shared_ptr<GraphChain> chain, pframes_t nframes, samplepos_t start_sample, samplepos_t end_sample, bool non_rt_pending)
{
	DEBUG_TRACE (DEBUG::ProcessThreads, string_compose ("no-roll graph execution from %1 to %2 = %3\n", start_sample, end_sample, nframes));

	if (g_atomic_int_get (&_terminate)) {
		return 0;
	}

	_graph_chain            = chain.get ();
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

int
Graph::process_io_plugs (boost::shared_ptr<GraphChain> chain, pframes_t nframes, samplepos_t start_sample)
{
	DEBUG_TRACE (DEBUG::ProcessThreads, string_compose ("IOPlug graph execution at %1 for %2\n", start_sample, nframes));

	if (g_atomic_int_get (&_terminate)) {
		return 0;
	}

	_graph_chain          = chain.get ();
	_process_nframes      = nframes;
	_process_start_sample = start_sample;

	DEBUG_TRACE (DEBUG::ProcessThreads, "wake graph for IOPlug processing\n");
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
		_process_need_butler = true; // -> atomic
	}
}

void
Graph::process_one_ioplug (IOPlug* ioplug)
{
	ioplug->run (_process_start_sample, _process_nframes);
}

bool
Graph::in_process_thread () const
{
	return AudioEngine::instance ()->in_process_thread ();
}

/* ****************************************************************************/

GraphChain::GraphChain (GraphNodeList const& nodelist, GraphEdges const& edges)
{
	DEBUG_TRACE (DEBUG::Graph, string_compose ("GraphChain constructed in thread:%1\n", pthread_name ()));
	/* This will become the number of nodes that do not feed any other node;
	 * once we have processed this number of those nodes, we have finished.
	 */
	_n_terminal_nodes = 0;

	/* This will become a list of nodes that are not fed by another node, ie
	 * those at the `input' end.
	 */
	_init_trigger_list.clear ();

	_nodes_rt.clear ();

	/* copy nodelist to _nodes_rt, prepare GraphNodes for this graph */
	for (auto const& ri : nodelist) {
		RCUWriter<GraphActivision::ActivationMap>         wa (ri->_activation_set);
		RCUWriter<GraphActivision::RefCntMap>             wr (ri->_init_refcount);
		boost::shared_ptr<GraphActivision::ActivationMap> ma (wa.get_copy ());
		boost::shared_ptr<GraphActivision::RefCntMap>     mr (wr.get_copy ());
		(*mr)[this] = 0;
		(*ma)[this].clear ();
		_nodes_rt.push_back (ri);
	}

	/* now add refs for the connections. */
	for (auto const& ni : _nodes_rt) {
		/* The nodes that are directly fed by ni */
		set<GraphVertex> fed_from_r = edges.from (ni);

		/* Hence whether ni has an output, or is otherwise a terminal node */
		bool const has_output = !fed_from_r.empty ();

		/* Set up ni's activation set */
		if (has_output) {
			boost::shared_ptr<GraphActivision::ActivationMap> m (ni->_activation_set.reader ());
			for (auto const& i : fed_from_r) {
				auto it = (*m)[this].insert (i);
				assert (it.second);

				/* Increment the refcount of any node that we directly feed */
				boost::shared_ptr<GraphActivision::RefCntMap> a ((*it.first)->_init_refcount.reader ());
				(*a)[this] += 1;
			}
		}

		/* ni has an input if there are some incoming edges to r in the graph */
		bool const has_input = !edges.has_none_to (ni);

		if (!has_input) {
			/* no input, so this node needs to be triggered initially to get things going */
			_init_trigger_list.push_back (ni);
		}

		if (!has_output) {
			/* no output, so this is one of the nodes that we can count off to decide
			 * if we've finished
			 */
			_n_terminal_nodes += 1;
		}
	}
	dump ();
}

GraphChain::~GraphChain ()
{
	/* clear chain */
	DEBUG_TRACE (DEBUG::Graph, string_compose ("~GraphChain destroyed in thread:%1\n", pthread_name ()));
	for (auto const& ni : _nodes_rt) {
		RCUWriter<GraphActivision::ActivationMap>         wa (ni->_activation_set);
		RCUWriter<GraphActivision::RefCntMap>             wr (ni->_init_refcount);
		boost::shared_ptr<GraphActivision::ActivationMap> ma (wa.get_copy ());
		boost::shared_ptr<GraphActivision::RefCntMap>     mr (wr.get_copy ());
		mr->erase (this);
		ma->erase (this);
	}
}

bool
GraphChain::plot (std::string const& file_name) const
{
	node_list_t::const_iterator ni;
	node_set_t::const_iterator  ai;
	stringstream                ss;

	ss << "digraph {\n";
	ss << "  node [shape = ellipse];\n";

	for (auto const& ni : _nodes_rt) {
		std::string sn = string_compose ("%1 (%2)", ni->graph_node_name (), ni->init_refcount (this));
		if (ni->init_refcount (this) == 0 && ni->activation_set (this).size () == 0) {
			ss << "  \"" << sn << "\"[style=filled,fillcolor=gold1];\n";
		} else if (ni->init_refcount (this) == 0) {
			ss << "  \"" << sn << "\"[style=filled,fillcolor=lightskyblue1];\n";
		} else if (ni->activation_set (this).size () == 0) {
			ss << "  \"" << sn << "\"[style=filled,fillcolor=aquamarine2];\n";
		}
		for (auto const& ai : ni->activation_set (this)) {
			std::string dn         = string_compose ("%1 (%2)", ai->graph_node_name (), ai->init_refcount (this));
			bool        sends_only = false;
			ni->direct_feeds_according_to_reality (ai, &sends_only);
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

	GError* err = NULL;
	if (!g_file_set_contents (file_name.c_str (), ss.str ().c_str (), -1, &err)) {
		if (err) {
			error << string_compose (_("Could not graph to file (%1)"), err->message) << endmsg;
			g_error_free (err);
		}
		return false;
	}
	return true;
}

void
GraphChain::dump () const
{
#ifndef NDEBUG
	DEBUG_TRACE (DEBUG::Graph, "--8<-- Graph dump ----------------------------\n");
	for (auto const& ni : _nodes_rt) {
		DEBUG_TRACE (DEBUG::Graph, string_compose ("GraphNode: %1  refcount: %2\n", ni->graph_node_name (), ni->init_refcount (this)));
		for (auto const& ai : ni->activation_set (this)) {
			DEBUG_TRACE (DEBUG::Graph, string_compose ("  triggers: %1\n", ai->graph_node_name ()));
		}
	}

	DEBUG_TRACE (DEBUG::Graph, " --- trigger list ---\n");
	for (auto const& ni : _init_trigger_list) {
		DEBUG_TRACE (DEBUG::Graph, string_compose ("GraphNode: %1  refcount: %2\n", ni->graph_node_name (), ni->init_refcount (this)));
	}

	DEBUG_TRACE (DEBUG::Graph, string_compose ("final activation refcount: %1\n", _n_terminal_nodes));
	DEBUG_TRACE (DEBUG::Graph, "-->8-- END Graph dump ------------------------\n");
#endif
}
