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

#ifndef __ardour_worker_h__
#define __ardour_worker_h__

#include <stdint.h>

#include <glibmm/thread.h>

#include "pbd/ringbuffer.h"
#include "pbd/semaphore.h"

namespace ARDOUR {

/**
   An object that needs to schedule non-RT work in the audio thread.
*/
class Workee {
public:
	virtual ~Workee() {}

	/**
	   Do some work in the worker thread.
	*/
	virtual void work(uint32_t size, const void* data) = 0;

	/**
	   Handle a response from the worker thread in the audio thread.
	*/
	virtual void work_response(uint32_t size, const void* data) = 0;
};

/**
   A worker thread for non-realtime tasks scheduled in the audio thread.
*/
class Worker
{
public:
	Worker(Workee* workee, uint32_t ring_size);
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

private:
	void run();

	Workee*              _workee;
	Glib::Thread*        _thread;
	RingBuffer<uint8_t>* _requests;
	RingBuffer<uint8_t>* _responses;
	uint8_t*             _response;
	PBD::Semaphore       _sem;
	bool                 _exit;
};

} // namespace ARDOUR

#endif  /* __ardour_worker_h__ */
