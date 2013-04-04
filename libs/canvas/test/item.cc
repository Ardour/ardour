#include "canvas/group.h"
#include "canvas/item.h"
#include "canvas/canvas.h"
#include "canvas/rectangle.h"
#include "item.h"

using namespace std;
using namespace ArdourCanvas;

CPPUNIT_TEST_SUITE_REGISTRATION (ItemTest);

void
ItemTest::item_to_canvas ()
{
	ImageCanvas canvas;
	Group gA (canvas.root ());
	gA.set_position (Duple (128, 128));
	Group gB (&gA);
	gB.set_position (Duple (45, 55));
	Rectangle rA (&gB);
	rA.set_position (Duple (99, 23));

	Rect const r = rA.item_to_canvas (Rect (3, 6, 7, 9));
	CPPUNIT_ASSERT (r.x0 == (128 + 45 + 99 + 3));
	CPPUNIT_ASSERT (r.y0 == (128 + 55 + 23 + 6));
	CPPUNIT_ASSERT (r.x1 == (128 + 45 + 99 + 7));
	CPPUNIT_ASSERT (r.y1 == (128 + 55 + 23 + 9));
}
