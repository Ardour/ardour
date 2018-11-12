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

#include "pbd/semutils.h"

#include "ardour/libardour_visibility.h"
#include "ardour/types.h"
#include "ardour/audio_backend.h"
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

class LIBARDOUR_API Graph : public SessionHandleRef
{
public:
	Graph (Session & session);

	void trigger (GraphNode * n);
	void rechain (boost::shared_ptr<RouteList>, GraphEdges const &);

	void dump (int chain);
	void dec_ref();

	void helper_thread();

	int process_routes (pframes_t nframes, samplepos_t start_sample, samplepos_t end_sample, bool& need_butler);

	int routes_no_roll (pframes_t nframes, samplepos_t start_sample, samplepos_t end_sample, bool non_rt_pending );

	void process_one_route (Route * route);

	void clear_other_chain ();

	bool in_process_thread () const;

protected:
	virtual void session_going_away ();

private:
	volatile bool        _threads_active;

	void reset_thread_list ();
	void drop_threads ();
	void restart_cycle();
	bool run_one();
	void main_thread();
	void prep();

	node_list_t _nodes_rt[2];

	node_list_t _init_trigger_list[2];

	std::vector<GraphNode *> _trigger_queue;
	pthread_mutex_t          _trigger_mutex;

	PBD::Semaphore _execution_sem;

	/** Signalled to start a run of the graph for a process callback */
	PBD::Semaphore _callback_start_sem;
	PBD::Semaphore _callback_done_sem;

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
	samplepos_t _process_start_sample;
	samplepos_t _process_end_sample;
	bool	   _process_can_record;
	bool	   _process_non_rt_pending;

	bool _process_noroll;
	int  _process_retval;
	bool _process_need_butler;

	// enginer / thread connection
	PBD::ScopedConnectionList engine_connections;
	void engine_stopped ();
};

} // namespace

#endif /* __ardour_graph_h__ */
