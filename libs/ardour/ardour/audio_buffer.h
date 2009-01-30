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
#include <ardour/buffer.h>

namespace ARDOUR {

class AudioBuffer : public Buffer
{
public:
	AudioBuffer(size_t capacity);
	
	~AudioBuffer();

	void silence(nframes_t len, nframes_t offset = 0) {
		if (!_silent) {
			assert(_capacity > 0);
			assert(offset + len <= _capacity);
			memset(_data + offset, 0, sizeof (Sample) * len);
			if (offset == 0 && len == _capacity) {
				_silent = true;
			}
		}
	}
	
	/** Read @a len frames FROM THE START OF @a src into self at @a offset */
	void read_from(const Buffer& src, nframes_t len, nframes_t offset) {
		assert(&src != this);
		assert(_capacity > 0);
		assert(src.type() == DataType::AUDIO);
		assert(offset + len <= _capacity);
		memcpy(_data + offset, ((AudioBuffer&)src).data(), sizeof(Sample) * len);
		_silent = src.silent();
	}
	
	/** Accumulate (add)@a len frames FROM THE START OF @a src into self at @a offset */
	void accumulate_from(const AudioBuffer& src, nframes_t len, nframes_t offset) {
		assert(_capacity > 0);
		assert(offset + len <= _capacity);

		Sample*       const dst_raw = _data + offset;
		const Sample* const src_raw = src.data();

		mix_buffers_no_gain(dst_raw, src_raw, len);

		_silent = (src.silent() && _silent);
	}
	
	/** Accumulate (add) @a len frames FROM THE START OF @a src into self at @a offset
	 * scaling by @a gain_coeff */
	void accumulate_with_gain_from(const AudioBuffer& src, nframes_t len, nframes_t offset, gain_t gain_coeff) {

		assert(_capacity > 0);
		assert(offset + len <= _capacity);

		if (src.silent()) {
			return;
		}

		Sample*       const dst_raw = _data + offset;
		const Sample* const src_raw = src.data();

		mix_buffers_with_gain (dst_raw, src_raw, len, gain_coeff);

		_silent = ( (src.silent() && _silent) || (_silent && gain_coeff == 0) );
	}

	/** Accumulate (add) @a len frames FROM THE START OF @a src into self at @a offset
	 * scaling by @a gain_coeff */
	void accumulate_with_gain_from(const Sample* src_raw, nframes_t len, nframes_t offset, gain_t gain_coeff) {

		assert(_capacity > 0);
		assert(offset + len <= _capacity);

		Sample*       const dst_raw = _data + offset;

		mix_buffers_with_gain (dst_raw, src_raw, len, gain_coeff);

		_silent = (_silent && gain_coeff == 0);
	}
	
	void apply_gain(gain_t gain, nframes_t len, nframes_t offset=0) {
		apply_gain_to_buffer (_data + offset, len, gain);
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
	}

	/** Reallocate the buffer used internally to handle at least @nframes of data
	 * 
	 * Constructor MUST have been passed capacity!=0 or this will die (to prevent mem leaks).
	 */
	void resize (size_t nframes);

	const Sample* data () const { return _data; }
	Sample* data () { return _data; }

	const Sample* data(nframes_t nframes, nframes_t offset) const
		{ assert(offset + nframes <= _capacity); return _data + offset; }

	Sample* data (nframes_t nframes, nframes_t offset)
		{ assert(offset + nframes <= _capacity); return _data + offset; }

	void replace_data (size_t nframes);

	void drop_data () {
		assert (_owns_data);
		assert (_data);

		free (_data);
		_data = 0;
		_size = 0;
		_capacity = 0;
		_silent = false;
	}

	void copy_to_internal (Sample* p, nframes_t cnt, nframes_t offset);

  private:
	bool    _owns_data;
	Sample* _data; ///< Actual buffer contents
};


} // namespace ARDOUR

#endif // __ardour_audio_audio_buffer_h__
