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
	, _key (nullptr)
{
}

void
ScaleProvider::set_key (MusicalKey const & k)
{
	delete _key;
	_key = new MusicalKey (k);
}

MusicalKey const *
ScaleProvider::key () const
{
	if (_key) {
		return _key;
	}

	if (_parent) {
		return _parent->key();
	}

	return nullptr;
}
