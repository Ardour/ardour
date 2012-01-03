#include "midi++/manager.h"
#include "pbd/textreceiver.h"
#include "pbd/compose.h"
#include "ardour/session.h"
#include "ardour/audioengine.h"
#include "ardour/playlist_factory.h"
#include "ardour/source_factory.h"
#include "ardour/region.h"
#include "ardour/region_factory.h"
#include "playlist_layering_test.h"

CPPUNIT_TEST_SUITE_REGISTRATION (PlaylistLayeringTest);

using namespace std;
using namespace ARDOUR;
using namespace PBD;

void
PlaylistLayeringTest::setUp ()
{
	TestNeedingSession::setUp ();
	
	string const test_wav_path = "libs/ardour/test/playlist_layering_test/playlist_layering_test.wav";
	_playlist = PlaylistFactory::create (DataType::AUDIO, *_session, "test");
	_source = SourceFactory::createWritable (DataType::AUDIO, *_session, test_wav_path, "", false, 44100);
}

void
PlaylistLayeringTest::tearDown ()
{
	_playlist.reset ();
	_source.reset ();
	for (int i = 0; i < 16; ++i) {
		_region[i].reset ();
	}

	TestNeedingSession::tearDown ();
}

void
PlaylistLayeringTest::create_three_short_regions ()
{
	PropertyList plist;
	plist.add (Properties::start, 0);
	plist.add (Properties::length, 100);
	for (int i = 0; i < 3; ++i) {
		_region[i] = RegionFactory::create (_source, plist);
	}
}

void
PlaylistLayeringTest::basicsTest ()
{
	create_three_short_regions ();

	_playlist->add_region (_region[0], 0);
	_playlist->add_region (_region[1], 10);
	_playlist->add_region (_region[2], 20);

	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[0]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (1), _region[1]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (2), _region[2]->layer ());

	_region[0]->set_position (5);

	/* region move should have no effect */
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[0]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (1), _region[1]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (2), _region[2]->layer ());
}
