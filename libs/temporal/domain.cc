/*
 * Copyright (C) 2023 Paul Davis <paul@linuxaudiosystems.com>
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

#include "temporal/domain_provider.h"
#include "temporal/types_convert.h"

#include "pbd/i18n.h"

using namespace Temporal;

XMLNode&
TimeDomainProvider::get_state () const
{
	XMLNode* node = new XMLNode (X_("TimeDomainProvider"));
	node->set_property (X_("has-own"), have_domain);
	if (have_domain) {
		node->set_property (X_("domain"), domain);
	}
	return *node;
}

int
TimeDomainProvider::set_state (const XMLNode& node, int version)
{
	node.get_property (X_("has-own"), have_domain);
	if (have_domain) {
		node.get_property (X_("domain"), domain);
	}
	return 0;
}

