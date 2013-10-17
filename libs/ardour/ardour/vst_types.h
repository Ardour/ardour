/*
    Copyright (C) 2010 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifndef __ardour_vst_types_h__
#define __ardour_vst_types_h__

#include "ardour/libardour_visibility.h"
#include "ardour/vestige/aeffectx.h"

struct LIBARDOUR_API _VSTKey
{
	/** virtual-key code, or 0 if this _VSTFXKey is a `character' key */
	int special;
	/** `character' key, or 0 if this _VSTFXKey is a virtual-key */
	int character;
};

typedef struct _VSTKey VSTKey;

struct LIBARDOUR_API _VSTInfo 
{
	char  *name;
	char  *creator;
	int    UniqueID;
	char  *Category;
    
	int    numInputs;
	int    numOutputs;
	int    numParams;
	
	int    wantMidi;
	int    wantEvents;
	int    hasEditor;
	int    canProcessReplacing;
	
	char** ParamNames;
	char** ParamLabels;
};

typedef struct _VSTInfo VSTInfo;

typedef AEffect * (* main_entry_t) (audioMasterCallback);

struct LIBARDOUR_API _VSTHandle
{
	void*        dll;
	char*        name;
	char*        nameptr;
	
	main_entry_t main_entry;

	int          plugincnt;
};

typedef struct _VSTHandle VSTHandle;

struct LIBARDOUR_API _VSTState
{
	AEffect* plugin;

	/* Linux */
	int         linux_window;            ///< The plugin's parent X11 XWindow
	int         linux_plugin_ui_window;  ///< The ID of the plugin UI window created by the plugin

	/* Windows */
	void*       windows_window;

	int         xid;               ///< X11 XWindow
	
	int         want_resize;       ///< Set to signal the plugin resized its UI
	void*       extra_data;        ///< Pointer to any extra data
	
	void * event_callback_thisptr;
	void  (* eventProc) (void * event);
	
	VSTHandle*  handle;
	
	int	    width;
	int 	    height;
	int	    wantIdle;
	int	    destroy;
	int	    vst_version;
	int 	    has_editor;
	
	int	    program_set_without_editor;
	
	int	    want_program;
	int 	    want_chunk;
	int	    n_pending_keys;
	unsigned char * wanted_chunk;
	int 	    wanted_chunk_size;
	float *     want_params;
	float *     set_params;
	
	VSTKey	    pending_keys[16];

	int	    dispatcher_wantcall;
	int	    dispatcher_opcode;
	int	    dispatcher_index;
	int	    dispatcher_val;
	void *	    dispatcher_ptr;
	float	    dispatcher_opt;
	int	    dispatcher_retval;

	struct _VSTState * next;
	pthread_mutex_t    lock;
	pthread_cond_t     window_status_change;
	pthread_cond_t     plugin_dispatcher_called;
	pthread_cond_t     window_created;
	int                been_activated;
};

typedef struct _VSTState VSTState;

#endif
