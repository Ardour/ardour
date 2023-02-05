/*
 * Copyright (C) 2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2017 Paul Davis <paul@linuxaudiosystems.com>
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

#include "canvas/container.h"

using namespace ArdourCanvas;

Container::Container (Canvas* canvas)
	: Item (canvas)
	, _render_with_alpha (-1)
{
}

Container::Container (Item* parent)
	: Item (parent)
	, _render_with_alpha (-1)
{
}


Container::Container (Item* parent, Duple const & p)
	: Item (parent, p)
	, _render_with_alpha (-1)
{
}

void
Container::prepare_for_render (Rect const & area) const
{
	Item::prepare_for_render_children (area);
}

void
Container::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	if (_render_with_alpha == 0) {
		return;
	} else if (_render_with_alpha > 0) {
		context->push_group ();
	}

	Item::render_children (area, context);

	if (_render_with_alpha >= 1.0) {
		context->pop_group_to_source ();
		context->paint ();
	} else if (_render_with_alpha > 0) {
		context->pop_group_to_source ();
		context->paint_with_alpha (_render_with_alpha);
	}
}

void
Container::compute_bounding_box () const
{
	_bounding_box = Rect ();
	/* nothing to do here; Item::bounding_box() will add all children for us */
	set_bbox_clean ();
}

void
Container::set_render_with_alpha (double alpha)
{
	if (alpha >= 1.0 && NULL == g_getenv("ARDOUR_OPAQUE_RENDER_GROUP")) {
		alpha = -1; // disable render group when fully opaque
	}
	if (_render_with_alpha == alpha) {
		return;
	}
	_render_with_alpha = alpha;
	redraw ();
}
