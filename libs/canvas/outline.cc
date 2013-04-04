#include <cairomm/context.h>

#include "pbd/xml++.h"
#include "pbd/compose.h"
#include "pbd/convert.h"

#include "ardour/utils.h"
#include "canvas/outline.h"
#include "canvas/utils.h"
#include "canvas/debug.h"

using namespace ArdourCanvas;

Outline::Outline (Group* parent)
	: Item (parent)
	, _outline_color (0x000000ff)
	, _outline_width (0.5)
	, _outline (true)
{

}

void
Outline::set_outline_color (Color color)
{
	begin_change ();
	
	_outline_color = color;

	end_change ();
}

void
Outline::set_outline_width (Distance width)
{
	begin_change ();
	
	_outline_width = width;

	_bounding_box_dirty = true;
	end_change ();

	DEBUG_TRACE (PBD::DEBUG::CanvasItemsDirtied, "canvas item dirty: outline width change\n");	
}

void
Outline::set_outline (bool outline)
{
	begin_change ();

	_outline = outline;

	_bounding_box_dirty = true;
	end_change ();
}

void
Outline::setup_outline_context (Cairo::RefPtr<Cairo::Context> context) const
{
	set_source_rgba (context, _outline_color);
	context->set_line_width (_outline_width);
}

void
Outline::add_outline_state (XMLNode* node) const
{
	node->add_property ("outline-color", string_compose ("%1", _outline_color));
	node->add_property ("outline", _outline ? "yes" : "no");
	node->add_property ("outline-width", string_compose ("%1", _outline_width));
}

void
Outline::set_outline_state (XMLNode const * node)
{
	_outline_color = atoll (node->property("outline-color")->value().c_str());
	_outline = PBD::string_is_affirmative (node->property("outline")->value().c_str());
	_outline_width = atof (node->property("outline-width")->value().c_str());

	_bounding_box_dirty = true;
}
