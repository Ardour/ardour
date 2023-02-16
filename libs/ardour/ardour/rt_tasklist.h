/*
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
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

#ifndef _ardour_rt_tasklist_h_
#define _ardour_rt_tasklist_h_

#include <boost/function.hpp>
#include <vector>

#include "ardour/libardour_visibility.h"
#include "ardour/rt_task.h"

namespace ARDOUR
{
class Graph;

class LIBARDOUR_API RTTaskList
{
public:
	RTTaskList (std::shared_ptr<Graph>);

	/** process tasks in list in parallel, wait for them to complete */
	void process ();
	void push_back (boost::function<void ()> fn);

	std::vector<RTTask> const& tasks () const { return _tasks; }

private:
	std::vector<RTTask>      _tasks;
	std::shared_ptr<Graph> _graph;
};

} // namespace ARDOUR
#endif
