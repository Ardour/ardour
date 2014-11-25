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
#include "canvas/canvas.h"

class Editor;

namespace ArdourCanvas {
     class TrackingText;
}

class VerboseCursor
{
public:
	VerboseCursor (Editor *);

	ArdourCanvas::Item* canvas_item () const;
	bool visible () const;

	void set (std::string const &);
	void set_time (framepos_t);
	void set_duration (framepos_t, framepos_t);
	void set_offset (ArdourCanvas::Duple const&);

	void show ();
	void hide ();

        std::string format_time (framepos_t);
        std::string format_duration (framepos_t, framepos_t);
	
private:
	Editor*                     _editor;
	ArdourCanvas::TrackingText* _canvas_item;

	void color_handler ();
};
