/*
 * Copyright (C) 2008-2012 Sakari Bergen <sakari.bergen@beatwaves.net>
 * Copyright (C) 2008-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
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

#include "ardour/export_status.h"

namespace ARDOUR
{

ExportStatus::ExportStatus ()
{
	init();
}

void
ExportStatus::init ()
{
	Glib::Threads::Mutex::Lock l (_run_lock);
	stop = false;
	_running = false;
	_aborted = false;
	_errors = false;

	active_job = Exporting;

	total_timespans = 0;
	timespan = 0;

	total_samples = 0;
	processed_samples = 0;

	total_samples_current_timespan = 0;
	processed_samples_current_timespan = 0;

	total_postprocessing_cycles = 0;
	current_postprocessing_cycle = 0;
	result_map.clear();
}

void
ExportStatus::abort (bool error_occurred)
{
	Glib::Threads::Mutex::Lock l (_run_lock);
	_aborted = true;
	_errors = _errors || error_occurred;
	_running = false;
}

void
ExportStatus::finish (TransportRequestSource trs)
{
	Glib::Threads::Mutex::Lock l (_run_lock);
	set_running (false);
	Finished (trs); /* EMIT SIGNAL */
}

} // namespace ARDOUR
