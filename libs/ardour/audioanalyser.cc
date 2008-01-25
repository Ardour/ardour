#include <vamp-sdk/hostext/PluginLoader.h>
#include <glibmm/miscutils.h>
#include <glibmm/fileutils.h>
#include <glib/gstdio.h> // for g_remove()

#include <pbd/error.h>

#include <ardour/audioanalyser.h>
#include <ardour/readable.h>
#include <ardour/readable.h>

#include "i18n.h"

using namespace std;
using namespace Vamp;
using namespace PBD;
using namespace ARDOUR;

AudioAnalyser::AudioAnalyser (float sr, AnalysisPluginKey key)
	: sample_rate (sr)
	, plugin (0)
	, plugin_key (key)
{
}

AudioAnalyser::~AudioAnalyser ()
{
}

int
AudioAnalyser::initialize_plugin (AnalysisPluginKey key, float sr)
{
	using namespace Vamp::HostExt;

	PluginLoader* loader (PluginLoader::getInstance());

	plugin = loader->loadPlugin (key, sr, PluginLoader::ADAPT_ALL);

	if (!plugin) {
		return -1;
	} 

	if ((bufsize = plugin->getPreferredBlockSize ()) == 0) {
		bufsize = 65536;
	}

	if ((stepsize = plugin->getPreferredStepSize()) == 0) {
		stepsize = bufsize;
	}

	if (plugin->getMinChannelCount() > 1) {
		delete plugin;
		return -1;
	}

	if (!plugin->initialise (1, stepsize, bufsize)) {
		delete plugin;
		return -1;
	}

	return 0;
}

void
AudioAnalyser::reset ()
{
	if (plugin) {
		plugin->reset ();
	}
}
	
int
AudioAnalyser::analyse (const string& path, boost::shared_ptr<Readable> src, uint32_t channel)
{
	ofstream ofile;
	Plugin::FeatureSet onsets;
	int ret = -1;
	bool done = false;
	Sample* data = 0;
	nframes64_t len = src->readable_length();
	nframes64_t pos = 0;
	float* bufs[1] = { 0 };

	if (!path.empty()) {
		ofile.open (path.c_str());
		if (!ofile) {
			goto out;
		}
	}

	/* create VAMP percussion onset plugin and initialize */
	
	if (plugin == 0) {
		if (initialize_plugin (plugin_key, sample_rate)) {
			goto out;
		} 
	} 

	data = new Sample[bufsize];
	bufs[0] = data;

	while (!done) {

		nframes64_t to_read;
		
		/* read from source */

		to_read = min ((len - pos), bufsize);
		
		if (src->read (data, pos, to_read, channel) != to_read) {
			cerr << "bad read\n";
			goto out;
		}

		/* zero fill buffer if necessary */

		if (to_read != bufsize) {
			memset (data + to_read, 0, (bufsize - to_read));
		}
		
		onsets = plugin->process (bufs, RealTime::fromSeconds ((double) pos / sample_rate));

		if (use_features (onsets, (path.empty() ? &ofile : 0))) {
			goto out;
		}

		pos += stepsize;
		
		if (pos >= len) {
			done = true;
		}
	}

	/* finish up VAMP plugin */

	onsets = plugin->getRemainingFeatures ();

	if (use_features (onsets, (path.empty() ? &ofile : 0))) {
		goto out;
	}

	ret = 0;

  out:
	/* works even if it has not been opened */
	ofile.close ();

	if (ret) {
		g_remove (path.c_str());
	}
	if (data) {
		delete data;
	}

	return ret;
}

