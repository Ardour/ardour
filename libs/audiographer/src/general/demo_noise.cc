/*
 * Copyright (C) 2020 Robin Gareus <robin@gareus.org>
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

#include "audiographer/general/demo_noise.h"
#include "ardour/dB.h"

using namespace AudioGrapher;

DemoNoiseAdder::DemoNoiseAdder (unsigned int channels)
  : _data_out (0)
  , _data_out_size (0)
	, _channels (channels)
	, _interval (48000 * 20)
	, _duration (48000)
	, _level    (.1) //  -20dBFS
	, _pos      (48000 * 5)
	, _rseed (1)
{
}

DemoNoiseAdder::~DemoNoiseAdder ()
{
	delete[] _data_out;
}

void
DemoNoiseAdder::init (samplecnt_t max_samples, samplecnt_t interval, samplecnt_t duration, float level)
{
	if (max_samples > _data_out_size) {
		delete[] _data_out;
		_data_out = new float[max_samples];
		_data_out_size = max_samples;
	}

	if (_duration <= 0) {
		_duration = 48000;
	}
	if (duration > interval) {
		_duration = std::min ((samplecnt_t)48000, interval / 5);
	} else {
		_duration = duration;
	}

	_interval = interval;
	_level = dB_to_coefficient (level);
	_pos = interval / 3 + duration;
}

void
DemoNoiseAdder::process (ProcessContext<float> const& ctx)
{
	const samplecnt_t n_samples = ctx.samples_per_channel ();

	if (throw_level (ThrowStrict) && ctx.channels () != _channels) {
		throw Exception (*this, boost::str (boost::format ("Wrong channel count given to process(), %1% instead of %2%") % ctx.channels () % _channels));
	}
	if (throw_level (ThrowProcess) && ctx.samples () > _data_out_size) {
		throw Exception (*this, boost::str (boost::format ("Too many samples given to process(), %1% instead of %2%") % ctx.samples () % _data_out_size));
	}

	if (_pos + n_samples > _duration) {
		_pos -= n_samples;
		ListedSource<float>::output (ctx);
		return;
	}

	assert (ctx.samples () % ctx.channels () == 0);
	assert (ctx.samples () == n_samples * _channels);

	memcpy (_data_out, ctx.data (), sizeof (float) * n_samples * _channels);

	// TODO: optimize, single loop
	float* d = _data_out;
	for (samplecnt_t s = 0; s < n_samples; ++s) {
		for (unsigned int c = 0; c < _channels; ++c, ++d) {
			if (_pos <= _duration) {
				*d += _level * randf ();
			}
		}
		if (--_pos == 0) {
			_pos += _interval;
			break;
		}
	}

	ProcessContext<float> ctx_out (ctx, _data_out);
	this->output (ctx_out);
}

inline uint32_t
DemoNoiseAdder::randi ()
{
  // 31bit Park-Miller-Carta Pseudo-Random Number Generator
  uint32_t hi, lo;
  lo = 16807 * (_rseed & 0xffff);
  hi = 16807 * (_rseed >> 16);
  lo += (hi & 0x7fff) << 16;
  lo += hi >> 15;
  lo = (lo & 0x7fffffff) + (lo >> 31);
  return (_rseed = lo);
}
