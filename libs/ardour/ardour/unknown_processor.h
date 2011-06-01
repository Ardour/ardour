/*
    Copyright (C) 2010 Paul Davis

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

#ifndef __ardour_unknown_processor_h__
#define __ardour_unknown_processor_h__

#include "ardour/processor.h"

namespace ARDOUR {

/** A stub Processor that can be used in place of a `real' one that cannot be
 *  created for some reason; usually because it requires a plugin which is not
 *  present.  UnknownProcessors are special-cased in a few places, notably
 *  in route configuration and signal processing, so that on encountering them
 *  configuration or processing stops.
 *
 *  When a Processor is missing from a Route, the following processors cannot
 *  be configured, as the missing Processor's output port configuration is
 *  unknown.
 *
 *  The main utility of the UnknownProcessor is that it allows state
 *  to be preserved, so that, for example, loading and re-saving a
 *  session on a machine without a particular plugin will not corrupt
 *  the session.
 */
class UnknownProcessor : public Processor
{
public:
	UnknownProcessor (Session &, XMLNode const &);

	/* These processors are hidden from view */
	bool display_to_user () const {
		return false;
	}

	bool can_support_io_configuration (const ChanCount &, ChanCount &) const {
		return false;
	}

	XMLNode & state (bool);

private:
	XMLNode _state;
};

}

#endif
