#ifndef __fst_fst_h__
#define __fst_fst_h__

#include <setjmp.h>
#include <signal.h>
#include <pthread.h>

#include "ardour/libardour_visibility.h"
#include "ardour/vst_types.h"
#include "ardour/vestige/aeffectx.h"

#include "pbd/libpbd_visibility.h"


/**
 * Display FST error message.
 *
 * Set via fst_set_error_function(), otherwise a FST-provided
 * default will print @a msg (plus a newline) to stderr.
 *
 * @param msg error message text (no newline at end).
 */
LIBARDOUR_API void (*fst_error_callback)(const char *msg);

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
extern void       fst_exit (void);

extern VSTHandle* fst_load (const char*);
extern int        fst_unload (VSTHandle**);

extern VSTState * fst_instantiate (VSTHandle *, audioMasterCallback amc, void* userptr);
extern void       fst_close (VSTState *);

//these funcs get called from gtk2_ardour, so need to be visible
LIBARDOUR_API int  fst_run_editor (VSTState *, void* window_parent);
LIBARDOUR_API void fst_destroy_editor (VSTState *);
LIBARDOUR_API void fst_move_window_into_view (VSTState *);
//----

extern void fst_event_loop_remove_plugin (VSTState* fst);

#ifndef PLATFORM_WINDOWS /* linux + wine */
extern void fst_start_threading(void);
extern void fst_stop_threading(void);
#endif

extern void fst_audio_master_idle(void);

#ifdef __cplusplus
}
#endif

#endif /* __fst_fst_h__ */
