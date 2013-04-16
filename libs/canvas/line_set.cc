#include "pbd/xml++.h"
#include "canvas/line_set.h"
#include "canvas/utils.h"

using namespace std;
using namespace ArdourCanvas;

/* XXX: hard-wired to horizontal only */

class LineSorter {
public:
	bool operator() (LineSet::Line& a, LineSet::Line& b) {
		return a.y < b.y;
	}
};

LineSet::LineSet (Group* parent)
	: Item (parent)
	, _height (0)
{

}


void
LineSet::compute_bounding_box () const
{
	if (_lines.empty ()) {
		_bounding_box = boost::optional<Rect> ();
		_bounding_box_dirty = false;
		return;
	}
	
	_bounding_box = Rect (0, _lines.front().y, COORD_MAX, min (_height, _lines.back().y));
	_bounding_box_dirty = false;
}

void
LineSet::set_height (Distance height)
{
	begin_change ();

	_height = height;

	_bounding_box_dirty = true;
	end_change ();
}

void
LineSet::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	for (list<Line>::const_iterator i = _lines.begin(); i != _lines.end(); ++i) {
		if (i->y > area.y1) {
			break;
		} else if (i->y > area.y0) {
			set_source_rgba (context, i->color);
			context->set_line_width (i->width);
			context->move_to (area.x0, i->y);
			context->line_to (area.x1, i->y);
			context->stroke ();
		}
	}
}

void
LineSet::add (Coord y, Distance width, Color color)
{
	begin_change ();
	
	_lines.push_back (Line (y, width, color));
	_lines.sort (LineSorter ());

	_bounding_box_dirty = true;
	end_change ();
}

void
LineSet::clear ()
{
	begin_change ();
	_lines.clear ();
	_bounding_box_dirty = true;
	end_change ();
}
