/*
 * Copyright (C) 2012 Carl Hetherington <carl@carlh.net>
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

#include <memory>

#include "test_needing_session.h"

namespace ARDOUR {
	class Playlist;
	class AudioPlaylist;
	class Source;
	class Region;
	class AudioRegion;
}

/** A parent class for tests which offers some audio regions,
 *  each with a staircase waveform within them.
 */
class AudioRegionTest : public TestNeedingSession
{
public:
	virtual void setUp ();
	virtual void tearDown ();

protected:
	std::shared_ptr<ARDOUR::Playlist> _playlist;
	/** AudioPlaylist downcast of _playlist */
	std::shared_ptr<ARDOUR::AudioPlaylist> _audio_playlist;
	std::shared_ptr<ARDOUR::Source> _source;
	/** 16 regions, of length 100, each referencing a source which is 4096
	 *  samples of a staircase waveform.
	 */
	std::shared_ptr<ARDOUR::Region> _r[16];
	/** AudioRegion downcasts of _r[] */
	std::shared_ptr<ARDOUR::AudioRegion> _ar[16];
};
