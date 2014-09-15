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
#include "canvas/debug.h"
#include "canvas/scroll_group.h"

#include "editor_cursors.h"
#include "editor.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;

EditorCursor::EditorCursor (Editor& ed, bool (Editor::*callbck)(GdkEvent*,ArdourCanvas::Item*))
	: _editor (ed)
	, _track_canvas_item (new ArdourCanvas::Arrow (_editor.get_hscroll_group()))
	, _length (1.0)
{
	CANVAS_DEBUG_NAME (_track_canvas_item, "track canvas editor cursor");

	_track_canvas_item->set_show_head (0, true);
	_track_canvas_item->set_head_height (0, 9);
	_track_canvas_item->set_head_width (0, 16);
	_track_canvas_item->set_head_outward (0, false);
	_track_canvas_item->set_show_head (1, false); // head only
	_track_canvas_item->set_data ("cursor", this);

	_track_canvas_item->Event.connect (sigc::bind (sigc::mem_fun (ed, callbck), _track_canvas_item));

	_track_canvas_item->set_y1 (ArdourCanvas::COORD_MAX);

    _track_canvas_item->set_x (0);
	
	_current_frame = 1; /* force redraw at 0 */
}

EditorCursor::EditorCursor (Editor& ed)
	: _editor (ed)
	, _track_canvas_item (new ArdourCanvas::Arrow (_editor.get_hscroll_group()))
	, _length (1.0)
{
	CANVAS_DEBUG_NAME (_track_canvas_item, "track canvas cursor");

	_track_canvas_item->set_show_head (0, false);
	_track_canvas_item->set_show_head (1, false);
	_track_canvas_item->set_y1 (ArdourCanvas::COORD_MAX);
	_track_canvas_item->set_ignore_events (true);
	
    _track_canvas_item->set_x (0);
    
	_current_frame = 1; /* force redraw at 0 */
}

EditorCursor::~EditorCursor ()
{
	delete _track_canvas_item;
}

void
EditorCursor::set_position (framepos_t frame)
{
	PositionChanged (frame);

	double const new_pos = _editor.sample_to_pixel_unrounded (frame);

	if (new_pos != _track_canvas_item->x ()) {
		_track_canvas_item->set_x (new_pos);
	}

	_current_frame = frame;
}

void
EditorCursor::show ()
{
	_track_canvas_item->show ();
}

void
EditorCursor::hide ()
{
	_track_canvas_item->hide ();
}

void
EditorCursor::set_color (ArdourCanvas::Color color)
{
	_track_canvas_item->set_color (color);
}

void
EditorCursor::set_sensitive (bool yn)
{
	_track_canvas_item->set_ignore_events (!yn);
}
