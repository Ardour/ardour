/*
 * Copyright (C) 2008-2010 David Robillard <d@drobilla.net>
 * Copyright (C) 2008-2017 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __ardour_audioanalyser_h__
#define __ardour_audioanalyser_h__

#include <vector>
#include <string>
#include <boost/utility.hpp>
#include <vamp-hostsdk/Plugin.h>
#include "ardour/libardour_visibility.h"
#include "ardour/types.h"

namespace ARDOUR {

class AudioReadable;
class Session;

class LIBARDOUR_API AudioAnalyser : public boost::noncopyable {

  public:
	typedef Vamp::Plugin AnalysisPlugin;
	typedef std::string AnalysisPluginKey;

	AudioAnalyser (float sample_rate, AnalysisPluginKey key);
	virtual ~AudioAnalyser();

	/* analysis object should provide a run method
	   that accepts a path to write the results to (optionally empty)
	   a AudioReadable* to read data from
	   and a reference to a type-specific container to return the
	   results.
	*/

	void reset ();

  protected:
	float sample_rate;
	AnalysisPlugin* plugin;
	AnalysisPluginKey plugin_key;

	samplecnt_t bufsize;
	samplecnt_t stepsize;

	int initialize_plugin (AnalysisPluginKey name, float sample_rate);
	int analyse (const std::string& path, AudioReadable*, uint32_t channel);

	/* instances of an analysis object will have this method called
	   whenever there are results to process. if out is non-null,
	   the data should be written to the stream it points to.
	*/

	virtual int use_features (Vamp::Plugin::FeatureSet&, std::ostream*) = 0;
};

} /* namespace */

#endif /* __ardour_audioanalyser_h__ */
