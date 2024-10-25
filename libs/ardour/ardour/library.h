/*
 * Copyright (C) 2022 Paul Davis <paul@linuxaudiosystems.com>
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

#pragma once

#include <cstdio>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>


#include <ardour/libardour_visibility.h>
#include <curl/curl.h>

namespace ARDOUR {

class LIBARDOUR_API LibraryDescription
{
   public:
	LibraryDescription (std::string const & n, std::string const & a, std::string const & d, std::string const & u, std::string const & l, std::string const & td, std::string const & s)
		: _name (n), _author (a), _description (d), _url (u), _license (l), _toplevel_dir (td), _size (s), _installed (false) {}

	std::string const & name() const { return _name; }
	std::string const & description() const { return _description; }
	std::string const & author() const { return _author; }
	std::string const & url() const { return _url; }
	std::string const & license() const { return _license; }
	std::string const & toplevel_dir() const { return _toplevel_dir; }
	std::string const & size() const { return _size; }

	bool installed() const { return _installed; }
	void set_installed (bool yn) { _installed = yn; }

  private:
	std::string _name;
	std::string _author;
	std::string _description;
	std::string _url;
	std::string _license;
	std::string _toplevel_dir;
	std::string _size;
	bool _installed;
};

class LIBARDOUR_API LibraryFetcher {
  public:
	LibraryFetcher();

	int add (std::string const & root_dir);

	int get_descriptions ();
	size_t n_descriptions() const { return _descriptions.size(); }
	void foreach_description (std::function<void (LibraryDescription)> f) const;

	bool installed (LibraryDescription const & desc);
	void foreach_description (std::function<void (LibraryDescription)> f);

  private:
	std::vector<LibraryDescription> _descriptions;
	std::string install_path_for (LibraryDescription const &);
};

} /* namespace */

