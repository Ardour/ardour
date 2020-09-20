/*
 * Copyright (C) 2008-2013 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2011 David Robillard <d@drobilla.net>
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

#ifndef __ardour_onset_detector_h__
#define __ardour_onset_detector_h__

#include "ardour/audioanalyser.h"

namespace ARDOUR {

class AudioSource;
class Session;

class LIBARDOUR_API OnsetDetector : public AudioAnalyser
{
public:
	OnsetDetector (float sample_rate);
	~OnsetDetector();

	static std::string operational_identifier();

	void set_silence_threshold (float);
	void set_peak_threshold (float);
	void set_minioi (float);
	void set_function (int);

	int run (const std::string& path, AudioReadable*, uint32_t channel, AnalysisFeatureList& results);

	static void cleanup_onsets (AnalysisFeatureList&, float sr, float gap_msecs);

protected:
	AnalysisFeatureList* current_results;
	int use_features (Vamp::Plugin::FeatureSet&, std::ostream*);

	static std::string _op_id;
};

} /* namespace */

#endif /* __ardour_audioanalyser_h__ */
