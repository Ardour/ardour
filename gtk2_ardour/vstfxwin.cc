/******************************************************************/
/** VSTFX - An engine based on FST for handling linuxVST plugins **/
/******************************************************************/

/*This is derived from the original FST (C code) with some tweaks*/


/** EDITOR tab stops at 4 **/

#include <stdlib.h>
#include <stdio.h>
#include <jack/jack.h>
#include <jack/thread.h>
#include <libgen.h>

#include <pthread.h>
#include <signal.h>
#include <glib.h>

#include "ardour/vstfx.h"

#include <X11/X.h>
#include <X11/Xlib.h>
#include <dlfcn.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

struct ERect{
    short top;
    short left;
    short bottom;
    short right;
};

static pthread_mutex_t plugin_mutex;

static VSTFX* vstfx_first = NULL;

const char magic[] = "VSTFX Plugin State v002";

int  gui_thread_id = 0;
static int gui_quit = 0;

/*This will be our connection to X*/

Display* LXVST_XDisplay = NULL;

/*The thread handle for the GUI event loop*/

pthread_t LXVST_gui_event_thread;

#define DELAYED_WINDOW 1

/*Util functions to get the value of a property attached to an XWindow*/

bool LXVST_xerror;

int TempErrorHandler(Display *display, XErrorEvent *e)
{
	LXVST_xerror = true;
	
	return 0;
}

#ifdef LXVST_32BIT

int getXWindowProperty(Window window, Atom atom)
{
	int result = 0;
	int userSize;
	unsigned long bytes;
	unsigned long userCount;
	unsigned char *data;
 	Atom userType;
	LXVST_xerror = false;
	
	/*Use our own Xerror handler while we're in here - in an
	attempt to stop the brain dead default Xerror behaviour of
	qutting the entire application because of e.g. an invalid
	window ID*/
	
	XErrorHandler olderrorhandler = XSetErrorHandler(TempErrorHandler);
 	
	XGetWindowProperty(	LXVST_XDisplay, 		//The display
						window, 				//The Window
						atom,					//The property
						0,						//Offset into the data
						1,						//Number of 32Bit chunks of data
						false,					//false = don't delete the property
						AnyPropertyType, 		//Required property type mask
	 					&userType,				//Actual type returned
						&userSize,				//Actual format returned
						&userCount,				//Actual number of items stored in the returned data
						&bytes,					//Number of bytes remaining if a partial read
						&data);					//The actual data read
						
	if(LXVST_xerror == false && userCount == 1)
 		result = *(int*)data;
		
	XSetErrorHandler(olderrorhandler);
	
	/*Hopefully this will return zero if the property is not set*/
	
	return result;
}

#endif

#ifdef LXVST_64BIT

/********************************************************************/
/* This is untested - have no 64Bit plugins which use this          */
/* system of passing an eventProc address                           */
/********************************************************************/

long getXWindowProperty(Window window, Atom atom)
{
	long result = 0;
	int userSize;
	unsigned long bytes;
	unsigned long userCount;
	unsigned char *data;
 	Atom userType;
	LXVST_xerror = false;
	
	/*Use our own Xerror handler while we're in here - in an
	attempt to stop the brain dead default Xerror behaviour of
	qutting the entire application because of e.g. an invalid
	window ID*/
	
	XErrorHandler olderrorhandler = XSetErrorHandler(TempErrorHandler);
 	
	XGetWindowProperty(	LXVST_XDisplay, 
						window, 
						atom,
						0,
						2,
						false,
						AnyPropertyType, 
	 					&userType,
						&userSize,
						&userCount,
						&bytes,
						&data);
						
	if(LXVST_xerror == false && userCount == 1)
 		result = *(long*)data;
		
	XSetErrorHandler(olderrorhandler);
	
	/*Hopefully this will return zero if the property is not set*/
	
	return result;
}

#endif

/*The event handler - called from within the main GUI thread to
dispatch events to any VST UIs which have callbacks stuck to them*/

