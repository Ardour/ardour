/*
 * Copyright (C) 2004,2014 Robin Gareus <robin@gareus.org>
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

#include <stdint.h>
#include <sys/select.h>

/* select() sleeps _at most_ a given time.
 * (compared to usleep() or nanosleep() which sleep at least a given time)
 */
static void select_sleep (uint64_t usec) {
	if (usec <= 10) return;
	fd_set fd;
	int max_fd=0;
	struct timeval tv;
	tv.tv_sec = usec / 1000000;
	tv.tv_usec = usec % 1000000;
	FD_ZERO (&fd);
	select (max_fd, &fd, NULL, NULL, &tv);
	// on Linux, tv reflects the actual time slept.
}
