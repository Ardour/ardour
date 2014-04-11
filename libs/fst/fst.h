#ifndef __fst_fst_h__
#define __fst_fst_h__

#include <setjmp.h>
#include <signal.h>
#include <pthread.h>

#include "ardour/libardour_visibility.h"
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
LIBARDOUR_API void fst_start_threading(void);
LIBARDOUR_API void fst_stop_threading(void);
LIBARDOUR_API void fst_audio_master_idle(void);

#ifdef __cplusplus
}
#endif

#endif /* __fst_fst_h__ */