static void dispatch_x_events(XEvent* event, VSTFX* vstfx)
{
	/*Handle some of the Events we might be interested in*/
	
	switch(event->type)
	{
		/*Configure event - when the window is resized or first drawn*/
			
		case ConfigureNotify:
		{
			Window window = event->xconfigure.event;
			
			int width = event->xconfigure.width;
			int height = event->xconfigure.height;
			
			/*If we get a config notify on the parent window XID then we need to see
			if the size has been changed - some plugins re-size their UI window e.g.
			when opening a preset manager (you might think that should be spawned as a new window...) */
			
			/*if the size has changed, we flag this so that in lxvst_pluginui.cc we can make the
			change to the GTK parent window in ardour, from its UI thread*/ 
			
			if(window == (Window)(vstfx->window))
			{
				if((width!=vstfx->width) || (height!=vstfx->height))
				{
					vstfx->width = width;
					vstfx->height = height;
					vstfx->want_resize = 1;
					
					/*QUIRK : Loomer plugins not only resize the UI but throw it into some random
					position at the same time. We need to re-position the window at the origin of
					the parent window*/
					
					if(vstfx->plugin_ui_window)
						XMoveWindow(LXVST_XDisplay, vstfx->plugin_ui_window, 0, 0);
				}
			}
			
			break;
			
		}
		
		/*Reparent Notify - when the plugin UI is reparented into
		our Host Window we will get an event here... probably... */
		
		case ReparentNotify:
		{
			Window ParentWindow = event->xreparent.parent;
			
			/*If the ParentWindow matches the window for the vstfx instance then
			the Child window must be the XID of the pluginUI window created by the
			plugin, so we need to see if it has a callback stuck to it, and if so
			set that up in the vstfx */
			
			/***********************************************************/
			/* 64Bit --- This mechanism is not 64Bit compatible at the */
			/* present time                                            */
			/***********************************************************/
			
			if(ParentWindow == (Window)(vstfx->window))
			{
				Window PluginUIWindowID = event->xreparent.window;
				
				vstfx->plugin_ui_window = PluginUIWindowID;
#ifdef LXVST_32BIT
				int result = getXWindowProperty(PluginUIWindowID, XInternAtom(LXVST_XDisplay, "_XEventProc", false));
	
				if(result == 0)
					vstfx->eventProc = NULL;
				else
					vstfx->eventProc = (void (*) (void* event))result;
#endif
#ifdef LXVST_64BIT
				long result = getXWindowProperty(PluginUIWindowID, XInternAtom(LXVST_XDisplay, "_XEventProc", false));
	
				if(result == 0)
					vstfx->eventProc = NULL;
				else
					vstfx->eventProc = (void (*) (void* event))result;
#endif
			}
			break;
		}
		
		case ClientMessage:
		{
			Window window = event->xany.window;
			Atom message_type = event->xclient.message_type;
			
			/*The only client message we are interested in is to signal
			that the plugin parent window is now valid and can be passed
			to effEditOpen when the editor is launched*/
			
			if(window == (Window)(vstfx->window))
			{
				char* message = XGetAtomName(LXVST_XDisplay, message_type);
				
				if(strcmp(message,"LaunchEditor") == 0)
				{
				
					if(event->xclient.data.l[0] == 0x0FEEDBAC)
						vstfx_launch_editor(vstfx);
				}
				
				XFree(message);
			}
			break;
		}
		
		default:
			break;
	}
	
	/* Some VSTs built with toolkits e.g. JUCE will manager their own UI
	autonomously in the plugin, running the UI in its own thread, so once
	we have created a parent window for the plugin, its UI takes care of
	itself.*/
	
	/*Other types register a callback as an Xwindow property on the plugin
	UI window after they create it.  If that is the case, we need to call it
	here, passing the XEvent into it*/
	
	if(vstfx->eventProc == NULL)
		return;
				
	vstfx->eventProc((void*)event);
}

/** This is the main gui event loop for the plugin, we also need to pass
any Xevents to all the UI callbacks plugins 'may' have registered on their
windows, that is if they don't manage their own UIs **/

