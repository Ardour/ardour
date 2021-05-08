/*
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2017 Paul Davis <paul@linuxaudiosystems.com>
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

#include "audiographer/general/loudness_reader.h"
#include "pbd/fastlog.h"

using namespace AudioGrapher;

LoudnessReader::LoudnessReader (float sample_rate, unsigned int channels, samplecnt_t bufsize)
	: _ebur_plugin (0)
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
		assert (_ebur_plugin); // should always be available
		if (_ebur_plugin) {
			_ebur_plugin->reset ();
			if (!_ebur_plugin->initialise (channels, _bufsize, _bufsize)) {
				delete _ebur_plugin;
				_ebur_plugin = 0;
			}
		}
	}

	for (unsigned int c = 0; c < _channels; ++c) {
		using namespace Vamp::HostExt;
		PluginLoader* loader (PluginLoader::getInstance ());
		Vamp::Plugin* dbtp_plugin = loader->loadPlugin ("libardourvampplugins:dBTP", sample_rate, PluginLoader::ADAPT_ALL_SAFE);
		assert (dbtp_plugin); // should always be available
		if (!dbtp_plugin) {
			continue;
		}
		dbtp_plugin->reset ();
		if (!dbtp_plugin->initialise (1, _bufsize, _bufsize)) {
			delete dbtp_plugin;
		} else {
			_dbtp_plugins.push_back (dbtp_plugin);
		}
	}

	_bufs[0] = (float*) malloc (sizeof (float) * _bufsize);
	_bufs[1] = (float*) malloc (sizeof (float) * _bufsize);
}

LoudnessReader::~LoudnessReader ()
{
	delete _ebur_plugin;
	while (!_dbtp_plugins.empty()) {
		delete _dbtp_plugins.back();
		_dbtp_plugins.pop_back();
	}
	free (_bufs[0]);
	free (_bufs[1]);
}

void
LoudnessReader::reset ()
{
	if (_ebur_plugin) {
		_ebur_plugin->reset ();
	}

	for (std::vector<Vamp::Plugin*>::iterator it = _dbtp_plugins.begin (); it != _dbtp_plugins.end(); ++it) {
		(*it)->reset ();
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

		if (_dbtp_plugins.size() > 0) {
			_dbtp_plugins.at(0)->process (&_bufs[0], Vamp::RealTime::fromSeconds ((double) _pos / _sample_rate));
		}
		/* combined dBTP for EBU-R128 */
		if (_channels == 2 && _dbtp_plugins.size() == 2) {
			_dbtp_plugins.at(0)->process (&_bufs[1], Vamp::RealTime::fromSeconds ((double) _pos / _sample_rate));
		}
	}

	for (unsigned int c = processed_channels; c < _channels && c < _dbtp_plugins.size (); ++c) {
		samplecnt_t s;
		float const * const d = ctx.data ();
		for (s = 0; s < n_samples; ++s) {
			_bufs[0][s] = d[s * _channels + c];
		}
		for (; s < _bufsize; ++s) {
			_bufs[0][s] = 0.f;
		}
		_dbtp_plugins.at(c)->process (_bufs, Vamp::RealTime::fromSeconds ((double) _pos / _sample_rate));
	}

	_pos += n_samples;
	ListedSource<float>::output (ctx);
}

bool
LoudnessReader::get_loudness (float* integrated, float* short_term, float* momentary) const
{
	if (_ebur_plugin) {
		Vamp::Plugin::FeatureSet features = _ebur_plugin->getRemainingFeatures ();
		if (!features.empty () && features.size () == 3) {
			if (integrated) {
				*integrated = features[0][0].values[0];
			}
			if (short_term) {
				*short_term = features[0][1].values[0];
			}
			if (momentary) {
				*momentary = features[0][2].values[0];
			}
			return true;
		}
	}
	return false;
}

float
LoudnessReader::calc_peak (float target_lufs, float target_dbtp) const
{
	float    LUFSi = 0;
	float    LUFSs = 0;
	uint32_t have_dbtp = 0;
	float    tp_coeff  = 0;

	bool have_lufs = get_loudness (&LUFSi, &LUFSs);

	for (unsigned int c = 0; c < _channels && c < _dbtp_plugins.size(); ++c) {
		Vamp::Plugin::FeatureSet features = _dbtp_plugins.at(c)->getRemainingFeatures ();
		if (!features.empty () && features.size () == 2) {
			const float tp = features[0][0].values[0];
			tp_coeff = std::max (tp_coeff, tp);
			++have_dbtp;
		}
	}

	float g = 1.f;
	bool set = false;

	if (have_lufs && LUFSi > -180.0f && target_lufs <= 0.f) {
		g = powf (10.f, .05f * (LUFSi - target_lufs));
		set = true;
	} else if (have_lufs && LUFSs > -180.0f && target_lufs <= 0.f) {
		g = powf (10.f, .05f * (LUFSs - target_lufs));
		set = true;
	}

	if (have_dbtp && tp_coeff > 0.f && target_dbtp <= 0.f) {
		const float ge = tp_coeff / powf (10.f, .05f * target_dbtp);
		if (set) {
			g = std::max (g, ge);
		} else {
			g = ge;
		}
		set = true;
	}

	return g;
}
