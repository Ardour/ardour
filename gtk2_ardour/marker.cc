/*
    Copyright (C) 2001 Paul Davis

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

#include <sigc++/bind.h>
#include "ardour/tempo.h"
#include "canvas/rectangle.h"
#include "canvas/group.h"
#include "canvas/line.h"
#include "canvas/polygon.h"
#include "canvas/pixbuf.h"

#include "ardour_ui.h"
/*
 * ardour_ui.h include was moved to the top of the list
 * due to a conflicting definition of 'Rect' between
 * Apple's MacTypes.h and GTK.
 */

#include "marker.h"
#include "public_editor.h"
#include "utils.h"
#include "rgb_macros.h"

#include <gtkmm2ext/utils.h>

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace Gtkmm2ext;

PBD::Signal1<void,Marker*> Marker::CatchDeletion;

Marker::Marker (PublicEditor& ed, ArdourCanvas::Group& parent, guint32 rgba, const string& annotation,
		Type type, framepos_t frame, bool handle_events)

	: editor (ed)
	, _parent (&parent)
	, _line (0)
	, _type (type)
	, _selected (false)
	, _shown (false)
	, _line_shown (false)
	, _canvas_height (0)
	, _color (rgba)
	, _left_label_limit (DBL_MAX)
	, _right_label_limit (DBL_MAX)
	, _label_offset (0)

