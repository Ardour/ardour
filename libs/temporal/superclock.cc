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

#include <iostream>

#include <sys/syscall.h>
#include <sys/types.h>

#include "pbd/pthread_utils.h"

#include "temporal/superclock.h"

Temporal::superclock_t Temporal::superclock_ticks_per_second = 508032000; // 2^10 * 3^4 * 5^3 * 7^2

int (*Temporal::sample_rate_callback)() = 0;

void
Temporal::set_sample_rate_callback (int (*func)())
{
	sample_rate_callback = func;
}
