/*
 * Copyright (C) 2005-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2006-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2008-2013 Sakari Bergen <sakari.bergen@beatwaves.net>
 * Copyright (C) 2010-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2015-2018 Robin Gareus <robin@gareus.org>
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


#include "pbd/error.h"
#include <glibmm/threads.h>
#include <glibmm/timer.h>

#include <midi++/mmc.h>

#include "ardour/audioengine.h"
#include "ardour/butler.h"
#include "ardour/export_handler.h"
#include "ardour/export_status.h"
#include "ardour/process_thread.h"
#include "ardour/session.h"
#include "ardour/track.h"
#include "ardour/transport_fsm.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

#define TFSM_ROLL() { _transport_fsm->enqueue (new TransportFSM::Event (TransportFSM::StartTransport)); }
#define TFSM_SPEED(speed) { _transport_fsm->enqueue (new TransportFSM::Event (speed)); }

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
		disable_record (false, true);
	}

	unset_play_loop ();

	/* no slaving */

	post_export_sync = config.get_external_sync ();
	post_export_position = _transport_sample;

	config.set_external_sync (false);

	_export_xruns = 0;
	_exporting = true;
	export_status->set_running (true);
	export_status->Finished.connect_same_thread (*this, boost::bind (&Session::finalize_audio_export, this, _1));

	/* disable MMC output early */

	_pre_export_mmc_enabled = _mmc->send_enabled ();
	_mmc->enable_send (false);

	return 0;
}

