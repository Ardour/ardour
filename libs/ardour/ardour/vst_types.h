/*
 * Copyright (C) 2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2017 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_vst_types_h__
#define __ardour_vst_types_h__

#include <pthread.h>
#include "ardour/libardour_visibility.h"
#include "ardour/vestige/vestige.h"

#ifdef MACVST_SUPPORT
#include <Carbon/Carbon.h>

/* fix up stupid apple macros */
#undef check
#undef require
#undef verify

#ifdef YES
#undef YES
#endif
#ifdef NO
#undef NO
#endif

#endif

struct LIBARDOUR_API _VSTKey
{
	/** virtual-key code, or 0 if this _VSTFXKey is a `character' key */
	int special;
	/** `character' key, or 0 if this _VSTFXKey is a virtual-key */
	int character;
};

typedef struct _VSTKey VSTKey;

typedef AEffect * (* main_entry_t) (audioMasterCallback);

struct LIBARDOUR_API _VSTHandle
{
#ifdef MACVST_SUPPORT
	CFBundleRef    bundleRef;
	CFBundleRefNum res_file_id;
#else
	void* dll;
#endif

	char*        name;
	char*        path;
	main_entry_t main_entry;
	int          plugincnt;
};

typedef struct _VSTHandle VSTHandle;

struct LIBARDOUR_API _VSTState
{
	AEffect*    plugin;
	VSTHandle*  handle;
	audioMasterCallback amc;

	void*       gtk_window_parent;
	int         xid;                     ///< X11 XWindow (wine + lxvst)

	/* LXVST/X11 */
	int         linux_window;            ///< The plugin's parent X11 XWindow
	int         linux_plugin_ui_window;  ///< The ID of the plugin UI window created by the plugin
	void  (* eventProc) (void * event);  ///< X11 UI _XEventProc

	/* Windows */
	void*       windows_window;


	int width;
	int height;
	int wantIdle;

	int voffset;
	int hoffset;
	int gui_shown;
	int destroy;
	int vst_version;
	int has_editor;

	int     program_set_without_editor;
	int     want_program;
	int     want_chunk;
	int     n_pending_keys;
	unsigned char* wanted_chunk;
	int     wanted_chunk_size;
	float*  want_params;
	float*  set_params;

	VSTKey  pending_keys[16];

	int     dispatcher_wantcall;
	int     dispatcher_opcode;
	int     dispatcher_index;
	int     dispatcher_val;
	void*   dispatcher_ptr;
	float   dispatcher_opt;
	int     dispatcher_retval;

	struct _VSTState* next;
	pthread_mutex_t   lock;
	pthread_mutex_t   state_lock;
	pthread_cond_t    window_status_change;
	pthread_cond_t    plugin_dispatcher_called;
	pthread_cond_t    window_created;
	int               been_activated;
};

typedef struct _VSTState VSTState;

#ifdef __cplusplus
extern "C" {
#endif
LIBARDOUR_API extern void vststate_init (VSTState* state);
LIBARDOUR_API extern void vststate_maybe_set_program (VSTState* state);
#ifdef __cplusplus
}
#endif

#endif
