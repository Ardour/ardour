#include "pbd/xml++.h"
#include "xml.h"
#include "canvas/canvas.h"
#include "canvas/rectangle.h"
#include "canvas/line.h"

CPPUNIT_TEST_SUITE_REGISTRATION (XMLTest);

using namespace std;
using namespace ArdourCanvas;

void
XMLTest::check (string const & name)
{
	stringstream s;
	s << "diff -q " << name << ".xml " << "../../libs/canvas/test/" << name << ".xml";
	int r = system (s.str().c_str());
	CPPUNIT_ASSERT (WEXITSTATUS (r) == 0);
}

void
XMLTest::get ()
{
	ImageCanvas canvas;

	Rectangle r (canvas.root(), Rect (0, 0, 16, 16));
	r.set_outline_color (0x12345678);
	Group g (canvas.root());
	g.set_position (Duple (64, 72));
	Line l (&g);
	l.set (Duple (41, 43), Duple (44, 46));
	
	XMLTree* tree = canvas.get_state ();
	tree->write ("test.xml");

	check ("test");
}

void
XMLTest::set ()
{
	XMLTree* tree = new XMLTree ("../../libs/canvas/test/test.xml");
	ImageCanvas canvas (tree);

	list<Item*> root_items = canvas.root()->items ();
	CPPUNIT_ASSERT (root_items.size() == 2);

	list<Item*>::iterator i = root_items.begin();
	Rectangle* r = dynamic_cast<Rectangle*> (*i++);
	CPPUNIT_ASSERT (r);
	CPPUNIT_ASSERT (r->outline_color() == 0x12345678);
	CPPUNIT_ASSERT (dynamic_cast<Group*> (*i++));
}
