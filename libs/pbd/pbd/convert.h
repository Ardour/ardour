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

#include <string>
#include <vector>

namespace PBD {

std::string short_version (std::string, std::string::size_type target_length);

int    atoi (const std::string&);
double atof (const std::string&);
void   url_decode (std::string&);

// std::string length2string (const int32_t frames, const float sample_rate);
std::string length2string (const int64_t frames, const double sample_rate);

std::vector<std::string> internationalize (const char *, const char **);

} //namespace PBD

#endif /* __pbd_convert_h__ */
