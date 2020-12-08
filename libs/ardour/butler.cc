/*
 * Copyright (C) 1999-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2010-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015 GZharun <grygoriiz@wavesglobal.com>
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

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#ifndef PLATFORM_WINDOWS
#include <poll.h>
#endif

#include "pbd/error.h"
#include "pbd/pthread_utils.h"

#include "temporal/superclock.h"
#include "temporal/tempo.h"

#include "ardour/butler.h"
#include "ardour/debug.h"
#include "ardour/disk_io.h"
#include "ardour/disk_reader.h"
#include "ardour/io.h"
#include "ardour/session.h"
#include "ardour/track.h"
#include "ardour/auditioner.h"

#include "pbd/i18n.h"

using namespace PBD;

namespace ARDOUR {

Butler::Butler(Session& s)
	: SessionHandleRef (s)
	, thread()
	, have_thread (false)
	, _audio_capture_buffer_size(0)
	, _audio_playback_buffer_size(0)
	, _midi_buffer_size(0)
	, pool_trash(16)
	, _xthread (true)
{
	g_atomic_int_set (&should_do_transport_work, 0);
	SessionEvent::pool->set_trash (&pool_trash);

	/* catch future changes to parameters */
	Config->ParameterChanged.connect_same_thread (*this, boost::bind (&Butler::config_changed, this, _1));
}

Butler::~Butler()
{
	terminate_thread ();
}

void
Butler::map_parameters ()
{
        /* use any current ones that we care about */
        boost::function<void (std::string)> ff (boost::bind (&Butler::config_changed, this, _1));
        Config->map_parameters (ff);
}

void
Butler::config_changed (std::string p)
{
	if (p == "playback-buffer-seconds") {
		_session.adjust_playback_buffering ();
		if (Config->get_buffering_preset() == Custom) {
			/* size is in Samples, not bytes */
			samplecnt_t audio_playback_buffer_size = (uint32_t) floor (Config->get_audio_playback_buffer_seconds() * _session.sample_rate());
			if (_audio_playback_buffer_size != audio_playback_buffer_size) {
				_audio_playback_buffer_size = audio_playback_buffer_size;
				_session.adjust_playback_buffering ();
			}
		}
	} else if (p == "capture-buffer-seconds") {
		if (Config->get_buffering_preset() == Custom) {
			/* size is in Samples, not bytes */
			samplecnt_t audio_capture_buffer_size = (uint32_t) floor (Config->get_audio_capture_buffer_seconds() * _session.sample_rate());
			if (_audio_capture_buffer_size != audio_capture_buffer_size) {
				_audio_capture_buffer_size = audio_capture_buffer_size;
				_session.adjust_capture_buffering ();
			}
		}
	} else if (p == "buffering-preset") {
		DiskIOProcessor::set_buffering_parameters (Config->get_buffering_preset());
		samplecnt_t audio_capture_buffer_size = (uint32_t) floor (Config->get_audio_capture_buffer_seconds() * _session.sample_rate());
		samplecnt_t audio_playback_buffer_size = (uint32_t) floor (Config->get_audio_playback_buffer_seconds() * _session.sample_rate());
		if (_audio_capture_buffer_size != audio_capture_buffer_size) {
			_audio_capture_buffer_size = audio_capture_buffer_size;
			_session.adjust_capture_buffering ();
		}
		if (_audio_playback_buffer_size != audio_playback_buffer_size) {
			_audio_playback_buffer_size = audio_playback_buffer_size;
			_session.adjust_playback_buffering ();
		}
	}
}

