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

#include "ardour/buffer.h"
#include "ardour/buffer_manager.h"
#include "ardour/buffer_set.h"
#include "ardour/process_thread.h"
#include "ardour/thread_buffers.h"

using namespace ARDOUR;
using namespace Glib;
using namespace std;

static void
release_thread_buffer (void* arg)
{
        BufferManager::put_thread_buffers ((ThreadBuffers*) arg);
}

Glib::Threads::Private<ThreadBuffers> ProcessThread::_private_thread_buffers (release_thread_buffer);

void
ProcessThread::init ()
{
}

ProcessThread::ProcessThread ()
{
}

ProcessThread::~ProcessThread ()
{
}

void
ProcessThread::get_buffers ()
{
        ThreadBuffers* tb = BufferManager::get_thread_buffers ();

        assert (tb);
        _private_thread_buffers.set (tb);
}

void
ProcessThread::drop_buffers ()
{
        ThreadBuffers* tb = _private_thread_buffers.get();
        assert (tb);
        BufferManager::put_thread_buffers (tb);
        _private_thread_buffers.set (0);
}

BufferSet&
ProcessThread::get_silent_buffers (ChanCount count)
{
        ThreadBuffers* tb = _private_thread_buffers.get();
        assert (tb);

        BufferSet* sb = tb->silent_buffers;
        assert (sb);

	assert(sb->available() >= count);
	sb->set_count(count);

	for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
		for (size_t i= 0; i < count.get(*t); ++i) {
			sb->get(*t, i).clear();
		}
	}

	return *sb;
}

BufferSet&
ProcessThread::get_scratch_buffers (ChanCount count)
{
        ThreadBuffers* tb = _private_thread_buffers.get();
        assert (tb);

        BufferSet* sb = tb->scratch_buffers;
        assert (sb);

	if (count != ChanCount::ZERO) {
		assert(sb->available() >= count);
		sb->set_count (count);
	} else {
		sb->set_count (sb->available());
	}

	return *sb;
}

BufferSet&
ProcessThread::get_route_buffers (ChanCount count, bool silence)
{
	ThreadBuffers* tb = _private_thread_buffers.get();
	assert (tb);

	BufferSet* sb = tb->route_buffers;
	assert (sb);

	if (count != ChanCount::ZERO) {
		assert(sb->available() >= count);
		sb->set_count (count);
	} else {
		sb->set_count (sb->available());
	}

	if (silence) {
		for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
			for (uint32_t i = 0; i < sb->count().get(*t); ++i) {
				sb->get(*t, i).clear();
			}
		}
	}

	return *sb;
}

BufferSet&
ProcessThread::get_mix_buffers (ChanCount count)
{
        ThreadBuffers* tb = _private_thread_buffers.get();
        assert (tb);

        BufferSet* mb = tb->mix_buffers;

        assert (mb);
	assert (mb->available() >= count);
	mb->set_count(count);
	return *mb;
}

gain_t*
ProcessThread::gain_automation_buffer()
{
        ThreadBuffers* tb = _private_thread_buffers.get();
        assert (tb);

        gain_t *g =  tb->gain_automation_buffer;
        assert (g);
        return g;
}

gain_t*
ProcessThread::send_gain_automation_buffer()
{
        ThreadBuffers* tb = _private_thread_buffers.get();
        assert (tb);

        gain_t* g = tb->send_gain_automation_buffer;
        assert (g);
        return g;
}

pan_t**
ProcessThread::pan_automation_buffer()
{
        ThreadBuffers* tb = _private_thread_buffers.get();
        assert (tb);

        pan_t** p = tb->pan_automation_buffer;
        assert (p);
        return p;
}
