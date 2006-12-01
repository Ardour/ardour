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

using std::vector;
using std::string;
using std::map;

namespace ARDOUR {

class AudioLibrary
{
  public:
	AudioLibrary ();
	~AudioLibrary ();

	void set_tags (string member, vector<string> tags);
	vector<string> get_tags (string member);

	void search_members_and (vector<string>& results, const vector<string> tags);

	void save_changes();
	
  private:
	string src;
	
	string path2uri (string);
	
	bool safe_file_extension (string);
};

extern AudioLibrary* Library;

} // ARDOUR namespace

#endif // __ardour_audio_library_h__
