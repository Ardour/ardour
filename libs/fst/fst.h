/*
 * Copyright (C) 2006-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2010-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014 Robin Gareus <robin@gareus.org>
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

#ifndef __fst_fst_h__
#define __fst_fst_h__

#include <setjmp.h>
#include <signal.h>
#include <pthread.h>

#include "ardour/libardour_visibility.h"
#include "ardour/vst_types.h"
#include "ardour/vestige/vestige.h"

#ifdef __cplusplus
extern "C" {
#endif

LIBARDOUR_API int        fst_init (void* possible_hmodule);
LIBARDOUR_API void       fst_exit (void);

LIBARDOUR_API VSTHandle* fst_load (const char*);
LIBARDOUR_API int        fst_unload (VSTHandle**);

LIBARDOUR_API VSTState * fst_instantiate (VSTHandle *, audioMasterCallback amc, void* userptr);
LIBARDOUR_API void       fst_close (VSTState *);

LIBARDOUR_API int  fst_run_editor (VSTState *, void* window_parent);
LIBARDOUR_API void fst_destroy_editor (VSTState *);
LIBARDOUR_API void fst_move_window_into_view (VSTState *);

LIBARDOUR_API void fst_event_loop_remove_plugin (VSTState* fst);
LIBARDOUR_API void fst_audio_master_idle(void);

#ifdef __cplusplus
}
#endif

#endif /* __fst_fst_h__ */
