/*
 * Copyright (C) 2015-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015 Tim Mayberry <mojofunk@gmail.com>
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

#ifndef ARDOUR_DSP_LOAD_CALCULATOR_H
#define ARDOUR_DSP_LOAD_CALCULATOR_H

#include <glib.h>
#include <stdlib.h>
#include <stdint.h>
#include <cassert>
#include <algorithm>

namespace ARDOUR {

class DSPLoadCalculator {
public:
	DSPLoadCalculator()
	    : m_max_time_us(0)
	    , m_start_timestamp_us(0)
	    , m_stop_timestamp_us(0)
	    , m_alpha(0)
	    , m_dsp_load(0)
	{
		m_calc_avg_load = NULL != g_getenv("ARDOUR_AVG_DSP_LOAD");
	}

	void reset () {
		m_dsp_load = 0;
		m_start_timestamp_us = 0;
		m_stop_timestamp_us = 0;
	}

	void set_max_time(double samplerate, uint32_t period_size) {
		m_max_time_us = period_size * 1e6 / samplerate;
		m_alpha = 0.2f * (m_max_time_us * 1e-6f);
	}

	void set_max_time_us(uint64_t max_time_us) {
		assert(max_time_us != 0);
		m_max_time_us = max_time_us;
		m_alpha = 0.2f * (m_max_time_us * 1e-6f);
	}

	int64_t get_max_time_us() const { return m_max_time_us; }

	void set_start_timestamp_us(int64_t start_timestamp_us) {
		m_start_timestamp_us = start_timestamp_us;
	}

	void set_stop_timestamp_us(int64_t stop_timestamp_us)
	{
		m_stop_timestamp_us = stop_timestamp_us;

		/* querying the performance counter can fail occasionally (-1).
		 * Also on some multi-core systems, timers are CPU specific and not
		 * synchronized. We assume they differ more than a few milliseconds
		 * (4 * nominal cycle time) and simply ignore cases where the
		 * execution switches cores.
		 */
		if (m_start_timestamp_us < 0 || m_stop_timestamp_us < 0 ||
		    m_start_timestamp_us > m_stop_timestamp_us ||
		    elapsed_time_us() > max_timer_error_us()) {
			return;
		}
		assert (m_max_time_us > 0);

		const float load = (float) elapsed_time_us() / (float)m_max_time_us;
		if ((m_calc_avg_load && load > .95f) || (!m_calc_avg_load && (load > m_dsp_load || load > 1.f))) {
			m_dsp_load = load;
		} else {
			m_dsp_load = std::min (1.f, m_dsp_load);
			m_dsp_load += m_alpha * (load - m_dsp_load) + 1e-12;
		}
	}

	int64_t elapsed_time_us()
	{
		return m_stop_timestamp_us - m_start_timestamp_us;
	}

	/**
	 * @return a decimal value between 0.0 and 1.0 representing the percentage
	 * of time spent between start and stop in proportion to the max expected time
	 * in microseconds(us).
	 */
	float get_dsp_load() const
	{
		assert (m_dsp_load >= 0.f); // since stop > start is assured this cannot happen.
		return std::min (1.f, m_dsp_load);
	}

	/**
	 * @return an unbound value representing the percentage of time spent between
	 * start and stop in proportion to the max expected time in microseconds(us).
	 * This is useful for cases to estimate overload (e.g. Dummy backend)
	 */
	float get_dsp_load_unbound() const
	{
		assert (m_dsp_load >= 0.f);
		return m_dsp_load;
	}

	/**
	 * The maximum error in timestamp values that will be tolerated before the
	 * current dsp load sample will be ignored
	 */
	int64_t max_timer_error_us() { return 4 * m_max_time_us; }

private: // data
	bool    m_calc_avg_load;
	int64_t m_max_time_us;
	int64_t m_start_timestamp_us;
	int64_t m_stop_timestamp_us;
	float m_alpha;
	float m_dsp_load;
};

} // namespace ARDOUR

#endif // ARDOUR_DSP_LOAD_CALCULATOR_H
