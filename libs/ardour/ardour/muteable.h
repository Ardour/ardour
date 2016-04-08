/*
    Copyright (C) 2016 Paul Davis

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __ardour_muteable_h__
#define __ardour_muteable_h__

#include <boost/shared_ptr.hpp>

#include "pbd/signals.h"

namespace ARDOUR {

class MuteMaster;
class Session;

class Muteable {
    public:
	Muteable (Session&, std::string const &name);
	virtual ~Muteable() {}

	virtual bool can_be_muted_by_others () const = 0;
	virtual void act_on_mute () {}

	boost::shared_ptr<MuteMaster> mute_master() const {
		return _mute_master;
	}

	PBD::Signal0<void> mute_points_changed;

    protected:
	boost::shared_ptr<MuteMaster> _mute_master;
};

} /* namespace */

#endif /* __ardour_muteable_h__ */