void* gui_event_loop (void* ptr)
{

	VSTFX* vstfx;
	int LXVST_sched_event_timer = 0;
	int LXVST_sched_timer_interval = 50; //ms
	XEvent event;
	
	/*The 'Forever' loop - runs the plugin UIs etc - based on the FST gui event loop*/
	
	while (!gui_quit)
	{
		/* handle window creation requests, destroy requests, 
		   and run idle callbacks */

		/*Look at the XEvent queue - if there are any XEvents we need to handle them,
		including passing them to all the plugin (eventProcs) we are currently managing*/
		
		if(LXVST_XDisplay)
		{
			/*See if there are any events in the queue*/
		
			int num_events = XPending(LXVST_XDisplay);
			
			/*process them if there are any*/
		
			while(num_events)
			{
				XNextEvent(LXVST_XDisplay, &event);
				
				/*Call dispatch events, with the event, for each plugin in the linked list*/
				
				for (vstfx = vstfx_first; vstfx; vstfx = vstfx->next)
				{	
					pthread_mutex_lock(&vstfx->lock);
					
					dispatch_x_events(&event, vstfx);
					
					pthread_mutex_unlock(&vstfx->lock);
				}
				
				num_events--;
			}
		}

		/*We don't want to use all the CPU.. */

		usleep(1000);
		
		LXVST_sched_event_timer++;
		
		LXVST_sched_event_timer = LXVST_sched_event_timer & 0x00FFFFFF;

		/*See if its time for us to do a scheduled event pass on all the plugins*/

		if((LXVST_sched_timer_interval!=0) && (!(LXVST_sched_event_timer% LXVST_sched_timer_interval)))
		{
			pthread_mutex_lock (&plugin_mutex);
		    
again:
			/*Parse through the linked list of plugins*/
			
			for (vstfx = vstfx_first; vstfx; vstfx = vstfx->next)
			{	
				pthread_mutex_lock (&vstfx->lock);

				/*Window scheduled for destruction*/
				
				if (vstfx->destroy)
				{
					if (vstfx->window)
					{
						vstfx->plugin->dispatcher( vstfx->plugin, effEditClose, 0, 0, NULL, 0.0 );
							
						XDestroyWindow (LXVST_XDisplay, vstfx->window);
						vstfx->window = 0;				//FIXME - probably safe to assume we never have an XID of 0 but not explicitly true
						vstfx->destroy = FALSE;
					}
					
					vstfx_event_loop_remove_plugin (vstfx);
					vstfx->been_activated = FALSE;
					pthread_cond_signal (&vstfx->window_status_change);
					pthread_mutex_unlock (&vstfx->lock);
					
					goto again;
				} 
				
				/*Window does not yet exist - scheduled for creation*/
				
				if (vstfx->window == 0)		//FIXME - probably safe to assume 0 is not a valid XID but not explicitly true
				{
					if (vstfx_create_editor (vstfx))
					{
						vstfx_error ("** ERROR ** VSTFX : Cannot create editor for plugin %s", vstfx->handle->name);
						vstfx_event_loop_remove_plugin (vstfx);
						pthread_cond_signal (&vstfx->window_status_change);
						pthread_mutex_unlock (&vstfx->lock);
						goto again;
					}
					else
					{
						/* condition/unlock: it was signalled & unlocked in fst_create_editor()   */
					}
				}

				/*Scheduled for setting a new program*/

				if (vstfx->want_program != -1 )
				{
					if (vstfx->vst_version >= 2)
					{
						vstfx->plugin->dispatcher (vstfx->plugin, 67 /* effBeginSetProgram */, 0, 0, NULL, 0);
					}

					vstfx->plugin->dispatcher (vstfx->plugin, effSetProgram, 0, vstfx->want_program, NULL, 0);

					if (vstfx->vst_version >= 2)
					{
						vstfx->plugin->dispatcher (vstfx->plugin, 68 /* effEndSetProgram */, 0, 0, NULL, 0);
					}
					
					/* did it work? */
					
					vstfx->current_program = vstfx->plugin->dispatcher (vstfx->plugin, 3, /* effGetProgram */ 0, 0, NULL, 0);
					vstfx->want_program = -1; 
				}
				
				/*scheduled call to dispatcher*/
				
				if (vstfx->dispatcher_wantcall) {
					vstfx->dispatcher_retval = vstfx->plugin->dispatcher (
						vstfx->plugin, 
						vstfx->dispatcher_opcode,
						vstfx->dispatcher_index,
						vstfx->dispatcher_val,
						vstfx->dispatcher_ptr,
						vstfx->dispatcher_opt
						);
					
					vstfx->dispatcher_wantcall = 0;
					pthread_cond_signal (&vstfx->plugin_dispatcher_called);
				}
				
				/*Call the editor Idle function in the plugin*/
				
				vstfx->plugin->dispatcher (vstfx->plugin, effEditIdle, 0, 0, NULL, 0);

				if(vstfx->wantIdle)
					vstfx->plugin->dispatcher (vstfx->plugin, 53, 0, 0, NULL, 0);
					
				pthread_mutex_unlock (&vstfx->lock);
			}
			pthread_mutex_unlock (&plugin_mutex);
		}
	}

	/*Drop out to here if we set gui_quit to 1 */

	return NULL;
}

/*The VSTFX Init function - this needs to be called before the VSTFX engine
can be accessed, it gets the UI thread running, opens a connection to X etc
normally started in globals.cc*/

