/*
 * Copyright (C) 2010-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2010-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2013-2017 Robin Gareus <robin@gareus.org>
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

#ifndef __libardour_thread_buffers__
#define __libardour_thread_buffers__

#include <glibmm/threads.h>

#include "ardour/chan_count.h"
#include "ardour/libardour_visibility.h"
#include "ardour/types.h"

namespace ARDOUR {

class BufferSet;

class LIBARDOUR_API ThreadBuffers {
public:
	ThreadBuffers ();
	~ThreadBuffers ();

	void ensure_buffers (ChanCount howmany = ChanCount::ZERO, size_t custom = 0);

	BufferSet* silent_buffers;
	BufferSet* scratch_buffers;
	BufferSet* noinplace_buffers;
	BufferSet* route_buffers;
	BufferSet* mix_buffers;
	gain_t*    gain_automation_buffer;
	gain_t*    trim_automation_buffer;
	gain_t*    send_gain_automation_buffer;
	gain_t*    scratch_automation_buffer;
	pan_t**    pan_automation_buffer;
	uint32_t   npan_buffers;

private:
	void allocate_pan_automation_buffers (samplecnt_t nframes, uint32_t howmany, bool force);
};

} // namespace

#endif /* __libardour_thread_buffers__ */
