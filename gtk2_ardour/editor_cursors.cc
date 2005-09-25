/*
    Copyright (C) 2000 Paul Davis 

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

#include <cstdlib>
#include <cmath>

#include <libgnomecanvas/libgnomecanvas.h>

#include "utils.h"
#include "editor.h"

using namespace sigc;
using namespace ARDOUR;
using namespace Gtk;

Editor::Cursor::Cursor (Editor& ed, const string& color, GtkSignalFunc callbck)
	: editor (ed), callback (callbck), length(1.0)
{
	GnomeCanvasGroup *group;
	points = gnome_canvas_points_new (2);
	
	/* "randomly" initialize coords */

	points->coords[0] = -9383839.0;
	points->coords[1] = 0.0;
	points->coords[2] = 1.0;
	points->coords[3] = 0.0;

	group = GNOME_CANVAS_GROUP (editor.cursor_group);

	// cerr << "set cursor points, nc = " << points->num_points << endl;
	canvas_item = gnome_canvas_item_new (group,
					   gnome_canvas_line_get_type(),
					   "points", points,
					   "fill_color", color.c_str(),
					   "width_pixels", 1,
					   "first_arrowhead", (gboolean) TRUE,
					   "last_arrowhead", (gboolean) TRUE,
					   "arrow_shape_a", 11.0,
					   "arrow_shape_b", 0.0,
					   "arrow_shape_c", 9.0,
					   NULL);

	// cerr << "cursor line @ " << canvas_item << endl;

	gtk_object_set_data (GTK_OBJECT(canvas_item), "cursor", this);
	gtk_signal_connect (GTK_OBJECT(canvas_item), "event", callback, &editor);

	current_frame = 1; /* force redraw at 0 */
}

Editor::Cursor::~Cursor ()

{
	gtk_object_destroy (GTK_OBJECT(canvas_item));
	gnome_canvas_points_unref (points);
}

void
Editor::Cursor::set_position (jack_nframes_t frame)
{
	double new_pos =  editor.frame_to_unit (frame);

	if (editor.session == 0) {
		gnome_canvas_item_hide (canvas_item);
	} else {
		gnome_canvas_item_show (canvas_item);
	}

	current_frame = frame;

	if (new_pos == points->coords[0]) {

		/* change in position is not visible, so just raise it */
		
		gnome_canvas_item_raise_to_top (canvas_item);
		return;
	} 

	points->coords[0] = new_pos;
	points->coords[2] = new_pos;

	// cerr << "set cursor2 al points, nc = " << points->num_points << endl;
	gnome_canvas_item_set (canvas_item, "points", points, NULL);
	gnome_canvas_item_raise_to_top (canvas_item);
}

void
Editor::Cursor::set_length (double units)
{
	length = units; 
	points->coords[3] = points->coords[1] + length;
	// cerr << "set cursor3 al points, nc = " << points->num_points << endl;
	gnome_canvas_item_set (canvas_item, "points", points, NULL);
}

void 
Editor::Cursor::set_y_axis (double position)
{
	points->coords[1] = position;
	points->coords[3] = position + length;
	// cerr << "set cursor4 al points, nc = " << points->num_points << endl;
	gnome_canvas_item_set (canvas_item, "points", points, NULL);
}
