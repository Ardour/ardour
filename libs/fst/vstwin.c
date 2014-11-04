#include <stdio.h>
#include <string.h>
#include <windows.h>

#define fst_error(...) fprintf(stderr, __VA_ARGS__)

#ifdef PLATFORM_WINDOWS

#include <pthread.h>
static UINT_PTR idle_timer_id   = 0;

#else /* linux + wine */

#include <linux/limits.h> // PATH_MAX
#include <winnt.h>
#include <wine/exception.h>
#include <pthread.h>
static int gui_quit = 0;
static unsigned int idle_id = 0;

#endif

#ifndef COMPILER_MSVC
extern char * strdup (const char *);
#endif

#include <glib.h>
#include "fst.h"

struct ERect {
	short top;
	short left;
	short bottom;
	short right;
};

static pthread_mutex_t  plugin_mutex;
static VSTState*        fst_first        = NULL; /**< Head of linked list of all FSTs */
static int              host_initialized = 0;
static const char       magic[]          =  "FST Plugin State v002";


static LRESULT WINAPI
vstedit_wndproc (HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
	switch (msg) {
		case WM_KEYUP:
		case WM_KEYDOWN:
			break;

		case WM_CLOSE:
			/* we don't care about windows closing ...
			 * WM_CLOSE is used for minimizing the window.
			 * Our window has no frame so it shouldn't ever
			 * get sent - but if it does, we don't want our
			 * window to get minimized!
			 */
			return 0;
			break;

		case WM_DESTROY:
		case WM_NCDESTROY:
			/* we don't care about windows being destroyed ... */
			return 0;
			break;

		default:
			break;
	}

	return DefWindowProcA (w, msg, wp, lp );
}


static void
maybe_set_program (VSTState* fst)
{
	if (fst->want_program != -1) {
		if (fst->vst_version >= 2) {
			fst->plugin->dispatcher (fst->plugin, effBeginSetProgram, 0, 0, NULL, 0);
		}

		fst->plugin->dispatcher (fst->plugin, effSetProgram, 0, fst->want_program, NULL, 0);

		if (fst->vst_version >= 2) {
			fst->plugin->dispatcher (fst->plugin, effEndSetProgram, 0, 0, NULL, 0);
		}
		fst->want_program = -1;
	}

	if (fst->want_chunk == 1) {
		// XXX check
		// 24 == audioMasterGetAutomationState,
		// 48 == audioMasterGetChunkFile
		fst->plugin->dispatcher (fst->plugin, 24 /* effSetChunk */, 1, fst->wanted_chunk_size, fst->wanted_chunk, 0);
		fst->want_chunk = 0;
	}
}

static VOID CALLBACK
idle_hands(
		HWND hwnd,        // handle to window for timer messages
		UINT message,     // WM_TIMER message
		UINT idTimer,     // timer identifier
		DWORD dwTime)     // current system time
{
	VSTState* fst;

	pthread_mutex_lock (&plugin_mutex);

	for (fst = fst_first; fst; fst = fst->next) {
		if (fst->gui_shown) {
			// this seems insane, but some plugins will not draw their meters if you don't
			// call this every time.  Example Ambience by Magnus @ Smartelectron:x
			fst->plugin->dispatcher (fst->plugin, effEditIdle, 0, 0, NULL, 0);

			if (fst->wantIdle) {
				fst->wantIdle = fst->plugin->dispatcher (fst->plugin, effIdle, 0, 0, NULL, 0);
			}
		}

		pthread_mutex_lock (&fst->lock);
#ifndef PLATFORM_WINDOWS /* linux + wine */
		/* Dispatch messages to send keypresses to the plugin */
		int i;

		for (i = 0; i < fst->n_pending_keys; ++i) {
			MSG msg;
			/* I'm not quite sure what is going on here; it seems
			 * `special' keys must be delivered with WM_KEYDOWN,
			 * but that alphanumerics etc. must use WM_CHAR or
			 * they will be ignored.  Ours is not to reason why ...
			 */
			if (fst->pending_keys[i].special != 0) {
				msg.message = WM_KEYDOWN;
				msg.wParam = fst->pending_keys[i].special;
			} else {
				msg.message = WM_CHAR;
				msg.wParam = fst->pending_keys[i].character;
			}
			msg.hwnd = GetFocus ();
			msg.lParam = 0;
			DispatchMessageA (&msg);
		}

		fst->n_pending_keys = 0;
#endif

		/* See comment for maybe_set_program call below */
		maybe_set_program (fst);
		fst->want_program = -1;
		fst->want_chunk = 0;
		/* If we don't have an editor window yet, we still need to
		 * set up the program, otherwise when we load a plugin without
		 * opening its window it will sound wrong.  However, it seems
		 * that if you don't also load the program after opening the GUI,
		 * the GUI does not reflect the program properly.  So we'll not
		 * mark that we've done this (ie we won't set want_program to -1)
		 * and so it will be done again if and when the GUI arrives.
		 */
		if (fst->program_set_without_editor == 0) {
			maybe_set_program (fst);
			fst->program_set_without_editor = 1;
		}

		pthread_mutex_unlock (&fst->lock);
	}

	pthread_mutex_unlock (&plugin_mutex);
}

