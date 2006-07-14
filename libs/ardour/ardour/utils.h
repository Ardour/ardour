/*
    Copyright (C) 1999 Paul Davis 

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

    $Id$
*/

#ifndef __ardour_utils_h__
#define __ardour_utils_h__

#include <iostream>
#include <string>
#include <cmath>

#include "ardour.h"

class XMLNode;

void elapsed_time_to_str (char *buf, uint32_t seconds);
std::string legalize_for_path (std::string str);
std::ostream& operator<< (std::ostream& o, const ARDOUR::BBT_Time& bbt);
XMLNode* find_named_node (const XMLNode& node, std::string name);
std::string placement_as_string (ARDOUR::Placement);

static inline float f_max(float x, float a) {
	x -= a;
	x += fabsf (x);
	x *= 0.5f;
	x += a;
	
	return (x);
}

int cmp_nocase (const std::string& s, const std::string& s2);

int tokenize_fullpath (std::string fullpath, std::string& path, std::string& name);

int touch_file(std::string path);

std::string region_name_from_path (std::string path);
std::string path_expand (std::string);

#endif /* __ardour_utils_h__ */
