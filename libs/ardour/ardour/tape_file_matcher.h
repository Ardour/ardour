/*
	Copyright (C) 2007 Tim Mayberry

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

#ifndef ARDOUR_TAPE_FILE_MATCHER_INCLUDED
#define ARDOUR_TAPE_FILE_MATCHER_INCLUDED

#include <string>

#include <regex.h>

#include "ardour/libardour_visibility.h"

namespace ARDOUR {

class LIBARDOUR_API TapeFileMatcher
{
public:

	TapeFileMatcher();

	bool matches (const std::string& filename) const;

private:

	regex_t m_compiled_pattern;
};

} // namespace ARDOUR

#endif
