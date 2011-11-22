#include <stdio.h>
#include <jack/jack.h>
#include <jack/thread.h>
#include <libgen.h>
#include <windows.h>
#include <winnt.h>
#include <wine/exception.h>
#include <pthread.h>
#include <signal.h>
#include <glib.h>

#include "fst.h"

#include <X11/X.h>
#include <X11/Xlib.h>

extern char * strdup (const char *);

struct ERect{
    short top;
    short left;
    short bottom;
    short right;
};

static pthread_mutex_t plugin_mutex;

/** Head of linked list of all FSTs */
static VSTState* fst_first = NULL;

const char magic[] = "FST Plugin State v002";

DWORD  gui_thread_id = 0;
static int gui_quit = 0;

#define DELAYED_WINDOW 1


static LRESULT WINAPI 
my_window_proc (HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
#if 0	
	if (msg != WM_TIMER) {
		fst_error ("window callback handler, msg = 0x%x win=%p\n", msg, w);
	}
#endif	

	switch (msg) {
	case WM_KEYUP:
	case WM_KEYDOWN:
		break;

	case WM_CLOSE:
		/* we don't care about windows closing ... */
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

static VSTState * 
fst_new ()
{
	VSTState* fst = (VSTState *) calloc (1, sizeof (VSTState));
	pthread_mutex_init (&fst->lock, NULL);
	pthread_cond_init (&fst->window_status_change, NULL);
	pthread_cond_init (&fst->plugin_dispatcher_called, NULL);
	fst->want_program = -1;
	fst->want_chunk = 0;
	fst->current_program = -1;
	fst->n_pending_keys = 0;
	fst->has_editor = 0;
	fst->program_set_without_editor = 0;
	return fst;
}

static VSTHandle* 
fst_handle_new ()
{
	VSTHandle* fst = (VSTHandle*) calloc (1, sizeof (VSTHandle));
	return fst;
}

void
maybe_set_program (VSTState* fst)
{
	if (fst->want_program != -1) {
		if (fst->vst_version >= 2) {
			fst->plugin->dispatcher (fst->plugin, 67 /* effBeginSetProgram */, 0, 0, NULL, 0);
		}
		
		fst->plugin->dispatcher (fst->plugin, effSetProgram, 0, fst->want_program, NULL, 0);
		
		if (fst->vst_version >= 2) {
			fst->plugin->dispatcher (fst->plugin, 68 /* effEndSetProgram */, 0, 0, NULL, 0);
		}
		/* did it work? */
		fst->current_program = fst->plugin->dispatcher (fst->plugin, 3, /* effGetProgram */ 0, 0, NULL, 0);
		fst->want_program = -1; 
	}
	
	if (fst->want_chunk == 1) {
		fst->plugin->dispatcher (fst->plugin, 24 /* effSetChunk */, 1, fst->wanted_chunk_size, fst->wanted_chunk, 0);
		fst->want_chunk = 0;
	}
}

DWORD WINAPI gui_event_loop (LPVOID param)
{
	MSG msg;
	VSTState* fst;
	HMODULE hInst;
	HWND window;
        int i;

	gui_thread_id = GetCurrentThreadId ();

	/* create a dummy window for timer events */

	if ((hInst = GetModuleHandleA (NULL)) == NULL) {
		fst_error ("can't get module handle");
		return 1;
	}
	
	if ((window = CreateWindowExA (0, "FST", "dummy",
				       WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
				       9999, 9999,
				       1, 1,
				       NULL, NULL,
				       hInst,
				       NULL )) == NULL) {
		fst_error ("cannot create dummy timer window");
	}

	if (!SetTimer (window, 1000, 20, NULL)) {
		fst_error ("cannot set timer on dummy window");
	}

	while (!gui_quit) {

		if (!GetMessageA (&msg, NULL, 0,0)) {
			if (!gui_quit) {
				fprintf (stderr, "QUIT message received by Windows GUI thread - ignored\n");
				continue;
			} else {
				break;
			}
		}

		TranslateMessage( &msg );
		DispatchMessageA (&msg);

		if (msg.message != WM_TIMER) {
			continue;
		}

		pthread_mutex_lock (&plugin_mutex);

		/* Do things that are appropriate for plugins which have open editor windows:
		   handle window creation requests, destroy requests, 
		   and run idle callbacks 
		*/
		
again:
		for (fst = fst_first; fst; fst = fst->next) {
			
			pthread_mutex_lock (&fst->lock);
			
			if (fst->has_editor == 1) {
				
				if (fst->destroy) {
					fprintf (stderr, "%s scheduled for destroy\n", fst->handle->name);
					if (fst->windows_window) {
						fst->plugin->dispatcher( fst->plugin, effEditClose, 0, 0, NULL, 0.0 );
						CloseWindow (fst->windows_window);
						fst->windows_window = NULL;
						fst->destroy = FALSE;
					}
					fst_event_loop_remove_plugin (fst);
					fst->been_activated = FALSE;
					pthread_cond_signal (&fst->window_status_change);
					pthread_mutex_unlock (&fst->lock);
					goto again;
				} 
				
				if (fst->windows_window == NULL) {
					if (fst_create_editor (fst)) {
						fst_error ("cannot create editor for plugin %s", fst->handle->name);
						fst_event_loop_remove_plugin (fst);
						pthread_cond_signal (&fst->window_status_change);
						pthread_mutex_unlock (&fst->lock);
						goto again;
					} else {
						/* condition/unlock: it was signalled & unlocked in fst_create_editor()   */
					}
				}
				
				if (fst->dispatcher_wantcall) {
					fst->dispatcher_retval = fst->plugin->dispatcher( fst->plugin, 
											  fst->dispatcher_opcode,
											  fst->dispatcher_index,
											  fst->dispatcher_val,
											  fst->dispatcher_ptr,
											  fst->dispatcher_opt );
					fst->dispatcher_wantcall = 0;
					pthread_cond_signal (&fst->plugin_dispatcher_called);
				}
				
				fst->plugin->dispatcher (fst->plugin, effEditIdle, 0, 0, NULL, 0);
				
				if (fst->wantIdle) {
					fst->plugin->dispatcher (fst->plugin, 53, 0, 0, NULL, 0);
				}
				
				/* Dispatch messages to send keypresses to the plugin */
				
				for (i = 0; i < fst->n_pending_keys; ++i) {
					/* I'm not quite sure what is going on here; it seems
					   `special' keys must be delivered with WM_KEYDOWN,
					   but that alphanumerics etc. must use WM_CHAR or
					   they will be ignored.  Ours is not to reason why ...
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

				/* See comment for maybe_set_program call below */
				maybe_set_program (fst);
				fst->want_program = -1;
				fst->want_chunk = 0;
			}

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

	return 0;
}

int
fst_init (void* possible_hmodule)
{
	WNDCLASSEX wclass;
	HMODULE hInst;
	
	if (possible_hmodule) {
		hInst = (HMODULE) possible_hmodule;
	} else if ((hInst = GetModuleHandleA (NULL)) == NULL) {
		fst_error ("can't get module handle");
		return -1;
	}

	wclass.cbSize = sizeof(WNDCLASSEX);
	wclass.style = 0;
	wclass.lpfnWndProc = my_window_proc;
	wclass.cbClsExtra = 0;
	wclass.cbWndExtra = 0;
	wclass.hInstance = hInst;
	wclass.hIcon = LoadIcon(hInst, "FST");
	wclass.hCursor = LoadCursor(0, IDI_APPLICATION);
//    wclass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wclass.lpszMenuName = "MENU_FST";
	wclass.lpszClassName = "FST";
	wclass.hIconSm = 0;


	if (!RegisterClassExA(&wclass)){
		printf( "Class register failed :(\n" );
		return -1;
	}

	fst_error ("Startup win32 GUI thread\n");

	if (CreateThread (NULL, 0, gui_event_loop, NULL, 0, NULL) == NULL) {
		fst_error ("could not create new thread proxy");
		return -1;
	}

#ifdef HAVE_JACK_SET_THREAD_CREATOR
	jack_set_thread_creator (wine_pthread_create);
#endif

	return 0;
}

void
fst_exit ()
{
	gui_quit = 1;
	PostQuitMessage (0);
}

int
fst_run_editor (VSTState* fst)
{
	/* wait for the plugin editor window to be created (or not) */

	pthread_mutex_lock (&fst->lock);

	fst->has_editor = 1;
	
	if (!fst->windows_window) {
		pthread_cond_wait (&fst->window_status_change, &fst->lock);
	}
	pthread_mutex_unlock (&fst->lock);

	if (!fst->windows_window) {
		return -1;
	}

	return 0;
}

int
fst_call_dispatcher (VSTState* fst, int opcode, int index, int val, void *ptr, float opt) 
{
	pthread_mutex_lock (&fst->lock);
	fst->dispatcher_opcode = opcode;
	fst->dispatcher_index = index;
	fst->dispatcher_val = val;
	fst->dispatcher_ptr = ptr;
	fst->dispatcher_opt = opt;
	fst->dispatcher_wantcall = 1;

	pthread_cond_wait (&fst->plugin_dispatcher_called, &fst->lock);
	pthread_mutex_unlock (&fst->lock);

	return fst->dispatcher_retval;
}

int
fst_create_editor (VSTState * fst)
{
	HMODULE hInst;
	HWND window;
	struct ERect* er;

	/* "guard point" to trap errors that occur during plugin loading */

	/* Note: fst->lock is held while this function is called */

	if (!(fst->plugin->flags & effFlagsHasEditor)) {
		fst_error ("Plugin \"%s\" has no editor", fst->handle->name);
		return -1;
	}

	if ((hInst = GetModuleHandleA (NULL)) == NULL) {
		fst_error ("can't get module handle");
		return 1;
	}
	
//	if ((window = CreateWindowExA (WS_EX_TOOLWINDOW | WS_EX_TRAYWINDOW, "FST", fst->handle->name,
	if ((window = CreateWindowExA (0, "FST", fst->handle->name,
				       (WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX),
//				       (WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX),
				       9999,9999,1,1,
//				       CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
				       NULL, NULL,
				       hInst,
				       NULL)) == NULL) {
		fst_error ("cannot create editor window");
		return 1;
	}

	if (!SetPropA (window, "fst_ptr", fst)) {
		fst_error ("cannot set fst_ptr on window");
	}

	fst->windows_window = window;
//	fst->xid = (int) GetPropA (window, "__wine_x11_whole_window");


	//printf( "effEditOpen......\n" );
	fst->plugin->dispatcher (fst->plugin, effEditOpen, 0, 0, fst->windows_window, 0);
	fst->plugin->dispatcher (fst->plugin, effEditGetRect, 0, 0, &er, 0 );

	fst->width =  er->right-er->left;
	fst->height =  er->bottom-er->top;
	//printf( "get rect ses... %d,%d\n", fst->width, fst->height );

	//SetWindowPos (fst->window, 0, 9999, 9999, er->right-er->left+8, er->bottom-er->top+26, 0);
	SetWindowPos (fst->windows_window, 0, 9999, 9999, 2, 2, 0);
	ShowWindow (fst->windows_window, SW_SHOWNA);
	//SetWindowPos (fst->window, 0, 0, 0, er->right-er->left+8, er->bottom-er->top+26, SWP_NOMOVE|SWP_NOZORDER);
	
	fst->xid = (int) GetPropA (window, "__wine_x11_whole_window");
	fst->been_activated = TRUE;
	pthread_cond_signal (&fst->window_status_change);
	pthread_mutex_unlock (&fst->lock);

	return 0;
}

void
fst_move_window_into_view (VSTState* fst)
{
        if (fst->windows_window) {
		SetWindowPos (fst->windows_window, 0, 0, 0, fst->width, fst->height + 24, 0);
		ShowWindow (fst->windows_window, SW_SHOWNA);
	}
}

void
fst_destroy_editor (VSTState* fst)
{
	pthread_mutex_lock (&fst->lock);
	if (fst->windows_window) {
		fprintf (stderr, "mark %s for destroy\n", fst->handle->name);
		fst->destroy = TRUE;
		//if (!PostThreadMessageA (gui_thread_id, WM_USER, 0, 0)) {
		//if (!PostThreadMessageA (gui_thread_id, WM_QUIT, 0, 0)) {
		//	fst_error ("could not post message to gui thread");
		//}
		pthread_cond_wait (&fst->window_status_change, &fst->lock);
		fprintf (stderr, "%s editor destroyed\n", fst->handle->name);
		fst->has_editor = 0;
	}
	pthread_mutex_unlock (&fst->lock);
}

void
fst_event_loop_remove_plugin (VSTState* fst)
{
	VSTState* p;
	VSTState* prev;

	for (p = fst_first, prev = NULL; p->next; prev = p, p = p->next) {
		if (p == fst) {
			if (prev) {
				prev->next = p->next;
			}
		}
	}

	if (fst_first == fst) {
		fst_first = fst_first->next;
	}

}

HMODULE
fst_load_vst_library(const char * path)
{
	HMODULE dll;
	char * full_path;
	char * envdup;
	char * vst_path;
	size_t len1;
	size_t len2;

	if ((dll = LoadLibraryA (path)) != NULL) {
		return dll;
	}

	envdup = getenv ("VST_PATH");
	if (envdup == NULL) {
		return NULL;
	}

	envdup = strdup (envdup);
	if (envdup == NULL) {
		fst_error ("strdup failed");
		return NULL;
	}

	len2 = strlen(path);

	vst_path = strtok (envdup, ":");
	while (vst_path != NULL) {
		fst_error ("\"%s\"", vst_path);
		len1 = strlen(vst_path);
		full_path = malloc (len1 + 1 + len2 + 1);
		memcpy(full_path, vst_path, len1);
		full_path[len1] = '/';
		memcpy(full_path + len1 + 1, path, len2);
		full_path[len1 + 1 + len2] = '\0';

		if ((dll = LoadLibraryA (full_path)) != NULL) {
			break;
		}

		vst_path = strtok (NULL, ":");
	}

	free(envdup);

	return dll;
}

VSTHandle *
fst_load (const char *path)
{
	char* buf;
	VSTHandle* fhandle;
	char* period;

	fhandle = fst_handle_new ();
	
	// XXX: Would be nice to find the correct call for this.
	//      if the user does not configure Z: to be / we are doomed :(

	if (strstr (path, ".dll") == NULL) {

		buf = (char *) malloc (strlen (path) + 7);

		if( path[0] == '/' ) {
		    sprintf (buf, "Z:%s.dll", path);
		} else {
		    sprintf (buf, "%s.dll", path);
		}

		fhandle->nameptr = strdup (path);

	} else {

		buf = (char *) malloc (strlen (path) + 3);

		if( path[0] == '/' ) {
		    sprintf (buf, "Z:%s", path);
		} else {
		    sprintf (buf, "%s", path);
		}

		fhandle->nameptr = strdup (path);
	}
	
	fhandle->name = basename (fhandle->nameptr);

	/* strip off .dll */

	if ((period = strrchr (fhandle->name, '.')) != NULL) {
		*period = '\0';
	}

	if ((fhandle->dll = fst_load_vst_library (buf)) == NULL) {
		fst_unload (fhandle);
		return NULL;
	}

	if ((fhandle->main_entry = (main_entry_t) GetProcAddress (fhandle->dll, "main")) == NULL) {
		fst_unload (fhandle);
		return NULL;
	}

	return fhandle;
}

int
fst_unload (VSTHandle* fhandle)
{
	if (fhandle->plugincnt) {
		return -1;
	}

	if (fhandle->dll) {
		FreeLibrary (fhandle->dll);
		fhandle->dll = NULL;
	}

	if (fhandle->nameptr) {
		free (fhandle->nameptr);
		fhandle->name = NULL;
	}
	
	free (fhandle);
	return 0;
}

VSTState*
fst_instantiate (VSTHandle* fhandle, audioMasterCallback amc, void* userptr)
{
	VSTState* fst = fst_new ();

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
	
	if( fhandle == NULL ) {
	    fst_error( "the handle was NULL\n" );
	    return NULL;
	}

	if ((fst->plugin = fhandle->main_entry (amc)) == NULL)  {
		fst_error ("%s could not be instantiated\n", fhandle->name);
		free (fst);
		return NULL;
	}
	
	fst->handle = fhandle;
	fst->plugin->user = userptr;
		
	if (fst->plugin->magic != kEffectMagic) {
		fst_error ("%s is not a VST plugin\n", fhandle->name);
		free (fst);
		return NULL;
	}
	
	fst->plugin->dispatcher (fst->plugin, effOpen, 0, 0, 0, 0);
	//fst->plugin->dispatcher (fst->plugin, effMainsChanged, 0, 0, NULL, 0);

	fst->vst_version = fst->plugin->dispatcher (fst->plugin, effGetVstVersion, 0, 0, 0, 0);
	
	fst->handle->plugincnt++;
	fst->wantIdle = 0;

	return fst;
}

void
fst_close (VSTState* fst)
{
	fst_destroy_editor (fst);

	fst->plugin->dispatcher (fst->plugin, effMainsChanged, 0, 0, NULL, 0);
	fst->plugin->dispatcher (fst->plugin, effClose, 0, 0, 0, 0);

	if (fst->handle->plugincnt) {
		--fst->handle->plugincnt;
	}
}

int
fst_get_XID (VSTState* fst)
{
	return fst->xid;
}

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

#if 0
int fst_load_state (FST * fst, char * filename)
{
	FILE * f = fopen (filename, "rb");
	if (f) {
		char testMagic[sizeof (magic)];
		fread (&testMagic, sizeof (magic), 1, f);
		if (strcmp (testMagic, magic)) {
			printf ("File corrupt\n");
			return FALSE;
		}

		char productString[64];
		char vendorString[64];
		char effectName[64];
		char testString[64];
		unsigned length;
		int success;

		fread (&length, sizeof (unsigned), 1, f);
		length = htonl (length);
		fread (productString, length, 1, f);
		productString[length] = 0;
		printf ("Product string: %s\n", productString);

		success = fst_call_dispatcher( fst, effGetProductString, 0, 0, testString, 0 );
		if (success == 1) {
			if (strcmp (testString, productString) != 0) {
				printf ("Product string mismatch! Plugin has: %s\n", testString);
				fclose (f);
				return FALSE;
			}
		} else if (length != 0) {
			printf ("Product string mismatch! Plugin has none.\n", testString);
			fclose (f);
			return FALSE;
		}

		fread (&length, sizeof (unsigned), 1, f);
		length = htonl (length);
		fread (effectName, length, 1, f);
		effectName[length] = 0;
		printf ("Effect name: %s\n", effectName);

		success = fst_call_dispatcher( fst, effGetEffectName, 0, 0, testString, 0 );
		if (success == 1) {
			if (strcmp (testString, effectName) != 0) {
				printf ("Effect name mismatch! Plugin has: %s\n", testString);
				fclose (f);
				return FALSE;
			}
		} else if (length != 0) {
			printf ("Effect name mismatch! Plugin has none.\n", testString);
			fclose (f);
			return FALSE;
		}

		fread (&length, sizeof (unsigned), 1, f);
		length = htonl (length);
		fread (vendorString, length, 1, f);
		vendorString[length] = 0;
		printf ("Vendor string: %s\n", vendorString);

		success = fst_call_dispatcher( fst, effGetVendorString, 0, 0, testString, 0 );
		if (success == 1) {
			if (strcmp (testString, vendorString) != 0) {
				printf ("Vendor string mismatch! Plugin has: %s\n", testString);
				fclose (f);
				return FALSE;
			}
		} else if (length != 0) {
			printf ("Vendor string mismatch! Plugin has none.\n", testString);
			fclose (f);
			return FALSE;
		}

		int numParam;
		unsigned i;
		fread (&numParam, sizeof (int), 1, f);
		numParam = htonl (numParam);
		for (i = 0; i < numParam; ++i) {
			float val;
			fread (&val, sizeof (float), 1, f);
			val = htonf (val);

			pthread_mutex_lock( &fst->lock );
			fst->plugin->setParameter( fst->plugin, i, val );
			pthread_mutex_unlock( &fst->lock );
		}

		int bytelen;
		fread (&bytelen, sizeof (int), 1, f);
		bytelen = htonl (bytelen);
		if (bytelen) {
			char * buf = malloc (bytelen);
			fread (buf, bytelen, 1, f);

			fst_call_dispatcher( fst, 24, 0, bytelen, buf, 0 );
			free (buf);
		}
	} else {
		printf ("Could not open state file\n");
		return FALSE;
	}
	return TRUE;

}
#endif

int
fst_save_state (VSTState * fst, char * filename)
{
	FILE * f = fopen (filename, "wb");
        int j;

	if (f) {
		int bytelen;
		int numParams = fst->plugin->numParams;
		char productString[64];
		char effectName[64];
		char vendorString[64];
		int success;

		// write header
		fprintf( f, "<plugin_state>\n" );

		success = fst_call_dispatcher( fst, effGetProductString, 0, 0, productString, 0 );
		if( success == 1 ) {
			fprintf (f, "  <check field=\"productString\" value=\"%s\"/>\n", productString);
		} else {
			printf ("No product string\n");
		}

		success = fst_call_dispatcher( fst, effGetEffectName, 0, 0, effectName, 0 );
		if( success == 1 ) {
			fprintf (f, "  <check field=\"effectName\" value=\"%s\"/>\n", effectName);
			printf ("Effect name: %s\n", effectName);
		} else {
			printf ("No effect name\n");
		}

		success = fst_call_dispatcher( fst, effGetVendorString, 0, 0, vendorString, 0 );
		if( success == 1 ) {
			fprintf (f, "  <check field=\"vendorString\" value=\"%s\"/>\n", vendorString);
			printf ("Vendor string: %s\n", vendorString);
		} else {
			printf ("No vendor string\n");
		}


		if( fst->plugin->flags & 32 ) {
			numParams = 0;
		}

		for (j = 0; j < numParams; ++j) {
			float val;
			
			pthread_mutex_lock( &fst->lock );
			val = fst->plugin->getParameter (fst->plugin, j);
			pthread_mutex_unlock( &fst->lock );
			fprintf( f, "  <param index=\"%d\" value=\"%f\"/>\n", j, val );
		}

		if( fst->plugin->flags & 32 ) {
			printf( "getting chunk...\n" );
			void * chunk;
			bytelen = fst_call_dispatcher( fst, 23, 0, 0, &chunk, 0 );
			printf( "got tha chunk..\n" );
			if( bytelen ) {
				if( bytelen < 0 ) {
					printf( "Chunke len < 0 !!! Not saving chunk.\n" );
				} else {
					char *encoded = g_base64_encode( chunk, bytelen );
					fprintf( f, "  <chunk size=\"%d\">\n    %s\n  </chunk>\n", bytelen, encoded );
					g_free( encoded );
				}
			}
		} 

		fprintf( f, "</plugin_state>\n" );
		fclose( f );
	} else {
		printf ("Could not open state file\n");
		return FALSE;
	}
	return TRUE;
}

