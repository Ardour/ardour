/*
 * Copyright (C) 2025 Paul Davis <paul@linuxaudiosystems.com>
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

#pragma once

#include "canvas/rectangle.h"

class StartBoundaryRect : public ArdourCanvas::Rectangle
{
  public:
	StartBoundaryRect (ArdourCanvas::Item* p) : ArdourCanvas::Rectangle (p) {}

	void render (ArdourCanvas::Rect const & area, Cairo::RefPtr<Cairo::Context> context) const;
	bool covers (ArdourCanvas::Duple const& point) const;
	void compute_bounding_box () const;
};

class EndBoundaryRect : public ArdourCanvas::Rectangle
{
  public:
	EndBoundaryRect (ArdourCanvas::Item* p) : ArdourCanvas::Rectangle (p) {}

	void render (ArdourCanvas::Rect const & area, Cairo::RefPtr<Cairo::Context> context) const;
	bool covers (ArdourCanvas::Duple const& point) const;
	void compute_bounding_box () const;
};

