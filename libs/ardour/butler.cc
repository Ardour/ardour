/*
    Copyright (C) 1999-2009 Paul Davis

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

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include "pbd/error.h"
#include "pbd/pthread_utils.h"
#include "ardour/butler.h"
#include "ardour/io.h"
#include "ardour/midi_diskstream.h"
#include "ardour/session.h"
#include "ardour/track.h"
#include "ardour/auditioner.h"

#include "i18n.h"

using namespace PBD;

namespace ARDOUR {

Butler::Butler(Session& s)
	: SessionHandleRef (s)
	, thread(0)
	, audio_dstream_capture_buffer_size(0)
	, audio_dstream_playback_buffer_size(0)
	, midi_dstream_buffer_size(0)
	, pool_trash(16)
{
	g_atomic_int_set(&should_do_transport_work, 0);
	SessionEvent::pool->set_trash (&pool_trash);

        Config->ParameterChanged.connect_same_thread (*this, boost::bind (&Butler::config_changed, this, _1));
}

Butler::~Butler()
{
	terminate_thread ();
}

void
Butler::config_changed (std::string p)
{
        if (p == "playback-buffer-seconds") {
                /* size is in Samples, not bytes */
                audio_dstream_playback_buffer_size = (uint32_t) floor (Config->get_audio_playback_buffer_seconds() * _session.frame_rate());
                _session.adjust_playback_buffering ();
        } else if (p == "capture-buffer-seconds") {
                audio_dstream_capture_buffer_size = (uint32_t) floor (Config->get_audio_capture_buffer_seconds() * _session.frame_rate());
                _session.adjust_capture_buffering ();
        }
}

int
Butler::start_thread()
{
	const float rate = (float)_session.frame_rate();

	/* size is in Samples, not bytes */
	audio_dstream_capture_buffer_size = (uint32_t) floor (Config->get_audio_capture_buffer_seconds() * rate);
	audio_dstream_playback_buffer_size = (uint32_t) floor (Config->get_audio_playback_buffer_seconds() * rate);

	/* size is in bytes
	 * XXX: Jack needs to tell us the MIDI buffer size
	 * (i.e. how many MIDI bytes we might see in a cycle)
	 */
	midi_dstream_buffer_size = (uint32_t) floor (Config->get_midi_track_buffer_seconds() * rate);

	MidiDiskstream::set_readahead_frames ((framecnt_t) (Config->get_midi_readahead() * rate));

	should_run = false;

	if (pipe (request_pipe)) {
		error << string_compose(_("Cannot create transport request signal pipe (%1)"),
				strerror (errno)) << endmsg;
		return -1;
	}

	if (fcntl (request_pipe[0], F_SETFL, O_NONBLOCK)) {
		error << string_compose(_("UI: cannot set O_NONBLOCK on butler request pipe (%1)"),
				strerror (errno)) << endmsg;
		return -1;
	}

	if (fcntl (request_pipe[1], F_SETFL, O_NONBLOCK)) {
		error << string_compose(_("UI: cannot set O_NONBLOCK on butler request pipe (%1)"),
				strerror (errno)) << endmsg;
		return -1;
	}

	if (pthread_create_and_store ("disk butler", &thread, _thread_work, this)) {
		error << _("Session: could not create butler thread") << endmsg;
		return -1;
	}

	//pthread_detach (thread);

	return 0;
}

