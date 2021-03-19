/*
 * Copyright (C) 2010-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2011 Carl Hetherington <carl@carlh.net>
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

#ifndef __ardour_graphnode_h__
#define __ardour_graphnode_h__

#include <list>
#include <set>
#include <vector>

#include <boost/shared_ptr.hpp>

#include "pbd/g_atomic_compat.h"

namespace ARDOUR
{
class Graph;
class GraphNode;

typedef boost::shared_ptr<GraphNode> node_ptr_t;
typedef std::set<node_ptr_t>         node_set_t;
typedef std::list<node_ptr_t>        node_list_t;

class LIBARDOUR_API GraphActivision
{
protected:
	friend class Graph;
	/** Nodes that we directly feed */
	node_set_t _activation_set[2];
	/** The number of nodes that we directly feed us (one count for each chain) */
	gint _init_refcount[2];
};

/** A node on our processing graph, ie a Route */
class LIBARDOUR_API GraphNode : public GraphActivision
{
public:
	GraphNode (boost::shared_ptr<Graph> Graph);
	virtual ~GraphNode ();

	void prep (int chain);
	void trigger ();

	void
	run (int chain)
	{
		process ();
		finish (chain);
	}

private:
	void finish (int chain);
	void process ();

	boost::shared_ptr<Graph> _graph;
	GATOMIC_QUAL gint        _refcount;
};
}

#endif
