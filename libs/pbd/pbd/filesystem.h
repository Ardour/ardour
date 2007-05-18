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

#ifndef __filesystem_h__
#define __filesystem_h__

#include <string>

namespace PBD {

namespace sys {

using std::string;

class path
{
public:
	path() : m_path("") { }
	path(const path & p) : m_path(p.m_path) { }
	path(const string & s) : m_path(s) { }
	path(const char* s) : m_path(s) { }
	
	path& operator=(const path& p) { m_path = p.m_path; return *this;}
	path& operator=(const string& s) { m_path = s; return *this; }
	path& operator=(const char* s) { m_path = s; return *this; }

	path& operator/=(const path& rhs);
	path& operator/=(const string& s);
	path& operator/=(const char* s);

	const string to_string() const { return m_path; }

private:

	string m_path;
};


bool exists(const path & p);
bool is_directory(const path & p);

bool create_directory(const path & p);
bool create_directories(const path & p);

string basename (const path& p);

} // namespace sys

} // namespace PBD

#endif