void
Butler::terminate_thread ()
{
	if (thread) {
		void* status;
		const char c = Request::Quit;
		(void) ::write (request_pipe[1], &c, 1);
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

	struct pollfd pfd[1];
	bool disk_work_outstanding = false;
	RouteList::iterator i;

	while (true) {
		pfd[0].fd = request_pipe[0];
		pfd[0].events = POLLIN|POLLERR|POLLHUP;

		if (poll (pfd, 1, (disk_work_outstanding ? 0 : -1)) < 0) {

			if (errno == EINTR) {
				continue;
			}

			error << string_compose (_("poll on butler request pipe failed (%1)"),
					  strerror (errno))
			      << endmsg;
			break;
		}

		if (pfd[0].revents & ~POLLIN) {
			error << string_compose (_("Error on butler thread request pipe: fd=%1 err=%2"), pfd[0].fd, pfd[0].revents) << endmsg;
			break;
		}

		if (pfd[0].revents & POLLIN) {

			char req;

			/* empty the pipe of all current requests */

			while (1) {
				size_t nread = ::read (request_pipe[0], &req, sizeof (req));
				if (nread == 1) {

					switch ((Request::Type) req) {

					case Request::Run:
						should_run = true;
						break;

					case Request::Pause:
						should_run = false;
						break;

					case Request::Quit:
						pthread_exit_pbd (0);
						/*NOTREACHED*/
						break;

					default:
						break;
					}

				} else if (nread == 0) {
					break;
				} else if (errno == EAGAIN) {
					break;
				} else {
					fatal << _("Error reading from butler request pipe") << endmsg;
					/*NOTREACHED*/
				}
			}
		}


restart:
		disk_work_outstanding = false;

		if (transport_work_requested()) {
			_session.butler_transport_work ();
		}

		boost::shared_ptr<RouteList> rl = _session.get_routes();

		RouteList rl_with_auditioner = *rl;
		rl_with_auditioner.push_back (_session.the_auditioner());

//		for (i = dsl->begin(); i != dsl->end(); ++i) {
//			cerr << "BEFORE " << (*i)->name() << ": pb = " << (*i)->playback_buffer_load() << " cp = " << (*i)->capture_buffer_load() << endl;
//		}

		for (i = rl_with_auditioner.begin(); !transport_work_requested() && should_run && i != rl_with_auditioner.end(); ++i) {

			boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);

			if (!tr) {
				continue;
			}

			boost::shared_ptr<IO> io = tr->input ();

			if (io && !io->active()) {
				/* don't read inactive tracks */
				continue;
			}

			switch (tr->do_refill ()) {
			case 0:
				break;
				
			case 1:
				disk_work_outstanding = true;
				break;

			default:
				error << string_compose(_("Butler read ahead failure on dstream %1"), (*i)->name()) << endmsg;
				break;
			}

		}

		if (i != rl_with_auditioner.begin() && i != rl_with_auditioner.end()) {
			/* we didn't get to all the streams */
			disk_work_outstanding = true;
		}

		if (!err && transport_work_requested()) {
			goto restart;
		}

		for (i = rl->begin(); !transport_work_requested() && should_run && i != rl->end(); ++i) {
			// cerr << "write behind for " << (*i)->name () << endl;

			boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);

			if (!tr) {
				continue;
			}

			/* note that we still try to flush diskstreams attached to inactive routes
			 */

			switch (tr->do_flush (ButlerContext)) {
			case 0:
				break;
				
			case 1:
				disk_work_outstanding = true;
				break;

			default:
				err++;
				error << string_compose(_("Butler write-behind failure on dstream %1"), (*i)->name()) << endmsg;
				/* don't break - try to flush all streams in case they
				   are split across disks.
				*/
			}
		}

		if (err && _session.actively_recording()) {
			/* stop the transport and try to catch as much possible
			   captured state as we can.
			*/
			_session.request_stop ();
		}

		if (i != rl->begin() && i != rl->end()) {
			/* we didn't get to all the streams */
			disk_work_outstanding = true;
		}

		if (!err && transport_work_requested()) {
			goto restart;
		}

		if (!disk_work_outstanding) {
			_session.refresh_disk_space ();
		}


		{
			Glib::Mutex::Lock lm (request_lock);

			if (should_run && (disk_work_outstanding || transport_work_requested())) {
//				for (DiskstreamList::iterator i = dsl->begin(); i != dsl->end(); ++i) {
//					cerr << "AFTER " << (*i)->name() << ": pb = " << (*i)->playback_buffer_load() << " cp = " << (*i)->capture_buffer_load() << endl;
//				}

				goto restart;
			}

			paused.signal();
		}

		empty_pool_trash ();
	}

	pthread_exit_pbd (0);
	/*NOTREACHED*/
	return (0);
}

void
Butler::schedule_transport_work ()
{
	g_atomic_int_inc (&should_do_transport_work);
	summon ();
}

void
Butler::summon ()
{
	char c = Request::Run;
	(void) ::write (request_pipe[1], &c, 1);
}

void
Butler::stop ()
{
	Glib::Mutex::Lock lm (request_lock);
	char c = Request::Pause;
	(void) ::write (request_pipe[1], &c, 1);
	paused.wait(request_lock);
}

void
Butler::wait_until_finished ()
{
	Glib::Mutex::Lock lm (request_lock);
	char c = Request::Pause;
	(void) ::write (request_pipe[1], &c, 1);
	paused.wait(request_lock);
}

bool
Butler::transport_work_requested () const
{
	return g_atomic_int_get(&should_do_transport_work);
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
	SessionEvent::pool->set_trash (0);
}


} // namespace ARDOUR

