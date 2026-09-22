/*
 * Copyright (C) 2026 Jean-Emmanuel Doucet <jean-emmanuel.doucet@groolot.net>
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

#include <sigc++/signal.h>

namespace sigc
{

/* mergeable_signal
 * A sigc::signal derivative whose emissions can be merged when a signal_merger object is alive
 * This only covers simple signals with a void return because ignored emissions wouldn't return anything
 */

template <typename = void, typename... T_arg>
class mergeable_signal : public sigc::signal<void, T_arg...>
{
private:
	template <typename F, typename... F_arg>
	friend class signal_merger;

	/* signal_merger counter */
	int _merger_alive = 0;

	/* Should merge flag */
	bool _should_emit_after_merger_release = false;

	/* Signal args storage */
	std::tuple<T_arg...> _merged_args;

public:
	/* mimic sigc:signal operator() -> emit() forwarding
	 * XXX: use of method shadowing (these are not virtual in sigc)
	 */
	void operator() (T_arg... a)
	{
		emit (std::forward<T_arg> (a)...);
	};
	void emit (T_arg... a)
	{
		if (_merger_alive > 0) {
			/* if a signal_merger for this signal is alive, prevent signal emission
			 * and store args for when the merged signal
			 */
			_should_emit_after_merger_release = true;
			if constexpr (sizeof...(a)) {
				_merged_args = { std::forward<T_arg> (a)... };
			}
		} else {
			/* otherwise, just emit */
			sigc::signal<void, T_arg...>::emit (std::forward<T_arg> (a)...);
		}
	}
};

/* signal_merger
 * A class that allows temporarly freezing a mergeable_signal's emission and emits
 * a single signal when destroyed if at least one signal emission was prevented
 * during its lifetime (RAII).
 */

template <typename = void, typename... T_arg>
class signal_merger
{
private:
	/* pointer to merged signal */
	mergeable_signal<void, T_arg...>* _signal;

public:
	signal_merger (mergeable_signal<void, T_arg...>* s)
	: _signal (s)
	{
		/* RAII: increment signal's counter */
		_signal->_merger_alive++;
	}
	~signal_merger ()
	{
		if (--_signal->_merger_alive == 0) {
			/* If no signal_merger left alive for this signal */
			if (_signal->_should_emit_after_merger_release) {
				/* ... and at least one signal emission was prevented,
				 * emit a single time with the last provided arguments
				 */
				_signal->_should_emit_after_merger_release = false;
				if constexpr (std::tuple_size<decltype (_signal->_merged_args)>::value) {
					std::apply (sigc::mem_fun (_signal, &mergeable_signal<void, T_arg...>::emit), _signal->_merged_args);
				} else {
					_signal->emit ();
				}

				/* XXX maybe empty/reset _signal->_merged_args ? */
			}
		}
	}
};

} // namespace sigc
