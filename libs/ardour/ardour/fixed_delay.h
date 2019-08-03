/*
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_fixed_delay_h__
#define __ardour_fixed_delay_h__

#include <vector>
#include "ardour/buffer.h"

namespace ARDOUR {

class ChanCount;

/** Multichannel Audio/Midi Delay Line
 *
 * This is an efficient delay line operating directly on Ardour buffers.
 * The drawback is that there is no thread safety:
 * All calls need to be executed in the same thread.
 *
 * After configuration, the delay can be changed safely up to the maximum
 * configured delay but doing so flushes the buffer. There is no de-clicking
 * (see ARDOUR::Delayline for those cases).
 *
 * Increasing the delay above the max configured or requesting more
 * buffers will allocate the required space (not realtime safe).
 *
 * All buffers part of the set are treated separately.
 */
class LIBARDOUR_API FixedDelay
{
public:
	FixedDelay ();
	~FixedDelay ();

	/** initial configuration, usually done after instantiation
	 *
	 * @param count Channel Count (audio+midi)
	 * @param max_delay the maximum number of samples to delay
	 * @param shrink when false already allocated buffers are kept if both channel-count and max-delay requirements are satisified
	 */
	void configure (const ChanCount& count, samplecnt_t max_delay, bool shrink = true);

	/** set delay time and update active process buffers
	 *
	 * This calls configure with shrink = false and sets the current delay time
	 * if the delay time mismatches, the buffers are silenced (zeroed).
	 *
	 * @param count channels to be processed
	 * @param delay number of audio samples to delay
	 */
	void set (const ChanCount& count, samplecnt_t delay);

	/** process a channel
	 *
	 * Read N samples from the input buffer, delay them by the configured delay-time and write
	 * the delayed samples to the output buffer at the given offset.
	 *
	 * @param dt datatype
	 * @param id buffer number (starting at 0)
	 * @param out output buffer to write data to
	 * @param in input buffer to read data from
	 * @param n_samples number of samples to process (must be <= 8192)
	 * @param dst_offset offset in output buffer to start writing to
	 * @param src_offset offset in input buffer to start reading from
	 */
	void delay (ARDOUR::DataType dt, uint32_t id, Buffer& out, const Buffer& in, pframes_t n_samples, samplecnt_t dst_offset = 0, samplecnt_t src_offset = 0);

	/** zero all buffers */
	void flush();

private:
	samplecnt_t _max_delay;
	samplecnt_t _buf_size;
	samplecnt_t _delay;
	ChanCount  _count;

	struct DelayBuffer {
		public:
		DelayBuffer () : buf (0), pos (0) {}
		DelayBuffer (DataType dt, size_t capacity)
			: buf (Buffer::create (dt, capacity)), pos (0) {}
		~DelayBuffer () { delete buf; }
		Buffer * buf;
		samplepos_t pos;
	};

	typedef std::vector<DelayBuffer*> BufferVec;
	// Vector of vectors, indexed by DataType
	std::vector<BufferVec> _buffers;

	void ensure_buffers(DataType type, size_t num_buffers, size_t buffer_capacity);
	void clear ();
};

} // namespace ARDOUR

#endif // __ardour_fixed_delay_h__
