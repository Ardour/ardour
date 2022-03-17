/*
 * Copyright (C) 2020 Paul Davis <paul@linuxaudiosystems.com>
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

#include "temporal/superclock.h"

#ifndef COMPILER_MSVC
Temporal::superclock_t Temporal::_superclock_ticks_per_second = 56448000; /* 2^10 * 3^2 * 5^3 * 7^2 */
#endif

int Temporal::most_recent_engine_sample_rate = 48000; /* have to pick something as a default */

bool Temporal::scts_set = false;

void
Temporal::set_sample_rate (int sr)
{
	most_recent_engine_sample_rate = sr;
}

void
Temporal::set_superclock_ticks_per_second (Temporal::superclock_t sc)
{
	_superclock_ticks_per_second = sc;
	scts_set = true;
}
