/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#include <glib/gprintf.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#ifdef HAVE_XKB
#include <X11/XKBlib.h>
#endif

#include "gdk.h"

#include "gdkx.h"
#include "gdkasync.h"
#include "gdkdisplay-x11.h"
#include "gdkinternals.h"
#include "gdkintl.h"
#include "gdkregion-generic.h"
#include "gdkinputprivate.h"
#include "gdkalias.h"

typedef struct _GdkPredicate  GdkPredicate;
typedef struct _GdkErrorTrap  GdkErrorTrap;

struct _GdkPredicate
{
  GdkEventFunc func;
  gpointer data;
};

struct _GdkErrorTrap
{
  int (*old_handler) (Display *, XErrorEvent *);
  gint error_warnings;
  gint error_code;
};

/* 
 * Private function declarations
 */

#ifndef HAVE_XCONVERTCASE
static void	 gdkx_XConvertCase	(KeySym	       symbol,
					 KeySym	      *lower,
					 KeySym	      *upper);
#define XConvertCase gdkx_XConvertCase
#endif

static int	    gdk_x_error			 (Display     *display, 
						  XErrorEvent *error);
static int	    gdk_x_io_error		 (Display     *display);

/* Private variable declarations
 */
static GSList *gdk_error_traps = NULL;               /* List of error traps */
static GSList *gdk_error_trap_free_list = NULL;      /* Free list */

const GOptionEntry _gdk_windowing_args[] = {
  { "sync", 0, 0, G_OPTION_ARG_NONE, &_gdk_synchronize, 
    /* Description of --sync in --help output */ N_("Make X calls synchronous"), NULL },
  { NULL }
};

void
_gdk_windowing_init (void)
{
  _gdk_x11_initialize_locale ();
  
  XSetErrorHandler (gdk_x_error);
  XSetIOErrorHandler (gdk_x_io_error);

  _gdk_selection_property = gdk_atom_intern_static_string ("GDK_SELECTION");
}

void
gdk_set_use_xshm (gboolean use_xshm)
{
}

gboolean
gdk_get_use_xshm (void)
{
  return GDK_DISPLAY_X11 (gdk_display_get_default ())->use_xshm;
}

static GdkGrabStatus
gdk_x11_convert_grab_status (gint status)
{
  switch (status)
    {
    case GrabSuccess:
      return GDK_GRAB_SUCCESS;
    case AlreadyGrabbed:
      return GDK_GRAB_ALREADY_GRABBED;
    case GrabInvalidTime:
      return GDK_GRAB_INVALID_TIME;
    case GrabNotViewable:
      return GDK_GRAB_NOT_VIEWABLE;
    case GrabFrozen:
      return GDK_GRAB_FROZEN;
    }

  g_assert_not_reached();

  return 0;
}

static void
has_pointer_grab_callback (GdkDisplay *display,
			   gpointer data,
			   gulong serial)
{
  _gdk_display_pointer_grab_update (display, serial);
}

