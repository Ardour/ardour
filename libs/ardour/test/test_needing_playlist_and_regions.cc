#include "pbd/filesystem.h"
#include "ardour/playlist_factory.h"
#include "ardour/source_factory.h"
#include "ardour/region.h"
#include "ardour/region_factory.h"
#include "test_needing_playlist_and_regions.h"

using namespace std;
using namespace PBD;
using namespace ARDOUR;

void
TestNeedingPlaylistAndRegions::setUp ()
{
	TestNeedingSession::setUp ();

	string const test_wav_path = "libs/ardour/test/test.wav";
	_playlist = PlaylistFactory::create (DataType::AUDIO, *_session, "test");
	_source = SourceFactory::createWritable (DataType::AUDIO, *_session, test_wav_path, "", false, 44100);
	
	PropertyList plist;
	plist.add (Properties::start, 0);
	plist.add (Properties::length, 100);
	for (int i = 0; i < 16; ++i) {
		_region[i] = RegionFactory::create (_source, plist);
	}
}

void
TestNeedingPlaylistAndRegions::tearDown ()
{
	_playlist.reset ();
	_source.reset ();
	for (int i = 0; i < 16; ++i) {
		_region[i].reset ();
	}

	TestNeedingSession::tearDown ();
}

	
