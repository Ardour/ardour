#ifndef __CANVAS_POLYGON_H__
#define __CANVAS_POLYGON_H__

#include "canvas/poly_item.h"
#include "canvas/outline.h"
#include "canvas/fill.h"

namespace ArdourCanvas {

class Polygon : public PolyItem, public Fill
{
public:
	Polygon (Group *);

	void render (Rect const & area, Cairo::RefPtr<Cairo::Context>) const;
	XMLNode* get_state () const;
	void set_state (XMLNode const *);
};
	
}

#endif
