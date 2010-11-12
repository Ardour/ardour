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

#include "ardour_ui.h"
/*
 * ardour_ui.h include was moved to the top of the list
 * due to a conflicting definition of 'Rect' between
 * Apple's MacTypes.h and GTK.
 */

#include "marker.h"
#include "public_editor.h"
#include "utils.h"
#include "canvas_impl.h"
#include "simpleline.h"

#include <gtkmm2ext/utils.h>

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace Gtkmm2ext;

PBD::Signal1<void,Marker*> Marker::CatchDeletion;

Marker::Marker (PublicEditor& ed, ArdourCanvas::Group& parent, ArdourCanvas::Group& line_parent, guint32 rgba, const string& annotation,
		Type type, nframes_t frame, bool handle_events)

	: editor (ed)
	, _parent (&parent)
	, _line_parent (&line_parent)
	, _line (0)
	, _type (type)
	, _selected (false)
	, _shown (false)
	, _line_shown (false)
	, _canvas_height (0)
	, _color (rgba)
{
	double label_offset = 0;

	/* Shapes we use:

	  Mark:

	   (0,0) -> (6,0)
	     ^        |
	     |	      V
           (0,5)    (6,5)
	       \    /
               (3,10)


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

		points->push_back (Gnome::Art::Point (0.0, 0.0));
		points->push_back (Gnome::Art::Point (6.0, 0.0));
		points->push_back (Gnome::Art::Point (6.0, 5.0));
		points->push_back (Gnome::Art::Point (3.0, 10.0));
		points->push_back (Gnome::Art::Point (0.0, 5.0));
		points->push_back (Gnome::Art::Point (0.0, 0.0));

		_shift = 3;
		label_offset = 8.0;
		break;

	case Tempo:
	case Meter:

		points = new ArdourCanvas::Points ();
		points->push_back (Gnome::Art::Point (3.0, 0.0));
		points->push_back (Gnome::Art::Point (6.0, 5.0));
		points->push_back (Gnome::Art::Point (6.0, 10.0));
		points->push_back (Gnome::Art::Point (0.0, 10.0));
		points->push_back (Gnome::Art::Point (0.0, 5.0));
		points->push_back (Gnome::Art::Point (3.0, 0.0));

		_shift = 3;
		label_offset = 8.0;
		break;

	case SessionStart:
	case RangeStart:
		
	        points = new ArdourCanvas::Points ();
		points->push_back (Gnome::Art::Point (0.0, 0.0));
		points->push_back (Gnome::Art::Point (6.5, 6.5));
		points->push_back (Gnome::Art::Point (0.0, 13.0));
		points->push_back (Gnome::Art::Point (0.0, 0.0));

		_shift = 0;
		label_offset = 13.0;
		break;

	case SessionEnd:
	case RangeEnd:
		points = new ArdourCanvas::Points ();
		points->push_back (Gnome::Art::Point (6.5, 6.5));
		points->push_back (Gnome::Art::Point (13.0, 0.0));
		points->push_back (Gnome::Art::Point (13.0, 13.0));
		points->push_back (Gnome::Art::Point (6.5, 6.5));

		_shift = 13;
		label_offset = 6.0;
		break;

	case LoopStart:
		points = new ArdourCanvas::Points ();
		points->push_back (Gnome::Art::Point (0.0, 0.0));
		points->push_back (Gnome::Art::Point (13.0, 13.0));
		points->push_back (Gnome::Art::Point (0.0, 13.0));
		points->push_back (Gnome::Art::Point (0.0, 0.0));

		_shift = 0;
		label_offset = 12.0;
		break;

	case LoopEnd:
		points = new ArdourCanvas::Points ();
		points->push_back (Gnome::Art::Point (13.0,  0.0));
		points->push_back (Gnome::Art::Point (13.0, 13.0));
		points->push_back (Gnome::Art::Point (0.0, 13.0));
		points->push_back (Gnome::Art::Point (13.0, 0.0));

		_shift = 13;
		label_offset = 0.0;
		break;

	case  PunchIn:
		points = new ArdourCanvas::Points ();
		points->push_back (Gnome::Art::Point (0.0, 0.0));
		points->push_back (Gnome::Art::Point (13.0, 0.0));
		points->push_back (Gnome::Art::Point (0.0, 13.0));
		points->push_back (Gnome::Art::Point (0.0, 0.0));

		_shift = 0;
		label_offset = 13.0;
		break;

	case  PunchOut:
		points = new ArdourCanvas::Points ();
		points->push_back (Gnome::Art::Point (0.0, 0.0));
		points->push_back (Gnome::Art::Point (12.0, 0.0));
		points->push_back (Gnome::Art::Point (12.0, 12.0));
		points->push_back (Gnome::Art::Point (0.0, 0.0));

		_shift = 13;
		label_offset = 0.0;
		break;

	}

	frame_position = frame;
	unit_position = editor.frame_to_unit (frame);

	/* adjust to properly locate the tip */

	unit_position -= _shift;

	group = new Group (parent, unit_position, 1.0);

	mark = new Polygon (*group);
	mark->property_points() = *points;
	set_color_rgba (rgba);
	mark->property_width_pixels() = 1;

	/* setup name pixbuf sizes */
	name_font = get_font_for_style (N_("MarkerText"));

	Gtk::Label foo;

	Glib::RefPtr<Pango::Layout> layout = foo.create_pango_layout (X_("Hg")); /* ascender + descender */
	int width;

	layout->set_font_description (*name_font);
	Gtkmm2ext::get_ink_pixel_size (layout, width, name_height);

	name_pixbuf = new ArdourCanvas::Pixbuf(*group);
	name_pixbuf->property_x() = label_offset;
	name_pixbuf->property_y() = (13/2) - (name_height/2);

	set_name (annotation.c_str());

	editor.ZoomChanged.connect (sigc::mem_fun (*this, &Marker::reposition));

	mark->set_data ("marker", this);

	if (handle_events) {
		group->signal_event().connect (sigc::bind (sigc::mem_fun (editor, &PublicEditor::canvas_marker_event), mark, this));
	}

}


