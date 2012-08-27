/*
  Copyright (C) 2012 Paul Davis
  Author: David Robillard

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

#include <stdlib.h>

#include "ardour/worker.h"
#include "pbd/error.h"

namespace ARDOUR {

Worker::Worker(Workee* workee, uint32_t ring_size)
	: _workee(workee)
	, _requests(new RingBuffer<uint8_t>(ring_size))
	, _responses(new RingBuffer<uint8_t>(ring_size))
	, _response((uint8_t*)malloc(ring_size))
	, _sem(0)
	, _exit(false)
	, _thread (Glib::Threads::Thread::create(sigc::mem_fun(*this, &Worker::run)))
{}

Worker::~Worker()
{
	_exit = true;
	_sem.post();
	_thread->join();
}

bool
Worker::schedule(uint32_t size, const void* data)
{
	if (_requests->write((const uint8_t*)&size, sizeof(size)) != sizeof(size)) {
		return false;
	}
	if (_requests->write((const uint8_t*)data, size) != size) {
		return false;  // FIXME: corruption
	}
	_sem.post();
	return true;
}

bool
Worker::respond(uint32_t size, const void* data)
{
	if (_responses->write((const uint8_t*)&size, sizeof(size)) != sizeof(size)) {
		return false;
	}
	if (_responses->write((const uint8_t*)data, size) != size) {
		return false;  // FIXME: corruption
	}
	return true;
}

void
Worker::emit_responses()
{
	uint32_t read_space = _responses->read_space();
	uint32_t size       = 0;
	while (read_space > sizeof(size)) {
		_responses->read((uint8_t*)&size, sizeof(size));
		_responses->read(_response, size);
		_workee->work_response(size, _response);
		read_space -= sizeof(size) + size;
	}
}

void
Worker::run()
{
	void*  buf      = NULL;
	size_t buf_size = 0;
	while (true) {
		_sem.wait();
		if (_exit) {
			return;
		}

		uint32_t size = 0;
		if (_requests->read((uint8_t*)&size, sizeof(size)) < sizeof(size)) {
			PBD::error << "Worker: Error reading size from request ring"
			           << endmsg;
			continue;
		}

		if (size > buf_size) {
			buf = realloc(buf, size);
			buf_size = size;
		}

		if (_requests->read((uint8_t*)buf, size) < size) {
			PBD::error << "Worker: Error reading body from request ring"
			           << endmsg;
			continue;  // TODO: This is probably fatal
		}

		_workee->work(size, buf);
	}
}

} // namespace ARDOUR
