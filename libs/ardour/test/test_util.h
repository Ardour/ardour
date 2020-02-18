/*
 * Copyright (C) 2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2016 Tim Mayberry <mojofunk@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef ARDOUR_TEST_UTIL_H
#define ARDOUR_TEST_UTIL_H

#include <string>
#include <list>

#include "pbd/search_path.h"

class XMLNode;

namespace ARDOUR {
	class Session;
}

PBD::Searchpath test_search_path ();

std::string new_test_output_dir (std::string prefix = "");

int get_test_sample_rate ();

extern void check_xml (XMLNode *, std::string, std::list<std::string> const &);
extern bool write_ref (XMLNode *, std::string);
extern void create_and_start_dummy_backend ();
extern void stop_and_destroy_backend ();
extern ARDOUR::Session* load_session (std::string, std::string);

void get_utf8_test_strings (std::vector<std::string>& results);

#endif
