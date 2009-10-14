/*
    Copyright (C) 2003 Paul Davis

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

#include <algorithm>
#include <string>

#include <list>

#include "pbd/error.h"

#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/selector.h>
#include <gtkmm2ext/gtk_ui.h>

#include "ardour/session.h"
#include "ardour/utils.h"

#include "public_editor.h"
#include "axis_view.h"
#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;

list<Gdk::Color> AxisView::used_colors;

AxisView::AxisView (ARDOUR::Session& sess) : _session(sess)
{
	_selected = false;
	_marked_for_display = false;
}

AxisView::~AxisView()
{

}

Gdk::Color
AxisView::unique_random_color()
{
  	Gdk::Color newcolor;

	while (1) {

		/* avoid neon/glowing tones by limiting them to the
		   "inner section" (paler) of a color wheel/circle.
		*/

		const int32_t max_saturation = 48000; // 65535 would open up the whole color wheel

		newcolor.set_red (random() % max_saturation);
		newcolor.set_blue (random() % max_saturation);
		newcolor.set_green (random() % max_saturation);

		if (used_colors.size() == 0) {
			used_colors.push_back (newcolor);
			return newcolor;
		}

		for (list<Gdk::Color>::iterator i = used_colors.begin(); i != used_colors.end(); ++i) {
		  Gdk::Color c = *i;
			float rdelta, bdelta, gdelta;

			rdelta = newcolor.get_red() - c.get_red();
			bdelta = newcolor.get_blue() - c.get_blue();
			gdelta = newcolor.get_green() - c.get_green();

			if (sqrt (rdelta*rdelta + bdelta*bdelta + gdelta*gdelta) > 25.0) {
				used_colors.push_back (newcolor);
				return newcolor;
			}
		}

		/* XXX need throttle here to make sure we don't spin for ever */
	}
}
