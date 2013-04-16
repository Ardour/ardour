#include "pbd/xml++.h"
#include "canvas/polygon.h"

using namespace ArdourCanvas;

Polygon::Polygon (Group* parent)
	: Item (parent)
	, PolyItem (parent)
	, Fill (parent)
{

}

void
Polygon::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	if (_outline) {
		setup_outline_context (context);
		render_path (area, context);
		
		if (!_points.empty ()) {
			context->move_to (_points.front().x, _points.front().y);
		}

		context->stroke_preserve ();
	}

	if (_fill) {
		setup_fill_context (context);
		context->fill ();
	}
}

