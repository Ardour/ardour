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

typedef struct _FST FST;

struct _FST 
{
    AEffect*    plugin;
    void*       window; /* win32 HWND */
    int         xid; /* X11 XWindow */
    VSTHandle*  handle;
    int 	width;
    int 	height;
    int		wantIdle;
    int         destroy;
    int         vst_version;
    int         has_editor;

    int         program_set_without_editor;
    int		want_program;
    int         want_chunk;
    unsigned char *wanted_chunk;
    int         wanted_chunk_size;
    int         current_program;
    float      *want_params;
    float      *set_params;
	
    VSTKey      pending_keys[16];
    int         n_pending_keys;

    int         dispatcher_wantcall;
    int         dispatcher_opcode;
    int         dispatcher_index;
    int         dispatcher_val;
    void *	dispatcher_ptr;
    float	dispatcher_opt;
    int		dispatcher_retval;

    struct _FST* next;
    pthread_mutex_t lock;
    pthread_cond_t  window_status_change;
    pthread_cond_t  plugin_dispatcher_called;
    int             been_activated;
};

#ifdef __cplusplus
extern "C" {
#endif

extern int        fst_init (void* possible_hmodule);
extern void       fst_exit ();

extern VSTHandle* fst_load (const char*);
extern int        fst_unload (VSTHandle*);

extern FST*       fst_instantiate (VSTHandle*, audioMasterCallback amc, void* userptr);
extern void       fst_close (FST*);

extern int fst_create_editor (FST* fst);
extern int  fst_run_editor (FST*);
extern void  fst_destroy_editor (FST*);
extern int  fst_get_XID (FST*);
extern void fst_move_window_into_view (FST*);

extern VSTInfo *fst_get_info (char *dllpathname);
extern void fst_free_info (VSTInfo *info);
extern void fst_event_loop_remove_plugin (FST* fst);
extern int fst_call_dispatcher(FST *fst, int opcode, int index, int val, void *ptr, float opt );

/**
 * Load a plugin state from a file.
 */
extern int fst_load_state (FST * fst, char * filename);

/**
 * Save a plugin state to a file.
 */
extern int fst_save_state (FST * fst, char * filename);

extern int wine_pthread_create (pthread_t* thread_id, const pthread_attr_t* attr, void *(*function)(void*), void* arg);


#ifdef __cplusplus
}
#endif

#endif /* __fst_fst_h__ */
