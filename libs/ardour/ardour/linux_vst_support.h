/*
    Copyright (C) 2012 Paul Davis 

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

#ifndef __vstfx_h__
#define __vstfx_h__

#include <setjmp.h>
#include <signal.h>
#include <pthread.h>
#include <stdio.h>

#include "ardour/vst_types.h"

/******************************************************************************************/
/*VSTFX - an engine to manage native linux VST plugins - derived from FST for Windows VSTs*/
/******************************************************************************************/
 
extern void (*vstfx_error_callback)(const char *msg);

void vstfx_set_error_function (void (*func)(const char *));

void  vstfx_error (const char *fmt, ...);

/*API to vstfx*/

extern int	    vstfx_launch_editor (VSTState *);
extern int          vstfx_init (void *);
extern void         vstfx_exit ();
extern VSTHandle *  vstfx_load (const char*);
extern int          vstfx_unload (VSTHandle *);
extern VSTState *   vstfx_instantiate (VSTHandle *, audioMasterCallback, void *);
extern void         vstfx_close (VSTState*);

extern int          vstfx_create_editor (VSTState *);
extern int          vstfx_run_editor (VSTState *);
extern void         vstfx_destroy_editor (VSTState *);

extern VSTInfo *    vstfx_get_info (char *);
extern void         vstfx_free_info (VSTInfo *);
extern void         vstfx_event_loop_remove_plugin (VSTState *);
extern int          vstfx_call_dispatcher (VSTState *, int, int, int, void *, float);

/** Load a plugin state from a file.**/

extern int vstfx_load_state (VSTState* vstfx, char * filename);

/** Save a plugin state to a file.**/

extern bool vstfx_save_state (VSTState* vstfx, char * filename);


#endif /* __vstfx_h__ */
