/*
 * Copyright (C) 2017-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2017-2018 Robin Gareus <robin@gareus.org>
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

#include <vector>

#include "pbd/compose.h"
#include "pbd/signals.h"

#include "ardour/automation_control.h"
#include "ardour/debug.h"
#include "ardour/route.h"
#include "ardour/route_group.h"
#include "ardour/selection.h"
#include "ardour/session.h"
#include "ardour/stripable.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;

bool
CoreSelection::do_select (std::shared_ptr<Stripable> s, std::shared_ptr<AutomationControl> c, SelectionOperation op, bool with_group, bool routes_only, RouteGroup* not_allowed_in_group)
{
	std::shared_ptr<Route> r;
	StripableList sl;
	bool changed = false;
	std::vector<std::shared_ptr<Stripable> > removed;

	/* no selection of hidden stripables (though they can be selected and
	 * then hidden
	 */

	if (s->is_hidden()) {
		return false;
	}

	/* monitor is never selectable */

	if (s->is_monitor() || s->is_surround_master ()) {
		return false;
	}

	if (!(r = std::dynamic_pointer_cast<Route> (s)) && routes_only) {
		return false;
	}

	if (r) {

		/* no selection of inactive routes, though they can be selected
		 * and made inactive.
		 */

		if (!r->active()) {
			return false;
		}

		if (!c && with_group) {


			if (!not_allowed_in_group || !r->route_group() || r->route_group() != not_allowed_in_group) {

				if (r->route_group() && r->route_group()->is_select() && r->route_group()->is_active()) {
					for (auto & ri : *(r->route_group()->route_list())) {
						if (ri != r) {
							sl.push_back (ri);
						}
					}
				}
			}
		}
	}

	/* it is important to make the "primary" stripable being selected the last in this
	 * list
	 */

	sl.push_back (s);

	switch (op) {
	case SelectionAdd:
		changed = add (sl, c);
		break;
	case SelectionToggle:
		changed = toggle (sl, c);
		break;
	case SelectionSet:
		changed = set (sl, c, removed);
		break;
	case SelectionRemove:
		changed = remove (sl, c);
		break;
	default:
		return false;
	}

	if (changed || !removed.empty()) {

		send_selection_change ();

		/* send per-object signal to notify interested parties
		   the selection status has changed
		*/

		PropertyChange pc (Properties::selected);

		for (auto & s : removed) {
			s->presentation_info().PropertyChanged (pc);
		}

		for (auto & s: sl) {
			s->presentation_info().PropertyChanged (pc);
		}

	}

	return changed;
}

bool
CoreSelection::select_stripable_and_maybe_group (std::shared_ptr<Stripable> s, SelectionOperation op, bool with_group, bool routes_only, RouteGroup* not_allowed_in_group)
{
	return do_select (s, nullptr, op, with_group, routes_only, not_allowed_in_group);
}

void
CoreSelection::select_stripable_with_control (std::shared_ptr<Stripable> s, std::shared_ptr<AutomationControl> c, SelectionOperation op)
{
	do_select (s, c, op, c ? false : true, false, nullptr);
}

void
CoreSelection::send_selection_change ()
{
	PropertyChange pc;
	pc.add (Properties::selected);
	PresentationInfo::send_static_change (pc);
}

CoreSelection::CoreSelection (Session& s)
	: session (s)
{
	_selection_order.store (0);
}

CoreSelection::~CoreSelection ()
{
}

