#include <stdio.h>
#include <libgen.h>
#include <windows.h>
#include <winnt.h>
#include <wine/exception.h>
#include <pthread.h>
#include <signal.h>

//#include <x11/xlib.h>
//#include <x11/xresource.h>
//#include <x11/xutil.h>
//#include <x11/xatom.h>

#include "fst.h"


struct ERect{
    short top;
    short left;
    short bottom;
    short right;
};

static pthread_mutex_t plugin_mutex = PTHREAD_MUTEX_INITIALIZER;
static FST* fst_first = NULL;

DWORD  gui_thread_id = 0;

static char* message_name (int message)
{
	switch (message) {
	case 0x0000:
		return "WM_NULL";

	case 0x0001:
		return "WM_CREATE";

	case 0x0002:
		return "WM_DESTROY";

	case 0x0003:
		return "WM_MOVE";

	case 0x0004:
		return "WM_SIZEWAIT";

	case 0x0005:
		return "WM_SIZE";

	case 0x0006:
		return "WM_ACTIVATE";

	case 0x0007:
		return "WM_SETFOCUS";

	case 0x0008:
		return "WM_KILLFOCUS";

	case 0x0009:
		return "WM_SETVISIBLE";

	case 0x000a:
		return "WM_ENABLE";

	case 0x000b:
		return "WM_SETREDRAW";

	case 0x000c:
		return "WM_SETTEXT";

	case 0x000d:
		return "WM_GETTEXT";

	case 0x000e:
		return "WM_GETTEXTLENGTH";

	case 0x000f:
		return "WM_PAINT";

	case 0x0010:
		return "WM_CLOSE";

	case 0x0011:
		return "WM_QUERYENDSESSION";

	case 0x0012:
		return "WM_QUIT";

	case 0x0013:
		return "WM_QUERYOPEN";

	case 0x0014:
		return "WM_ERASEBKGND";

	case 0x0015:
		return "WM_SYSCOLORCHANGE";

	case 0x0016:
		return "WM_ENDSESSION";

	case 0x0017:
		return "WM_SYSTEMERROR";

	case 0x0018:
		return "WM_SHOWWINDOW";

	case 0x0019:
		return "WM_CTLCOLOR";

	case 0x001a:
		return "WM_WININICHANGE";

	case 0x001b:
		return "WM_DEVMODECHANGE";

	case 0x001c:
		return "WM_ACTIVATEAPP";

	case 0x001d:
		return "WM_FONTCHANGE";

	case 0x001e:
		return "WM_TIMECHANGE";

	case 0x001f:
		return "WM_CANCELMODE";

	case 0x0020:
		return "WM_SETCURSOR";

	case 0x0021:
		return "WM_MOUSEACTIVATE";

	case 0x0022:
		return "WM_CHILDACTIVATE";

	case 0x0023:
		return "WM_QUEUESYNC";

	case 0x0024:
		return "WM_GETMINMAXINFO";

	default:
		break;
	}
	return "--- OTHER ---";
}
	
static LRESULT WINAPI 
my_window_proc (HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
	FST* fst;

//	if (msg != WM_TIMER) {
//		fst_error ("window callback handler, msg = 0x%x (%s) win=%p\n", msg, message_name (msg), w);
//	}

	switch (msg) {
	case WM_KEYUP:
	case WM_KEYDOWN:
		break;

	case WM_CLOSE:
		PostQuitMessage (0);

	case WM_DESTROY:
	case WM_NCDESTROY:
		/* we should never get these */
		//return 0;
		break;

	case WM_PAINT:
		if ((fst = GetPropA (w, "fst_ptr")) != NULL) {
			if (fst->window && !fst->been_activated) {
				fst->been_activated = TRUE;
				pthread_cond_signal (&fst->window_status_change);
				pthread_mutex_unlock (&fst->lock);
			}
		}
		break;

	default:
		break;
	}

	return DefWindowProcA (w, msg, wp, lp );
}

