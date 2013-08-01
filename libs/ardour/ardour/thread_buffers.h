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

#ifndef __libardour_thread_buffers__
#define __libardour_thread_buffers__

#include <glibmm/threads.h>

#include "ardour/chan_count.h"
#include "ardour/types.h"

namespace ARDOUR {

class BufferSet;

class ThreadBuffers {
public:
	ThreadBuffers ();
	~ThreadBuffers ();

	void ensure_buffers (ChanCount howmany = ChanCount::ZERO);

	BufferSet* silent_buffers;
	BufferSet* scratch_buffers;
	BufferSet* route_buffers;
	BufferSet* mix_buffers;
	gain_t*    gain_automation_buffer;
	gain_t*    send_gain_automation_buffer;
	pan_t**    pan_automation_buffer;
	uint32_t   npan_buffers;

private:
	void allocate_pan_automation_buffers (framecnt_t nframes, uint32_t howmany, bool force);
};

} // namespace

#endif /* __libardour_thread_buffers__ */
