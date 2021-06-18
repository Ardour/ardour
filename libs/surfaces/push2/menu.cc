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
	, _baseline (-1)
	, _ncols (0)
	, _nrows (0)
	, _wrap (true)
	, _first (0)
	, _last (0)
	, _active (0)
{
	Pango::FontDescription fd ("Sans 10");

	if (_baseline < 0) {
		Push2Canvas* p2c = dynamic_cast<Push2Canvas*> (canvas());
		Glib::RefPtr<Pango::Layout> throwaway = Pango::Layout::create (p2c->image_context());
		throwaway->set_font_description (fd);
		throwaway->set_text (X_("Hg")); /* ascender + descender) */
		int h, w;
		throwaway->get_pixel_size (w, h);
		_baseline = h;
	}

	_active_bg = new ArdourCanvas::Rectangle (this);

	for (vector<string>::iterator si = s.begin(); si != s.end(); ++si) {
		Text* t = new Text (this);
		t->set_font_description (fd);
		t->set (*si);
		_displays.push_back (t);
	}

}

void
Push2Menu::set_layout (int c, int r)
{
	_ncols = c;
	_nrows = r;

	set_active (_active);
	rearrange (_active);
}

void
Push2Menu::rearrange (uint32_t initial_display)
{
	if (initial_display >= _displays.size()) {
		return;
	}

	vector<Text*>::iterator i = _displays.begin();

	/* move to first */

	for (uint32_t n = 0; n < initial_display; ++n) {
		(*i)->hide ();
		++i;
	}

	uint32_t index = initial_display;
	uint32_t col = 0;
	uint32_t row = 0;
	bool active_shown = false;

	while (i != _displays.end()) {

		Coord x = col * Push2Canvas::inter_button_spacing();
		Coord y = 2 + (row * _baseline);

		(*i)->set_position (Duple (x, y));

		if (index == _active) {
			_active_bg->set (Rect (x - 1,
			                       y - 1,
			                       x - 1 + Push2Canvas::inter_button_spacing (),
			                       y - 1 + _baseline));

			_active_bg->show ();
			active_shown = true;
		}

		(*i)->show ();
		_last = index;
		++i;
		++index;

		if (++row >= _nrows) {
			row = 0;
			if (++col >= _ncols) {
				/* no more to display */
				break;
			}
		}

	}

	while (i != _displays.end()) {
		(*i)->hide ();
		++i;
	}

	if (!active_shown) {
		_active_bg->hide ();
	}

	_first = initial_display;

	Rearranged (); /* EMIT SIGNAL */
}

void
Push2Menu::scroll (Direction dir, bool page)
{
	switch (dir) {
	case DirectionUp:
		if (_active == 0) {
			if (_wrap) {
				set_active (_displays.size() - 1);
			}
		} else {
			set_active (_active - 1);
		}
		break;

	case DirectionDown:
		if (_active == _displays.size() - 1) {
			if (_wrap) {
				set_active (0);
			}
		} else {
			set_active (_active + 1);
		}
		break;

	case DirectionLeft:
		if (page) {
			set_active (max (0, (int) (_first - (_nrows * _ncols))));
		} else {
			if (_active / _nrows == 0) {
				/* in the first column, go to last column, same row */
				if (_wrap) {
					set_active (_displays.size() - 1 - active_row ());
				}
			} else {
				/* move to same row, previous column */
				set_active (_active - _nrows);
			}
		}
		break;

	case DirectionRight:
		if (page) {
			set_active (min ((uint32_t) _displays.size(), _first + (_nrows * _ncols)));
		} else {
			if (_active / _nrows == _ncols) {
				/* in the last column, go to same row in first column */
				if (_wrap) {
					set_active (active_row());
				}
			} else {
				/* move to same row, next column */
				set_active (_active + _nrows);
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

	if (index >= _displays.size()) {
		_active_bg->hide ();
		return;
	}

	/* set text color for old active item, and the new one */

	if (_active < _displays.size()) {
		_displays[_active]->set_color (_text_color);
	}

	_displays[index]->set_color (_contrast_color);

	Duple p = _displays[index]->position ();

	_active_bg->set (Rect (p.x - 1, p.y - 1, p.x - 1 + Push2Canvas::inter_button_spacing(), p.y - 1 + _baseline ));
	_active_bg->show ();
	_active = index;

	if (_active < _first) {

		/* we jumped before current visible range : try to put its column first
		 */

		rearrange (active_top());

	} else if (_active > _last) {

		/* we jumped after current visible range : try putting its
		 * column last
		 */

		rearrange (active_top() - ((_ncols - 1) * _nrows));
	}

	ActiveChanged (); /* EMIT SIGNAL */
}

void
Push2Menu::set_text_color (Gtkmm2ext::Color c)
{
	_text_color = c;

	for (vector<Text*>::iterator t = _displays.begin(); t != _displays.end(); ++t) {
		(*t)->set_color (c);
	}

}

void
Push2Menu::set_active_color (Gtkmm2ext::Color c)
{
	_active_color = c;
	_contrast_color = Gtkmm2ext::contrasting_text_color (_active_color);
	if (_active_bg) {
		_active_bg->set_fill_color (c);
	}

	if (_active < _displays.size()) {
		_displays[_active]->set_color (_contrast_color);
	}
}

void
Push2Menu::set_font_description (Pango::FontDescription fd)
{
	_font_description = fd;

	for (vector<Text*>::iterator t = _displays.begin(); t != _displays.end(); ++t) {
		(*t)->set_font_description (fd);
	}
}
