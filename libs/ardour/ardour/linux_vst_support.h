/*
 * Copyright (C) 2013-2015 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2013-2016 Robin Gareus <robin@gareus.org>
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

#ifndef __vstfx_h__
#define __vstfx_h__

#include <setjmp.h>
#include <signal.h>
#include <pthread.h>
#include <stdio.h>

#include "ardour/libardour_visibility.h"
#include "ardour/vst_types.h"

/******************************************************************************************/
/*VSTFX - an engine to manage native linux VST plugins - derived from FST for Windows VSTs*/
/******************************************************************************************/

LIBARDOUR_API extern void (*vstfx_error_callback)(const char *msg);
LIBARDOUR_API void  vstfx_error (const char *fmt, ...);

/*API to vstfx*/

LIBARDOUR_API extern int          vstfx_launch_editor (VSTState *);
LIBARDOUR_API extern int          vstfx_init (void *);
LIBARDOUR_API extern void         vstfx_exit ();
LIBARDOUR_API extern VSTHandle *  vstfx_load (const char*);
LIBARDOUR_API extern int          vstfx_unload (VSTHandle *);

LIBARDOUR_API extern VSTState *   vstfx_instantiate (VSTHandle *, audioMasterCallback, void *);
LIBARDOUR_API extern void         vstfx_close (VSTState*);

LIBARDOUR_API extern int          vstfx_create_editor (VSTState *);
LIBARDOUR_API extern int          vstfx_run_editor (VSTState *);
LIBARDOUR_API extern void         vstfx_destroy_editor (VSTState *);

LIBARDOUR_API extern void         vstfx_event_loop_remove_plugin (VSTState *);

#endif /* __vstfx_h__ */
