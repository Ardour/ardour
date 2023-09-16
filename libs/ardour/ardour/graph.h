/*
 * Copyright (C) 2008-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2010-2012 Carl Hetherington <carl@carlh.net>
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

#ifndef __ardour_graph_h__
#define __ardour_graph_h__

#include <atomic>
#include <list>
#include <memory>
#include <set>
#include <string>
#include <vector>


#include "pbd/mpmc_queue.h"
#include "pbd/semutils.h"

#include "ardour/audio_backend.h"
#include "ardour/libardour_visibility.h"
#include "ardour/session_handle.h"
#include "ardour/types.h"

namespace ARDOUR
{
class ProcessNode;
class GraphNode;
class Graph;

class IOPlug;
class Route;
class RTTaskList;
class Session;
class GraphEdges;

typedef std::shared_ptr<GraphNode> node_ptr_t;

typedef std::list<node_ptr_t> node_list_t;
typedef std::set<node_ptr_t>  node_set_t;

struct GraphChain {
	GraphChain (GraphNodeList const&, GraphEdges const&);
	~GraphChain ();
	void dump () const;
	bool plot (std::string const&) const;

	node_list_t _nodes_rt;
	/** Nodes that are not fed by any other nodes */
	node_list_t _init_trigger_list;
	/** The number of nodes that do not feed any other node */
	int _n_terminal_nodes;
};

class LIBARDOUR_API Graph : public SessionHandleRef
{
public:
	Graph (Session& session);

	/* public API for use by session-process */
	int process_routes (std::shared_ptr<GraphChain> chain, pframes_t nframes, samplepos_t start_sample, samplepos_t end_sample, bool& need_butler);
	int routes_no_roll (std::shared_ptr<GraphChain> chain, pframes_t nframes, samplepos_t start_sample, samplepos_t end_sample, bool non_rt_pending);
	int silence_routes (std::shared_ptr<GraphChain> chain, pframes_t nframes);
	int process_io_plugs (std::shared_ptr<GraphChain> chain, pframes_t nframes, samplepos_t start_sample);

	bool     in_process_thread () const;
	uint32_t n_threads () const;

	/* called by GraphNode */
	void trigger (ProcessNode* n);
	void reached_terminal_node ();

	/* called by virtual GraphNode::process() */
	void process_one_route (Route* route);
	void process_one_ioplug (IOPlug*);

	/* RTTasks */
	void process_tasklist (RTTaskList const&);

protected:
	virtual void session_going_away ();

private:
	void reset_thread_list ();
	void drop_threads ();
	void run_one ();
	void main_thread ();
	void prep ();

	void helper_thread ();

	PBD::MPMCQueue<ProcessNode*> _trigger_queue;      ///< nodes that can be processed
	std::atomic<uint32_t>        _trigger_queue_size; ///< number of entries in trigger-queue

	/** Start worker threads */
	PBD::Semaphore _execution_sem;

	/** The number of processing threads that are asleep */
	std::atomic<uint32_t> _idle_thread_cnt;

	/** Signalled to start a run of the graph for a process callback */
	PBD::Semaphore _callback_start_sem;
	PBD::Semaphore _callback_done_sem;

	/** The number of unprocessed nodes that do not feed any other node; updated during processing */
	std::atomic<uint32_t> _terminal_refcnt;

	bool _graph_empty;

	/* number of background worker threads >= 0 */
	std::atomic<uint32_t> _n_workers;

	/* flag to terminate background threads */
	std::atomic<int> _terminate;

	/* graph chain */
	GraphChain const* _graph_chain;

	/* parameter caches */
	pframes_t   _process_nframes;
	samplepos_t _process_start_sample;
	samplepos_t _process_end_sample;
	bool        _process_non_rt_pending;

	enum ProcessMode {
		Roll, NoRoll, Silence
	} _process_mode;

	int  _process_retval;
	bool _process_need_butler;

	/* engine / thread connection */
	PBD::ScopedConnectionList engine_connections;
	void                      engine_stopped ();
};

} // namespace ARDOUR

#endif /* __ardour_graph_h__ */
