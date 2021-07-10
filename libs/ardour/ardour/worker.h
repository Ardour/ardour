/*
 * Copyright (C) 2012-2013 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2012-2016 David Robillard <d@drobilla.net>
 * Copyright (C) 2012-2017 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_worker_h__
#define __ardour_worker_h__

#include <stdint.h>

#include <glibmm/threads.h>

#include "pbd/ringbuffer.h"
#include "pbd/semutils.h"

#include "ardour/libardour_visibility.h"

namespace ARDOUR {

class Worker;

/**
   An object that needs to schedule non-RT work in the audio thread.
*/
class LIBARDOUR_API Workee {
public:
	virtual ~Workee() {}

	/**
	   Do some work in the worker thread.
	*/
	virtual int work(Worker& worker, uint32_t size, const void* data) = 0;

	/**
	   Handle a response from the worker thread in the audio thread.
	*/
	virtual int work_response(uint32_t size, const void* data) = 0;
};

/**
   A worker for non-realtime tasks scheduled from another thread.

   A worker may be a separate thread that runs to execute scheduled work
   asynchronously, or unthreaded, in which case work is executed immediately
   upon scheduling by the calling thread.
*/
class LIBARDOUR_API Worker
{
public:
	Worker(Workee* workee, uint32_t ring_size, bool threaded=true);
	~Worker();

	/**
	   Schedule work (audio thread).
	   @return false on error.
	*/
	bool schedule(uint32_t size, const void* data);

	/**
	   Respond from work (worker thread).
	   @return false on error.
	*/
	bool respond(uint32_t size, const void* data);

	/**
	   Emit any pending responses (audio thread).
	*/
	void emit_responses();

	/**
	   Enable or disable synchronous execution.

	   If enabled, all work is performed immediately in schedule() regardless
	   of whether or not the worker is threaded.  This is used for exporting,
	   where we want to temporarily execute all work synchronously but the
	   worker is typically used threaded for live rolling.
	*/
	void set_synchronous(bool synchronous) { _synchronous = synchronous; }

private:
	void run();
	/**
	   Peek in RB, get size and check if a block of 'size' is available.

	   Handle the unlikely edge-case, if we're called in between the
	   responder writing 'size' and 'data'.

	   @param rb the ringbuffer to check
	   @return true if the message is complete, false otherwise
	*/
	bool verify_message_completeness(PBD::RingBuffer<uint8_t>* rb);

	Workee*                   _workee;
	PBD::RingBuffer<uint8_t>* _requests;
	PBD::RingBuffer<uint8_t>* _responses;
	uint8_t*                  _response;
	PBD::Semaphore            _sem;
	Glib::Threads::Thread*    _thread;
	bool                      _exit;
	bool                      _synchronous;
};

} // namespace ARDOUR

#endif  /* __ardour_worker_h__ */
