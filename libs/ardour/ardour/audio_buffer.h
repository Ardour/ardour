/*
 * Copyright (C) 2007-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2010-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2016 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_audio_buffer_h__
#define __ardour_audio_buffer_h__

#include <cstring>

#include "ardour/buffer.h"
#include "ardour/runtime_functions.h"

namespace ARDOUR
{
/** Buffer containing audio data. */
class LIBARDOUR_API AudioBuffer : public Buffer
{
public:
	AudioBuffer (size_t capacity);
	~AudioBuffer ();

	/** silence buffer
	 * @param len number of samples to clear
	 * @param offset start offset
	 */
	void silence (samplecnt_t len, samplecnt_t offset = 0);

	/** Copy samples from src array starting at src_offset into self starting at dst_offset
	 * @param src array to read from
	 * @param len number of samples to copy
	 * @param dst_offset offset in destination buffer
	 * @param src_offset start offset in src buffer
	 */
	void read_from (const Sample* src, samplecnt_t len, sampleoffset_t dst_offset = 0, sampleoffset_t src_offset = 0)
	{
		assert (src != 0);
		assert (_capacity > 0);
		assert (len <= _capacity);
		copy_vector (_data + dst_offset, src + src_offset, len);
		_silent  = false;
		_written = true;
	}

	/** Copy samples from src buffer starting at src_offset into self starting at dst_offset
	 * @param src buffer to read from
	 * @param len number of samples to copy
	 * @param dst_offset offset in destination buffer
	 * @param src_offset start offset in src buffer
	 */
	void read_from (const Buffer& src, samplecnt_t len, sampleoffset_t dst_offset = 0, sampleoffset_t src_offset = 0)
	{
		assert (&src != this);
		assert (_capacity > 0);
		assert (src.type () == DataType::AUDIO);
		assert (dst_offset + len <= _capacity);
		assert (src_offset <= ((samplecnt_t)src.capacity () - len));

		if (src.silent ()) {
			memset (_data + dst_offset, 0, sizeof (Sample) * len);
		} else {
			copy_vector (_data + dst_offset, ((const AudioBuffer&)src).data () + src_offset, len);
		}

		if (dst_offset == 0 && src_offset == 0 && len == _capacity) {
			_silent = src.silent ();
		} else {
			_silent = _silent && src.silent ();
		}
		_written = true;
	}

	/** Accumulate (add) \p len samples from \p src starting at \p src_offset into self starting at \p dst_offset */
	void merge_from (const Buffer& src, samplecnt_t len, sampleoffset_t dst_offset = 0, sampleoffset_t src_offset = 0)
	{
		const AudioBuffer* ab = dynamic_cast<const AudioBuffer*> (&src);
		assert (ab);
		accumulate_from (*ab, len, dst_offset, src_offset);
	}

	/** Accumulate (add) \p len samples from \p src starting at \p src_offset into self starting at \p dst_offset */
	void accumulate_from (const AudioBuffer& src, samplecnt_t len, sampleoffset_t dst_offset = 0, sampleoffset_t src_offset = 0)
	{
		assert (_capacity > 0);
		assert (len <= _capacity);
		if (src.silent ()) {
			return;
		}
		Sample* const       dst_raw = _data + dst_offset;
		const Sample* const src_raw = src.data () + src_offset;

		mix_buffers_no_gain (dst_raw, src_raw, len);

		_silent  = (src.silent () && _silent);
		_written = true;
	}

	/** Accumulate (add) \p len samples of \p src starting at \p src_offset into self
	 * starting at \p dst_offset
	 */
	void accumulate_from (const Sample* src, samplecnt_t len, sampleoffset_t dst_offset = 0, sampleoffset_t src_offset = 0)
	{
		assert (_capacity > 0);
		assert (len <= _capacity);

		Sample* const       dst_raw = _data + dst_offset;
		const Sample* const src_raw = src + src_offset;

		mix_buffers_no_gain (dst_raw, src_raw, len);

		_silent  = false;
		_written = true;
	}