GdkGrabStatus
_gdk_windowing_pointer_grab (GdkWindow *window,
			     GdkWindow *native,
			     gboolean owner_events,
			     GdkEventMask event_mask,
			     GdkWindow *confine_to,
			     GdkCursor *cursor,
			     guint32 time)
{
  gint return_val;
  GdkCursorPrivate *cursor_private;
  GdkDisplayX11 *display_x11;
  guint xevent_mask;
  Window xwindow;
  Window xconfine_to;
  Cursor xcursor;
  int i;

  if (confine_to)
    confine_to = _gdk_window_get_impl_window (confine_to);

  display_x11 = GDK_DISPLAY_X11 (GDK_WINDOW_DISPLAY (native));

  cursor_private = (GdkCursorPrivate*) cursor;

  xwindow = GDK_WINDOW_XID (native);

  if (!confine_to || GDK_WINDOW_DESTROYED (confine_to))
    xconfine_to = None;
  else
    xconfine_to = GDK_WINDOW_XID (confine_to);

  if (!cursor)
    xcursor = None;
  else
    {
      _gdk_x11_cursor_update_theme (cursor);
      xcursor = cursor_private->xcursor;
    }

  xevent_mask = 0;
  for (i = 0; i < _gdk_nenvent_masks; i++)
    {
      if (event_mask & (1 << (i + 1)))
	xevent_mask |= _gdk_event_mask_table[i];
    }

  /* We don't want to set a native motion hint mask, as we're emulating motion
   * hints. If we set a native one we just wouldn't get any events.
   */
  xevent_mask &= ~PointerMotionHintMask;

  return_val = _gdk_input_grab_pointer (window,
					native,
					owner_events,
					event_mask,
					confine_to,
					time);

  if (return_val == GrabSuccess ||
      G_UNLIKELY (!display_x11->trusted_client && return_val == AlreadyGrabbed))
    {
      if (!GDK_WINDOW_DESTROYED (native))
	{
#ifdef G_ENABLE_DEBUG
	  if (_gdk_debug_flags & GDK_DEBUG_NOGRABS)
	    return_val = GrabSuccess;
	  else
#endif
	    return_val = XGrabPointer (GDK_WINDOW_XDISPLAY (native),
				       xwindow,
				       owner_events,
				       xevent_mask,
				       GrabModeAsync, GrabModeAsync,
				       xconfine_to,
				       xcursor,
				       time);
	}
      else
	return_val = AlreadyGrabbed;
    }

  if (return_val == GrabSuccess)
    _gdk_x11_roundtrip_async (GDK_DISPLAY_OBJECT (display_x11),
			      has_pointer_grab_callback,
			      NULL);

  return gdk_x11_convert_grab_status (return_val);
}

/*
 *--------------------------------------------------------------
 * gdk_keyboard_grab
 *
 *   Grabs the keyboard to a specific window
 *
 * Arguments:
 *   "window" is the window which will receive the grab
 *   "owner_events" specifies whether events will be reported as is,
 *     or relative to "window"
 *   "time" specifies the time
 *
 * Results:
 *
 * Side effects:
 *   requires a corresponding call to gdk_keyboard_ungrab
 *
 *--------------------------------------------------------------
 */

GdkGrabStatus
gdk_keyboard_grab (GdkWindow *	   window,
		   gboolean	   owner_events,
		   guint32	   time)
{
  gint return_val;
  unsigned long serial;
  GdkDisplay *display;
  GdkDisplayX11 *display_x11;
  GdkWindow *native;

  g_return_val_if_fail (window != NULL, 0);
  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);

  native = gdk_window_get_toplevel (window);

  /* TODO: What do we do for offscreens and  children? We need to proxy the grab somehow */
  if (!GDK_IS_WINDOW_IMPL_X11 (GDK_WINDOW_OBJECT (native)->impl))
    return GDK_GRAB_SUCCESS;

  display = GDK_WINDOW_DISPLAY (native);
  display_x11 = GDK_DISPLAY_X11 (display);

  serial = NextRequest (GDK_WINDOW_XDISPLAY (native));

  if (!GDK_WINDOW_DESTROYED (native))
    {
#ifdef G_ENABLE_DEBUG
      if (_gdk_debug_flags & GDK_DEBUG_NOGRABS)
	return_val = GrabSuccess;
      else
#endif
	return_val = XGrabKeyboard (GDK_WINDOW_XDISPLAY (native),
				    GDK_WINDOW_XID (native),
				    owner_events,
				    GrabModeAsync, GrabModeAsync,
				    time);
	if (G_UNLIKELY (!display_x11->trusted_client && 
			return_val == AlreadyGrabbed))
	  /* we can't grab the keyboard, but we can do a GTK-local grab */
	  return_val = GrabSuccess;
    }
  else
    return_val = AlreadyGrabbed;

  if (return_val == GrabSuccess)
    _gdk_display_set_has_keyboard_grab (display,
					window,	native,
					owner_events,
					serial, time);

  return gdk_x11_convert_grab_status (return_val);
}

/**
 * _gdk_xgrab_check_unmap:
 * @window: a #GdkWindow
 * @serial: serial from Unmap event (or from NextRequest(display)
 *   if the unmap is being done by this client.)
 * 
 * Checks to see if an unmap request or event causes the current
 * grab window to become not viewable, and if so, clear the
 * the pointer we keep to it.
 **/
