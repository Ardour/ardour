#include "pbd/compose.h"
#include "ardour/playlist.h"
#include "ardour/playlist_factory.h"
#include "ardour/source_factory.h"
#include "ardour/region.h"
#include "ardour/region_sorters.h"
#include "ardour/region_factory.h"
#include "playlist_overlap_cache_test.h"

using namespace std;
using namespace PBD;
using namespace ARDOUR;

CPPUNIT_TEST_SUITE_REGISTRATION (PlaylistOverlapCacheTest);

void
PlaylistOverlapCacheTest::tearDown ()
{
	_playlist.reset ();
	_source.reset ();

	TestNeedingSession::tearDown ();
}

void
PlaylistOverlapCacheTest::basicTest ()
{
	string const test_wav_path = "libs/ardour/test/test.wav";
	
	_playlist = PlaylistFactory::create (DataType::AUDIO, *_session, "test");
	_source = SourceFactory::createWritable (DataType::AUDIO, *_session, test_wav_path, "", false, 44100);

	PropertyList plist;
	plist.add (Properties::length, 256);
	
	boost::shared_ptr<Region> regionA = RegionFactory::create (_source, plist);
	regionA->set_name ("A");
	_playlist->add_region (regionA, 0);
	

	{
		Playlist::OverlapCache cache (_playlist.get ());
		Playlist::RegionList rl = cache.get (Evoral::Range<framepos_t> (0, 256));
		CPPUNIT_ASSERT_EQUAL (size_t (1), rl.size ());
		CPPUNIT_ASSERT_EQUAL (regionA, rl.front ());
		
		rl = cache.get (Evoral::Range<framepos_t> (-1000, 1000));
		CPPUNIT_ASSERT_EQUAL (size_t (1), rl.size ());
		CPPUNIT_ASSERT_EQUAL (regionA, rl.front ());
	}

	boost::shared_ptr<Region> regionB = RegionFactory::create (_source, plist);
	regionA->set_name ("B");
	_playlist->add_region (regionB, 53);

	{
		Playlist::OverlapCache cache (_playlist.get ());
		Playlist::RegionList rl = cache.get (Evoral::Range<framepos_t> (0, 256));
		CPPUNIT_ASSERT_EQUAL (size_t (2), rl.size ());
		rl.sort (RegionSortByPosition ());
		CPPUNIT_ASSERT_EQUAL (regionA, rl.front ());
		CPPUNIT_ASSERT_EQUAL (regionB, rl.back ());

		rl = cache.get (Evoral::Range<framepos_t> (260, 274));
		CPPUNIT_ASSERT_EQUAL (size_t (1), rl.size ());
		CPPUNIT_ASSERT_EQUAL (regionB, rl.front ());
	}
}

void
PlaylistOverlapCacheTest::stressTest ()
{
	string const test_wav_path = "libs/ardour/test/test.wav";

	_playlist = PlaylistFactory::create (DataType::AUDIO, *_session, "test");
	_source = SourceFactory::createWritable (DataType::AUDIO, *_session, test_wav_path, "", false, 44100);

	srand (42);

	int const num_regions = rand () % 256;

	for (int i = 0; i < num_regions; ++i) {
		PropertyList plist;
		plist.add (Properties::length, rand () % 32768);
		boost::shared_ptr<Region> r = RegionFactory::create (_source, plist);
		r->set_name (string_compose ("%1", i));
		_playlist->add_region (r, rand() % 32768);
	}

	Playlist::OverlapCache cache (_playlist.get ());

	int const tests = rand () % 256;

	for (int i = 0; i < tests; ++i) {
		framepos_t const start = rand () % 32768;
		framepos_t const length = rand () % 32768;
		framepos_t const end = start + length;

		Playlist::RegionList cached = cache.get (Evoral::Range<framepos_t> (start, end));

		Playlist::RegionList actual;
		Playlist::RegionList regions = _playlist->region_list().rlist();
		for (Playlist::RegionList::iterator j = regions.begin(); j != regions.end(); ++j) {
			if ((*j)->coverage (start, end) != OverlapNone) {
				actual.push_back (*j);
			}
		}

		cached.sort (RegionSortByPosition ());
		actual.sort (RegionSortByPosition ());

		CPPUNIT_ASSERT_EQUAL (actual.size (), cached.size ());
		Playlist::RegionList::iterator j = actual.begin ();
		Playlist::RegionList::iterator k = cached.begin ();
		for (; j != actual.end(); ++j, ++k) {
			CPPUNIT_ASSERT_EQUAL (*j, *k);
		}
	}
}
