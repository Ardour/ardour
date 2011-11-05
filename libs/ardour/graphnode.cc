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

#include "ardour/graph.h"
#include "ardour/graphnode.h"
#include "ardour/route.h"

using namespace ARDOUR;

GraphNode::GraphNode (boost::shared_ptr<Graph> graph)
        : _graph(graph)
{
}

GraphNode::~GraphNode()
{
}

void
GraphNode::prep (int chain)
{
	/* This is the number of nodes that directly feed us */
        _refcount = _init_refcount[chain];
}

/** Called by another node to tell us that one of the nodes that feed us
 *  has been processed.
 */
void
GraphNode::dec_ref()
{
        if (g_atomic_int_dec_and_test (&_refcount)) {
		/* All the nodes that feed us are done, so we can queue this node
		   for processing.
		*/
                _graph->trigger (this);
	}
}

void
GraphNode::finish (int chain)
{
        node_set_t::iterator i;
        bool feeds_somebody = false;

	/* Tell the nodes that we feed that we've finished */
        for (i=_activation_set[chain].begin(); i!=_activation_set[chain].end(); i++) {
                (*i)->dec_ref();
                feeds_somebody = true;
        }

        if (!feeds_somebody) {
		/* This node does not feed anybody, so decrement the graph's finished count */
                _graph->dec_ref();
        }
}


void
GraphNode::process()
{
        _graph->process_one_route (dynamic_cast<Route *>(this));
}
