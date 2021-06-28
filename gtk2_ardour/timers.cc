/*
 * Copyright (C) 2014 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2015 Robin Gareus <robin@gareus.org>
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

#include "timers.h"

#include "pbd/timer.h"
#include "pbd/debug.h"
#include "pbd/compose.h"
#include "pbd/g_atomic_compat.h"

#include "debug.h"

namespace {

class StandardTimer : public PBD::StandardTimer
{
public:
	StandardTimer (unsigned int interval)
		: PBD::StandardTimer(interval)
	{ }

	virtual bool on_elapsed () {
		DEBUG_TIMING_ADD_ELAPSED(PBD::DEBUG::GUITiming, timing_interval_data);
		DEBUG_TIMING_START(PBD::DEBUG::GUITiming, timing_exec_data);

		bool ret_val = PBD::StandardTimer::on_elapsed ();

		DEBUG_TIMING_ADD_ELAPSED(PBD::DEBUG::GUITiming, timing_exec_data);
		DEBUG_TIMING_START(PBD::DEBUG::GUITiming, timing_interval_data);
		return ret_val;
	}

#ifndef NDEBUG
	PBD::TimingData timing_interval_data;
	PBD::TimingData timing_exec_data;
#endif
};

class BlinkTimer : public PBD::BlinkTimer
{
public:
	BlinkTimer (unsigned int interval)
		: PBD::BlinkTimer(interval)
	{ }

	virtual bool on_elapsed () {
		DEBUG_TIMING_ADD_ELAPSED(PBD::DEBUG::GUITiming, timing_interval_data);
		DEBUG_TIMING_START(PBD::DEBUG::GUITiming, timing_exec_data);

		bool ret_val = PBD::BlinkTimer::on_elapsed ();

		DEBUG_TIMING_ADD_ELAPSED(PBD::DEBUG::GUITiming, timing_exec_data);
		DEBUG_TIMING_START(PBD::DEBUG::GUITiming, timing_interval_data);
		return ret_val;
	}

#ifndef NDEBUG
	PBD::TimingData timing_interval_data;
	PBD::TimingData timing_exec_data;
#endif
};


class UITimers
{

public:

	UITimers ()
		: blink(240)
		, second(1000)
		, rapid(100)
		, super_rapid(40)
		, fps(40)
	{
	g_atomic_int_set (&_suspend_counter, 0);
#ifndef NDEBUG
		second.connect (sigc::mem_fun (*this, &UITimers::on_second_timer));
#endif
	}

	BlinkTimer      blink;
	StandardTimer   second;
	StandardTimer   rapid;
	StandardTimer   super_rapid;
	StandardTimer   fps;

	GATOMIC_QUAL gint _suspend_counter;

#ifndef NDEBUG
	std::vector<int64_t> rapid_eps_count;
	std::vector<int64_t> super_rapid_eps_count;
	std::vector<int64_t> fps_eps_count;

private:

	void debug_rapid_timer () {
		DEBUG_TRACE(PBD::DEBUG::GUITiming, string_compose ("Rapid Connections: %1\n", rapid.connection_count ()));

		rapid_eps_count.push_back (rapid.timing_exec_data.size());

		DEBUG_TRACE(PBD::DEBUG::GUITiming, string_compose ("Rapid Exec Totals: %1", PBD::timing_summary (rapid_eps_count)));

		DEBUG_TRACE(PBD::DEBUG::GUITiming, string_compose ("Rapid Interval: %1", rapid.timing_interval_data.summary()));
		DEBUG_TRACE(PBD::DEBUG::GUITiming, string_compose ("Rapid Exec: %1", rapid.timing_exec_data.summary()));
		DEBUG_TIMING_RESET(PBD::DEBUG::GUITiming, rapid.timing_interval_data);
		DEBUG_TIMING_RESET(PBD::DEBUG::GUITiming, rapid.timing_exec_data);
	}

	void debug_super_rapid_timer () {
		// we don't use this timer on windows so don't display empty data for it
#ifndef PLATFORM_WINDOWS

		DEBUG_TRACE(PBD::DEBUG::GUITiming, string_compose ("Super Rapid Connections: %1\n", super_rapid.connection_count ()));

		super_rapid_eps_count.push_back (super_rapid.timing_exec_data.size());

		DEBUG_TRACE(PBD::DEBUG::GUITiming, string_compose ("Super Rapid Exec Totals: %1", PBD::timing_summary (super_rapid_eps_count)));
		DEBUG_TRACE(PBD::DEBUG::GUITiming, string_compose ("Super Rapid Interval: %1", super_rapid.timing_interval_data.summary()));
		DEBUG_TRACE(PBD::DEBUG::GUITiming, string_compose ("Super Rapid Exec: %1", super_rapid.timing_exec_data.summary()));
		DEBUG_TIMING_RESET(PBD::DEBUG::GUITiming, super_rapid.timing_interval_data);
		DEBUG_TIMING_RESET(PBD::DEBUG::GUITiming, super_rapid.timing_exec_data);
#endif
	}

	void debug_fps_timer () {
		DEBUG_TRACE(PBD::DEBUG::GUITiming, string_compose ("FPS Connections: %1\n", fps.connection_count ()));

		fps_eps_count.push_back (fps.timing_exec_data.size());

		DEBUG_TRACE(PBD::DEBUG::GUITiming, string_compose ("FPS Exec Totals: %1", PBD::timing_summary (fps_eps_count)));

		DEBUG_TRACE(PBD::DEBUG::GUITiming, string_compose ("FPS Interval: %1", fps.timing_interval_data.summary()));
		DEBUG_TRACE(PBD::DEBUG::GUITiming, string_compose ("FPS Exec: %1", fps.timing_exec_data.summary()));
		DEBUG_TIMING_RESET(PBD::DEBUG::GUITiming, fps.timing_interval_data);
		DEBUG_TIMING_RESET(PBD::DEBUG::GUITiming, fps.timing_exec_data);
	}

	void on_second_timer () {
		debug_rapid_timer ();
		debug_super_rapid_timer ();
		debug_fps_timer ();
	}
#endif
};

UITimers&
get_timers ()
{
	static UITimers timers;
	return timers;
}

} // anon namespace

namespace Timers {

sigc::connection
blink_connect(const sigc::slot<void,bool>& slot)
{
	return get_timers().blink.connect (slot);
}

sigc::connection
second_connect(const sigc::slot<void>& slot)
{
	return get_timers().second.connect (slot);
}

sigc::connection
rapid_connect(const sigc::slot<void>& slot)
{
	return get_timers().rapid.connect (slot);
}

sigc::connection
super_rapid_connect(const sigc::slot<void>& slot)
{
#ifdef PLATFORM_WINDOWS
	return get_timers().fps.connect (slot);
#else
	return get_timers().super_rapid.connect (slot);
#endif
}

void
set_fps_interval (unsigned int interval)
{
	get_timers().fps.set_interval (interval);
}

sigc::connection
fps_connect(const sigc::slot<void>& slot)
{
	return get_timers().fps.connect (slot);
}

TimerSuspender::TimerSuspender ()
{
	if (g_atomic_int_add (&get_timers()._suspend_counter, 1) == 0) {
		get_timers().rapid.suspend();
		get_timers().super_rapid.suspend();
		get_timers().fps.suspend();
	}
}

TimerSuspender::~TimerSuspender ()
{
	if (g_atomic_int_dec_and_test (&get_timers()._suspend_counter)) {
		get_timers().rapid.resume();
		get_timers().super_rapid.resume();
		get_timers().fps.resume();
	}
}

} // namespace Timers
