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

#include <cstring>

#include "vamp-hostsdk/PluginLoader.h"

#include <glibmm/miscutils.h>
#include <glibmm/fileutils.h>
#include <glib/gstdio.h> // for g_remove()

#include "pbd/error.h"
#include "pbd/failed_constructor.h"

#include "ardour/audioanalyser.h"
#include "ardour/readable.h"

#include <cstring>

#include "i18n.h"

using namespace std;
using namespace Vamp;
using namespace PBD;
using namespace ARDOUR;

AudioAnalyser::AudioAnalyser (float sr, AnalysisPluginKey key)
	: sample_rate (sr)
	, plugin_key (key)
{
	/* create VAMP plugin and initialize */

	if (initialize_plugin (plugin_key, sample_rate)) {
		error << string_compose (_("cannot load VAMP plugin \"%1\""), key) << endmsg;
		throw failed_constructor();
	}
}

AudioAnalyser::~AudioAnalyser ()
{
	delete plugin;
}

int
AudioAnalyser::initialize_plugin (AnalysisPluginKey key, float sr)
{
	using namespace Vamp::HostExt;

	PluginLoader* loader (PluginLoader::getInstance());

	plugin = loader->loadPlugin (key, sr, PluginLoader::ADAPT_ALL);

	if (!plugin) {
		error << string_compose (_("VAMP Plugin \"%1\" could not be loaded"), key) << endmsg;
		return -1;
	}

	/* we asked for the buffering adapter, so set the blocksize to
	   something that makes for efficient disk i/o
	*/

	bufsize = 65536;
	stepsize = bufsize;

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
AudioAnalyser::analyse (const string& path, Readable* src, uint32_t channel)
{
	ofstream ofile;
	Plugin::FeatureSet features;
	int ret = -1;
	bool done = false;
	Sample* data = 0;
	framecnt_t len = src->readable_length();
	framepos_t pos = 0;
	float* bufs[1] = { 0 };
	string tmp_path;

	if (!path.empty()) {

		/* store data in tmp file, not the real one */

		tmp_path = path;
		tmp_path += ".tmp";

		ofile.open (tmp_path.c_str());
		if (!ofile) {
			goto out;
		}
	}

	data = new Sample[bufsize];
	bufs[0] = data;

	while (!done) {

		framecnt_t to_read;

		/* read from source */

		to_read = min ((len - pos), (framecnt_t) bufsize);

		if (src->read (data, pos, to_read, channel) != to_read) {
			goto out;
		}

		/* zero fill buffer if necessary */

		if (to_read != bufsize) {
			memset (data + to_read, 0, (bufsize - to_read) * sizeof (Sample));
		}

		features = plugin->process (bufs, RealTime::fromSeconds ((double) pos / sample_rate));

		if (use_features (features, (path.empty() ? 0 : &ofile))) {
			goto out;
		}

		pos += min (stepsize, to_read);

		if (pos >= len) {
			done = true;
		}
	}

	/* finish up VAMP plugin */

	features = plugin->getRemainingFeatures ();

	if (use_features (features, (path.empty() ? &ofile : 0))) {
		goto out;
	}

	ret = 0;

  out:
	/* works even if it has not been opened */
	ofile.close ();

	if (ret) {
		g_remove (tmp_path.c_str());
	} else if (!path.empty()) {
		/* move the data file to the requested path */
		g_rename (tmp_path.c_str(), path.c_str());
	}

	delete [] data;

	return ret;
}

