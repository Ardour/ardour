/*
	Copyright (C) 2016 Paul Davis

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <cairomm/context.h>
#include <cairomm/surface.h>
#include <cairomm/region.h>
#include <pangomm/layout.h>

#include "canvas/text.h"
#include "canvas/rectangle.h"
#include "canvas/colors.h"

#include "canvas.h"
#include "gui.h"
#include "push2.h"

using namespace ARDOUR;
using namespace std;
using namespace PBD;
using namespace Glib;
using namespace ArdourSurface;
using namespace ArdourCanvas;

#include "pbd/i18n.h"
#include "menu.h"

Push2Menu::Push2Menu (Item* parent)
	: Container (parent)
	, baseline (-1)
{
	Pango::FontDescription fd2 ("Sans 10");

	if (baseline < 0) {
		Push2Canvas* p2c = dynamic_cast<Push2Canvas*> (canvas());
		Glib::RefPtr<Pango::Layout> throwaway = Pango::Layout::create (p2c->image_context());
		throwaway->set_font_description (fd2);
		throwaway->set_text (X_("Hg")); /* ascender + descender) */
		int h, w;
		throwaway->get_pixel_size (w, h);
		baseline = h;
		// nrows = Push2::rows / baseline;
	}


	for (int n = 0; n < 8; ++n) {
		Text* t = new Text (this);
		t->set_font_description (fd2);
		t->set_color (rgba_to_color (0.23, 0.0, 0.349, 1.0));

		const double x = 10.0 + (n * Push2Canvas::inter_button_spacing());
		const double y = 2.0;
		t->set_position (Duple (x, y));

		Rectangle* r = new Rectangle (this);
		r->set (Rect (x, y, x + Push2Canvas::inter_button_spacing(), y + baseline));

		columns[n].lines = t;
		columns[n].active_bg = r;
		columns[n].top = -1;
		columns[n].active = -1;
	}
}

void
Push2Menu::fill_column (int col, vector<string> v)
{
	if (col < 0 || col > 7) {
		return;
	}

	columns[col].text = v;

	if (v.empty()) {
		columns[col].active = -1;
	} else {
		columns[col].active = 0;
	}

	set_text (col, 0);
}

void
Push2Menu::set_text (int col, int top_row)
{
	if (top_row > (int) columns[col].text.size() - nrows || top_row < 0) {
		return;
	}

	if (top_row == columns[col].top) {
		return;
	}


	vector<string>::iterator s = columns[col].text.begin();
	s += top_row;

	string rows;

	while (true) {
		rows += *s;
		++s;
		if (s != columns[col].text.end()) {
			rows += '\n';
		} else {
			break;
		}
	}

	columns[col].lines->set (rows);
	columns[col].top = top_row;

	redraw ();
}

void
Push2Menu::scroll (int col, int dir)
{
	if (dir > 0) {
		set_text (col, columns[col].top + 1);
	} else {
		set_text (col, columns[col].top - 1);
	}

	redraw ();
}

void
Push2Menu::set_active (int col, int index)
{
	if (col < 0 || col > 7) {
		columns[col].active_bg->hide ();
		return;
	}

	if (index < 0 || index > (int) columns[col].text.size()) {
		columns[col].active_bg->hide ();
		return;
	}

	columns[col].active = index;
	int effective_row = columns[col].active - columns[col].top;

	/* Move active bg */

	Duple p (columns[col].active_bg->position());

	columns[col].active_bg->set (Rect (p.x, p.y + (effective_row * baseline),
	                                   p.x + Push2Canvas::inter_button_spacing(), p.y + baseline));
	columns[col].active_bg->show ();

	if (columns[col].active < nrows/2) {
		set_text (col, 0);
	} else {
		set_text (col, columns[col].active - (nrows/2) + 1);
	}

	ActiveChanged (); /* emit signal */

	redraw ();
}

void
Push2Menu::step_active (int col, int dir)
{
	if (col < 0 || col > 7) {
		return;
	}

	if (columns[col].text.empty()) {
		return;
	}


	if (dir < 0) {
		if (columns[col].active == -1) {
			set_active (col, -1);
		} else {
			columns[col].active = columns[col].active - 1;
			if (columns[col].active < 0) {
				set_active (col, columns[col].text.size() - 1);
			}
		}
	} else {
		if (columns[col].active == -1) {
			set_active (col, 0);
		} else {
			columns[col].active = columns[col].active + 1;
			if (columns[col].active >= (int) columns[col].text.size()) {
				set_active (col, 0);
			}
		}
	}

	redraw ();
}

int
Push2Menu::get_active (int col)
{
	if (col < 0 || col > 7) {
		return -1;
	}

	return columns[col].active;
}

void
Push2Menu::render (Rect const& area, Cairo::RefPtr<Cairo::Context> context) const
{
	render_children (area, context);
}
