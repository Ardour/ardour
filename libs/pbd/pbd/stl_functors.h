/*
    Copyright (C) 1998-99 Paul Barton-Davis
 
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

#ifndef __stl_functors_h__
#define __stl_functors_h__

#include <string>

#include "pbd/libpbd_visibility.h"

#ifndef LESS_STRING_P
struct LIBPBD_API less<std::string *> {
    bool operator()(std::string *s1, std::string *s2) const {
      return *s1 < *s2;
    }
};
#define LESS_STRING_P
#endif // LESS_STRING_P

#ifndef LESS_CONST_STRING_P
struct LIBPBD_API less<const std::string *> {
    bool operator()(const std::string *s1, const std::string *s2) const {
	return *s1 < *s2;
    }
};
#define LESS_CONST_STRING_P
#endif // LESS_CONST_STRING_P

#ifndef LESS_CONST_CHAR_P
struct LIBPBD_API less<const char *>
{
	bool operator()(const char* s1, const char* s2) const {
		return strcmp(s1, s2) < 0;
	}
};
#define LESS_CONST_CHAR_P
#endif // LESS_CONST_CHAR_P

#ifndef LESS_CONST_FLOAT_P
struct LIBPBD_API less<const float *>
{
	bool operator()(const float *n1, const float *n2) const {
		return *n1 < *n2;
	}
};
#define LESS_CONST_FLOAT_P
#endif // LESS_CONST_FLOAT_P

#ifndef EQUAL_TO_CONST_CHAR_P
struct LIBPBD_API equal_to<const char *>
{
        bool operator()(const char *s1, const char *s2) const {
		return strcmp (s1, s2) == 0;
	}
};
#define EQUAL_TO_CONST_CHAR_P
#endif // EQUAL_TO_CONST_CHAR_P

#ifndef EQUAL_TO_STRING_P
struct LIBPBD_API equal_to<std::string *>
{
        bool operator()(const std::string *s1, const std::string *s2) const {
		return *s1 == *s2;
	}
};
#define EQUAL_TO_STRING_P
#endif // EQUAL_TO_STRING_P

#ifndef LESS_CONST_STRING_R
struct LIBPBD_API less<const std::string &> {
    bool operator() (const std::string &s1, const std::string &s2) {
	    return s1 < s2; 
    }
};
#define LESS_CONST_STRING_R
#endif // EQUAL_TO_STRING_P

#endif // __stl_functors_h__
