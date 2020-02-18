/*
 * Copyright (C) 2010-2012 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2011-2012 David Robillard <d@drobilla.net>
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

#include <iostream>

#include "pbd/compose.h"

#include "ardour/buffer_manager.h"
#include "ardour/thread_buffers.h"

using namespace ARDOUR;
using namespace PBD;

RingBufferNPT<ThreadBuffers*>* BufferManager::thread_buffers = 0;
std::list<ThreadBuffers*>* BufferManager::thread_buffers_list = 0;
Glib::Threads::Mutex BufferManager::rb_mutex;

using std::cerr;
using std::endl;

void
BufferManager::init (uint32_t size)
{
        thread_buffers = new ThreadBufferFIFO (size+1); // must be one larger than requested
	thread_buffers_list = new ThreadBufferList;

        /* and populate with actual ThreadBuffers
         */

        for (uint32_t n = 0; n < size; ++n) {
                ThreadBuffers* ts = new ThreadBuffers;
                thread_buffers->write (&ts, 1);
		thread_buffers_list->push_back (ts);
        }
	// cerr << "Initialized thread buffers, readable count now " << thread_buffers->read_space() << endl;

}

ThreadBuffers*
BufferManager::get_thread_buffers ()
{
	Glib::Threads::Mutex::Lock em (rb_mutex);
        ThreadBuffers* tbp;

        if (thread_buffers->read (&tbp, 1) == 1) {
		// cerr << "Got thread buffers, readable count now " << thread_buffers->read_space() << endl;
                return tbp;
        }

        return 0;
}

void
BufferManager::put_thread_buffers (ThreadBuffers* tbp)
{
	Glib::Threads::Mutex::Lock em (rb_mutex);
        thread_buffers->write (&tbp, 1);
	// cerr << "Put back thread buffers, readable count now " << thread_buffers->read_space() << endl;
}

void
BufferManager::ensure_buffers (ChanCount howmany, size_t custom)
{
        /* this is protected by the audioengine's process lock: we do not  */

	for (ThreadBufferList::iterator i = thread_buffers_list->begin(); i != thread_buffers_list->end(); ++i) {
		(*i)->ensure_buffers (howmany, custom);
	}
}
