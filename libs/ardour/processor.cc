/*
    Copyright (C) 2000 Paul Davis 

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

#include <string>

#include <sigc++/bind.h>

#include <pbd/failed_constructor.h>
#include <pbd/enumwriter.h>
#include <pbd/xml++.h>

#include <ardour/processor.h>
#include <ardour/plugin.h>
#include <ardour/port.h>
#include <ardour/route.h>
#include <ardour/ladspa_plugin.h>
#include <ardour/buffer_set.h>
#include <ardour/send.h>
#include <ardour/port_insert.h>
#include <ardour/plugin_insert.h>

#ifdef VST_SUPPORT
#include <ardour/vst_plugin.h>
#endif

#ifdef HAVE_AUDIOUNITS
#include <ardour/audio_unit.h>
#endif

#include <ardour/audioengine.h>
#include <ardour/session.h>
#include <ardour/types.h>

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

sigc::signal<void,Processor*> Processor::ProcessorCreated;

// Always saved as Processor, but may be IOProcessor or Send in legacy sessions
const string Processor::state_node_name = "Processor";

Processor::Processor(Session& session, const string& name, Placement p)
	: Automatable(session, name)
	, _active(false)
	, _next_ab_is_active(false)
	, _configured(false)
	, _placement(p)
	, _gui(0)
{
}

boost::shared_ptr<Processor>
Processor::clone (boost::shared_ptr<const Processor> other)
{
	boost::shared_ptr<const Send> send;
	boost::shared_ptr<const PortInsert> port_insert;
	boost::shared_ptr<const PluginInsert> plugin_insert;

	if ((send = boost::dynamic_pointer_cast<const Send>(other)) != 0) {
		return boost::shared_ptr<Processor> (new Send (*send));
	} else if ((port_insert = boost::dynamic_pointer_cast<const PortInsert>(other)) != 0) {
		return boost::shared_ptr<Processor> (new PortInsert (*port_insert));
	} else if ((plugin_insert = boost::dynamic_pointer_cast<const PluginInsert>(other)) != 0) {
		return boost::shared_ptr<Processor> (new PluginInsert (*plugin_insert));
	} else {
		fatal << _("programming error: unknown Processor type in Processor::Clone!\n")
		      << endmsg;
		/*NOTREACHED*/
	}
	return boost::shared_ptr<Processor>();
}

void
Processor::set_sort_key (uint32_t key)
{
	_sort_key = key;
}
	
void
Processor::set_placement (Placement p)
{
	if (_placement != p) {
		_placement = p;
		 PlacementChanged (); /* EMIT SIGNAL */
	}
}

void
Processor::set_active (bool yn)
{
	_active = yn; 
	ActiveChanged (); 
}

XMLNode&
Processor::get_state (void)
{
	return state (true);
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

XMLNode&
Processor::state (bool full_state)
{
	XMLNode* node = new XMLNode (state_node_name);
	stringstream sstr;
	
	// FIXME: This conflicts with "id" used by plugin for name in legacy sessions (ugh).
	// Do we need to serialize this?
	/*
	char buf[64];
	id().print (buf, sizeof (buf));
	node->add_property("id", buf);
	*/

	node->add_property("name", _name);
	node->add_property("active", active() ? "yes" : "no");	
	node->add_property("placement", enum_2_string (_placement));

	if (_extra_xml){
		node->add_child_copy (*_extra_xml);
	}
	
	if (full_state) {

		XMLNode& automation = Automatable::get_automation_state(); 
		
		for (set<Parameter>::iterator x = _visible_controls.begin(); x != _visible_controls.end(); ++x) {
			if (x != _visible_controls.begin()) {
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
Processor::set_state (const XMLNode& node)
{
	const XMLProperty *prop;
	const XMLProperty *legacy_active = 0;
	const XMLProperty *legacy_placement = 0;

	// may not exist for legacy sessions
	if ((prop = node.property ("name")) != 0) {
		set_name(prop->value());
	}

	XMLNodeList nlist = node.children();
	XMLNodeIterator niter;

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {

		if ((*niter)->name() == X_("Automation")) {


			XMLProperty *prop;
			
			if ((prop = (*niter)->property ("path")) != 0) {
				old_set_automation_state (*(*niter));
			} else {
				set_automation_state (*(*niter), Parameter(PluginAutomation));
			}

			if ((prop = (*niter)->property ("visible")) != 0) {
				uint32_t what;
				stringstream sstr;

				_visible_controls.clear ();
				
				sstr << prop->value();
				while (1) {
					sstr >> what;
					if (sstr.fail()) {
						break;
					}
					// FIXME: other automation types?
					mark_automation_visible (Parameter(PluginAutomation, what), true);
				}
			}

		} else if ((*niter)->name() == "extra") {
			_extra_xml = new XMLNode (*(*niter));
		} else if ((*niter)->name() == "Redirect") {
			if ( !(legacy_active = (*niter)->property("active"))) {
				error << string_compose(_("No %1 property flag in element %2"), "active", (*niter)->name()) << endl;
			}
			if ( !(legacy_placement = (*niter)->property("placement"))) {
				error << string_compose(_("No %1 property flag in element %2"), "placement", (*niter)->name()) << endl;
			}
		}
	}

	if ((prop = node.property ("active")) == 0) {
		warning << _("XML node describing a processor is missing the `active' field, trying legacy active flag from child node") << endmsg;
		if (legacy_active) {
			prop = legacy_active;
		} else {
			error << _("No child node with active property") << endmsg;
			return -1;
		}
	}

	if (_active != (prop->value() == "yes")) {
		_active = !_active;
		ActiveChanged (); /* EMIT_SIGNAL */
	}	

	if ((prop = node.property ("placement")) == 0) {
		warning << _("XML node describing a processor is missing the `placement' field, trying legacy placement flag from child node") << endmsg;
		if (legacy_placement) {
			prop = legacy_placement; 
		} else {
			error << _("No child node with placement property") << endmsg;
			return -1;
		}
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
	set_placement (p);

	return 0;
}

bool
Processor::configure_io (ChanCount in, ChanCount out)
{
	/* this class assumes static output stream count.
	   Derived classes must override, and must set "out"
	   to reflect "in" before calling this.
	*/

	_configured_input = in; 
	_configured = true;

	return true;
}
