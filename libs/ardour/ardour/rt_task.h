/*
 * Copyright (C) 2017,2022 Robin Gareus <robin@gareus.org>
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

#ifndef _ardour_rt_task_h_
#define _ardour_rt_task_h_

#include <boost/function.hpp>

#include "ardour/graphnode.h"

namespace ARDOUR
{
class Graph;
class RTTaskList;

class LIBARDOUR_API RTTask : public ProcessNode
{
public:
	RTTask (Graph* g, boost::function<void ()> const& fn);

	void prep (GraphChain const*) {}
	void run (GraphChain const*);

private:
	friend class RTTaskList;
	boost::function<void ()> _f;
	Graph*                   _graph;
};

}

#endif
