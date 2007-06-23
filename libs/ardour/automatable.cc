/*
    Copyright (C) 2001,2007 Paul Davis 

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

#include <ardour/ardour.h>
#include <fstream>
#include <inttypes.h>
#include <cstdio>
#include <errno.h>
#include <pbd/error.h>
#include <pbd/enumwriter.h>
#include <ardour/session.h>
#include <ardour/automatable.h>

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;


Automatable::Automatable(Session& _session, const string& name)
	: SessionObject(_session, name)
	, _last_automation_snapshot(0)
{}

int
Automatable::old_set_automation_state (const XMLNode& node)
{
	const XMLProperty *prop;
			
	if ((prop = node.property ("path")) != 0) {
		load_automation (prop->value());
	} else {
		warning << string_compose(_("%1: Automation node has no path property"), _name) << endmsg;
	}
	
	if ((prop = node.property ("visible")) != 0) {
		uint32_t what;
		stringstream sstr;
		
		_visible_parameter_automation.clear ();
		
		sstr << prop->value();
		while (1) {
			sstr >> what;
			if (sstr.fail()) {
				break;
			}
			mark_automation_visible (what, true);
		}
	}

	return 0;
}

int
Automatable::load_automation (const string& path)
{
	string fullpath;

	if (path[0] == '/') { // legacy
		fullpath = path;
	} else {
		fullpath = _session.automation_dir();
		fullpath += path;
	}
	ifstream in (fullpath.c_str());

	if (!in) {
		warning << string_compose(_("%1: cannot open %2 to load automation data (%3)"), _name, fullpath, strerror (errno)) << endmsg;
		return 1;
	}

	Glib::Mutex::Lock lm (_automation_lock);
	set<uint32_t> tosave;
	_parameter_automation.clear ();

	while (in) {
		double when;
		double value;
		uint32_t port;

		in >> port;  if (!in) break;
		in >> when;  if (!in) goto bad;
		in >> value; if (!in) goto bad;
		
		AutomationList& al = automation_list (port);
		al.add (when, value);
		tosave.insert (port);
	}
	
	return 0;

  bad:
	error << string_compose(_("%1: cannot load automation data from %2"), _name, fullpath) << endmsg;
	_parameter_automation.clear ();
	return -1;
}


void
Automatable::what_has_automation (set<uint32_t>& s) const
{
	Glib::Mutex::Lock lm (_automation_lock);
	map<uint32_t,AutomationList*>::const_iterator li;
	
	for (li = _parameter_automation.begin(); li != _parameter_automation.end(); ++li) {
		s.insert  ((*li).first);
	}
}

void
Automatable::what_has_visible_automation (set<uint32_t>& s) const
{
	Glib::Mutex::Lock lm (_automation_lock);
	set<uint32_t>::const_iterator li;
	
	for (li = _visible_parameter_automation.begin(); li != _visible_parameter_automation.end(); ++li) {
		s.insert  (*li);
	}
}
AutomationList&
Automatable::automation_list (uint32_t parameter)
{
	AutomationList* al = _parameter_automation[parameter];

	if (al == 0) {
		al = _parameter_automation[parameter] = new AutomationList (default_parameter_value (parameter));
		/* let derived classes do whatever they need with this */
		automation_list_creation_callback (parameter, *al);
	}

	return *al;
}

string
Automatable::describe_parameter (uint32_t which)
{
	/* derived classes will override this */
	return "";
}

void
Automatable::can_automate (uint32_t what)
{
	_can_automate_list.insert (what);
}

void
Automatable::mark_automation_visible (uint32_t what, bool yn)
{
	if (yn) {
		_visible_parameter_automation.insert (what);
	} else {
		set<uint32_t>::iterator i;

		if ((i = _visible_parameter_automation.find (what)) != _visible_parameter_automation.end()) {
			_visible_parameter_automation.erase (i);
		}
	}
}

bool
Automatable::find_next_event (nframes_t now, nframes_t end, ControlEvent& next_event) const
{
	map<uint32_t,AutomationList*>::const_iterator li;	
	AutomationList::TimeComparator cmp;

	next_event.when = max_frames;
	
  	for (li = _parameter_automation.begin(); li != _parameter_automation.end(); ++li) {
		
		AutomationList::const_iterator i;
 		const AutomationList& alist (*((*li).second));
		ControlEvent cp (now, 0.0f);
		
 		for (i = lower_bound (alist.const_begin(), alist.const_end(), &cp, cmp); i != alist.const_end() && (*i)->when < end; ++i) {
 			if ((*i)->when > now) {
 				break; 
 			}
 		}
 		
 		if (i != alist.const_end() && (*i)->when < end) {
			
 			if ((*i)->when < next_event.when) {
 				next_event.when = (*i)->when;
 			}
 		}
 	}

 	return next_event.when != max_frames;
}

int
Automatable::set_automation_state (const XMLNode& node)
{	
	Glib::Mutex::Lock lm (_automation_lock);

	_parameter_automation.clear ();

	XMLNodeList nlist = node.children();
	XMLNodeIterator niter;

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		uint32_t param;

		if (sscanf ((*niter)->name().c_str(), "parameter-%" PRIu32, &param) != 1) {
			error << string_compose (_("%2: badly formatted node name in XML automation state, ignored"), _name) << endmsg;
			continue;
		}

		AutomationList& al = automation_list (param);
		if (al.set_state (*(*niter)->children().front())) {
			goto bad;
		}
	}

	return 0;

  bad:
	error << string_compose(_("%1: cannot load automation data from XML"), _name) << endmsg;
	_parameter_automation.clear ();
	return -1;
}

XMLNode&
Automatable::get_automation_state ()
{
	Glib::Mutex::Lock lm (_automation_lock);
	XMLNode* node = new XMLNode (X_("Automation"));
	string fullpath;

	if (_parameter_automation.empty()) {
		return *node;
	}

	map<uint32_t,AutomationList*>::iterator li;
	
	for (li = _parameter_automation.begin(); li != _parameter_automation.end(); ++li) {
	
		XMLNode* child;
		
		char buf[64];
		stringstream str;
		snprintf (buf, sizeof (buf), "parameter-%" PRIu32, li->first);
		child = new XMLNode (buf);
		child->add_child_nocopy (li->second->get_state ());
	}

	return *node;
}
