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
	g_atomic_int_set (&_selection_order, 0);
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
			if (select_stripable_and_maybe_group (*s, true, routes_only, 0)) {
				break;
			}
		}

		return;
	}

	/* fetch the current selection so that we can get the most recently selected */
	StripableAutomationControls selected;
	get_stripables (selected);
	boost::shared_ptr<Stripable> last_selected =
	  selected.empty () ? boost::shared_ptr<Stripable> ()
	                    : selected.back ().stripable;

	/* Get all stripables and sort into the appropriate ordering */
	StripableList stripables;
	session.get_stripables (stripables);
	stripables.sort (ARDOUR::Stripable::Sorter (mixer_order));


	/* Check for a possible selection-affecting route group */

	RouteGroup* group = 0;
	boost::shared_ptr<Route> r = boost::dynamic_pointer_cast<Route> (last_selected);

	if (r && r->route_group() && r->route_group()->is_select() && r->route_group()->is_active()) {
		group = r->route_group();
	}

	bool select_me = false;

	for (IterTypeCore i = (stripables.*begin_method)(); i != (stripables.*end_method)(); ++i) {

		if (select_me) {

			if (!this->selected (*i)) { /* not currently selected */
				if (select_stripable_and_maybe_group (*i, true, routes_only, group)) {
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

		r = boost::dynamic_pointer_cast<Route> (*s);

		/* monitor is never selectable anywhere. for now, anyway */

		if (!routes_only || r) {
			if (select_stripable_and_maybe_group (*s, true, routes_only, 0)) {
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
CoreSelection::select_stripable_and_maybe_group (boost::shared_ptr<Stripable> s, bool with_group, bool routes_only, RouteGroup* not_allowed_in_group)
{
	boost::shared_ptr<Route> r;
	StripableList sl;

	/* no selection of hidden stripables (though they can be selected and
	 * then hidden
	 */

	if (s->is_hidden()) {
		return false;
	}

	/* monitor is never selectable */

	if (s->is_monitor()) {
		return false;
	}

	if ((r = boost::dynamic_pointer_cast<Route> (s))) {

		/* no selection of inactive routes, though they can be selected
		 * and made inactive.
		 */

		if (!r->active()) {
			return false;
		}

		if (with_group) {

			if (!not_allowed_in_group || !r->route_group() || r->route_group() != not_allowed_in_group) {

				if (r->route_group() && r->route_group()->is_select() && r->route_group()->is_active()) {
					boost::shared_ptr<RouteList> rl = r->route_group()->route_list ();
					for (RouteList::iterator ri = rl->begin(); ri != rl->end(); ++ri) {
						if (*ri != r) {
							sl.push_back (*ri);
						}
					}
				}

				/* it is important to make the "primary" stripable being selected the last in this
				 * list
				 */

				sl.push_back (s);
				set (sl);
				return true;
			}

		} else {
			set (s, boost::shared_ptr<AutomationControl>());
			return true;
		}

	} else if (!routes_only) {
		set (s, boost::shared_ptr<AutomationControl>());
		return true;
	}

	return false;
}

void
CoreSelection::toggle (boost::shared_ptr<Stripable> s, boost::shared_ptr<AutomationControl> c)
{
	DEBUG_TRACE (DEBUG::Selection, string_compose ("toggle: s %1 selected %2 c %3 selected %4\n",
	                                               s, selected (s), c, selected (c)));
	if ((c && selected (c)) || selected (s)) {
		remove (s, c);
	} else {
		add (s, c);
	}
}

void
CoreSelection::set (StripableList& sl)
{
	bool send = false;
	boost::shared_ptr<AutomationControl> no_control;

	std::vector<boost::shared_ptr<Stripable> > removed;

	{
		Glib::Threads::RWLock::WriterLock lm (_lock);

		removed.reserve (_stripables.size());

		for (SelectedStripables::const_iterator x = _stripables.begin(); x != _stripables.end(); ++x) {
			boost::shared_ptr<Stripable> sp = session.stripable_by_id ((*x).stripable);
			if (sp) {
				removed.push_back (sp);
			}
		}

		_stripables.clear ();

		for (StripableList::iterator s = sl.begin(); s != sl.end(); ++s) {

			SelectedStripable ss (*s, no_control, g_atomic_int_add (&_selection_order, 1));

			if (_stripables.insert (ss).second) {
				DEBUG_TRACE (DEBUG::Selection, string_compose ("set:added %1 to s/c selection\n", (*s)->name()));
				send = true;
			} else {
				DEBUG_TRACE (DEBUG::Selection, string_compose ("%1 already in s/c selection\n", (*s)->name()));
			}
		}

		if (sl.size () > 0) {
			_first_selected_stripable = sl.back ();
		} else {
			_first_selected_stripable.reset ();
		}
	}

	if (send || !removed.empty()) {

		send_selection_change ();

		/* send per-object signal to notify interested parties
		   the selection status has changed
		*/

		PropertyChange pc (Properties::selected);

		for (std::vector<boost::shared_ptr<Stripable> >::iterator s = removed.begin(); s != removed.end(); ++s) {
			(*s)->presentation_info().PropertyChanged (pc);
		}

		for (StripableList::iterator s = sl.begin(); s != sl.end(); ++s) {
			(*s)->presentation_info().PropertyChanged (pc);
		}

	}

}

void
CoreSelection::add (boost::shared_ptr<Stripable> s, boost::shared_ptr<AutomationControl> c)
{
	bool send = false;

	{
		Glib::Threads::RWLock::WriterLock lm (_lock);

		SelectedStripable ss (s, c, g_atomic_int_add (&_selection_order, 1));

		if (_stripables.insert (ss).second) {
			DEBUG_TRACE (DEBUG::Selection, string_compose ("added %1/%2 to s/c selection\n", s->name(), c));
			send = true;
		} else {
			DEBUG_TRACE (DEBUG::Selection, string_compose ("%1/%2 already in s/c selection\n", s->name(), c));
		}
		_first_selected_stripable = s;
	}

	if (send) {
		send_selection_change ();
		/* send per-object signal to notify interested parties
		   the selection status has changed
		*/
		if (s) {
			PropertyChange pc (Properties::selected);
			s->presentation_info().PropertyChanged (pc);
		}
	}
}

void
CoreSelection::remove (boost::shared_ptr<Stripable> s, boost::shared_ptr<AutomationControl> c)
{
	bool send = false;
	{
		Glib::Threads::RWLock::WriterLock lm (_lock);

		SelectedStripable ss (s, c, 0);

		SelectedStripables::iterator i = _stripables.find (ss);

		if (i != _stripables.end()) {
			_stripables.erase (i);
			DEBUG_TRACE (DEBUG::Selection, string_compose ("removed %1/%2 from s/c selection\n", s, c));
			send = true;
		}
		if (s == _first_selected_stripable.lock ()) {
			_first_selected_stripable.reset ();
		}
	}

	if (send) {
		send_selection_change ();
		/* send per-object signal to notify interested parties
		   the selection status has changed
		*/
		if (s) {
			PropertyChange pc (Properties::selected);
			s->presentation_info().PropertyChanged (pc);
		}
	}
}

void
CoreSelection::set (boost::shared_ptr<Stripable> s, boost::shared_ptr<AutomationControl> c)
{
	{
		Glib::Threads::RWLock::WriterLock lm (_lock);

		SelectedStripable ss (s, c, g_atomic_int_add (&_selection_order, 1));

		if (_stripables.size() == 1 && _stripables.find (ss) != _stripables.end()) {
			return;
		}

		_stripables.clear ();
		_stripables.insert (ss);
		_first_selected_stripable = s;
		DEBUG_TRACE (DEBUG::Selection, string_compose ("set s/c selection to %1/%2\n", s->name(), c));
	}

	send_selection_change ();

	/* send per-object signal to notify interested parties
	   the selection status has changed
	*/
	if (s) {
		PropertyChange pc (Properties::selected);
		s->presentation_info().PropertyChanged (pc);
	}
}

void
CoreSelection::clear_stripables ()
{
	bool send = false;
	std::vector<boost::shared_ptr<Stripable> > s;

	DEBUG_TRACE (DEBUG::Selection, "clearing s/c selection\n");
	{
		Glib::Threads::RWLock::WriterLock lm (_lock);

		if (!_stripables.empty()) {

			s.reserve (_stripables.size());

			for (SelectedStripables::const_iterator x = _stripables.begin(); x != _stripables.end(); ++x) {
				boost::shared_ptr<Stripable> sp = session.stripable_by_id ((*x).stripable);
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

		for (std::vector<boost::shared_ptr<Stripable> >::iterator ss = s.begin(); ss != s.end(); ++ss) {
			(*ss)->presentation_info().PropertyChanged (pc);
		}

	}
}

boost::shared_ptr<Stripable>
CoreSelection::first_selected_stripable () const
{
	Glib::Threads::RWLock::ReaderLock lm (_lock);
  return _first_selected_stripable.lock();
}

bool
CoreSelection::selected (boost::shared_ptr<const Stripable> s) const
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
CoreSelection::selected (boost::shared_ptr<const AutomationControl> c) const
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

CoreSelection::SelectedStripable::SelectedStripable (boost::shared_ptr<Stripable> s, boost::shared_ptr<AutomationControl> c, int o)
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

		boost::shared_ptr<Stripable> s = session.stripable_by_id ((*x).stripable);
		boost::shared_ptr<AutomationControl> c;

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
CoreSelection::get_state (void)
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
