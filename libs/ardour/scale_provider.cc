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

#include "ardour/scale_provider.h"

using namespace ARDOUR;

ScaleProvider::ScaleProvider (ScaleProvider* parent)
	: _parent (parent)
	, _scale (nullptr)
{
}

void
ScaleProvider::set_scale (MusicalScale const & sc)
{
	delete _scale;
	_scale = new MusicalScale (sc);
}

MusicalScale const *
ScaleProvider::scale () const
{
	if (_scale) {
		return _scale;
	}

	if (_parent) {
		return _parent->scale();
	}

	return nullptr;
}
