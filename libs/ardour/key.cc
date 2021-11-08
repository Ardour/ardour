/*
 * Copyright (C) 2021 Paul Davis <paul@linuxaudiosystems.com>
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

#include "ardour/key.h"

MusicalKey::~MusicalKey ()
{
}

void
MusicalKey::set_root (int r)
{
	/* force root into lowest octave. Yes, 12 tone for now */
	_root = (r % 12);
}

bool
MusicalKey::in_key (int note) const
{
	/* currently 12 tone based */

	note = note % 12;

	/* we should speed this us. Probably a bitset */

	if (note == _root) {
		return true;
	}

	for (std::vector<float>::const_iterator i = steps.begin(); i != steps.end(); ++i) {
		int ii = (int) ((*i) * 2.0);

		if (note == _root + ii) {
			return true;
		}
	}

	return false;
}
