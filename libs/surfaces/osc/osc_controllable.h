/*
    Copyright (C) 1998-2006 Paul Davis
 
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

#ifndef __osc_osccontrollable_h__
#define __osc_osccontrollable_h__

#include <string>

#include <sigc++/sigc++.h>
#include <lo/lo.h>

#include <pbd/controllable.h>
#include <pbd/stateful.h>
#include <ardour/types.h>

class OSCControllable : public PBD::Stateful
{
  public:
	OSCControllable (lo_address addr, PBD::Controllable&);
	virtual ~OSCControllable ();

	XMLNode& get_state ();
	int set_state (const XMLNode& node);

  private:
	PBD::Controllable& controllable;
	lo_address addr;
};

#endif /* __osc_osccontrollable_h__ */
