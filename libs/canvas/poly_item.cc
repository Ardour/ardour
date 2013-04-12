#include <algorithm>

#include "pbd/xml++.h"
#include "pbd/compose.h"

#include "canvas/poly_item.h"
#include "canvas/canvas.h"

using namespace std;
using namespace ArdourCanvas;

PolyItem::PolyItem (Group* parent)
	: Item (parent)
	, Outline (parent)
{

}

void
PolyItem::compute_bounding_box () const
{
	bool have_one = false;

	Rect bbox;

	for (Points::const_iterator i = _points.begin(); i != _points.end(); ++i) {
		if (have_one) {
			bbox.x0 = min (bbox.x0, i->x);
			bbox.y0 = min (bbox.y0, i->y);
			bbox.x1 = max (bbox.x1, i->x);
			bbox.y1 = max (bbox.y1, i->y);
		} else {
			bbox.x0 = bbox.x1 = i->x;
			bbox.y0 = bbox.y1 = i->y;
			have_one = true;
		}
	}


	if (!have_one) {
		_bounding_box = boost::optional<Rect> ();
	} else {
		_bounding_box = bbox.expand (_outline_width / 2);
	}
	
	_bounding_box_dirty = false;
}

void
PolyItem::render_path (Rect const & /*area*/, Cairo::RefPtr<Cairo::Context> context) const
{
	bool done_first = false;
	for (Points::const_iterator i = _points.begin(); i != _points.end(); ++i) {
		if (done_first) {
			context->line_to (i->x, i->y);
		} else {
			context->move_to (i->x, i->y);
			done_first = true;
		}
	}
}

void
PolyItem::render_curve (Rect const & area, Cairo::RefPtr<Cairo::Context> context, Points const & first_control_points, Points const & second_control_points) const
{
	bool done_first = false;

	if (_points.size() <= 2) {
		render_path (area, context);
		return;
	}

	Points::const_iterator cp1 = first_control_points.begin();
	Points::const_iterator cp2 = second_control_points.begin();

	for (Points::const_iterator i = _points.begin(); i != _points.end(); ++i) {

		if (done_first) {

			context->curve_to (cp1->x, cp1->y,
					   cp2->x, cp2->y,
					   i->x, i->y);

			cp1++;
			cp2++;
			
		} else {

			context->move_to (i->x, i->y);
			done_first = true;
		}
	}
}

void
PolyItem::set (Points const & points)
{
	begin_change ();
	
	_points = points;
	
	_bounding_box_dirty = true;
	end_change ();
}

Points const &
PolyItem::get () const
{
	return _points;
}

void
PolyItem::add_poly_item_state (XMLNode* node) const
{
	add_item_state (node);
	
	for (Points::const_iterator i = _points.begin(); i != _points.end(); ++i) {
		XMLNode* p = new XMLNode ("Point");
		p->add_property ("x", string_compose ("%1", i->x));
		p->add_property ("y", string_compose ("%1", i->y));
		node->add_child_nocopy (*p);
	}
}

void
PolyItem::set_poly_item_state (XMLNode const * node)
{
	XMLNodeList const & children = node->children ();
	for (XMLNodeList::const_iterator i = children.begin(); i != children.end(); ++i) {
		Duple p;
		p.x = atof ((*i)->property("x")->value().c_str());
		p.y = atof ((*i)->property("y")->value().c_str());
		_points.push_back (p);
	}

	_bounding_box_dirty = true;
}

void
PolyItem::dump (ostream& o) const
{
	Item::dump (o);

	o << _canvas->indent() << '\t' << _points.size() << " points" << endl;
	for (Points::const_iterator i = _points.begin(); i != _points.end(); ++i) {
		o << _canvas->indent() << "\t\t" << i->x << ", " << i->y << endl;
	}
}
