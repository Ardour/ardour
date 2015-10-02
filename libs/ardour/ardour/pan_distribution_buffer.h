/*
    Copyright (C) 2014 Sebastian Reichelt

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

#ifndef __libardour_pan_distribution_buffer_h__
#define __libardour_pan_distribution_buffer_h__

#include "ardour/types.h"

namespace ARDOUR {

class Session;

/* Maximum number of frames to interpolate between gains (used by
 * BasePanDistributionBuffer::mix_buffers(); must be a multiple of 16). */
static const pframes_t _gain_interp_frames = 64;

/** Helper class for panners to manage distribution of signals to outputs.
 *
 * BasePanDistributionBuffer can be regarded as a "base class" of
 * DummyPanDistributionBuffer and PanDelayBuffer. In reality, these
 * "subclasses" are really typedefs that supply the inner implementation.
 * The methods of BasePanDistributionBuffer define a compile-time interface;
 * actual "subclass" to use must be selected using a template parameter or
 * preprocessor macro. The reason is that set_pan_position() and process()
 * are called in a tight loop and must not cause any unnecessary overhead.
 *
 * Clients should call update_session_config() whenever the session
 * configuration might have changed, then set_pan_position() whenever the
 * position of the panner might have changed, and then process() for every
 * sample. For convenience and performance, the helper method mix_buffers()
 * can be used instead if the panner position stays constant.
 *
 * For more information, see pan_delay_buffer.h.
 */
template <class Impl>
class BasePanDistributionBuffer
{
  public:
	BasePanDistributionBuffer(Session& session) : _impl(session) {}

	/** Updates internal data according to the session configuration. */
	void update_session_config() { _impl.update_session_config(); }

	/** Updates internal data according to the given panner position.
	 *
	 * @a pan_position should be a value between 0 and 1, and should not
	 * be a gain value that has been calculated according to the pan law.
	 * For a stereo output, the @a pan_position values of the left and
	 * right channel should sum to 1. */
	void set_pan_position(float pan_position) { _impl.set_pan_position(pan_position); }

	/** Processes one sample, and returns the sample that should actually
	 *  be output. */
	Sample process(Sample input) { return _impl.process(input); }

	/** Same as calling process() for each sample in @a src multiplied by
	 *  @a gain, and adding the result to @a dst. However, if @a prev_gain
	 *  is different from @a gain, interpolates between gains for the
	 *  first 64 samples.
	 *
	 * In simple cases, this is implemented using mix_buffers_no_gain() and
	 * mix_buffers_with_gain() from runtime_functions.h. */
	void mix_buffers(Sample *dst, const Sample *src, pframes_t nframes, float prev_gain, float gain)
	{
		if (nframes == 0) {
			return;
		}

		if (gain == prev_gain) {
			_impl.mix_buffers(dst, src, nframes, gain);
		} else {
			/* gain has changed, so we must interpolate over 64 frames or nframes, whichever is smaller */
			/* (code adapted from panner_1in2out.cc and panner_2in2out.cc) */

			pframes_t const limit = std::min(_gain_interp_frames, nframes);
			float const delta = (gain - prev_gain) / limit;
			float current_gain = prev_gain;
			pframes_t n = 0;

			for (; n < limit; n++) {
				prev_gain += delta;
				current_gain = prev_gain + 0.9 * (current_gain - prev_gain);
				dst[n] += _impl.process(src[n] * current_gain);
			}

			if (n < nframes) {
				_impl.mix_buffers(dst + n, src + n, nframes - n, gain);
			}
		}
	}

  private:
	Impl _impl;
};

/** Internal class used by DummyPanDistributionBuffer; do not reference directly. */
class LIBARDOUR_API DummyPanDistributionBufferImpl
{
  public:
	DummyPanDistributionBufferImpl(Session& session) {}

	static void update_session_config() {}
	static void set_pan_position(float pan_position) {}
	static Sample process(Sample input) { return input; }

	static void mix_buffers(Sample *dst, const Sample *src, pframes_t nframes, float gain);
};

/** Dummy "distribtion buffer" which just forwards the samples. */
typedef BasePanDistributionBuffer<DummyPanDistributionBufferImpl> DummyPanDistributionBuffer;

} // namespace

#endif /* __libardour_pan_distribution_buffer_h__ */
