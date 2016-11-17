/*
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "m2_map_mikro.h"

using namespace ArdourSurface;

M2MapMikro::M2MapMikro ()
	: M2Contols ()
	, enc_master (16)
{}

M2ButtonInterface*
M2MapMikro::button (PhysicalButtonId id, Modifier m)
{
	return M2Contols::button (id, m);
}

M2ButtonInterface*
M2MapMikro::button (SemanticButtonId id)
{
	return M2Contols::button (id);
}

M2EncoderInterface*
M2MapMikro::encoder (unsigned int id)
{
	if (id == 0) {
		return &enc_master;
	}
	// TODO map "nav" (select) and Left/Right to encoder(s) delta.
	return M2Contols::encoder (id);
}

M2PadInterface*
M2MapMikro::pad (unsigned int id)
{
	if (id < 16) {
		return &pads[id];
	}
	return M2Contols::pad (id);
}
