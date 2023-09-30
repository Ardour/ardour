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

#include <algorithm>

#include "ardour/session.h"

#include "automation_line.h"
#include "editor.h"
#include "mergeable_line.h"
#include "route_time_axis.h"
#include "selectable.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace ARDOUR;

void
MergeableLine::merge_drawn_line (Editor& e, Session& s, Evoral::ControlList::OrderedPoints& points, bool thin)
{
	if (points.empty()) {
		return;
	}

	if (!_line) {
		return;
	}

	std::shared_ptr<AutomationList> list = _line->the_list ();

	if (list->in_write_pass()) {
		/* do not allow the GUI to add automation events during an
		   automation write pass.
		*/
		return;
	}

	XMLNode& before = list->get_state();
	std::list<Selectable*> results;

	/* If necessary convert all point times. This is necessary
	   for region-based automation data, because the time values for the
	   points drawn are in absolute time, but the ControlList expects data
	   in source-reference time.
	*/

	if (time_filter) {
		for (auto & p : points) {
			p.when = time_filter (p.when);
		}
	}

	Temporal::timepos_t earliest = points.front().when;
	Temporal::timepos_t latest = points.back().when;

	if (earliest > latest) {
		std::swap (earliest, latest);
	}

	/* Convert each point's "value" from geometric coordinate space to
	 * value space for the control
	 */

	for (auto & dp : points) {
		/* compute vertical fractional position */
		dp.value = 1.0 - (dp.value / _line->height());
		/* map using line */
		_line->view_to_model_coord_y (dp.value);
	}

	list->freeze ();
	list->editor_add_ordered (points, false);
	if (thin) {
		list->thin (Config->get_automation_thinning_factor());
	}
	list->thaw ();

	if (_control && _control->automation_state () == ARDOUR::Off) {
		automation_state_callback (ARDOUR::Play);
	}

	if (UIConfiguration::instance().get_automation_edit_cancels_auto_hide () && _control == s.recently_touched_controllable ()) {
		control_touched_callback ();
	}

	XMLNode& after = list->get_state();
	e.begin_reversible_command (_("draw automation"));
	s.add_command (new MementoCommand<ARDOUR::AutomationList> (*list.get (), &before, &after));

	_line->get_selectables (earliest, latest, 0.0, 1.0, results);
	e.get_selection ().set (results);

	e.commit_reversible_command ();
	s.set_dirty ();
}