int vstfx_init (void* ptr)
{

	int thread_create_result;
	
	pthread_attr_t thread_attributes;
	
	/*Init the attribs to defaults*/
	
	pthread_attr_init(&thread_attributes);
	
	/*Make sure the thread is joinable - this should be the default anyway - 
	so we can join to it on vstfx_exit*/
	
	pthread_attr_setdetachstate(&thread_attributes, PTHREAD_CREATE_JOINABLE);
	

	/*This is where we need to open a connection to X, and start the GUI thread*/
	
	/*Open our connection to X - all linuxVST plugin UIs handled by the LXVST engine
	will talk to X down this connection - X cannot handle multi-threaded access via
	the same Display* */
	
	if(LXVST_XDisplay==NULL)
		LXVST_XDisplay = XOpenDisplay(NULL);	//We might be able to make this open a specific screen etc

	/*Drop out and report the error if we fail to connect to X */
	
	if(LXVST_XDisplay==NULL)
	{
		vstfx_error ("** ERROR ** VSTFX: Failed opening connection to X");
		
		return -1;
	}
	
	/*We have a connection to X - so start the gui event loop*/
	
	/*Create the thread - use default attrs for now, don't think we need anything special*/
	
	thread_create_result = pthread_create(&LXVST_gui_event_thread, NULL, gui_event_loop, NULL);
	
	if(thread_create_result!=0)
	{
		/*There was a problem starting the GUI event thread*/
		
		vstfx_error ("** ERROR ** VSTFX: Failed starting GUI event thread");
		
		XCloseDisplay(LXVST_XDisplay);
		
		return -1;
	}
	
	return 0;
}

/*The vstfx Quit function*/

void vstfx_exit()
{
	gui_quit = 1;
	
	/*We need to pthread_join the gui_thread here so
	we know when it has stopped*/
	
	pthread_join(LXVST_gui_event_thread, NULL);
}

/*Adds a new plugin (VSTFX) instance to the linked list*/

int vstfx_run_editor (VSTFX* vstfx)
{
	pthread_mutex_lock (&plugin_mutex);

	/*Add the new VSTFX instance to the linked list*/

	if (vstfx_first == NULL)
	{
		vstfx_first = vstfx;
	}
	else
	{
		VSTFX* p = vstfx_first;
		
		while (p->next)
		{
			p = p->next;
		}
		p->next = vstfx;
		
		/*Mark the new end of the list*/
		
		vstfx->next = NULL;
	}

	pthread_mutex_unlock (&plugin_mutex);

	/* wait for the plugin editor window to be created (or not) */

	pthread_mutex_lock (&vstfx->lock);
	
	if (!vstfx->window)
	{
		pthread_cond_wait (&vstfx->window_status_change, &vstfx->lock);
	}
	
	pthread_mutex_unlock (&vstfx->lock);

	if (!vstfx->window)
	{
		return -1;
	}

	return 0;
}


/*Creates an editor for the plugin - normally called from within the gui event loop
after run_editor has added the plugin (editor) to the linked list*/

int vstfx_create_editor (VSTFX* vstfx)
{
	Window parent_window;
	
	int x_size = 1;
	int y_size = 1;

	/* Note: vstfx->lock is held while this function is called */

	if (!(vstfx->plugin->flags & effFlagsHasEditor))
	{
		vstfx_error ("** ERROR ** VSTFX: Plugin \"%s\" has no editor", vstfx->handle->name);
		return -1;
	}
	
	
	/*Create an XWindow for the plugin to inhabit*/
	
	parent_window = XCreateSimpleWindow (
		LXVST_XDisplay,
		DefaultRootWindow(LXVST_XDisplay),
		0,
		0,
		x_size,
		y_size,
		0,
		0,
		0
		);
										
	/*Select the events we are interested in receiving - we need Substructure notify so that
	if the plugin resizes its window - e.g. Loomer Manifold then we get a message*/
	
	XSelectInput(LXVST_XDisplay, 
				parent_window,
				SubstructureNotifyMask | ButtonPressMask | ButtonReleaseMask | ButtonMotionMask | ExposureMask);
										
	vstfx->window = parent_window;
										
	vstfx->xid = parent_window;  //vstfx->xid will be referenced to connect to GTK UI in ardour later
	
	/*Because the plugin may be operating on a different Display* to us, and therefore
	the two event queues can be asynchronous, although we have created the window on
	our display, we can't guarantee it exists in the server yet, which will
	cause BadWindow crashes if the plugin tries to use it.
	
	It would be nice to use CreateNotify events here, but they don't get
	through on all window managers, so instead we pass a client message
	into out queue, after the XCreateWindow.  When this message pops out
	in our event handler, it will trigger the second stage of plugin
	Editor instantiation, and by then the Window should be valid...*/
	
	XClientMessageEvent event;
	
	/*Create an atom to identify our message (only if it doesn't already exist)*/
	
	Atom WindowActiveAtom = XInternAtom(LXVST_XDisplay, "LaunchEditor", false);
	
	event.type = ClientMessage;
	event.send_event = true;
	event.window = parent_window;
	event.message_type = WindowActiveAtom;

	event.format = 32;						//Data format
	event.data.l[0] = 0x0FEEDBAC;			//Something we can recognize later
	
	/*Push the event into the queue on our Display*/
	
	XSendEvent(LXVST_XDisplay, parent_window, FALSE, NoEventMask, (XEvent*)&event);

	/*Unlock - and we are done for the first part of staring the Editor...*/
	
	pthread_mutex_unlock (&vstfx->lock);
	
	return 0;
}

