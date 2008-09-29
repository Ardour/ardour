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
#include <midi++/names.h>
#include <ardour/session.h>
#include <ardour/automatable.h>
#include <ardour/midi_track.h>
#include <ardour/plugin_insert.h>

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

nframes_t Automatable::_automation_interval = 0;

Automatable::Automatable(Session& session)
	: _a_session(session)
	, _last_automation_snapshot(0)
{
}

int
Automatable::old_set_automation_state (const XMLNode& node)
{
	const XMLProperty *prop;
			
	if ((prop = node.property ("path")) != 0) {
		load_automation (prop->value());
	} else {
		warning << _("Automation node has no path property") << endmsg;
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
			mark_automation_visible (Parameter(PluginAutomation, what), true);
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
		fullpath = _a_session.automation_dir();
		fullpath += path;
	}
	ifstream in (fullpath.c_str());

	if (!in) {
		warning << string_compose(_("cannot open %2 to load automation data (%3)")
				, fullpath, strerror (errno)) << endmsg;
		return 1;
	}

	Glib::Mutex::Lock lm (control_lock());
	set<Parameter> tosave;
	controls().clear ();
	
	_last_automation_snapshot = 0;

	while (in) {
		double when;
		double value;
		uint32_t port;

		in >> port;  if (!in) break;
		in >> when;  if (!in) goto bad;
		in >> value; if (!in) goto bad;
		
		/* FIXME: this is legacy and only used for plugin inserts?  I think? */
		boost::shared_ptr<Evoral::Control> c = control (Parameter(PluginAutomation, port), true);
		c->list()->add (when, value);
		tosave.insert (Parameter(PluginAutomation, port));
	}
	
	return 0;

  bad:
	error << string_compose(_("cannot load automation data from %2"), fullpath) << endmsg;
	controls().clear ();
	return -1;
}

void
Automatable::add_control(boost::shared_ptr<Evoral::Control> ac)
{
	Parameter param = ac->parameter();
	
	ControlSet::add_control(ac);
	_can_automate_list.insert(param);
	auto_state_changed(param); // sync everything up
}

void
Automatable::what_has_visible_data(set<Parameter>& s) const
{
	Glib::Mutex::Lock lm (control_lock());
	set<Parameter>::const_iterator li;
	
	for (li = _visible_controls.begin(); li != _visible_controls.end(); ++li) {
		s.insert  (*li);
	}
}

string
Automatable::describe_parameter (Parameter param)
{
	/* derived classes like PluginInsert should override this */

	if (param == Parameter(GainAutomation)) {
		return _("Fader");
	} else if (param.type() == PanAutomation) {
		/* ID's are zero-based, present them as 1-based */
		return (string_compose(_("Pan %1"), param.id() + 1));
	} else if (param.type() == MidiCCAutomation) {
		return string_compose("CC %1 (%2) [%3]",
				param.id() + 1, midi_name(param.id()), int(param.channel()) + 1);			
	} else if (param.type() == MidiPgmChangeAutomation) {
		return string_compose("Program [%1]", int(param.channel()) + 1);
	} else if (param.type() == MidiPitchBenderAutomation) {
		return string_compose("Bender [%1]", int(param.channel()) + 1);
	} else if (param.type() == MidiChannelPressureAutomation) {
		return string_compose("Pressure [%1]", int(param.channel()) + 1);
	} else {
		return param.symbol();
	}
}

void
Automatable::can_automate (Parameter what)
{
	_can_automate_list.insert (what);
}

void
Automatable::mark_automation_visible (Parameter what, bool yn)
{
	if (yn) {
		_visible_controls.insert (what);
	} else {
		set<Parameter>::iterator i;

		if ((i = _visible_controls.find (what)) != _visible_controls.end()) {
			_visible_controls.erase (i);
		}
	}
}

/** \a legacy_param is used for loading legacy sessions where an object (IO, Panner)
 * had a single automation parameter, with it's type implicit.  Derived objects should
 * pass that type and it will be used for the untyped AutomationList found.
 */
int
Automatable::set_automation_state (const XMLNode& node, Parameter legacy_param)
{	
	Glib::Mutex::Lock lm (control_lock());

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

			Parameter param = (id_prop ? Parameter(id_prop->value()) : legacy_param);
			if (param.type() == NullAutomation) {
				warning << "Automation has null type" << endl;
				continue;
			}
			
			boost::shared_ptr<AutomationList> al (new AutomationList(**niter, param));
			
			if (!id_prop) {
				warning << "AutomationList node without automation-id property, "
					<< "using default: " << legacy_param.symbol() << endmsg;
			}

			boost::shared_ptr<Evoral::Control> existing = control(param);
			if (existing)
				existing->set_list(al);
			else
				add_control(control_factory(param));

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
	Glib::Mutex::Lock lm (control_lock());
	XMLNode* node = new XMLNode (X_("Automation"));
	
	if (controls().empty()) {
		return *node;
	}

	for (Controls::iterator li = controls().begin(); li != controls().end(); ++li) {
		boost::shared_ptr<AutomationList> l
				= boost::dynamic_pointer_cast<AutomationList>(li->second->list());
		node->add_child_nocopy (l->get_state ());
	}

	return *node;
}

