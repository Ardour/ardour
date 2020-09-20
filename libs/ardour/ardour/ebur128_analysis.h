/*
 * Copyright (C) 2015 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_ebur128_analysis_h__
#define __ardour_ebur128_analysis_h__

#include "ardour/audioanalyser.h"
#include "ardour/readable.h"

namespace ARDOUR {

class AudioSource;
class Session;

class LIBARDOUR_API EBUr128Analysis : public AudioAnalyser
{
public:
	EBUr128Analysis (float sample_rate);
	~EBUr128Analysis();

	int run (AudioReadable*);

	float loudness () const { return _loudness; }
	float loudness_range () const { return _loudness_range; }

protected:
	int use_features (Vamp::Plugin::FeatureSet&, std::ostream*);

private:
	float _loudness;
	float _loudness_range;

};

} /* namespace */

#endif /* __ardour_audioanalyser_h__ */