void
_gdk_xgrab_check_unmap (GdkWindow *window,
			gulong     serial)
{
  GdkDisplay *display = gdk_drawable_get_display (window);

  _gdk_display_end_pointer_grab (display, serial, window, TRUE);

  if (display->keyboard_grab.window &&
      serial >= display->keyboard_grab.serial)
    {
      GdkWindowObject *private = GDK_WINDOW_OBJECT (window);
      GdkWindowObject *tmp = GDK_WINDOW_OBJECT (display->keyboard_grab.window);

      while (tmp && tmp != private)
	tmp = tmp->parent;

      if (tmp)
	_gdk_display_unset_has_keyboard_grab (display, TRUE);
    }
}

/**
 * _gdk_xgrab_check_destroy:
 * @window: a #GdkWindow
 * 
 * Checks to see if window is the current grab window, and if
 * so, clear the current grab window.
 **/
void
_gdk_xgrab_check_destroy (GdkWindow *window)
{
  GdkDisplay *display = gdk_drawable_get_display (window);
  GdkPointerGrabInfo *grab;

  /* Make sure there is no lasting grab in this native
     window */
  grab = _gdk_display_get_last_pointer_grab (display);
  if (grab && grab->native_window == window)
    {
      /* We don't know the actual serial to end, but it
	 doesn't really matter as this only happens
	 after we get told of the destroy from the
	 server so we know its ended in the server,
	 just make sure its ended. */
      grab->serial_end = grab->serial_start;
      grab->implicit_ungrab = TRUE;
    }
  
  if (window == display->keyboard_grab.native_window &&
      display->keyboard_grab.window != NULL)
    _gdk_display_unset_has_keyboard_grab (display, TRUE);
}

void
_gdk_windowing_display_set_sm_client_id (GdkDisplay  *display,
					 const gchar *sm_client_id)
{
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (display);

  if (display->closed)
    return;

  if (sm_client_id && strcmp (sm_client_id, ""))
    {
      XChangeProperty (display_x11->xdisplay, display_x11->leader_window,
		       gdk_x11_get_xatom_by_name_for_display (display, "SM_CLIENT_ID"),
		       XA_STRING, 8, PropModeReplace, (guchar *)sm_client_id,
		       strlen (sm_client_id));
    }
  else
    XDeleteProperty (display_x11->xdisplay, display_x11->leader_window,
		     gdk_x11_get_xatom_by_name_for_display (display, "SM_CLIENT_ID"));
}

/**
 * gdk_x11_set_sm_client_id:
 * @sm_client_id: the client id assigned by the session manager when the
 *    connection was opened, or %NULL to remove the property.
 *
 * Sets the <literal>SM_CLIENT_ID</literal> property on the application's leader window so that
 * the window manager can save the application's state using the X11R6 ICCCM
 * session management protocol.
 *
 * See the X Session Management Library documentation for more information on
 * session management and the Inter-Client Communication Conventions Manual
 *
 * Since: 2.24
 */
void
gdk_x11_set_sm_client_id (const gchar *sm_client_id)
{
  gdk_set_sm_client_id (sm_client_id);
}

/* Close all open displays
 */
void
_gdk_windowing_exit (void)
{
  GSList *tmp_list = _gdk_displays;
    
  while (tmp_list)
    {
      XCloseDisplay (GDK_DISPLAY_XDISPLAY (tmp_list->data));
      
      tmp_list = tmp_list->next;
  }
}