static FST* 
fst_new ()
{
	FST* fst = (FST*) calloc (1, sizeof (FST));

	pthread_mutex_init (&fst->lock, NULL);
	pthread_cond_init (&fst->window_status_change, NULL);

	return fst;
}

static FSTHandle* 
fst_handle_new ()
{
	FSTHandle* fst = (FSTHandle*) calloc (1, sizeof (FSTHandle));
	return fst;
}

int
fst_create_editor (FST* fst)
{
	HMODULE hInst;
	HWND window;

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
				       0, 0, 1, 1,
				       NULL, NULL,
				       hInst,
				       NULL)) == NULL) {
		fst_error ("cannot create editor window");
		return 1;
	}

	if (!SetPropA (window, "fst_ptr", fst)) {
		fst_error ("cannot set fst_ptr on window");
	}

	fst->window = window;
	fst->xid = (int) GetPropA (window, "__wine_x11_whole_window");

	{
		struct ERect* er;

		ShowWindow (fst->window, SW_SHOW);
	
		fst->plugin->dispatcher (fst->plugin, effEditOpen, 0, 0, fst->window, 0 );
		fst->plugin->dispatcher (fst->plugin, effEditGetRect, 0, 0, &er, 0 );
		
		fst->width =  er->right-er->left;
		fst->height =  er->bottom-er->top;
		
		SetWindowPos (fst->window, 0, 0, 0, er->right-er->left+8, er->bottom-er->top+26, SWP_SHOWWINDOW|SWP_NOMOVE|SWP_NOZORDER);
	}

	return 0;
}

void
fst_destroy_editor (FST* fst)
{
	pthread_mutex_lock (&fst->lock);
	if (fst->window) {
		fst->destroy = TRUE;
		if (!PostThreadMessageA (gui_thread_id, WM_USER, 0, 0)) {
			fst_error ("could not post message to gui thread");
		}
		pthread_cond_wait (&fst->window_status_change, &fst->lock);

	}
	pthread_mutex_unlock (&fst->lock);
}

