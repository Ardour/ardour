/*
 * Copyright (C) 2014-2016 Tim Mayberry <mojofunk@gmail.com>
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

#ifndef __libpbd_timing_h__
#define __libpbd_timing_h__

#include <glib.h>

#include <stdint.h>

#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "pbd/libpbd_visibility.h"

#ifdef COMPILER_MSVC
#undef min
#undef max
#endif

namespace PBD {

LIBPBD_API bool get_min_max_avg_total (const std::vector<uint64_t>& values, uint64_t& min, uint64_t& max, uint64_t& avg, uint64_t& total);

LIBPBD_API std::string timing_summary (const std::vector<uint64_t>& values);

/**
 * This class allows collecting timing data using two different
 * techniques. The first is using start() and update() and then
 * calling elapsed() to get the elapsed time. This is useful when
 * you want to measure the elapsed time between two different
 * execution points. e.g
 *
 * timing.start();
 * do_stuff();
 * timing.update();
 * cerr << "do_stuff took: "
 *      << timing.elapsed()
 *      << "usecs" << endl;
 *
 * The other is timing intervals using start() and calling
 * get_interval() periodically to measure the time intervals
 * between the same execution point. The difference is necessary
 * to get the most accurate timing information when timing
 * intervals but I didn't feel it necessary to have two separate
 * classes.
 */
class LIBPBD_API Timing
{
public:

	Timing ()
		: m_start_val(0)
		, m_last_val(0)
	{ start ();}

	bool valid () const {
		return (m_start_val != 0 && m_last_val != 0);
	}

	void start () {
		m_start_val = g_get_monotonic_time ();
		m_last_val = 0;
	}

	void update () {
		m_last_val = g_get_monotonic_time ();
	}

	void reset () {
		m_start_val = m_last_val = 0;
	}

	uint64_t get_interval () {
		uint64_t elapsed = 0;
		update ();
		if (valid()) {
			elapsed = m_last_val - m_start_val;
			m_start_val = m_last_val;
			m_last_val = 0;
		}
		return elapsed;
	}

	/// @return Elapsed time in microseconds
	uint64_t elapsed () const {
		return m_last_val - m_start_val;
	}

	/// @return Elapsed time in milliseconds
	uint64_t elapsed_msecs () const {
		return elapsed () / 1000;
	}

private:

	uint64_t m_start_val;
	uint64_t m_last_val;

};

class LIBPBD_API TimingStats : public Timing
{
public:
	TimingStats ()
	{
		/* override implicit Timing::start () */
		reset ();
	}

	void update ()
	{
		/* throw away the first 1000 values */
		if (_update_cnt++ > 1000) {
			Timing::update ();
			calc ();
		}
	}

	void reset ()
	{
		Timing::reset ();
		_min = std::numeric_limits<uint64_t>::max();
		_max = 0;
		_cnt = 0;
		_avg = 0.;
		_vm  = 0.;
		_vs  = 0.;
		_update_cnt = 0;
	}

	bool valid () const {
		return Timing::valid () && _cnt > 1;
	}

	bool get_stats (uint64_t& min,
	                uint64_t& max,
	                double& avg,
	                double& dev) const
	{
		if (_cnt < 2) {
			return false;
		}
		min = _min;
		max = _max;
		avg = _avg / (double)_cnt;
		dev = sqrt (_vs / (_cnt - 1.0));
		return true;
	}

private:
	void calc ()
	{
		const uint64_t diff = elapsed ();

		_avg += diff;

		if (diff > _max) {
			_max = diff;
		}
		if (diff < _min) {
			_min = diff;
		}

		if (_cnt == 0) {
			_vm = diff;
		} else {
			const double ela = diff;
			const double var_m1 = _vm;
			_vm = _vm + (ela - _vm) / (1.0 + _cnt);
			_vs = _vs + (ela - _vm) * (ela - var_m1);
		}
		++_cnt;
	}

	uint64_t _cnt;
	uint64_t _min;
	uint64_t _max;
	double   _avg;
	double   _vm;
	double   _vs;
	uint64_t   _update_cnt;
};

class LIBPBD_API TimerRAII
{
  public:
	TimerRAII (TimingStats& ts) : stats (ts) { stats.start(); }
	~TimerRAII() { stats.update(); }
	TimingStats& stats;
};

class LIBPBD_API TimingData
{
public:
	TimingData () : m_reserve_size(256)
	{ reset (); }

	void start_timing () {
		m_timing.start ();
	}

	void add_elapsed () {
		m_timing.update ();
		if (m_timing.valid()) {
			m_elapsed_values.push_back (m_timing.elapsed());
		}
	}

	void add_interval () {
		uint64_t interval = m_timing.get_interval ();
		m_elapsed_values.push_back (interval);
	}

	void reset () {
		m_elapsed_values.clear ();
		m_elapsed_values.reserve (m_reserve_size);
	}

	std::string summary () const
	{ return timing_summary (m_elapsed_values); }

	bool get_min_max_avg_total (uint64_t& min,
	                            uint64_t& max,
	                            uint64_t& avg,
	                            uint64_t& total) const
	{ return PBD::get_min_max_avg_total (m_elapsed_values, min, max, avg, total); }

	void reserve (uint32_t reserve_size)
	{ m_reserve_size = reserve_size; reset (); }

	uint32_t size () const
	{ return m_elapsed_values.size(); }

private:

	Timing m_timing;

	uint32_t m_reserve_size;

	std::vector<uint64_t> m_elapsed_values;
};

class LIBPBD_API Timed
{
public:
	Timed (TimingData& data)
		: m_data(data)
	{
		m_data.start_timing ();
	}

	~Timed ()
	{
		m_data.add_elapsed ();
	}

private:

	TimingData& m_data;

};

} // namespace PBD

#endif // __libpbd_timing_h__
