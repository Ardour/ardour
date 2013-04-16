#ifndef __CANVAS_LINE_H__
#define __CANVAS_LINE_H__

#include "canvas/item.h"
#include "canvas/outline.h"
#include "canvas/poly_line.h"

namespace ArdourCanvas {

class Line : virtual public Item, public Outline
{
public:
	Line (Group *);

	void render (Rect const & area, Cairo::RefPtr<Cairo::Context>) const;
	void compute_bounding_box () const;

	void set (Duple, Duple);
	void set_x0 (Coord);
	void set_y0 (Coord);
	void set_x1 (Coord);
	void set_y1 (Coord);
	Coord x0 () const {
		return _points[0].x;
	}
	Coord y0 () const {
		return _points[0].y;
	}
	Coord x1 () const {
		return _points[1].x;
	}
	Coord y1 () const {
		return _points[1].y;
	}

private:
	Duple _points[2];
};
	
}

#endif
