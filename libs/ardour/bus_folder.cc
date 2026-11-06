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

#include "ardour/bus_folder.h"

#include "pbd/i18n.h"

using namespace ARDOUR;

BusFolder::BusFolder (std::shared_ptr<Route> bus, Session & s, std::string const & name, PresentationInfo const & pi)
	: Folder (s, name, pi)
	, _bus (bus)
{
}

int
BusFolder::set_state (XMLNode const&, int)
{
	return 0;
}

XMLNode&
BusFolder::get_state () const
{
	XMLNode* root = new XMLNode (X_("BusFolder"));
	return *root;
}
