#include "pbd/compose.h"
#include "midi++/manager.h"
#include "ardour/playlist_factory.h"
#include "ardour/source_factory.h"
#include "ardour/region.h"
#include "ardour/region_factory.h"
#include "ardour/session.h"
#include "ardour/audiosource.h"
#include "ardour/audioengine.h"
#include "playlist_layering_test.h"
#include "test_receiver.h"

CPPUNIT_TEST_SUITE_REGISTRATION (PlaylistLayeringTest);

using namespace std;
using namespace ARDOUR;
using namespace PBD;

int const PlaylistLayeringTest::num_regions = 6;

void
PlaylistLayeringTest::setUp ()
{
	TestNeedingSession::setUp ();
	string const test_wav_path = "libs/ardour/test/test.wav";
	
	_playlist = PlaylistFactory::create (DataType::AUDIO, *_session, "test");
	_source = SourceFactory::createWritable (DataType::AUDIO, *_session, test_wav_path, "", false, 44100);

	system ("pwd");
	
	/* Must write some data to our source, otherwise regions which use it will
	   be limited in whether they can be trimmed or not.
	*/
	boost::shared_ptr<AudioSource> a = boost::dynamic_pointer_cast<AudioSource> (_source);
	Sample silence[512];
	memset (silence, 0, 512 * sizeof (Sample));
	a->write (silence, 512);

	_region = new boost::shared_ptr<Region>[num_regions];
}

void
PlaylistLayeringTest::tearDown ()
{
	_playlist.reset ();
	_source.reset ();
	for (int i = 0; i < num_regions; ++i) {
		_region[i].reset ();
	}
	
	delete[] _region;
	
	TestNeedingSession::tearDown ();
}

void
PlaylistLayeringTest::create_short_regions ()
{
	PropertyList plist;
	plist.add (Properties::start, 0);
	plist.add (Properties::length, 100);
	for (int i = 0; i < num_regions; ++i) {
		_region[i] = RegionFactory::create (_source, plist);
		_region[i]->set_name (string_compose ("%1", char (int ('A') + i)));
	}
}

void
PlaylistLayeringTest::laterHigher_relayerOnAll_Test ()
{
	_session->config.set_layer_model (LaterHigher);
	_session->config.set_relayer_on_all_edits (true);
	
	create_short_regions ();

	/* three overlapping regions */
	_playlist->add_region (_region[A], 0);
	_playlist->add_region (_region[B], 10);
	_playlist->add_region (_region[C], 20);
	/* and another non-overlapping one */
	_playlist->add_region (_region[D], 200);

	/* LaterHigher means that they should be arranged thus */
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[A]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (1), _region[B]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (2), _region[C]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[D]->layer ());

	_region[A]->set_position (5);

	/* Region move should have no effect in LaterHigher mode */
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[A]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (1), _region[B]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (2), _region[C]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[D]->layer ());

	/* C -> bottom should give C A B, not touching D */
	_region[C]->lower_to_bottom ();
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[C]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (1), _region[A]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (2), _region[B]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[D]->layer ());

	/* C -> top should go back to A B C, not touching D */
	_region[C]->raise_to_top ();
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[A]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (1), _region[B]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (2), _region[C]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[D]->layer ());
}

void
PlaylistLayeringTest::addHigher_relayerOnAll_Test ()
{
	_session->config.set_layer_model (AddHigher);
	_session->config.set_relayer_on_all_edits (true);
	
	create_short_regions ();

	/* three overlapping regions */
	_playlist->add_region (_region[A], 0);
	_playlist->add_region (_region[B], 10);
	_playlist->add_region (_region[C], 20);
	/* and another non-overlapping one */
	_playlist->add_region (_region[D], 200);

	/* AddHigher means that they should be arranged thus */
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[A]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (1), _region[B]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (2), _region[C]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[D]->layer ());

	_region[A]->set_position (5);

	/* region move should have no effect in AddHigher mode */
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[A]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (1), _region[B]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (2), _region[C]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[D]->layer ());

	/* C -> bottom should give C A B, not touching D */
	_region[C]->lower_to_bottom ();
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[C]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (1), _region[A]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (2), _region[B]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[D]->layer ());

	/* C -> top should go back to A B C, not touching D */
	_region[C]->raise_to_top ();
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[A]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (1), _region[B]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (2), _region[C]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[D]->layer ());
}

