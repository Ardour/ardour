/*
 * Copyright (C) 2015 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015-2017 Paul Davis <paul@linuxaudiosystems.com>
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

#include <cmath>
#include <cstring>

#include "ardour/ebur128_analysis.h"

#include "pbd/i18n.h"

using namespace Vamp;
using namespace ARDOUR;
using namespace std;

/* need a static initializer function for this */

EBUr128Analysis::EBUr128Analysis (float sr)
	: AudioAnalyser (sr, X_("libardourvampplugins:ebur128"))
	, _loudness (0)
	, _loudness_range (0)
{
}

EBUr128Analysis::~EBUr128Analysis()
{
}

int
EBUr128Analysis::run (Readable* src)
{
	int ret = -1;
	bool done = false;
	samplecnt_t len = src->readable_length_samples();
	samplepos_t pos = 0;
	uint32_t n_channels = src->n_channels();
	Plugin::FeatureSet features;

	plugin->reset ();
	if (!plugin->initialise (n_channels, stepsize, bufsize)) {
		return -1;
	}

	float** bufs = (float**) malloc(n_channels * sizeof(float*));
	for (uint32_t c = 0; c < n_channels; ++c) {
		bufs[c] = (float*) malloc(bufsize * sizeof(float));
	}

	while (!done) {
		samplecnt_t to_read;
		to_read = min ((len - pos), (samplecnt_t) bufsize);

		for (uint32_t c = 0; c < n_channels; ++c) {
			if (src->read (bufs[c], pos, to_read, c) != to_read) {
				goto out;
			}
			/* zero fill buffer if necessary */
			if (to_read != bufsize) {
				memset (bufs[c] + to_read, 0, (bufsize - to_read) * sizeof (float));
			}
		}

		plugin->process (bufs, RealTime::fromSeconds ((double) pos / sample_rate));

		pos += min (stepsize, to_read);

		if (pos >= len) {
			done = true;
		}
	}

	features = plugin->getRemainingFeatures ();

	if (use_features (features, 0)) {
		goto out;
	}

	ret = 0;

out:
	for (uint32_t c = 0; c < n_channels; ++c) {
		free (bufs[c]);
	}
	free (bufs);

	return ret;
}

int
EBUr128Analysis::use_features (Plugin::FeatureSet& features, ostream* out)
{
	if (features.empty() || features.size() != 2) {
		return 0;
	}
	_loudness = features[0][0].values[0];
	_loudness_range = features[1][0].values[0];

	return 0;
}
