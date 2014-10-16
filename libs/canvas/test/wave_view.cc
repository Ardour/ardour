#include <gtkmm/main.h>
#include "pbd/textreceiver.h"
#include "gtkmm2ext/utils.h"
#include "midi++/manager.h"
#include "ardour/session.h"
#include "ardour/audioengine.h"
#include "ardour/source_factory.h"
#include "ardour/audiosource.h"
#include "ardour/audiofilesource.h"
#include "ardour/region_factory.h"
#include "ardour/audioregion.h"
#include "canvas/wave_view.h"
#include "canvas/canvas.h"
#include "wave_view.h"

using namespace std;
using namespace PBD;
using namespace ARDOUR;
using namespace ArdourCanvas;

CPPUNIT_TEST_SUITE_REGISTRATION (WaveViewTest);

TextReceiver text_receiver ("test");

void
WaveViewTest::setUp ()
{
	init (false, true);
	Gtkmm2ext::init ();
	SessionEvent::create_per_thread_pool ("test", 512);

	Gtk::Main kit ();
	Gtk::Main::init_gtkmm_internals ();

	text_receiver.listen_to (error);
	text_receiver.listen_to (info);
	text_receiver.listen_to (fatal);
	text_receiver.listen_to (warning);

	AudioFileSource::set_build_peakfiles (true);
	AudioFileSource::set_build_missing_peakfiles (true);

	AudioEngine engine ("test", "");
	MIDI::Manager::create (engine.jack ());
	CPPUNIT_ASSERT (engine.start () == 0);
	
	Session session (engine, "tmp_session", "tmp_session");
	engine.set_session (&session);

	char buf[256];
	getcwd (buf, sizeof (buf));
	string const path = string_compose ("%1/../../libs/canvas/test/sine.wav", buf);

	boost::shared_ptr<Source> source = SourceFactory::createReadable (
		DataType::AUDIO, session, path, 0, (Source::Flag) 0, false, true
		);

	boost::shared_ptr<AudioFileSource> audio_file_source = boost::dynamic_pointer_cast<AudioFileSource> (source);

	audio_file_source->setup_peakfile ();

	PBD::PropertyList properties;
	properties.add (Properties::position, 128);
	properties.add (Properties::length, audio_file_source->readable_length ());
	_region = RegionFactory::create (source, properties, false);
	_audio_region = boost::dynamic_pointer_cast<AudioRegion> (_region);
}

void
WaveViewTest::make_canvas ()
{
	/* this leaks various things, but hey ho */
	
	_canvas = new ImageCanvas (Duple (256, 256));
	_wave_view = new WaveView (_canvas->root(), _audio_region);
	_wave_view->set_frames_per_pixel ((double) (44100 / 1000) / 64);
	_wave_view->set_height (64);
}

void
WaveViewTest::all ()
{
	/* XXX: we run these all from the same method so that the setUp code only
	   gets called once; there are various singletons etc. in Ardour which don't
	   like being recreated.
	*/
	
	render_all_at_once ();
	render_in_pieces ();
	cache ();
}

void
WaveViewTest::render_all_at_once ()
{
	make_canvas ();
	
	_canvas->render_to_image (Rect (0, 0, 256, 256));
	_canvas->write_to_png ("waveview_1.png");

	/* XXX: doesn't check the result! */
}

void
WaveViewTest::render_in_pieces ()
{
	make_canvas ();

	cout << "\n\n--------------> PIECES\n";
	_canvas->render_to_image (Rect (0, 0, 128, 256));
	_canvas->render_to_image (Rect (128, 0, 256, 256));
	_canvas->write_to_png ("waveview_2.png");
	cout << "\n\n<-------------- PIECES\n";

	/* XXX: doesn't check the result! */
}

void
WaveViewTest::cache ()
{
	make_canvas ();
	
	/* Whole of the render area needs caching from scratch */
	
	_wave_view->invalidate_whole_cache ();
	
	Rect whole (0, 0, 256, 256);
	_canvas->render_to_image (whole);

	CPPUNIT_ASSERT (_wave_view->_cache.size() == 1);
	CPPUNIT_ASSERT (_wave_view->_cache.front()->start() == 0);
	CPPUNIT_ASSERT (_wave_view->_cache.front()->end() == 256);

	_wave_view->invalidate_whole_cache ();
	
	/* Render a bit in the middle */

	Rect part1 (128, 0, 196, 256);
	_canvas->render_to_image (part1);

	CPPUNIT_ASSERT (_wave_view->_cache.size() == 1);
	CPPUNIT_ASSERT (_wave_view->_cache.front()->start() == 128);
	CPPUNIT_ASSERT (_wave_view->_cache.front()->end() == 196);

	/* Now render the whole thing and check that the cache sorts itself out */

	_canvas->render_to_image (whole);
	
	CPPUNIT_ASSERT (_wave_view->_cache.size() == 3);

	list<WaveView::CacheEntry*>::iterator i = _wave_view->_cache.begin ();
	
	CPPUNIT_ASSERT ((*i)->start() == 0);
	CPPUNIT_ASSERT ((*i)->end() == 128);
	++i;

	CPPUNIT_ASSERT ((*i)->start() == 128);
	CPPUNIT_ASSERT ((*i)->end() == 196);
	++i;

	CPPUNIT_ASSERT ((*i)->start() == 196);
	CPPUNIT_ASSERT ((*i)->end() == 256);
	++i;
}
