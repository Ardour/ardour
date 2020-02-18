/*
 * Copyright (C) 2009-2015 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __pbd_malign_h__
#define __pbd_malign_h__

#include <stdlib.h>

#include "pbd/libpbd_visibility.h"

LIBPBD_API int  cache_aligned_malloc (void** memptr, size_t size);
LIBPBD_API void cache_aligned_free (void* memptr);

LIBPBD_API int  aligned_malloc (void** memptr, size_t size, size_t alignment);
LIBPBD_API void aligned_free (void* memptr);

#endif /* __pbd_malign_h__ */
