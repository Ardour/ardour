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

#include <chrono>
#include <future>
#include <iostream>
#include <thread>

#include "ardour/plugin.h"
#include "ardour/session.h"
#include "ardour/libardour_visibility.h"

using namespace std;
using namespace ARDOUR;

class PluginLoader
{
private:
    std::chrono::milliseconds timeout_ms;
    std::atomic<bool> cancelled{false};

public:
    PluginLoader(int timeout_ms = 30000) : timeout_ms(timeout_ms) {}

    template <typename F>
    auto execute_with_timeout(F &&func) -> decltype(func())
    {
        auto future = std::async(std::launch::async, std::forward<F>(func));

        if (future.wait_for(timeout_ms) == std::future_status::timeout)
        {
            cancelled = true;
            throw std::runtime_error("Plugin processing timeout");
        }

        return future.get();
    }

    bool load_plugin_with_retry(PluginInfoPtr plugin_info, Session &session)
    {
        for (int retry = 0; retry < 3; ++retry)
        {
            try
            {
                PluginLoader loader(5000); // 5 second timeout per attempt
                return loader.execute_with_timeout([&]()
                                                   { return plugin_info->load(session) != nullptr; });
            }
            catch (const std::exception &e)
            {
                cerr << "Plugin loading attempt " << (retry + 1) << " failed: " << e.what() << endl;
                if (retry == 2)
                {
                    return false;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        return false;
    }

    void cancel()
    {
        cancelled = true;
    }

    bool is_cancelled() const
    {
        return cancelled;
    }
};