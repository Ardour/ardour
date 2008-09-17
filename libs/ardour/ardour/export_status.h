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

#include <list>
#include <stdint.h>

#include <sigc++/signal.h>

namespace ARDOUR
{

enum ExportStage {
	export_None,
	export_ReadTimespan,
	export_PostProcess,
	export_Write
};

struct ExportStatus {

	ExportStatus ();
	void init ();
	
	/* Status info */
	
	volatile bool           stop;
	volatile bool           running;
	
	sigc::signal<void>      Aborting;
	void abort () { _aborted = true; Aborting(); }
	bool aborted () const { return _aborted; }
	
	/* Progress info */
	
	volatile ExportStage    stage;
	volatile float          progress;
	
	volatile uint32_t       total_timespans;
	volatile uint32_t       timespan;
	
	volatile uint32_t       total_channel_configs;
	volatile uint32_t       channel_config;
	
	volatile uint32_t       total_formats;
	volatile uint32_t       format;
	
  private:
	volatile bool          _aborted;
};

} // namespace ARDOUR

#endif /* __ardour_export_status_h__ */
