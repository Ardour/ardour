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

#include "pbd/atomic.h"
#include "pbd/signals.h"

namespace PBD
{

/* MergeableSignal
 * A PBD::Signal derivative whose emissions can be merged when a SignalMerger object is alive
 * This only covers simple signals with a void return because ignored emissions wouldn't return anything
 */

template <typename = void, typename... T_arg>
class MergeableSignal : public PBD::Signal<void (T_arg...)>
{
private:
	template <typename F, typename... F_arg>
	friend class SignalMerger;

	/* SignalMerger counter */
	std::atomic<int> _merger_alive = 0;

	/* Should merge flag */
	std::atomic<bool> _should_emit_after_merger_release = false;

	/* Signal args storage */
	std::tuple<T_arg...> _merged_args;

public:
	/*  mimic sigc:signal operator() -> emit() forwarding */
	void operator() (T_arg... a)
	{
		emit (std::forward<T_arg> (a)...);
	};
	void emit (T_arg... a)
	{
		if (_merger_alive.load () > 0) {
			/* if a SignalMerger for this signal is alive, prevent signal emission
			 * and store args for when the merged signal
			 */
			_should_emit_after_merger_release.store (true);
			if constexpr (sizeof...(a)) {
				_merged_args = { std::forward<T_arg> (a)... };
			}
		} else {
			/* otherwise, just emit */
			PBD::Signal<void (T_arg...)> (std::forward<T_arg> (a)...);
		}
	}
};

/* SignalMerger
 * A class that allows temporarly freezing a MergeableSignal's emission and emits
 * a single signal when destroyed if at least one signal emission was prevented
 * during its lifetime (RAII).
 */

template <typename = void, typename... T_arg>
class SignalMerger
{
private:
	/* pointer to merged signal */
	MergeableSignal<void (T_arg...)>* _signal;

public:
	SignalMerger (MergeableSignal<void (T_arg...)>* s)
	        : _signal (s)
	{
		/* RAII: increment signal's counter */
		PBD::atomic_inc (_signal->_merger_alive);
	}
	~SignalMerger ()
	{
		if (PBD::atomic_dec_and_test (_signal->_merger_alive)) {
			/* If no SignalMerger left alive for this signal */
			if (_signal->_should_emit_after_merger_release.load ()) {
				/* ... and at least one signal emission was prevented,
				 * emit a single time with the last provided arguments
				 */
				_signal->_should_emit_after_merger_release.store (false);
				if constexpr (std::tuple_size<decltype (_signal->_merged_args)>::value) {
					std::apply (sigc::mem_fun (_signal, &MergeableSignal<void (T_arg...)>::emit), _signal->_merged_args);
				} else {
					_signal->emit ();
				}

				/* XXX maybe empty/reset _signal->_merged_args ? */
			}
		}
	}
};

} // namespace PBD
