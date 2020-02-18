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

#ifndef _ardour_surfaces_m2map_mk2_h_
#define _ardour_surfaces_m2map_mk2_h_

#include "m2controls.h"

namespace ArdourSurface {

class M2MapMk2 : public M2Contols
{
	public:
		M2MapMk2 ();

		M2ButtonInterface*  button  (PhysicalButtonId id, Modifier m);
		M2ButtonInterface*  button  (SemanticButtonId id);
		M2EncoderInterface* encoder (unsigned int id);
		M2PadInterface*     pad     (unsigned int id);

	private:
		PhysicalMap pmap[2]; // 2: Modifiers
		SematicMap  smap;

		M2Button tr[5]; // transport controlables
		M2StatelessButton ts[6]; // transport pushbuttons

		M2Button mst[4]; // master "volume", "swing", "tempo", "encoder-push"

		M2Button save;

		M2Button undoredo[2];
		M2Button sm[2]; // solo, mute
		M2StatelessButton panic;

		M2Encoder enc_master;
		M2Encoder enc_top[8];

		M2Pad pads[16];
};

} /* namespace */
#endif
