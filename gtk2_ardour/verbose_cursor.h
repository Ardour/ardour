/*
    Copyright (C) 2000-2011 Paul Davis

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

#include "ardour/types.h"
#include "canvas/text.h"
#include "canvas/canvas.h"

class Editor;

class VerboseCursor
{
public:
	VerboseCursor (Editor *);

	ArdourCanvas::Item* canvas_item () const;
	bool visible () const;

	void set_color (uint32_t);

	void set (std::string const &, double, double);
	void set_text (std::string const &);
	void set_position (double, double);
	void set_time (framepos_t, double, double);
	void set_duration (framepos_t, framepos_t, double, double);

	void show (double xoffset = 0, double yoffset = 0);
	void hide ();

         ArdourCanvas::Item& item() { return *_canvas_item; }

private:
	double clamp_x (double);
	double clamp_y (double);

	Editor*             _editor;
	ArdourCanvas::Text* _canvas_item;
	bool                _visible;
	double              _xoffset;
	double              _yoffset;
};
