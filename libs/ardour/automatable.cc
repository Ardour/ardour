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
		
		_visible_controls.clear ();
		
		sstr << prop->value();
		while (1) {
			sstr >> what;
			if (sstr.fail()) {
				break;
			}
			mark_automation_visible (ParamID(PluginAutomation, what), true);
		}
	}
	
	_last_automation_snapshot = 0;

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
	set<ParamID> tosave;
	_controls.clear ();
	
	_last_automation_snapshot = 0;

	while (in) {
		double when;
		double value;
		uint32_t port;

		in >> port;  if (!in) break;
		in >> when;  if (!in) goto bad;
		in >> value; if (!in) goto bad;
		
		/* FIXME: this is legacy and only used for plugin inserts?  I think? */
		boost::shared_ptr<AutomationControl> c = control (ParamID(PluginAutomation, port), true);
		c->list()->add (when, value);
		tosave.insert (ParamID(PluginAutomation, port));
	}
	
	return 0;

  bad:
	error << string_compose(_("%1: cannot load automation data from %2"), _name, fullpath) << endmsg;
	_controls.clear ();
	return -1;
}

void
Automatable::add_control(boost::shared_ptr<AutomationControl> ac)
{
	ParamID param = ac->list()->param_id();

	_controls[param] = ac;
	
	cerr << _name << ": added parameter " << param.to_string() << endl;

	// FIXME: sane default behaviour?
	_visible_controls.insert(param);
	_can_automate_list.insert(param);

	// Sync everything (derived classes) up to initial values
	auto_state_changed(param);
}

void
Automatable::what_has_automation (set<ParamID>& s) const
{
	Glib::Mutex::Lock lm (_automation_lock);
	Controls::const_iterator li;
	
	// FIXME: correct semantics?
	for (li = _controls.begin(); li != _controls.end(); ++li) {
		s.insert  ((*li).first);
	}
}

void
Automatable::what_has_visible_automation (set<ParamID>& s) const
{
	Glib::Mutex::Lock lm (_automation_lock);
	set<ParamID>::const_iterator li;
	
	for (li = _visible_controls.begin(); li != _visible_controls.end(); ++li) {
		s.insert  (*li);
	}
}

/** Returns NULL if we don't have an AutomationList for \a parameter.
 */
boost::shared_ptr<AutomationControl>
Automatable::control (ParamID parameter, bool create_if_missing)
{
	Controls::iterator i = _controls.find(parameter);

	if (i != _controls.end()) {
		return i->second;

	} else if (create_if_missing) {
		assert(parameter.type() != GainAutomation);
		boost::shared_ptr<AutomationList> al (new AutomationList (
					parameter, FLT_MIN, FLT_MAX, default_parameter_value (parameter)));
		boost::shared_ptr<AutomationControl> ac (new AutomationControl(_session, al));
		add_control(ac);
		return ac;

	} else {
		//warning << "AutomationList " << parameter.to_string() << " not found for " << _name << endmsg;
		return boost::shared_ptr<AutomationControl>();
	}
}

boost::shared_ptr<const AutomationControl>
Automatable::control (ParamID parameter) const
{
	Controls::const_iterator i = _controls.find(parameter);

	if (i != _controls.end()) {
		return i->second;
	} else {
		//warning << "AutomationList " << parameter.to_string() << " not found for " << _name << endmsg;
		return boost::shared_ptr<AutomationControl>();
	}
}


string
Automatable::describe_parameter (ParamID param)
{
	/* derived classes like PluginInsert should override this */

	if (param == ParamID(GainAutomation))
		return _("Fader");
	else if (param == ParamID(PanAutomation))
		return _("Pan");
	else if (param.type() == MidiCCAutomation)
		return string_compose("CC %1", param.id());
	else
		return param.to_string();
}

void
Automatable::can_automate (ParamID what)
{
	_can_automate_list.insert (what);
}

void
Automatable::mark_automation_visible (ParamID what, bool yn)
{
	if (yn) {
		_visible_controls.insert (what);
	} else {
		set<ParamID>::iterator i;

		if ((i = _visible_controls.find (what)) != _visible_controls.end()) {
			_visible_controls.erase (i);
		}
	}
}

bool
Automatable::find_next_event (nframes_t now, nframes_t end, ControlEvent& next_event) const
{
	Controls::const_iterator li;	
	AutomationList::TimeComparator cmp;

	next_event.when = max_frames;
	
  	for (li = _controls.begin(); li != _controls.end(); ++li) {
		
		AutomationList::const_iterator i;
		boost::shared_ptr<const AutomationList> alist (li->second->list());
		ControlEvent cp (now, 0.0f);
		
 		for (i = lower_bound (alist->const_begin(), alist->const_end(), &cp, cmp); i != alist->const_end() && (*i)->when < end; ++i) {
 			if ((*i)->when > now) {
 				break; 
 			}
 		}
 		
 		if (i != alist->const_end() && (*i)->when < end) {
			
 			if ((*i)->when < next_event.when) {
 				next_event.when = (*i)->when;
 			}
 		}
 	}

 	return next_event.when != max_frames;
}

