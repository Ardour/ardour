/*
 * Copyright (C) 2024 Ardour Development Team
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

#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <string>

#include "ardour/libardour_visibility.h"

using namespace std;

class PluginErrorHandler
{
private:
    std::set<std::string> blacklisted_plugins;
    std::map<std::string, int> plugin_failure_count;

public:
    bool should_retry_plugin(const std::string &plugin_name)
    {
        if (blacklisted_plugins.find(plugin_name) != blacklisted_plugins.end())
        {
            return false;
        }

        auto it = plugin_failure_count.find(plugin_name);
        return it == plugin_failure_count.end() || it->second < 3;
    }

    void record_plugin_failure(const std::string &plugin_name)
    {
        plugin_failure_count[plugin_name]++;
        if (plugin_failure_count[plugin_name] >= 3)
        {
            blacklisted_plugins.insert(plugin_name);
            cerr << "Plugin blacklisted due to repeated failures: " << plugin_name << endl;
        }
    }

    void load_blacklist_from_file(const std::string &filename)
    {
        std::ifstream file(filename);
        if (!file.is_open())
        {
            cerr << "Could not open plugin blacklist file: " << filename << endl;
            return;
        }

        std::string line;
        while (std::getline(file, line))
        {
            // Skip empty lines and comments
            if (line.empty() || line[0] == '#')
            {
                continue;
            }
            blacklisted_plugins.insert(line);
        }
    }

    void save_blacklist_to_file(const std::string &filename)
    {
        std::ofstream file(filename);
        if (!file.is_open())
        {
            cerr << "Could not open plugin blacklist file for writing: " << filename << endl;
            return;
        }

        for (const auto &plugin : blacklisted_plugins)
        {
            file << plugin << endl;
        }
    }

    bool is_blacklisted(const std::string &plugin_name) const
    {
        return blacklisted_plugins.find(plugin_name) != blacklisted_plugins.end();
    }

    void clear_blacklist()
    {
        blacklisted_plugins.clear();
        plugin_failure_count.clear();
    }

    size_t blacklist_size() const
    {
        return blacklisted_plugins.size();
    }
};