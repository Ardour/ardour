/*
    Copyright (C) 2008 Paul Davis
    Author: Sakari Bergen

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

#ifndef __ardour_export_status_h__
#define __ardour_export_status_h__

#include <stdint.h>

#include "ardour/libardour_visibility.h"
#include "ardour/export_analysis.h"
#include "ardour/types.h"

#include "pbd/signals.h"

namespace ARDOUR
{

class LIBARDOUR_API ExportStatus {
  public:
	ExportStatus ();
	void init ();

	/* Status info */

	volatile bool           stop;

	void abort (bool error_occurred = false);
	bool aborted () const { return _aborted; }
	bool errors () const { return _errors; }
	bool running () const { return _running; }

	void set_running (bool r) {
		assert (!_run_lock.trylock()); // must hold lock
		_running = r;
	}
	Glib::Threads::Mutex& lock () { return _run_lock; }

	PBD::Signal0<void>      Finished;
	void finish ();

	void cleanup ();

	/* Progress info */

	volatile enum Progress {
		Exporting,
		Normalizing,
		Encoding,
		Tagging,
		Uploading,
		Command }
	                        active_job;

	volatile uint32_t       total_timespans;
	volatile uint32_t       timespan;
	std::string             timespan_name;

	volatile framecnt_t     total_frames;
	volatile framecnt_t     processed_frames;

	volatile framecnt_t     total_frames_current_timespan;
	volatile framecnt_t     processed_frames_current_timespan;

	volatile uint32_t       total_postprocessing_cycles;
	volatile uint32_t       current_postprocessing_cycle;

	AnalysisResults         result_map;

  private:
	volatile bool          _aborted;
	volatile bool          _errors;
	volatile bool          _running;

	Glib::Threads::Mutex   _run_lock;
};

} // namespace ARDOUR

#endif /* __ardour_export_status_h__ */
