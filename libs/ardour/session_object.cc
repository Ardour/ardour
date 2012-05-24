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

#include <iostream>

#include "ardour/debug.h"
#include "ardour/session_object.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;

namespace ARDOUR {
	namespace Properties {
		PropertyDescriptor<std::string> name;
	}
}

void
SessionObject::make_property_quarks ()
{
	Properties::name.property_id = g_quark_from_static_string (X_("name"));
        DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for name = %1\n", 	Properties::name.property_id));
}

