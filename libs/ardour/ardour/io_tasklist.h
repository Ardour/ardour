/*
 * Copyright (C) 2024 Robin Gareus <robin@gareus.org>
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

#ifndef _ardour_io_tasklist_h_
#define _ardour_io_tasklist_h_

#include <boost/function.hpp>
#include <vector>

#include "ardour/libardour_visibility.h"

namespace ARDOUR
{

class LIBARDOUR_API IOTaskList
{
public:
	IOTaskList ();
	~IOTaskList ();

	/** process tasks in list in parallel, wait for them to complete */
	void process ();
	void push_back (boost::function<void ()> fn);

private:
	std::vector<boost::function<void ()>> _tasks;

	size_t _n_threads;
};

} // namespace ARDOUR
#endif