int
Butler::start_thread()
{
	// set up capture and playback buffering
	DiskIOProcessor::set_buffering_parameters (Config->get_buffering_preset());

	/* size is in Samples, not bytes */
	const float rate = (float)_session.sample_rate();
	_audio_capture_buffer_size = (uint32_t) floor (Config->get_audio_capture_buffer_seconds() * rate);
	_audio_playback_buffer_size = (uint32_t) floor (Config->get_audio_playback_buffer_seconds() * rate);

	/* size is in bytes
	 * XXX: AudioEngine needs to tell us the MIDI buffer size
	 * (i.e. how many MIDI bytes we might see in a cycle)
	 */
	_midi_buffer_size = (uint32_t) floor (Config->get_midi_track_buffer_seconds() * rate);

	should_run = false;

	if (pthread_create_and_store ("disk butler", &thread, _thread_work, this)) {
		error << _("Session: could not create butler thread") << endmsg;
		return -1;
	}

	//pthread_detach (thread);
	have_thread = true;

	// we are ready to request buffer adjustments
	_session.adjust_capture_buffering ();
	_session.adjust_playback_buffering ();

	return 0;
}

void
Butler::terminate_thread ()
{
	if (have_thread) {
		void* status;
                DEBUG_TRACE (DEBUG::Butler, string_compose ("%1: ask butler to quit @ %2\n", DEBUG_THREAD_SELF, g_get_monotonic_time()));
		queue_request (Request::Quit);
		pthread_join (thread, &status);
	}
}

void *
Butler::_thread_work (void* arg)
{
	SessionEvent::create_per_thread_pool ("butler events", 4096);
	pthread_set_name (X_("butler"));
	return ((Butler *) arg)->thread_work ();
}

