/*
    Copyright (C) 2012 Paul Davis 

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

#ifndef __libardour_buffer_manager__
#define __libardour_buffer_manager__

#include <stdint.h>

#include "pbd/ringbufferNPT.h"

#include "ardour/chan_count.h"
#include <list>
#include <glibmm/threads.h>

namespace ARDOUR {

class ThreadBuffers;

class BufferManager
{
public:
	static void init (uint32_t);

	static ThreadBuffers* get_thread_buffers ();
	static void           put_thread_buffers (ThreadBuffers*);

	static void ensure_buffers (ChanCount howmany = ChanCount::ZERO);

private:
        static Glib::Threads::Mutex rb_mutex;

	typedef PBD::RingBufferNPT<ThreadBuffers*> ThreadBufferFIFO;
	typedef std::list<ThreadBuffers*> ThreadBufferList;

	static ThreadBufferFIFO* thread_buffers;
	static ThreadBufferList* thread_buffers_list;
};

}

#endif /* __libardour_buffer_manager__ */
