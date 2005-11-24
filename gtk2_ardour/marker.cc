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

    $Id$
*/

#include <sigc++/bind.h>
#include <ardour/tempo.h>

#include "marker.h"
#include "public_editor.h"
#include "utils.h"
#include "canvas_impl.h"

#include "i18n.h"

Marker::Marker (PublicEditor& ed, ArdourCanvas::Group& parent, guint32 rgba, const string& annotation, 
		Type type, jack_nframes_t frame, bool handle_events)

	: editor (ed), _type(type)
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

	   0,0  -> 5,0 
       	    |          \
            |          10,5
	    |	       /
           0,10 -> 5,10

	   End:

	     5,0 -> 10,0
             /       |
	   0,5       |
             \       |
	     5,10 <-10,10

	     
	   TransportStart:

	     0,0->3,0
	      |    |
	      |    |
	      |    |
	      |   3,8 -> 7,8
	      |           |
	     0,11 ------ 7,11

	   TransportEnd:

	              4,0->7,0
	               |    |
	               |    |
	               |    |
	     0,8 ---- 4,8   |
	      |             |
	     0,11 -------- 7,11
	     

	     PunchIn:

	     0,0 ------> 8,0
	      |       /
	      |      /
	      |    4,5
	      |     |
	      |     |
	      |     |
	     0,11->4,11

	     PunchOut

	   0,0 -->-8,0
	    \       | 
	     \      |
	     4,5    |
	      |     |
	      |     |
	      |     |
	     4,11->8,11
	     
	   
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
		points->push_back (Gnome::Art::Point (5.0, 0.0)); 		
		points->push_back (Gnome::Art::Point (10.0, 5.0)); 		
		points->push_back (Gnome::Art::Point (5.0, 10.0)); 		
		points->push_back (Gnome::Art::Point (0.0, 10.0)); 		
		points->push_back (Gnome::Art::Point (0.0, 0.0));	

		shift = 10;
		label_offset = 12.0;
		break;

	case End:
		points = new ArdourCanvas::Points ();
		points->push_back (Gnome::Art::Point (5.0, 0.0));
		points->push_back (Gnome::Art::Point (10.0, 0.0));		
		points->push_back (Gnome::Art::Point (10.0, 10.0));		
		points->push_back (Gnome::Art::Point (5.0, 10.0));		
		points->push_back (Gnome::Art::Point (0.0, 5.0));		
		points->push_back (Gnome::Art::Point (5.0, 0.0));		
		
		shift = 0;
		label_offset = 12.0;
		break;

	case LoopStart:
		points = new ArdourCanvas::Points ();
		points->push_back (Gnome::Art::Point (0.0, 0.0));
		points->push_back (Gnome::Art::Point (4.0, 0.0));		
		points->push_back (Gnome::Art::Point (4.0, 8.0));		
		points->push_back (Gnome::Art::Point (8.0, 8.0));		
		points->push_back (Gnome::Art::Point (8.0, 11.0));		
		points->push_back (Gnome::Art::Point (0.0, 11.0));		
		points->push_back (Gnome::Art::Point (0.0, 0.0));		
		
		shift = 0;
		label_offset = 11.0;
		break;

	case LoopEnd:
		points = new ArdourCanvas::Points ();
		points->push_back (Gnome::Art::Point (8.0,  0.0));
		points->push_back (Gnome::Art::Point (8.0, 11.0));	
		points->push_back (Gnome::Art::Point (0.0, 11.0));		
		points->push_back (Gnome::Art::Point (0.0, 8.0));
		points->push_back (Gnome::Art::Point (4.0, 8.0));
		points->push_back (Gnome::Art::Point (4.0, 0.0));	
		points->push_back (Gnome::Art::Point (8.0, 0.0));
		
		shift = 8;
		label_offset = 11.0;
		break;

	case  PunchIn:
		points = new ArdourCanvas::Points ();
		points->push_back (Gnome::Art::Point (0.0, 0.0));
		points->push_back (Gnome::Art::Point (8.0, 0.0));		
		points->push_back (Gnome::Art::Point (4.0, 4.0));	
		points->push_back (Gnome::Art::Point (4.0, 11.0));	
		points->push_back (Gnome::Art::Point (0.0, 11.0));	
		points->push_back (Gnome::Art::Point (0.0, 0.0));	

		shift = 0;
		label_offset = 10.0;
		break;
		
	case  PunchOut:
		points = new ArdourCanvas::Points ();
		points->push_back (Gnome::Art::Point (0.0, 0.0));
		points->push_back (Gnome::Art::Point (8.0, 0.0));		
		points->push_back (Gnome::Art::Point (8.0, 11.0));		
		points->push_back (Gnome::Art::Point (4.0, 11.0));		
		points->push_back (Gnome::Art::Point (4.0, 4.0));		
		points->push_back (Gnome::Art::Point (0.0, 0.0));		

		shift = 8;
		label_offset = 11.0;
		break;
		
	}

	frame_position = frame;
	unit_position = editor.frame_to_unit (frame);

	/* adjust to properly locate the tip */

	unit_position -= shift;

	group = &parent;
	group->set_property ("x", unit_position);
	group->set_property ("y", 1.0);
	// cerr << "set mark al points, nc = " << points->num_points << endl;
	mark = new Polygon (*group);
	mark->set_property ("points", points);
	mark->set_property ("fill_color_rgba", rgba);
	mark->set_property ("outline_color", Gdk::Color ("black"));

	Pango::FontDescription font = get_font_for_style (N_("MarkerText"));

	text = new Text (*group);
	text->set_property ("text", annotation.c_str());
	text->set_property ("x", label_offset);
	text->set_property ("y", 0.0);
	text->set_property ("fontdesc", font);
	text->set_property ("anchor", Gtk::ANCHOR_NW);
	text->set_property ("fill_color", Gdk::Color ("black"));

	editor.ZoomChanged.connect (mem_fun (*this, &Marker::reposition));

	mark->set_data ("marker", this);

	if (handle_events) {
		group->signal_event().connect (bind (mem_fun (editor, &PublicEditor::canvas_marker_event), mark, this));
	}

}

Marker::~Marker ()
{
	/* destroying the parent group destroys its contents, namely any polygons etc. that we added */
	delete text;
	delete mark;
	delete points;
}

ArdourCanvas::Item&
Marker::the_item() const
{
	return *mark;
}

void
Marker::set_name (const string& name)
{
	text->set_property ("text", name.c_str());
}

void
Marker::set_position (jack_nframes_t frame)
{
	double new_unit_position = editor.frame_to_unit (frame);
	new_unit_position -= shift;
	group->move (new_unit_position - unit_position, 0.0);
	frame_position = frame;
	unit_position = new_unit_position;
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
	mark->set_property ("fill_color_rgba", color);
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

