/*
 * Copyright (C) 2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2017 Paul Davis <paul@linuxaudiosystems.com>
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

#include "pbd/stacktrace.h"

#include "canvas/root_group.h"
#include "canvas/canvas.h"
#include "canvas/constraint_packer.h"

using namespace std;
using namespace ArdourCanvas;

Root::Root (Canvas* canvas)
	: Container (canvas)
{
#ifdef CANVAS_DEBUG
	name = "ROOT";
#endif
}

void
Root::preferred_size (Duple& min, Duple& natural) const
{
	if (_items.size() == 1) {
		cerr << "Call prefsize on " << _items.front()->whoami() << endl;
		_items.front()->preferred_size (min, natural);
		return;
	}

	cerr << "use regular prefsize for root\n";

	Item::preferred_size (min, natural);
}

void
Root::size_allocate (Rect const & r)
{
	bool have_constraint_container = false;

	for (list<Item*>::const_iterator i = _items.begin(); i != _items.end(); ++i) {
		if (dynamic_cast<ConstraintPacker*> (*i)) {
			(*i)->size_allocate (r);
			have_constraint_container = true;
		}
	}

	if (have_constraint_container) {
		_bounding_box_dirty = true;
	}
}
