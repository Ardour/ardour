/*
    Copyright (C) 2000 Paul Davis

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


#ifndef __ardour_graphnode_h__
#define __ardour_graphnode_h__

#include <list>
#include <set>
#include <vector>

#include <boost/shared_ptr.hpp>

namespace ARDOUR
{

class Graph;
class GraphNode;

typedef boost::shared_ptr<GraphNode> node_ptr_t;
typedef std::set< node_ptr_t > node_set_t;
typedef std::list< node_ptr_t > node_list_t;

/** A node on our processing graph, ie a Route */	
class LIBARDOUR_API GraphNode
{
    public:
	GraphNode( boost::shared_ptr<Graph> Graph );
	virtual ~GraphNode();

	void prep( int chain );
	void dec_ref();
	void finish( int chain );

	virtual void process();

    private:
	friend class Graph;

	/** Nodes that we directly feed */
	node_set_t  _activation_set[2];

	boost::shared_ptr<Graph> _graph;

	gint _refcount;
	/** The number of nodes that we directly feed us (one count for each chain) */
	gint _init_refcount[2];
};

}

#endif
