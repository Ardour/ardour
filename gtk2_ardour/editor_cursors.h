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

#include "pbd/signals.h"

class Editor;

class EditorCursor {
public:
	Editor&              editor;
	ArdourCanvas::Points points;
	ArdourCanvas::Line   canvas_item;
	framepos_t           current_frame;
	double               length;

	EditorCursor (Editor&, bool (Editor::*)(GdkEvent*,ArdourCanvas::Item*));
	~EditorCursor ();

	void set_position (framepos_t);
	void set_length (double units);
	void set_y_axis (double position);

	PBD::Signal1<void, framepos_t> PositionChanged;
};
