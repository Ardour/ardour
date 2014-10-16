#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class WaveViewTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (WaveViewTest);
	CPPUNIT_TEST (all);
	CPPUNIT_TEST_SUITE_END ();

public:
	void setUp ();
	void all ();

private:
	void make_canvas ();
	void render_all_at_once ();
	void render_in_pieces ();
	void cache ();

	ArdourCanvas::ImageCanvas* _canvas;
	ArdourCanvas::WaveView* _wave_view;
	boost::shared_ptr<ARDOUR::Region> _region;
	boost::shared_ptr<ARDOUR::AudioRegion> _audio_region;
};


