/*
    Copyright (C) 2013-2014 Sebastian Reichelt

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

#ifndef __libardour_pan_delay_buffer_h__
#define __libardour_pan_delay_buffer_h__

#include <cmath>

#include "ardour/session_handle.h"
#include "ardour/pan_distribution_buffer.h"

namespace ARDOUR {

class Session;

/** Buffer to add a delay to a panned channel.
 *
 * The delay is specified in the session properties, in ms/100%, where the
 * percentage refers to the difference between the two channels (for example,
 * L60R40 means 20% in this case). Only the position is relevant, not the
 * width of the stereo panner. The delay is applied to the output channel with
 * the lower percentage. (It might be nice if the width control affected the
 * phase differences of the incoming stereo signal, but that is a different
 * topic.)
 *
 * To keep things simple, the applied delay is always an integer number of
 * frames. As long as this integer stays the same, the implementation matches
 * a regular circular buffer. (We no longer use boost::circular_buffer because
 * it does not offer a way to preallocate memory beyond its capacity.) Things
 * become more complicated whenever the delay changes, as this requires
 * non-integer interpolation between the old and new delay, to avoid minor
 * clicks in the audio.
 *
 * Note that PanDelayBufferImpl just provides the internal implementation;
 * use PanDelayBuffer instead.
 */
class LIBARDOUR_API PanDelayBufferImpl : public SessionHandleRef
{
  public:
	PanDelayBufferImpl(Session &s);
	~PanDelayBufferImpl();

	/* Updates _session_delay_coeff according to the delay specified in
	 * the session configuration. */
	void update_session_config();

	/* Updates the delay according to the given panner position. */
	void set_pan_position(float pan_position)
	{
		/* convert panner position to percentage value that is 0 if pan_position is 0.5, and 1 if pan_position is 0 */
		float const delay_percentage = std::max(std::min(1.0f - 2.0f * pan_position, 1.0f), 0.0f);

		/* calculate delay in frames */
		pframes_t new_delay = rint(delay_percentage * _session_delay_coeff);
		if (new_delay > _buffer_size) {
			new_delay = _buffer_size;
		}

		/* update _desired_delay */
		if (_desired_delay != new_delay) {
			if (_samples_processed) {
				/* set up interpolation */
				_interp_active = true;
			} else {
				/* no samples processed yet; change delay immediately */
				_current_delay = new_delay;
			}

			_desired_delay = new_delay;
		}
	}

	/* Appends the @a input sample to the delay buffer and removes and
	 * returns the oldest sample in the buffer. */
	Sample process(Sample input)
	{
		_samples_processed = true;

		Sample result;
		if (_interp_active) {
			/* interpolating between integer delays; continue in
			 * non-inlined code because this only happens for
			 * short intervals */
			result = interpolate(input);
		} else if (_desired_delay == 0) {
			/* currently bypassed */
			return input;
		} else {
			/* get the oldest sample in the buffer */
			pframes_t buffer_read_pos = _buffer_write_pos < _desired_delay ? _buffer_size + _buffer_write_pos - _desired_delay : _buffer_write_pos - _desired_delay;
			result = _buffer[buffer_read_pos];
		}

		/* write the current sample into the buffer */
		_buffer[_buffer_write_pos] = input;
		if (++_buffer_write_pos >= _buffer_size) {
			_buffer_write_pos = 0;
		}

		return result;
	}

	/* See BasePanDistributionBuffer. (Implementation is highly optimized.) */
	void mix_buffers(Sample *dst, const Sample *src, pframes_t nframes, float gain);

  private:
	/* The delay buffer, which is an array of size _buffer_size that is
	 * used as a circular buffer. */
	Sample *_buffer;

	/* Size of the _buffer array. */
	pframes_t _buffer_size;

	/* Position in the buffer where the next sample will be written.
	 * Increased by 1 for every sample, then wraps around at _buffer_size. */
	pframes_t _buffer_write_pos;

	/* Delay coefficient according to session configuration (in frames
	 * instead of ms). */
	float _session_delay_coeff;

	/* Current delay when interpolating. */
	float _current_delay;

	/* Desired delay; matches current delay if _interp_active is false. */
	pframes_t _desired_delay;

	/* Interpolation mode: See comment for _buffer. If true, _current_delay
	 * approaches _desired_delay in small steps; interpolation is finished
	 * as soon as they are equal. */
	bool _interp_active;

	/* Set to true on the first call to process() or an equivalent
	 * convenience method (and by update_session_config() if it returns
	 * false). As long as it is false, set_pan_position() sets the delay
	 * immediately without interpolation. */
	bool _samples_processed;

	/* Maximum delay, needed for memory preallocation. */
	static const float _max_delay_in_ms = 10.0f;

	/* Step size for _current_delay if _interp_active is true. */
	static const float _interp_inc = 1.0f / 16;

	/* Called by process() if _interp_active is true. */
	Sample interpolate(Sample input);
};

/** Actual pan distribution buffer class to be used by clients. */
typedef BasePanDistributionBuffer<PanDelayBufferImpl> PanDelayBuffer;

} // namespace

#endif /* __libardour_pan_delay_buffer_h__ */
