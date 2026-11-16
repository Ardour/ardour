/*
 * Copyright (C) 2026 Paul Davis <paul@linuxaudiosystems.com>
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

#include "ardour/group_folder.h"
#include "ardour/session.h"

#include "pbd/i18n.h"

using namespace ARDOUR;

GroupFolder::GroupFolder (std::shared_ptr<RouteGroup> grp, Session & s, std::string const & name, PresentationInfo const & pi)
	: Folder (s, name, pi)
	, _group (grp)
{
	/* Having this enabled at present will cause a crash when we hide members */
	_group->set_select (false);

	StripableList sl = _session.get_stripables ();

	for (auto & s : sl) {
		if (s->route_group() == _group) {
			_contents.push_back (s);
		}
	}
}

std::shared_ptr<Stripable>
GroupFolder::get_master_stripable() const
{
	std::shared_ptr<Route> bus (_group->subgroup_bus().lock());
	if (bus) {
		return bus;
	}
	return nullptr;
}

int
GroupFolder::set_state (XMLNode const&, int)
{
	return 0;
}

XMLNode&
GroupFolder::get_state () const
{
	XMLNode* root = new XMLNode (X_("GroupFolder"));
	root->set_property (X_("type"), X_("group"));
	root->set_property (X_("master-name"), _group->name());
	root->set_property (X_("name"), name());
	return *root;
}

