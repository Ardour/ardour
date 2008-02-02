#include <ardour/transient_detector.h>

#include "i18n.h"

using namespace Vamp;
using namespace ARDOUR;
using namespace std;

string TransientDetector::_op_id;

TransientDetector::TransientDetector (float sr)
	: AudioAnalyser (sr, X_("libardourvampplugins:percussiononsets"))
{
	if (_op_id.empty()) {
		_op_id = X_("libardourvampplugins:percussiononsets");

		// XXX this should load the above-named plugin and get the current version

		_op_id += ":2";
	}
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
		
		if ((*f).hasTimestamp) {

			if (out) {
				(*out) << (*f).timestamp.toString() << endl;
			} 

			current_results->push_back (RealTime::realTime2Frame ((*f).timestamp, (nframes_t) floor(sample_rate)));
		}
	}

	return 0;
}

void
TransientDetector::set_threshold (float val)
{
	if (plugin) {
		plugin->setParameter ("threshold", val);
	}
}

void
TransientDetector::set_sensitivity (float val)
{
	if (plugin) {
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
	const nframes64_t gap_frames = (nframes64_t) floor (gap_msecs * (sr / 1000.0));
	
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
