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

#include <pbd/error.h>

#include <ardour/tape_file_matcher.h>

#include "i18n.h"

namespace {

const char* const tape_file_regex_string = X_("/T[0-9][0-9][0-9][0-9]-");

}

namespace ARDOUR {

TapeFileMatcher::TapeFileMatcher()
{
	int err;

	if ((err = regcomp (&m_compiled_pattern,
					tape_file_regex_string, REG_EXTENDED|REG_NOSUB)))
	{
		char msg[256];
		
		regerror (err, &m_compiled_pattern, msg, sizeof (msg));
		
		PBD::error << string_compose (_("Cannot compile tape track regexp for use (%1)"), msg) << endmsg;
		// throw
	}

}

bool
TapeFileMatcher::matches (const string& audio_filename) const
{

	if (regexec (&m_compiled_pattern, audio_filename.c_str(), 0, 0, 0) == 0)
	{
		// matches
		return true;
	}
	return false;
}

} // namespace ARDOUR
