/*
 * Copyright (C) 2012-2013 Tim Mayberry <mojofunk@gmail.com>
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

#include <glibmm/miscutils.h>

#include "pbd/compose.h"
#include "ardour/playlist_factory.h"
#include "ardour/source_factory.h"
#include "ardour/region.h"
#include "ardour/region_factory.h"
#include "ardour/sndfilesource.h"
#include "ardour/audioregion.h"
#include "ardour/audioplaylist.h"
#include "audio_region_test.h"
#include "test_util.h"

using namespace std;
using namespace PBD;
using namespace ARDOUR;

void
AudioRegionTest::setUp ()
{
	TestNeedingSession::setUp ();

	std::string const test_wav_path = Glib::build_filename (new_test_output_dir(), "test.wav");
	_playlist = PlaylistFactory::create (DataType::AUDIO, *_session, "test");
	_audio_playlist = boost::dynamic_pointer_cast<AudioPlaylist> (_playlist);
	_source = SourceFactory::createWritable (DataType::AUDIO, *_session, test_wav_path, get_test_sample_rate ());

	/* Write a staircase to the source */

	boost::shared_ptr<SndFileSource> s = boost::dynamic_pointer_cast<SndFileSource> (_source);
	assert (s);

	int const signal_length = 4096;

	Sample staircase[signal_length];
	for (int i = 0; i < signal_length; ++i) {
		staircase[i] = i;
	}

	s->write (staircase, signal_length);

	PropertyList plist;
	plist.add (Properties::start, 0);
	plist.add (Properties::length, 100);
	for (int i = 0; i < 16; ++i) {
		_r[i] = RegionFactory::create (_source, plist);
		_ar[i] = boost::dynamic_pointer_cast<AudioRegion> (_r[i]);
		_ar[i]->set_name (string_compose ("ar%1", i));
	}
}

void
AudioRegionTest::tearDown ()
{
	_playlist.reset ();
	_audio_playlist.reset ();
	_source.reset ();
	for (int i = 0; i < 16; ++i) {
		_r[i].reset ();
		_ar[i].reset ();
	}

	TestNeedingSession::tearDown ();
}


