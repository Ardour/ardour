/*
 * Copyright (C) 2023 Robin Gareus <robin@gareus.org>
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

#include "ardour/automation_list.h"
#include "ardour/surround_pannable.h"
#include "ardour/session.h"
#include "ardour/value_as_string.h"

#include "pbd/i18n.h"

using namespace std;
using namespace PBD;
using namespace ARDOUR;

SurroundControllable::SurroundControllable (Session& s, Evoral::Parameter param, Temporal::TimeDomainProvider const& tdp)
	: AutomationControl (s,
	                     param,
	                     ParameterDescriptor(param),
	                     std::shared_ptr<AutomationList>(new AutomationList(param, tdp)))
{
}

std::string
SurroundControllable::get_user_string () const
{
	float v = get_value ();
	char buf[32];
	switch (parameter ().type ()) {
		case PanSurroundX:
			if (v == 0.5) {
				return _("Center");
			}
			snprintf(buf, sizeof(buf), "L%3d R%3d", (int)rint (100.0 * (1.0 - v)), (int)rint (100.0 * v));
			break;
		case PanSurroundY:
			snprintf(buf, sizeof(buf), "F%3d B%3d", (int)rint (100.0 * (1.0 - v)), (int)rint (100.0 * v));
			break;
		case PanSurroundSize:
			snprintf(buf, sizeof(buf), "%.0f%%", 100.f * v);
			break;
		default:
			return value_as_string (desc(), v);
	}
	return buf;
}

SurroundPannable::SurroundPannable (Session& s, uint32_t chn, Temporal::TimeDomainProvider const & tdp)
	: Automatable (s, tdp)
	, SessionHandleRef (s)
	, pan_pos_x (new SurroundControllable (s, Evoral::Parameter (PanSurroundX, 0, chn), tdp))
	, pan_pos_y (new SurroundControllable (s, Evoral::Parameter (PanSurroundY, 0, chn), tdp))
	, pan_pos_z (new SurroundControllable (s, Evoral::Parameter (PanSurroundZ, 0, chn), tdp))
	, pan_size (new SurroundControllable  (s, Evoral::Parameter (PanSurroundSize, 0, chn), tdp))
	, pan_snap (new SurroundControllable  (s, Evoral::Parameter (PanSurroundSnap, 0, chn), tdp))
	, binaural_render_mode (new SurroundControllable (s, Evoral::Parameter (BinauralRenderMode, 0, chn), tdp))
	, sur_elevation_enable (new SurroundControllable (s, Evoral::Parameter (PanSurroundElevationEnable, 0, chn), tdp))
	, sur_zones (new SurroundControllable (s, Evoral::Parameter (PanSurroundZones, 0, chn), tdp))
	, sur_ramp (new SurroundControllable (s, Evoral::Parameter (PanSurroundRamp, 0, chn), tdp))
	, _auto_state (Off)
	, _responding_to_control_auto_state_change (0)
{
	binaural_render_mode->set_flag (Controllable::NotAutomatable);

	add_control (pan_pos_x);
	add_control (pan_pos_y);
	add_control (pan_pos_z);
	add_control (pan_size);
	add_control (pan_snap);
	add_control (binaural_render_mode); // not automatable
	add_control (sur_elevation_enable); // hidden, volatile
	add_control (sur_zones); // hidden, volatile
	add_control (sur_ramp); // hidden, volatile

	/* all controls change state together */
	pan_pos_x->alist()->automation_state_changed.connect_same_thread (*this, boost::bind (&SurroundPannable::control_auto_state_changed, this, _1));
	pan_pos_y->alist()->automation_state_changed.connect_same_thread (*this, boost::bind (&SurroundPannable::control_auto_state_changed, this, _1));
	pan_pos_z->alist()->automation_state_changed.connect_same_thread (*this, boost::bind (&SurroundPannable::control_auto_state_changed, this, _1));
	pan_size->alist()->automation_state_changed.connect_same_thread (*this, boost::bind (&SurroundPannable::control_auto_state_changed, this, _1));
	pan_snap->alist()->automation_state_changed.connect_same_thread (*this, boost::bind (&SurroundPannable::control_auto_state_changed, this, _1));

	pan_pos_x->Changed.connect_same_thread (*this, boost::bind (&SurroundPannable::value_changed, this));
	pan_pos_y->Changed.connect_same_thread (*this, boost::bind (&SurroundPannable::value_changed, this));
	pan_pos_z->Changed.connect_same_thread (*this, boost::bind (&SurroundPannable::value_changed, this));
	pan_size->Changed.connect_same_thread (*this, boost::bind (&SurroundPannable::value_changed, this));
	pan_snap->Changed.connect_same_thread (*this, boost::bind (&SurroundPannable::value_changed, this));

	setup_visual_links ();
}

SurroundPannable::~SurroundPannable ()
{
}

