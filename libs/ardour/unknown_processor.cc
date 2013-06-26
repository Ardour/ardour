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

#include "ardour/unknown_processor.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;

UnknownProcessor::UnknownProcessor (Session& s, XMLNode const & state)
	: Processor (s, "")
	, _state (state)
{
	XMLProperty const * prop = state.property (X_("name"));
	if (prop) {
		set_name (prop->value ());
	}
}

XMLNode &
UnknownProcessor::state (bool)
{
	return *(new XMLNode (_state));
}