/*
 *--------------------------------------------------------------
 * gdk_x_error
 *
 *   The X error handling routine.
 *
 * Arguments:
 *   "display" is the X display the error orignated from.
 *   "error" is the XErrorEvent that we are handling.
 *
 * Results:
 *   Either we were expecting some sort of error to occur,
 *   in which case we set the "_gdk_error_code" flag, or this
 *   error was unexpected, in which case we will print an
 *   error message and exit. (Since trying to continue will
 *   most likely simply lead to more errors).
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

static int
gdk_x_error (Display	 *display,
	     XErrorEvent *error)
{
  if (error->error_code)
    {
      if (_gdk_error_warnings)
	{
	  gchar buf[64];
          gchar *msg;
          
	  XGetErrorText (display, error->error_code, buf, 63);

          msg =
            g_strdup_printf ("The program '%s' received an X Window System error.\n"
                             "This probably reflects a bug in the program.\n"
                             "The error was '%s'.\n"
                             "  (Details: serial %ld error_code %d request_code %d minor_code %d)\n"
                             "  (Note to programmers: normally, X errors are reported asynchronously;\n"
                             "   that is, you will receive the error a while after causing it.\n"
                             "   To debug your program, run it with the --sync command line\n"
                             "   option to change this behavior. You can then get a meaningful\n"
                             "   backtrace from your debugger if you break on the gdk_x_error() function.)",
                             g_get_prgname (),
                             buf,
                             error->serial, 
                             error->error_code, 
                             error->request_code,
                             error->minor_code);
          
#ifdef G_ENABLE_DEBUG	  
	  g_error ("%s", msg);
#else /* !G_ENABLE_DEBUG */
	  g_fprintf (stderr, "%s\n", msg);

	  exit (1);
#endif /* G_ENABLE_DEBUG */
	}
      _gdk_error_code = error->error_code;
    }
  
  return 0;
}

/*
 *--------------------------------------------------------------
 * gdk_x_io_error
 *
 *   The X I/O error handling routine.
 *
 * Arguments:
 *   "display" is the X display the error orignated from.
 *
 * Results:
 *   An X I/O error basically means we lost our connection
 *   to the X server. There is not much we can do to
 *   continue, so simply print an error message and exit.
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

static int
gdk_x_io_error (Display *display)
{
  /* This is basically modelled after the code in XLib. We need
   * an explicit error handler here, so we can disable our atexit()
   * which would otherwise cause a nice segfault.
   * We fprintf(stderr, instead of g_warning() because g_warning()
   * could possibly be redirected to a dialog
   */
  if (errno == EPIPE)
    {
      g_fprintf (stderr,
               "The application '%s' lost its connection to the display %s;\n"
               "most likely the X server was shut down or you killed/destroyed\n"
               "the application.\n",
               g_get_prgname (),
               display ? DisplayString (display) : gdk_get_display_arg_name ());
    }
  else
    {
      g_fprintf (stderr, "%s: Fatal IO error %d (%s) on X server %s.\n",
               g_get_prgname (),
	       errno, g_strerror (errno),
	       display ? DisplayString (display) : gdk_get_display_arg_name ());
    }

  exit(1);
}

/*************************************************************
 * gdk_error_trap_push:
 *     Push an error trap. X errors will be trapped until
 *     the corresponding gdk_error_pop(), which will return
 *     the error code, if any.
 *   arguments:
 *     
 *   results:
 *************************************************************/

void
gdk_error_trap_push (void)
{
  GSList *node;
  GdkErrorTrap *trap;

  if (gdk_error_trap_free_list)
    {
      node = gdk_error_trap_free_list;
      gdk_error_trap_free_list = gdk_error_trap_free_list->next;
    }
  else
    {
      node = g_slist_alloc ();
      node->data = g_new (GdkErrorTrap, 1);
    }

  node->next = gdk_error_traps;
  gdk_error_traps = node;
  
  trap = node->data;
  trap->old_handler = XSetErrorHandler (gdk_x_error);
  trap->error_code = _gdk_error_code;
  trap->error_warnings = _gdk_error_warnings;

  _gdk_error_code = 0;
  _gdk_error_warnings = 0;
}

/*************************************************************
 * gdk_error_trap_pop:
 *     Pop an error trap added with gdk_error_push()
 *   arguments:
 *     
 *   results:
 *     0, if no error occured, otherwise the error code.
 *************************************************************/

gint
gdk_error_trap_pop (void)
{
  GSList *node;
  GdkErrorTrap *trap;
  gint result;

  g_return_val_if_fail (gdk_error_traps != NULL, 0);

  node = gdk_error_traps;
  gdk_error_traps = gdk_error_traps->next;

  node->next = gdk_error_trap_free_list;
  gdk_error_trap_free_list = node;
  
  result = _gdk_error_code;
  
  trap = node->data;
  _gdk_error_code = trap->error_code;
  _gdk_error_warnings = trap->error_warnings;
  XSetErrorHandler (trap->old_handler);
  
  return result;
}

