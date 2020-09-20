/*
 * Copyright (C) 2008-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2008-2013 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __ardour_transient_detector_h__
#define __ardour_transient_detector_h__

#include "ardour/audioanalyser.h"

namespace ARDOUR {

class AudioSource;
class AudioReadable;
class Session;

class LIBARDOUR_API TransientDetector : public AudioAnalyser
{
public:
	TransientDetector (float sample_rate);
	~TransientDetector();

	static std::string operational_identifier();

	void set_threshold (float);
	void set_sensitivity (uint32_t, float);

	int run (const std::string& path, AudioReadable*, uint32_t channel, AnalysisFeatureList& results);
	void update_positions (AudioReadable* src, uint32_t channel, AnalysisFeatureList& results);

	static void cleanup_transients (AnalysisFeatureList&, float sr, float gap_msecs);

protected:
	AnalysisFeatureList* current_results;
	int use_features (Vamp::Plugin::FeatureSet&, std::ostream*);

	static std::string _op_id;
	float threshold;
};

} /* namespace */

#endif /* __ardour_audioanalyser_h__ */
