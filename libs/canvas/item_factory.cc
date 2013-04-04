#include "pbd/xml++.h"
#include "canvas/item_factory.h"
#include "canvas/group.h"
#include "canvas/line.h"
#include "canvas/rectangle.h"
#include "canvas/poly_line.h"
#include "canvas/polygon.h"
#include "canvas/pixbuf.h"
#include "canvas/wave_view.h"
#include "canvas/text.h"
#include "canvas/line_set.h"

using namespace std;
using namespace ArdourCanvas;

Item*
ArdourCanvas::create_item (Group* parent, XMLNode const * node)
{
	Item* item = 0;
	if (node->name() == "Group") {
		item = new Group (parent);
	} else if (node->name() == "Line") {
		item = new Line (parent);
	} else if (node->name() == "Rectangle") {
		item = new Rectangle (parent);
	} else if (node->name() == "PolyLine") {
		item = new PolyLine (parent);
	} else if (node->name() == "Polygon") {
		item = new Polygon (parent);
	} else if (node->name() == "Pixbuf") {
		item = new Pixbuf (parent);
	} else if (node->name() == "WaveView") {
		item = new WaveView (parent, boost::shared_ptr<ARDOUR::AudioRegion> ());
	} else if (node->name() == "Text") {
		item = new Text (parent);
	} else if (node->name() == "LineSet") {
		item = new LineSet (parent);
	}

	assert (item);
	item->set_state (node);
	return item;
}