void *
Butler::thread_work ()
{
	uint32_t err = 0;

	bool disk_work_outstanding = false;
	RouteList::iterator i;

	while (true) {
		DEBUG_TRACE (DEBUG::Butler, string_compose ("%1 butler main loop, disk work outstanding ? %2 @ %3\n", DEBUG_THREAD_SELF, disk_work_outstanding, g_get_monotonic_time()));

		if(!disk_work_outstanding) {
			DEBUG_TRACE (DEBUG::Butler, string_compose ("%1 butler waits for requests @ %2\n", DEBUG_THREAD_SELF, g_get_monotonic_time()));

			char msg;
			/* empty the pipe of all current requests */
			if (_xthread.receive (msg, true) >= 0) {
				Request::Type req = (Request::Type) msg;
				switch (req) {

					case Request::Run:
						DEBUG_TRACE (DEBUG::Butler, string_compose ("%1: butler asked to run @ %2\n", DEBUG_THREAD_SELF, g_get_monotonic_time()));
						should_run = true;
						break;

					case Request::Pause:
						DEBUG_TRACE (DEBUG::Butler, string_compose ("%1: butler asked to pause @ %2\n", DEBUG_THREAD_SELF, g_get_monotonic_time()));
						should_run = false;
						break;

					case Request::Quit:
						DEBUG_TRACE (DEBUG::Butler, string_compose ("%1: butler asked to quit @ %2\n", DEBUG_THREAD_SELF, g_get_monotonic_time()));
						return 0;
						abort(); /*NOTREACHED*/
						break;

					default:
						break;
				}
			}
		}

		Temporal::TempoMap::fetch ();

	  restart:
		DEBUG_TRACE (DEBUG::Butler, "at restart for disk work\n");
		disk_work_outstanding = false;

		if (transport_work_requested()) {
			DEBUG_TRACE (DEBUG::Butler, string_compose ("do transport work @ %1\n", g_get_monotonic_time()));
			_session.butler_transport_work ();
			DEBUG_TRACE (DEBUG::Butler, string_compose ("\ttransport work complete @ %1, twr = %2\n", g_get_monotonic_time(), transport_work_requested()));

			if (_session.locate_initiated()) {
				/* we have done the "stop" required for a
				   locate (DeclickToLocate state in TFSM), but
				   once that finishes we're going to do a locate,
				   so do not bother with buffer refills at this
				   time.
				*/
				Glib::Threads::Mutex::Lock lm (request_lock);
				DEBUG_TRACE (DEBUG::Butler, string_compose ("\tlocate pending, so just pause @ %1 till woken again\n", g_get_monotonic_time()));
				paused.signal ();
				continue;
			}
		}

		sampleoffset_t audition_seek;
		if (should_run && _session.is_auditioning() && (audition_seek = _session.the_auditioner()->seek_sample()) >= 0) {
			boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (_session.the_auditioner());
			DEBUG_TRACE (DEBUG::Butler, "seek the auditioner\n");
			tr->seek(audition_seek);
			tr->do_refill ();
			_session.the_auditioner()->seek_response(audition_seek);
		}

		boost::shared_ptr<RouteList> rl = _session.get_routes();

		RouteList rl_with_auditioner = *rl;
		rl_with_auditioner.push_back (_session.the_auditioner());

		DEBUG_TRACE (DEBUG::Butler, string_compose ("butler starts refill loop, twr = %1\n", transport_work_requested()));

		for (i = rl_with_auditioner.begin(); !transport_work_requested() && should_run && i != rl_with_auditioner.end(); ++i) {

			boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);

			if (!tr) {
				continue;
			}

			boost::shared_ptr<IO> io = tr->input ();

			if (io && !io->active()) {
				/* don't read inactive tracks */
				// DEBUG_TRACE (DEBUG::Butler, string_compose ("butler skips inactive track %1\n", tr->name()));
				continue;
			}
			// DEBUG_TRACE (DEBUG::Butler, string_compose ("butler refills %1, playback load = %2\n", tr->name(), tr->playback_buffer_load()));
			switch (tr->do_refill ()) {
			case 0:
				//DEBUG_TRACE (DEBUG::Butler, string_compose ("\ttrack refill done %1\n", tr->name()));
				break;

			case 1:
				DEBUG_TRACE (DEBUG::Butler, string_compose ("\ttrack refill unfinished %1\n", tr->name()));
				disk_work_outstanding = true;
				break;

			default:
				error << string_compose(_("Butler read ahead failure on dstream %1"), (*i)->name()) << endmsg;
                                std::cerr << string_compose(_("Butler read ahead failure on dstream %1"), (*i)->name()) << std::endl;
				break;
			}

		}

		if (i != rl_with_auditioner.begin() && i != rl_with_auditioner.end()) {
			/* we didn't get to all the streams */
			disk_work_outstanding = true;
		}

		if (!err && transport_work_requested()) {
			DEBUG_TRACE (DEBUG::Butler, "transport work requested during refill, back to restart\n");
			goto restart;
		}

		disk_work_outstanding = disk_work_outstanding || flush_tracks_to_disk_normal (rl, err);

		if (err && _session.actively_recording()) {
			/* stop the transport and try to catch as much possible
			   captured state as we can.
			*/
			DEBUG_TRACE (DEBUG::Butler, "error occurred during recording - stop transport\n");
			_session.request_stop ();
		}

		if (!err && transport_work_requested()) {
			DEBUG_TRACE (DEBUG::Butler, "transport work requested during flush, back to restart\n");
			goto restart;
		}

		if (!disk_work_outstanding) {
			_session.refresh_disk_space ();
		}

		{
			Glib::Threads::Mutex::Lock lm (request_lock);

			if (should_run && (disk_work_outstanding || transport_work_requested())) {
                                DEBUG_TRACE (DEBUG::Butler, string_compose ("at end, should run %1 disk work %2 transport work %3 ... goto restart\n",
                                                                            should_run, disk_work_outstanding, transport_work_requested()));
				goto restart;
			}

                        DEBUG_TRACE (DEBUG::Butler, string_compose ("%1: butler signals pause @ %2\n", DEBUG_THREAD_SELF, g_get_monotonic_time()));
			paused.signal();
		}

		DEBUG_TRACE (DEBUG::Butler, "butler emptying pool trash\n");
		empty_pool_trash ();
	}

	return (0);
}

