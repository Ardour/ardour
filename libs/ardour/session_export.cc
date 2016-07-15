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


#include "pbd/error.h"
#include <glibmm/threads.h>

#include <midi++/mmc.h>

#include "ardour/audioengine.h"
#include "ardour/butler.h"
#include "ardour/export_handler.h"
#include "ardour/export_status.h"
#include "ardour/process_thread.h"
#include "ardour/session.h"
#include "ardour/track.h"

#include "pbd/i18n.h"

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

	/* take everyone out of awrite to avoid disasters */

	{
		boost::shared_ptr<RouteList> r = routes.reader ();

		for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
			(*i)->protect_automation ();
		}
	}

	/* prepare transport */

	realtime_stop (true, true);

	if (get_record_enabled()) {
		disable_record (false);
	}

	unset_play_loop ();

	/* no slaving */

	post_export_sync = config.get_external_sync ();
	post_export_position = _transport_frame;

	config.set_external_sync (false);

	_exporting = true;
	export_status->set_running (true);
	export_status->Finished.connect_same_thread (*this, boost::bind (&Session::finalize_audio_export, this));

	/* disable MMC output early */

	_pre_export_mmc_enabled = _mmc->send_enabled ();
	_mmc->enable_send (false);

	return 0;
}

/** Called for each range that is being exported */
int
Session::start_audio_export (framepos_t position, bool realtime)
{
	if (!_exporting) {
		pre_export ();
	}

	_realtime_export = realtime;

	if (realtime) {
		_export_preroll = nominal_frame_rate ();
	} else {
		_export_preroll = Config->get_export_preroll() * nominal_frame_rate ();
	}

	if (_export_preroll == 0) {
		// must be > 0 so that transport is started in sync.
		_export_preroll = 1;
	}

	/* "worst_track_latency" is the correct value for stem-exports
	 * see to Route::add_export_point(),
	 *
	 * for master-bus export, we'd need to add the master's latency.
	 * or actually longest-total-session-latency.
	 *
	 * We can't use worst_playback_latency because that includes
	 * includes external latencies and would overcompensate.
	 */
	_export_latency = worst_track_latency ();

	/* We're about to call Track::seek, so the butler must have finished everything
	   up otherwise it could be doing do_refill in its thread while we are doing
	   it here.
	*/

	_butler->wait_until_finished ();

	/* get everyone to the right position */

	{
		boost::shared_ptr<RouteList> rl = routes.reader();

		for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
			boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);
			if (tr && tr->seek (position, true)) {
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
	export_status->stop = false;

	/* get transport ready. note how this is calling butler functions
	   from a non-butler thread. we waited for the butler to stop
	   what it was doing earlier in Session::pre_export() and nothing
	   since then has re-awakened it.
	 */

	/* we are ready to go ... */

	if (!_engine.connected()) {
		return -1;
	}

	_engine.Freewheel.connect_same_thread (export_freewheel_connection, boost::bind (&Session::process_export_fw, this, _1));

	if (_realtime_export) {
		Glib::Threads::Mutex::Lock lm (AudioEngine::instance()->process_lock ());
		_export_rolling = true;
		process_function = &Session::process_export_fw;
		return 0;
	} else {
		_export_rolling = true;
		return _engine.freewheel (true);
	}
}

void
Session::process_export (pframes_t nframes)
{
	if (_export_rolling && export_status->stop) {
		stop_audio_export ();
	}

	if (_export_rolling) {
		if (!_realtime_export)  {
			/* make sure we've caught up with disk i/o, since
			 * we're running faster than realtime c/o JACK.
			 */
			_butler->wait_until_finished ();
		}

		/* do the usual stuff */

		process_without_events (nframes);
	} else if (_realtime_export) {
		fail_roll (nframes); // somehow we need to silence _ALL_ output buffers
	}

	try {
		/* handle export - XXX what about error handling? */

		ProcessExport (nframes);

	} catch (std::exception & e) {
		error << string_compose (_("Export ended unexpectedly: %1"), e.what()) << endmsg;
		export_status->abort (true);
	}
}

void
Session::process_export_fw (pframes_t nframes)
{
	const bool need_buffers = _engine.freewheeling ();
	if (_export_preroll > 0) {

		if (need_buffers) {
			_engine.main_thread()->get_buffers ();
		}
		fail_roll (nframes);
		if (need_buffers) {
			_engine.main_thread()->drop_buffers ();
		}

		_export_preroll -= std::min ((framepos_t)nframes, _export_preroll);

		if (_export_preroll > 0) {
			// clear out buffers (reverb tails etc).
			return;
		}

		set_transport_speed (1.0, 0, false);
		butler_transport_work ();
		g_atomic_int_set (&_butler->should_do_transport_work, 0);
		post_transport ();

		return;
	}

	if (_export_latency > 0) {
		framepos_t remain = std::min ((framepos_t)nframes, _export_latency);

		if (need_buffers) {
			_engine.main_thread()->get_buffers ();
		}
		process_without_events (remain);
		if (need_buffers) {
			_engine.main_thread()->drop_buffers ();
		}

		_export_latency -= remain;
		nframes -= remain;

		if (nframes == 0) {
			return;
		}
	}

	if (need_buffers) {
		_engine.main_thread()->get_buffers ();
	}
	process_export (nframes);
	if (need_buffers) {
		_engine.main_thread()->drop_buffers ();
	}

	return;
}

int
Session::stop_audio_export ()
{
	/* can't use stop_transport() here because we need
	   an immediate halt and don't require all the declick
	   stuff that stop_transport() implements.
	*/

	realtime_stop (true, true);
	_export_rolling = false;
	_butler->schedule_transport_work ();

	return 0;
}

void
Session::finalize_audio_export ()
{
	_exporting = false;

	if (_export_rolling) {
		stop_audio_export ();
	}

	/* Clean up */

	if (_realtime_export) {
		Glib::Threads::Mutex::Lock lm (AudioEngine::instance()->process_lock ());
		process_function = &Session::process_with_events;
	}
	_engine.freewheel (false);
	export_freewheel_connection.disconnect();

	_mmc->enable_send (_pre_export_mmc_enabled);

	/* maybe write CUE/TOC */

	export_handler.reset();
	export_status.reset();

	/* restart slaving */

	if (post_export_sync) {
		config.set_external_sync (true);
	} else {
		locate (post_export_position, false, false, false, false, false);
	}
}
