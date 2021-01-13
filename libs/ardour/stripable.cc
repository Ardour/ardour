/*
 * Copyright (C) 2016-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
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

#include <boost/algorithm/string.hpp>

#include "pbd/compose.h"
#include "pbd/convert.h"
#include "pbd/i18n.h"


#include "ardour/debug.h"
#include "ardour/rc_configuration.h"
#include "ardour/session.h"
#include "ardour/selection.h"
#include "ardour/stripable.h"

using namespace ARDOUR;
using namespace PBD;
using std::string;

Stripable::Stripable (Session& s, string const & name, PresentationInfo const & pi)
	: SessionObject (s, name)
	, Automatable (s, (pi.flags() & PresentationInfo::MidiIndicatingFlags) ? Temporal::BeatTime : Temporal::AudioTime)
	, _presentation_info (pi)
	, _active_color_picker (0)
{
}

Stripable::~Stripable ()
{
	if (!_session.deletion_in_progress ()) {
		_session.selection().remove_stripable_by_id (id());
	}
}

void
Stripable::set_presentation_order (PresentationInfo::order_t order)
{
	_presentation_info.set_order (order);
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

			if (child->name() == PresentationInfo::state_node_name) {
				_presentation_info.set_state (*child, version);
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

		if (!_presentation_info.special(false)) {
			if ((prop = node.property (X_("order-key"))) != 0) {
				_presentation_info.set_order (atol (prop->value()));
			}
		}
	}

	return 0;
}

bool
Stripable::is_selected() const
{
	try {
		boost::shared_ptr<const Stripable> s (shared_from_this());
	} catch (...) {
		std::cerr << "cannot shared-from-this for " << this << std::endl;
		abort ();
	}
	return _session.selection().selected (shared_from_this());
}

bool
Stripable::Sorter::operator() (boost::shared_ptr<ARDOUR::Stripable> a, boost::shared_ptr<ARDOUR::Stripable> b)
{
	const PresentationInfo::Flag a_flag = a->presentation_info().flags ();
	const PresentationInfo::Flag b_flag = b->presentation_info().flags ();

	if (a_flag == b_flag) {
		return a->presentation_info().order() < b->presentation_info().order();
	}

	int cmp_a = 0;
	int cmp_b = 0;

	if (a->is_auditioner ()) { cmp_a = -2; }
	if (b->is_auditioner ()) { cmp_b = -2; }
	if (a->is_monitor ())    { cmp_a = -1; }
	if (b->is_monitor ())    { cmp_b = -1; }

	/* ARDOUR-Editor: [Track|Bus|Master] (0) < VCA (3)
	 * ARDOUR-Mixer : [Track|Bus] (0) < VCA (3) < Master (4)
	 *
	 * Mixbus-Editor: [Track|Bus] (0) < Mixbus (1) < VCA (3) < Master (4)
	 * Mixbus-Mixer : [Track|Bus] (0) < Mixbus (1) < Master (2) < VCA (3)
	 */

	if (a_flag & ARDOUR::PresentationInfo::VCA) {
		cmp_a = 3;
	}
#ifdef MIXBUS
	else if (a_flag & ARDOUR::PresentationInfo::MasterOut) {
		cmp_a = _mixer_order ? 2 : 4;
	}
	else if (a_flag & ARDOUR::PresentationInfo::Mixbus) {
		cmp_a = 1;
	}
#endif
	else if (_mixer_order && (a_flag & ARDOUR::PresentationInfo::MasterOut)) {
		cmp_a = 4;
	}


	if (b_flag & ARDOUR::PresentationInfo::VCA) {
		cmp_b = 3;
	}
#ifdef MIXBUS
	else if (b_flag & ARDOUR::PresentationInfo::MasterOut) {
		cmp_b = _mixer_order ? 2 : 4;
	}
	else if (b_flag & ARDOUR::PresentationInfo::Mixbus) {
		cmp_b = 1;
	}
#endif
	else if (_mixer_order && (b_flag & ARDOUR::PresentationInfo::MasterOut)) {
		cmp_b = 4;
	}

	if (cmp_a == cmp_b) {
		return a->presentation_info().order() < b->presentation_info().order();
	}
	return cmp_a < cmp_b;
}
