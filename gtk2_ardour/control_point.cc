/*
    Copyright (C) 2002-2007 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "control_point.h"
#include "automation_line.h"
#include "ardour_ui.h"
#include "public_editor.h"

#include "canvas/rectangle.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

PBD::Signal1<void, ControlPoint *> ControlPoint::CatchDeletion;

ControlPoint::ControlPoint (AutomationLine& al)
	: _line (al)
{
	_model = al.the_list()->end();
	_view_index = 0;
	_can_slide = true;
	_x = 0;
	_y = 0;
	_shape = Full;
	_size = 4.0;

	_item = new ArdourCanvas::Rectangle (&_line.canvas_group());
	_item->property_draw() = true;
	_item->set_fill (false);
	_item->set_fill_color (ARDOUR_UI::config()->canvasvar_ControlPointFill.get());
	_item->set_outline_color (ARDOUR_UI::config()->canvasvar_ControlPointOutline.get());
	_item->set_outline_width (1);
	_item->set_data ("control_point", this);
	_item->Event.connect (sigc::mem_fun (this, &ControlPoint::event_handler));

	hide ();
	set_visible (false);
}

ControlPoint::ControlPoint (const ControlPoint& other, bool /*dummy_arg_to_force_special_copy_constructor*/)
	: _line (other._line)
{
	if (&other == this) {
		return;
	}

	_model = other._model;
	_view_index = other._view_index;
	_can_slide = other._can_slide;
	_x = other._x;
	_y = other._y;
	_shape = other._shape;
	_size = other._size;

	_item = new ArdourCanvas::Rectangle (&_line.canvas_group());
	_item->set_fill (false);
	_item->set_outline_color (ARDOUR_UI::config()->canvasvar_ControlPointOutline.get());
	_item->set_outline_width (1);

	/* NOTE: no event handling in copied ControlPoints */

	hide ();
	set_visible (false);
}

ControlPoint::~ControlPoint ()
{
	CatchDeletion (this); /* EMIT SIGNAL */
	
	delete _item;
}

bool
ControlPoint::event_handler (GdkEvent* event)
{
	return PublicEditor::instance().canvas_control_point_event (event, _item, this);
}

void
ControlPoint::hide ()
{
	_item->hide();
}

void
ControlPoint::show()
{
	_item->show();
}

void
ControlPoint::set_visible (bool yn)
{
	_item->property_draw() = (gboolean) yn;
}

bool
ControlPoint::visible () const
{
	return _item->property_draw ();
}

void
ControlPoint::reset (double x, double y, AutomationList::iterator mi, uint32_t vi, ShapeType shape)
{
	/* If this is too big, libart will confuse itself and segfault after it casts the bounding box
	   of this automation line to ints.  Sigh.
	*/

	if (x > INT32_MAX) {
		x = INT32_MAX;
	}

	_model = mi;
	_view_index = vi;
	move_to (x, y, shape);
}

void
ControlPoint::set_color ()
{
	uint32_t color = 0;

	if (_selected) {
		color = ARDOUR_UI::config()->canvasvar_ControlPointSelected.get();
	} else {
		color = ARDOUR_UI::config()->canvasvar_ControlPointOutline.get();
	}

	_item->set_outline_color (color);
	_item->set_fill_color (ARDOUR_UI::config()->canvasvar_ControlPointFill.get());
}

void
ControlPoint::set_size (double sz)
{
	_size = sz;
	move_to (_x, _y, _shape);
}

void
ControlPoint::move_to (double x, double y, ShapeType shape)
{
	double x1 = 0;
	double x2 = 0;
	double half_size = rint(_size/2.0);

	switch (shape) {
	case Full:
		x1 = x - half_size;
		x2 = x + half_size;
		break;
	case Start:
		x1 = x;
		x2 = x + half_size;
		break;
	case End:
		x1 = x - half_size;
		x2 = x;
		break;
	}

	_item->set (ArdourCanvas::Rect (x1, y - half_size, x2, y + half_size));

	_x = x;
	_y = y;
	_shape = shape;
}

void
ControlPoint::i2w (double& x, double& y) const
{
	_item->item_to_canvas (x, y);
}
