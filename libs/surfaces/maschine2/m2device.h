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

#ifndef _ardour_surfaces_maschine2hardware_h_
#define _ardour_surfaces_maschine2hardware_h_

#include <hidapi.h>
#include <cairomm/refptr.h>
#include <cairomm/surface.h>
#include "pbd/signals.h"

namespace ArdourSurface {

class M2Contols;

/** Abstraction for various variants:
 *  - NI Maschine Mikro
 *  - NI Maschine
 *  - NI Maschine Studio
 */

class M2Device
{
	public:
		M2Device ()
			: _splashcnt (0)
			, _blink_counter (0)
			, _blink_shade (0.f)
			{}
		virtual ~M2Device () {}

		virtual void clear (bool splash = false) {
			if (splash) {
				_splashcnt = 0;
			} else {
				_splashcnt = _splashtime;
			}
			_blink_counter = 0;
			_blink_shade = 0.f;
		}

		virtual void read (hid_device*, M2Contols*) = 0;
		virtual void write (hid_device*, M2Contols*) = 0;
		virtual Cairo::RefPtr<Cairo::ImageSurface> surface () = 0;

		PBD::Signal0<bool> vblank;

	protected:
		void bump_blink () {
			_blink_counter = (_blink_counter + 1) % 12;
			_blink_shade = fabsf (1.f - _blink_counter / 6.f);
		}

		uint32_t _splashcnt;
		static const uint32_t _splashtime = 25 * 3;
		unsigned int _blink_counter;
		float        _blink_shade;
};

} /* namespace */
#endif /* _ardour_surfaces_maschine2_h_*/