/** Called for each range that is being exported */
int
Session::start_audio_export (samplepos_t position, bool realtime, bool region_export)
{
	assert (!engine().in_process_thread ());

	if (!_exporting) {
		pre_export ();
	} else if (_transport_fsm->transport_speed() != 0) {
		realtime_stop (true, true);
	}

	_region_export = region_export;

	if (region_export) {
		_export_preroll = 0;
	}
	else if (realtime) {
		_export_preroll = nominal_sample_rate ();
	} else {
		_export_preroll = Config->get_export_preroll() * nominal_sample_rate ();
	}

	if (_export_preroll == 0) {
		// must be > 0 so that transport is started in sync.
		_export_preroll = 1;
	}

	/* realtime_stop will have queued butler work (and TSFM),
	 * but the butler may not run immediately, so well have
	 * to wait for it to wake up and call
	 * non_realtime_stop ().
	 */
	int sleeptm = std::max (40000, engine().usecs_per_cycle ());
	int timeout = std::max (100, 8000000 / sleeptm);
	do {
		Glib::usleep (sleeptm);
		sched_yield ();
	} while (_transport_fsm->waiting_for_butler() && --timeout > 0);

	if (timeout == 0) {
		error << _("Cannot prepare transport for export") << endmsg;
		return -1;
	}

	/* We're about to call Track::seek, so the butler must have finished everything
	   up otherwise it could be doing do_refill in its thread while we are doing
	   it here.
	*/

	{
		Glib::Threads::Mutex::Lock lm (AudioEngine::instance()->process_lock ());
		_butler->wait_until_finished ();

	/* get everyone to the right position */

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

	/* we just did the core part of a locate call above, but
	   for the sake of any GUI, put the _transport_sample in
	   the right place too.
	*/

	_transport_sample = position;

	if (!region_export) {
		_remaining_latency_preroll = worst_latency_preroll_buffer_size_ceil ();
	} else {
		_remaining_latency_preroll = 0;
	}

	/* get transport ready. note how this is calling butler functions
	   from a non-butler thread. we waited for the butler to stop
	   what it was doing earlier in Session::pre_export() and nothing
	   since then has re-awakened it.
	 */

	/* we are ready to go ... */

	if (!_engine.running()) {
		return -1;
	}

	assert (!_engine.freewheeling ());
	assert (!_engine.in_process_thread ());

	if (realtime) {
		Glib::Threads::Mutex::Lock lm (AudioEngine::instance()->process_lock ());
		_export_rolling = true;
		_realtime_export = true;
		export_status->stop = false;
		process_function = &Session::process_export_fw;
		/* this is required for ExportGraphBuilder::Intermediate::start_post_processing */
		_engine.Freewheel.connect_same_thread (export_freewheel_connection, boost::bind (&Session::process_export_fw, this, _1));
		reset_xrun_count ();
		return 0;
	} else {
		if (_realtime_export) {
			Glib::Threads::Mutex::Lock lm (AudioEngine::instance()->process_lock ());
			process_function = &Session::process_with_events;
		}
		_realtime_export = false;
		_export_rolling = true;
		export_status->stop = false;
		_engine.Freewheel.connect_same_thread (export_freewheel_connection, boost::bind (&Session::process_export_fw, this, _1));
		reset_xrun_count ();
		return _engine.freewheel (true);
	}
}

void
Session::process_export (pframes_t nframes)
{
	if (_export_rolling && export_status->stop) {
		stop_audio_export ();
	}

	/* for Region Raw or Fades, we can skip this
	 * RegionExportChannelFactory::update_buffers() does not care
	 * about anything done here
	 */
	if (!_region_export) {
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
	}

	try {
		/* handle export - XXX what about error handling? */

		if (ProcessExport (nframes).value_or (0) > 0) {
			/* last cycle completed */
			assert (_export_rolling);
			stop_audio_export ();
		}

	} catch (std::exception & e) {
		error << string_compose (_("Export ended unexpectedly: %1"), e.what()) << endmsg;
		export_status->abort (true);
	}
}

void
Session::process_export_fw (pframes_t nframes)
{
	if (!_export_rolling) {
		try {
			ProcessExport (0);
		} catch (std::exception & e) {
			/* pre-roll export must not throw */
			assert (0);
			export_status->abort (true);
		}
		return;
	}

	const bool need_buffers = _engine.freewheeling ();
	if (_export_preroll > 0) {

		if (need_buffers) {
			_engine.main_thread()->get_buffers ();
		}
		fail_roll (nframes);
		if (need_buffers) {
			_engine.main_thread()->drop_buffers ();
		}

		_export_preroll -= std::min ((samplepos_t)nframes, _export_preroll);

		if (_export_preroll > 0) {
			// clear out buffers (reverb tails etc).
			return;
		}

		TFSM_SPEED (1.0);
		TFSM_ROLL ();
		_butler->schedule_transport_work ();

		/* Session::process_with_events () sets _remaining_latency_preroll = 0
		 * when being called with _transport_fsm->transport_speed() == 0.
		 *
		 * This can happen wit JACK, there is a process-callback before
		 * freewheeling becomes active, after Session::start_audio_export().
		 */
		if (!_region_export) {
			_remaining_latency_preroll = worst_latency_preroll_buffer_size_ceil ();
		}

		return;
	}

	/* wait for butler to complete schedule_transport_work(),
	 * compare to Session::process */
	if (non_realtime_work_pending ()) {
		if (_butler->transport_work_requested ()) {
			/* butler is still processing */
			return;
		}
		butler_completed_transport_work ();
	}

	if (_remaining_latency_preroll > 0) {
		samplepos_t remain = std::min ((samplepos_t)nframes, _remaining_latency_preroll);

		if (need_buffers) {
			_engine.main_thread()->get_buffers ();
		}

		assert (_count_in_samples == 0);
		while (remain > 0) {
			samplecnt_t ns = calc_preroll_subcycle (remain);

			bool session_needs_butler = false;
			if (process_routes (ns, session_needs_butler)) {
				fail_roll (ns);
			}

			try {
				ProcessExport (ns);
			} catch (std::exception & e) {
				/* pre-roll export must not throw */
				assert (0);
				export_status->abort (true);
			}

			_remaining_latency_preroll -= ns;
			remain -= ns;
			nframes -= ns;

			if (remain != 0) {
				_engine.split_cycle (ns);
			}
		}

		if (need_buffers) {
			_engine.main_thread()->drop_buffers ();
		}

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
	   an synchronous halt and don't require all the declick
	   stuff that stop_transport() implements.
	*/

	realtime_stop (true, true);
	flush_all_inserts ();
	_export_rolling = false;
	_butler->schedule_transport_work ();
	reset_xrun_count ();

	return 0;
}

void
Session::finalize_audio_export (TransportRequestSource trs)
{
	/* This is called as a handler for the Finished signal, which is
	   emitted by a UI component once the ExportStatus object associated
	   with this export indicates that it has finished. It runs in the UI
	   thread that emits the signal.
	*/

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
		request_locate (post_export_position, MustStop, trs);
	}
}
