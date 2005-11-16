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

Editor::Cursor::Cursor (Editor& ed, const string& color, bool (Editor::*callbck)(GdkEvent*,ArdourCanvas::Item*))
	: editor (ed), length(1.0)
{
	
	/* "randomly" initialize coords */

	points->push_back(Gnome::Art::Point(-9383839.0, 0.0));
	points->push_back(Gnome::Art::Point(1.0, 0.0));
	Gnome::Canvas::Group *group = editor.cursor_group;

	// cerr << "set cursor points, nc = " << points->num_points << endl;

	canvas_item = new Gnome::Canvas::Line (*group, *points);
	canvas_item->set_property ("fill_color", color.c_str());
	canvas_item->set_property ("width_pixels", 1);
	canvas_item->set_property ("first_arrowhead", (gboolean) TRUE);
	canvas_item->set_property ("last_arrowhead", (gboolean) TRUE);
	canvas_item->set_property ("arrow_shape_a", 11.0);
	canvas_item->set_property ("arrow_shape_b", 0.0);
	canvas_item->set_property ("arrow_shape_c", 9.0);

	// cerr << "cursor line @ " << canvas_item << endl;

	canvas_item->set_data ("cursor", this);
	canvas_item->signal_event().connect (bind (mem_fun (ed, callbck), canvas_item));

	current_frame = 1; /* force redraw at 0 */
}

Editor::Cursor::~Cursor ()

{
        gtk_object_destroy (GTK_OBJECT(canvas_item));
	gnome_canvas_points_unref (points->gobj());
}

void
Editor::Cursor::set_position (jack_nframes_t frame)
{
	double new_pos =  editor.frame_to_unit (frame);

	if (editor.session == 0) {
		canvas_item->hide();
	} else {
		canvas_item->show();
	}

	current_frame = frame;

	if (new_pos == points->front().get_x()) {

		/* change in position is not visible, so just raise it */

		canvas_item->raise_to_top();
		return;
	} 

	points->front().set_x(new_pos);
	points->back().set_x(new_pos);

	// cerr << "set cursor2 al points, nc = " << points->num_points << endl;
	canvas_item->set_property ("points", points);
	canvas_item->raise_to_top();
}

void
Editor::Cursor::set_length (double units)
{
	length = units; 
	points->back().set_x (points->front().get_y() + length);
	// cerr << "set cursor3 al points, nc = " << points->num_points << endl;
	canvas_item->set_property("points", points);
}

void 
Editor::Cursor::set_y_axis (double position)
{
        points->front().set_y (position);
	points->back().set_x (position + length);
	// cerr << "set cursor4 al points, nc = " << points->num_points << endl;
	canvas_item->set_property("points", points);
}
