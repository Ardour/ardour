/*
    Copyright (C) 2013 Sebastian Reichelt

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

#include "ardour/pan_delay_buffer.h"
#include "ardour/session.h"

using namespace std;
using namespace ARDOUR;

PanDelayBufferImpl::PanDelayBufferImpl(Session &s)
	: SessionHandleRef(s)
	, _buffer(0)
	, _buffer_size(rint(_max_delay_in_ms * s.frame_rate() * 0.001f))
	, _buffer_write_pos(0)
	, _session_delay_coeff(0.0f)
	, _current_delay(0.0f)
	, _desired_delay(0)
	, _interp_active(false)
	, _samples_processed(false)
{
	_buffer = new Sample[_buffer_size];
	for (pframes_t n = 0; n < _buffer_size; n++) {
		_buffer[n] = 0.0f;
	}

	update_session_config();
}

PanDelayBufferImpl::~PanDelayBufferImpl()
{
	delete [] _buffer;
}

void
PanDelayBufferImpl::update_session_config()
{
	_session_delay_coeff = _session.config.get_panning_delay() * _session.frame_rate() * 0.001f;
}

Sample
PanDelayBufferImpl::interpolate(Sample input)
{
	/* can always decrease the current delay, so do it right away (in contrast to increasing; see below) */
	/* (use >= instead of > to avoid getting stuck in interpolation mode) */
	if (_current_delay >= _desired_delay) {
		_current_delay -= _interp_inc;
		/* check if interpolation is finished */
		if (_current_delay <= _desired_delay) {
			_current_delay = _desired_delay;
			_interp_active = false;
			/* could return here, but this is not necessary */
		}
	}

	/* check which two samples we need, and which coefficients we should apply */
	pframes_t current_delay_int = _current_delay;
	float interp_coeff = _current_delay - current_delay_int;
	pframes_t buffer_read_pos = _buffer_write_pos < current_delay_int ? _buffer_size + _buffer_write_pos - current_delay_int : _buffer_write_pos - current_delay_int;

	/* interpolate between the two samples */
	Sample const first = _buffer[buffer_read_pos == 0 ? _buffer_size - 1 : buffer_read_pos - 1];
	Sample const second = current_delay_int == 0 ? input : _buffer[buffer_read_pos];
	Sample const result = first * interp_coeff + second * (1.0f - interp_coeff);

	/* increase the current delay at the end instead of the beginning, since the buffer may not have been filled enough at first */
	if (_current_delay < _desired_delay) {
		_current_delay += _interp_inc;
		/* check if interpolation is finished */
		if (_current_delay >= _desired_delay) {
			_current_delay = _desired_delay;
			_interp_active = false;
		}
	}

	return result;
}

void
PanDelayBufferImpl::mix_buffers(Sample *dst, const Sample *src, pframes_t nframes, float gain)
{
	_samples_processed = true;

	if (_desired_delay == 0 && !_interp_active) {
		/* fast path: no delay */

		DummyPanDistributionBufferImpl::mix_buffers(dst, src, nframes, gain);
	} else {
		pframes_t n = 0;

		/* process samples normally as long as interpolation is active */
		for (; _interp_active && n < nframes; n++) {
			dst[n] += process(src[n] * gain);
		}

		/* try to bypass the buffer as much as possible */
		pframes_t bypass_start = n + _desired_delay;
		if (bypass_start < nframes) {
			/* fast path: have more samples left than the length of the delay */

			/* first output the tail of the buffer */
			pframes_t buffer_read_pos = _buffer_write_pos < _desired_delay ? _buffer_size + _buffer_write_pos - _desired_delay : _buffer_write_pos - _desired_delay;
			/* n < bypass_start implies n < nframes because bypass_start < nframes */
			for (; n < bypass_start; n++) {
				dst[n] += _buffer[buffer_read_pos];
				if (++buffer_read_pos >= _buffer_size) {
					buffer_read_pos = 0;
				}
			}

			/* then copy as many samples directly as possible */
			if (gain != 0.0f) {
				for (; n < nframes; n++) {
					/* n >= _desired_delay because n >= bypass_start */
					dst[n] += (src[n - _desired_delay] * gain);
				}
			}

			/* finally, fill the buffer with the remaining samples */
			/* n >= _desired_delay because n >= bypass_start */
			for (n -= _desired_delay; n < nframes; n++) {
				_buffer[_buffer_write_pos] = src[n] * gain;
				if (++_buffer_write_pos >= _buffer_size) {
					_buffer_write_pos = 0;
				}
			}
		} else {
			/* general case: process samples normally */
			for (; n < nframes; n++) {
				dst[n] += process(src[n] * gain);
			}
		}
	}
}