{
	/* Shapes we use:

	  Mark:

	   (0,0) -> (6,0)
	     ^        |
	     |	      V
           (0,5)    (6,5)
	       \    /
               (3,13)


	   TempoMark:
	   MeterMark:

               (3,0)
              /      \
	   (0,5) -> (6,5)
	     ^        |
	     |	      V
           (0,10)<-(6,10)


           Start:

	   0,0\
       	    |  \
            |   \ 6,6
	    |	/
            |  /
           0,12

	   End:

	       /12,0
	      /     |
             /      |
	   6,6      |
             \      |
              \     |
               \    |
	       12,12

             PunchIn:

	     0,0 ------> 13,0
	      |       /
	      |      /
	      |     /
	      |    /
	      |   /
	      |  /
	     0,13

	     PunchOut

	   0,0 -->-13,0
	    \       |
	     \      |
	      \     |
	       \    |
	        \   |
	         \  |
	         13,13


	*/

	switch (type) {
	case Mark:
		points = new ArdourCanvas::Points ();

		points->push_back (ArdourCanvas::Duple (0.0, 0.0));
		points->push_back (ArdourCanvas::Duple (6.0, 0.0));
		points->push_back (ArdourCanvas::Duple (6.0, 5.0));
		points->push_back (ArdourCanvas::Duple (3.0, 13.0));
		points->push_back (ArdourCanvas::Duple (0.0, 5.0));
		points->push_back (ArdourCanvas::Duple (0.0, 0.0));

		_shift = 3;
		_label_offset = 8.0;
		break;

	case Tempo:
	case Meter:

		points = new ArdourCanvas::Points ();
		points->push_back (ArdourCanvas::Duple (3.0, 0.0));
		points->push_back (ArdourCanvas::Duple (6.0, 5.0));
		points->push_back (ArdourCanvas::Duple (6.0, 10.0));
		points->push_back (ArdourCanvas::Duple (0.0, 10.0));
		points->push_back (ArdourCanvas::Duple (0.0, 5.0));
		points->push_back (ArdourCanvas::Duple (3.0, 0.0));

		_shift = 3;
		_label_offset = 8.0;
		break;

	case SessionStart:
	case RangeStart:

	        points = new ArdourCanvas::Points ();
		points->push_back (ArdourCanvas::Duple (0.0, 0.0));
		points->push_back (ArdourCanvas::Duple (6.5, 6.5));
		points->push_back (ArdourCanvas::Duple (0.0, 13.0));
		points->push_back (ArdourCanvas::Duple (0.0, 0.0));

		_shift = 0;
		_label_offset = 13.0;
		break;

	case SessionEnd:
	case RangeEnd:
		points = new ArdourCanvas::Points ();
		points->push_back (ArdourCanvas::Duple (6.5, 6.5));
		points->push_back (ArdourCanvas::Duple (13.0, 0.0));
		points->push_back (ArdourCanvas::Duple (13.0, 13.0));
		points->push_back (ArdourCanvas::Duple (6.5, 6.5));

		_shift = 13;
		_label_offset = 6.0;
		break;

	case LoopStart:
		points = new ArdourCanvas::Points ();
		points->push_back (ArdourCanvas::Duple (0.0, 0.0));
		points->push_back (ArdourCanvas::Duple (13.0, 13.0));
		points->push_back (ArdourCanvas::Duple (0.0, 13.0));
		points->push_back (ArdourCanvas::Duple (0.0, 0.0));

		_shift = 0;
		_label_offset = 12.0;
		break;

	case LoopEnd:
		points = new ArdourCanvas::Points ();
		points->push_back (ArdourCanvas::Duple (13.0,  0.0));
		points->push_back (ArdourCanvas::Duple (13.0, 13.0));
		points->push_back (ArdourCanvas::Duple (0.0, 13.0));
		points->push_back (ArdourCanvas::Duple (13.0, 0.0));

		_shift = 13;
		_label_offset = 0.0;
		break;

	case  PunchIn:
		points = new ArdourCanvas::Points ();
		points->push_back (ArdourCanvas::Duple (0.0, 0.0));
		points->push_back (ArdourCanvas::Duple (13.0, 0.0));
		points->push_back (ArdourCanvas::Duple (0.0, 13.0));
		points->push_back (ArdourCanvas::Duple (0.0, 0.0));

		_shift = 0;
		_label_offset = 13.0;
		break;

	case  PunchOut:
		points = new ArdourCanvas::Points ();
		points->push_back (ArdourCanvas::Duple (0.0, 0.0));
		points->push_back (ArdourCanvas::Duple (12.0, 0.0));
		points->push_back (ArdourCanvas::Duple (12.0, 12.0));
		points->push_back (ArdourCanvas::Duple (0.0, 0.0));

		_shift = 13;
		_label_offset = 0.0;
		break;

	}

	frame_position = frame;
	unit_position = editor.frame_to_unit (frame);
	unit_position -= _shift;

	group = new ArdourCanvas::Group (&parent, ArdourCanvas::Duple (unit_position, 0));
#ifdef CANVAS_DEBUG
	group->name = string_compose ("Marker::group for %1", annotation);
#endif	

	_name_background = new ArdourCanvas::Rectangle (group);
#ifdef CANVAS_DEBUG
	_name_background->name = string_compose ("Marker::_name_background for %1", annotation);
#endif	
	_name_background->set_outline_width (1);

	/* adjust to properly locate the tip */

	mark = new ArdourCanvas::Polygon (group);
#ifdef CANVAS_DEBUG
	mark->name = string_compose ("Marker::mark for %1", annotation);
#endif	
	mark->set (*points);
	set_color_rgba (rgba);
	mark->set_outline_width (1);

	/* setup name pixbuf sizes */
	name_font = get_font_for_style (N_("MarkerText"));

	Gtk::Label foo;

	Glib::RefPtr<Pango::Layout> layout = foo.create_pango_layout (X_("Hg")); /* ascender + descender */
	int width;

	layout->set_font_description (name_font);
	Gtkmm2ext::get_ink_pixel_size (layout, width, name_height);

	name_pixbuf = new ArdourCanvas::Pixbuf (group);
#ifdef CANVAS_DEBUG
	name_pixbuf->name = string_compose ("Marker::name_pixbuf for %1", annotation);
#endif	
	name_pixbuf->set_position (ArdourCanvas::Duple (_label_offset, 13 / 2 - name_height / 2));

	set_name (annotation.c_str());

	editor.ZoomChanged.connect (sigc::mem_fun (*this, &Marker::reposition));

	group->set_data ("marker", this);

	if (handle_events) {
		group->Event.connect (sigc::bind (sigc::mem_fun (editor, &PublicEditor::canvas_marker_event), group, this));
	}

}


Marker::~Marker ()
{
	CatchDeletion (this); /* EMIT SIGNAL */

	/* destroying the parent group destroys its contents, namely any polygons etc. that we added */
	delete group;
	delete _line;
}

void Marker::reparent(ArdourCanvas::Group & parent)
{
	group->reparent (&parent);
	_parent = &parent;
}

void
Marker::set_selected (bool s)
{
	_selected = s;
	setup_line ();
}

void
Marker::set_show_line (bool s)
{
	_line_shown = s;
	setup_line ();
}

void
Marker::setup_line ()
{
	if (_shown && (_selected || _line_shown)) {

		if (_line == 0) {

			_line = new ArdourCanvas::Line (group);
			_line->set_outline_color (ARDOUR_UI::config()->canvasvar_EditPoint.get());

			_line->Event.connect (sigc::bind (sigc::mem_fun (editor, &PublicEditor::canvas_marker_event), mark, this));
		}

                /* work out where to start the line from so that it extends from the top of the canvas */
		double yo = 0;
                double xo = 0;

                _line->item_to_canvas (xo, yo);

                _line->set_x0 (_shift);
                _line->set_x1 (_shift);
		_line->set_y0 (-yo); // zero in world coordinates, negative in item/parent coordinate space
		_line->set_y1 (-yo + _canvas_height);

		_line->set_outline_color (_selected ? ARDOUR_UI::config()->canvasvar_EditPoint.get() : _color);
		_line->raise_to_top ();
		_line->show ();

	} else {
		if (_line) {
			_line->hide ();
		}
	}
}

