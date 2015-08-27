/*
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef DSP_LOAD_CALCULATOR_H
#define DSP_LOAD_CALCULATOR_H

#include <stdint.h>

class DSPLoadCalculator {
public:
	DSPLoadCalculator()
	    : m_max_time_us(0)
	    , m_start_timestamp_us(0)
	    , m_stop_timestamp_us(0)
	    , m_dsp_load(0.0f)
	{
	}

	void set_max_time_us(uint64_t max_time_us) { m_max_time_us = max_time_us; }

	uint64_t get_max_time_us() { return m_max_time_us; }

	void set_start_timestamp_us(uint64_t start_timestamp_us)
	{
		m_start_timestamp_us = start_timestamp_us;
	}

	void set_stop_timestamp_us(uint64_t stop_timestamp_us)
	{
		m_stop_timestamp_us = stop_timestamp_us;

		if (elapsed_time_us() > m_max_time_us) {
			m_dsp_load = 1.0f;
		} else {
			const float load = elapsed_time_us() / (float)m_max_time_us;
			if (load > m_dsp_load) {
				m_dsp_load = load;
			} else {
				const float alpha = 0.2f * (m_max_time_us * 1e-6f);
				m_dsp_load = m_dsp_load + alpha * (load - m_dsp_load) + 1e-12;
			}
		}
	}

	uint64_t elapsed_time_us()
	{
		return m_stop_timestamp_us - m_start_timestamp_us;
	}

	/**
	 * @return a decimal value between 0.0 and 1.0 representing the percentage
	 * of time spent between start and stop in proportion to the max expected time
	 * in microseconds(us).
	 */
	float get_dsp_load()
	{
		if (m_dsp_load > m_max_time_us) {
			return 1.0f;
		}
		if (m_dsp_load < 0.0f) {
			return 0.0f;
		}
		return m_dsp_load;
	}

private:
	uint64_t m_max_time_us;
	uint64_t m_start_timestamp_us;
	uint64_t m_stop_timestamp_us;
	float m_dsp_load;
};

#endif // DSP_LOAD_CALCULATOR_H
