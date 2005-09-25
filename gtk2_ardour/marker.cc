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

#include <ardour/tempo.h>

#include "marker.h"
#include "public_editor.h"
#include "canvas-simpleline.h"
#include "utils.h"

#include "i18n.h"

Marker::Marker (PublicEditor& ed, GnomeCanvasGroup *parent, guint32 rgba, const string& annotation, 
		Type type, gint (*callback)(GnomeCanvasItem *, GdkEvent *, gpointer), jack_nframes_t frame)

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
		points = gnome_canvas_points_new (6);

		points->coords[0] = 0.0;
		points->coords[1] = 0.0;
		
		points->coords[2] = 6.0;
		points->coords[3] = 0.0;
		
		points->coords[4] = 6.0;
		points->coords[5] = 5.0;
		
		points->coords[6] = 3.0;
		points->coords[7] = 10.0;
		
		points->coords[8] = 0.0;
		points->coords[9] = 5.0;
		
		points->coords[10] = 0.0;
		points->coords[11] = 0.0;
		
		shift = 3;
		label_offset = 8.0;
		break;

	case Tempo:
	case Meter:
		points = gnome_canvas_points_new (6);

		points->coords[0] = 3.0;
		points->coords[1] = 0.0;
		
		points->coords[2] = 6.0;
		points->coords[3] = 5.0;
		
		points->coords[4] = 6.0;
		points->coords[5] = 10.0;
		
		points->coords[6] = 0.0;
		points->coords[7] = 10.0;
		
		points->coords[8] = 0.0;
		points->coords[9] = 5.0;
		
		points->coords[10] = 3.0;
		points->coords[11] = 0.0;
		
		shift = 3;
		label_offset = 8.0;
		break;

	case Start:
		points = gnome_canvas_points_new (6);

		points->coords[0] = 0.0;
		points->coords[1] = 0.0;
		
		points->coords[2] = 5.0;
		points->coords[3] = 0.0;
		
		points->coords[4] = 10.0;
		points->coords[5] = 5.0;
		
		points->coords[6] = 5.0;
		points->coords[7] = 10.0;
		
		points->coords[8] = 0.0;
		points->coords[9] = 10.0;
		
		points->coords[10] = 0.0;
		points->coords[11] = 0.0;
		
		shift = 10;
		label_offset = 12.0;
		break;

	case End:
		points = gnome_canvas_points_new (6);

		points->coords[0] = 5.0;
		points->coords[1] = 0.0;
		
		points->coords[2] = 10.0;
		points->coords[3] = 0.0;
		
		points->coords[4] = 10.0;
		points->coords[5] = 10.0;
		
		points->coords[6] = 5.0;
		points->coords[7] = 10.0;
		
		points->coords[8] = 0.0;
		points->coords[9] = 5.0;
		
		points->coords[10] = 5.0;
		points->coords[11] = 0.0;
		
		shift = 0;
		label_offset = 12.0;
		break;

	case LoopStart:
		points = gnome_canvas_points_new (7);

		points->coords[0] = 0.0;
		points->coords[1] = 0.0;
		
		points->coords[2] = 4.0;
		points->coords[3] = 0.0;
		
		points->coords[4] = 4.0;
		points->coords[5] = 8.0;
		
		points->coords[6] = 8.0;
		points->coords[7] = 8.0;
		
		points->coords[8] = 8.0;
		points->coords[9] = 11.0;
		
		points->coords[10] = 0.0;
		points->coords[11] = 11.0;
		
		points->coords[12] = 0.0;
		points->coords[13] = 0.0;
		
		shift = 0;
		label_offset = 11.0;
		break;

	case LoopEnd:
		points = gnome_canvas_points_new (7);

		points->coords[0] = 8.0;
		points->coords[1] = 0.0;
		
		points->coords[2] = 8.0;
		points->coords[3] = 11.0;
		
		points->coords[4] = 0.0;
		points->coords[5] = 11.0;
		
		points->coords[6] = 0.0;
		points->coords[7] = 8.0;
		
		points->coords[8] = 4.0;
		points->coords[9] = 8.0;
		
		points->coords[10] = 4.0;
		points->coords[11] = 0.0;
		
		points->coords[12] = 8.0;
		points->coords[13] = 0.0;
		
		shift = 8;
		label_offset = 11.0;
		break;

	case  PunchIn:
		points = gnome_canvas_points_new (6);

		points->coords[0] = 0.0;
		points->coords[1] = 0.0;
		
		points->coords[2] = 8.0;
		points->coords[3] = 0.0;
		
		points->coords[4] = 4.0;
		points->coords[5] = 4.0;
		
		points->coords[6] = 4.0;
		points->coords[7] = 11.0;
		
		points->coords[8] = 0.0;
		points->coords[9] = 11.0;
		
		points->coords[10] = 0.0;
		points->coords[11] = 0.0;
		
		shift = 0;
		label_offset = 10.0;
		break;
		
	case  PunchOut:
		points = gnome_canvas_points_new (6);

		points->coords[0] = 0.0;
		points->coords[1] = 0.0;
		
		points->coords[2] = 8.0;
		points->coords[3] = 0.0;
		
		points->coords[4] = 8.0;
		points->coords[5] = 11.0;
		
		points->coords[6] = 4.0;
		points->coords[7] = 11.0;
		
		points->coords[8] = 4.0;
		points->coords[9] = 4.0;
		
		points->coords[10] = 0.0;
		points->coords[11] = 0.0;
		
		shift = 8;
		label_offset = 11.0;
		break;
		
	}

	frame_position = frame;
	unit_position = editor.frame_to_unit (frame);

	/* adjust to properly locate the tip */

	unit_position -= shift;

	group = gnome_canvas_item_new (parent,
				     gnome_canvas_group_get_type(),
				     "x", unit_position,
				     "y", 1.0,
				     NULL);

	// cerr << "set mark al points, nc = " << points->num_points << endl;
	mark = gnome_canvas_item_new (GNOME_CANVAS_GROUP(group),
				    gnome_canvas_polygon_get_type(),
				    "points", points,
				    "fill_color_rgba", rgba,
				    "outline_color", "black",
				    NULL);

	string fontname = get_font_for_style (N_("MarkerText"));

	text = gnome_canvas_item_new (GNOME_CANVAS_GROUP(group),
				    gnome_canvas_text_get_type (),
				    "text", annotation.c_str(),
				    "x", label_offset,
				    "y", 0.0,
				    "font", fontname.c_str(),
				    "anchor", GTK_ANCHOR_NW,
				    "fill_color", "black",
				    NULL);

	gtk_object_set_data (GTK_OBJECT(group), "marker", this);
	gtk_signal_connect (GTK_OBJECT(group), "event", (GtkSignalFunc) callback, &editor);

	editor.ZoomChanged.connect (mem_fun(*this, &Marker::reposition));
}

