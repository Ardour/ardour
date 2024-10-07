/*
 * Copyright (C) 2006-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2008-2009 David Robillard <d@drobilla.net>
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
#include <list>

#include <sigc++/signal.h>

#include "temporal/timeline.h"

class Selection;

class Selectable : public virtual sigc::trackable
{
public:
	Selectable() {
		_selected = false;
	}

	virtual ~Selectable() {}

	virtual void set_selected (bool yn) {
		if (yn != _selected) {
			_selected = yn;
		}
	}

	virtual bool selected() const {
		return _selected;
	}

protected:
	bool _selected;
};

class SelectableOwner
{
  public:
	SelectableOwner() {}
	virtual ~SelectableOwner() {}

	void get_selectables (Temporal::timepos_t const & start, Temporal::timepos_t  const & end, double x, double y, std::list<Selectable*>& sl, bool within = false) {
		_get_selectables (start, end, x, y, sl, within);
	}

	virtual void _get_selectables (Temporal::timepos_t const &, Temporal::timepos_t  const &, double, double, std::list<Selectable*>&, bool within) = 0;
	virtual void get_inverted_selectables (Selection&, std::list<Selectable *>& results) = 0;
};