int vstfx_launch_editor(VSTFX* vstfx)
{
	/*This is the second stage of launching the editor (see vstfx_create editor)
	we get called here in response to receiving the ClientMessage on our Window,
	therefore it's about as safe (as can be) to assume that the Window we created
	is now valid in the XServer and can be passed to the plugin in effEditOpen
	without generating BadWindow errors when the plugin reparents itself into our
	parent window*/
	
	if(vstfx->been_activated)
		return 0;
	
	Window parent_window;
	struct ERect* er;
	
	int x_size = 1;
	int y_size = 1;
	
	parent_window = vstfx->window;
	
	/*Open the editor - Bah! we have to pass the int windowID as a void pointer - yuck
	it gets cast back to an int as the parent window XID in the plugin - and we have to pass the
	Display* as a long */
	
	/**************************************************************/
	/* 64Bit --- parent window is an int passed as a void* so     */
	/* that should be ok for 64Bit machines                       */
	/*                                                            */
	/* Display is passed in as a long - ok on arch's where sizeof */
	/* long = 8                                                   */
	/*                                                            */
	/* Most linux VST plugins open a connection to X on their own */
	/* Display anyway so it may not matter                        */
	/*                                                            */
	/* linuxDSP VSTs don't use the host Display* at all           */
	/**************************************************************/
	
	vstfx->plugin->dispatcher (vstfx->plugin, effEditOpen, 0, (long)LXVST_XDisplay, (void*)(parent_window), 0 );
	
	/*QUIRK - some plugins need a slight delay after opening the editor before you can
	ask the window size or they might return zero - specifically discoDSP */
	
	usleep(100000);
	
	/*Now we can find out how big the parent window should be (and try) to resize it*/
	
	vstfx->plugin->dispatcher (vstfx->plugin, effEditGetRect, 0, 0, &er, 0 );

	x_size = er->right - er->left;
	y_size = er->bottom - er->top;
	
	vstfx->width =  x_size;
	vstfx->height =  y_size;
	
	XResizeWindow(LXVST_XDisplay, parent_window, x_size, y_size);
	
	XFlush (LXVST_XDisplay);
	
	/*Not sure if we need to map the window or if the plugin will do it for us
	it should be ok because XReparentWindow generates a Map event*/
	
	/*mark the editor as activated - mainly so that vstfx_get_XID
	will know it is valid*/

	vstfx->been_activated = TRUE;
	
	pthread_cond_signal (&vstfx->window_status_change);
	return 0;
}

/*May not be needed in the XLib version*/

void vstfx_move_window_into_view (VSTFX* vstfx)
{

	/*This is probably the equivalent of Mapping an XWindow
	but we most likely don't need it because the window
	will be Mapped by XReparentWindow*/

}

/*Destroy the editor window*/

void vstfx_destroy_editor (VSTFX* vstfx)
{
	pthread_mutex_lock (&vstfx->lock);
	if (vstfx->window)
	{
		vstfx->destroy = TRUE;
		pthread_cond_wait (&vstfx->window_status_change, &vstfx->lock);
	}
	pthread_mutex_unlock (&vstfx->lock);
}

/*Remove a vstfx instance from the linked list parsed by the
event loop*/

