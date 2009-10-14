/*
    Copyright (C) 2006 Paul Davis

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

#include "ardour/control_outputs.h"

using namespace std;
using namespace ARDOUR;

const std::string ControlOutputs::processor_type_name = "control-outputs";

ControlOutputs::ControlOutputs(Session& s, IO* io, const std::string& name)
	: Delivery (s, io, name)
{
}

XMLNode&
ControlOutputs::get_state()
{
	XMLNode& node (Delivery::get_state());
	node.add_property ("type", processor_type_name);
	return node;
}

