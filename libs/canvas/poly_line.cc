#include "pbd/xml++.h"
#include "canvas/poly_line.h"

using namespace ArdourCanvas;

PolyLine::PolyLine (Group* parent)
	: Item (parent)
	, PolyItem (parent)
{

}

void
PolyLine::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	if (_outline) {
		setup_outline_context (context);
		render_path (area, context);
		context->stroke ();
	}
}


XMLNode *
PolyLine::get_state () const
{
	XMLNode* node = new XMLNode ("PolyLine");
	add_poly_item_state (node);
	add_outline_state (node);
	return node;
}

void
PolyLine::set_state (XMLNode const * node)
{
	set_poly_item_state (node);
	set_outline_state (node);
}
