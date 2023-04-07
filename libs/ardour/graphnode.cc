/*
 * Copyright (C) 2010-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2017-2022 Robin Gareus <robin@gareus.org>
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

#include "pbd/atomic.h"

#include "ardour/graphnode.h"
#include "ardour/graph.h"
#include "ardour/route.h"

using namespace ARDOUR;

GraphActivision::GraphActivision ()
	: _activation_set (new ActivationMap)
	, _init_refcount (new RefCntMap)
{
}

node_set_t const&
GraphActivision::activation_set (GraphChain const* const g) const
{
	std::shared_ptr<ActivationMap const> m (_activation_set.reader ());
	return m->at (g);
}

int
GraphActivision::init_refcount (GraphChain const* const g) const
{
	std::shared_ptr<RefCntMap const> m (_init_refcount.reader ());
	return m->at (g);
}

/* ****************************************************************************/

GraphNode::GraphNode (std::shared_ptr<Graph> graph)
	: _graph (graph)
{
	_refcount.store (0);
}

void
GraphNode::prep (GraphChain const* chain)
{
	/* This is the number of nodes that directly feed us */
	_refcount.store (init_refcount (chain));
}

void
GraphNode::run (GraphChain const* chain)
{
	process ();
	finish (chain);
}

/** Called by an upstream node, when it has completed processing */
void
GraphNode::trigger ()
{
	/* check if we can run */
	if (PBD::atomic_dec_and_test (_refcount)) {
#if 0 // TODO optimize: remove prep()
		/* reset reference count for next cycle */
		_refcount.store (_init_refcount[chain]);
#endif
		/* All nodes that feed this node have completed, so this node be processed now. */
		_graph->trigger (this);
	}
}

void
GraphNode::finish (GraphChain const* chain)
{
	node_set_t::iterator i;
	bool                 feeds = false;

	/* Notify downstream nodes that depend on this node */
	for (auto const& i : activation_set (chain)) {
		i->trigger ();
		feeds = true;
	}

	if (!feeds) {
		/* This node is a terminal node that does not feed another note,
		 * so notify the graph to decrement the the finished count */
		_graph->reached_terminal_node ();
	}
}
