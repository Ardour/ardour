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

#include "ardour/dsp_load_calculator.h"

namespace ARDOUR {

void
DSPLoadCalculator::set_stop_timestamp_us(int64_t stop_timestamp_us)
{
	// We could only bother with calculations if a certain amount of time
	// has passed, or the Raw DSP value is > X% different than last calc
	// which would mean consistent overhead for small values of m_max_time_us

	m_stop_timestamp_us = stop_timestamp_us;

	/* querying the performance counter can fail occasionally (-1).
	 * Also on some multi-core systems, timers are CPU specific and not
	 * synchronized. We assume they differ more than a few milliseconds
	 * (4 * nominal cycle time) and simply ignore cases where the
	 * execution switches cores.
	 */
	if (m_start_timestamp_us < 0 || m_stop_timestamp_us < 0 ||
	    m_start_timestamp_us > m_stop_timestamp_us ||
	    elapsed_time_us() > max_timer_error()) {
		   return;
	}

	float load = 0;

	if (elapsed_time_us() > m_max_time_us) {
		load = 1.0f;
	} else {
		load = elapsed_time_us() / (float)m_max_time_us;
	}

	assert(m_value_history.write_space() >= 1);

	// push raw load value onto history
	m_value_history.write(&load, 1);

	// if load is under 80% use an average of past values
	if (elapsed_time_us() < ((m_max_time_us * 80) / 100)) {

		RingBuffer<float>::rw_vector vec;
		m_value_history.get_read_vector(&vec);
		uint32_t values_read = 0;
		float dsp_accumulator = 0.0f;

		// iterate through the read vectors accumulating the dsp load
		for (unsigned int i = 0; i < vec.len[0]; ++i) {
			dsp_accumulator += vec.buf[0][i];
			values_read++;
		}

		for (unsigned int i = 0; i < vec.len[1]; ++i) {
			dsp_accumulator += vec.buf[1][i];
			values_read++;
		}

		load = dsp_accumulator / (float)values_read;

		const float alpha = 0.2f * (m_max_time_us * 1e-6f);
		m_dsp_load = m_dsp_load + alpha * (load - m_dsp_load) + 1e-12;

	} else {
		// Use raw load value otherwise 100% may never be indicated because of
		// averaging/LPF etc
		m_dsp_load = load;
	}

	if (m_value_history.read_space() >= m_num_values) {
		// "remove" the oldest value
		m_value_history.increment_read_idx(1);
	}
}

} // namespace ARDOUR