static void
fst_idle_timer_add_plugin (VSTState* fst)
{
	pthread_mutex_lock (&plugin_mutex);

	if (fst_first == NULL) {
		fst_first = fst;
	} else {
		VSTState* p = fst_first;
		while (p->next) {
			p = p->next;
		}
		p->next = fst;
	}

	pthread_mutex_unlock (&plugin_mutex);
}

static void
fst_idle_timer_remove_plugin (VSTState* fst)
{
	VSTState* p;
	VSTState* prev;

	pthread_mutex_lock (&plugin_mutex);

	for (p = fst_first, prev = NULL; p; prev = p, p = p->next) {
		if (p == fst) {
			if (prev) {
				prev->next = p->next;
			}
			break;
		}
		if (!p->next) {
			break;
		}
	}

	if (fst_first == fst) {
		fst_first = fst_first->next;
	}

	pthread_mutex_unlock (&plugin_mutex);
}

static VSTState*
fst_new (void)
{
	VSTState* fst = (VSTState*) calloc (1, sizeof (VSTState));
	pthread_mutex_init (&fst->lock, NULL);
	pthread_cond_init (&fst->window_status_change, NULL); // unused ?? -> TODO check gtk2ardour
	pthread_cond_init (&fst->plugin_dispatcher_called, NULL); // unused ??
	fst->want_program = -1;
	fst->want_chunk = 0;
	fst->n_pending_keys = 0;
	fst->has_editor = 0;
#ifdef PLATFORM_WINDOWS
	fst->voffset = 50;
	fst->hoffset = 0;
#else /* linux + wine */
	fst->voffset = 24;
	fst->hoffset = 6;
#endif
	fst->program_set_without_editor = 0;
	return fst;
}

static void
fst_delete (VSTState* fst)
{
	if (fst) {
		free((void*)fst);
		fst = NULL;
	}
}

static VSTHandle*
fst_handle_new (void)
{
	VSTHandle* fst = (VSTHandle*) calloc (1, sizeof (VSTHandle));
	return fst;
}

#ifndef PLATFORM_WINDOWS /* linux + wine */
static gboolean
g_idle_call (gpointer ignored) {
	if (gui_quit) return FALSE;
	MSG msg;
	if (PeekMessageA (&msg, NULL, 0, 0, 1)) {
		TranslateMessage (&msg);
		DispatchMessageA (&msg);
	}
	idle_hands(NULL, 0, 0, 0);
	g_main_context_iteration(NULL, FALSE);
	return gui_quit ? FALSE : TRUE;
}
#endif


int
fst_init (void* possible_hmodule)
{
	if (host_initialized) return 0;
	HMODULE hInst;

	if (possible_hmodule) {
#ifdef PLATFORM_WINDOWS
		fst_error ("Error in fst_init(): (module handle is unnecessary for Win32 build)");
		return -1;
#else /* linux + wine */
		hInst = (HMODULE) possible_hmodule;
#endif
	} else if ((hInst = GetModuleHandleA (NULL)) == NULL) {
		fst_error ("can't get module handle");
		return -1;
	}

	if (!hInst) {
		fst_error ("Cannot initialise VST host");
		return -1;
	}

	WNDCLASSEX wclass;

	wclass.cbSize = sizeof(WNDCLASSEX);
#ifdef PLATFORM_WINDOWS
	wclass.style = (CS_HREDRAW | CS_VREDRAW);
	wclass.hIcon = NULL;
	wclass.hCursor = LoadCursor(0, IDC_ARROW);
#else /* linux + wine */
	wclass.style = 0;
	wclass.hIcon = LoadIcon(hInst, "FST");
	wclass.hCursor = LoadCursor(0, IDI_APPLICATION);
#endif
	wclass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wclass.lpfnWndProc = vstedit_wndproc;
	wclass.cbClsExtra = 0;
	wclass.cbWndExtra = 0;
	wclass.hInstance = hInst;
	wclass.lpszMenuName = "MENU_FST";
	wclass.lpszClassName = "FST";
	wclass.hIconSm = 0;

	pthread_mutex_init (&plugin_mutex, NULL);
	host_initialized = -1;

	if (!RegisterClassExA(&wclass)){
		fst_error ("Error in fst_init(): (class registration failed");
		return -1;
	}
	return 0;
}

