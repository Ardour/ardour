/*
    Copyright (C) 2012 Paul Davis 

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

#include <cmath>

#include "ardour/readable.h"
#include "ardour/transient_detector.h"

#include "i18n.h"

using namespace Vamp;
using namespace ARDOUR;
using namespace std;

/* need a static initializer function for this */

string TransientDetector::_op_id = X_("libardourvampplugins:qm-onsetdetector:2");

TransientDetector::TransientDetector (float sr)
	: AudioAnalyser (sr, X_("libardourvampplugins:qm-onsetdetector"))
{
	/* update the op_id */

	_op_id = X_("libardourvampplugins:qm-onsetdetector");

	// XXX this should load the above-named plugin and get the current version

	_op_id += ":2";

	threshold = 0.00;
}

TransientDetector::~TransientDetector()
{
}

string
TransientDetector::operational_identifier()
{
	return _op_id;
}

int
TransientDetector::run (const std::string& path, Readable* src, uint32_t channel, AnalysisFeatureList& results)
{
	current_results = &results;
	int ret = analyse (path, src, channel);

	current_results = 0;

	return ret;
}

int
TransientDetector::use_features (Plugin::FeatureSet& features, ostream* out)
{
	const Plugin::FeatureList& fl (features[0]);

	for (Plugin::FeatureList::const_iterator f = fl.begin(); f != fl.end(); ++f) {

		if (f->hasTimestamp) {

			if (out) {
				(*out) << (*f).timestamp.toString() << endl;
			}

			current_results->push_back (RealTime::realTime2Frame (f->timestamp, (framecnt_t) floor(sample_rate)));
		}
	}

	return 0;
}

void
TransientDetector::set_threshold (float val)
{
	threshold = val;
}

void
TransientDetector::set_sensitivity (float val)
{
	if (plugin) {
		plugin->selectProgram ("Percussive onsets");
		plugin->setParameter ("sensitivity", val);
	}
}

void
TransientDetector::cleanup_transients (AnalysisFeatureList& t, float sr, float gap_msecs)
{
	if (t.empty()) {
		return;
	}

	t.sort ();

	/* remove duplicates or other things that are too close */

	AnalysisFeatureList::iterator i = t.begin();
	AnalysisFeatureList::iterator f, b;
	const framecnt_t gap_frames = (framecnt_t) floor (gap_msecs * (sr / 1000.0));

	while (i != t.end()) {

		// move front iterator to just past i, and back iterator the same place

		f = i;
		++f;
		b = f;

		// move f until we find a new value that is far enough away

		while ((f != t.end()) && (((*f) - (*i)) < gap_frames)) {
			++f;
		}

		i = f;

		// if f moved forward from b, we had duplicates/too-close points: get rid of them

		if (b != f) {
			t.erase (b, f);
		}
	}
}

void
TransientDetector::update_positions (Readable* src, uint32_t channel, AnalysisFeatureList& positions)
{
	int const buff_size = 1024;
	int const step_size = 64;

	Sample* data = new Sample[buff_size];

	AnalysisFeatureList::iterator i = positions.begin();

	while (i != positions.end()) {

		/* read from source */
		framecnt_t const to_read = buff_size;

		if (src->read (data, (*i) - buff_size, to_read, channel) != to_read) {
			break;
		}

		// Simple heuristic for locating approx correct cut position.

		for (int j = 0; j < (buff_size - step_size); ) {

			Sample const s = abs (data[j]);
			Sample const s2 = abs (data[j + step_size]);

			if ((s2 - s) > threshold) {
				//cerr << "Thresh exceeded. Moving pos from: " << (*i) << " to: " << (*i) - buff_size + (j + 16) << endl;
				(*i) = (*i) - buff_size + (j + 24);
				break;
			}

			j += step_size;
		}

		++i;
	}

	delete [] data;
}
