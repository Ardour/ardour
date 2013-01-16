/*
    Copyright (C) 2012 Paul Davis 
    Author: Sakari Bergen

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

#include "audiographer/debug_utils.h"

#include "audiographer/process_context.h"

#include <sstream>

namespace AudioGrapher {

std::string
DebugUtils::process_context_flag_name (FlagField::Flag flag)
{
	std::ostringstream ret;
	
	switch (flag) {
		case ProcessContext<>::EndOfInput:
			ret << "EndOfInput";
			break;
		default:
			ret << flag;
			break;
	}
	
	return ret.str();
}

} // namespace
