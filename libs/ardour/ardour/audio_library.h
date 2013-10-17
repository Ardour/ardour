/*
    Copyright (C) 2003-2006 Paul Davis

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

#ifndef __ardour_audio_library_h__
#define __ardour_audio_library_h__

#include <string>
#include <map>
#include <vector>

#include "ardour/libardour_visibility.h"

namespace ARDOUR {

class LIBARDOUR_API AudioLibrary
{
  public:
	AudioLibrary ();
	~AudioLibrary ();

	void set_tags (std::string member, std::vector<std::string> tags);
	std::vector<std::string> get_tags (std::string member);

	void search_members_and (std::vector<std::string>& results, const std::vector<std::string>& tags);

	void save_changes();

  private:
	std::string src;
};

extern AudioLibrary* Library;

} // ARDOUR namespace

#endif // __ardour_audio_library_h__