void
Marker::canvas_height_set (double h)
{
	_canvas_height = h;
	setup_line ();
}

ArdourCanvas::Item&
Marker::the_item() const
{
	return *mark;
}

void
Marker::set_name (const string& new_name)
{
	_name = new_name;

	setup_name_display ();
}

/** @return true if our label is on the left of the mark, otherwise false */
bool
Marker::label_on_left () const
{
	return (_type == SessionEnd || _type == RangeEnd || _type == LoopEnd || _type == PunchOut);
}

void
Marker::setup_name_display ()
{
	double limit = DBL_MAX;

	if (label_on_left ()) {
		limit = _left_label_limit;
	} else {
		limit = _right_label_limit;
	}

	/* Work out how wide the name can be */
	int name_width = min ((double) pixel_width (_name, name_font) + 2, limit);
	if (name_width == 0) {
		name_width = 1;
	}

	if (label_on_left ()) {
		name_pixbuf->set_x_position (-name_width);
	}

	name_pixbuf->set (pixbuf_from_string (_name, name_font, name_width, name_height, Gdk::Color ("#000000")));

	if (label_on_left ()) {
		_name_background->set_x0 (name_pixbuf->position().x - 2);
		_name_background->set_x1 (name_pixbuf->position().x + name_width + _shift);
	} else {
		_name_background->set_x0 (name_pixbuf->position().x - _label_offset + 2);
		_name_background->set_x1 (name_pixbuf->position().x + name_width);
	}

	_name_background->set_y0 (0);
	_name_background->set_y1 (13);
}

void
Marker::set_position (framepos_t frame)
{
	unit_position = editor.frame_to_unit (frame) - _shift;
	group->set_x_position (unit_position);
	frame_position = frame;
}

void
Marker::reposition ()
{
	set_position (frame_position);
}

void
Marker::show ()
{
	_shown = true;

        group->show ();
	setup_line ();
}

void
Marker::hide ()
{
	_shown = false;

	group->hide ();
	setup_line ();
}

void
Marker::set_color_rgba (uint32_t c)
{
	_color = c;
	mark->set_fill_color (_color);
	mark->set_outline_color (_color);

	if (_line && !_selected) {
		_line->set_outline_color (_color);
	}

	_name_background->set_fill (true);
	_name_background->set_fill_color (UINT_RGBA_CHANGE_A (_color, 0x70));
	_name_background->set_outline_color (_color);
}

/** Set the number of pixels that are available for a label to the left of the centre of this marker */
void
Marker::set_left_label_limit (double p)
{
	/* Account for the size of the marker */
	_left_label_limit = p - 13;
	if (_left_label_limit < 0) {
		_left_label_limit = 0;
	}

	if (label_on_left ()) {
		setup_name_display ();
	}
}

/** Set the number of pixels that are available for a label to the right of the centre of this marker */
void
Marker::set_right_label_limit (double p)
{
	/* Account for the size of the marker */
	_right_label_limit = p - 13;
	if (_right_label_limit < 0) {
		_right_label_limit = 0;
	}

	if (!label_on_left ()) {
		setup_name_display ();
	}
}

/***********************************************************************/

TempoMarker::TempoMarker (PublicEditor& editor, ArdourCanvas::Group& parent, guint32 rgba, const string& text,
			  ARDOUR::TempoSection& temp)
	: Marker (editor, parent, rgba, text, Tempo, 0, false),
	  _tempo (temp)
{
	set_position (_tempo.frame());
	group->Event.connect (sigc::bind (sigc::mem_fun (editor, &PublicEditor::canvas_tempo_marker_event), mark, this));
}

TempoMarker::~TempoMarker ()
{
}

/***********************************************************************/

MeterMarker::MeterMarker (PublicEditor& editor, ArdourCanvas::Group& parent, guint32 rgba, const string& text,
			  ARDOUR::MeterSection& m)
	: Marker (editor, parent, rgba, text, Meter, 0, false),
	  _meter (m)
{
	set_position (_meter.frame());
	group->Event.connect (sigc::bind (sigc::mem_fun (editor, &PublicEditor::canvas_meter_marker_event), mark, this));
}

MeterMarker::~MeterMarker ()
{
}

