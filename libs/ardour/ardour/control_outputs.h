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

#ifndef __ardour_control_outputs_h__
#define __ardour_control_outputs_h__

#include <string>
#include "ardour/types.h"
#include "ardour/chan_count.h"
#include "ardour/io_processor.h"

namespace ARDOUR {

/* this exists for one reason only: so that it can override the "type"
   property in the state of the Delivery processor. we need this
   because ControlOutputs are "unique" because they deliver to
   an IO object that is private to a Route and so cannot be looked
   up in the Session etc.
*/

class ControlOutputs : public Delivery {
public:
	ControlOutputs(Session& s);
	XMLNode& get_state ();

	static const std::string processor_type_name;
};


} // namespace ARDOUR

#endif // __ardour_control_outputs_h__