template<typename IterTypeCore>
void
CoreSelection::select_adjacent_stripable (bool mixer_order, bool routes_only,
                                          IterTypeCore (StripableList::*begin_method)(),
                                          IterTypeCore (StripableList::*end_method)())
{
	if (_stripables.empty()) {

		/* Pick first acceptable */

		StripableList stripables;
		session.get_stripables (stripables);
		stripables.sort (ARDOUR::Stripable::Sorter (mixer_order));

		for (StripableList::iterator s = stripables.begin(); s != stripables.end(); ++s) {
			if (select_stripable_and_maybe_group (*s, SelectionSet, true, routes_only, nullptr)) {
				break;
			}
		}

		return;
	}

	/* fetch the current selection so that we can get the most recently selected */
	StripableAutomationControls selected;
	get_stripables (selected);
	std::shared_ptr<Stripable> last_selected =
	  selected.empty () ? std::shared_ptr<Stripable> ()
	                    : selected.back ().stripable;

	/* Get all stripables and sort into the appropriate ordering */
	StripableList stripables;
	session.get_stripables (stripables);
	stripables.sort (ARDOUR::Stripable::Sorter (mixer_order));


	/* Check for a possible selection-affecting route group */

	RouteGroup* group = 0;
	std::shared_ptr<Route> r = std::dynamic_pointer_cast<Route> (last_selected);

	if (r && r->route_group() && r->route_group()->is_select() && r->route_group()->is_active()) {
		group = r->route_group();
	}

	bool select_me = false;

	for (IterTypeCore i = (stripables.*begin_method)(); i != (stripables.*end_method)(); ++i) {

		if (select_me) {

			if (!this->selected (*i)) { /* not currently selected */
				if (select_stripable_and_maybe_group (*i, SelectionSet, true, routes_only, group)) {
					return;
				}
			}
		}

		if ((*i) == last_selected) {
			select_me = true;
		}
	}

	/* no next/previous, wrap around ... find first usable stripable from
	 * the appropriate end.
	*/

	for (IterTypeCore s = (stripables.*begin_method)(); s != (stripables.*end_method)(); ++s) {

		r = std::dynamic_pointer_cast<Route> (*s);

		/* monitor is never selectable anywhere. for now, anyway */

		if (!routes_only || r) {
			if (select_stripable_and_maybe_group (*s, SelectionSet, true, routes_only, 0)) {
				return;
			}
		}
	}
}

void
CoreSelection::select_next_stripable (bool mixer_order, bool routes_only)
{
	select_adjacent_stripable<StripableList::iterator> (mixer_order, routes_only, &StripableList::begin, &StripableList::end);
}

void
CoreSelection::select_prev_stripable (bool mixer_order, bool routes_only)
{
	select_adjacent_stripable<StripableList::reverse_iterator> (mixer_order, routes_only, &StripableList::rbegin, &StripableList::rend);
}


bool
CoreSelection::toggle (StripableList& sl, std::shared_ptr<AutomationControl> c)
{
	assert (sl.size() == 1 || !c);
	bool changed = false;
	StripableList sl2;

	for (auto & s : sl) {
		DEBUG_TRACE (DEBUG::Selection, string_compose ("toggle: s %1 selected %2 c %3 selected %4\n",
		                                               s, selected (s), c, selected (c)));

		sl2.clear ();
		sl2.push_back (s);

		if ((c && selected (c)) || selected (s)) {
			if (remove (sl2, c)) {
				changed = true;
			}
		} else {
			if (add (sl2, c)) {
				changed = true;
			}
		}
	}

	return changed;
}

bool
CoreSelection::set (StripableList& sl, std::shared_ptr<AutomationControl> c, std::vector<std::shared_ptr<Stripable> > & removed)
{
	assert (sl.size() == 1 || !c);

	bool changed = false;

	{
		Glib::Threads::RWLock::WriterLock lm (_lock);

		removed.reserve (_stripables.size());

		for (SelectedStripables::const_iterator x = _stripables.begin(); x != _stripables.end(); ++x) {
			std::shared_ptr<Stripable> sp = session.stripable_by_id ((*x).stripable);
			if (sp) {
				removed.push_back (sp);
			}
		}

		_stripables.clear ();

		for (StripableList::iterator s = sl.begin(); s != sl.end(); ++s) {

			SelectedStripable ss (*s, c, _selection_order.fetch_add (1));

			if (_stripables.insert (ss).second) {
				DEBUG_TRACE (DEBUG::Selection, string_compose ("set:added %1 to s/c selection\n", (*s)->name()));
				changed = true;
			} else {
				DEBUG_TRACE (DEBUG::Selection, string_compose ("%1 already in s/c selection\n", (*s)->name()));
			}
		}

		if (!sl.empty()) {
			_first_selected_stripable = sl.back ();
		} else {
			_first_selected_stripable.reset ();
		}
	}

	return changed;
}

bool
CoreSelection::add (StripableList& sl, std::shared_ptr<AutomationControl> c)
{
	assert (sl.size() == 1 || !c);

	bool changed = false;

	{
		Glib::Threads::RWLock::WriterLock lm (_lock);

		for (auto & s : sl) {
			SelectedStripable ss (s, c, _selection_order.fetch_add (1));

			if (_stripables.insert (ss).second) {
				DEBUG_TRACE (DEBUG::Selection, string_compose ("added %1/%2 to s/c selection\n", s->name(), c));
				changed  = true;
			} else {
				DEBUG_TRACE (DEBUG::Selection, string_compose ("%1/%2 already in s/c selection\n", s->name(), c));
			}
		}

		if (!sl.empty()) {
			_first_selected_stripable = sl.back();
		} else {
			_first_selected_stripable.reset ();
		}
	}

	return changed;
}

