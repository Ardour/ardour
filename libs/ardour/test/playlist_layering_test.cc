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

class TestReceiver : public Receiver 
{
protected:
	void receive (Transmitter::Channel chn, const char * str) {
		const char *prefix = "";
		
		switch (chn) {
		case Transmitter::Error:
			prefix = ": [ERROR]: ";
			break;
		case Transmitter::Info:
			/* ignore */
			return;
		case Transmitter::Warning:
			prefix = ": [WARNING]: ";
			break;
		case Transmitter::Fatal:
			prefix = ": [FATAL]: ";
			break;
		case Transmitter::Throw:
			/* this isn't supposed to happen */
			abort ();
		}
		
		/* note: iostreams are already thread-safe: no external
		   lock required.
		*/
		
		cout << prefix << str << endl;
		
		if (chn == Transmitter::Fatal) {
			exit (9);
		}
	}
};

TestReceiver test_receiver;

void
PlaylistLayeringTest::setUp ()
{
	string const test_session_path = "libs/ardour/test/playlist_layering_test";
	string const test_wav_path = "libs/ardour/test/playlist_layering_test/playlist_layering_test.wav";
	system (string_compose ("rm -rf %1", test_session_path).c_str());
	
	init (false, true);
	SessionEvent::create_per_thread_pool ("test", 512);

	test_receiver.listen_to (error);
	test_receiver.listen_to (info);
	test_receiver.listen_to (fatal);
	test_receiver.listen_to (warning);

	AudioEngine* engine = new AudioEngine ("test", "");
	MIDI::Manager::create (engine->jack ());
	CPPUNIT_ASSERT (engine->start () == 0);

	_session = new Session (*engine, test_session_path, "playlist_layering_test");
	engine->set_session (_session);

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
	
	AudioEngine::instance()->remove_session ();
	delete _session;
	EnumWriter::destroy ();
	MIDI::Manager::destroy ();
	AudioEngine::destroy ();
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
PlaylistLayeringTest::addHigherTest ()
{
	_session->config.set_layer_model (AddHigher);
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

void
PlaylistLayeringTest::moveAddHigherTest ()
{
	_session->config.set_layer_model (MoveAddHigher);
	create_three_short_regions ();

	_playlist->add_region (_region[0], 0);
	_playlist->add_region (_region[1], 10);
	_playlist->add_region (_region[2], 20);

	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[0]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (1), _region[1]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (2), _region[2]->layer ());

	_region[0]->set_position (5);

	/* region move should have put 0 on top */
	CPPUNIT_ASSERT_EQUAL (layer_t (2), _region[0]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[1]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (1), _region[2]->layer ());
}
