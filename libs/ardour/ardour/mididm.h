/*
 * Copyright (C) 2014-2017 Robin Gareus <robin@gareus.org>
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

#ifndef __libardour_mididm_h__
#define __libardour_mididm_h__

#include "ardour/types.h"
#include "ardour/libardour_visibility.h"

namespace ARDOUR {

class PortEngine;

class LIBARDOUR_API MIDIDM
{
public:

	MIDIDM (samplecnt_t sample_rate);

	int process (pframes_t nframes, PortEngine &pe, void *midi_in, void *midi_out);

	samplecnt_t latency (void) { return _cnt_total > 10 ? _avg_delay : 0; }
	samplecnt_t processed (void) { return _cnt_total; }
	double     deviation (void) { return _cnt_total > 1 ? sqrt(_var_s / ((double)(_cnt_total - 1))) : 0; }
	bool       ok (void) { return _cnt_total > 200; }
	bool       have_signal (void) { return (_monotonic_cnt - _last_signal_tme) < (uint64_t) _sample_rate ; }

private:
	int64_t parse_mclk (uint8_t const * const buf, pframes_t timestamp) const;
	int64_t parse_mtc  (uint8_t const * const buf, pframes_t timestamp) const;

	samplecnt_t _sample_rate;

	uint64_t _monotonic_cnt;
	uint64_t _last_signal_tme;

	uint64_t _cnt_total;
	uint64_t _dly_total;
	uint32_t _min_delay;
	uint32_t _max_delay;
	double   _avg_delay;
	double   _var_m;
	double   _var_s;

};

}

#endif /* __libardour_mididm_h__ */