	/** Accumulate (add) \p len samples if \p src starting at \p src_offset into self
	 * starting at \p dst_offset scaling by \p gain_coeff
	 */
	void accumulate_with_gain_from (const AudioBuffer& src, samplecnt_t len, gain_t gain_coeff, sampleoffset_t dst_offset = 0, sampleoffset_t src_offset = 0)
	{
		assert (_capacity > 0);
		assert (len <= _capacity);

		if (src.silent () || gain_coeff == 0) {
			return;
		}

		Sample* const       dst_raw = _data + dst_offset;
		const Sample* const src_raw = src.data () + src_offset;

		mix_buffers_with_gain (dst_raw, src_raw, len, gain_coeff);

		_silent  = ((src.silent () && _silent) || (_silent && gain_coeff == 0));
		_written = true;
	}

	/** Accumulate (add) \p len samples from the start of \p src_raw into self at \p dst_offset
	 * scaling by \p gain_coeff
	 */
	void accumulate_with_gain_from (const Sample* src_raw, samplecnt_t len, gain_t gain_coeff, sampleoffset_t dst_offset = 0)
	{
		assert (_capacity > 0);
		assert (len <= _capacity);

		Sample* const dst_raw = _data + dst_offset;

		mix_buffers_with_gain (dst_raw, src_raw, len, gain_coeff);

		_silent  = (_silent && gain_coeff == 0);
		_written = true;
	}

	/** Accumulate (add) \p len samples from the start of \p src into self at \p dst_offset
	 * using a linear gain ramp from \p initial to \p target .
	 */
	void accumulate_with_ramped_gain_from (const Sample* src, samplecnt_t len, gain_t initial, gain_t target, sampleoffset_t dst_offset = 0)
	{
		assert (_capacity > 0);
		assert (len <= _capacity);

		if (initial == 0 && target == 0) {
			return;
		}

		Sample* dst        = _data + dst_offset;
		gain_t  gain_delta = (target - initial) / len;

		for (samplecnt_t n = 0; n < len; ++n) {
			*dst++ += (*src++ * initial);
			initial += gain_delta;
		}

		_silent  = (_silent && initial == 0 && target == 0);
		_written = true;
	}

	/** Apply a fixed gain factor to the audio buffer
	 *
	 * @param gain gain factor
	 * @param len number of samples to amplify
	 */
	void apply_gain (gain_t gain, samplecnt_t len)
	{
		if (gain == 0) {
			memset (_data, 0, sizeof (Sample) * len);
			if (len == _capacity) {
				_silent = true;
			}
			return;
		}
		apply_gain_to_buffer (_data, len, gain);
	}

	/** Set the data contained by this buffer manually (for setting directly to jack buffer).
	 *
	 * Constructor MUST have been passed capacity=0 or this will die (to prevent mem leaks).
	 */
	void set_data (Sample* data, size_t size)
	{
		assert (!_owns_data); // prevent leaks
		_capacity = size;
		_data     = data;
		_silent   = false;
		_written  = false;
	}

	/** Reallocate the buffer used internally to handle at least \p nframes of data
	 *
	 * Constructor MUST have been passed capacity!=0 or this will die (to prevent mem leaks).
	 */
	void resize (size_t nframes);

	const Sample* data (samplecnt_t offset = 0) const
	{
		assert (offset <= _capacity);
		return _data + offset;
	}

	Sample* data (samplecnt_t offset = 0)
	{
		assert (offset <= _capacity);
		_silent = false;
		return _data + offset;
	}

	/** Check buffer for silence
	 *
	 * @param nframes  number of samples to check
	 * @param n first non zero sample (if any)
	 * @return true if all samples are zero
	 */
	bool check_silence (pframes_t nframes, pframes_t& n) const;

	void prepare ()
	{
		if (!_owns_data) {
			_data = 0;
		}
		_written = false;
		_silent  = false;
	}

	bool written () const { return _written; }
	void set_written (bool w) { _written = w; }

private:
	bool    _owns_data;
	bool    _written;
	Sample* _data; ///< Actual buffer contents
};

} // namespace ARDOUR

#endif // __ardour_audio_audio_buffer_h__
