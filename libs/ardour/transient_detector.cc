#include <ardour/transient_detector.h>

#include "i18n.h"

using namespace Vamp;
using namespace ARDOUR;
using namespace std;

TransientDetector::TransientDetector (float sr)
	: AudioAnalyser (sr, X_("ardour-vamp-plugins:percussiononsets"))
{
}

TransientDetector::~TransientDetector()
{
}

int
TransientDetector::run (const std::string& path, boost::shared_ptr<Readable> src, uint32_t channel, vector<nframes64_t>& results)
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

			current_results->push_back (RealTime::realTime2Frame ((*f).timestamp, sample_rate));
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
