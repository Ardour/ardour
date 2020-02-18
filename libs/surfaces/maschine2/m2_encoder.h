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

#ifndef _ardour_surfaces_m2encoder_h_
#define _ardour_surfaces_m2encoder_h_

#include <stdint.h>
#include "pbd/signals.h"

namespace ArdourSurface {

class M2EncoderInterface
{
	public:
		M2EncoderInterface () {}
		virtual ~M2EncoderInterface () {}

		/* user API */
		PBD::Signal1<void, int> changed;
		virtual float value () const { return 0.f; }
		virtual float range () const { return 0.f; }

		/* internal API - called from device thread */
		virtual bool set_value (unsigned int v) { return false; }
};

class M2Encoder : public M2EncoderInterface
{
	public:
		M2Encoder (unsigned int upper = 1000)
			: M2EncoderInterface ()
			, _upper (upper /* limit, exclusive. eg [0..15]: 16 */)
			, _value (0)
			, _initialized (false)
		{
			assert (_upper > 7);
			_wrapcnt = std::max (3U, upper / 6);
		}

		float value () const { return _value / (_upper - 1.f); }
		float range () const { return (_upper - 1.f); }

		bool set_value (unsigned int v) {
			if (!_initialized) {
				_initialized = true;
				_value = v;
				return false;
			}

			if (v == _value) {
				return false;
			}

			int delta;
			if (v < _wrapcnt && _value > _upper - _wrapcnt) {
				// wrap around max -> min
				delta = v + _upper - _value;
			}
			else if (_value < _wrapcnt && v > _upper - _wrapcnt) {
				// wrap around min -> max
				delta = v - _upper - _value;
			}
			else {
				delta = v - _value;
			}

			_value = v;
			changed (delta);
			return true;
		}

	protected:
		unsigned int _upper;
		unsigned int _value;
		unsigned int _wrapcnt;
		bool _initialized;
};

} /* namespace */
#endif /* _ardour_surfaces_m2encoder_h_ */


