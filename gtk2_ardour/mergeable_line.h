/*
 * Copyright (C) 2023 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __gtk2_ardour_mergeable_line__
#define __gtk2_ardour_mergeable_line__

#include <functional>
#include <memory>

#include "evoral/ControlList.h"

#include "ardour/types.h"

class AutomationLine;
class RouteTimeAxisView;
class EditingContext;

namespace ARDOUR {
	class Session;
class AutomationControl;
}

class MergeableLine
{
   public:
	MergeableLine (std::shared_ptr<AutomationLine> l, std::shared_ptr<ARDOUR::AutomationControl> c,
	               std::function<Temporal::timepos_t(Temporal::timepos_t const &)> tf,
	               std::function<void(ARDOUR::AutoState)> asc,
	               std::function<void()> ctc)
		: _line (l)
		, _control (c)
		, time_filter (tf)
		, automation_state_callback (asc)
		, control_touched_callback (ctc) {}

	virtual ~MergeableLine() {}

	void merge_drawn_line (EditingContext& e, ARDOUR::Session& s, Evoral::ControlList::OrderedPoints& points, bool thin);

  private:
	std::shared_ptr<AutomationLine> _line;
	std::shared_ptr<ARDOUR::AutomationControl> _control;
	std::function<Temporal::timepos_t(Temporal::timepos_t const &)> time_filter;
	std::function<void(ARDOUR::AutoState)> automation_state_callback;
	std::function<void()> control_touched_callback;
};

#endif /* __gtk2_ardour_mergeable_line__ */
