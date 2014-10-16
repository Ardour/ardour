/*
    Copyright (C) 2010 Paul Davis

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

#ifndef __ardour_progress_h__
#define __ardour_progress_h__

#include <list>

#include "ardour/libardour_visibility.h"

namespace ARDOUR {

/** A class to handle reporting of progress of something */
class LIBARDOUR_API Progress
{
public:
	Progress ();
	virtual ~Progress () {}
	void set_progress (float);

	void ascend ();
	void descend (float);

	bool cancelled () const;

protected:
	void cancel ();

private:
	/** Report overall progress.
	 *  @param p Current progress (from 0 to 1)
	 */
	virtual void set_overall_progress (float p) = 0;

	struct Level {
		Level (float a) : allocation (a), normalised (0) {}

		float allocation;
		float normalised;
	};

	std::list<Level> _stack;
	bool _cancelled;
};

}

#endif
