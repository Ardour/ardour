#include "canvas/lookup_table.h"
#include "canvas/types.h"
#include "canvas/rectangle.h"
#include "canvas/group.h"
#include "canvas/canvas.h"
#include "optimizing_lookup_table.h"

using namespace std;
using namespace ArdourCanvas;

CPPUNIT_TEST_SUITE_REGISTRATION (OptimizingLookupTableTest);

void
OptimizingLookupTableTest::build_1 ()
{
	ImageCanvas canvas;
	Rectangle a (canvas.root(), Rect (0, 0, 32, 32));
	a.set_outline_width (0);
	Rectangle b (canvas.root(), Rect (0, 33, 32, 64));
	b.set_outline_width (0);
	Rectangle c (canvas.root(), Rect (33, 0, 64, 32));
	c.set_outline_width (0);
	Rectangle d (canvas.root(), Rect (33, 33, 64, 64));
	d.set_outline_width (0);
	OptimizingLookupTable table (*canvas.root(), 1);
	
	CPPUNIT_ASSERT (table._items_per_cell == 1);
	CPPUNIT_ASSERT (table._cell_size.x == 32);
	CPPUNIT_ASSERT (table._cell_size.y == 32);
	CPPUNIT_ASSERT (table._cells[0][0].front() == &a);
	CPPUNIT_ASSERT (table._cells[0][1].front() == &b);
	CPPUNIT_ASSERT (table._cells[1][0].front() == &c);
	CPPUNIT_ASSERT (table._cells[1][1].front() == &d);
}

void
OptimizingLookupTableTest::build_2 ()
{
	ImageCanvas canvas;
	Rectangle a (canvas.root(), Rect (0, 0, 713, 1024));
	a.set_outline_width (0);
	Rectangle b (canvas.root(), Rect (0, 0, 0, 1024));
	b.set_outline_width (0);
	OptimizingLookupTable table (*canvas.root(), 64);
}

void
OptimizingLookupTableTest::build_negative ()
{
	ImageCanvas canvas;
	Rectangle a (canvas.root(), Rect (-32, -32, 32, 32));
	OptimizingLookupTable table (*canvas.root(), 1);
}

void
OptimizingLookupTableTest::get_small ()
{
	ImageCanvas canvas;
	Rectangle a (canvas.root(), Rect (0, 0, 32, 32));
	a.set_outline_width (0);
	Rectangle b (canvas.root(), Rect (0, 33, 32, 64));
	b.set_outline_width (0);
	Rectangle c (canvas.root(), Rect (33, 0, 64, 32));
	c.set_outline_width (0);
	Rectangle d (canvas.root(), Rect (33, 33, 64, 64));
	d.set_outline_width (0);
	OptimizingLookupTable table (*canvas.root(), 1);
	
	vector<Item*> items = table.get (Rect (16, 16, 48, 48));
	CPPUNIT_ASSERT (items.size() == 4);
	
	items = table.get (Rect (32, 32, 33, 33));
	CPPUNIT_ASSERT (items.size() == 1);
}

void
OptimizingLookupTableTest::get_big ()
{
	ImageCanvas canvas;

	double const s = 8;
	int const N = 1024;
	
	for (int x = 0; x < N; ++x) {
		for (int y = 0; y < N; ++y) {
			Rectangle* r = new Rectangle (canvas.root());
			r->set_outline_width (0);
			r->set (Rect (x * s, y * s, (x + 1) * s, (y + 1) * s));
		}
	}

	OptimizingLookupTable table (*canvas.root(), 16);
	vector<Item*> items = table.get (Rect (0, 0, 15, 15));
	CPPUNIT_ASSERT (items.size() == 16);
}

/** Check that calling OptimizingLookupTable::get() returns things in the correct order.
 *  The order should be the same as it is in the owning group.
 */
void
OptimizingLookupTableTest::check_ordering ()
{
	ImageCanvas canvas;

	Rectangle a (canvas.root (), Rect (0, 0, 64, 64));
	Rectangle b (canvas.root (), Rect (0, 0, 64, 64));
	Rectangle c (canvas.root (), Rect (0, 0, 64, 64));

	/* since there have been bugs introduced due to sorting pointers,
	   get these rectangles in ascending order of their address
	*/

	list<Item*> items;
	items.push_back (&a);
	items.push_back (&b);
	items.push_back (&c);
	items.sort ();

	/* now arrange these items in the group in reverse order of address */

	for (list<Item*>::reverse_iterator i = items.rbegin(); i != items.rend(); ++i) {
		(*i)->raise_to_top ();
	}

	/* ask the LUT for the items */

	canvas.root()->ensure_lut ();
	vector<Item*> lut_items = canvas.root()->_lut->get (Rect (0, 0, 64, 64));
	CPPUNIT_ASSERT (lut_items.size() == 3);

	/* check that they are in the right order */

	vector<Item*>::iterator i = lut_items.begin ();
	list<Item*>::reverse_iterator j = items.rbegin ();

	while (i != lut_items.end ()) {
		CPPUNIT_ASSERT (*i == *j);
		++i;
		++j;
	}
}
