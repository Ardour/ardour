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
#include <ardour/tempo.h>

#include "marker.h"
#include "public_editor.h"
#include "utils.h"
#include "canvas_impl.h"
#include "ardour_ui.h"

#include "i18n.h"

Marker::Marker (PublicEditor& ed, ArdourCanvas::Group& parent, guint32 rgba, const string& annotation, 
		Type type, nframes_t frame, bool handle_events)

	: editor (ed), _type(type)
{
	double label_offset = 0;
	bool annotate_left = false;

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
		annotate_left = true;
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
		annotate_left = true;
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
		annotate_left = true;
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
	Pango::FontDescription* font = get_font_for_style (N_("MarkerText"));
	
	text = new Text (*group);
	text->property_text() = annotation.c_str();
	text->property_font_desc() = *font;

	delete font;
	
	if (annotate_left) {
		text->property_x() = -(text->property_text_width());
	} else {
		text->property_x() = label_offset;
	}
	text->property_y() = 0.0;
	text->property_anchor() = Gtk::ANCHOR_NW;
	text->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_MarkerLabel.get();

	editor.ZoomChanged.connect (mem_fun (*this, &Marker::reposition));

	mark->set_data ("marker", this);

	if (handle_events) {
		group->signal_event().connect (bind (mem_fun (editor, &PublicEditor::canvas_marker_event), mark, this));
	}

	line = 0;
	line_points = 0;

}

Marker::~Marker ()
{
	/* destroying the parent group destroys its contents, namely any polygons etc. that we added */
	delete text;
	delete mark;
	delete points;

	if (line) {
		delete line;
		delete line_points;
	}
}

void
Marker::add_line (ArdourCanvas::Group* group, double initial_height)
{
	if (!line) {

		line_points = new ArdourCanvas::Points ();
		line_points->push_back (Gnome::Art::Point (unit_position + shift, 0.0));
		line_points->push_back (Gnome::Art::Point (unit_position + shift, initial_height));			
		
		line = new ArdourCanvas::Line (*group);
		line->property_width_pixels() = 1;
		line->property_points() = *line_points;
		line->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_EditPoint.get();
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
Marker::set_name (const string& name)
{
	text->property_text() = name.c_str();
	if (_type == End) {
	  text->property_x() = -(text->property_text_width());
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
		(*line_points)[0].set_x (unit_position + shift);
		(*line_points)[1].set_x (unit_position + shift);
		line->property_points() = *line_points;
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

