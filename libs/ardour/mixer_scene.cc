/*
 * Copyright (C) 2022 Robin Gareus <robin@gareus.org>
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

#include "pbd/controllable.h"
#include "pbd/types_convert.h"

#include "ardour/automation_control.h"
#include "ardour/mixer_scene.h"
#include "ardour/slavable_automation_control.h"
#include "ardour/session.h"

#include "pbd/i18n.h"

using namespace PBD;
using namespace ARDOUR;

PBD::Signal0<void> MixerScene::Change;

MixerScene::MixerScene (Session& s)
	: SessionHandleRef (s)
{
	Change (); /* EMIT SIGNAL */
}

MixerScene::~MixerScene ()
{
	Change (); /* EMIT SIGNAL */
}

bool
MixerScene::set_name (std::string const& name)
{
	if (_name != name) {
		_name = name;
		_session.set_dirty ();
		Change (); /* EMIT SIGNAL */
	}
	return true;
}

void
MixerScene::clear ()
{
	_ctrl_map.clear ();
	_name.clear ();
	Change (); /* EMIT SIGNAL */
}

void
MixerScene::snapshot ()
{
	_ctrl_map.clear ();
	for (auto const& c : Controllable::registered_controllables ()) {
		if (!boost::dynamic_pointer_cast<AutomationControl> (c)) {
			continue;
		}
		if (c->flags () & Controllable::HiddenControl) {
			continue;
		}
		_ctrl_map[c->id ()] = c->get_save_value ();
	}
	_session.set_dirty ();
	Change (); /* EMIT SIGNAL */
}

bool
MixerScene::recurse_to_master (boost::shared_ptr<PBD::Controllable> c, std::set <PBD::ID>& done) const
{
	if (done.find (c->id()) != done.end ()) {
		return false;
	}

#if 1 /* ignore controls in Write, or Touch + touching() state */
	auto ac = boost::dynamic_pointer_cast<AutomationControl> (c);
	if (ac && ac->automation_write ()) {
		done.insert (c->id ());
		return false;
	}
#endif

	auto sc = boost::dynamic_pointer_cast<SlavableAutomationControl> (c);
	if (sc && sc->slaved ()) {
		/* first set masters, then set own value */
		for (auto const& m : sc->masters ()) {
			recurse_to_master (m, done);
		}
	}

	ControllableValueMap::const_iterator it = _ctrl_map.find (c->id ());
	if (it == _ctrl_map.end ()) {
		done.insert (c->id ());
		return false;
	}

	if (sc && sc->slaved ()) {
		double x = sc->reduce_by_masters (1.0);
		if (x <= 0) {
			c->set_value (0, Controllable::NoGroup);
		} else {
			c->set_value (it->second / x, Controllable::NoGroup);
		}
	} else {
		c->set_value (it->second, Controllable::NoGroup);
	}

	done.insert (it->first);
	return true;
}

bool
MixerScene::apply () const
{
	bool rv = false;
	std::set<PBD::ID> done;

	for (auto const& c : Controllable::registered_controllables ()) {
		rv |= recurse_to_master (c, done);
	}

	return rv;
}

XMLNode&
MixerScene::get_state () const
{
	XMLNode* root = new XMLNode ("MixerScene");
	root->set_property ("id", id ());
	root->set_property ("name", name ());

	for (auto const& c : _ctrl_map) {
		XMLNode* node = new XMLNode ("ControlValue");
		node->set_property (X_("id"), c.first);
		node->set_property (X_("value"), c.second);
		root->add_child_nocopy (*node);
	}
	return *root;
}

int
MixerScene::set_state (XMLNode const& node, int /* version */)
{
	_ctrl_map.clear ();

	std::string name;
	if (node.get_property ("name", name)) {
		set_name (name);
	}

	for (auto const& n : node.children ()) {
		if (n->name() != "ControlValue") {
			continue;
		}
		PBD::ID id;
		double  value;
		if (!n->get_property (X_("id"), id)) {
			continue;
		}
		if (!n->get_property (X_("value"), value)) {
			continue;
		}
		_ctrl_map[id] = value;
	}
	return 0;
}
