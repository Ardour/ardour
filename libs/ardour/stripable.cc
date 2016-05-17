/*
    Copyright (C) 2016 Paul Davis

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

#include <boost/algorithm/string.hpp>

#include "pbd/compose.h"
#include "pbd/convert.h"

#include "ardour/debug.h"
#include "ardour/rc_configuration.h"
#include "ardour/stripable.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using std::string;

PBD::Signal0<void> Stripable::PresentationInfoChange;

Stripable::Stripable (Session& s, string const & name, PresentationInfo const & pi)
	: SessionObject (s, name)
	, _presentation_info (pi)
{
}

void
Stripable::set_presentation_group_order (PresentationInfo::order_t order, bool notify_class_listeners)
{
	set_presentation_info (PresentationInfo (order, _presentation_info.flags()), notify_class_listeners);
}

void
Stripable::set_presentation_group_order_explicit (PresentationInfo::order_t order)
{
	set_presentation_group_order (order, false);
}

void
Stripable::set_presentation_info (PresentationInfo pi, bool notify_class_listeners)
{
	if (pi != presentation_info()) {

		DEBUG_TRACE (DEBUG::OrderKeys, string_compose ("%1: set presentation info to %2\n", name(), pi));

		if (is_master()) {
			_presentation_info = PresentationInfo (0, PresentationInfo::MasterOut);
		} else if (is_monitor()) {
			_presentation_info = PresentationInfo (0, PresentationInfo::MonitorOut);
		} else {
			_presentation_info = pi;
		}

		PresentationInfoChanged ();

		if (notify_class_listeners) {
			PresentationInfoChange ();
		}
	}
}

void
Stripable::set_presentation_info_explicit (PresentationInfo pi)
{
	set_presentation_info (pi, false);
}

int
Stripable::set_state (XMLNode const& node, int version)
{
	const XMLProperty *prop;
	XMLNodeList const & nlist (node.children());
	XMLNodeConstIterator niter;
	XMLNode *child;

	if (version > 3001) {

		for (niter = nlist.begin(); niter != nlist.end(); ++niter){
			child = *niter;

			if (child->name() == X_("PresentationInfo")) {
				if ((prop = child->property (X_("value"))) != 0) {
					_presentation_info = prop->value ();
				}
			}
		}

	} else {

		/* Older versions of Ardour stored "_flags" as a property of the Route
		 * node, only for 3 special Routes (MasterOut, MonitorOut, Auditioner.
		 *
		 * Their presentation order was stored in a node called "RemoteControl"
		 *
		 * This information is now part of the PresentationInfo of every Stripable.
		 */

		if ((prop = node.property (X_("flags"))) != 0) {

			/* 4.x and earlier - didn't have Stripable but the
			 * relevant enums have the same names (MasterOut,
			 * MonitorOut, Auditioner), so we can use string_2_enum
			 */

			PresentationInfo::Flag flags;

			if (version < 3000) {
				string f (prop->value());
				boost::replace_all (f, "ControlOut", "MonitorOut");
				flags = PresentationInfo::Flag (string_2_enum (f, flags));
			} else {
				flags = PresentationInfo::Flag (string_2_enum (prop->value(), flags));
			}

			_presentation_info.set_flags (flags);

		}

		if (!_presentation_info.special()) {
			if ((prop = node.property (X_("order-key"))) != 0) {
				_presentation_info.set_group_order (atol (prop->value()));
			}
		}
	}

	return 0;
}

void
Stripable::add_state (XMLNode& node) const
{
	XMLNode* remote_control_node = new XMLNode (X_("PresentationInfo"));
	remote_control_node->add_property (X_("value"), _presentation_info.to_string());
	node.add_child_nocopy (*remote_control_node);
}