void
fst_start_threading(void)
{
#ifndef PLATFORM_WINDOWS /* linux + wine */
	if (idle_id == 0) {
		gui_quit = 0;
		idle_id = g_idle_add (g_idle_call, NULL);
	}
#endif
}

void
fst_stop_threading(void) {
#ifndef PLATFORM_WINDOWS /* linux + wine */
	if (idle_id != 0) {
		gui_quit = 1;
		PostQuitMessage (0);
		g_main_context_iteration(NULL, FALSE);
		//g_source_remove(idle_id);
		idle_id = 0;
	}
#endif
}

void
fst_exit (void)
{
	if (!host_initialized) return;
	VSTState* fst;
	// If any plugins are still open at this point, close them!
	while ((fst = fst_first))
		fst_close (fst);

#ifdef PLATFORM_WINDOWS
	if (idle_timer_id != 0) {
		KillTimer(NULL, idle_timer_id);
	}
#else /* linux + wine */
	if (idle_id) {
		gui_quit = 1;
		PostQuitMessage (0);
	}
#endif

	host_initialized = FALSE;
	pthread_mutex_destroy (&plugin_mutex);
}


int
fst_run_editor (VSTState* fst, void* window_parent)
{
	if (fst->windows_window == NULL) {
		HMODULE hInst;
		HWND window;
		struct ERect* er;

		if (!(fst->plugin->flags & effFlagsHasEditor)) {
			fst_error ("Plugin \"%s\" has no editor", fst->handle->name);
			return -1;
		}

		if ((hInst = GetModuleHandleA (NULL)) == NULL) {
			fst_error ("fst_create_editor() can't get module handle");
			return 1;
		}

		if ((window = CreateWindowExA (0, "FST", fst->handle->name,
						window_parent ? WS_CHILD : (WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX),
						CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
						(HWND)window_parent, NULL,
						hInst,
						NULL) ) == NULL) {
			fst_error ("fst_create_editor() cannot create editor window");
			return 1;
		}

		if (!SetPropA (window, "fst_ptr", fst)) {
			fst_error ("fst_create_editor() cannot set fst_ptr on window");
		}

		fst->windows_window = window;

		if (window_parent) {
			// This is requiredv for some reason. Note the parent is set above when the window
			// is created. Without this extra call the actual plugin window will draw outside
			// of our plugin window.
			SetParent((HWND)fst->windows_window, (HWND)window_parent);
			fst->xid = 0;
#ifndef PLATFORM_WINDOWS /* linux + wine */
		} else {
			SetWindowPos (fst->windows_window, 0, 9999, 9999, 2, 2, 0);
			ShowWindow (fst->windows_window, SW_SHOWNA);
			fst->xid = (int) GetPropA (fst->windows_window, "__wine_x11_whole_window");
#endif
		}

		// This is the suggested order of calls.
		fst->plugin->dispatcher (fst->plugin, effEditGetRect, 0, 0, &er, 0 );
		fst->plugin->dispatcher (fst->plugin, effEditOpen, 0, 0, fst->windows_window, 0 );
		fst->plugin->dispatcher (fst->plugin, effEditGetRect, 0, 0, &er, 0 );

		fst->width =  er->right-er->left;
		fst->height =  er->bottom-er->top;


		fst->been_activated = TRUE;

	}

	if (fst->windows_window) {
#ifdef PLATFORM_WINDOWS
		if (idle_timer_id == 0) {
			// Init the idle timer if needed, so that the main window calls us.
			idle_timer_id = SetTimer(NULL, idle_timer_id, 50, (TIMERPROC) idle_hands);
		}
#endif

		fst_idle_timer_add_plugin (fst);
	}

	return fst->windows_window == NULL ? -1 : 0;
}

void
fst_destroy_editor (VSTState* fst)
{
	if (fst->windows_window) {
		fprintf (stderr, "%s destroying edit window\n", fst->handle->name);

		fst_idle_timer_remove_plugin (fst);
		fst->plugin->dispatcher( fst->plugin, effEditClose, 0, 0, NULL, 0.0 );

		DestroyWindow ((HWND)(fst->windows_window));

		fst->windows_window = NULL;
	}

	fst->been_activated = FALSE;
}

void
fst_move_window_into_view (VSTState* fst)
{
	if (fst->windows_window) {
#ifdef PLATFORM_WINDOWS
		SetWindowPos ((HWND)(fst->windows_window), 0, fst->hoffset, fst->voffset, fst->width + fst->hoffset, fst->height + fst->voffset, 0);
#else /* linux + wine */
		SetWindowPos ((HWND)(fst->windows_window), 0, 0, 0, fst->width + fst->hoffset, fst->height + fst->voffset, 0);
#endif
		ShowWindow ((HWND)(fst->windows_window), SW_SHOWNA);
	}
}

