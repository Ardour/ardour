/*
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
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

#ifndef _ardour_surfaces_m2pad_h_
#define _ardour_surfaces_m2pad_h_

#include <stdint.h>
#include "pbd/signals.h"

namespace ArdourSurface {

class M2PadInterface
{
	public:
		M2PadInterface () {}
		virtual ~M2PadInterface () {}

		/* user API */
		PBD::Signal1<void, float> pressed;
		PBD::Signal0<void> released;
		PBD::Signal1<void, float> aftertouch;
		PBD::Signal2<void, float, bool> event;
		PBD::Signal1<void, float> changed;

		virtual uint16_t value () const { return 0; }
		virtual float pressure () const { return 0.f; }
		virtual void set_color (uint32_t rgba) {}

		/* internal API - called from device thread */
		virtual void set_value (uint16_t v) {}

		virtual void color (uint8_t& r, uint8_t& g, uint8_t& b) const {
			r = g = b = 0;
		}
};

class M2Pad : public M2PadInterface
{
	public:
		M2Pad ()
			: M2PadInterface ()
			, _pressed (false)
			, _pressure (0)
			, _last (0)
			, _cnt (0)
			, _rgba (0)
		{
			for (int i = 0; i < 4; ++i) {
				hist[i] = 0;
			}
		}

		uint16_t value () const { return _raw; }
		float pressure () const { return _pressure; }

		void set_color (uint32_t rgba) { _rgba = rgba; }

		void color (uint8_t& r, uint8_t& g, uint8_t& b) const
		{
			r = ((_rgba >> 24) & 0xff) >> 1;
			g = ((_rgba >> 16) & 0xff) >> 1;
			b = ((_rgba >>  8) & 0xff) >> 1;
		}

		void set_value (uint16_t v)
		{
			// bleed to neighboring pads...
			static const uint16_t high  = 159;
			static const float    low   = 159 / 4095.f;
			static const float mindelta = 32.f / 4096.f;

			if (_raw != v) {
				changed (v / 4095.f);
				_raw = v;
			}

			// some pads never return to "0", and there's
			// TODO map pressure from a min..max range,
			// even hard hits rarely exceed 3400 or thereabouts.
			// -> "pad sensitivity" config or "calibrate pads"

			hist[_cnt] = v;
			_cnt = (_cnt + 1) & 3;

			if (_pressed) {
				const float p = v / 4095.f;
				_pressure += .1 * (p - _pressure);
				if (_pressure < low) {
					_pressure = 0;
					_pressed = false;
					released (); /* EMIT SIGNAL */
					event (_pressure, true); /* EMIT SIGNAL */
				} else {
					if (fabsf (_last - _pressure) > mindelta) {
						_last = _pressure;
						aftertouch (_pressure); /* EMIT SIGNAL */
						event (_pressure, false); /* EMIT SIGNAL */
					}
				}
			} else {
				bool above_thresh = true;
				uint16_t max = 0;
				for (int i = 0; i < 4; ++i) {
					if (hist[i] < high) {
						above_thresh = false;
						break;
					}
					max = std::max (max, hist[i]);
				}
				if (above_thresh) {
					_pressed = true;
					_last = _pressure = max / 4095.f;
					pressed (_pressure);
					event (_pressure, true); /* EMIT SIGNAL */
				}
			}
		}

	protected:
		bool  _pressed;
		float _pressure;
		uint16_t _raw;
		float _last;
		uint16_t hist[4];
		unsigned int _cnt;
		uint32_t _rgba;
};

} /* namespace */
#endif /* _ardour_surfaces_m2pad_h_ */



