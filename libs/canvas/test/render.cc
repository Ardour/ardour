#include <pangomm/init.h>
#include "canvas/canvas.h"
#include "canvas/line.h"
#include "canvas/rectangle.h"
#include "canvas/polygon.h"
#include "canvas/arrow.h"
#include "canvas/text.h"
#include "render.h"

using namespace std;
using namespace ArdourCanvas;

CPPUNIT_TEST_SUITE_REGISTRATION (RenderTest);

void
RenderTest::check (string const & name)
{
	stringstream s;
	s << "diff -q " << name << ".png " << "../../libs/canvas/test/" << name << ".png";
	int r = system (s.str().c_str());
	CPPUNIT_ASSERT (WEXITSTATUS (r) == 0);
}

void
RenderTest::basics ()
{
	ImageCanvas canvas (Duple (256, 256));

	/* line */
	Group line_group (canvas.root ());
	line_group.set_position (Duple (0, 0));
	Line line (&line_group);
	line.set (Duple (0, 0), Duple (32, 32));
	line.set_outline_width (2);

	/* rectangle */
	Group rectangle_group (canvas.root ());
	rectangle_group.set_position (Duple (64, 0));
	Rectangle rectangle (&rectangle_group);
	rectangle.set (Rect (0, 0, 32, 32));
	rectangle.set_outline_width (2);
	rectangle.set_outline_color (0x00ff00ff);
	rectangle.set_fill_color (0x0000ffff);

	/* poly line */
	Group poly_line_group (canvas.root ());
	poly_line_group.set_position (Duple (0, 64));
	PolyLine poly_line (&poly_line_group);
	Points points;
	points.push_back (Duple (0, 0));
	points.push_back (Duple (16, 48));
	points.push_back (Duple (32, 32));
	poly_line.set (points);
	poly_line.set_outline_color (0xff0000ff);
	poly_line.set_outline_width (2);

	/* polygon */
	Group polygon_group (canvas.root ());
	polygon_group.set_position (Duple (64, 64));
	Polygon polygon (&polygon_group);
	polygon.set (points);
	polygon.set_outline_color (0xff00ffff);
	polygon.set_fill_color (0xcc00ffff);
	polygon.set_outline_width (2);

	/* arrow */
	Group arrow_group (canvas.root ());
	arrow_group.set_position (Duple (128, 0));
	Arrow arrow (&arrow_group);
	arrow.set_outline_width (2);
	arrow.set_x (32);
	arrow.set_y0 (0);
	arrow.set_y1 (64);

	/* text */
	Pango::init ();
	Group text_group (canvas.root ());
	text_group.set_position (Duple (128, 64));
	Text text (&text_group);
	text.set ("Hello world!");
	
	canvas.render_to_image (Rect (0, 0, 256, 256));
	canvas.write_to_png ("render_basics.png");

	check ("render_basics");
}
