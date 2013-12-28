/*
    Copyright (C) 2006 Paul Davis

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __ardour_audio_buffer_h__
#define __ardour_audio_buffer_h__

#include <cstring>

#include "ardour/buffer.h"
#include "ardour/runtime_functions.h"

namespace ARDOUR {

/** Buffer containing audio data. */
class LIBARDOUR_API AudioBuffer : public Buffer
{
public:
	AudioBuffer(size_t capacity);
	~AudioBuffer();

	void silence (framecnt_t len, framecnt_t offset = 0);

	/** Read @a len frames @a src starting at @a src_offset into self starting at @ dst_offset*/
	void read_from (const Sample* src, framecnt_t len, framecnt_t dst_offset = 0, framecnt_t src_offset = 0) {
		assert(src != 0);
		assert(_capacity > 0);
		assert(len <= _capacity);
		memcpy(_data + dst_offset, src + src_offset, sizeof(Sample) * len);
		_silent = false;
		_written = true;
	}

        void read_from_with_gain (const Sample* src, framecnt_t len, gain_t gain, framecnt_t dst_offset = 0, framecnt_t src_offset = 0) {
		assert(src != 0);
		assert(_capacity > 0);
		assert(len <= _capacity);
		src += src_offset;
		for (framecnt_t n = 0; n < len; ++n) {
			_data[dst_offset+n] = src[n] * gain;
		}
		_silent = false;
		_written = true;
	}

	/** Read @a len frames @a src starting at @a src_offset into self starting at @ dst_offset*/
	void read_from (const Buffer& src, framecnt_t len, framecnt_t dst_offset = 0, framecnt_t src_offset = 0) {
		assert(&src != this);
		assert(_capacity > 0);
		assert(src.type() == DataType::AUDIO);
		assert(len <= _capacity);
		assert( src_offset <= ((framecnt_t) src.capacity()-len));
		memcpy(_data + dst_offset, ((const AudioBuffer&)src).data() + src_offset, sizeof(Sample) * len);
		if (dst_offset == 0 && src_offset == 0 && len == _capacity) {
			_silent = src.silent();
		} else {
			_silent = _silent && src.silent();
		}
		_written = true;
	}

	/** Acumulate (add) @a len frames @a src starting at @a src_offset into self starting at @a dst_offset */
	void merge_from (const Buffer& src, framecnt_t len, framecnt_t dst_offset = 0, framecnt_t src_offset = 0) {
		const AudioBuffer* ab = dynamic_cast<const AudioBuffer*>(&src);
		assert (ab);
		accumulate_from (*ab, len, dst_offset, src_offset);
	}

	/** Acumulate (add) @a len frames @a src starting at @a src_offset into self starting at @a dst_offset */
	void accumulate_from (const AudioBuffer& src, framecnt_t len, framecnt_t dst_offset = 0, framecnt_t src_offset = 0) {
		assert(_capacity > 0);
		assert(len <= _capacity);

		Sample*       const dst_raw = _data + dst_offset;
		const Sample* const src_raw = src.data() + src_offset;

		mix_buffers_no_gain(dst_raw, src_raw, len);

		_silent = (src.silent() && _silent);
		_written = true;
	}

	/** Acumulate (add) @a len frames @a src starting at @a src_offset into self starting at @a dst_offset */
	void accumulate_from (const Sample* src, framecnt_t len, framecnt_t dst_offset = 0, framecnt_t src_offset = 0) {
		assert(_capacity > 0);
		assert(len <= _capacity);

		Sample*       const dst_raw = _data + dst_offset;
		const Sample* const src_raw = src + src_offset;

		mix_buffers_no_gain(dst_raw, src_raw, len);
		
		_silent = false;
		_written = true;
	}

	/** Acumulate (add) @a len frames @a src starting at @a src_offset into self starting at @dst_offset
	 * scaling by @a gain_coeff */
	void accumulate_with_gain_from (const AudioBuffer& src, framecnt_t len, gain_t gain_coeff, framecnt_t dst_offset = 0, framecnt_t src_offset = 0) {

		assert(_capacity > 0);
		assert(len <= _capacity);

		if (src.silent()) {
			return;
		}

		Sample*       const dst_raw = _data + dst_offset;
		const Sample* const src_raw = src.data() + src_offset;

		mix_buffers_with_gain (dst_raw, src_raw, len, gain_coeff);

		_silent = ( (src.silent() && _silent) || (_silent && gain_coeff == 0) );
		_written = true;
	}

	/** Accumulate (add) @a len frames FROM THE START OF @a src into self
	 * scaling by @a gain_coeff */
	void accumulate_with_gain_from (const Sample* src_raw, framecnt_t len, gain_t gain_coeff, framecnt_t dst_offset = 0) {

		assert(_capacity > 0);
		assert(len <= _capacity);

		Sample*       const dst_raw = _data + dst_offset;

		mix_buffers_with_gain (dst_raw, src_raw, len, gain_coeff);

		_silent = (_silent && gain_coeff == 0);
		_written = true;
	}

	/** Accumulate (add) @a len frames FROM THE START OF @a src into self
	 * scaling by @a gain_coeff */
	void accumulate_with_ramped_gain_from (const Sample* src, framecnt_t len, gain_t initial, gain_t target, framecnt_t dst_offset = 0) {

		assert(_capacity > 0);
		assert(len <= _capacity);

		Sample* dst = _data + dst_offset;
		gain_t  gain_delta = (target - initial)/len;

		for (framecnt_t n = 0; n < len; ++n) {
			*dst++ += (*src++ * initial);
			initial += gain_delta;
		}

		_silent = (_silent && initial == 0 && target == 0);
		_written = true;
	}

	void apply_gain (gain_t gain, framecnt_t len) {
		apply_gain_to_buffer (_data, len, gain);
	}

	/** Set the data contained by this buffer manually (for setting directly to jack buffer).
	 *
	 * Constructor MUST have been passed capacity=0 or this will die (to prevent mem leaks).
	 */
	void set_data (Sample* data, size_t size) {
		assert(!_owns_data); // prevent leaks
		_capacity = size;
		_size = size;
		_data = data;
		_silent = false;
		_written = false;
	}

	/** Reallocate the buffer used internally to handle at least @nframes of data
	 *
	 * Constructor MUST have been passed capacity!=0 or this will die (to prevent mem leaks).
	 */
	void resize (size_t nframes);

	bool empty() const { return _size == 0; }

	const Sample* data (framecnt_t offset = 0) const {
		assert(offset <= _capacity);
		return _data + offset;
	}

	Sample* data (framecnt_t offset = 0) {
		assert(offset <= _capacity);
		_silent = false;
		return _data + offset;
	}

	bool check_silence (pframes_t, bool, pframes_t&) const;

	void prepare () { _written = false; _silent = false; }
	bool written() const { return _written; }
	void set_written(bool w) { _written = w; }

  private:
	bool    _owns_data;
	bool    _written;
	Sample* _data; ///< Actual buffer contents
};


} // namespace ARDOUR

#endif // __ardour_audio_audio_buffer_h__
