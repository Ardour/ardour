#ifndef __fst_fst_h__
#define __fst_fst_h__

#include <setjmp.h>
#include <signal.h>
#include <pthread.h>

/**
 * Display FST error message.
 *
 * @param fmt printf-style formatting specification
 */
extern void fst_error (const char *fmt, ...);

/**
 * Set the @ref fst_error_callback for error message display.
 *
 * The FST library provides two built-in callbacks for this purpose:
 * default_fst_error_callback().
 *
 * The default will print the message (plus a newline) to stderr.
 *
 */
void fst_set_error_function (void (*func)(const char *));

#include <vst/AEffect.h>

typedef struct _FST FST;
typedef struct _FSTHandle FSTHandle;
typedef struct _FSTInfo FSTInfo;

struct _FSTInfo 
{
    char *name;
    int UniqueID;
    char *Category;
    
    int numInputs;
    int numOutputs;
    int numParams;

    int wantMidi;
    int wantEvents;
    int hasEditor;
    int canProcessReplacing; // what do we need this for ?

    // i think we should save the parameter Info Stuff soon.
    // struct VstParameterInfo *infos;
    char **ParamNames;
    char **ParamLabels;
};

struct _FSTHandle
{
    void*    dll;
    char*    name;
    char*    nameptr; /* ptr returned from strdup() etc. */
    AEffect* (*main_entry)(audioMasterCallback);

    int plugincnt;
};

struct _FST 
{
    AEffect*    plugin;
    void*       window; /* win32 HWND */
    int         xid; /* X11 XWindow */
    FSTHandle*  handle;
    int 	width;
    int 	height;
    int         destroy;

    struct _FST* next;

    pthread_mutex_t lock;
    pthread_cond_t  window_status_change;
    int             been_activated;
};

#ifdef __cplusplus
extern "C" {
#endif

extern int  fst_init ();

extern FSTHandle* fst_load (const char*);
extern int        fst_unload (FSTHandle*);

extern FST*       fst_instantiate (FSTHandle*, audioMasterCallback amc, void* userptr);
extern void       fst_close (FST*);

extern void fst_event_loop_remove_plugin (FST* fst);
extern void fst_event_loop_add_plugin (FST* fst);

extern int  fst_run_editor (FST*);
extern void  fst_destroy_editor (FST*);
extern int  fst_get_XID (FST*);

extern void fst_signal_handler (int sig, siginfo_t* info, void* context);

extern FSTInfo *fst_get_info( char *dllpathname );
extern void fst_free_info( FSTInfo *info );

#ifdef __cplusplus
}
#endif

#endif /* __fst_fst_h__ */
