/*
	Copyright (C) 2006,2007 John Anderson
	Copyright (C) 2012 Paul Davis

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
*/

#include "fader.h"
#include "surface.h"
#include "control_group.h"

using namespace Mackie;

Control*
Fader::factory (Surface& surface, int id, const char* name, Group& group)
{
	Fader* f = new Fader (id, name, group);

	surface.faders[id] = f;
	surface.controls.push_back (f);
	group.add (*f);
	return f;
}

