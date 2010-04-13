/*
    Copyright (C) 2010 Paul Davis

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

#include <iostream>
#include "ardour/buffer_manager.h"
#include "ardour/thread_buffers.h"

using namespace ARDOUR;
using namespace PBD;

RingBufferNPT<ThreadBuffers*>* BufferManager::thread_buffers = 0;

void
BufferManager::init (uint32_t size)
{
        thread_buffers = new ThreadBufferFIFO (size+1); // must be one larger than requested

        /* and populate with actual ThreadBuffers 
         */

        for (uint32_t n = 0; n < size; ++n) {        
                ThreadBuffers* ts = new ThreadBuffers;
                thread_buffers->write (&ts, 1);
        }
}

ThreadBuffers*
BufferManager::get_thread_buffers ()
{
        ThreadBuffers* tbp;

        if (thread_buffers->read (&tbp, 1) == 1) {
                return tbp;
        }

        return 0;
}

void
BufferManager::put_thread_buffers (ThreadBuffers* tbp)
{
        thread_buffers->write (&tbp, 1);
}

void
BufferManager::ensure_buffers (ChanCount howmany)
{
        /* this is protected by the audioengine's process lock: we do not  */

        for (uint32_t n = 0; n < thread_buffers->bufsize() - 1; ++n) {
                thread_buffers->buffer()[n]->ensure_buffers (howmany);
        }
}
