/*
    Copyright (C) 2001 Paul Davis 

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

#include <fstream>
#include <algorithm>
#include <string>
#include <cerrno>
#include <unistd.h>
#include <sstream>

#include <sigc++/bind.h>

#include <pbd/xml++.h>
#include <pbd/enumwriter.h>

#include <ardour/redirect.h>
#include <ardour/session.h>
#include <ardour/utils.h>
#include <ardour/send.h>
#include <ardour/insert.h>

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

const string Redirect::state_node_name = "Redirect";
sigc::signal<void,Redirect*> Redirect::RedirectCreated;

Redirect::Redirect (Session& s, const string& name, Placement p,

		    int input_min, int input_max, int output_min, int output_max)
	: IO (s, name, input_min, input_max, output_min, output_max)
{
	_placement = p;
	_active = false;
	_sort_key = 0;
	_gui = 0;
	_extra_xml = 0;
}

Redirect::~Redirect ()
{
	notify_callbacks ();
}

boost::shared_ptr<Redirect>
Redirect::clone (boost::shared_ptr<const Redirect> other)
{
	boost::shared_ptr<const Send> send;
	boost::shared_ptr<const PortInsert> port_insert;
	boost::shared_ptr<const PluginInsert> plugin_insert;

	if ((send = boost::dynamic_pointer_cast<const Send>(other)) != 0) {
		return boost::shared_ptr<Redirect> (new Send (*send));
	} else if ((port_insert = boost::dynamic_pointer_cast<const PortInsert>(other)) != 0) {
		return boost::shared_ptr<Redirect> (new PortInsert (*port_insert));
	} else if ((plugin_insert = boost::dynamic_pointer_cast<const PluginInsert>(other)) != 0) {
		return boost::shared_ptr<Redirect> (new PluginInsert (*plugin_insert));
	} else {
		fatal << _("programming error: unknown Redirect type in Redirect::Clone!\n")
		      << endmsg;
		/*NOTREACHED*/
	}
	return boost::shared_ptr<Redirect>();
}

void
Redirect::set_sort_key (uint32_t key)
{
	_sort_key = key;
}
	
void
Redirect::set_placement (Placement p, void *src)
{
	if (_placement != p) {
		_placement = p;
		 placement_changed (this, src); /* EMIT SIGNAL */
	}
}

/* NODE STRUCTURE 
   
    <Automation [optionally with visible="...." ]>
       <parameter-N>
         <AutomationList id=N>
	   <events>
	   X1 Y1
	   X2 Y2
	   ....
	   </events>
       </parameter-N>
    <Automation>
*/

int
Redirect::set_automation_state (const XMLNode& node)
{
	Glib::Mutex::Lock lm (_automation_lock);

	parameter_automation.clear ();

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
	parameter_automation.clear ();
	return -1;
}

XMLNode&
Redirect::get_automation_state ()
{
	Glib::Mutex::Lock lm (_automation_lock);
	XMLNode* node = new XMLNode (X_("Automation"));
	string fullpath;

	if (parameter_automation.empty()) {
		return *node;
	}

	map<uint32_t,AutomationList*>::iterator li;
	
	for (li = parameter_automation.begin(); li != parameter_automation.end(); ++li) {
	
		XMLNode* child;
		
		char buf[64];
		stringstream str;
		snprintf (buf, sizeof (buf), "parameter-%" PRIu32, li->first);
		child = new XMLNode (buf);
		child->add_child_nocopy (li->second->get_state ());
	}

	return *node;
}

XMLNode&
Redirect::get_state (void)
{
	return state (true);
}

XMLNode&
Redirect::state (bool full_state)
{
	XMLNode* node = new XMLNode (state_node_name);
	stringstream sstr;

	node->add_property("active", active() ? "yes" : "no");	
	node->add_property("placement", enum_2_string (_placement));
	node->add_child_nocopy (IO::state (full_state));

	if (_extra_xml){
		node->add_child_copy (*_extra_xml);
	}
	
	if (full_state) {

		XMLNode& automation = get_automation_state(); 
		
		for (set<uint32_t>::iterator x = visible_parameter_automation.begin(); x != visible_parameter_automation.end(); ++x) {
			if (x != visible_parameter_automation.begin()) {
				sstr << ' ';
			}
			sstr << *x;
		}

		automation.add_property ("visible", sstr.str());

		node->add_child_nocopy (automation);
	}

	return *node;
}