void
PlaylistLayeringTest::addOrBoundsHigher_relayerOnAll_Test ()
{
	_session->config.set_layer_model (AddOrBoundsChangeHigher);
	_session->config.set_relayer_on_all_edits (true);
	
	create_short_regions ();

	/* three overlapping regions */
	_playlist->add_region (_region[A], 0);
	_playlist->add_region (_region[B], 10);
	_playlist->add_region (_region[C], 20);
	/* and another non-overlapping one */
	_playlist->add_region (_region[D], 200);

	/* AddOrBoundsHigher means that they should be arranged thus */
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[A]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (1), _region[B]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (2), _region[C]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[D]->layer ());

	/* region move should put A on top for B C A, not touching D */
	_region[A]->set_position (5);
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[B]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (1), _region[C]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (2), _region[A]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[D]->layer ());

	/* C -> bottom should give C B A, not touching D */
	_region[C]->lower_to_bottom ();
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[C]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (1), _region[B]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (2), _region[A]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[D]->layer ());

	/* C -> top should go back to B A C, not touching D */
	_region[C]->raise_to_top ();
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[B]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (1), _region[A]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (2), _region[C]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[D]->layer ());

	/* Put C on the bottom */
	_region[C]->lower_to_bottom ();
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[C]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (1), _region[B]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (2), _region[A]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[D]->layer ());

	/* Now move it slightly, and it should go back to the top again */
	_region[C]->set_position (21);
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[B]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (1), _region[A]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (2), _region[C]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[D]->layer ());

	/* Put C back on the bottom */
	_region[C]->lower_to_bottom ();
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[C]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (1), _region[B]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (2), _region[A]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[D]->layer ());

	/* Trim it slightly, and it should go back to the top again */
	_region[C]->trim_front (23);
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[B]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (1), _region[A]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (2), _region[C]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[D]->layer ());

	/* Same with the end */
	_region[C]->lower_to_bottom ();
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[C]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (1), _region[B]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (2), _region[A]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[D]->layer ());

	_region[C]->trim_end (118);
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[B]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (1), _region[A]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (2), _region[C]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[D]->layer ());
}

void
PlaylistLayeringTest::addOrBoundsHigher_relayerWhenNecessary_Test ()
{
	_session->config.set_layer_model (AddOrBoundsChangeHigher);
	_session->config.set_relayer_on_all_edits (false);
	
	create_short_regions ();

	/* three overlapping regions */
	_playlist->add_region (_region[A], 0);
	_playlist->add_region (_region[B], 10);
	_playlist->add_region (_region[C], 20);
	/* and another non-overlapping one */
	_playlist->add_region (_region[D], 200);

	/* AddOrBoundsHigher means that they should be arranged thus */
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[A]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (1), _region[B]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (2), _region[C]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[D]->layer ());

	_region[A]->set_position (5);

	/* region move should not have changed anything, since in
	   this mode we only relayer when there is a new overlap
	*/
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[A]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (1), _region[B]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (2), _region[C]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[D]->layer ());

	/* C -> bottom should give C A B, not touching D */
	_region[C]->lower_to_bottom ();
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[C]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (1), _region[A]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (2), _region[B]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[D]->layer ());

	/* C -> top should go back to A B C, not touching D */
	_region[C]->raise_to_top ();
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[A]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (1), _region[B]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (2), _region[C]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[D]->layer ());

	/* Put C on the bottom */
	_region[C]->lower_to_bottom ();
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[C]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (1), _region[A]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (2), _region[B]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[D]->layer ());
	
	/* Now move it slightly, and it should stay where it is */
	_region[C]->set_position (21);
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[C]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (1), _region[A]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (2), _region[B]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[D]->layer ());
}

void
PlaylistLayeringTest::lastLayerOpTest ()
{
	create_short_regions ();

	_playlist->add_region (_region[A], 0);
	CPPUNIT_ASSERT_EQUAL (_playlist->layer_op_counter, _region[A]->last_layer_op (LayerOpAdd));
	uint64_t const last_add = _region[A]->last_layer_op (LayerOpAdd);
	
	_region[A]->set_position (42);
	CPPUNIT_ASSERT_EQUAL (_playlist->layer_op_counter, _region[A]->last_layer_op (LayerOpBoundsChange));
	CPPUNIT_ASSERT_EQUAL (last_add, _region[A]->last_layer_op (LayerOpAdd));

	_region[A]->trim_front (46);
	CPPUNIT_ASSERT_EQUAL (_playlist->layer_op_counter, _region[A]->last_layer_op (LayerOpBoundsChange));
	CPPUNIT_ASSERT_EQUAL (last_add, _region[A]->last_layer_op (LayerOpAdd));

	_region[A]->trim_end (102);
	CPPUNIT_ASSERT_EQUAL (_playlist->layer_op_counter, _region[A]->last_layer_op (LayerOpBoundsChange));
	CPPUNIT_ASSERT_EQUAL (last_add, _region[A]->last_layer_op (LayerOpAdd));
}

void
PlaylistLayeringTest::recursiveRelayerTest ()
{
	_session->config.set_layer_model (AddOrBoundsChangeHigher);
	_session->config.set_relayer_on_all_edits (false);
	
	create_short_regions ();

	_playlist->add_region (_region[A], 100);
	_playlist->add_region (_region[B], 125);
	_playlist->add_region (_region[C], 50);
	_playlist->add_region (_region[D], 250);

	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[A]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (1), _region[B]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (2), _region[C]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[D]->layer ());

	_region[A]->set_position (200);

	CPPUNIT_ASSERT_EQUAL (layer_t (0), _region[D]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (1), _region[A]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (2), _region[B]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (3), _region[C]->layer ());
}
