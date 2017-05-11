/*
  Copyright (C) 2017 Paul Davis

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

#include <vector>

#include "pbd/compose.h"
#include "pbd/signals.h"

#include "ardour/automation_control.h"
#include "ardour/debug.h"
#include "ardour/selection.h"
#include "ardour/session.h"
#include "ardour/stripable.h"

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
	, selection_order (0)
{
}

CoreSelection::~CoreSelection ()
{
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
CoreSelection::add (boost::shared_ptr<Stripable> s, boost::shared_ptr<AutomationControl> c)
{
	bool send = false;

	{
		Glib::Threads::RWLock::WriterLock lm (_lock);

		SelectedStripable ss (s, c, g_atomic_int_add (&selection_order, 1));

		if (_stripables.insert (ss).second) {
			DEBUG_TRACE (DEBUG::Selection, string_compose ("added %1/%2 to s/c selection\n", s->name(), c));
			send = true;
		} else {
			DEBUG_TRACE (DEBUG::Selection, string_compose ("%1/%2 already in s/c selection\n", s->name(), c));
		}
	}

	if (send) {
		send_selection_change ();
		/* send per-object signal to notify interested parties
		   the selection status has changed
		*/
		if (s) {
			PropertyChange pc (Properties::selected);
			s->PropertyChanged (pc);
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
	}

	if (send) {
		send_selection_change ();
		/* send per-object signal to notify interested parties
		   the selection status has changed
		*/
		if (s) {
			PropertyChange pc (Properties::selected);
			s->PropertyChanged (pc);
		}
	}
}

void
CoreSelection::set (boost::shared_ptr<Stripable> s, boost::shared_ptr<AutomationControl> c)
{
	{
		Glib::Threads::RWLock::WriterLock lm (_lock);

		SelectedStripable ss (s, c, g_atomic_int_add (&selection_order, 1));

		if (_stripables.size() == 1 && _stripables.find (ss) != _stripables.end()) {
			return;
		}

		_stripables.clear ();
		_stripables.insert (ss);
		DEBUG_TRACE (DEBUG::Selection, string_compose ("set s/c selection to %1/%2\n", s->name(), c));
	}

	send_selection_change ();

	/* send per-object signal to notify interested parties
	   the selection status has changed
	*/
	if (s) {
		PropertyChange pc (Properties::selected);
		s->PropertyChanged (pc);
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
	}

	if (send) {
		send_selection_change ();

		PropertyChange pc (Properties::selected);

		for (std::vector<boost::shared_ptr<Stripable> >::iterator ss = s.begin(); ss != s.end(); ++ss) {
			(*ss)->PropertyChanged (pc);
		}

	}
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

	std::cerr << "set state: " << _stripables.size() << std::endl;

	return 0;
}

uint32_t
CoreSelection::selected () const
{
	Glib::Threads::RWLock::ReaderLock lm (_lock);
	return _stripables.size();
}
