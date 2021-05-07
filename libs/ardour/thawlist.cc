/*
 * Copyright (C) 2021 Robin Gareus <robin@gareus.org>
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

#include <algorithm>

#include "ardour/region.h"
#include "ardour/thawlist.h"

using namespace ARDOUR;

ThawList::~ThawList ()
{
	assert (empty ()); // so far all lists are explicitly release()d
	release ();
}

void
ThawList::add (boost::shared_ptr<Region> r)
{
	if (std::find (begin (), end (), r) != end ()) {
		return;
	}
	r->suspend_property_changes ();
	push_back (r);
}

void
ThawList::release ()
{
	Region::ChangeMap cm;
	for (RegionList::iterator i = begin (); i != end (); ++i) {
		(*i)->set_changemap (&cm);
		(*i)->resume_property_changes ();
		(*i)->set_changemap (0);
	}
	for (Region::ChangeMap::const_iterator i = cm.begin (); i != cm.end (); ++i) {
		boost::shared_ptr<RegionList> rl (new RegionList (i->second));
		assert (rl->size () > 0);
		Region::RegionsPropertyChanged (rl, i->first);
	}
	clear ();
}
