/*
 * Copyright (C) 2013-2014 Robin Gareus <robin@gareus.org>
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

#include "ardour/mididm.h"
#include "ardour/port_engine.h"

using namespace ARDOUR;

MIDIDM::MIDIDM (framecnt_t sample_rate)
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


int MIDIDM::process (pframes_t nframes, PortEngine &pe, void *midi_in, void *midi_out)
{
	/* send midi event */
	uint8_t obuf[3];
	obuf[0] = 0xf2;
	obuf[1] = (_monotonic_cnt)      & 0x7f;
	obuf[2] = (_monotonic_cnt >> 7) & 0x7f;

	pe.midi_clear(midi_out);
	pe.midi_event_put (midi_out, 0, obuf, 3);

	/* process incoming */
	const pframes_t nevents = pe.get_midi_event_count (midi_in);
#if 1 //DEBUG
		printf("MIDI SEND: @%8"PRId64", recv: %d systime:%"PRId64"\n", _monotonic_cnt, nevents, g_get_monotonic_time());
#endif
	for (pframes_t n = 0; n < nevents; ++n) {
		pframes_t timestamp;
		size_t size;
		uint8_t* buf;
		pe.midi_event_get (timestamp, size, &buf, midi_in, n);

		if (size != 3 || buf[0] != 0xf2 ) continue;

		_last_signal_tme = _monotonic_cnt;

		/* calculate time difference */
#define MODX (16384)  // 1<<(2*7)
#define MASK (0x3fff) // MODX - 1
		const int64_t tc = (_monotonic_cnt + timestamp) & MASK;
		const int64_t ti = ((buf[2] & 0x7f) << 7) | (buf[1] & 0x7f);
		const int64_t tdiff = (MODX + tc - ti) % MODX;
#if 1 //DEBUG
		printf("MIDI DELAY: # %5"PRId64" %5"PRId64" [samples] (%5"PRId64" - %8"PRId64") @(%5"PRId64" + %d)\n",
				_cnt_total, tdiff, tc, ti, _monotonic_cnt, timestamp);
#endif

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
