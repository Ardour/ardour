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

#include "audiographer/general/loudness_reader.h"
#include "pbd/fastlog.h"

using namespace AudioGrapher;

LoudnessReader::LoudnessReader (float sample_rate, unsigned int channels, samplecnt_t bufsize)
	: _ebur_plugin (0)
	, _dbtp_plugin (0)
	, _sample_rate (sample_rate)
	, _channels (channels)
	, _bufsize (bufsize / channels)
	, _pos (0)
{
	//printf ("NEW LoudnessReader %p r:%.1f c:%d f:%ld\n", this, sample_rate, channels, bufsize);
	assert (bufsize % channels == 0);
	assert (bufsize > 1);
	assert (_bufsize > 0);

	if (channels > 0 && channels <= 2) {
		using namespace Vamp::HostExt;
		PluginLoader* loader (PluginLoader::getInstance ());
		_ebur_plugin = loader->loadPlugin ("libardourvampplugins:ebur128", sample_rate, PluginLoader::ADAPT_ALL_SAFE);
		assert (_ebur_plugin);
		_ebur_plugin->reset ();
		if (!_ebur_plugin->initialise (channels, _bufsize, _bufsize)) {
			delete _ebur_plugin;
			_ebur_plugin = 0;
		}
	}

	_dbtp_plugin = (Vamp::Plugin**) malloc (sizeof(Vamp::Plugin*) * channels);
	for (unsigned int c = 0; c < _channels; ++c) {
		using namespace Vamp::HostExt;
		PluginLoader* loader (PluginLoader::getInstance ());
		_dbtp_plugin[c] = loader->loadPlugin ("libardourvampplugins:dBTP", sample_rate, PluginLoader::ADAPT_ALL_SAFE);
		assert (_dbtp_plugin[c]);
		_dbtp_plugin[c]->reset ();
		if (!_dbtp_plugin[c]->initialise (1, _bufsize, _bufsize)) {
			delete _dbtp_plugin[c];
			_dbtp_plugin[c] = 0;
		}
	}

	_bufs[0] = (float*) malloc (sizeof (float) * _bufsize);
	_bufs[1] = (float*) malloc (sizeof (float) * _bufsize);
}

LoudnessReader::~LoudnessReader ()
{
	delete _ebur_plugin;
	for (unsigned int c = 0; c < _channels; ++c) {
		delete _dbtp_plugin[c];
	}
	free (_dbtp_plugin);
	free (_bufs[0]);
	free (_bufs[1]);
}

void
LoudnessReader::reset ()
{
	if (_ebur_plugin) {
		_ebur_plugin->reset ();
	}

	for (unsigned int c = 0; c < _channels; ++c) {
		if (_dbtp_plugin[c]) {
			_dbtp_plugin[c]->reset ();
		}
	}
}

void
LoudnessReader::process (ProcessContext<float> const & ctx)
{
	const samplecnt_t n_samples = ctx.samples () / ctx.channels ();
	assert (ctx.channels () == _channels);
	assert (ctx.samples () % ctx.channels () == 0);
	assert (n_samples <= _bufsize);
	//printf ("PROC %p @%ld F: %ld, S: %ld C:%d\n", this, _pos, ctx.samples (), n_samples, ctx.channels ());

	unsigned processed_channels = 0;
	if (_ebur_plugin) {
		assert (_channels <= 2);
		processed_channels = _channels;
		samplecnt_t s;
		float const * d = ctx.data ();
		for (s = 0; s < n_samples; ++s) {
			for (unsigned int c = 0; c < _channels; ++c, ++d) {
				_bufs[c][s] = *d;
			}
		}
		for (; s < _bufsize; ++s) {
			for (unsigned int c = 0; c < _channels; ++c) {
				_bufs[c][s] = 0.f;
			}
		}
		_ebur_plugin->process (_bufs, Vamp::RealTime::fromSeconds ((double) _pos / _sample_rate));
		if (_dbtp_plugin[0]) {
			_dbtp_plugin[0]->process (&_bufs[0], Vamp::RealTime::fromSeconds ((double) _pos / _sample_rate));
		}
		if (_channels == 2 && _dbtp_plugin[1]) {
			_dbtp_plugin[0]->process (&_bufs[1], Vamp::RealTime::fromSeconds ((double) _pos / _sample_rate));
		}
	}

	for (unsigned int c = processed_channels; c < _channels; ++c) {
		if (!_dbtp_plugin[c]) {
			continue;
		}
		samplecnt_t s;
		float const * const d = ctx.data ();
		for (s = 0; s < n_samples; ++s) {
			_bufs[0][s] = d[s * _channels + c];
		}
		for (; s < _bufsize; ++s) {
			_bufs[0][s] = 0.f;
		}
		_dbtp_plugin[c]->process (_bufs, Vamp::RealTime::fromSeconds ((double) _pos / _sample_rate));
	}

	_pos += n_samples;
	ListedSource<float>::output (ctx);
}

float
LoudnessReader::get_normalize_gain (float target_lufs, float target_dbtp)
{
	float dBTP = 0;
	float LUFS = -200;
	uint32_t have_lufs = 0;
	uint32_t have_dbtp = 0;

	if (_ebur_plugin) {
		Vamp::Plugin::FeatureSet features = _ebur_plugin->getRemainingFeatures ();
		if (!features.empty () && features.size () == 3) {
			const float lufs = features[0][0].values[0];
			LUFS = std::max (LUFS, lufs);
			++have_lufs;
		}
	}

	for (unsigned int c = 0; c < _channels; ++c) {
		if (_dbtp_plugin[c]) {
			Vamp::Plugin::FeatureSet features = _dbtp_plugin[c]->getRemainingFeatures ();
			if (!features.empty () && features.size () == 2) {
				const float dbtp = features[0][0].values[0];
				dBTP = std::max (dBTP, dbtp);
				++have_dbtp;
			}
		}
	}

	float g = 100000.0; // +100dB
	bool set = false;
	if (have_lufs && LUFS > -180.0f && target_lufs <= 0.f) {
		const float ge = pow (10.f, (target_lufs * 0.05f)) / pow (10.f, (LUFS * 0.05f));
		//printf ("LU: %f LUFS, %f\n", LUFS, ge);
		g = std::min (g, ge);
		set = true;
	}

	// TODO check that all channels were used.. ? (have_dbtp == _channels)
	if (have_dbtp && dBTP > 0.f && target_dbtp <= 0.f) {
		const float ge = pow (10.f, (target_dbtp * 0.05f)) / dBTP;
		//printf ("TP:(%d chn) %fdBTP -> %f\n", have_dbtp, dBTP, ge);
		g = std::min (g, ge);
		set = true;
	}

	if (!set) {
		g = 1.f;
	}
	//printf ("LF %f  / %f\n", g, 1.f / g);
	return g;
}
