/*
 * Copyright (C) 2011-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2015-2017 Robin Gareus <robin@gareus.org>
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

#include "pbd/error.h"
#include "pbd/compose.h"

#include "ardour/boost_debug.h"
#include "ardour/debug.h"
#include "ardour/automation_control.h"
#include "ardour/automation_list.h"
#include "ardour/pannable.h"
#include "ardour/panner.h"
#include "ardour/pan_controllable.h"
#include "ardour/session.h"
#include "ardour/value_as_string.h"

#include "pbd/i18n.h"

using namespace std;
using namespace PBD;
using namespace ARDOUR;

Pannable::Pannable (Session& s, Temporal::TimeDomain td)
	: Automatable (s, td)
	, SessionHandleRef (s)
	, pan_azimuth_control (new PanControllable (s, "", this, PanAzimuthAutomation))
	, pan_elevation_control (new PanControllable (s, "", this, PanElevationAutomation))
	, pan_width_control (new PanControllable (s, "", this, PanWidthAutomation))
	, pan_frontback_control (new PanControllable (s, "", this, PanFrontBackAutomation))
	, pan_lfe_control (new PanControllable (s, "", this, PanLFEAutomation))
	, _auto_state (Off)
	, _has_state (false)
	, _responding_to_control_auto_state_change (0)
{
	//boost_debug_shared_ptr_mark_interesting (this, "pannable");

	g_atomic_int_set (&_touching, 0);

	add_control (pan_azimuth_control);
	add_control (pan_elevation_control);
	add_control (pan_width_control);
	add_control (pan_frontback_control);
	add_control (pan_lfe_control);

	/* all controls change state together */

	pan_azimuth_control->alist()->automation_state_changed.connect_same_thread (*this, boost::bind (&Pannable::control_auto_state_changed, this, _1));
	pan_elevation_control->alist()->automation_state_changed.connect_same_thread (*this, boost::bind (&Pannable::control_auto_state_changed, this, _1));
	pan_width_control->alist()->automation_state_changed.connect_same_thread (*this, boost::bind (&Pannable::control_auto_state_changed, this, _1));
	pan_frontback_control->alist()->automation_state_changed.connect_same_thread (*this, boost::bind (&Pannable::control_auto_state_changed, this, _1));
	pan_lfe_control->alist()->automation_state_changed.connect_same_thread (*this, boost::bind (&Pannable::control_auto_state_changed, this, _1));

	pan_azimuth_control->Changed.connect_same_thread (*this, boost::bind (&Pannable::value_changed, this));
	pan_elevation_control->Changed.connect_same_thread (*this, boost::bind (&Pannable::value_changed, this));
	pan_width_control->Changed.connect_same_thread (*this, boost::bind (&Pannable::value_changed, this));
	pan_frontback_control->Changed.connect_same_thread (*this, boost::bind (&Pannable::value_changed, this));
	pan_lfe_control->Changed.connect_same_thread (*this, boost::bind (&Pannable::value_changed, this));
}

Pannable::~Pannable ()
{
	DEBUG_TRACE (DEBUG::Destruction, string_compose ("pannable @ %1 destructor\n", this));
}

void
Pannable::control_auto_state_changed (AutoState new_state)
{
	if (_responding_to_control_auto_state_change) {
		return;
	}

	_responding_to_control_auto_state_change++;

	pan_azimuth_control->set_automation_state (new_state);
	pan_width_control->set_automation_state (new_state);
	pan_elevation_control->set_automation_state (new_state);
	pan_frontback_control->set_automation_state (new_state);
	pan_lfe_control->set_automation_state (new_state);

	_responding_to_control_auto_state_change--;

	_auto_state = new_state;
	automation_state_changed (new_state);  /* EMIT SIGNAL */
}

void
Pannable::set_panner (boost::shared_ptr<Panner> p)
{
	_panner = p;
}

const std::set<Evoral::Parameter>&
Pannable::what_can_be_automated() const
{
	boost::shared_ptr<Panner> const panner = _panner.lock();
	if (panner) {
		return panner->what_can_be_automated ();
	}
	return Automatable::what_can_be_automated ();
}

void
Pannable::value_changed ()
{
	if (!has_state ()) {
		// prevent the GUI from resetting panners
		// e.g. when switching to aux-sends and back.
		_has_state = true;
	}

	_session.set_dirty ();
}