gchar *
gdk_get_display (void)
{
  return g_strdup (gdk_display_get_name (gdk_display_get_default ()));
}

/**
 * _gdk_send_xevent:
 * @display: #GdkDisplay which @window is on
 * @window: window ID to which to send the event
 * @propagate: %TRUE if the event should be propagated if the target window
 *             doesn't handle it.
 * @event_mask: event mask to match against, or 0 to send it to @window
 *              without regard to event masks.
 * @event_send: #XEvent to send
 * 
 * Send an event, like XSendEvent(), but trap errors and check
 * the result.
 * 
 * Return value: %TRUE if sending the event succeeded.
 **/
gint 
_gdk_send_xevent (GdkDisplay *display,
		  Window      window, 
		  gboolean    propagate, 
		  glong       event_mask,
		  XEvent     *event_send)
{
  gboolean result;

  if (display->closed)
    return FALSE;

  gdk_error_trap_push ();
  result = XSendEvent (GDK_DISPLAY_XDISPLAY (display), window, 
		       propagate, event_mask, event_send);
  XSync (GDK_DISPLAY_XDISPLAY (display), False);
  
  if (gdk_error_trap_pop ())
    return FALSE;
 
  return result;
}

void
_gdk_region_get_xrectangles (const GdkRegion *region,
                             gint             x_offset,
                             gint             y_offset,
                             XRectangle     **rects,
                             gint            *n_rects)
{
  XRectangle *rectangles = g_new (XRectangle, region->numRects);
  GdkRegionBox *boxes = region->rects;
  gint i;
  
  for (i = 0; i < region->numRects; i++)
    {
      rectangles[i].x = CLAMP (boxes[i].x1 + x_offset, G_MINSHORT, G_MAXSHORT);
      rectangles[i].y = CLAMP (boxes[i].y1 + y_offset, G_MINSHORT, G_MAXSHORT);
      rectangles[i].width = CLAMP (boxes[i].x2 + x_offset, G_MINSHORT, G_MAXSHORT) - rectangles[i].x;
      rectangles[i].height = CLAMP (boxes[i].y2 + y_offset, G_MINSHORT, G_MAXSHORT) - rectangles[i].y;
    }

  *rects = rectangles;
  *n_rects = region->numRects;
}

/**
 * gdk_x11_grab_server:
 * 
 * Call gdk_x11_display_grab() on the default display. 
 * To ungrab the server again, use gdk_x11_ungrab_server(). 
 *
 * gdk_x11_grab_server()/gdk_x11_ungrab_server() calls can be nested.
 **/ 
void
gdk_x11_grab_server (void)
{
  gdk_x11_display_grab (gdk_display_get_default ());
}

/**
 * gdk_x11_ungrab_server:
 *
 * Ungrab the default display after it has been grabbed with 
 * gdk_x11_grab_server(). 
 **/
void
gdk_x11_ungrab_server (void)
{
  gdk_x11_display_ungrab (gdk_display_get_default ());
}

/**
 * gdk_x11_get_default_screen:
 * 
 * Gets the default GTK+ screen number.
 * 
 * Return value: returns the screen number specified by
 *   the --display command line option or the DISPLAY environment
 *   variable when gdk_init() calls XOpenDisplay().
 **/
gint
gdk_x11_get_default_screen (void)
{
  return gdk_screen_get_number (gdk_screen_get_default ());
}

/**
 * gdk_x11_get_default_root_xwindow:
 * 
 * Gets the root window of the default screen 
 * (see gdk_x11_get_default_screen()).  
 * 
 * Return value: an Xlib <type>Window</type>.
 **/
Window
gdk_x11_get_default_root_xwindow (void)
{
  return GDK_SCREEN_XROOTWIN (gdk_screen_get_default ());
}

/**
 * gdk_x11_get_default_xdisplay:
 * 
 * Gets the default GTK+ display.
 * 
 * Return value: the Xlib <type>Display*</type> for the display
 * specified in the <option>--display</option> command line option 
 * or the <envar>DISPLAY</envar> environment variable.
 **/
Display *
gdk_x11_get_default_xdisplay (void)
{
  return GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
}

#define __GDK_MAIN_X11_C__
#include "gdkaliasdef.c"
