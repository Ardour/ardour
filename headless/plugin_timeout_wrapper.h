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

#ifndef __ardour_plugin_timeout_wrapper_h__
#define __ardour_plugin_timeout_wrapper_h__

#include <chrono>
#include <future>
#include <atomic>

class PluginTimeoutWrapper
{
private:
    std::chrono::milliseconds timeout_ms;
    std::atomic<bool> cancelled{false};

public:
    PluginTimeoutWrapper(int timeout_ms = 30000) : timeout_ms(timeout_ms) {}

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

    void cancel()
    {
        cancelled = true;
    }

    bool is_cancelled() const
    {
        return cancelled;
    }

    void set_timeout(int timeout_ms)
    {
        this->timeout_ms = std::chrono::milliseconds(timeout_ms);
    }

    int get_timeout() const
    {
        return timeout_ms.count();
    }
};

#endif // __ardour_plugin_timeout_wrapper_h__