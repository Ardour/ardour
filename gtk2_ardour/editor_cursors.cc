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

*/

#include <cstdlib>
#include <cmath>

#include "canvas/canvas.h"

#include "utils.h"
#include "editor_cursors.h"
#include "editor.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;

EditorCursor::EditorCursor (Editor& ed, bool (Editor::*callbck)(GdkEvent*,ArdourCanvas::Item*))
	: _editor (ed)
	, _time_bars_canvas_item (_editor._time_bars_canvas->root ())
	, _track_canvas_item (_editor._track_canvas->root ())
	, _length (1.0)
{
	_time_bars_canvas_item.set_outline_width (1);
	_track_canvas_item.set_outline_width (1);

	_time_bars_canvas_item.set_show_head (0, true);
	_time_bars_canvas_item.set_head_height (0, 9);
	_time_bars_canvas_item.set_head_width (0, 16);
	_time_bars_canvas_item.set_head_outward (0, false);
	_time_bars_canvas_item.set_show_head (1, false); // head only

	_time_bars_canvas_item.set_data ("cursor", this);
	_track_canvas_item.set_data ("cursor", this);

	_time_bars_canvas_item.Event.connect (sigc::bind (sigc::mem_fun (ed, callbck), &_time_bars_canvas_item));
	_track_canvas_item.Event.connect (sigc::bind (sigc::mem_fun (ed, callbck), &_track_canvas_item));

	_time_bars_canvas_item.set_y1 (ArdourCanvas::COORD_MAX);
	_track_canvas_item.set_y1 (ArdourCanvas::COORD_MAX);
	
	_current_frame = 1; /* force redraw at 0 */
}

EditorCursor::~EditorCursor ()
{
	
}

void
EditorCursor::set_position (framepos_t frame)
{
	PositionChanged (frame);

	double const new_pos = _editor.frame_to_unit (frame);

	if (new_pos != _time_bars_canvas_item.x ()) {
		_time_bars_canvas_item.set_x (new_pos);
	}

	if (new_pos != _track_canvas_item.x0 ()) {
		_track_canvas_item.set_x0 (new_pos);
		_track_canvas_item.set_x1 (new_pos);
	}
	
	_current_frame = frame;
}

void
EditorCursor::show ()
{
	_time_bars_canvas_item.show ();
	_track_canvas_item.show ();
}

void
EditorCursor::hide ()
{
	_time_bars_canvas_item.hide ();
	_track_canvas_item.hide ();
}

void
EditorCursor::set_color (ArdourCanvas::Color color)
{
	_time_bars_canvas_item.set_color (color);
	_track_canvas_item.set_outline_color (color);
}
