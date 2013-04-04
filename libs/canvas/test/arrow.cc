#include "canvas/group.h"
#include "canvas/types.h"
#include "canvas/arrow.h"
#include "canvas/canvas.h"
#include "arrow.h"

using namespace std;
using namespace ArdourCanvas;

CPPUNIT_TEST_SUITE_REGISTRATION (ArrowTest);

void
ArrowTest::bounding_box ()
{
	ImageCanvas canvas;
	Arrow arrow (canvas.root ());

	for (int i = 0; i < 2; ++i) {
		arrow.set_show_head (i, true);
		arrow.set_head_outward (i, true);
		arrow.set_head_height (i, 16);
		arrow.set_head_width (i, 12);
		arrow.set_x (0);
		arrow.set_y0 (0);
		arrow.set_y1 (128);
	}

	arrow.set_outline_width (0);

	boost::optional<Rect> bbox = arrow.bounding_box ();
	
	CPPUNIT_ASSERT (bbox.is_initialized ());
	CPPUNIT_ASSERT (bbox.get().x0 == -6);
	CPPUNIT_ASSERT (bbox.get().y0 == 0);
	CPPUNIT_ASSERT (bbox.get().x1 == 6);
	CPPUNIT_ASSERT (bbox.get().y1 == 128);
}
