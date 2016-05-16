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

#include <sstream>
#include <typeinfo>

#include <cassert>

#include "pbd/enumwriter.h"
#include "pbd/error.h"
#include "pbd/failed_constructor.h"
#include "pbd/xml++.h"

#include "ardour/presentation_info.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using std::string;

const PresentationInfo::order_t PresentationInfo::max_order = UINT32_MAX;
const PresentationInfo::Flag PresentationInfo::Bus = PresentationInfo::Flag (PresentationInfo::AudioBus|PresentationInfo::MidiBus);
const PresentationInfo::Flag PresentationInfo::Track = PresentationInfo::Flag (PresentationInfo::AudioTrack|PresentationInfo::MidiTrack);
const PresentationInfo::Flag PresentationInfo::Route = PresentationInfo::Flag (PresentationInfo::Bus|PresentationInfo::Track);

PresentationInfo::PresentationInfo (std::string const & str)
{
	if (parse (str)) {
		throw failed_constructor ();
	}
}

int
PresentationInfo::parse (string const& str)
{
	std::stringstream s (str);

	/* new school, segmented string "NNN:TYPE" */
	string f;
	char c;
	s >> _order;
	/* skip colon */
	s >> c;
	/* grab flags */
	s >> f;
	_flags = Flag (string_2_enum (f, _flags)|GroupOrderSet);
	std::cerr << "Parsed [" << str << "] as " << _order << " + " << enum_2_string (_flags) << std::endl;
	return 0;
}

int
PresentationInfo::parse (uint32_t n, Flag f)
{
	if (n < UINT16_MAX) {
		assert (f != Flag (0));
		_order = n;
		_flags = Flag (f|GroupOrderSet);
	} else {
		_order = (n & 0xffff);
		_flags = Flag ((n >> 16)|GroupOrderSet);
	}

	return 0;
}

std::string
PresentationInfo::to_string() const
{
	std::stringstream ss;

	/* Do not save or selected hidden status, or group-order set bit */

	Flag f = Flag (_flags & ~(Hidden|Selected|GroupOrderSet));

	ss << _order << ':' << enum_2_string (f);

	return ss.str();
}

PresentationInfo::Flag
PresentationInfo::get_flags (XMLNode const& node)
{
	const XMLProperty *prop;
	XMLNodeList nlist = node.children ();
	XMLNodeConstIterator niter;
	XMLNode *child;

	for (niter = nlist.begin(); niter != nlist.end(); ++niter){
		child = *niter;

		if (child->name() == X_("PresentationInfo")) {
			if ((prop = child->property (X_("value"))) != 0) {
				PresentationInfo pi (prop->value());
				return pi.flags ();
			}
		}
	}
	return Flag (0);
}

std::ostream&
operator<<(std::ostream& o, ARDOUR::PresentationInfo const& rid)
{
	return o << rid.to_string ();
}
