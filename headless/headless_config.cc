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
#include <string>

#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include "ardour/filesystem_paths.h"
#include "ardour/libardour_visibility.h"

using namespace std;

class HeadlessConfig
{
private:
    std::string config_file;

public:
    struct PluginConfig
    {
        bool enable_plugins = false;
        int plugin_timeout_ms = 30000;
        bool strict_plugin_loading = false;
        std::string vst_path;
        std::string plugin_blacklist_file;
        size_t plugin_memory_limit_mb = 1024;
        int plugin_threads = 1;
    };

    HeadlessConfig()
    {
        config_file = Glib::build_filename(ARDOUR::user_config_directory(), "headless_config");
    }

    PluginConfig load_config()
    {
        PluginConfig config;

        // Load from config file if it exists
        if (Glib::file_test(config_file, Glib::FILE_TEST_EXISTS))
        {
            try
            {
                std::ifstream file(config_file);
                std::string line;
                while (std::getline(file, line))
                {
                    if (line.empty() || line[0] == '#')
                    {
                        continue;
                    }

                    size_t pos = line.find('=');
                    if (pos != std::string::npos)
                    {
                        std::string key = line.substr(0, pos);
                        std::string value = line.substr(pos + 1);

                        if (key == "enable_plugins")
                        {
                            config.enable_plugins = (value == "true");
                        }
                        else if (key == "plugin_timeout_ms")
                        {
                            config.plugin_timeout_ms = std::stoi(value);
                        }
                        else if (key == "strict_plugin_loading")
                        {
                            config.strict_plugin_loading = (value == "true");
                        }
                        else if (key == "vst_path")
                        {
                            config.vst_path = value;
                        }
                        else if (key == "plugin_blacklist_file")
                        {
                            config.plugin_blacklist_file = value;
                        }
                        else if (key == "plugin_memory_limit_mb")
                        {
                            config.plugin_memory_limit_mb = std::stoul(value);
                        }
                        else if (key == "plugin_threads")
                        {
                            config.plugin_threads = std::stoi(value);
                        }
                    }
                }
            }
            catch (const std::exception &e)
            {
                cerr << "Error loading headless config: " << e.what() << endl;
            }
        }

        // Load from environment variables (override config file)
        const char *env_enable = getenv("ARDOUR_HEADLESS_ENABLE_PLUGINS");
        if (env_enable)
        {
            config.enable_plugins = (std::string(env_enable) == "true");
        }

        const char *env_timeout = getenv("ARDOUR_HEADLESS_PLUGIN_TIMEOUT");
        if (env_timeout)
        {
            config.plugin_timeout_ms = std::stoi(env_timeout);
        }

        const char *env_strict = getenv("ARDOUR_HEADLESS_STRICT_PLUGINS");
        if (env_strict)
        {
            config.strict_plugin_loading = (std::string(env_strict) == "true");
        }

        const char *env_vst_path = getenv("ARDOUR_HEADLESS_VST_PATH");
        if (env_vst_path)
        {
            config.vst_path = env_vst_path;
        }

        const char *env_blacklist = getenv("ARDOUR_HEADLESS_PLUGIN_BLACKLIST");
        if (env_blacklist)
        {
            config.plugin_blacklist_file = env_blacklist;
        }

        const char *env_memory = getenv("ARDOUR_HEADLESS_PLUGIN_MEMORY_LIMIT");
        if (env_memory)
        {
            config.plugin_memory_limit_mb = std::stoul(env_memory);
        }

        const char *env_threads = getenv("ARDOUR_HEADLESS_PLUGIN_THREADS");
        if (env_threads)
        {
            config.plugin_threads = std::stoi(env_threads);
        }

        return config;
    }

    void save_config(const PluginConfig &config)
    {
        try
        {
            std::ofstream file(config_file);
            if (!file.is_open())
            {
                cerr << "Could not open config file for writing: " << config_file << endl;
                return;
            }

            file << "# Ardour Headless Configuration" << endl;
            file << "enable_plugins=" << (config.enable_plugins ? "true" : "false") << endl;
            file << "plugin_timeout_ms=" << config.plugin_timeout_ms << endl;
            file << "strict_plugin_loading=" << (config.strict_plugin_loading ? "true" : "false") << endl;
            file << "vst_path=" << config.vst_path << endl;
            file << "plugin_blacklist_file=" << config.plugin_blacklist_file << endl;
            file << "plugin_memory_limit_mb=" << config.plugin_memory_limit_mb << endl;
            file << "plugin_threads=" << config.plugin_threads << endl;
        }
        catch (const std::exception &e)
        {
            cerr << "Error saving headless config: " << e.what() << endl;
        }
    }

    std::string get_config_file() const
    {
        return config_file;
    }
};