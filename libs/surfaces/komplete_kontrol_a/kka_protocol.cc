/*
 * Copyright (C) 2026 Joshua Perry <josh@6bit.com>
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

#include <string.h>

#include "kka_protocol.h"

namespace ArdourSurface { namespace KKA {

/* The surface list in the GUI is generated from this table, in this order, so
 * adding a model here is all it takes to offer it -- and nothing can be
 * offered that the surface would then fail to recognise.  Widest first,
 * because the A61 is the most common of the three.
 */
const Variant Variants[] = {
	{ 0x1750, 61, "Komplete Kontrol A61", true  },
	{ 0x1740, 49, "Komplete Kontrol A49", false },
	{ 0x1730, 25, "Komplete Kontrol A25", false },
};

const size_t NumVariants = sizeof (Variants) / sizeof (Variants[0]);

const Variant*
variant_for_pid (uint16_t pid)
{
	for (size_t i = 0; i < NumVariants; ++i) {
		if (Variants[i].pid == pid) {
			return &Variants[i];
		}
	}
	return 0;
}

const Variant*
variant_for_model (const char* model)
{
	if (!model) {
		return 0;
	}
	for (size_t i = 0; i < NumVariants; ++i) {
		if (!strcmp (Variants[i].model, model)) {
			return &Variants[i];
		}
	}
	return 0;
}

const char*
control_name (ControlID c)
{
	/* Indexed by ControlID, which is the button bit index. */
	static const char* const names[NumControls] = {
		"Shift",
		"Scale",
		"Arp",
		"Undo",
		"Quantize",
		"Ideas",
		"Loop",
		"Metro",
		"Tempo",
		"Play",
		"Rec",
		"Stop",
		"Preset Up",
		"Preset Down",
		"M / Page Left",
		"S / Page Right",
		"Browser",
		"Plug-In",
		"Track",
		"Octave Down",
		"Octave Up",
		"4-D Up",
		"4-D Left",
		"4-D Right",
		"4-D Down",
		"Knob 1 Touch",
		"Knob 2 Touch",
		"Knob 3 Touch",
		"Knob 4 Touch",
		"Knob 5 Touch",
		"Knob 6 Touch",
		"Knob 7 Touch",
		"Knob 8 Touch",
		"4-D Press",
	};

	if ((int) c < 0 || (int) c >= NumControls) {
		return "?";
	}
	return names[c];
}

} } /* namespace ArdourSurface::KKA */