int
Redirect::set_state (const XMLNode& node)
{
	const XMLProperty *prop;

	if (node.name() != state_node_name) {
		error << string_compose(_("incorrect XML node \"%1\" passed to Redirect object"), node.name()) << endmsg;
		return -1;
	}

	XMLNodeList nlist = node.children();
	XMLNodeIterator niter;
	bool have_io = false;

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {

		if ((*niter)->name() == IO::state_node_name) {

			IO::set_state (**niter);
			have_io = true;

		} else if ((*niter)->name() == X_("Automation")) {


			XMLProperty *prop;
			
			if ((prop = (*niter)->property ("path")) != 0) {
				old_set_automation_state (*(*niter));
			} else {
				set_automation_state (*(*niter));
			}

			if ((prop = (*niter)->property ("visible")) != 0) {
				uint32_t what;
				stringstream sstr;

				visible_parameter_automation.clear ();
				
				sstr << prop->value();
				while (1) {
					sstr >> what;
					if (sstr.fail()) {
						break;
					}
					mark_automation_visible (what, true);
				}
			}

		} else if ((*niter)->name() == "extra") {
			_extra_xml = new XMLNode (*(*niter));
		}
	}

	if (!have_io) {
		error << _("XML node describing an IO is missing an IO node") << endmsg;
		return -1;
	}

	if ((prop = node.property ("active")) == 0) {
		error << _("XML node describing a redirect is missing the `active' field") << endmsg;
		return -1;
	}

	if (_active != (prop->value() == "yes")) {
		if (!(_session.state_of_the_state() & Session::Loading) || 
		    !Session::get_disable_all_loaded_plugins()) {
			_active = !_active;
			active_changed (this, this); /* EMIT_SIGNAL */
		}
	}
	
	if ((prop = node.property ("placement")) == 0) {
		error << _("XML node describing a redirect is missing the `placement' field") << endmsg;
		return -1;
	}

	/* hack to handle older sessions before we only used EnumWriter */

	string pstr;

	if (prop->value() == "pre") {
		pstr = "PreFader";
	} else if (prop->value() == "post") {
		pstr = "PostFader";
	} else {
		pstr = prop->value();
	}

	Placement p = Placement (string_2_enum (pstr, p));
	set_placement (p, this);

	return 0;
}

int
Redirect::old_set_automation_state (const XMLNode& node)
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
		
		visible_parameter_automation.clear ();
		
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
Redirect::load_automation (string path)
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
	parameter_automation.clear ();

	while (in) {
		double when;
		double value;
		uint32_t port;

		in >> port;     if (!in) break;
		in >> when;  if (!in) goto bad;
		in >> value; if (!in) goto bad;
		
		AutomationList& al = automation_list (port);
		al.add (when, value);
		tosave.insert (port);
	}
	
	return 0;

  bad:
	error << string_compose(_("%1: cannot load automation data from %2"), _name, fullpath) << endmsg;
	parameter_automation.clear ();
	return -1;
}


void
Redirect::what_has_automation (set<uint32_t>& s) const
{
	Glib::Mutex::Lock lm (_automation_lock);
	map<uint32_t,AutomationList*>::const_iterator li;
	
	for (li = parameter_automation.begin(); li != parameter_automation.end(); ++li) {
		s.insert  ((*li).first);
	}
}

void
Redirect::what_has_visible_automation (set<uint32_t>& s) const
{
	Glib::Mutex::Lock lm (_automation_lock);
	set<uint32_t>::const_iterator li;
	
	for (li = visible_parameter_automation.begin(); li != visible_parameter_automation.end(); ++li) {
		s.insert  (*li);
	}
}
AutomationList&
Redirect::automation_list (uint32_t parameter)
{
	AutomationList* al = parameter_automation[parameter];

	if (al == 0) {
		al = parameter_automation[parameter] = new AutomationList (default_parameter_value (parameter));
		/* let derived classes do whatever they need with this */
		automation_list_creation_callback (parameter, *al);
	}

	return *al;
}

string
Redirect::describe_parameter (uint32_t which)
{
	/* derived classes will override this */
	return "";
}

void
Redirect::can_automate (uint32_t what)
{
	can_automate_list.insert (what);
}

void
Redirect::mark_automation_visible (uint32_t what, bool yn)
{
	if (yn) {
		visible_parameter_automation.insert (what);
	} else {
		set<uint32_t>::iterator i;

		if ((i = visible_parameter_automation.find (what)) != visible_parameter_automation.end()) {
			visible_parameter_automation.erase (i);
		}
	}
}

bool
Redirect::find_next_event (nframes_t now, nframes_t end, ControlEvent& next_event) const
{
	map<uint32_t,AutomationList*>::const_iterator li;	
	AutomationList::TimeComparator cmp;

	next_event.when = max_frames;
	
  	for (li = parameter_automation.begin(); li != parameter_automation.end(); ++li) {
		
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

void
Redirect::set_active (bool yn, void* src)
{
	_active = yn; 
	active_changed (this, src); 
	_session.set_dirty ();
}

