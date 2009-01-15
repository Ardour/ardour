/*
    Copyright (C) 2009 Paul Davis
 
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

#include <cstdio> /* for sprintf, sigh */
#include <climits>
#include <pbd/error.h>
#include <pbd/xml++.h>

#include "osc_controllable.h"

using namespace sigc;
using namespace PBD;
using namespace ARDOUR;

OSCControllable::OSCControllable (lo_address a, Controllable& c)
	: controllable (c)
	, addr (a)
{
}

OSCControllable::~OSCControllable ()
{
	lo_address_free (addr);
}

XMLNode&
OSCControllable::get_state ()
{
	XMLNode& root (controllable.get_state());
	return root;
}

int
OSCControllable::set_state (const XMLNode& node)
{
	return 0;
}