bool
CoreSelection::remove (StripableList & sl, std::shared_ptr<AutomationControl> c)
{
	assert (sl.size() == 1 || !c);
	bool changed = false;

	{
		Glib::Threads::RWLock::WriterLock lm (_lock);

		for (auto & s : sl) {
			SelectedStripable ss (s, c, 0);

			SelectedStripables::iterator i = _stripables.find (ss);

			if (i != _stripables.end()) {
				_stripables.erase (i);
				DEBUG_TRACE (DEBUG::Selection, string_compose ("removed %1/%2 from s/c selection\n", s, c));
				changed = true;
			}

			if (s == _first_selected_stripable.lock ()) {
				_first_selected_stripable.reset ();
			}
		}
	}

	return changed;
}

void
CoreSelection::clear_stripables ()
{
	bool send = false;
	std::vector<std::shared_ptr<Stripable> > s;

	DEBUG_TRACE (DEBUG::Selection, "clearing s/c selection\n");
	{
		Glib::Threads::RWLock::WriterLock lm (_lock);

		if (!_stripables.empty()) {

			s.reserve (_stripables.size());

			for (SelectedStripables::const_iterator x = _stripables.begin(); x != _stripables.end(); ++x) {
				std::shared_ptr<Stripable> sp = session.stripable_by_id ((*x).stripable);
				if (sp) {
					s.push_back (sp);
				}
			}

			_stripables.clear ();

			send = true;
			DEBUG_TRACE (DEBUG::Selection, "cleared s/c selection\n");
		}

		_first_selected_stripable.reset ();
	}

	if (send) {
		send_selection_change ();

		PropertyChange pc (Properties::selected);

		for (std::vector<std::shared_ptr<Stripable> >::iterator ss = s.begin(); ss != s.end(); ++ss) {
			(*ss)->presentation_info().PropertyChanged (pc);
		}

	}
}

std::shared_ptr<Stripable>
CoreSelection::first_selected_stripable () const
{
	Glib::Threads::RWLock::ReaderLock lm (_lock);
  return _first_selected_stripable.lock();
}

bool
CoreSelection::selected (std::shared_ptr<const Stripable> s) const
{
	if (!s) {
		return false;
	}

	Glib::Threads::RWLock::ReaderLock lm (_lock);

	for (SelectedStripables::const_iterator x = _stripables.begin(); x != _stripables.end(); ++x) {

		if (!((*x).controllable == 0)) {
			/* selected automation control */
			continue;
		}

		/* stripable itself selected, not just a control belonging to
		 * it
		 */

		if ((*x).stripable == s->id()) {
			return true;
		}
	}

	return false;
}

bool
CoreSelection::selected (std::shared_ptr<const AutomationControl> c) const
{
	if (!c) {
		return false;
	}

	Glib::Threads::RWLock::ReaderLock lm (_lock);

	for (SelectedStripables::const_iterator x = _stripables.begin(); x != _stripables.end(); ++x) {
		if ((*x).controllable == c->id()) {
			return true;
		}
	}

	return false;
}

CoreSelection::SelectedStripable::SelectedStripable (std::shared_ptr<Stripable> s, std::shared_ptr<AutomationControl> c, int o)
	: stripable (s ? s->id() : PBD::ID (0))
	, controllable (c ? c->id() : PBD::ID (0))
	, order (o)
{
}

struct StripableControllerSort {
	bool operator() (CoreSelection::StripableAutomationControl const &a, CoreSelection::StripableAutomationControl const & b) const {
		return a.order < b.order;
	}
};

void
CoreSelection::get_stripables (StripableAutomationControls& sc) const
{
	Glib::Threads::RWLock::ReaderLock lm (_lock);

	for (SelectedStripables::const_iterator x = _stripables.begin(); x != _stripables.end(); ++x) {

		std::shared_ptr<Stripable> s = session.stripable_by_id ((*x).stripable);
		std::shared_ptr<AutomationControl> c;

		if (!s) {
			/* some global automation control, not owned by a Stripable */
			c = session.automation_control_by_id ((*x).controllable);
		} else {
			/* automation control owned by a Stripable or one of its children */
			c = s->automation_control_recurse ((*x).controllable);
		}

		if (s || c) {
			sc.push_back (StripableAutomationControl (s, c, (*x).order));
		}
	}

	StripableControllerSort cmp;
	sort (sc.begin(), sc.end(), cmp);
}

