/*
 * Copyright (C) 2010-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2015-2022 Robin Gareus <robin@gareus.org>
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
#include <map>
#include <set>

#include <boost/shared_ptr.hpp>

#include "pbd/g_atomic_compat.h"
#include "pbd/rcu.h"

#include "ardour/libardour_visibility.h"

namespace ARDOUR
{
class Graph;
class GraphNode;
struct GraphChain;

typedef boost::shared_ptr<GraphNode> node_ptr_t;
typedef std::set<node_ptr_t>         node_set_t;
typedef std::list<node_ptr_t>        node_list_t;

class LIBARDOUR_API GraphActivision
{
public:
	GraphActivision ();
	virtual ~GraphActivision () {}

	typedef std::map<GraphChain const*, node_set_t> ActivationMap;
	typedef std::map<GraphChain const*, int>        RefCntMap;

	node_set_t const& activation_set (GraphChain const* const g) const;
	int               init_refcount (GraphChain const* const g) const;

protected:
	friend struct GraphChain;

	/** Nodes that we directly feed */
	SerializedRCUManager<ActivationMap> _activation_set;
	/** The number of nodes that we directly feed us (one count for each chain) */
	SerializedRCUManager<RefCntMap> _init_refcount;
};

/** A node on our processing graph, ie a Route */
class LIBARDOUR_API GraphNode : public GraphActivision
{
public:
	GraphNode (boost::shared_ptr<Graph> Graph);

	/* API used by Graph */
	void prep (GraphChain const*);
	void trigger ();
	void run (GraphChain const* chain);

	/* API used to sort Nodes and create GraphChain */
	virtual std::string graph_node_name () const = 0;

	virtual bool direct_feeds_according_to_reality (boost::shared_ptr<GraphNode>, bool* via_send_only = 0) = 0;

protected:
	virtual void process () = 0;

	boost::shared_ptr<Graph> _graph;

private:
	void finish (GraphChain const*);

	GATOMIC_QUAL gint _refcount;
};

} // namespace ARDOUR

#endif
