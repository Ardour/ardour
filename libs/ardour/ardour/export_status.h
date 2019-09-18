/*
 * Copyright (C) 2008-2012 Sakari Bergen <sakari.bergen@beatwaves.net>
 * Copyright (C) 2008-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2008 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2009-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2016-2019 Robin Gareus <robin@gareus.org>
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

	PBD::Signal1<void,TransportRequestSource> Finished;
	void finish (TransportRequestSource);

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

	volatile samplecnt_t    total_samples;
	volatile samplecnt_t    processed_samples;

	volatile samplecnt_t    total_samples_current_timespan;
	volatile samplecnt_t    processed_samples_current_timespan;

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
