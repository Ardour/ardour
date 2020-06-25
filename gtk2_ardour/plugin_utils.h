/*
 * Copyright (C) 2020 Robin Gareus <robin@gareus.org>
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

#ifndef __gtkardour_plugin_utils_h__
#define __gtkardour_plugin_utils_h__

#include <list>
#include <string>

#include <boost/tokenizer.hpp>

#include "ardour/plugin.h"
#include "ardour/plugin_manager.h"
#include "ardour/utils.h"

namespace ARDOUR_PLUGIN_UTILS
{

inline static void
setup_search_string (std::string& searchstr)
{
	transform (searchstr.begin (), searchstr.end (), searchstr.begin (), ::toupper);
}

inline static bool
match_search_strings (std::string const& haystack, std::string const& needle)
{
	boost::char_separator<char> sep (" ");
	typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
	tokenizer t (needle, sep);

	for (tokenizer::iterator ti = t.begin (); ti != t.end (); ++ti) {
		if (haystack.find (*ti) == std::string::npos) {
			return false;
		}
	}
	return true;
}

struct PluginUIOrderSorter {
public:
	bool operator() (ARDOUR::PluginInfoPtr a, ARDOUR::PluginInfoPtr b) const
	{
		std::list<std::string>::const_iterator aiter = std::find (_user.begin (), _user.end (), (*a).unique_id);
		std::list<std::string>::const_iterator biter = std::find (_user.begin (), _user.end (), (*b).unique_id);
		if (aiter != _user.end () && biter != _user.end ()) {
			return std::distance (_user.begin (), aiter) < std::distance (_user.begin (), biter);
		}
		if (aiter != _user.end ()) {
			return true;
		}
		if (biter != _user.end ()) {
			return false;
		}
		return ARDOUR::cmp_nocase ((*a).name, (*b).name) == -1;
	}

	PluginUIOrderSorter (std::list<std::string> user)
		: _user (user)
	{ }

private:
	std::list<std::string> _user;
};

struct PluginABCSorter {
	bool operator() (ARDOUR::PluginInfoPtr a, ARDOUR::PluginInfoPtr b) const
	{
		int cmp = ARDOUR::cmp_nocase_utf8 (a->name, b->name);
		if (cmp == 0) {
			/* identical name, compare type */
			return a->type < b->type;
		} else {
			return cmp < 0;
		}
	}
};

struct PluginRecentSorter {
	bool operator() (ARDOUR::PluginInfoPtr a, ARDOUR::PluginInfoPtr b) const
	{
		ARDOUR::PluginManager& manager (ARDOUR::PluginManager::instance ());

		int64_t  lru_a, lru_b;
		uint64_t use_a, use_b;
		bool     stats_a, stats_b;

		stats_a = manager.stats (a, lru_a, use_a);
		stats_b = manager.stats (b, lru_b, use_b);

		if (stats_a && stats_b) {
			return lru_a > lru_b;
		}
		if (stats_a) {
			return true;
		}
		if (stats_b) {
			return false;
		}
		return ARDOUR::cmp_nocase ((*a).name, (*b).name) == -1;
	}
	PluginRecentSorter ()
		: manager (ARDOUR::PluginManager::instance ())
	{ }

private:
	ARDOUR::PluginManager& manager;
};

struct PluginChartsSorter {
	bool operator() (ARDOUR::PluginInfoPtr a, ARDOUR::PluginInfoPtr b) const
	{
		ARDOUR::PluginManager& manager (ARDOUR::PluginManager::instance ());

		int64_t  lru_a, lru_b;
		uint64_t use_a, use_b;
		bool     stats_a, stats_b;

		stats_a = manager.stats (a, lru_a, use_a);
		stats_b = manager.stats (b, lru_b, use_b);

		if (stats_a && stats_b) {
			return use_a > use_b;
		}
		if (stats_a) {
			return true;
		}
		if (stats_b) {
			return false;
		}
		return ARDOUR::cmp_nocase ((*a).name, (*b).name) == -1;
	}
	PluginChartsSorter ()
		: manager (ARDOUR::PluginManager::instance ())
	{ }

private:
	ARDOUR::PluginManager& manager;
};

} // namespace ARDOUR_PLUGIN_UTILS
#endif
