/*
    Copyright (C) 1999-2008 Paul Davis 

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

#include <sigc++/bind.h>

#include <pbd/error.h>
#include <glibmm/thread.h>

#include <ardour/export_failed.h>
#include <ardour/export_file_io.h>
#include <ardour/export_utilities.h>
#include <ardour/export_handler.h>
#include <ardour/export_status.h>
#include <ardour/timestamps.h>
#include <ardour/ardour.h>
#include <ardour/session.h>
#include <ardour/audioengine.h>
#include <ardour/audio_diskstream.h>
#include <ardour/panner.h>

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

boost::shared_ptr<ExportHandler>
Session::get_export_handler ()
{
	if (!export_handler) {
		export_handler.reset (new ExportHandler (*this));
	}
	
	return export_handler;
}

boost::shared_ptr<ExportStatus>
Session::get_export_status ()
{
	if (!export_status) {
		export_status.reset (new ExportStatus ());
	}
	
	return export_status;
}

int
Session::pre_export ()
{
	get_export_status (); // Init export_status

	wait_till_butler_finished ();

	/* take everyone out of awrite to avoid disasters */

	{
		boost::shared_ptr<RouteList> r = routes.reader ();

		for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
			(*i)->protect_automation ();
		}
	}

	/* make sure we are actually rolling */

	if (get_record_enabled()) {
		disable_record (false);
	}

	/* no slaving */

	post_export_slave = Config->get_slave_source ();
	post_export_position = _transport_frame;

	Config->set_slave_source (None);
	
	_exporting = true;
	export_status->running = true;
	export_status->Aborting.connect (sigc::hide_return (sigc::mem_fun (*this, &Session::stop_audio_export)));
	export_status->Finished.connect (sigc::hide_return (sigc::mem_fun (*this, &Session::finalize_audio_export)));

	return 0;
}

int
Session::start_audio_export (nframes_t position, bool realtime)
{
	if (!_exporting) {
		pre_export ();
	}

	/* get everyone to the right position */

	{
		boost::shared_ptr<DiskstreamList> dsl = diskstreams.reader();

		for (DiskstreamList::iterator i = dsl->begin(); i != dsl->end(); ++i) {
			if ((*i)-> seek (position, true)) {
				error << string_compose (_("%1: cannot seek to %2 for export"),
						  (*i)->name(), position)
				      << endmsg;
				return -1;
			}
		}
	}

	/* we just did the core part of a locate() call above, but
	   for the sake of any GUI, put the _transport_frame in
	   the right place too.
	*/

	_transport_frame = position;
	
	_exporting_realtime = realtime;
	export_status->stop = false;

	/* get transport ready. note how this is calling butler functions
	   from a non-butler thread. we waited for the butler to stop
	   what it was doing earlier in Session::pre_export() and nothing
	   since then has re-awakened it.
	 */

	set_transport_speed (1.0, false);
	butler_transport_work ();
	g_atomic_int_set (&butler_should_do_transport_work, 0);
	post_transport ();

	/* we are ready to go ... */
	
	if (!_engine.connected()) {
		return -1;
	}

	if (realtime) {
		last_process_function = process_function;
		process_function = &Session::process_export;
	} else {
		export_freewheel_connection = _engine.Freewheel.connect (sigc::mem_fun (*this, &Session::process_export_fw));
		return _engine.freewheel (true);
	}

	return 0;
}

void
Session::process_export (nframes_t nframes)
{
	try {

		if (export_status->stop) {
			stop_audio_export ();
			return;
		}
	
		if (!_exporting_realtime) {
			/* make sure we've caught up with disk i/o, since
			we're running faster than realtime c/o JACK.
			*/
			
			wait_till_butler_finished ();
		}
	
		/* do the usual stuff */
	
		process_without_events (nframes);
	
		/* handle export */
	
		ProcessExport (nframes);

	} catch (ExportFailed e) {
		export_status->abort (true);
	}
}

int
Session::process_export_fw (nframes_t nframes)
{
	process_export (nframes);	
	return 0;
}

int
Session::stop_audio_export ()
{
	if (_exporting_realtime) {
		process_function = last_process_function;
	} else {
		export_freewheel_connection.disconnect();
	}

	/* can't use stop_transport() here because we need
	   an immediate halt and don't require all the declick
	   stuff that stop_transport() implements.
	*/

	realtime_stop (true);
	schedule_butler_transport_work ();

	if (!export_status->aborted()) {
		ExportReadFinished ();
	} else {
		finalize_audio_export ();
	}
	
	return 0;

}

void
Session::finalize_audio_export ()
{
	_exporting = false;
	export_status->running = false;

	if (!_exporting_realtime) {
		_engine.freewheel (false);
		_exporting_realtime = false;
	}

	/* Clean up */
	
	ProcessExport.clear();
	ExportReadFinished.clear();
	export_freewheel_connection.disconnect();
	export_handler.reset();
	export_status.reset();

	/* restart slaving */

	if (post_export_slave != None) {
		Config->set_slave_source (post_export_slave);
	} else {
		locate (post_export_position, false, false, false);
	}
}
