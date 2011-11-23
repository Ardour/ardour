#ifndef __fst_fst_h__
#define __fst_fst_h__

#include <setjmp.h>
#include <signal.h>
#include <pthread.h>

#include "ardour/vst_types.h"
#include "ardour/vestige/aeffectx.h"

/**
 * Display FST error message.
 *
 * Set via fst_set_error_function(), otherwise a FST-provided
 * default will print @a msg (plus a newline) to stderr.
 *
 * @param msg error message text (no newline at end).
 */
extern void (*fst_error_callback)(const char *msg);

/**
 * Set the @ref fst_error_callback for error message display.
 *
 * The FST library provides two built-in callbacks for this purpose:
 * default_fst_error_callback() and silent_fst_error_callback().
 */
void fst_set_error_function (void (*func)(const char *));

void  fst_error (const char *fmt, ...);

#ifdef __cplusplus
extern "C" {
#endif

extern int        fst_init (void* possible_hmodule);
extern void       fst_exit ();

extern VSTHandle* fst_load (const char*);
extern int        fst_unload (VSTHandle*);

extern VSTState * fst_instantiate (VSTHandle *, audioMasterCallback amc, void* userptr);
extern void       fst_close (VSTState *);

extern int  fst_create_editor (VSTState* fst);
extern int  fst_run_editor (VSTState *);
extern void fst_destroy_editor (VSTState *);
extern void fst_move_window_into_view (VSTState *);

extern VSTInfo *fst_get_info (char *dllpathname);
extern void fst_free_info (VSTInfo *info);
extern void fst_event_loop_remove_plugin (VSTState* fst);
extern int fst_call_dispatcher (VSTState *, int, int, int, void *, float);

/**
 * Load a plugin state from a file.
 */
extern int fst_load_state (VSTState *, char *);

/**
 * Save a plugin state to a file.
 */
extern int fst_save_state (VSTState *, char *);

extern int wine_pthread_create (pthread_t* thread_id, const pthread_attr_t* attr, void *(*function)(void*), void* arg);


#ifdef __cplusplus
}
#endif

#endif /* __fst_fst_h__ */
