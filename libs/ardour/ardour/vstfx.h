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
extern int          vstfx_get_XID (VSTState *);
extern void         vstfx_move_window_into_view (VSTState *);

extern VSTInfo *    vstfx_get_info (char *);
extern void         vstfx_free_info (VSTInfo *);
extern void         vstfx_event_loop_remove_plugin (VSTState *);
extern int          vstfx_call_dispatcher (VSTState *, int, int, int, void *, float);

/** Load a plugin state from a file.**/

extern int vstfx_load_state (VSTState* vstfx, char * filename);

/** Save a plugin state to a file.**/

extern bool vstfx_save_state (VSTState* vstfx, char * filename);


#endif /* __vstfx_h__ */