void
CoreSelection::remove_control_by_id (PBD::ID const & id)
{
	Glib::Threads::RWLock::WriterLock lm (_lock);

	for (SelectedStripables::iterator x = _stripables.begin(); x != _stripables.end(); ++x) {
		if ((*x).controllable == id) {
			_stripables.erase (x);
			return;
		}
	}
}

void
CoreSelection::remove_stripable_by_id (PBD::ID const & id)
{
	Glib::Threads::RWLock::WriterLock lm (_lock);

	for (SelectedStripables::iterator x = _stripables.begin(); x != _stripables.end(); ) {
		if ((*x).stripable == id) {
			if (_first_selected_stripable.lock ()) {
				if (session.stripable_by_id (id) == _first_selected_stripable.lock ()) {
					_first_selected_stripable.reset ();
				}
			}

			_stripables.erase (x++);
			/* keep going because there may be more than 1 pair of
			   stripable/automation-control in the selection.
			*/
		} else {
			++x;
		}
	}
}

XMLNode&
CoreSelection::get_state () const
{
	XMLNode* node = new XMLNode (X_("Selection"));

	Glib::Threads::RWLock::WriterLock lm (_lock);

	for (SelectedStripables::const_iterator x = _stripables.begin(); x != _stripables.end(); ++x) {
		XMLNode* child = new XMLNode (X_("StripableAutomationControl"));
		child->set_property (X_("stripable"), (*x).stripable.to_s());
		child->set_property (X_("control"), (*x).controllable.to_s());
		child->set_property (X_("order"), (*x).order);

		node->add_child_nocopy (*child);
	}

	return *node;
}
int
CoreSelection::set_state (const XMLNode& node, int /* version */)
{
	XMLNodeList children (node.children());
	Glib::Threads::RWLock::WriterLock lm (_lock);

	_stripables.clear ();

	for (XMLNodeConstIterator i = children.begin(); i != children.end(); ++i) {
		if ((*i)->name() != X_("StripableAutomationControl")) {
			continue;
		}

		std::string s;

		if (!(*i)->get_property (X_("stripable"), s)) {
			continue;
		}

		std::string c;

		if (!(*i)->get_property (X_("control"), c)) {
			continue;
		}

		int order;

		if (!(*i)->get_property (X_("order"), order)) {
			continue;
		}

		SelectedStripable ss (PBD::ID (s), PBD::ID (c), order);
		_stripables.insert (ss);
	}

	return 0;
}

uint32_t
CoreSelection::selected () const
{
	Glib::Threads::RWLock::ReaderLock lm (_lock);
	return _stripables.size();
}

void
CoreSelection::get_stripables_for_op (std::shared_ptr<StripableList> sl, std::shared_ptr<Stripable> target, bool (RouteGroup::*group_predicate)() const) const
{
	return get_stripables_for_op (*sl.get(), target, group_predicate);
}

void
CoreSelection::get_stripables_for_op (StripableList& sl, std::shared_ptr<Stripable> target, bool (RouteGroup::*group_predicate)() const) const
{
	assert (target);

	std::shared_ptr<Route> r (std::dynamic_pointer_cast<Route> (target));

	if (_stripables.empty()) {

		if (r) {
			RouteGroup* rg = r->route_group();

			if (rg && rg->is_active() && (rg->*group_predicate)()) {
				for (auto & r : *rg->route_list()) {
					sl.push_back (r);
				}
			} else {
				/* target is not member of an active group that
				   shares the relevant property, and nothing is
				   selected, so use it and it alone.
				*/
				sl.push_back (target);
			}

		} else {
			/* Base is not a route, use it and it alone */
			sl.push_back (target);
		}

	} else {

		if (target->is_selected()) {

			/* Use full selection */

			StripableAutomationControls sc;
			get_stripables (sc);

			for (auto & s : sc) {
				sl.push_back (s.stripable);
			}

		} else {

			/* target not selected but might be part of a group */

			if (r) {
				RouteGroup* rg = r->route_group();

				if (rg && rg->is_active() && (rg->*group_predicate)()) {
					for (auto & r : *rg->route_list()) {
						sl.push_back (r);
					}
				} else {
					/* Target not selected, and not part of an
					 * active group that shares the relevant
					 * property, so use it and it alone
					 */
					sl.push_back (target);
				}
			} else {
				/* Base is not a route, use it and it alone */
				sl.push_back (target);
			}
		}
	}
}
