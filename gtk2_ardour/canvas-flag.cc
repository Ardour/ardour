#include <iostream>

#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/rgb_macros.h"

#include "ardour_ui.h"
#include "canvas-flag.h"
#include "canvas-noevent-pixbuf.h"
#include "time_axis_view_item.h"
#include "utils.h"

using namespace Gnome::Canvas;
using namespace std;

CanvasFlag::CanvasFlag (MidiRegionView& region,
                        Group&          parent,
                        double          height,
                        guint           outline_color_rgba,
                        guint           fill_color_rgba,
                        double          x,
                        double          y)
	: Group(parent, x, y)
	, _name_pixbuf(0)
	, _height(height)
	, _outline_color_rgba(outline_color_rgba)
	, _fill_color_rgba(fill_color_rgba)
	, _region(region)
	, name_pixbuf_width (0)
	, _line(0)
	, _rect(0)
{
}

void
CanvasFlag::delete_allocated_objects()
{
	delete _name_pixbuf;
	_name_pixbuf = 0;

	delete _line;
	_line = 0;

	delete _rect;
	_rect = 0;
}

void
CanvasFlag::set_text (const string& text)
{
	delete_allocated_objects();

	_name_pixbuf = new ArdourCanvas::NoEventPixbuf (*this);
	name_pixbuf_width = Gtkmm2ext::pixel_width (text, TimeAxisViewItem::NAME_FONT) + 2;
	Gdk::Color c;
	set_color (c, _outline_color_rgba);
	_name_pixbuf->property_pixbuf() = Gtkmm2ext::pixbuf_from_string (text, TimeAxisViewItem::NAME_FONT, name_pixbuf_width, 
									 TimeAxisViewItem::NAME_HEIGHT, c);
	_name_pixbuf->property_x() = 10.0;
	_name_pixbuf->property_y() = 2.0;
	_name_pixbuf->show();

	double flagwidth  = name_pixbuf_width + 8.0;
	double flagheight = TimeAxisViewItem::NAME_HEIGHT + 3.0;
	_line = new SimpleLine(*this, 0.0, 0.0, 0.0, _height);
	_line->property_color_rgba() = _outline_color_rgba;
	_rect = new SimpleRect(*this, 0.0, 0.0, flagwidth, flagheight);
	_rect->property_outline_color_rgba() = _outline_color_rgba;
	_rect->property_fill_color_rgba() = _fill_color_rgba;

	_name_pixbuf->raise_to_top();
}

CanvasFlag::~CanvasFlag()
{
	delete_allocated_objects();
}

void
CanvasFlag::set_height (double h)
{
	_height = h;

	if (_line) {
		_line->property_y2() = _height;
	}
}
