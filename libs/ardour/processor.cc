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

#ifdef WAF_BUILD
#include "libardour-config.h"
#endif

#include <string>

#include <sigc++/bind.h>

#include "pbd/failed_constructor.h"
#include "pbd/enumwriter.h"
#include "pbd/xml++.h"

#include "ardour/processor.h"
#include "ardour/plugin.h"
#include "ardour/port.h"
#include "ardour/route.h"
#include "ardour/ladspa_plugin.h"
#include "ardour/buffer_set.h"
#include "ardour/send.h"
#include "ardour/port_insert.h"
#include "ardour/plugin_insert.h"

#ifdef VST_SUPPORT
#include "ardour/vst_plugin.h"
#endif

#ifdef HAVE_AUDIOUNITS
#include "ardour/audio_unit.h"
#endif

#include "ardour/audioengine.h"
#include "ardour/session.h"
#include "ardour/types.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

sigc::signal<void,Processor*> Processor::ProcessorCreated;

// Always saved as Processor, but may be IOProcessor or Send in legacy sessions
const string Processor::state_node_name = "Processor";

Processor::Processor(Session& session, const string& name)
	: SessionObject(session, name)
	, AutomatableControls(session)
	, _pending_active(false)
	, _active(false)
	, _next_ab_is_active(false)
	, _configured(false)
	, _gui(0)
{
}

Processor::Processor (Session& session, const XMLNode& node)
	: SessionObject(session, "renameMe")
	, AutomatableControls(session)
	, _pending_active(false)
	, _active(false)
	, _next_ab_is_active(false)
	, _configured(false)
	, _gui(0)
{
	set_state (node);
	_pending_active = _active;
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
	char buf[64];

	id().print (buf, sizeof (buf));
	node->add_property("id", buf);
	node->add_property("name", _name);
	node->add_property("active", active() ? "yes" : "no");

	if (_extra_xml){
		node->add_child_copy (*_extra_xml);
	}

	if (full_state) {
		XMLNode& automation = Automatable::get_automation_state();
		if (!automation.children().empty()
				|| !automation.properties().empty()
				|| !_visible_controls.empty()) {

			for (set<Evoral::Parameter>::iterator x = _visible_controls.begin();
					x != _visible_controls.end(); ++x) {
				if (x != _visible_controls.begin()) {
					sstr << ' ';
				}
				sstr << *x;
			}

			automation.add_property ("visible", sstr.str());
			node->add_child_nocopy (automation);
		}
	}

	return *node;
}

int
Processor::set_state_2X (const XMLNode & node, int version)
{
	XMLProperty const * prop;

	XMLNodeList children = node.children ();

	for (XMLNodeIterator i = children.begin(); i != children.end(); ++i) {

		if ((*i)->name() == X_("IO")) {

			if ((prop = (*i)->property ("name")) != 0) {
				set_name (prop->value ());
			}

			if ((prop = (*i)->property ("id")) != 0) {
				_id = prop->value ();
			}

			if ((prop = (*i)->property ("active")) != 0) {
				if (_active != string_is_affirmative (prop->value())) {
					_active = !_active;
					ActiveChanged (); /* EMIT_SIGNAL */
				}
			}
		}
	}

	return 0;
}

int
Processor::set_state (const XMLNode& node, int version)
{
	if (version < 3000) {
		return set_state_2X (node, version);
	}
	
	const XMLProperty *prop;
	const XMLProperty *legacy_active = 0;

	// may not exist for legacy 3.0 sessions
	if ((prop = node.property ("name")) != 0) {
		set_name(prop->value());
	}

	// may not exist for legacy 3.0 sessions
	if ((prop = node.property ("id")) != 0) {
		_id = prop->value();
	}

	XMLNodeList nlist = node.children();
	XMLNodeIterator niter;

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {

		if ((*niter)->name() == X_("Automation")) {

			XMLProperty *prop;

			if ((prop = (*niter)->property ("path")) != 0) {
				old_set_automation_state (*(*niter));
			} else {
				set_automation_state (*(*niter), Evoral::Parameter(PluginAutomation));
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
					mark_automation_visible (Evoral::Parameter(PluginAutomation, 0, what), true);
				}
			}

		} else if ((*niter)->name() == "Extra") {
			_extra_xml = new XMLNode (*(*niter));
		} else if ((*niter)->name() == "Redirect") {
			if ( !(legacy_active = (*niter)->property("active"))) {
				error << string_compose(_("No %1 property flag in element %2"), "active", (*niter)->name()) << endl;
			}
		}
	}

	if ((prop = node.property ("active")) == 0) {
		warning << _("XML node describing a processor is missing the `active' field,"
			   "trying legacy active flag from child node") << endmsg;
		if (legacy_active) {
			prop = legacy_active;
		} else {
			error << _("No child node with active property") << endmsg;
			return -1;
		}
	}

	if (_active != string_is_affirmative (prop->value())) {
		_active = !_active;
		ActiveChanged (); /* EMIT_SIGNAL */
	}

	return 0;
}

bool
Processor::configure_io (ChanCount in, ChanCount out)
{
	/* This class assumes 1:1 input:output.static output stream count.
	   Derived classes must override and set _configured_output appropriately
	   if this is not the case
	*/

	_configured_input = in;
	_configured_output = out;
	_configured = true;

	ConfigurationChanged.emit (in, out);

	return true;
}
