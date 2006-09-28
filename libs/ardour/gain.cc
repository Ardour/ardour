/*
    Copyright (C) 2000 Paul Davis 

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    $Id$
*/

#include <ardour/gain.h>

using namespace ARDOUR;

Gain::Gain ()
	: Curve (0.0, 2.0, 1.0f)   /* XXX yuck; clamps gain to -inf .. +6db */
{
}

Gain::Gain (const Gain& other)
	: Curve (other)
{
}

Gain&
Gain::operator= (const Gain& other)
{
	if (this != &other) {
		Curve::operator= (other);
	}
	return *this;
}

void
Gain::fill_linear_volume_fade_in (Gain& gain, nframes_t frames)
{
}

void
Gain::fill_linear_volume_fade_out (Gain& gain, nframes_t frames)
{
}

void
Gain::fill_linear_fade_in (Gain& gain, nframes_t frames)
{
}

void
Gain::fill_linear_fade_out (Gain& gain, nframes_t frames)
{
}