Marker::~Marker ()
{
	CatchDeletion (this); /* EMIT SIGNAL */

	/* destroying the parent group destroys its contents, namely any polygons etc. that we added */
	delete name_pixbuf;
	delete mark;
	delete points;

	delete _line;
}

void Marker::reparent(ArdourCanvas::Group & parent)
{
	group->reparent(parent);
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

			_line = new ArdourCanvas::SimpleLine (*_line_parent);
			_line->property_color_rgba() = ARDOUR_UI::config()->canvasvar_EditPoint.get();

			setup_line_x ();

			_line->signal_event().connect (sigc::bind (sigc::mem_fun (editor, &PublicEditor::canvas_marker_event), mark, this));
		}
		
		double yo = 0;
		if (!_selected) {
			/* work out where to start the line from so that it extends only as far as the mark */
			double x = 0;
			_parent->i2w (x, yo);
			_line_parent->w2i (x, yo);
		}

		_line->property_y1() = yo + 10;
		_line->property_y2() = yo + 10 + _canvas_height;

		_line->property_color_rgba() = _selected ? ARDOUR_UI::config()->canvasvar_EditPoint.get() : _color;
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
	int name_width = pixel_width (new_name, *name_font) + 2;

	name_pixbuf->property_pixbuf() = pixbuf_from_string(new_name, name_font, name_width, name_height, Gdk::Color ("#000000"));

	if (_type == SessionEnd || _type == RangeEnd || _type == LoopEnd || _type == PunchOut) {
		name_pixbuf->property_x() = - (name_width);
	}
}

void
Marker::setup_line_x ()
{
	if (_line) {
		_line->property_x1() = unit_position + _shift - 0.5;
		_line->property_x2() = unit_position + _shift - 0.5;
	}
}

void
Marker::set_position (framepos_t frame)
{
	double new_unit_position = editor.frame_to_unit (frame);
	new_unit_position -= _shift;
	group->move (new_unit_position - unit_position, 0.0);
	frame_position = frame;
	unit_position = new_unit_position;

	setup_line_x ();
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
	mark->property_fill_color_rgba() = _color;
	mark->property_outline_color_rgba() = _color;
	if (_line && !_selected) {
		_line->property_color_rgba() = _color;
	}
}

/***********************************************************************/

TempoMarker::TempoMarker (PublicEditor& editor, ArdourCanvas::Group& parent, ArdourCanvas::Group& line_parent, guint32 rgba, const string& text,
			  ARDOUR::TempoSection& temp)
	: Marker (editor, parent, line_parent, rgba, text, Tempo, 0, false),
	  _tempo (temp)
{
	set_position (_tempo.frame());
	group->signal_event().connect (sigc::bind (sigc::mem_fun (editor, &PublicEditor::canvas_tempo_marker_event), mark, this));
}

TempoMarker::~TempoMarker ()
{
}

/***********************************************************************/

MeterMarker::MeterMarker (PublicEditor& editor, ArdourCanvas::Group& parent, ArdourCanvas::Group& line_parent, guint32 rgba, const string& text,
			  ARDOUR::MeterSection& m)
	: Marker (editor, parent, line_parent, rgba, text, Meter, 0, false),
	  _meter (m)
{
	set_position (_meter.frame());
	group->signal_event().connect (sigc::bind (sigc::mem_fun (editor, &PublicEditor::canvas_meter_marker_event), mark, this));
}

MeterMarker::~MeterMarker ()
{
}

