/*
 * Copyright (C) 2005-2008 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2005-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2014-2017 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <cstdlib>
#include <cmath>

#include "canvas/canvas.h"
#include "canvas/debug.h"
#include "canvas/scroll_group.h"

#include "editor_cursors.h"
#include "editing_context.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;

EditorCursor::EditorCursor (EditingContext& ed, bool (EditingContext::*callback)(GdkEvent*,ArdourCanvas::Item*), std::string const & name)
	: _editor (ed)
	, _track_canvas_item (new ArdourCanvas::Arrow (_editor.get_cursor_scroll_group()))
{
	CANVAS_DEBUG_NAME (_track_canvas_item, string_compose ("track canvas editor cursor <%1>", name));

	_track_canvas_item->set_show_head (0, true);
	_track_canvas_item->set_head_height (0, 9);
	_track_canvas_item->set_head_width (0, 16);
	_track_canvas_item->set_head_outward (0, false);
	_track_canvas_item->set_show_head (1, false); // head only
	_track_canvas_item->set_data ("cursor", this);

	_track_canvas_item->Event.connect (sigc::bind (sigc::mem_fun (ed, callback), _track_canvas_item));

	_track_canvas_item->set_y1 (ArdourCanvas::COORD_MAX);

	_track_canvas_item->set_x (0);

	_current_sample = 1; /* force redraw at 0 */
}

EditorCursor::EditorCursor (EditingContext& ed, std::string const & name)
	: _editor (ed)
	, _track_canvas_item (new ArdourCanvas::Arrow (_editor.get_hscroll_group()))
{
	CANVAS_DEBUG_NAME (_track_canvas_item, string_compose ("track canvas cursor <%1>", name));

	_track_canvas_item->set_show_head (0, false);
	_track_canvas_item->set_show_head (1, false);
	_track_canvas_item->set_y1 (ArdourCanvas::COORD_MAX);
	_track_canvas_item->set_ignore_events (true);

	_track_canvas_item->set_x (0);

	_current_sample = 1; /* force redraw at 0 */
}

EditorCursor::~EditorCursor ()
{
	delete _track_canvas_item;
}

void
EditorCursor::set_position (samplepos_t sample)
{
	if (_current_sample != sample) {
		PositionChanged (sample);
	}

	const double new_pos = _editor.sample_to_pixel (sample);

	if (new_pos != _track_canvas_item->x ()) {
		_track_canvas_item->set_x (new_pos);
	}

	_current_sample = sample;
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
EditorCursor::set_color (Gtkmm2ext::Color color)
{
	_track_canvas_item->set_color (color);
}

void
EditorCursor::set_sensitive (bool yn)
{
	_track_canvas_item->set_ignore_events (!yn);
}
