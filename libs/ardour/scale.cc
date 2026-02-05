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

#include "ardour/scale.h"

using namespace ARDOUR;

MusicalScale::MusicalScale (std::string const & name, MusicalScaleType type, MusicalScaleTemperament t, std::vector<float> const & elements)
	: _name (name)
	, _type (type)
	, _temperament (t)
	, _elements (elements)
{
}

MusicalScale::MusicalScale (MusicalScale const & other)
	: _name (other._name)
	, _type (other._type)
	, _elements (other._elements)
{
}

void
MusicalScale::set_name (std::string const & str)
{
	_name = str;
	NameChanged(); /* EMIT SIGNAL */
}

std::vector<float>
MusicalScale::pitches_from_root (float root, int steps) const
{
	return std::vector<float> ();
}


/*---------*/

MusicalKey::MusicalKey (float root, MusicalScale const & sc)
	: MusicalScale (sc)
	, _root (root)
{
}
