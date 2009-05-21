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

#include "marker.h"
#include "public_editor.h"
#include "utils.h"
#include "canvas_impl.h"
#include "ardour_ui.h"
#include "simpleline.h"

#include <gtkmm2ext/utils.h>

#include "i18n.h"

using namespace std;
using namespace ARDOUR;

Marker::Marker (PublicEditor& ed, ArdourCanvas::Group& parent, guint32 rgba, const string& annotation, 
		Type type, nframes_t frame, bool handle_events)

	: editor (ed), _parent(&parent), _type(type)
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

	     
	   TransportStart:

	     0,0
	      | \ 
	      |  \ 
	      |   \ 
	      |    \
	      |     \  
	     0,13 --- 13,13

	   TransportEnd:

	            /13,0
	           /   |
	          /    |
	         /     |
	        /      |
	       /       |
	     0,13 ------ 13,13
	     

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
		
		shift = 3;
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

		shift = 3;
		label_offset = 8.0;
		break;

	case Start:
	        points = new ArdourCanvas::Points ();
		points->push_back (Gnome::Art::Point (0.0, 0.0)); 		
		points->push_back (Gnome::Art::Point (6.5, 6.5)); 		
		points->push_back (Gnome::Art::Point (0.0, 13.0)); 		
		points->push_back (Gnome::Art::Point (0.0, 0.0));	

		shift = 0;
		label_offset = 13.0;
		break;

	case End:
		points = new ArdourCanvas::Points ();
		points->push_back (Gnome::Art::Point (6.5, 6.5));
		points->push_back (Gnome::Art::Point (13.0, 0.0));		
		points->push_back (Gnome::Art::Point (13.0, 13.0));			
		points->push_back (Gnome::Art::Point (6.5, 6.5));		
		
		shift = 13;
		label_offset = 6.0;
		break;

	case LoopStart:
		points = new ArdourCanvas::Points ();
		points->push_back (Gnome::Art::Point (0.0, 0.0));	
		points->push_back (Gnome::Art::Point (13.0, 13.0));		
		points->push_back (Gnome::Art::Point (0.0, 13.0));		
		points->push_back (Gnome::Art::Point (0.0, 0.0));		
		
		shift = 0;
		label_offset = 12.0;
		break;

	case LoopEnd:
		points = new ArdourCanvas::Points ();
		points->push_back (Gnome::Art::Point (13.0,  0.0));
		points->push_back (Gnome::Art::Point (13.0, 13.0));	
		points->push_back (Gnome::Art::Point (0.0, 13.0));		
		points->push_back (Gnome::Art::Point (13.0, 0.0));
		
		shift = 13;
		label_offset = 0.0;
		break;

	case  PunchIn:
		points = new ArdourCanvas::Points ();
		points->push_back (Gnome::Art::Point (0.0, 0.0));
		points->push_back (Gnome::Art::Point (13.0, 0.0));		
		points->push_back (Gnome::Art::Point (0.0, 13.0));	
		points->push_back (Gnome::Art::Point (0.0, 0.0));	

		shift = 0;
		label_offset = 13.0;
		break;
		
	case  PunchOut:
		points = new ArdourCanvas::Points ();
		points->push_back (Gnome::Art::Point (0.0, 0.0));
		points->push_back (Gnome::Art::Point (12.0, 0.0));			
		points->push_back (Gnome::Art::Point (12.0, 12.0));		
		points->push_back (Gnome::Art::Point (0.0, 0.0));		

		shift = 13;
		label_offset = 0.0;
		break;
		
	}

	frame_position = frame;
	unit_position = editor.frame_to_unit (frame);

	/* adjust to properly locate the tip */

	unit_position -= shift;

	group = new Group (parent, unit_position, 1.0);

	mark = new Polygon (*group);
	mark->property_points() = *points;
	mark->property_fill_color_rgba() = rgba;
	mark->property_outline_color_rgba() = rgba;
	mark->property_width_pixels() = 1;

	/* setup name pixbuf sizes */
	name_font = get_font_for_style (N_("MarkerText"));
	
	Gtk::Window win;
	Gtk::Label foo;
	win.add (foo);
	
	Glib::RefPtr<Pango::Layout> layout = foo.create_pango_layout (X_("Hg")); /* ascender + descender */
	int width;
	int height;
	
	layout->set_font_description (*name_font);
	Gtkmm2ext::get_ink_pixel_size (layout, width, height);
	name_height = height + 6;
	
	name_pixbuf = new ArdourCanvas::Pixbuf(*group);
	name_pixbuf->property_x() = label_offset;
	
	set_name (annotation.c_str());
	
	editor.ZoomChanged.connect (mem_fun (*this, &Marker::reposition));

	mark->set_data ("marker", this);

	if (handle_events) {
		group->signal_event().connect (bind (mem_fun (editor, &PublicEditor::canvas_marker_event), mark, this));
	}

	line = 0;

}


