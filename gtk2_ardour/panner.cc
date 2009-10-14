/*
    Copyright (C) 2000-2007 Paul Davis

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

#include <iostream>
#include <iomanip>
#include <cstring>
#include "ardour/panner.h"
#include "panner.h"
#include "i18n.h"

using namespace std;
using namespace Gtk;

static const int triangle_size = 5;

PannerBar::PannerBar (Adjustment& adj, boost::shared_ptr<PBD::Controllable> c)
	: BarController (adj, c)
{
	set_style (BarController::Line);
}

PannerBar::~PannerBar ()
{
}

bool
PannerBar::expose (GdkEventExpose* ev)
{
	Glib::RefPtr<Gdk::Window> win (darea.get_window());
	Glib::RefPtr<Gdk::GC> gc (get_style()->get_base_gc (get_state()));

	BarController::expose (ev);

	/* now draw triangles for left, right and center */

	GdkPoint points[3];

	// left

	points[0].x = 0;
	points[0].y = 0;

	points[1].x = triangle_size;
	points[1].y = 0;

	points[2].x = 0;
	points[2].y = triangle_size;

	gdk_draw_polygon (win->gobj(), gc->gobj(), true, points, 3);

	// center

	points[0].x = (darea.get_width()/2 - triangle_size);
	points[0].y = 0;

	points[1].x = (darea.get_width()/2 + triangle_size);
	points[1].y = 0;

	points[2].x = darea.get_width()/2;
	points[2].y = triangle_size - 1;

	gdk_draw_polygon (win->gobj(), gc->gobj(), true, points, 3);

	// right

	points[0].x = (darea.get_width() - triangle_size);
	points[0].y = 0;

	points[1].x = darea.get_width();
	points[1].y = 0;

	points[2].x = darea.get_width();
	points[2].y = triangle_size;

	gdk_draw_polygon (win->gobj(), gc->gobj(), true, points, 3);

	return true;
}

bool
PannerBar::button_press (GdkEventButton* ev)
{
	if (ev->button == 1 && ev->type == GDK_BUTTON_PRESS && ev->y < 10) {
		if (ev->x < triangle_size) {
			adjustment.set_value (adjustment.get_lower());
		} else if (ev->x > (darea.get_width() - triangle_size)) {
			adjustment.set_value (adjustment.get_upper());
		} else if (ev->x > (darea.get_width()/2 - triangle_size) &&
			   ev->x < (darea.get_width()/2 + triangle_size)) {
			adjustment.set_value (adjustment.get_lower() + ((adjustment.get_upper() - adjustment.get_lower()) / 2.0));
		}
	}

	return BarController::button_press (ev);
}

bool
PannerBar::button_release (GdkEventButton* ev)
{
	bool const r = BarController::button_release (ev);

	/* get rid of any `C' labels that may exist */
	queue_draw ();
	return r;
}

bool
PannerBar::entry_input (double *new_value)
{
	Entry* e = dynamic_cast<Entry*> (&spinner);
	string const text = e->get_text ();

	string digits;
	string letters;

	string const L = _("L");
	string const C = _("C");
	string const R = _("R");

	for (string::size_type i = 0; i < text.length(); ++i) {
		if (isdigit (text[i])) {
			digits += text[i];
		} else if (text[i] != '%') {
			letters += text[i];
		}
	}

	if (letters.empty()) {
		/* no letter specified, so take any number as a percentage where
		 * 0 is left and 100 right */
		*new_value = digits.empty() ? 0.5 : (atoi (digits.c_str()) / 100.0);
	} else {
		/* letter given, so value is a percentage to the extreme
		 * (e.g. 100L is full left, 1L is slightly left */
		if (letters[0] == L[0] || letters[0] == tolower (L[0])) {
			*new_value = digits.empty() ? 0 : (0.5 - atoi (digits.c_str()) / 200.0);
		} else if (letters[0] == R[0] || letters[0] == tolower (R[0])) {
			*new_value = digits.empty() ? 1 : 0.5 + atoi (digits.c_str()) / 200.0;
		} else if (letters[0] == C[0] || letters[0] == tolower (C[0])) {
			*new_value = 0.5;
		}
	}

	return true;
}

bool
PannerBar::entry_output ()
{
	Entry* e = dynamic_cast<Entry*> (&spinner);
	e->set_text (value_as_string (spinner.get_adjustment()->get_value()));
	return true;
}

string
PannerBar::value_as_string (double v) const
{
	if (ARDOUR::Panner::equivalent (v, 0.5)) {
		return _("C");
	} else if (ARDOUR::Panner::equivalent (v, 0)) {
		return _("L");
	} else if (ARDOUR::Panner::equivalent (v, 1)) {
		return _("R");
	} else if (v < 0.5) {
		std::stringstream s;
		s << fixed << setprecision (0) << _("L") << ((0.5 - v) * 200) << "%";
		return s.str();
	} else if (v > 0.5) {
		std::stringstream s;
		s << fixed << setprecision (0) << _("R") << ((v -0.5) * 200) << "%";
		return s.str ();
	}

	return "";
}

std::string
PannerBar::get_label (int& x)
{
	double const value = spinner.get_adjustment()->get_value ();

	if (ARDOUR::Panner::equivalent (value, 0.5)) {

		/* centre: only display text during a drag */

		if (!grabbed) {
			return "";
		}

	} else {

		/* non-centre: display text on the side of the panner which has more space */

 		Glib::RefPtr<Pango::Context> p = get_pango_context ();
 		Glib::RefPtr<Pango::Layout> l = Pango::Layout::create (p);
 		l->set_text (value_as_string (value));

 		Pango::Rectangle const ext = l->get_ink_extents ();

 		if (value < 0.5) {
 			x = (darea.get_width() - 4 - ext.get_width() / Pango::SCALE);
 		} else {
 			x = 4;
 		}
 	}

	return value_as_string (value);
}
