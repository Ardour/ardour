/*
 * Copyright (C) 2006-2010 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2010-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2016 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2014-2021 Robin Gareus <robin@gareus.org>
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

#include <stdio.h>
#include <string.h>
#include <windows.h>

#define fst_error(...) fprintf(stderr, __VA_ARGS__)

#ifndef PLATFORM_WINDOWS
#error VSTWIN ONLY WORKS ON WINDOWS
#endif

#include <pthread.h>
static UINT_PTR idle_timer_id   = 0;

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

		case WM_SIZE:
			{
				LRESULT rv = DefWindowProcA (w, msg, wp, lp);
				RECT rect;
				GetClientRect(w, &rect);
#ifndef NDEBUG
				printf("VST WM_SIZE.. %ld %ld %ld %ld\n", rect.top, rect.left, (rect.right - rect.left), (rect.bottom - rect.top));
#endif
				VSTState* fst = (VSTState*) GetProp (w, "fst_ptr");
				if (fst) {
					int32_t width = (rect.right - rect.left);
					int32_t height = (rect.bottom - rect.top);
					if (width > 0 && height > 0) {
						fst->amc (fst->plugin, 15 /*audioMasterSizeWindow */, width, height, NULL, 0);
					}
				}
				return rv;
			}
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

	return DefWindowProcA (w, msg, wp, lp);
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

		/* See comment for call below */
		vststate_maybe_set_program (fst);
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
			vststate_maybe_set_program (fst);
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
	vststate_init (fst);

	fst->voffset = 45;
	fst->hoffset = 0;
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

int
fst_init (void* possible_hmodule)
{
	if (host_initialized) return 0;
	HMODULE hInst;

	if (possible_hmodule) {
		fst_error ("Error in fst_init(): (module handle is unnecessary for Win32 build)");
		return -1;
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
	wclass.style = (CS_HREDRAW | CS_VREDRAW);
	wclass.hIcon = NULL;
	wclass.hCursor = LoadCursor(0, IDC_ARROW);
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
fst_exit (void)
{
	if (!host_initialized) return;
	VSTState* fst;
	// If any plugins are still open at this point, close them!
	while ((fst = fst_first))
		fst_close (fst);

	if (idle_timer_id != 0) {
		KillTimer(NULL, idle_timer_id);
	}

	host_initialized = FALSE;
	pthread_mutex_destroy (&plugin_mutex);
}


int
fst_run_editor (VSTState* fst, void* window_parent)
{
	/* For safety, remove any pre-existing editor window */ 
	fst_destroy_editor (fst);
	
	if (fst->windows_window == NULL) {
		HMODULE hInst;
		HWND window;
		struct ERect* er = NULL;

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
		}

		// This is the suggested order of calls.
		fst->plugin->dispatcher (fst->plugin, effEditGetRect, 0, 0, &er, 0 );
		fst->plugin->dispatcher (fst->plugin, effEditOpen, 0, 0, fst->windows_window, 0 );
		fst->plugin->dispatcher (fst->plugin, effEditGetRect, 0, 0, &er, 0 );

		if (er != NULL) {
			fst->width = er->right - er->left;
			fst->height = er->bottom - er->top;
		}

		fst->been_activated = TRUE;

	}

	if (fst->windows_window) {
		if (idle_timer_id == 0) {
			// Init the idle timer if needed, so that the main window calls us.
			idle_timer_id = SetTimer(NULL, idle_timer_id, 50, (TIMERPROC) idle_hands);
		}

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
		SetWindowPos ((HWND)(fst->windows_window),
				HWND_TOP /*0*/,
				fst->hoffset, fst->voffset,
				fst->width, fst->height,
				SWP_NOACTIVATE|SWP_NOOWNERZORDER);
		ShowWindow ((HWND)(fst->windows_window), SW_SHOWNA);
		UpdateWindow ((HWND)(fst->windows_window));
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
			fst_error ("fst_load(): Cannot open plugin dll\n");
			fst_unload (&fhandle);
			return NULL;
		}

		fhandle->main_entry = (main_entry_t) GetProcAddress ((HMODULE)fhandle->dll, "VSTPluginMain");

		if (fhandle->main_entry == 0) {
			fhandle->main_entry = (main_entry_t) GetProcAddress ((HMODULE)fhandle->dll, "main");
		}

		if (fhandle->main_entry == 0) {
			fst_error ("fst_load(): Missing entry method in VST2 plugin\n");
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
	fst->amc = amc;

	if ((fst->plugin = fhandle->main_entry (amc)) == NULL)  {
		fst_error ("fst_instantiate: %s could not be instantiated\n", fhandle->name);
		free (fst);
		return NULL;
	}

	fst->handle = fhandle;
	fst->plugin->ptr1 = userptr;

	if (fst->plugin->magic != kEffectMagic) {
		fst_error ("fst_instantiate: %s is not a vst plugin\n", fhandle->name);
		fst_close(fst);
		return NULL;
	}

	if (!userptr) {
		/* scanning.. or w/o master-callback userptr == 0, open now.
		 *
		 * Session::vst_callback needs a pointer to the AEffect
		 *     ((VSTPlugin*)userptr)->_plugin = vstfx->plugin
		 * before calling effOpen, because effOpen may call back
		 */
		fst->plugin->dispatcher (fst->plugin, effOpen, 0, 0, 0, 0);
		fst->vst_version = fst->plugin->dispatcher (fst->plugin, effGetVstVersion, 0, 0, 0, 0);
	}

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
