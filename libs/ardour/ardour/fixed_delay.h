/*
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __ardour_fixed_delay_h__
#define __ardour_fixed_delay_h__

#include <vector>
#include "ardour/buffer.h"

namespace ARDOUR {

class ChanCount;

class LIBARDOUR_API FixedDelay
{
public:
	FixedDelay ();
	~FixedDelay ();

	void configure (const ChanCount& count, framecnt_t);
	void set (const ChanCount& count, framecnt_t);

	void delay (ARDOUR::DataType, uint32_t, Buffer&, const Buffer&, pframes_t, framecnt_t dst_offset = 0, framecnt_t src_offset = 0);
	void flush() { _pending_flush = true; }

private:
	framecnt_t _max_delay;
	framecnt_t _delay;
	bool       _pending_flush;
	ChanCount  _count;

	struct DelayBuffer {
		public:
		DelayBuffer () : buf (0), pos (0) {}
		DelayBuffer (DataType dt, size_t capacity)
			: buf (Buffer::create (dt, capacity)), pos (0) {}
		~DelayBuffer () { delete buf; }
		Buffer * buf;
		framepos_t pos;
	};

	typedef std::vector<DelayBuffer*> BufferVec;
	// Vector of vectors, indexed by DataType
	std::vector<BufferVec> _buffers;

	void ensure_buffers(DataType type, size_t num_buffers, size_t buffer_capacity);
	void clear ();
};

} // namespace ARDOUR

#endif // __ardour_fixed_delay_h__
