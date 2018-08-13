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

#ifndef __CANVAS_STEP_H__
#define __CANVAS_STEP_H__

#include <cairomm/pattern.h>

#include "canvas/visibility.h"
#include "canvas/item.h"
#include "canvas/types.h"

#include "gtkmm2ext/colors.h"

namespace Cairo {
class LinearGradient;
}

namespace ArdourCanvas
{
class Text;

class LIBCANVAS_API StepButton : public Item
{
public:
	StepButton (Canvas*, double width, double height, Gtkmm2ext::Color c = Gtkmm2ext::rgba_to_color (0, 0, 0, 1.0));

	void render (Rect const &, Cairo::RefPtr<Cairo::Context>) const;
	void compute_bounding_box () const;

	void set_value (double val);
	double value() const { return current_value; }

	void set_size (double w, double h);
	void set_highlight (bool);
	void set_color (Gtkmm2ext::Color);

	Text* text() const { return label; }

  private:
	double width;
	double height;
	Text* label;
	double current_value;
	bool prelight;
	bool highlight;
	bool dragging;
	bool clicking;
	double scale;
	Gtkmm2ext::HSV color;

	Cairo::RefPtr<Cairo::LinearGradient> inactive_pattern;
	Cairo::RefPtr<Cairo::LinearGradient> enabled_pattern;

	void create_patterns ();
	bool event_handler (GdkEvent*);
};

}

#endif