Marker::~Marker ()
{
	drop_references ();

	/* destroying the parent group destroys its contents, namely any polygons etc. that we added */
	delete name_pixbuf;
	delete mark;
	delete points;

	delete line;
	line = 0;
}

void Marker::reparent(ArdourCanvas::Group & parent)
{
	group->reparent(parent);
	_parent = &parent;
}


void
Marker::set_line_vpos (double pos, double height)
{
	if (line) {
		line->property_y1() = pos;
		line->property_y2() = pos + height;
	}
}

void
Marker::add_line (ArdourCanvas::Group* group, double y_origin, double initial_height)
{
	if (!line) {

		line = new ArdourCanvas::SimpleLine (*group);
		line->property_color_rgba() = ARDOUR_UI::config()->canvasvar_EditPoint.get();
		line->property_x1() = unit_position + shift;
		line->property_y1() = y_origin;
		line->property_x2() = unit_position + shift;
		line->property_y2() = y_origin + initial_height;

		line->signal_event().connect (bind (mem_fun (editor, &PublicEditor::canvas_marker_event), mark, this));
	}

	show_line ();
}

void
Marker::show_line ()
{
	if (line) {
		line->raise_to_top();
		line->show ();
	}
}

void 
Marker::hide_line ()
{
	if (line) {
		line->hide ();
	}
}

ArdourCanvas::Item&
Marker::the_item() const
{
	return *mark;
}

void
Marker::set_name (const string& new_name)
{
	uint32_t pb_width;
	double font_size;
	
	font_size = name_font->get_size() / Pango::SCALE;
	pb_width = new_name.length() * font_size;
	
	Glib::RefPtr<Gdk::Pixbuf> buf = Gdk::Pixbuf::create(Gdk::COLORSPACE_RGB, true, 8, pb_width, name_height);
	
	cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, pb_width, name_height);
	cairo_t *cr = cairo_create (surface);
	cairo_text_extents_t te;
	cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 1.0);
	cairo_select_font_face (cr, name_font->get_family().c_str(),
				CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size (cr, font_size);
	cairo_text_extents (cr, new_name.c_str(), &te);
	
	cairo_move_to (cr, 0.5,
		       0.5 - te.height / 2 - te.y_bearing + name_height / 2);
	cairo_show_text (cr, new_name.c_str());
	
	unsigned char* src = cairo_image_surface_get_data (surface);
	convert_bgra_to_rgba(src, buf->get_pixels(), pb_width, name_height);
	
	cairo_destroy(cr);
	name_pixbuf->property_pixbuf() = buf;
	
	if (_type == End || _type == LoopEnd || _type == PunchOut) {
		name_pixbuf->property_x() = -(te.width);
        }
	
}

void
Marker::set_position (nframes_t frame)
{
	double new_unit_position = editor.frame_to_unit (frame);
	new_unit_position -= shift;
	group->move (new_unit_position - unit_position, 0.0);
	frame_position = frame;
	unit_position = new_unit_position;

	if (line) {
		line->property_x1() = unit_position + shift;
		line->property_x2() = unit_position + shift;
	}
}

void
Marker::reposition ()
{
	set_position (frame_position);
}	

void
Marker::show ()
{
        group->show();
}

void
Marker::hide ()
{
	group->hide();
}

void
Marker::set_color_rgba (uint32_t color)
{
	mark->property_fill_color_rgba() = color;
	mark->property_outline_color_rgba() = color;
}

/***********************************************************************/

TempoMarker::TempoMarker (PublicEditor& editor, ArdourCanvas::Group& parent, guint32 rgba, const string& text, 
			  ARDOUR::TempoSection& temp)
	: Marker (editor, parent, rgba, text, Tempo, 0, false),
	  _tempo (temp)
{
	set_position (_tempo.frame());
	group->signal_event().connect (bind (mem_fun (editor, &PublicEditor::canvas_tempo_marker_event), mark, this));
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
	group->signal_event().connect (bind (mem_fun (editor, &PublicEditor::canvas_meter_marker_event), mark, this));
}

MeterMarker::~MeterMarker ()
{
}