bool
Butler::flush_tracks_to_disk_normal (boost::shared_ptr<RouteList> rl, uint32_t& errors)
{
	bool disk_work_outstanding = false;

	for (RouteList::iterator i = rl->begin(); !transport_work_requested() && should_run && i != rl->end(); ++i) {

		// cerr << "write behind for " << (*i)->name () << endl;

		boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);

		if (!tr) {
			continue;
		}

		/* note that we still try to flush diskstreams attached to inactive routes
		 */

		int ret;

		// DEBUG_TRACE (DEBUG::Butler, string_compose ("butler flushes track %1 capture load %2\n", tr->name(), tr->capture_buffer_load()));
		ret = tr->do_flush (ButlerContext, false);
		switch (ret) {
		case 0:
			//DEBUG_TRACE (DEBUG::Butler, string_compose ("\tflush complete for %1\n", tr->name()));
			break;

		case 1:
			//DEBUG_TRACE (DEBUG::Butler, string_compose ("\tflush not finished for %1\n", tr->name()));
			disk_work_outstanding = true;
			break;

		default:
			errors++;
			error << string_compose(_("Butler write-behind failure on dstream %1"), (*i)->name()) << endmsg;
			std::cerr << string_compose(_("Butler write-behind failure on dstream %1"), (*i)->name()) << std::endl;
			/* don't break - try to flush all streams in case they
			   are split across disks.
			*/
		}
	}

	return disk_work_outstanding;
}

void
Butler::schedule_transport_work ()
{
	DEBUG_TRACE (DEBUG::Butler, "requesting more transport work\n");
	g_atomic_int_inc (&should_do_transport_work);
	summon ();
}

void
Butler::queue_request (Request::Type r)
{
	char c = r;
	if (_xthread.deliver (c) != 1) {
		/* the x-thread channel is non-blocking
		 * write may fail, but we really don't want to wait
		 * under normal circumstances.
		 *
		 * a lost "run" requests under normal RT operation
		 * is mostly harmless.
		 *
		 * TODO if ardour is freehweeling, wait & retry.
		 * ditto for Request::Type Quit
		 */
		assert(1); // we're screwd
	}
}

void
Butler::summon ()
{
	DEBUG_TRACE (DEBUG::Butler, string_compose ("%1: summon butler to run @ %2\n", DEBUG_THREAD_SELF, g_get_monotonic_time()));
	queue_request (Request::Run);
}

void
Butler::stop ()
{
	Glib::Threads::Mutex::Lock lm (request_lock);
	DEBUG_TRACE (DEBUG::Butler, string_compose ("%1: asking butler to stop @ %2\n", DEBUG_THREAD_SELF, g_get_monotonic_time()));
	queue_request (Request::Pause);
	paused.wait(request_lock);
}

void
Butler::wait_until_finished ()
{
	Glib::Threads::Mutex::Lock lm (request_lock);
	DEBUG_TRACE (DEBUG::Butler, string_compose ("%1: waiting for butler to finish @ %2\n", DEBUG_THREAD_SELF, g_get_monotonic_time()));
	queue_request (Request::Pause);
	paused.wait(request_lock);
}

bool
Butler::transport_work_requested () const
{
	return g_atomic_int_get (&should_do_transport_work);
}

void
Butler::empty_pool_trash ()
{
	/* look in the trash, deleting empty pools until we come to one that is not empty */

	RingBuffer<CrossThreadPool*>::rw_vector vec;
	pool_trash.get_read_vector (&vec);

	guint deleted = 0;

	for (int i = 0; i < 2; ++i) {
		for (guint j = 0; j < vec.len[i]; ++j) {
			if (vec.buf[i][j]->empty()) {
				delete vec.buf[i][j];
				++deleted;
			} else {
				/* found a non-empty pool, so stop deleting */
				if (deleted) {
					pool_trash.increment_read_idx (deleted);
				}
				return;
			}
		}
	}

	if (deleted) {
		pool_trash.increment_read_idx (deleted);
	}
}

void
Butler::drop_references ()
{
	std::cerr << "Butler drops pool trash\n";
	SessionEvent::pool->set_trash (0);
}


} // namespace ARDOUR
