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

#include <iostream>
#include <glibmm/main.h>
#include "progress_reporter.h"

ProgressReporter::ProgressReporter ()
{

}

ProgressReporter::~ProgressReporter ()
{

}

void
ProgressReporter::set_overall_progress (float p)
{
	update_progress_gui (p);

	/* Make sure the progress widget gets updated */
	while (Glib::MainContext::get_default()->iteration (false)) {
		/* do nothing */
	}
}

