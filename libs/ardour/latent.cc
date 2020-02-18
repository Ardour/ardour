/*
 * Copyright (C) 2019 Robin Gareus <robin@gareus.org>
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

#include "pbd/xml++.h"

#include "ardour/latent.h"

using namespace ARDOUR;

bool ARDOUR::Latent::_zero_latency = false;
PBD::Signal0<void> Latent::DisableSwitchChanged;

Latent::Latent ()
	: HasLatency ()
	, _use_user_latency (false)
	, _user_latency (0)
{}

Latent::Latent (Latent const& other)
	: HasLatency ()
	, _use_user_latency (other._use_user_latency)
	, _user_latency (other._user_latency)
{}


int
Latent::set_state (const XMLNode& node, int version)
{
	node.get_property ("user-latency", _user_latency);
	if (!node.get_property ("use-user-latency", _use_user_latency)) {
		_use_user_latency = _user_latency > 0;
	}
	return 0;
}

void
Latent::add_state (XMLNode* node) const
{
	node->set_property ("user-latency", _user_latency);
	node->set_property ("use-user-latency", _use_user_latency);
}
