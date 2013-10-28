/*
    Copyright (C) 2008 Paul Davis

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

#ifndef __ardour_audioanalyser_h__
#define __ardour_audioanalyser_h__

#include <vector>
#include <string>
#include <ostream>
#include <fstream>
#include <boost/utility.hpp>
#include <vamp-sdk/Plugin.h>
#include "ardour/types.h"

namespace ARDOUR {

class Readable;
class Session;

class AudioAnalyser : public boost::noncopyable {

  public:
	typedef Vamp::Plugin AnalysisPlugin;
	typedef std::string AnalysisPluginKey;

	AudioAnalyser (float sample_rate, AnalysisPluginKey key);
	virtual ~AudioAnalyser();

	/* analysis object should provide a run method
	   that accepts a path to write the results to (optionally empty)
	   a Readable* to read data from
	   and a reference to a type-specific container to return the
	   results.
	*/

	void reset ();

  protected:
	float sample_rate;
	AnalysisPlugin* plugin;
	AnalysisPluginKey plugin_key;

	framecnt_t bufsize;
	framecnt_t stepsize;

	int initialize_plugin (AnalysisPluginKey name, float sample_rate);
	int analyse (const std::string& path, Readable*, uint32_t channel);

	/* instances of an analysis object will have this method called
	   whenever there are results to process. if out is non-null,
	   the data should be written to the stream it points to.
	*/

	virtual int use_features (Vamp::Plugin::FeatureSet&, std::ostream*) = 0;
};

} /* namespace */

#endif /* __ardour_audioanalyser_h__ */
