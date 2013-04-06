#include <cairomm/cairomm.h>
#include <gtkmm/label.h>

#include "pbd/xml++.h"

#include "canvas/text.h"
#include "canvas/canvas.h"
#include "canvas/utils.h"

using namespace std;
using namespace ArdourCanvas;

Text::Text (Group* parent)
	: Item (parent)
	, _image (0)
	, _color (0x000000ff)
	, _font_description (0)
	, _alignment (Pango::ALIGN_LEFT)
	, _width (0)
	, _height (0)
{

}

Text::~Text ()
{
	delete _font_description;
}

void
Text::set (string const & text)
{
	begin_change ();
	
	_text = text;

	redraw ();

	_bounding_box_dirty = true;
	end_change ();
}

void
Text::redraw ()
{
	_image = Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, _width, _height);

	Cairo::RefPtr<Cairo::Context> context = Cairo::Context::create (_image);
	Glib::RefPtr<Pango::Layout> layout = Pango::Layout::create (context);

	layout->set_text (_text);

	if (_font_description) {
		layout->set_font_description (*_font_description);
	}

	layout->set_alignment (_alignment);
	
	Pango::Rectangle ink_rect = layout->get_ink_extents();
	
	_origin.x = ink_rect.get_x() / Pango::SCALE;
	_origin.y = ink_rect.get_y() / Pango::SCALE;
	_width = (ink_rect.get_width() + Pango::SCALE / 2) / Pango::SCALE;
	_height = (ink_rect.get_height() + Pango::SCALE / 2) / Pango::SCALE;
	
	set_source_rgba (context, _color);

	layout->show_in_cairo_context (context);

	/* text has now been rendered */
}

void
Text::compute_bounding_box () const
{
	_bounding_box = Rect (_origin.x, _origin.y, _origin.x + _width, _origin.y + _height);
	_bounding_box_dirty = false;
}

void
Text::render (Rect const & /*area*/, Cairo::RefPtr<Cairo::Context> context) const
{
	context->set_source (_image, 0, 0);
	context->rectangle (0, 0, _width, _height);
	context->fill ();
}

XMLNode *
Text::get_state () const
{
	XMLNode* node = new XMLNode ("Text");
#ifdef CANVAS_DEBUG
	if (!name.empty ()) {
		node->add_property ("name", name);
	}
#endif
	return node;
}

void
Text::set_state (XMLNode const * /*node*/)
{
	/* XXX */
}

void
Text::set_alignment (Pango::Alignment alignment)
{
	begin_change ();
	
	_alignment = alignment;
	redraw ();
	_bounding_box_dirty = true;
	end_change ();
}

void
Text::set_font_description (Pango::FontDescription font_description)
{
	begin_change ();
	
	_font_description = new Pango::FontDescription (font_description);
	redraw ();

	_bounding_box_dirty = true;
	end_change ();
}

void
Text::set_color (Color color)
{
	begin_change ();

	_color = color;
	redraw ();

	end_change ();
}

		
