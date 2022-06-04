/*
 * Copyright (C) 2017-2018 Robin Gareus <robin@gareus.org>
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

#include "ardour/graph.h"
#include "ardour/rt_tasklist.h"

using namespace ARDOUR;

RTTaskList::RTTaskList (boost::shared_ptr<Graph> process_graph)
	: _graph (process_graph)
{
	_tasks.reserve (256);
}

void
RTTaskList::push_back (boost::function<void ()> fn)
{
	_tasks.push_back (RTTask (_graph.get(), fn));
}

void
RTTaskList::process ()
{
	if (_graph->n_threads () > 1 && _tasks.size () > 2) {
		_graph->process_tasklist (*this);
	} else {
		for (auto const& fn : _tasks) {
			fn._f ();
		}
	}
	_tasks.clear ();
}
