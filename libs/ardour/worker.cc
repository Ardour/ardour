/*
 * Copyright (C) 2012-2016 David Robillard <d@drobilla.net>
 * Copyright (C) 2012-2019 Robin Gareus <robin@gareus.org>
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

#include <stdlib.h>
#include <unistd.h>

#include <glibmm/timer.h>

#include "pbd/error.h"
#include "pbd/compose.h"
#include "pbd/pthread_utils.h"

#include "ardour/worker.h"

namespace ARDOUR {

Worker::Worker(Workee* workee, uint32_t ring_size, bool threaded)
	: _workee(workee)
	, _requests(threaded ? new PBD::RingBuffer<uint8_t>(ring_size) : NULL)
	, _responses(new PBD::RingBuffer<uint8_t>(ring_size))
	, _response((uint8_t*)malloc(ring_size))
	, _sem(string_compose ("worker_semaphore%1", this).c_str(), 0)
	, _thread(NULL)
	, _exit(false)
	, _synchronous(!threaded)
{
	if (threaded) {
		_thread = Glib::Threads::Thread::create(
			sigc::mem_fun(*this, &Worker::run));
	}
}

Worker::~Worker()
{
	_exit = true;
	_sem.signal();
	if (_thread) {
		_thread->join();
	}
	delete _responses;
	delete _requests;
	free (_response);
}

bool
Worker::schedule(uint32_t size, const void* data)
{
	if (_synchronous || !_requests) {
		_workee->work(*this, size, data);
		emit_responses ();
		return true;
	}
	if (_requests->write_space() < size + sizeof(size)) {
		return false;
	}
	if (_requests->write((const uint8_t*)&size, sizeof(size)) != sizeof(size)) {
		return false;
	}
	if (_requests->write((const uint8_t*)data, size) != size) {
		return false;
	}
	_sem.signal();
	return true;
}

bool
Worker::respond(uint32_t size, const void* data)
{
	if (_responses->write_space() < size + sizeof(size)) {
		return false;
	}
	if (_responses->write((const uint8_t*)&size, sizeof(size)) != sizeof(size)) {
		return false;
	}
	if (_responses->write((const uint8_t*)data, size) != size) {
		return false;
	}
	return true;
}

bool
Worker::verify_message_completeness(PBD::RingBuffer<uint8_t>* rb)
{
	uint32_t read_space = rb->read_space();
	uint32_t size;
	PBD::RingBuffer<uint8_t>::rw_vector vec;
	rb->get_read_vector (&vec);
	if (vec.len[0] + vec.len[1] < sizeof(size)) {
		return false;
	}
	if (vec.len[0] >= sizeof(size)) {
		memcpy (&size, vec.buf[0], sizeof (size));
	} else {
		memcpy (&size, vec.buf[0], vec.len[0]);
		memcpy (& reinterpret_cast<uint8_t*>(&size)[vec.len[0]], vec.buf[1], sizeof(size) - vec.len[0]);
	}
	if (read_space < size+sizeof(size)) {
		/* message from writer is yet incomplete. respond next cycle */
		return false;
	}
	return true;
}

void
Worker::emit_responses()
{
	uint32_t read_space = _responses->read_space();
	uint32_t size       = 0;
	while (read_space >= sizeof(size)) {
		if (!verify_message_completeness(_responses)) {
			/* message from writer is yet incomplete. respond next cycle */
			return;
		}
		/* read and send response */
		_responses->read((uint8_t*)&size, sizeof(size));
		_responses->read(_response, size);
		_workee->work_response(size, _response);
		read_space -= sizeof(size) + size;
	}
}

void
Worker::run()
{
	pthread_set_name ("LV2Worker");

	void*  buf      = NULL;
	size_t buf_size = 0;
	while (true) {
		_sem.wait();
		if (_exit) {
			free(buf);
			return;
		}

		uint32_t size = _requests->read_space();
		if (size < sizeof(size)) {
			PBD::error << "Worker: no work-data on ring buffer" << endmsg;
			continue;
		}
		while (!verify_message_completeness(_requests)) {
			Glib::usleep(2000);
			if (_exit) {
				if (buf) free(buf);
				return;
			}
		}
		if (_requests->read((uint8_t*)&size, sizeof(size)) < sizeof(size)) {
			PBD::error << "Worker: Error reading size from request ring"
			           << endmsg;
			continue;
		}

		if (size > buf_size) {
			buf = realloc(buf, size);
			if (buf) {
				buf_size = size;
			} else {
				PBD::fatal << "Worker: Error allocating memory" << endmsg;
				abort(); /*NOTREACHED*/
			}
		}
		assert (buf);

		if (_requests->read((uint8_t*)buf, size) < size) {
			PBD::error << "Worker: Error reading body from request ring"
			           << endmsg;
			continue;  // TODO: This is probably fatal
		}

		_workee->work(*this, size, buf);
	}
}

} // namespace ARDOUR