static HMODULE
fst_load_vst_library(const char * path)
{
	char legalized_path[PATH_MAX];
	strcpy (legalized_path, g_locale_from_utf8(path, -1, NULL, NULL, NULL));
	return ( LoadLibraryA (legalized_path) );
}

VSTHandle *
fst_load (const char *path)
{
	VSTHandle* fhandle = NULL;

	if ((strlen(path)) && (NULL != (fhandle = fst_handle_new ())))
	{
		char* period;
		fhandle->path = strdup (path);
		fhandle->name = g_path_get_basename(path);
		if ((period = strrchr (fhandle->name, '.'))) {
			*period = '\0';
		}

		// See if we can load the plugin DLL
		if ((fhandle->dll = (HMODULE)fst_load_vst_library (path)) == NULL) {
			fst_unload (&fhandle);
			return NULL;
		}

		fhandle->main_entry = (main_entry_t) GetProcAddress ((HMODULE)fhandle->dll, "main");

		if (fhandle->main_entry == 0) {
			if ((fhandle->main_entry = (main_entry_t) GetProcAddress ((HMODULE)fhandle->dll, "VSTPluginMain"))) {
				fprintf(stderr, "VST >= 2.4 plugin '%s'\n", path);
				//PBD::warning << path << _(": is a VST >= 2.4 - this plugin may or may not function correctly with this version of Ardour.") << endmsg;
			}
		}

		if (fhandle->main_entry == 0) {
			fst_unload (&fhandle);
			return NULL;
		}
	}
	return fhandle;
}

int
fst_unload (VSTHandle** fhandle)
{
	if (!(*fhandle)) {
		return -1;
	}

	if ((*fhandle)->plugincnt) {
		return -1;
	}

	if ((*fhandle)->dll) {
		FreeLibrary ((HMODULE)(*fhandle)->dll);
		(*fhandle)->dll = NULL;
	}

	if ((*fhandle)->path) {
		free ((*fhandle)->path);
		(*fhandle)->path = NULL;
	}

	if ((*fhandle)->name) {
		free ((*fhandle)->name);
		(*fhandle)->name = NULL;
	}

	free (*fhandle);
	*fhandle = NULL;

	return 0;
}

VSTState*
fst_instantiate (VSTHandle* fhandle, audioMasterCallback amc, void* userptr)
{
	VSTState* fst = NULL;

	if( fhandle == NULL ) {
		fst_error( "fst_instantiate(): (the handle was NULL)\n" );
		return NULL;
	}

	fst = fst_new ();

	if ((fst->plugin = fhandle->main_entry (amc)) == NULL)  {
		fst_error ("fst_instantiate: %s could not be instantiated\n", fhandle->name);
		free (fst);
		return NULL;
	}

	fst->handle = fhandle;
	fst->plugin->user = userptr;

	if (fst->plugin->magic != kEffectMagic) {
		fst_error ("fst_instantiate: %s is not a vst plugin\n", fhandle->name);
		fst_close(fst);
		return NULL;
	}

	fst->plugin->dispatcher (fst->plugin, effOpen, 0, 0, 0, 0);
	fst->vst_version = fst->plugin->dispatcher (fst->plugin, effGetVstVersion, 0, 0, 0, 0);

	fst->handle->plugincnt++;
	fst->wantIdle = 0;

	return fst;
}

void fst_audio_master_idle(void) {
	while(g_main_context_iteration(NULL, FALSE)) ;
}

void
fst_close (VSTState* fst)
{
	if (fst != NULL) {
		fst_destroy_editor (fst);

		if (fst->plugin) {
			fst->plugin->dispatcher (fst->plugin, effMainsChanged, 0, 0, NULL, 0);
			fst->plugin->dispatcher (fst->plugin, effClose, 0, 0, 0, 0);
			fst->plugin = NULL;
		}

		if (fst->handle) {
			if (fst->handle->plugincnt && --fst->handle->plugincnt == 0) {

				fst->handle->main_entry = NULL;
				fst_unload (&fst->handle); // XXX
			}
		}

		/* It might be good for this to be in it's own cleanup function
			since it will free the memory for the fst leaving the caller
			with an invalid pointer.  Caller beware */
		fst_delete(fst);
	}
}

#if 0 // ?? who needs this, where?
float htonf (float v)
{
	float result;
	char * fin = (char*)&v;
	char * fout = (char*)&result;
	fout[0] = fin[3];
	fout[1] = fin[2];
	fout[2] = fin[1];
	fout[3] = fin[0];
	return result;
}
#endif
