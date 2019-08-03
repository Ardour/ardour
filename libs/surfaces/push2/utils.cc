/*
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

#include "gtkmm2ext/rgb_macros.h"

#include "utils.h"

void
set_source_rgba (Cairo::RefPtr<Cairo::Context> context, uint32_t col, bool with_alpha)
{
	int r, g, b, a;

	UINT_TO_RGBA (col, &r, &g, &b, &a);

	if (with_alpha) {
		context->set_source_rgba (r/255.0, g/255.0, b/255.0, a/255.0);
	} else {
		context->set_source_rgb (r/255.0, g/255.0, b/255.0);
	}
}

void
set_source_rgb (Cairo::RefPtr<Cairo::Context> context, uint32_t color)
{
	set_source_rgba (context, color, false);
}
