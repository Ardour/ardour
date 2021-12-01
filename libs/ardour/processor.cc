/*
 * Copyright (C) 2007-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2008-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2016 Tim Mayberry <mojofunk@gmail.com>
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

#ifdef WAF_BUILD
#include "libardour-config.h"
#endif

#include <string>

#include "pbd/xml++.h"
#include "pbd/types_convert.h"

#include "ardour/automatable.h"
#include "ardour/chan_count.h"
#include "ardour/debug.h"
#include "ardour/processor.h"
#include "ardour/types.h"

#ifdef WINDOWS_VST_SUPPORT
#include "ardour/windows_vst_plugin.h"
#endif

#ifdef AUDIOUNIT_SUPPORT
#include "ardour/audio_unit.h"
#endif

#include "ardour/audioengine.h"
#include "ardour/session.h"
#include "ardour/types.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

namespace ARDOUR { class Session; }

// Always saved as Processor, but may be IOProcessor or Send in legacy sessions
const string Processor::state_node_name = "Processor";

Processor::Processor(Session& session, const string& name, Temporal::TimeDomain td)
	: SessionObject(session, name)
	, Automatable (session, td)
	, _pending_active(false)
	, _active(false)
	, _next_ab_is_active(false)
	, _configured(false)
	, _display_to_user (true)
	, _pre_fader (false)
	, _ui_pointer (0)
	, _window_proxy (0)
	, _pinmgr_proxy (0)
	, _owner (0)
	, _input_latency (0)
	, _output_latency (0)
	, _capture_offset (0)
	, _playback_offset (0)
	, _loop_location (0)
{
}

Processor::Processor (const Processor& other)
	: Evoral::ControlSet (other)
	, SessionObject (other.session(), other.name())
	, Automatable (other.session(), other.time_domain())
	, Latent (other)
	, _pending_active(other._pending_active)
	, _active(other._active)
	, _next_ab_is_active(false)
	, _configured(false)
	, _display_to_user (true)
	, _pre_fader (false)
	, _ui_pointer (0)
	, _window_proxy (0)
	, _pinmgr_proxy (0)
	, _owner (0)
	, _input_latency (0)
	, _output_latency (0)
	, _capture_offset (0)
	, _playback_offset (0)
	, _loop_location (other._loop_location)
{
}

Processor::~Processor ()
{
	DEBUG_TRACE (DEBUG::Destruction, string_compose ("processor %1 destructor\n", _name));
}

XMLNode&
Processor::get_state (void)
{
	return state ();
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
Processor::state ()
{
	XMLNode* node = new XMLNode (state_node_name);

	node->set_property("id", id());
	node->set_property("name", name());
	node->set_property("active", active());

	if (_extra_xml){
		node->add_child_copy (*_extra_xml);
	}

	if (!skip_saving_automation) {
		XMLNode& automation = Automatable::get_automation_xml_state();
		if (!automation.children().empty() || !automation.properties().empty()) {
			node->add_child_nocopy (automation);
		} else {
			delete &automation;
		}
	}

	Latent::add_state (node);

	return *node;
}

int
Processor::set_state_2X (const XMLNode & node, int /*version*/)
{
	XMLProperty const * prop;

	XMLNodeList children = node.children ();

	for (XMLNodeIterator i = children.begin(); i != children.end(); ++i) {

		if ((*i)->name() == X_("IO")) {

			if ((prop = (*i)->property ("name")) != 0) {
				set_name (prop->value ());
			}

			set_id (**i);

			//note:  in A2, active state was stored in the Redirect node, not the child IO node
			/*
			 * if ((prop = (*i)->property ("active")) != 0) {
				bool const a = string_is_affirmative (prop->value ());
				if (_active != a) {
					if (a) {
						activate ();
					} else {
						deactivate ();
					}
				}
			}*/

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

	bool ignore_name;
	// Only testing for the presence of the property not value
	if (!node.get_property("ignore-name", ignore_name)) {
		string name;
		// may not exist for legacy 3.0 sessions
		if (node.get_property ("name", name)) {
			/* don't let derived classes have a crack at set_name,
			   as some (like Send) will screw with the one we suggest.
			*/
			Processor::set_name (name);
		}

		set_id (node);
	}

	XMLNodeList nlist = node.children();
	XMLNodeIterator niter;

	Stateful::save_extra_xml (node);

	XMLProperty const * prop = 0;
	XMLProperty const * legacy_active = 0;

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {

		if ((*niter)->name() == X_("Automation")) {

			if ((prop = (*niter)->property ("path")) != 0) {
				old_set_automation_state (*(*niter));
			} else {
				set_automation_xml_state (*(*niter), Evoral::Parameter(PluginAutomation));
			}

		} else if ((*niter)->name() == "Redirect") {
			if ( !(legacy_active = (*niter)->property("active"))) {
				error << string_compose(_("No %1 property flag in element %2"), "active", (*niter)->name()) << endl;
			}
		}
	}

	if ((prop = node.property ("active")) == 0) {
		if (legacy_active) {
			prop = legacy_active;
		} else {
			error << _("No child node with active property") << endmsg;
			return -1;
		}
	}

	bool const a = string_to<bool> (prop->value ()) && !_session.get_bypass_all_loaded_plugins();
	if (_active != a) {
		if (a) {
			activate ();
		} else {
			deactivate ();
		}
	}

	Latent::set_state (node, version);

	return 0;
}

/** @pre Caller must hold process lock */
bool
Processor::configure_io (ChanCount in, ChanCount out)
{
	/* This class assumes 1:1 input:output.static output stream count.
	   Derived classes must override and set _configured_output appropriately
	   if this is not the case
	*/
	bool changed = _configured_input != in || _configured_output != out;

	_configured_input = in;
	_configured_output = out;
	_configured = true;

	if (changed) {
		ConfigurationChanged (in, out); /* EMIT SIGNAL */
	}

	return true;
}

bool
Processor::map_loop_range (samplepos_t& start, samplepos_t& end) const
{
	if (!_loop_location) {
		return false;
	}
	if (start >= end) {
		/* no backwards looping */
		return false;
	}

	const samplepos_t loop_end = _loop_location->end().samples ();
	if (start < loop_end) {
		return false;
	}

	const samplepos_t loop_start   = _loop_location->start().samples ();
	const samplecnt_t looplen      = loop_end - loop_start;
	const sampleoffset_t start_off = (start - loop_start) % looplen;
	const samplepos_t start_pos    = loop_start + start_off;

	assert (start >= start_pos);
	end -= start - start_pos;
	start = start_pos;
	assert (end > start);
	return true;
}

void
Processor::set_display_to_user (bool yn)
{
	_display_to_user = yn;
}

void
Processor::set_pre_fader (bool p)
{
	_pre_fader = p;
}

void
Processor::set_owner (SessionObject* o)
{
	_owner = o;
}

SessionObject*
Processor::owner() const
{
	return _owner;
}
