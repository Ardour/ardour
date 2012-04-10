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

#include "button.h"
#include "surface.h"
#include "control_group.h"

using namespace Mackie;

Control*
Button::factory (Surface& surface, int id, const char* name, Group& group)
{
	Button* b = new Button (id, name, group);
	surface.buttons[id] = b;
	surface.controls.push_back (b);
	group.add (*b);
	return b;
}
