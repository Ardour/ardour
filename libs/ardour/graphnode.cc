
#include "ardour/graph.h"
#include "ardour/graphnode.h"
#include "ardour/route.h"

using namespace ARDOUR;

// ========================================== GraphNode

GraphNode::GraphNode( graph_ptr_t graph )
  : _graph(graph)
{ }

void
GraphNode::prep( int chain )
{
    _refcount = _init_refcount[chain];
}

void
GraphNode::dec_ref()
{
    if (g_atomic_int_dec_and_test( &_refcount ))
	_graph->trigger( this );
}

void
GraphNode::finish( int chain )
{
    node_set_t::iterator i;
    bool feeds_somebody = false;
    for (i=_activation_set[chain].begin(); i!=_activation_set[chain].end(); i++)
    {
	(*i)->dec_ref();
	feeds_somebody = true;
    }
    if (!feeds_somebody)
    {
	_graph->dec_ref();
    }
}


void
GraphNode::process()
{
    _graph->process_one_route( dynamic_cast<Route *>(this) );
}