void
Automatable::set_parameter_automation_state (Parameter param, AutoState s)
{
	Glib::Mutex::Lock lm (control_lock());
	
	boost::shared_ptr<Evoral::Control> c = control (param, true);
	boost::shared_ptr<AutomationList> l = boost::dynamic_pointer_cast<AutomationList>(c->list());

	if (s != l->automation_state()) {
		l->set_automation_state (s);
		_a_session.set_dirty ();
	}
}

AutoState
Automatable::get_parameter_automation_state (Parameter param, bool lock)
{
	AutoState result = Off;

	if (lock)
		control_lock().lock();

	boost::shared_ptr<Evoral::Control> c = control(param);
	boost::shared_ptr<AutomationList> l = boost::dynamic_pointer_cast<AutomationList>(c->list());

	if (c)
		result = l->automation_state();
	
	if (lock)
		control_lock().unlock();

	return result;
}

void
Automatable::set_parameter_automation_style (Parameter param, AutoStyle s)
{
	Glib::Mutex::Lock lm (control_lock());
	
	boost::shared_ptr<Evoral::Control> c = control(param, true);
	boost::shared_ptr<AutomationList> l = boost::dynamic_pointer_cast<AutomationList>(c->list());

	if (s != l->automation_style()) {
		l->set_automation_style (s);
		_a_session.set_dirty ();
	}
}

AutoStyle
Automatable::get_parameter_automation_style (Parameter param)
{
	Glib::Mutex::Lock lm (control_lock());

	boost::shared_ptr<Evoral::Control> c = control(param);
	boost::shared_ptr<AutomationList> l = boost::dynamic_pointer_cast<AutomationList>(c->list());

	if (c) {
		return l->automation_style();
	} else {
		return Absolute; // whatever
	}
}

void
Automatable::protect_automation ()
{
	typedef set<Evoral::Parameter> ParameterSet;
	ParameterSet automated_params;

	what_has_data(automated_params);

	for (ParameterSet::iterator i = automated_params.begin(); i != automated_params.end(); ++i) {

		boost::shared_ptr<Evoral::Control> c = control(*i);
		boost::shared_ptr<AutomationList> l = boost::dynamic_pointer_cast<AutomationList>(c->list());

		switch (l->automation_state()) {
		case Write:
			l->set_automation_state (Off);
			break;
		case Touch:
			l->set_automation_state (Play);
			break;
		default:
			break;
		}
	}
}

void
Automatable::automation_snapshot (nframes_t now, bool force)
{
	if (force || _last_automation_snapshot > now || (now - _last_automation_snapshot) > _automation_interval) {

		for (Controls::iterator i = controls().begin(); i != controls().end(); ++i) {
			boost::shared_ptr<AutomationControl> c
					= boost::dynamic_pointer_cast<AutomationControl>(i->second);
			if (c->automation_write()) {
				c->list()->rt_add (now, i->second->user_float());
			}
		}
		
		_last_automation_snapshot = now;
	}
}

void
Automatable::transport_stopped (nframes_t now)
{
	for (Controls::iterator li = controls().begin(); li != controls().end(); ++li) {
		
		boost::shared_ptr<AutomationControl> c
				= boost::dynamic_pointer_cast<AutomationControl>(li->second);
		boost::shared_ptr<AutomationList> l
				= boost::dynamic_pointer_cast<AutomationList>(c->list());
		
		c->list()->reposition_for_rt_add (now);

		if (c->automation_state() != Off) {
			c->set_value(c->list()->eval(now));
		}
	}
}

boost::shared_ptr<Evoral::Control>
Automatable::control_factory(const Evoral::Parameter& param)
{
	boost::shared_ptr<AutomationList> list(new AutomationList(param));
	Evoral::Control* control = NULL;
	if (param.type() >= MidiCCAutomation && param.type() <= MidiChannelPressureAutomation) {
		control = new MidiTrack::MidiControl((MidiTrack*)this, param);
	} else if (param.type() == PluginAutomation) {
		control = new PluginInsert::PluginControl((PluginInsert*)this, param);
	} else {
		control = new AutomationControl(_a_session, param);
	}
	control->set_list(list);
	return boost::shared_ptr<Evoral::Control>(control);
}

