/*
 * Copyright (C) 2008-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2010 David Robillard <d@drobilla.net>
 * Copyright (C) 2015-2016 Robin Gareus <robin@gareus.org>
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
#include "ardour/onset_detector.h"

#include "pbd/i18n.h"

using namespace Vamp;
using namespace ARDOUR;
using namespace std;

/* need a static initializer function for this */

string OnsetDetector::_op_id = X_("aubio-onset");

OnsetDetector::OnsetDetector (float sr)
	: AudioAnalyser (sr, X_("libardourvampplugins:aubioonset"))
	, current_results (0)
{
}

OnsetDetector::~OnsetDetector()
{
}

string
OnsetDetector::operational_identifier()
{
	return _op_id;
}

int
OnsetDetector::run (const std::string& path, AudioReadable* src, uint32_t channel, AnalysisFeatureList& results)
{
	current_results = &results;
	int ret = analyse (path, src, channel);

	current_results = 0;
	return ret;
}

int
OnsetDetector::use_features (Plugin::FeatureSet& features, ostream* out)
{
	const Plugin::FeatureList& fl (features[0]);

	for (Plugin::FeatureList::const_iterator f = fl.begin(); f != fl.end(); ++f) {

		if ((*f).hasTimestamp) {

			if (out) {
				(*out) << (*f).timestamp.toString() << endl;
			}

			current_results->push_back (RealTime::realTime2Frame ((*f).timestamp, (samplecnt_t) floor(sample_rate)));
		}
	}

	return 0;
}

void
OnsetDetector::set_silence_threshold (float val)
{
	if (plugin) {
		plugin->setParameter ("silencethreshold", val);
	}
}

void
OnsetDetector::set_peak_threshold (float val)
{
	if (plugin) {
		plugin->setParameter ("peakpickthreshold", val);
	}
}

void
OnsetDetector::set_minioi (float val)
{
#ifdef HAVE_AUBIO4
	if (plugin) {
		plugin->setParameter ("minioi", val);
	}
#endif
}

void
OnsetDetector::set_function (int val)
{
	if (plugin) {
		plugin->setParameter ("onsettype", (float) val);
	}
}

void
OnsetDetector::cleanup_onsets (AnalysisFeatureList& t, float sr, float gap_msecs)
{
	if (t.empty()) {
		return;
	}

	t.sort ();

	/* remove duplicates or other things that are too close */

	AnalysisFeatureList::iterator i = t.begin();
	AnalysisFeatureList::iterator f, b;
	const samplecnt_t gap_samples = (samplecnt_t) floor (gap_msecs * (sr / 1000.0));

	while (i != t.end()) {

		// move front iterator to just past i, and back iterator the same place

		f = i;
		++f;
		b = f;

		// move f until we find a new value that is far enough away

		while ((f != t.end()) && (((*f) - (*i)) < gap_samples)) {
			++f;
		}

		i = f;

		// if f moved forward from b, we had duplicates/too-close points: get rid of them

		if (b != f) {
			t.erase (b, f);
		}
	}
}