/** \a legacy_param is used for loading legacy sessions where an object (IO, Panner)
 * had a single automation parameter, with it's type implicit.  Derived objects should
 * pass that type and it will be used for the untyped AutomationList found.
 */
int
Automatable::set_automation_state (const XMLNode& node, ParamID legacy_param)
{	
	Glib::Mutex::Lock lm (_automation_lock);

	/* Don't clear controls, since some may be special derived Controllable classes */

	_visible_controls.clear ();

	XMLNodeList nlist = node.children();
	XMLNodeIterator niter;
	
	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {

		/*if (sscanf ((*niter)->name().c_str(), "parameter-%" PRIu32, &param) != 1) {
		  error << string_compose (_("%2: badly formatted node name in XML automation state, ignored"), _name) << endmsg;
		  continue;
		  }*/

		if ((*niter)->name() == "AutomationList") {

			const XMLProperty* id_prop = (*niter)->property("automation-id");

			ParamID param = (id_prop ? ParamID(id_prop->value()) : legacy_param);
			
			boost::shared_ptr<AutomationList> al (new AutomationList(**niter, param));
			
			if (!id_prop) {
				warning << "AutomationList node without automation-id property, "
					<< "using default: " << legacy_param.to_string() << endmsg;
				al->set_param_id(legacy_param);
			}

			boost::shared_ptr<AutomationControl> existing = control(param);
			if (existing)
				existing->set_list(al);
			else
				add_control(boost::shared_ptr<AutomationControl>(new AutomationControl(_session, al)));

		} else {
			error << "Expected AutomationList node, got '" << (*niter)->name() << endmsg;
		}
	}

	_last_automation_snapshot = 0;

	return 0;
}

XMLNode&
Automatable::get_automation_state ()
{
	Glib::Mutex::Lock lm (_automation_lock);
	XMLNode* node = new XMLNode (X_("Automation"));
	
	if (_controls.empty()) {
		return *node;
	}

	for (Controls::iterator li = _controls.begin(); li != _controls.end(); ++li) {
		node->add_child_nocopy (li->second->list()->get_state ());
	}

	return *node;
}

void
Automatable::clear_automation ()
{
	Glib::Mutex::Lock lm (_automation_lock);

	for (Controls::iterator li = _controls.begin(); li != _controls.end(); ++li)
		li->second->list()->clear();
}
	
void
Automatable::set_parameter_automation_state (ParamID param, AutoState s)
{
	Glib::Mutex::Lock lm (_automation_lock);
	
	boost::shared_ptr<AutomationControl> c = control (param, true);

	if (s != c->list()->automation_state()) {
		c->list()->set_automation_state (s);
		_session.set_dirty ();
	}
}

AutoState
Automatable::get_parameter_automation_state (ParamID param)
{
	Glib::Mutex::Lock lm (_automation_lock);

	boost::shared_ptr<AutomationControl> c = control(param);

	if (c) {
		return c->list()->automation_state();
	} else {
		return Off;
	}
}

void
Automatable::set_parameter_automation_style (ParamID param, AutoStyle s)
{
	Glib::Mutex::Lock lm (_automation_lock);
	
	boost::shared_ptr<AutomationControl> c = control(param, true);

	if (s != c->list()->automation_style()) {
		c->list()->set_automation_style (s);
		_session.set_dirty ();
	}
}

AutoStyle
Automatable::get_parameter_automation_style (ParamID param)
{
	Glib::Mutex::Lock lm (_automation_lock);

	boost::shared_ptr<AutomationControl> c = control(param);

	if (c) {
		return c->list()->automation_style();
	} else {
		return Absolute; // whatever
	}
}

void
Automatable::protect_automation ()
{
	set<ParamID> automated_params;

	what_has_automation (automated_params);

	for (set<ParamID>::iterator i = automated_params.begin(); i != automated_params.end(); ++i) {

		boost::shared_ptr<AutomationControl> c = control(*i);

		switch (c->list()->automation_state()) {
		case Write:
			c->list()->set_automation_state (Off);
			break;
		case Touch:
			c->list()->set_automation_state (Play);
			break;
		default:
			break;
		}
	}
}

void
Automatable::automation_snapshot (nframes_t now)
{
	if (_last_automation_snapshot > now || (now - _last_automation_snapshot) > _session.automation_interval()) {

		for (Controls::iterator i = _controls.begin(); i != _controls.end(); ++i) {
			if (i->second->list()->automation_write()) {
				i->second->list()->rt_add (now, i->second->user_value());
			}
		}
		
		_last_automation_snapshot = now;
	}
}

