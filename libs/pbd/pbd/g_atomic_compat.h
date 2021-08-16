/*
 * Copyright (C) 2021 Robin Gareus <robin@gareus.org>
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

#ifndef _PBD_G_ATOMIC_COMPAT_H_
#define _PBD_G_ATOMIC_COMPAT_H_

/* requires for gint, guint, gpointer */
#include <glib.h>

/* This is to for g_atomic_* compatibility with glib >= 2.68 and gcc-11
 *
 * "While atomic has a volatile qualifier, this is a historical artifact and the pointer passed to it should not be volatile."
 * (https://developer.gnome.org/glib/2.68/glib-Atomic-Operations.html)
 *
 * Older versions of glib and older compilers still expect a volatile qualifier and print
 * "cast from type 'volatile long int*' to type 'long int*' casts away qualifiers [-Wcast-qual]"
 */
#if defined HAVE_GLIB_2_64 && (defined(__cplusplus) && __cplusplus >= 201103L)
#  define GATOMIC_QUAL
#else
#  define GATOMIC_QUAL volatile
#endif

#endif