void
fst_event_loop_remove_plugin (FST* fst)
{
	FST* p;
	FST* prev;

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

void debreak( void ) { printf( "debreak\n" ); }

DWORD WINAPI gui_event_loop (LPVOID param)
{
	MSG msg;
	FST* fst;
	HMODULE hInst;
	HWND window;

	gui_thread_id = GetCurrentThreadId ();

	/* create a dummy window for timer events */

	if ((hInst = GetModuleHandleA (NULL)) == NULL) {
		fst_error ("can't get module handle");
		return 1;
	}
	
	if ((window = CreateWindowExA (0, "FST", "dummy",
				       WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
				       CW_USEDEFAULT, CW_USEDEFAULT,
				       CW_USEDEFAULT, CW_USEDEFAULT,
				       NULL, NULL,
				       hInst,
				       NULL )) == NULL) {
		fst_error ("cannot create dummy timer window");
	}

	if (!SetTimer (window, 1000, 100, NULL)) {
		fst_error ("cannot set timer on dummy window");
	}

	while (1) {

		GetMessageA (&msg, NULL, 0,0);

		if (msg.message == WM_SYSTEMERROR) {
			/* sent when this thread is supposed to exist */
			break;
		}
		
		if (msg.message == WM_KEYDOWN) debreak();

		TranslateMessage( &msg );
		DispatchMessageA (&msg);

		/* handle window creation requests, destroy requests, 
		   and run idle callbacks 
		*/
		
		if( msg.message == WM_TIMER ) {
		    pthread_mutex_lock (&plugin_mutex);
again:
		    for (fst = fst_first; fst; fst = fst->next) {

			if (fst->destroy) {
			    if (fst->window) {
				fst->plugin->dispatcher( fst->plugin, effEditClose, 0, 0, NULL, 0.0 );
				CloseWindow (fst->window);
				fst->window = NULL;
				fst->destroy = FALSE;
			    }
			    fst_event_loop_remove_plugin (fst);
			    fst->been_activated = FALSE;
			    pthread_mutex_lock (&fst->lock);
			    pthread_cond_signal (&fst->window_status_change);
			    pthread_mutex_unlock (&fst->lock);
			    goto again;
			} 

			if (fst->window == NULL) {
			    pthread_mutex_lock (&fst->lock);
			    if (fst_create_editor (fst)) {
				fst_error ("cannot create editor for plugin %s", fst->handle->name);
				fst_event_loop_remove_plugin (fst);
				pthread_cond_signal (&fst->window_status_change);
				pthread_mutex_unlock (&fst->lock);
				goto again;
			    }
			    /* condition/unlock handled when we receive WM_ACTIVATE */
			}

			fst->plugin->dispatcher (fst->plugin, effEditIdle, 0, 0, NULL, 0);
		    }
		    pthread_mutex_unlock (&plugin_mutex);
		}
	}
	fst_error ("FST GUI event loop has quit!");
	return 0;
}

int
fst_init ()
{
	WNDCLASSA wc;
	HMODULE hInst;

	if ((hInst = GetModuleHandleA (NULL)) == NULL) {
		fst_error ("can't get module handle");
		return -1;
	}
	wc.style = 0;
	wc.lpfnWndProc = my_window_proc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInst;
	wc.hIcon = LoadIconA( hInst, "FST");
	wc.hCursor = LoadCursorA( NULL, IDI_APPLICATION );
	wc.hbrBackground = GetStockObject( BLACK_BRUSH );
	wc.lpszMenuName = "MENU_FST";
	wc.lpszClassName = "FST";

	if (!RegisterClassA(&wc)){
		return 1;
	}

	if (CreateThread (NULL, 0, gui_event_loop, NULL, 0, NULL) == NULL) {
		fst_error ("could not create new thread proxy");
		return -1;
	}

	return 0;
}

void
fst_finish ()
{
	PostThreadMessageA (gui_thread_id, WM_SYSTEMERROR, 0, 0);
}

int
fst_run_editor (FST* fst)
{
	/* Add the FST to the list of all that should be handled by the GUI thread */

	pthread_mutex_lock (&plugin_mutex);

	if (fst_first == NULL) {
		fst_first = fst;
	} else {
		FST* p = fst_first;
		while (p->next) {
			p = p->next;
		}
		p->next = fst;
	}

	if (!PostThreadMessageA (gui_thread_id, WM_USER, 0, 0)) {
		fst_error ("could not post message to gui thread");
		return -1;
	}

	pthread_mutex_unlock (&plugin_mutex);

	/* wait for the plugin editor window to be created (or not) */

	pthread_mutex_lock (&fst->lock);
	if (!fst->window) {
		pthread_cond_wait (&fst->window_status_change, &fst->lock);
	} 
	pthread_mutex_unlock (&fst->lock);

	if (!fst->window) {
		fst_error ("no window created for VST plugin editor");
		return -1;
	}

	return 0;
}

FSTHandle*
fst_load (const char *path)
{
	char* buf;
	FSTHandle* fhandle;
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

	if ((fhandle->dll = LoadLibraryA (buf)) == NULL) {
		fst_unload (fhandle);
		return NULL;
	}

	if ((fhandle->main_entry = ((AEffect*)()(audioMasterCallback)) GetProcAddress (fhandle->dll, "main")) == NULL) {
		fst_unload (fhandle);
		return NULL;
	}

	return fhandle;
}

int
fst_unload (FSTHandle* fhandle)
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

FST*
fst_instantiate (FSTHandle* fhandle, audioMasterCallback amc, void* userptr)
{
	FST* fst = fst_new ();

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

	fst->handle->plugincnt++;

	return fst;
}

void
fst_close (FST* fst)
{
	fst_destroy_editor (fst);

	fst->plugin->dispatcher (fst->plugin, effMainsChanged, 0, 0, NULL, 0);
	fst->plugin->dispatcher (fst->plugin, effClose, 0, 0, 0, 0);

	if (fst->handle->plugincnt) {
		--fst->handle->plugincnt;
	}
}

int
fst_get_XID (FST* fst)
{
	return fst->xid;
}
