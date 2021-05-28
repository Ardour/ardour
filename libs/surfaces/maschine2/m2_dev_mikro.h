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

#ifndef _ardour_surfaces_m2mikro_h_
#define _ardour_surfaces_m2mikro_h_

#include "m2device.h"

#include <cairomm/context.h>
#include <pangomm/layout.h>

namespace ArdourSurface {

class Maschine2Mikro : public M2Device
{
	public:
		Maschine2Mikro ();
		void clear (bool splash = false);
		void read (hid_device*, M2Contols*);
		void write (hid_device*, M2Contols*);
		Cairo::RefPtr<Cairo::ImageSurface> surface () { return _surface; }

	private:

#if defined(__GNUC__)
#define ATTRIBUTE_PACKED  __attribute__((__packed__))
#else
#define ATTRIBUTE_PACKED
#pragma pack(1)
#endif

		struct machine_mk2_input {
			unsigned int trs_shift       : 1; // 0
			unsigned int trs_erase       : 1;
			unsigned int trs_rec         : 1;
			unsigned int trs_play        : 1;
			unsigned int trs_grid        : 1;
			unsigned int trs_right       : 1;
			unsigned int trs_left        : 1;
			unsigned int trs_restart     : 1;
			unsigned int group           : 1; // 8
			unsigned int browse          : 1;
			unsigned int sampling        : 1;
			unsigned int note_repeat     : 1;
			unsigned int mst_wheel       : 1;
			unsigned int reserved        : 3;
			unsigned int f1              : 1; // 16
			unsigned int f2              : 1;
			unsigned int f3              : 1;
			unsigned int control         : 1;
			unsigned int navigate        : 1;
			unsigned int nav_left        : 1;
			unsigned int nav_right       : 1;
			unsigned int main            : 1;
			unsigned int pads_mute       : 1; // 24
			unsigned int pads_solo       : 1;
			unsigned int pads_select     : 1;
			unsigned int pads_duplicate  : 1;
			unsigned int pads_navigate   : 1;
			unsigned int pads_mode       : 1;
			unsigned int pads_pattern    : 1;
			unsigned int pads_scene      : 1; // 31
			unsigned int mst_wheel_pos   : 8; // 32..40 // range: 0..15
		} ATTRIBUTE_PACKED ctrl_in;

#if (!defined __GNUC__)
#pragma pack()
#endif
		uint16_t pad[16];

		Cairo::RefPtr<Cairo::ImageSurface> _surface;

	private:
		void assign_controls (M2Contols*) const;

		void set_lights (M2Contols*, uint8_t*) const;
		void set_pads (M2Contols*, uint8_t*) const;

		uint8_t _lights[79];
		uint8_t _img[4][265];
};
} /* namespace */

#endif
