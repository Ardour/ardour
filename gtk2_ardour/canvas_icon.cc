/*
 * Copyright (C) 2026 Paul Davis <paul@linuxaudiosystems.com>
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

#include "canvas_icon.h"

using namespace ArdourCanvas;
using namespace ArdourWidgets;

Icon::Icon (Canvas* c, ArdourIcon::Icon ic)
	: Rectangle (c)
	, _icon (ic)
{
}


Icon::Icon (Item* parent, ArdourIcon::Icon ic)
	: Rectangle (parent)
	, _icon (ic)
{
}

void
Icon::render (Rect const & area, Cairo::RefPtr<Cairo::Context> ctxt) const
{
	/* no rectangle rendering here */
	Rect self (item_to_window (_rect));
	ctxt->save ();
	ctxt->translate (self.x0, self.y0);
	ArdourIcon::render (ctxt->cobj(), _icon, self.width(), self.height(), Gtkmm2ext::Off, _outline_color);
	ctxt->restore ();
}

void
Icon::set_icon (ArdourIcon::Icon ic)
{
	_icon = ic;
	redraw ();
}
