#include "canvas/group.h"
#include "canvas/types.h"
#include "canvas/polygon.h"
#include "canvas/canvas.h"
#include "polygon.h"

using namespace std;
using namespace ArdourCanvas;

CPPUNIT_TEST_SUITE_REGISTRATION (PolygonTest);

void
PolygonTest::bounding_box ()
{
	ImageCanvas canvas;
	Group group (canvas.root ());
	Polygon polygon (&group);

	/* should have no initial bounding box */
	CPPUNIT_ASSERT (!polygon.bounding_box().is_initialized());

	Points points;
	points.push_back (Duple (-6, -6));
	points.push_back (Duple ( 6, -6));
	points.push_back (Duple ( 6,  6));
	points.push_back (Duple (-6,  6));
	polygon.set (points);

	/* should now have a bounding box around those points,
	   taking into account default line width
	*/
	boost::optional<Rect> bbox = polygon.bounding_box ();
	CPPUNIT_ASSERT (bbox.is_initialized ());
	CPPUNIT_ASSERT (bbox.get().x0 == -6.25);
	CPPUNIT_ASSERT (bbox.get().x1 ==  6.25);
	CPPUNIT_ASSERT (bbox.get().y0 == -6.25);
	CPPUNIT_ASSERT (bbox.get().y1 ==  6.25);

	/* and its parent group should have noticed and adjusted
	   its bounding box
	*/

	bbox = group.bounding_box ();
	CPPUNIT_ASSERT (bbox.is_initialized ());
	CPPUNIT_ASSERT (bbox.get().x0 == -6.25);
	CPPUNIT_ASSERT (bbox.get().x1 ==  6.25);
	CPPUNIT_ASSERT (bbox.get().y0 == -6.25);
	CPPUNIT_ASSERT (bbox.get().y1 ==  6.25);
}
