/*
    Copyright (C) 2002 Paul Davis 

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

#ifndef __pbd_convert_h__
#define __pbd_convert_h__

#include <stdint.h>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <glibmm/ustring.h>

namespace PBD {

std::string short_version (std::string, std::string::size_type target_length);

int     atoi (const std::string&);
int32_t atol (const std::string&);
int64_t atoll (const std::string&);
double  atof (const std::string&);
std::string url_decode (std::string const &);

std::string capitalize (const std::string&);

// std::string length2string (const int32_t frames, const float sample_rate);
std::string length2string (const int64_t frames, const double sample_rate);

std::vector<std::string> internationalize (const char *, const char **);
bool strings_equal_ignore_case (const std::string& a, const std::string& b);

template <class T> std::string 
to_string (T t, std::ios_base & (*f)(std::ios_base&))
{
	std::ostringstream oss;
	oss << f << t;
	return oss.str();
}

const char *
sgettext (const char *, const char *);

} //namespace PBD

#endif /* __pbd_convert_h__ */
