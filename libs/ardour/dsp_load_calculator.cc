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

} // namespace ARDOUR
