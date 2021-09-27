/*
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
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

#include "gtkmm2ext/colors.h"

#include "canvas/text.h"
#include "canvas/types.h"
#include "canvas/rectangle.h"

#include "maschine2.h"
#include "m2controls.h"

#include "canvas.h"
#include "ui_menu.h"

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

Maschine2Menu::Maschine2Menu (PBD::EventLoop* el, Item* parent, const std::vector<std::string>& s, double width)
	: Container (parent)
	, _ctrl (0)
	, _eventloop (el)
	, _baseline (-1)
	, _height (-1)
	, _width (width)
	, _active (0)
	, _wrap (false)
	, _first (0)
	, _last (0)
	, _rotary (0)
{
	Pango::FontDescription fd ("Sans 10px");

	Maschine2Canvas* m2c = dynamic_cast<Maschine2Canvas*> (canvas());
	Glib::RefPtr<Pango::Layout> throwaway = Pango::Layout::create (m2c->image_context());
	throwaway->set_font_description (fd);
	throwaway->set_text (X_("Hg")); /* ascender + descender) */
	int h, w;
	throwaway->get_pixel_size (w, h);
	_baseline = ceil(h);
	_height = m2c->height();

	_active_bg = new ArdourCanvas::Rectangle (this);
	_active_bg->set_fill_color (0xffffffff);

	for (vector<string>::const_iterator i = s.begin(); i != s.end(); ++i) {
		Text* t = new Text (this);
		t->set_font_description (fd);
		t->set_color (0xffffffff);
		t->set (*i);
		_displays.push_back (t);
	}
	rearrange (0);
}

Maschine2Menu::~Maschine2Menu ()
{
}

void
Maschine2Menu::rearrange (uint32_t initial_display)
{
	vector<Text*>::iterator i = _displays.begin();

	Duple origin = item_to_window (Duple (0, 0));

	for (uint32_t n = 0; n < initial_display; ++n) {
		(*i)->hide ();
		++i;
	}

	uint32_t index = initial_display;
	uint32_t row = 0;
	bool active_shown = false;

	_first = _last = index;
	while (i != _displays.end()) {
		Coord y = row * _baseline;
		if (y + _baseline + origin.y > _height) {
			break;
		}
		(*i)->set_position (Duple (2, y));

		if (index == _active) {
			(*i)->set_color (0x000000ff);
			active_shown = true;
			_active_bg->set (Rect (0, y - 1, 64, y - 1 + _baseline));
			_active_bg->show ();
		} else {
			(*i)->set_color (0xffffffff);
		}
		_last = index;
		(*i)->show ();
		++i;
		++index;
		++row;
	}

	while (i != _displays.end()) {
		(*i)->hide ();
		++i;
	}

	if (!active_shown) {
		_active_bg->hide ();
	}
}

void
Maschine2Menu::render (Rect const& area, Cairo::RefPtr<Cairo::Context> context) const
{
	context->save ();
	Duple origin = item_to_window (Duple (0, 0));
	context->rectangle (origin.x, origin.y, _width, _height);
	context->clip ();
	render_children (area, context);
	context->restore ();
}

void
Maschine2Menu::set_active (uint32_t a)
{
	if (a == _active || a > items ()) {
		return;
	}

	_active = a;

	if (_active < _first) {
		rearrange (_active);
	}
	else if (_active > _last) {
		rearrange (_active - _last);
	} else {
		rearrange (_first);
	}
	redraw ();
}

void
Maschine2Menu::set_wrap (bool b)
{
	if (b == _wrap) {
		return;
	}
	_wrap = b;
}

void
Maschine2Menu::set_control (M2EncoderInterface* ctrl)
{
	encoder_connection.disconnect ();
	_ctrl = ctrl;
	if (!ctrl) {
		return;
	}
	ctrl->changed.connect_same_thread (encoder_connection, boost::bind (&Maschine2Menu::encoder_changed, this, _1));
}

void
Maschine2Menu::encoder_changed (int delta)
{
	assert (_ctrl);
	if (items() == 0) {
		return;
	}
	double d = delta * 8. / _ctrl->range ();
	d = fmodf (d, items());
	if (_wrap) {
		_rotary = fmodf (items () + _rotary + d, items());
	} else {
		_rotary += + d;
		if (_rotary < 0) { _rotary = 0; }
		if (_rotary >= items ()) {  _rotary = items () - 1; }
	}

	uint32_t a = floor (_rotary);

	if (a == _active) {
		return;
	}
	set_active (a);
	ActiveChanged (); /* EMIT SIGNAL */
}
