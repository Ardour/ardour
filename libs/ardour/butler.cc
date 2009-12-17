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
#include "ardour/crossfade.h"
#include "ardour/io.h"
#include "ardour/midi_diskstream.h"
#include "ardour/session.h"

#include "i18n.h"

using namespace PBD;

static float _read_data_rate;
static float _write_data_rate;

namespace ARDOUR {

Butler::Butler(Session& s)
	: SessionHandleRef (s)
	, thread(0)
	, audio_dstream_buffer_size(0)
	, midi_dstream_buffer_size(0)
{
	g_atomic_int_set(&should_do_transport_work, 0);
}

Butler::~Butler()
{
}

int
Butler::start_thread()
{
	const float rate = (float)_session.frame_rate();

	/* size is in Samples, not bytes */
	audio_dstream_buffer_size = (uint32_t) floor (Config->get_audio_track_buffer_seconds() * rate);

	/* size is in bytes
	 * XXX: Jack needs to tell us the MIDI buffer size
	 * (i.e. how many MIDI bytes we might see in a cycle)
	 */
	midi_dstream_buffer_size = (uint32_t) floor (Config->get_midi_track_buffer_seconds() * rate);

	MidiDiskstream::set_readahead_frames ((nframes_t)(Config->get_midi_readahead() * rate));

	Crossfade::set_buffer_size (audio_dstream_buffer_size);

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
		::write (request_pipe[1], &c, 1);
		pthread_join (thread, &status);
	}
}

void *
Butler::_thread_work (void* arg)
{
	SessionEvent::create_per_thread_pool ("butler events", 64);
	return ((Butler *) arg)->thread_work ();
}

void *
Butler::thread_work ()
{
	uint32_t err = 0;
	int32_t bytes;
	bool compute_io;
	microseconds_t begin, end;

	struct pollfd pfd[1];
	bool disk_work_outstanding = false;
	Session::DiskstreamList::iterator i;

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

					case Request::Wake:
						break;

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

		if (transport_work_requested()) {
			_session.butler_transport_work ();
		}

		disk_work_outstanding = false;
		bytes = 0;
		compute_io = true;

		begin = get_microseconds();

		boost::shared_ptr<Session::DiskstreamList> dsl = _session.diskstream_list().reader ();

//		for (i = dsl->begin(); i != dsl->end(); ++i) {
//			cerr << "BEFORE " << (*i)->name() << ": pb = " << (*i)->playback_buffer_load() << " cp = " << (*i)->capture_buffer_load() << endl;
//		}

		for (i = dsl->begin(); !transport_work_requested() && should_run && i != dsl->end(); ++i) {

			boost::shared_ptr<Diskstream> ds = *i;

			/* don't read inactive tracks */

			boost::shared_ptr<IO> io = ds->io();

			if (io && !io->active()) {
				continue;
			}

			switch (ds->do_refill ()) {
			case 0:
				bytes += ds->read_data_count();
				break;
			case 1:
				bytes += ds->read_data_count();
				disk_work_outstanding = true;
				break;

			default:
				compute_io = false;
				error << string_compose(_("Butler read ahead failure on dstream %1"), (*i)->name()) << endmsg;
				break;
			}

		}

		if (i != dsl->end()) {
			/* we didn't get to all the streams */
			disk_work_outstanding = true;
		}

		if (!err && transport_work_requested()) {
			continue;
		}

		if (compute_io) {
			end = get_microseconds();
			if (end - begin > 0) {
				_read_data_rate = (float) bytes / (float) (end - begin);
			} else {
				_read_data_rate = 0; // infinity better
			}
		}

		bytes = 0;
		compute_io = true;
		begin = get_microseconds();

		for (i = dsl->begin(); !transport_work_requested() && should_run && i != dsl->end(); ++i) {
			// cerr << "write behind for " << (*i)->name () << endl;

			/* note that we still try to flush diskstreams attached to inactive routes
			 */

			switch ((*i)->do_flush (ButlerContext)) {
			case 0:
				bytes += (*i)->write_data_count();
				break;
			case 1:
				bytes += (*i)->write_data_count();
				disk_work_outstanding = true;
				break;

			default:
				err++;
				compute_io = false;
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

		if (i != dsl->end()) {
			/* we didn't get to all the streams */
			disk_work_outstanding = true;
		}

		if (!err && transport_work_requested()) {
			continue;
		}

		if (compute_io) {
			// there are no apparent users for this calculation?
			end = get_microseconds();
			if (end - begin > 0) {
				_write_data_rate = (float) bytes / (float) (end - begin);
			} else {
				_write_data_rate = 0; // Well, infinity would be better
			}
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

				continue;
			}

			paused.signal();
		}
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
	::write (request_pipe[1], &c, 1);
}

void
Butler::stop ()
{
	Glib::Mutex::Lock lm (request_lock);
	char c = Request::Pause;
	::write (request_pipe[1], &c, 1);
	paused.wait(request_lock);
}

void
Butler::wait_until_finished ()
{
	Glib::Mutex::Lock lm (request_lock);
	char c = Request::Wake;
	::write (request_pipe[1], &c, 1);
	paused.wait(request_lock);
}

bool
Butler::transport_work_requested () const
{
	return g_atomic_int_get(&should_do_transport_work);
}

float
Butler::read_data_rate () const
{
	/* disk i/o in excess of 10000MB/sec indicate the buffer cache
	   in action. ignore it.
	*/
	return _read_data_rate > 10485.7600000f ? 0.0f : _read_data_rate;
}

float
Butler::write_data_rate () const
{
	/* disk i/o in excess of 10000MB/sec indicate the buffer cache
	   in action. ignore it.
	*/
	return _write_data_rate > 10485.7600000f ? 0.0f : _write_data_rate;
}

} // namespace ARDOUR
