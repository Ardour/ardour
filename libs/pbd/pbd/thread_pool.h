/*
 * Copyright (C) 2026 Robin Gareus <robin@gareus.org>
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

#include "pbd/libpbd_visibility.h"

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

namespace PBD
{
class LIBPBD_API ThreadPool
{
public:
	ThreadPool (size_t num_threads)
		: _run (true)
	{
		for (size_t i = 0; i < num_threads; ++i) {
			_threads.emplace_back ([this] {
				while (true) {
					std::function<void ()> task;
					{
						std::unique_lock<std::mutex> lock (_queue_lock);

						_trigger.wait (lock, [this] {
							return !_queue.empty () || !_run;
						});

						if (!_run && _queue.empty ()) {
							return;
						}

						task = std::move (_queue.front ());
						_queue.pop ();
					}

					task ();
				}
			});
		}
	}

	~ThreadPool ()
	{
		{
			std::unique_lock<std::mutex> lock (_queue_lock);
			_run = false;
		}

		_trigger.notify_all ();

		for (auto& thread : _threads) {
			thread.join ();
		}
	}

	void push (std::function<void ()> task)
	{
		{
			std::unique_lock<std::mutex> lock (_queue_lock);
			_queue.emplace (std::move (task));
		}
		_trigger.notify_one ();
	}

private:
	std::vector<std::thread>           _threads;
	std::queue<std::function<void ()>> _queue;
	std::mutex                         _queue_lock;
	std::condition_variable            _trigger;
	bool                               _run;
};

} // namespace PBD
