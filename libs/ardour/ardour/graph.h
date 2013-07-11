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


#ifndef __ardour_graph_h__
#define __ardour_graph_h__

#include <list>
#include <set>
#include <vector>
#include <string>

#include <boost/shared_ptr.hpp>

#include <glib.h>
#include <cassert>

#include <pthread.h>

#include "pbd/semutils.h"

#include "ardour/types.h"
#include "ardour/session_handle.h"

namespace ARDOUR
{

class GraphNode;
class Graph;

class Route;
class Session;
class GraphEdges;	

typedef boost::shared_ptr<GraphNode> node_ptr_t;

typedef std::list< node_ptr_t > node_list_t;
typedef std::set< node_ptr_t > node_set_t;

class Graph : public SessionHandleRef
{
public:
	Graph (Session & session);

	uint32_t threads_in_use () const { return _thread_list.size(); }

	void prep();
	void trigger (GraphNode * n);
	void rechain (boost::shared_ptr<RouteList>, GraphEdges const &);

	void dump (int chain);
	void process();
	void dec_ref();
	void restart_cycle();

	bool run_one();
	void helper_thread();
	void main_thread();

	int silent_process_routes (pframes_t nframes, framepos_t start_frame, framepos_t end_frame,
	                           bool& need_butler);

	int process_routes (pframes_t nframes, framepos_t start_frame, framepos_t end_frame, int declick,
	                    bool& need_butler);

	int routes_no_roll (pframes_t nframes, framepos_t start_frame, framepos_t end_frame,
	                    bool non_rt_pending, int declick);

	void process_one_route (Route * route);

	void clear_other_chain ();

	bool in_process_thread () const;

protected:
	virtual void session_going_away ();

private:
	std::list<jack_native_thread_t> _thread_list;
	volatile bool        _quit_threads;

	void reset_thread_list ();
	void drop_threads ();

	node_list_t _nodes_rt[2];

	node_list_t _init_trigger_list[2];

	std::vector<GraphNode *> _trigger_queue;
	pthread_mutex_t          _trigger_mutex;

	PBD::ProcessSemaphore _execution_sem;

	/** Signalled to start a run of the graph for a process callback */
	PBD::ProcessSemaphore _callback_start_sem;
	PBD::ProcessSemaphore _callback_done_sem;
	PBD::ProcessSemaphore _cleanup_sem;

	/** The number of processing threads that are asleep */
	volatile gint _execution_tokens;
	/** The number of unprocessed nodes that do not feed any other node; updated during processing */
	volatile gint _finished_refcount;
	/** The initial number of nodes that do not feed any other node (for each chain) */
	volatile gint _init_finished_refcount[2];

	bool _graph_empty;

	// chain swapping
	Glib::Threads::Mutex  _swap_mutex;
        Glib::Threads::Cond   _cleanup_cond;
	volatile int _current_chain;
	volatile int _pending_chain;
	volatile int _setup_chain;

	// parameter caches.
	pframes_t  _process_nframes;
	framepos_t _process_start_frame;
	framepos_t _process_end_frame;
	bool	   _process_can_record;
	bool	   _process_non_rt_pending;
	int        _process_declick;

	bool _process_silent;
	bool _process_noroll;
	int  _process_retval;
	bool _process_need_butler;
};

} // namespace

#endif /* __ardour_graph_h__ */
