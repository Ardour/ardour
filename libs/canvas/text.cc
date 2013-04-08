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
	, _color (0x000000ff)
	, _font_description (0)
	, _alignment (Pango::ALIGN_LEFT)
	, _width (0)
	, _height (0)
	, _need_redraw (false)
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

	_need_redraw = true;
	_bounding_box_dirty = true;

	end_change ();
}

void
Text::redraw (Cairo::RefPtr<Cairo::Context> context) const
{
	Glib::RefPtr<Pango::Layout> layout = Pango::Layout::create (context);

	layout->set_text (_text);

	if (_font_description) {
		layout->set_font_description (*_font_description);
	}

	layout->set_alignment (_alignment);
	
	Pango::Rectangle ink_rect = layout->get_ink_extents();
	
	_origin.x = ink_rect.get_x() / Pango::SCALE;
	_origin.y = ink_rect.get_y() / Pango::SCALE;

	_width = _origin.x + ((ink_rect.get_width() + Pango::SCALE / 2) / Pango::SCALE);
	_height = _origin.y + ((ink_rect.get_height() + Pango::SCALE / 2) / Pango::SCALE);
	
	_image = Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, _width, _height);

	Cairo::RefPtr<Cairo::Context> img_context = Cairo::Context::create (_image);

	/* and draw, in the appropriate color of course */

	set_source_rgba (img_context, _color);
	
	std::cerr << "render " << _text << " as " 
		  << ((_color >> 24) & 0xff) / 255.0
		  << ((_color >> 16) & 0xff) / 255.0
		  << ((_color >>  8) & 0xff) / 255.0
		  << ((_color >>  0) & 0xff) / 255.
		  << std::endl;

	layout->show_in_cairo_context (img_context);

	/* text has now been rendered in _image and is ready for blit in
	 * ::render 
	 */

	_need_redraw = false;
}

void
Text::compute_bounding_box () const
{
	if (!_canvas || !_canvas->context () || _text.empty()) {
		_bounding_box = boost::optional<Rect> ();
		_bounding_box_dirty = false;
		return;
	}

	redraw (_canvas->context());

	_bounding_box = Rect (_origin.x, _origin.y, _width - _origin.x, _height - _origin.y);
	_bounding_box_dirty = false;

	cerr << "bbox for " << _text << " = " << _bounding_box << endl;
}

void
Text::render (Rect const & /*area*/, Cairo::RefPtr<Cairo::Context> context) const
{
	if (_text.empty()) {
		return;
	}

	if (_need_redraw) {
		redraw (context);
	}
	
	cerr << " with " << _origin  << " and " << _width << " x " << _height << " render " << _text << endl;

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
	_need_redraw = true;
	_bounding_box_dirty = true;
	end_change ();
}

void
Text::set_font_description (Pango::FontDescription font_description)
{
	begin_change ();
	
	_font_description = new Pango::FontDescription (font_description);
	_need_redraw = true;

	_bounding_box_dirty = true;
	end_change ();
}

void
Text::set_color (Color color)
{
	begin_change ();

	_color = color;
	_need_redraw = true;

	end_change ();
}

		
void
Text::dump (ostream& o) const
{
	Item::dump (o);

	o << _canvas->indent() << '\t' << " text = " << _text << endl
	  << _canvas->indent() << " color = " << _color;

	o << endl;
}
