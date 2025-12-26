/*
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2018 Ben Loftis <ben@harrisonconsoles.com>
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

#include <algorithm>

#include "widgets/ardour_spacer.h"

using namespace ArdourWidgets;

ArdourScalingSpacer::ArdourScalingSpacer (int natural_width, int natural_height)
	: CairoWidget ()
	, _natural_width (natural_width)
	, _natural_height (natural_height)
{
}


ArdourVSpacer::ArdourVSpacer (float r)
	: CairoWidget ()
	, _ratio (std::min (1.f, r))
{
}

ArdourHSpacer::ArdourHSpacer (float r)
	: CairoWidget ()
	, _ratio (std::min (1.f, r))
{
}

ArdourDropShadow::ArdourDropShadow (ShadowMode m, float a)
	: CairoWidget ()
	, alpha (a)
	, mode (m)
{
}
