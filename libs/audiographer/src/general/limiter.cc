/*
 * Copyright (C) 2021 Robin Gareus <robin@gareus.org>
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

#include "audiographer/general/limiter.h"

namespace AudioGrapher
{

Limiter::Limiter (float sample_rate, unsigned int channels, samplecnt_t size)
	: _enabled (false)
	, _buf (0)
	, _size (0)
	, _cnt (0)
	, _spp (0)
	, _pos (0)
	{

	_limiter.init (sample_rate, channels);
	_limiter.set_truepeak (true);
	_limiter.set_inpgain (0);
	_limiter.set_threshold (-1);
	_limiter.set_release (0.01);

	_latency = _limiter.get_latency ();
	_buf = new float[size];
	_size = size;
}

Limiter::~Limiter ()
{
	delete [] _buf;
}

void
Limiter::set_input_gain (float dB)
{
	_enabled = _enabled || dB != 0;
	_limiter.set_inpgain (dB);
}

void
Limiter::set_threshold (float dB)
{
	_enabled = true;
	_limiter.set_threshold (dB);
}

void
Limiter::set_release (float s)
{
	_limiter.set_release (s);
}

void
Limiter::set_duration (samplecnt_t s)
{
	if (_pos != 0 || !_result) {
		return;
	}
	_spp = ceilf ((s + 2.f) / (float) _result->width);
}

void
Limiter::set_result (ARDOUR::ExportAnalysisPtr r)
{
	_result = r;
}

void
Limiter::stats (samplecnt_t n_samples)
{
	if (!_result || _spp == 0) {
		return;
	}
	_cnt += n_samples;
	while (_cnt >= _spp) {
		float peak, gmax, gmin;
		_limiter.get_stats (&peak, &gmax, &gmin);
		_cnt -= _spp;
		assert (_pos < _result->width);
		_result->limiter_pk[_pos++] = peak;
	}
}

void Limiter::process (ProcessContext<float> const& ctx)
{
	const samplecnt_t n_samples  = ctx.samples_per_channel ();
	const int         n_channels = ctx.channels ();

	if (!_enabled) {
		ProcessContext<float> c_out (ctx);
		ListedSource<float>::output (c_out);
		return;
	}

	_limiter.process (n_samples, ctx.data (), _buf);
	stats (n_samples);

	if (_latency > 0) {
		samplecnt_t ns = n_samples > _latency ? n_samples - _latency : 0;
		if (ns > 0) {
			ProcessContext<float> ctx_out (ctx, &_buf[n_channels * _latency], n_channels * ns);
			ctx_out.remove_flag (ProcessContext<float>::EndOfInput);
			this->output (ctx_out);
		}
		if (n_samples >= _latency) {
			_latency = 0;
		} else {
			_latency -= n_samples;
		}
	} else {
		ProcessContext<float> ctx_out (ctx, _buf);
		ctx_out.remove_flag (ProcessContext<float>::EndOfInput);
		this->output (ctx_out);
	}

	if (ctx.has_flag(ProcessContext<float>::EndOfInput)) {
		samplecnt_t bs = _size / n_channels;
		_latency = _limiter.get_latency ();
		while (_latency > 0) {
			memset (_buf, 0, _size * sizeof (float));
			samplecnt_t ns = _latency > bs ? bs : _latency;
			_limiter.process (ns, _buf, _buf);
			//stats (ns);
			ProcessContext<float> ctx_out (ctx, _buf, ns * n_channels);
			if (_latency == ns) {
				ctx_out.set_flag (ProcessContext<float>::EndOfInput);
			} else {
				ctx_out.remove_flag (ProcessContext<float>::EndOfInput);
			}
			this->output (ctx_out);
			_latency -= ns;
		}
  }
}

} // namespace