void
SurroundPannable::setup_visual_links ()
{
	/* all controls are visible together */
	pan_pos_x->add_visually_linked_control (pan_pos_y);
	pan_pos_x->add_visually_linked_control (pan_pos_z);
	pan_pos_y->add_visually_linked_control (pan_pos_x);
	pan_pos_y->add_visually_linked_control (pan_pos_z);
	pan_pos_z->add_visually_linked_control (pan_pos_x);
	pan_pos_z->add_visually_linked_control (pan_pos_y);
}

void
SurroundPannable::sync_visual_link_to (std::shared_ptr<SurroundPannable> other)
{
	pan_pos_x->add_visually_linked_control (other->pan_pos_x);
	pan_pos_x->add_visually_linked_control (other->pan_pos_y);
	pan_pos_x->add_visually_linked_control (other->pan_pos_z);

	pan_pos_y->add_visually_linked_control (other->pan_pos_x);
	pan_pos_y->add_visually_linked_control (other->pan_pos_y);
	pan_pos_y->add_visually_linked_control (other->pan_pos_z);

	pan_pos_z->add_visually_linked_control (other->pan_pos_x);
	pan_pos_z->add_visually_linked_control (other->pan_pos_y);
	pan_pos_z->add_visually_linked_control (other->pan_pos_z);
}

void
SurroundPannable::sync_auto_state_with (std::shared_ptr<SurroundPannable> other)
{
	other->pan_pos_x->alist()->automation_state_changed.connect_same_thread (*this, boost::bind (&SurroundPannable::control_auto_state_changed, this, _1));
}

void
SurroundPannable::foreach_pan_control (boost::function<void(std::shared_ptr<AutomationControl>)> f) const
{
	f (pan_pos_x);
	f (pan_pos_y);
	f (pan_pos_z);
	f (pan_size);
	f (pan_snap);
	f (sur_elevation_enable);
	f (sur_zones);
	f (sur_ramp);
}

void
SurroundPannable::control_auto_state_changed (AutoState new_state)
{
	if (_responding_to_control_auto_state_change || _auto_state == new_state) {
		return;
	}

	_responding_to_control_auto_state_change++;

	foreach_pan_control ([new_state](std::shared_ptr<AutomationControl> ac) {
			ac->set_automation_state (new_state);
			});

	_responding_to_control_auto_state_change--;

	_auto_state = new_state;
	automation_state_changed (new_state);  /* EMIT SIGNAL */
}

void
SurroundPannable::value_changed ()
{
	_session.set_dirty ();
}

void
SurroundPannable::set_automation_state (AutoState state)
{
	if (state == _auto_state) {
		return;
	}
	_auto_state = state;

	const Controls& c (controls());

	for (Controls::const_iterator ci = c.begin(); ci != c.end(); ++ci) {
		std::shared_ptr<AutomationControl> ac = std::dynamic_pointer_cast<AutomationControl>(ci->second);
		if (ac) {
			ac->alist()->set_automation_state (state);
		}
	}

	_session.set_dirty ();
	automation_state_changed (_auto_state); /* EMIT SIGNAL */
}

bool
SurroundPannable::touching () const
{
	const Controls& c (controls());

	for (auto const& i : c) {
		std::shared_ptr<AutomationControl> ac = std::dynamic_pointer_cast<AutomationControl>(i.second);
		if (ac && ac->touching ()) {
			return true;
		}
	}
	return false;
}

XMLNode&
SurroundPannable::get_state () const
{
	return state ();
}

XMLNode&
SurroundPannable::state () const
{
	XMLNode* node = new XMLNode (X_("SurroundPannable"));
	node->set_property ("channel", pan_pos_x->parameter ().id ());

	node->add_child_nocopy (pan_pos_x->get_state());
	node->add_child_nocopy (pan_pos_y->get_state());
	node->add_child_nocopy (pan_pos_z->get_state());
	node->add_child_nocopy (pan_size->get_state());
	node->add_child_nocopy (pan_snap->get_state());
	node->add_child_nocopy (binaural_render_mode->get_state());

	return *node;
}

int
SurroundPannable::set_state (const XMLNode& root, int version)
{
	if (root.name() != X_("SurroundPannable")) {
		return -1;
	}

	const XMLNodeList& nlist (root.children());
	XMLNodeConstIterator niter;

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		if ((*niter)->name() != Controllable::xml_node_name) {
			continue;
		}
		std::string control_name;

		if (!(*niter)->get_property (X_("name"), control_name)) {
			continue;
		}

		if (control_name == pan_pos_x->name()) {
			pan_pos_x->set_state (**niter, version);
		} else if (control_name == pan_pos_y->name()) {
			pan_pos_y->set_state (**niter, version);
		} else if (control_name == pan_pos_z->name()) {
			pan_pos_z->set_state (**niter, version);
		} else if (control_name == pan_size->name()) {
			pan_size->set_state (**niter, version);
		} else if (control_name == pan_snap->name()) {
			pan_snap->set_state (**niter, version);
		} else if (control_name == binaural_render_mode->name()) {
			binaural_render_mode->set_state (**niter, version);
		}
	}

	return 0;
}