Marker::~Marker ()
{
	/* destroying the group destroys its contents */
	gtk_object_destroy (GTK_OBJECT(group));
	gnome_canvas_points_unref (points);
}

void
Marker::set_name (const string& name)
{
	gnome_canvas_item_set (text, "text", name.c_str(), NULL);
}

void
Marker::set_position (jack_nframes_t frame)
{
	double new_unit_position = editor.frame_to_unit (frame);
	new_unit_position -= shift;
	gnome_canvas_item_move (group, new_unit_position - unit_position, 0.0);
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
	gnome_canvas_item_show (group);
}

void
Marker::hide ()
{
	gnome_canvas_item_hide (group);
}

void
Marker::set_color_rgba (uint32_t color)
{
	gnome_canvas_item_set (mark, "fill_color_rgba", color, NULL);
}

/***********************************************************************/

TempoMarker::TempoMarker (PublicEditor& editor, GnomeCanvasGroup *parent, guint32 rgba, const string& text, 
			  ARDOUR::TempoSection& temp, 
			  gint (*callback)(GnomeCanvasItem *, GdkEvent *, gpointer))
	: Marker (editor, parent, rgba, text, Tempo, callback, 0),
	  _tempo (temp)
{
	set_position (_tempo.frame());
	gtk_object_set_data (GTK_OBJECT(group), "tempo_marker", this);
}

TempoMarker::~TempoMarker ()
{
}

/***********************************************************************/

MeterMarker::MeterMarker (PublicEditor& editor, GnomeCanvasGroup *parent, guint32 rgba, const string& text, 
			  ARDOUR::MeterSection& m, 
			  gint (*callback)(GnomeCanvasItem *, GdkEvent *, gpointer))
	: Marker (editor, parent, rgba, text, Meter, callback, 0),
	  _meter (m)
{
	set_position (_meter.frame());
	gtk_object_set_data (GTK_OBJECT(group), "meter_marker", this);
}

MeterMarker::~MeterMarker ()
{
}
