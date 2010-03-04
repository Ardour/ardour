/*
    Copyright (C) 2000-2010 Paul Davis 

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

#ifndef __pbd_stateful_owner_h__
#define __pbd_stateful_owner_h__

#include <vector>

namespace PBD {

class StatefulDiffCommand;

/** Base (pure virtual) class for objects with other Stateful's within */
class StatefulOwner {
  public:
	StatefulOwner () {}
	virtual ~StatefulOwner () {}

        virtual void rdiff (std::vector<StatefulDiffCommand*>& cmds) const = 0;
        virtual void clear_owned_history () = 0;
};

} // namespace PBD

#endif /* __pbd_stateful_owner_h__ */

