#include <iostream>

#include "ardour_ui.h"
#include "canvas-flag.h"
#include "time_axis_view_item.h"

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
	, _text(0)
	, _height(height)
	, _outline_color_rgba(outline_color_rgba)
	, _fill_color_rgba(fill_color_rgba)
	, _region(region)
	, _line(0)
	, _rect(0)
{
	/* XXX this connection is needed if ::on_event() is changed to actually do anything */
	signal_event().connect (sigc::mem_fun (*this, &CanvasFlag::on_event));
}

void
CanvasFlag::delete_allocated_objects()
{
	delete _text;
	_text = 0;

	delete _line;
	_line = 0;

	delete _rect;
	_rect = 0;
}

void
CanvasFlag::set_text(const string& a_text)
{
	delete_allocated_objects();

	_text = new NoEventText (*this, 0.0, 0.0, a_text);
	_text->property_justification() = Gtk::JUSTIFY_CENTER;
	_text->property_fill_color_rgba() = _outline_color_rgba;
	_text->property_font_desc() = TimeAxisViewItem::NAME_FONT;
	double flagwidth  = _text->property_text_width()  + 10.0;
	double flagheight = _text->property_text_height() + 3.0;
	_text->property_x() = flagwidth / 2.0;
	_text->property_y() = flagheight / 2.0;
	_text->show();
	_line = new SimpleLine(*this, 0.0, 0.0, 0.0, _height);
	_line->property_color_rgba() = _outline_color_rgba;
	_rect = new SimpleRect(*this, 0.0, 0.0, flagwidth, flagheight);
	_rect->property_outline_color_rgba() = _outline_color_rgba;
	_rect->property_fill_color_rgba() = _fill_color_rgba;
	_text->raise_to_top();

	/* XXX these two connections are needed if ::on_event() is changed to actually do anything */
	//_rect->signal_event().connect (sigc::mem_fun (*this, &CanvasFlag::on_event));
	//_text->signal_event().connect (sigc::mem_fun (*this, &CanvasFlag::on_event));
}

CanvasFlag::~CanvasFlag()
{
	delete_allocated_objects();
}

bool
CanvasFlag::on_event(GdkEvent* /*ev*/)
{
	/* XXX if you change this function to actually do anything, be sure
	   to fix the connections commented out elsewhere in this file.
	*/
	return false;
}

void
CanvasFlag::set_height (double h)
{
	_height = h;

	if (_line) {
		_line->property_y2() = _height;
	}
}
