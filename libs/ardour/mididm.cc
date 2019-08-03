/*
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
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

#include "ardour/mididm.h"
#include "ardour/port_engine.h"

using namespace ARDOUR;

MIDIDM::MIDIDM (samplecnt_t sample_rate)
  : _sample_rate (sample_rate)
  , _monotonic_cnt (sample_rate)
  , _last_signal_tme (0)
  , _cnt_total (0)
  , _dly_total (0)
  , _min_delay (INT32_MAX)
  , _max_delay (0)
  , _avg_delay (0)
  , _var_m (0)
  , _var_s (0)
{

}

int64_t
MIDIDM::parse_mclk (uint8_t const * const buf, pframes_t timestamp) const
{
	/* calculate time difference */
#define MODCLK (16384)  // 1<<(2*7)
	const int64_t tc = (_monotonic_cnt + timestamp) & 0x3fff; // MODCLK - 1;
	const int64_t ti = ((buf[2] & 0x7f) << 7) | (buf[1] & 0x7f);
	const int64_t tdiff = (MODCLK + tc - ti) % MODCLK;
#ifdef DEBUG_MIDIDM
		printf("MCLK DELAY: #%5"PRId64" dt:%6"PRId64" [spl] (%6"PRId64" - %8"PRId64") @(%8"PRId64" + %d)\n",
				_cnt_total, tdiff, tc, ti, _monotonic_cnt, timestamp);
#endif
	return tdiff;
}

int64_t
MIDIDM::parse_mtc (uint8_t const * const buf, pframes_t timestamp) const
{
#define MODTC (2097152)  // 1<<(3*7)
	const int64_t tc = (_monotonic_cnt + timestamp) & 0x001FFFFF;
	const int64_t ti = (buf[5] & 0x7f)
		| ((buf[6] & 0x7f) << 7)
		| ((buf[7] & 0x7f) << 14)
		| ((buf[8] & 0x7f) << 21);
	const int64_t tdiff = (MODTC + tc - ti) % MODTC;
#ifdef DEBUG_MIDIDM
		printf("MTC DELAY: #%5"PRId64" dt:%6"PRId64" [spl] (%6"PRId64" - %8"PRId64") @(%8"PRId64" + %d)\n",
				_cnt_total, tdiff, tc, ti, _monotonic_cnt, timestamp);
#endif
	return tdiff;
}

int MIDIDM::process (pframes_t nframes, PortEngine &pe, void *midi_in, void *midi_out)
{
	/* send midi event */
	pe.midi_clear(midi_out);
#ifndef USE_MTC // use 3-byte song position
	uint8_t obuf[3];
	obuf[0] = 0xf2;
	obuf[1] = (_monotonic_cnt)      & 0x7f;
	obuf[2] = (_monotonic_cnt >> 7) & 0x7f;
	pe.midi_event_put (midi_out, 0, obuf, 3);
#else // sysex MTC frame
	uint8_t obuf[10];
	obuf[0] = 0xf0;
	obuf[1] = 0x7f;
	obuf[2] = 0x7f;
	obuf[3] = 0x01;
	obuf[4] = 0x01;
	obuf[9] = 0xf7;
	obuf[5] = (_monotonic_cnt      ) & 0x7f;
	obuf[6] = (_monotonic_cnt >>  7) & 0x7f;
	obuf[7] = (_monotonic_cnt >> 14) & 0x7f;
	obuf[8] = (_monotonic_cnt >> 21) & 0x7f;
	pe.midi_event_put (midi_out, 0, obuf, 10);
#endif

	/* process incoming */
	const pframes_t nevents = pe.get_midi_event_count (midi_in);
#ifdef DEBUG_MIDIDM
		printf("MIDI SEND: @%8"PRId64", recv: %d systime:%"PRId64"\n", _monotonic_cnt, nevents, g_get_monotonic_time());
#endif
	for (pframes_t n = 0; n < nevents; ++n) {
		pframes_t timestamp;
		size_t size;
		uint8_t const* buf;
		int64_t tdiff;
		pe.midi_event_get (timestamp, size, &buf, midi_in, n);

		if (size == 3 && buf[0] == 0xf2 )
		{
			tdiff = parse_mclk(buf, timestamp);
		} else if (size == 10 && buf[0] == 0xf0)
		{
			tdiff = parse_mtc(buf, timestamp);
		}
		else
		{
			continue;
		}

		_last_signal_tme = _monotonic_cnt;

		/* running variance */
		if (_cnt_total == 0) {
			_var_m = tdiff;
		} else {
			const double var_m1 = _var_m;
			_var_m = _var_m + ((double)tdiff - _var_m) / (double)(_cnt_total + 1);
			_var_s = _var_s + ((double)tdiff - _var_m) * ((double)tdiff - var_m1);
		}
		/* average and mix/max */
		++_cnt_total;
		_dly_total += tdiff;
		_avg_delay = _dly_total / _cnt_total;
		if (tdiff < _min_delay) _min_delay = tdiff;
		if (tdiff > _max_delay) _max_delay = tdiff;
	}

  _monotonic_cnt += nframes;
	return 0;
}
