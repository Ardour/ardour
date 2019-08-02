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

#ifndef __ardour_progress_reporter_h__
#define __ardour_progress_reporter_h__

#include "ardour/progress.h"

/** A parent class for classes which can report progress on something */
class ProgressReporter : public ARDOUR::Progress
{
public:
	ProgressReporter ();
	virtual ~ProgressReporter ();

private:
	void set_overall_progress (float);

	/** Update our GUI to reflect progress.
	 *  @param p Progress, from 0 to 1.
	 */
	virtual void update_progress_gui (float p) = 0;
};

#endif
