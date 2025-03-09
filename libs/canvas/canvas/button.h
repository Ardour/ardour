/*
    Copyright (C) 2011-2013 Paul Davis
    Author: Carl Hetherington <cth@carlh.net>

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

#pragma once

#include <cairomm/pattern.h>

#include "canvas/visibility.h"
#include "canvas/rectangle.h"
#include "canvas/types.h"

namespace Pango {
	class FontDescription;
}

namespace ArdourCanvas
{
class Text;

class LIBCANVAS_API Button : public Rectangle
{
public:
	Button (Canvas*, double width, double height, Pango::FontDescription const &);
	Button (Item*, double width, double height, Pango::FontDescription const &);

	Button (Canvas*, std::string const &, Pango::FontDescription const &);
	Button (Item*, std::string const &, Pango::FontDescription const &);

	void render (Rect const &, Cairo::RefPtr<Cairo::Context>) const;
	void compute_bounding_box () const;

	void set_label (std::string const &);
	std::string label() const;
	Text* text() const { return _label; }

	void set_size (double w, double h);
	void set_highlight (bool);

  private:
	double width;
	double height;
	Text* _label;
	bool prelight;
	bool highlight;
	bool clicking;
	Gtkmm2ext::HSV color;

	bool event_handler (GdkEvent*);
	void init ();
};

}