void
Pannable::set_automation_state (AutoState state)
{
	if (state != _auto_state) {
		_auto_state = state;

		const Controls& c (controls());

		for (Controls::const_iterator ci = c.begin(); ci != c.end(); ++ci) {
			boost::shared_ptr<AutomationControl> ac = boost::dynamic_pointer_cast<AutomationControl>(ci->second);
			if (ac) {
				ac->alist()->set_automation_state (state);
			}
		}

		_session.set_dirty ();
		automation_state_changed (_auto_state);
	}
}

void
Pannable::start_touch (timepos_t const & when)
{
	const Controls& c (controls());

	for (Controls::const_iterator ci = c.begin(); ci != c.end(); ++ci) {
		boost::shared_ptr<AutomationControl> ac = boost::dynamic_pointer_cast<AutomationControl>(ci->second);
		if (ac) {
			ac->alist()->start_touch (when);
		}
	}
	g_atomic_int_set (&_touching, 1);
}

void
Pannable::stop_touch (timepos_t const & when)
{
	const Controls& c (controls());

	for (Controls::const_iterator ci = c.begin(); ci != c.end(); ++ci) {
		boost::shared_ptr<AutomationControl> ac = boost::dynamic_pointer_cast<AutomationControl>(ci->second);
		if (ac) {
			ac->alist()->stop_touch (when);
		}
	}
	g_atomic_int_set (&_touching, 0);
}

XMLNode&
Pannable::get_state ()
{
	return state ();
}

XMLNode&
Pannable::state ()
{
	XMLNode* node = new XMLNode (X_("Pannable"));

	node->add_child_nocopy (pan_azimuth_control->get_state());
	node->add_child_nocopy (pan_width_control->get_state());
	node->add_child_nocopy (pan_elevation_control->get_state());
	node->add_child_nocopy (pan_frontback_control->get_state());
	node->add_child_nocopy (pan_lfe_control->get_state());

	node->add_child_nocopy (get_automation_xml_state ());

	return *node;
}

int
Pannable::set_state (const XMLNode& root, int version)
{
	if (root.name() != X_("Pannable")) {
		warning << string_compose (_("Pannable given XML data for %1 - ignored"), root.name()) << endmsg;
		return -1;
	}

	const XMLNodeList& nlist (root.children());
	XMLNodeConstIterator niter;

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		if ((*niter)->name() == Controllable::xml_node_name) {
			std::string control_name;

			if (!(*niter)->get_property (X_("name"), control_name)) {
				continue;
			}

			if (control_name == pan_azimuth_control->name()) {
				pan_azimuth_control->set_state (**niter, version);
			} else if (control_name == pan_width_control->name()) {
				pan_width_control->set_state (**niter, version);
			} else if (control_name == pan_elevation_control->name()) {
				pan_elevation_control->set_state (**niter, version);
			} else if (control_name == pan_frontback_control->name()) {
				pan_frontback_control->set_state (**niter, version);
			} else if (control_name == pan_lfe_control->name()) {
				pan_lfe_control->set_state (**niter, version);
			}

		} else if ((*niter)->name() == Automatable::xml_node_name) {
			set_automation_xml_state (**niter, PanAzimuthAutomation);

		} else {
			/* old school (alpha1-6) XML info */

			float val;
			if ((*niter)->name() == X_("azimuth")) {
				if ((*niter)->get_property (X_("value"), val)) {
					pan_azimuth_control->set_value (val, Controllable::NoGroup);
				}
			} else if ((*niter)->name() == X_("width")) {
				if ((*niter)->get_property (X_("value"), val)) {
					pan_width_control->set_value (val, Controllable::NoGroup);
				}
			} else if ((*niter)->name() == X_("elevation")) {
				if ((*niter)->get_property (X_("value"), val)) {
					pan_elevation_control->set_value (val, Controllable::NoGroup);
				}
			} else if ((*niter)->name() == X_("frontback")) {
				if ((*niter)->get_property (X_("value"), val)) {
					pan_frontback_control->set_value (val, Controllable::NoGroup);
				}
			} else if ((*niter)->name() == X_("lfe")) {
				if ((*niter)->get_property (X_("value"), val)) {
					pan_lfe_control->set_value (val, Controllable::NoGroup);
				}
			}
		}
	}

	_has_state = true;

	return 0;
}
