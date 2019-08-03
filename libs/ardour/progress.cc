/*
 * Copyright (C) 2010 Carl Hetherington <carl@carlh.net>
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

#include <cassert>
#include <iostream>
#include "ardour/progress.h"

using namespace std;

ARDOUR::Progress::Progress ()
	: _cancelled (false)
{
	descend (1);
}

/** Descend down one level in terms of progress reporting; e.g. if
 *  there is a task which is split up into N subtasks, each of which
 *  report their progress from 0 to 100%, call descend() before executing
 *  each subtask, and ascend() afterwards to ensure that overall progress
 *  is reported correctly.
 *
 *  @param p Percentage (from 0 to 1) of the current task to allocate to the subtask.
 */
void
ARDOUR::Progress::descend (float a)
{
	_stack.push_back (Level (a));
}

void
ARDOUR::Progress::ascend ()
{
	assert (!_stack.empty ());
	float const a = _stack.back().allocation;
	_stack.pop_back ();
	_stack.back().normalised += a;
}

/** Set the progress of the current task.
 *  @param p Progress (from 0 to 1)
 */
void
ARDOUR::Progress::set_progress (float p)
{
	assert (!_stack.empty ());

	_stack.back().normalised = p;

	float overall = 0;
	float factor = 1;
	for (list<Level>::iterator i = _stack.begin(); i != _stack.end(); ++i) {
		factor *= i->allocation;
		overall += i->normalised * factor;
	}

	set_overall_progress (overall);
}

void
ARDOUR::Progress::cancel ()
{
	_cancelled = true;
}

bool
ARDOUR::Progress::cancelled () const
{
	return _cancelled;
}
