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

#ifndef __mac_vst_support_h__
#define __mac_vst_support_h__

#include <setjmp.h>
#include <signal.h>
#include <pthread.h>
#include <stdio.h>

#include "ardour/libardour_visibility.h"
#include "ardour/vst_types.h"

LIBARDOUR_API extern void (*mac_vst_error_callback)(const char *msg);
LIBARDOUR_API void  mac_vst_error (const char *fmt, ...);

LIBARDOUR_API extern VSTHandle *  mac_vst_load (const char*);
LIBARDOUR_API extern int          mac_vst_unload (VSTHandle *);
LIBARDOUR_API extern VSTState *   mac_vst_instantiate (VSTHandle *, audioMasterCallback, void *);
LIBARDOUR_API extern void         mac_vst_close (VSTState*);

#endif /* ____mac_vst_support_h__ */
