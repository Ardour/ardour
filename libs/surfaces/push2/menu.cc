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
#include <pangomm/layout.h>

#include "push2.h"
#include "gui.h"

using namespace ARDOUR;
using namespace std;
using namespace PBD;
using namespace Glib;
using namespace ArdourSurface;

#include "i18n.h"
#include "menu.h"

Push2Menu::Push2Menu (Cairo::RefPtr<Cairo::Context> context)
	: _dirty (true)
{
	Pango::FontDescription fd2 ("Sans 10");

	{
		Glib::RefPtr<Pango::Layout> throwaway = Pango::Layout::create (context);
		throwaway->set_font_description (fd2);
		throwaway->set_text (X_("Hg")); /* ascender + descender) */
		int h, w;
		throwaway->get_pixel_size (w, h);
		baseline = h;
		nrows = Push2::rows / baseline;
	}

	for (int n = 0; n < 8; ++n) {
		columns[n].layout = Pango::Layout::create (context);
		columns[n].layout->set_font_description (fd2);
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

	_dirty = true;
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

	columns[col].layout->set_text (rows);
	columns[col].top = top_row;

	_dirty = true;
}

void
Push2Menu::scroll (int col, int dir)
{
	if (dir > 0) {
		set_text (col, columns[col].top + 1);
	} else {
		set_text (col, columns[col].top - 1);
	}
}

void
Push2Menu::set_active (int col, int index)
{
	if (col < 0 || col > 7) {
		return;
	}

	if (index < 0 || index > (int) columns[col].text.size()) {
		return;
	}

	columns[col].active = index;

	ActiveChanged (); /* emit signal */

	_dirty = true;
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
			columns[col].active = 0;
		} else {
			columns[col].active = columns[col].active - 1;
			if (columns[col].active < 0) {
				columns[col].active = columns[col].text.size() - 1;
			}
		}
	} else {
		if (columns[col].active == -1) {
			columns[col].active = 0;
		} else {
			columns[col].active = columns[col].active + 1;
			if (columns[col].active >= (int) columns[col].text.size()) {
				columns[col].active = 0;
			}
		}
	}

	if (columns[col].active < nrows/2) {
		set_text (col, 0);
	} else {
		set_text (col, columns[col].active - (nrows/2) + 1);
	}

	_dirty = true;
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
Push2Menu::redraw (Cairo::RefPtr<Cairo::Context> context, bool force) const
{
	for (int n = 0; n < 8; ++n) {

		/* Active: move to column/now, draw background indicator
		   for active row.
		*/

		const double x = 10.0 + (n * 120.0);
		const double y = 2.0;

		if (columns[n].active >= 0) {
			int effective_row = columns[n].active - columns[n].top;
			context->rectangle (x, y + (effective_row * baseline), 120.0, baseline);
			context->set_source_rgb (1.0, 1.0, 1.0);
			context->fill ();
		}

		/* now draw all the text, in one go */

		context->move_to (x, y);
		context->set_source_rgb (0.23, 0.0, 0.349);
		columns[n].layout->update_from_cairo_context (context);
		columns[n].layout->show_in_cairo_context (context);

	}

	_dirty = false;
}
