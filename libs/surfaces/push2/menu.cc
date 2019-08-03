/*
 * Copyright (C) 2016-2018 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2016 Paul Davis <paul@linuxaudiosystems.com>
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

#include <cairomm/context.h>
#include <cairomm/surface.h>
#include <cairomm/region.h>
#include <pangomm/layout.h>

#include "canvas/text.h"
#include "canvas/types.h"
#include "canvas/rectangle.h"
#include "gtkmm2ext/colors.h"

#include "canvas.h"
#include "gui.h"
#include "push2.h"

#include "menu.h"

#include "pbd/i18n.h"

#ifdef __APPLE__
#define Rect ArdourCanvas::Rect
#endif

using namespace ARDOUR;
using namespace std;
using namespace PBD;
using namespace Glib;
using namespace ArdourSurface;
using namespace ArdourCanvas;

Push2Menu::Push2Menu (Item* parent, vector<string> s)
	: Container (parent)
	, baseline (-1)
	, ncols (0)
	, nrows (0)
	, wrap (true)
	, first (0)
	, last (0)
	, _active (0)
{
	Pango::FontDescription fd ("Sans 10");

	if (baseline < 0) {
		Push2Canvas* p2c = dynamic_cast<Push2Canvas*> (canvas());
		Glib::RefPtr<Pango::Layout> throwaway = Pango::Layout::create (p2c->image_context());
		throwaway->set_font_description (fd);
		throwaway->set_text (X_("Hg")); /* ascender + descender) */
		int h, w;
		throwaway->get_pixel_size (w, h);
		baseline = h;
	}

	active_bg = new ArdourCanvas::Rectangle (this);

	for (vector<string>::iterator si = s.begin(); si != s.end(); ++si) {
		Text* t = new Text (this);
		t->set_font_description (fd);
		t->set (*si);
		displays.push_back (t);
	}

}

void
Push2Menu::set_layout (int c, int r)
{
	ncols = c;
	nrows = r;

	set_active (_active);
	rearrange (_active);
}

void
Push2Menu::rearrange (uint32_t initial_display)
{
	if (initial_display >= displays.size()) {
		return;
	}

	vector<Text*>::iterator i = displays.begin();

	/* move to first */

	for (uint32_t n = 0; n < initial_display; ++n) {
		(*i)->hide ();
		++i;
	}

	uint32_t index = initial_display;
	uint32_t col = 0;
	uint32_t row = 0;
	bool active_shown = false;

	while (i != displays.end()) {

		Coord x = col * Push2Canvas::inter_button_spacing();
		Coord y = 2 + (row * baseline);

		(*i)->set_position (Duple (x, y));

		if (index == _active) {
			active_bg->set (Rect (x - 1, y - 1,
			                      x - 1 + Push2Canvas::inter_button_spacing(), y - 1 + baseline));
			active_bg->show ();
			active_shown = true;
		}

		(*i)->show ();
		last = index;
		++i;
		++index;

		if (++row >= nrows) {
			row = 0;
			if (++col >= ncols) {
				/* no more to display */
				break;
			}
		}

	}

	while (i != displays.end()) {
		(*i)->hide ();
		++i;
	}

	if (!active_shown) {
		active_bg->hide ();
	}

	first = initial_display;

	Rearranged (); /* EMIT SIGNAL */
}

void
Push2Menu::scroll (Direction dir, bool page)
{
	switch (dir) {
	case DirectionUp:
		if (_active == 0) {
			if (wrap) {
				set_active (displays.size() - 1);
			}
		} else {
			set_active (_active - 1);
		}
		break;

	case DirectionDown:
		if (_active == displays.size() - 1) {
			if (wrap) {
				set_active (0);
			}
		} else {
			set_active (_active + 1);
		}
		break;

	case DirectionLeft:
		if (page) {
			set_active (max (0, (int) (first - (nrows * ncols))));
		} else {
			if (_active / nrows == 0) {
				/* in the first column, go to last column, same row */
				if (wrap) {
					set_active (displays.size() - 1 - active_row ());
				}
			} else {
				/* move to same row, previous column */
				set_active (_active - nrows);
			}
		}
		break;

	case DirectionRight:
		if (page) {
			set_active (min ((uint32_t) displays.size(), first + (nrows * ncols)));
		} else {
			if (_active / nrows == ncols) {
				/* in the last column, go to same row in first column */
				if (wrap) {
				set_active (active_row());
				}
			} else {
				/* move to same row, next column */
				set_active (_active + nrows);
			}
		}
		break;
	}
}

void
Push2Menu::render (Rect const& area, Cairo::RefPtr<Cairo::Context> context) const
{
	render_children (area, context);
}

void
Push2Menu::set_active (uint32_t index)
{
	if (!parent() || (index == _active)) {
		return;
	}

	if (index >= displays.size()) {
		active_bg->hide ();
		return;
	}

	/* set text color for old active item, and the new one */

	if (_active < displays.size()) {
		displays[_active]->set_color (text_color);
	}

	displays[index]->set_color (contrast_color);

	Duple p = displays[index]->position ();

	active_bg->set (Rect (p.x - 1, p.y - 1, p.x - 1 + Push2Canvas::inter_button_spacing(), p.y - 1 + baseline ));
	active_bg->show ();
	_active = index;

	if (_active < first) {

		/* we jumped before current visible range : try to put its column first
		 */

		rearrange (active_top());

	} else if (_active > last) {

		/* we jumped after current visible range : try putting its
		 * column last
		 */

		rearrange (active_top() - ((ncols - 1) * nrows));
	}

	ActiveChanged (); /* EMIT SIGNAL */
}

void
Push2Menu::set_text_color (Gtkmm2ext::Color c)
{
	text_color = c;

	for (vector<Text*>::iterator t = displays.begin(); t != displays.end(); ++t) {
		(*t)->set_color (c);
	}

}

void
Push2Menu::set_active_color (Gtkmm2ext::Color c)
{
	active_color = c;
	contrast_color = Gtkmm2ext::contrasting_text_color (active_color);
	if (active_bg) {
		active_bg->set_fill_color (c);
	}

	if (_active < displays.size()) {
		displays[_active]->set_color (contrast_color);
	}
}

void
Push2Menu::set_font_description (Pango::FontDescription fd)
{
	font_description = fd;

	for (vector<Text*>::iterator t = displays.begin(); t != displays.end(); ++t) {
		(*t)->set_font_description (fd);
	}
}