void vstfx_event_loop_remove_plugin (VSTFX* vstfx)
{
	/*This only ever gets called from within our GUI thread
	so we don't need to lock here - if we did there would be
	a deadlock anyway*/
	
	VSTFX* p;
	VSTFX* prev;
	
	for(p = vstfx_first, prev = NULL; p; prev = p, p = p->next)
	{
		if(p == vstfx)
		{
			if(prev)
			{
				prev->next = p->next;
				break;
			}
		}
	}

	if (vstfx_first == vstfx)
		vstfx_first = vstfx_first->next;
}

/*Get the XID of the plugin editor window*/

int vstfx_get_XID (VSTFX* vstfx)
{
	int id;
	
	/*Wait for the lock to become free - otherwise
	the window might be in the process of being
	created and we get bad Window errors when trying
	to embed it in the GTK UI*/
	
	pthread_mutex_lock(&vstfx->lock);
	
	/*The Window may be scheduled for creation
	but not actually created by the gui_event_loop yet - 
	
	spin here until it has been activated.  Possible
	deadlock if the window never gets activated but
	should not be called here if the window doesn't
	exist or will never exist*/
	
	while(!(vstfx->been_activated))
		usleep(1000);
	
	id = vstfx->xid;
	
	pthread_mutex_unlock(&vstfx->lock);
	
	/*Finally it might be safe to return the ID - 
	problems will arise if we return either a zero ID
	and GTK tries to socket it or if we return an ID
	which hasn't yet become real to the server*/
	
	return id;
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


/*load_state and save_state do not appear to be needed (yet) in ardour
- untested at the moment, these are just replicas of the fst code*/


#if 0
int vstfx_load_state (VSTFX* vstfx, char * filename)
{
	FILE* f = fopen (filename, "rb");
	if(f)
	{
		char testMagic[sizeof (magic)];
		fread (&testMagic, sizeof (magic), 1, f);
		if (strcmp (testMagic, magic))
		{
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

		success = vstfx_call_dispatcher(vstfx, effGetProductString, 0, 0, testString, 0);
		
		if (success == 1)
		{
			if (strcmp (testString, productString) != 0)
			{
				printf ("Product string mismatch! Plugin has: %s\n", testString);
				fclose (f);
				return FALSE;
			}
		}
		else if (length != 0)
		{
			printf ("Product string mismatch! Plugin has none.\n", testString);
			fclose (f);
			return FALSE;
		}

		fread (&length, sizeof (unsigned), 1, f);
		length = htonl (length);
		fread (effectName, length, 1, f);
		effectName[length] = 0;
		printf ("Effect name: %s\n", effectName);

		success = vstfx_call_dispatcher(vstfx, effGetEffectName, 0, 0, testString, 0);
		
		if(success == 1)
		{
			if(strcmp(testString, effectName)!= 0)
			{
				printf ("Effect name mismatch! Plugin has: %s\n", testString);
				fclose (f);
				return FALSE;
			}
		}
		else if(length != 0)
		{
			printf ("Effect name mismatch! Plugin has none.\n", testString);
			fclose (f);
			return FALSE;
		}

		fread (&length, sizeof (unsigned), 1, f);
		length = htonl (length);
		fread (vendorString, length, 1, f);
		vendorString[length] = 0;
		
		printf ("Vendor string: %s\n", vendorString);

		success = vstfx_call_dispatcher(vstfx, effGetVendorString, 0, 0, testString, 0);
		if(success == 1)
		{
			if (strcmp(testString, vendorString)!= 0)
			{
				printf ("Vendor string mismatch! Plugin has: %s\n", testString);
				fclose (f);
				return FALSE;
			}
		}
		else if(length != 0)
		{
			printf ("Vendor string mismatch! Plugin has none.\n", testString);
			fclose (f);
			return FALSE;
		}

		int numParam;
		unsigned i;
		fread (&numParam, sizeof (int), 1, f);
		numParam = htonl (numParam);
		
		for (i = 0; i < numParam; ++i)
		{
			float val;
			fread (&val, sizeof (float), 1, f);
			val = htonf (val);

			pthread_mutex_lock(&vstfx->lock );
			vstfx->plugin->setParameter(vstfx->plugin, i, val);
			pthread_mutex_unlock(&vstfx->lock );
		}

		int bytelen;
		
		fread (&bytelen, sizeof (int), 1, f);
		bytelen = htonl (bytelen);
		
		if (bytelen)
		{
			char * buf = malloc (bytelen);
			fread (buf, bytelen, 1, f);

			vstfx_call_dispatcher(vstfx, 24, 0, bytelen, buf, 0);
			free (buf);
		}
	}
	else
	{
		printf ("Could not open state file\n");
		return FALSE;
	}
	return TRUE;

}
#endif

