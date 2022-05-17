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

#include "pbd/types_convert.h"

#include "ardour/mixer_scene.h"
#include "ardour/session.h"

#include "pbd/i18n.h"

using namespace PBD;
using namespace ARDOUR;

MixerScene::MixerScene(Session& s)
	: SessionHandleRef (s)
{
}

bool
MixerScene::set_name (std::string const& name)
{
	if (_name != name) {
		_name = name;
		_session.set_dirty ();
	}
	return true;
}

void
MixerScene::clear ()
{
	_ctrl_map.clear ();
	_name.clear ();
}

void
MixerScene::snapshot ()
{
	_ctrl_map.clear ();
	for (auto const& c : Controllable::registered_controllables ()) {
		if (!boost::dynamic_pointer_cast<AutomationControl> (c)) {
			continue;
		}
		_ctrl_map[c->id ()] = c->get_save_value ();
	}
	_session.set_dirty ();
}

bool
MixerScene::apply () const
{
	bool rv = false;
	// TODO special-case solo-iso, and solo (restore order, or ignore)
	for (auto const& c : Controllable::registered_controllables ()) {
		ControllableValueMap::const_iterator it = _ctrl_map.find (c->id ());
		if (it == _ctrl_map.end ()) {
			continue;
		}
		rv = true;
		c->set_value (it->second, Controllable::NoGroup);
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
