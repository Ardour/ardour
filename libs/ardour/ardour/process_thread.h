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

#ifndef __libardour_process_thread__
#define __libardour_process_thread__

#include <glibmm/threads.h>

#include "ardour/chan_count.h"
#include "ardour/libardour_visibility.h"
#include "ardour/types.h"

namespace ARDOUR {

class ThreadBuffers;
class BufferSet;

class LIBARDOUR_API ProcessThread
{
public:
	ProcessThread ();
	~ProcessThread ();

	static void init();

	void get_buffers ();
	void drop_buffers ();

	/* these MUST be called by a process thread's thread, nothing else
	 */

	static BufferSet& get_silent_buffers (ChanCount count = ChanCount::ZERO);
	static BufferSet& get_scratch_buffers (ChanCount count = ChanCount::ZERO, bool silence = false);
	static BufferSet& get_route_buffers (ChanCount count = ChanCount::ZERO, bool silence = false);
	static BufferSet& get_mix_buffers (ChanCount count = ChanCount::ZERO);
	static gain_t* gain_automation_buffer ();
	static gain_t* send_gain_automation_buffer ();
	static pan_t** pan_automation_buffer ();

protected:
	void session_going_away ();

private:
    static Glib::Threads::Private<ThreadBuffers> _private_thread_buffers;
};

} // namespace

#endif /* __libardour_process_thread__ */
