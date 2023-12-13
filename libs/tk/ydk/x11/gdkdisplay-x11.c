/* GDK - The GIMP Drawing Kit
 * gdkdisplay-x11.c
 * 
 * Copyright 2001 Sun Microsystems Inc.
 * Copyright (C) 2004 Nokia Corporation
 *
 * Erwann Chenede <erwann.chenede@sun.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <glib.h>
#include "gdkx.h"
#include "gdkasync.h"
#include "gdkdisplay.h"
#include "gdkdisplay-x11.h"
#include "gdkscreen.h"
#include "gdkscreen-x11.h"
#include "gdkinternals.h"
#include "gdkinputprivate.h"
#include "xsettings-client.h"
#include "gdkalias.h"

#include <X11/Xatom.h>

#ifdef HAVE_XKB
#include <X11/XKBlib.h>
#endif

#ifdef HAVE_XFIXES
#include <X11/extensions/Xfixes.h>
#endif

#include <X11/extensions/shape.h>

#ifdef HAVE_XCOMPOSITE
#include <X11/extensions/Xcomposite.h>
#endif

#ifdef HAVE_XDAMAGE
#include <X11/extensions/Xdamage.h>
#endif

#ifdef HAVE_RANDR
#include <X11/extensions/Xrandr.h>
#endif


static void   gdk_display_x11_dispose            (GObject            *object);
static void   gdk_display_x11_finalize           (GObject            *object);

#ifdef HAVE_X11R6
static void gdk_internal_connection_watch (Display  *display,
					   XPointer  arg,
					   gint      fd,
					   gboolean  opening,
					   XPointer *watch_data);
#endif /* HAVE_X11R6 */

/* Note that we never *directly* use WM_LOCALE_NAME, WM_PROTOCOLS,
 * but including them here has the side-effect of getting them
 * into the internal Xlib cache
 */
static const char *const precache_atoms[] = {
  "UTF8_STRING",
  "WM_CLIENT_LEADER",
  "WM_DELETE_WINDOW",
  "WM_ICON_NAME",
  "WM_LOCALE_NAME",
  "WM_NAME",
  "WM_PROTOCOLS",
  "WM_TAKE_FOCUS",
  "WM_WINDOW_ROLE",
  "_NET_ACTIVE_WINDOW",
  "_NET_CURRENT_DESKTOP",
  "_NET_FRAME_EXTENTS",
  "_NET_STARTUP_ID",
  "_NET_WM_CM_S0",
  "_NET_WM_DESKTOP",
  "_NET_WM_ICON",
  "_NET_WM_ICON_NAME",
  "_NET_WM_NAME",
  "_NET_WM_PID",
  "_NET_WM_PING",
  "_NET_WM_STATE",
  "_NET_WM_STATE_ABOVE",
  "_NET_WM_STATE_BELOW",
  "_NET_WM_STATE_FULLSCREEN",
  "_NET_WM_STATE_MODAL",
  "_NET_WM_STATE_MAXIMIZED_VERT",
  "_NET_WM_STATE_MAXIMIZED_HORZ",
  "_NET_WM_STATE_SKIP_TASKBAR",
  "_NET_WM_STATE_SKIP_PAGER",
  "_NET_WM_STATE_STICKY",
  "_NET_WM_SYNC_REQUEST",
  "_NET_WM_SYNC_REQUEST_COUNTER",
  "_NET_WM_WINDOW_TYPE",
  "_NET_WM_WINDOW_TYPE_NORMAL",
  "_NET_WM_USER_TIME",
  "_NET_VIRTUAL_ROOTS"
};

G_DEFINE_TYPE (GdkDisplayX11, _gdk_display_x11, GDK_TYPE_DISPLAY)

static void
_gdk_display_x11_class_init (GdkDisplayX11Class * class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  
  object_class->dispose = gdk_display_x11_dispose;
  object_class->finalize = gdk_display_x11_finalize;
}

static void
_gdk_display_x11_init (GdkDisplayX11 *display)
{
}

/**
 * gdk_display_open:
 * @display_name: the name of the display to open
 * @returns: a #GdkDisplay, or %NULL if the display
 *  could not be opened.
 *
 * Opens a display.
 *
 * Since: 2.2
 */
GdkDisplay *
gdk_display_open (const gchar *display_name)
{
  Display *xdisplay;
  GdkDisplay *display;
  GdkDisplayX11 *display_x11;
  GdkWindowAttr attr;
  gint argc;
  gchar *argv[1];
  const char *sm_client_id;
  
  XClassHint *class_hint;
  gulong pid;
  gint i;
  gint ignore;
  gint maj, min;

  xdisplay = XOpenDisplay (display_name);
  if (!xdisplay)
    return NULL;
  
  display = g_object_new (GDK_TYPE_DISPLAY_X11, NULL);
  display_x11 = GDK_DISPLAY_X11 (display);

  display_x11->use_xshm = TRUE;
  display_x11->xdisplay = xdisplay;

#ifdef HAVE_X11R6  
  /* Set up handlers for Xlib internal connections */
  XAddConnectionWatch (xdisplay, gdk_internal_connection_watch, NULL);
#endif /* HAVE_X11R6 */
  
  _gdk_x11_precache_atoms (display, precache_atoms, G_N_ELEMENTS (precache_atoms));

  /* RandR must be initialized before we initialize the screens */
  display_x11->have_randr13 = FALSE;
  display_x11->have_randr15 = FALSE;
#ifdef HAVE_RANDR
  if (XRRQueryExtension (display_x11->xdisplay,
			 &display_x11->xrandr_event_base, &ignore))
  {
      int major, minor;
      
      XRRQueryVersion (display_x11->xdisplay, &major, &minor);

      if ((major == 1 && minor >= 3) || major > 1)
	  display_x11->have_randr13 = TRUE;

#ifdef HAVE_RANDR15
      if (minor >= 5 || major > 1)
	display_x11->have_randr15 = TRUE;
#endif

       gdk_x11_register_standard_event_type (display, display_x11->xrandr_event_base, RRNumberEvents);
  }
#endif
  
  /* initialize the display's screens */ 
  display_x11->screens = g_new (GdkScreen *, ScreenCount (display_x11->xdisplay));
  for (i = 0; i < ScreenCount (display_x11->xdisplay); i++)
    display_x11->screens[i] = _gdk_x11_screen_new (display, i);

  /* We need to initialize events after we have the screen
   * structures in places
   */
  for (i = 0; i < ScreenCount (display_x11->xdisplay); i++)
    _gdk_x11_events_init_screen (display_x11->screens[i]);
  
  /*set the default screen */
  display_x11->default_screen = display_x11->screens[DefaultScreen (display_x11->xdisplay)];

  attr.window_type = GDK_WINDOW_TOPLEVEL;
  attr.wclass = GDK_INPUT_OUTPUT;
  attr.x = 10;
  attr.y = 10;
  attr.width = 10;
  attr.height = 10;
  attr.event_mask = 0;

  display_x11->leader_gdk_window = gdk_window_new (GDK_SCREEN_X11 (display_x11->default_screen)->root_window, 
						   &attr, GDK_WA_X | GDK_WA_Y);
  (_gdk_x11_window_get_toplevel (display_x11->leader_gdk_window))->is_leader = TRUE;

  display_x11->leader_window = GDK_WINDOW_XID (display_x11->leader_gdk_window);

  display_x11->leader_window_title_set = FALSE;

  display_x11->have_render = GDK_UNKNOWN;

#ifdef HAVE_XFIXES
  if (XFixesQueryExtension (display_x11->xdisplay, 
			    &display_x11->xfixes_event_base, 
			    &ignore))
    {
      display_x11->have_xfixes = TRUE;

      gdk_x11_register_standard_event_type (display,
					    display_x11->xfixes_event_base, 
					    XFixesNumberEvents);
    }
  else
#endif
    display_x11->have_xfixes = FALSE;

#ifdef HAVE_XCOMPOSITE
  if (XCompositeQueryExtension (display_x11->xdisplay,
				&ignore, &ignore))
    {
      int major, minor;

      XCompositeQueryVersion (display_x11->xdisplay, &major, &minor);

      /* Prior to Composite version 0.4, composited windows clipped their
       * parents, so you had to use IncludeInferiors to draw to the parent
       * This isn't useful for our purposes, so require 0.4
       */
      display_x11->have_xcomposite = major > 0 || (major == 0 && minor >= 4);
    }
  else
#endif
    display_x11->have_xcomposite = FALSE;

#ifdef HAVE_XDAMAGE
  if (XDamageQueryExtension (display_x11->xdisplay,
			     &display_x11->xdamage_event_base,
			     &ignore))
    {
      display_x11->have_xdamage = TRUE;

      gdk_x11_register_standard_event_type (display,
					    display_x11->xdamage_event_base,
					    XDamageNumberEvents);
    }
  else
#endif
    display_x11->have_xdamage = FALSE;

  display_x11->have_shapes = FALSE;
  display_x11->have_input_shapes = FALSE;

  if (XShapeQueryExtension (GDK_DISPLAY_XDISPLAY (display), &display_x11->shape_event_base, &ignore))
    {
      display_x11->have_shapes = TRUE;
#ifdef ShapeInput
      if (XShapeQueryVersion (GDK_DISPLAY_XDISPLAY (display), &maj, &min))
	display_x11->have_input_shapes = (maj == 1 && min >= 1);
#endif
    }

  display_x11->trusted_client = TRUE;
  {
    Window root, child;
    int rootx, rooty, winx, winy;
    unsigned int xmask;

    gdk_error_trap_push ();
    XQueryPointer (display_x11->xdisplay, 
		   GDK_SCREEN_X11 (display_x11->default_screen)->xroot_window,
		   &root, &child, &rootx, &rooty, &winx, &winy, &xmask);
    gdk_flush ();
    if (G_UNLIKELY (gdk_error_trap_pop () == BadWindow)) 
      {
	g_warning ("Connection to display %s appears to be untrusted. Pointer and keyboard grabs and inter-client communication may not work as expected.", gdk_display_get_name (display));
	display_x11->trusted_client = FALSE;
      }
  }

  if (_gdk_synchronize)
    XSynchronize (display_x11->xdisplay, True);
  
  class_hint = XAllocClassHint();
  class_hint->res_name = g_get_prgname ();
  
  class_hint->res_class = (char *)gdk_get_program_class ();

  /* XmbSetWMProperties sets the RESOURCE_NAME environment variable
   * from argv[0], so we just synthesize an argument array here.
   */
  argc = 1;
  argv[0] = g_get_prgname ();
  
  XmbSetWMProperties (display_x11->xdisplay,
		      display_x11->leader_window,
		      NULL, NULL, argv, argc, NULL, NULL,
		      class_hint);
  XFree (class_hint);

  sm_client_id = _gdk_get_sm_client_id ();
  if (sm_client_id)
    _gdk_windowing_display_set_sm_client_id (display, sm_client_id);

  pid = getpid ();
  XChangeProperty (display_x11->xdisplay,
		   display_x11->leader_window,
		   gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_PID"),
		   XA_CARDINAL, 32, PropModeReplace, (guchar *) & pid, 1);

  /* We don't yet know a valid time. */
  display_x11->user_time = 0;
  
#ifdef HAVE_XKB
  {
    gint xkb_major = XkbMajorVersion;
    gint xkb_minor = XkbMinorVersion;
    if (XkbLibraryVersion (&xkb_major, &xkb_minor))
      {
        xkb_major = XkbMajorVersion;
        xkb_minor = XkbMinorVersion;
	    
        if (XkbQueryExtension (display_x11->xdisplay, 
			       NULL, &display_x11->xkb_event_type, NULL,
                               &xkb_major, &xkb_minor))
          {
	    Bool detectable_autorepeat_supported;
	    
	    display_x11->use_xkb = TRUE;

            XkbSelectEvents (display_x11->xdisplay,
                             XkbUseCoreKbd,
                             XkbNewKeyboardNotifyMask | XkbMapNotifyMask | XkbStateNotifyMask,
                             XkbNewKeyboardNotifyMask | XkbMapNotifyMask | XkbStateNotifyMask);

	    /* keep this in sync with _gdk_keymap_state_changed() */ 
	    XkbSelectEventDetails (display_x11->xdisplay,
				   XkbUseCoreKbd, XkbStateNotify,
				   XkbAllStateComponentsMask,
                                   XkbGroupLockMask|XkbModifierLockMask);

	    XkbSetDetectableAutoRepeat (display_x11->xdisplay,
					True,
					&detectable_autorepeat_supported);

	    GDK_NOTE (MISC, g_message ("Detectable autorepeat %s.",
				       detectable_autorepeat_supported ? 
				       "supported" : "not supported"));
	    
	    display_x11->have_xkb_autorepeat = detectable_autorepeat_supported;
          }
      }
  }
#endif

  display_x11->use_sync = FALSE;
#ifdef HAVE_XSYNC
  {
    int major, minor;
    int error_base, event_base;
    
    if (XSyncQueryExtension (display_x11->xdisplay,
			     &event_base, &error_base) &&
        XSyncInitialize (display_x11->xdisplay,
                         &major, &minor))
      display_x11->use_sync = TRUE;
  }
#endif
  
  _gdk_windowing_image_init (display);
  _gdk_events_init (display);
  _gdk_input_init (display);
  _gdk_dnd_init (display);

  for (i = 0; i < ScreenCount (display_x11->xdisplay); i++)
    _gdk_x11_screen_setup (display_x11->screens[i]);

  g_signal_emit_by_name (gdk_display_manager_get(),
			 "display_opened", display);

  return display;
}

#ifdef HAVE_X11R6
/*
 * XLib internal connection handling
 */
typedef struct _GdkInternalConnection GdkInternalConnection;

struct _GdkInternalConnection
{
  gint	         fd;
  GSource	*source;
  Display	*display;
};

static gboolean
process_internal_connection (GIOChannel  *gioc,
			     GIOCondition cond,
			     gpointer     data)
{
  GdkInternalConnection *connection = (GdkInternalConnection *)data;

  GDK_THREADS_ENTER ();

  XProcessInternalConnection ((Display*)connection->display, connection->fd);

  GDK_THREADS_LEAVE ();

  return TRUE;
}

gulong
_gdk_windowing_window_get_next_serial (GdkDisplay *display)
{
  return NextRequest (GDK_DISPLAY_XDISPLAY (display));
}


static GdkInternalConnection *
gdk_add_connection_handler (Display *display,
			    guint    fd)
{
  GIOChannel *io_channel;
  GdkInternalConnection *connection;

  connection = g_new (GdkInternalConnection, 1);

  connection->fd = fd;
  connection->display = display;
  
  io_channel = g_io_channel_unix_new (fd);
  
  connection->source = g_io_create_watch (io_channel, G_IO_IN);
  g_source_set_callback (connection->source,
			 (GSourceFunc)process_internal_connection, connection, NULL);
  g_source_attach (connection->source, NULL);
  
  g_io_channel_unref (io_channel);
  
  return connection;
}

static void
gdk_remove_connection_handler (GdkInternalConnection *connection)
{
  g_source_destroy (connection->source);
  g_free (connection);
}

static void
gdk_internal_connection_watch (Display  *display,
			       XPointer  arg,
			       gint      fd,
			       gboolean  opening,
			       XPointer *watch_data)
{
  if (opening)
    *watch_data = (XPointer)gdk_add_connection_handler (display, fd);
  else
    gdk_remove_connection_handler ((GdkInternalConnection *)*watch_data);
}
#endif /* HAVE_X11R6 */

/**
 * gdk_display_get_name:
 * @display: a #GdkDisplay
 *
 * Gets the name of the display.
 * 
 * Returns: a string representing the display name. This string is owned
 * by GDK and should not be modified or freed.
 * 
 * Since: 2.2
 */
const gchar *
gdk_display_get_name (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  
  return (gchar *) DisplayString (GDK_DISPLAY_X11 (display)->xdisplay);
}

/**
 * gdk_display_get_n_screens:
 * @display: a #GdkDisplay
 *
 * Gets the number of screen managed by the @display.
 * 
 * Returns: number of screens.
 * 
 * Since: 2.2
 */
gint
gdk_display_get_n_screens (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), 0);
  
  return ScreenCount (GDK_DISPLAY_X11 (display)->xdisplay);
}

/**
 * gdk_display_get_screen:
 * @display: a #GdkDisplay
 * @screen_num: the screen number
 *
 * Returns a screen object for one of the screens of the display.
 *
 * Returns: the #GdkScreen object
 *
 * Since: 2.2
 */
GdkScreen *
gdk_display_get_screen (GdkDisplay *display, 
			gint        screen_num)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  g_return_val_if_fail (ScreenCount (GDK_DISPLAY_X11 (display)->xdisplay) > screen_num, NULL);
  
  return GDK_DISPLAY_X11 (display)->screens[screen_num];
}

/**
 * gdk_display_get_default_screen:
 * @display: a #GdkDisplay
 *
 * Get the default #GdkScreen for @display.
 * 
 * Returns: the default #GdkScreen object for @display
 *
 * Since: 2.2
 */
GdkScreen *
gdk_display_get_default_screen (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  
  return GDK_DISPLAY_X11 (display)->default_screen;
}

gboolean
_gdk_x11_display_is_root_window (GdkDisplay *display,
				 Window      xroot_window)
{
  GdkDisplayX11 *display_x11;
  gint i;
  
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);
  
  display_x11 = GDK_DISPLAY_X11 (display);
  
  for (i = 0; i < ScreenCount (display_x11->xdisplay); i++)
    {
      if (GDK_SCREEN_XROOTWIN (display_x11->screens[i]) == xroot_window)
	return TRUE;
    }
  return FALSE;
}

struct XPointerUngrabInfo {
  GdkDisplay *display;
  guint32 time;
};

static void
pointer_ungrab_callback (GdkDisplay *display,
			 gpointer data,
			 gulong serial)
{
  _gdk_display_pointer_grab_update (display, serial);
}


#define XSERVER_TIME_IS_LATER(time1, time2)                        \
  ( (( time1 > time2 ) && ( time1 - time2 < ((guint32)-1)/2 )) ||  \
    (( time1 < time2 ) && ( time2 - time1 > ((guint32)-1)/2 ))     \
  )

/**
 * gdk_display_pointer_ungrab:
 * @display: a #GdkDisplay.
 * @time_: a timestap (e.g. %GDK_CURRENT_TIME).
 *
 * Release any pointer grab.
 *
 * Since: 2.2
 */
void
gdk_display_pointer_ungrab (GdkDisplay *display,
			    guint32     time_)
{
  Display *xdisplay;
  GdkDisplayX11 *display_x11;
  GdkPointerGrabInfo *grab;
  unsigned long serial;

  g_return_if_fail (GDK_IS_DISPLAY (display));

  display_x11 = GDK_DISPLAY_X11 (display);
  xdisplay = GDK_DISPLAY_XDISPLAY (display);

  serial = NextRequest (xdisplay);
  
  _gdk_input_ungrab_pointer (display, time_);
  XUngrabPointer (xdisplay, time_);
  XFlush (xdisplay);

  grab = _gdk_display_get_last_pointer_grab (display);
  if (grab &&
      (time_ == GDK_CURRENT_TIME ||
       grab->time == GDK_CURRENT_TIME ||
       !XSERVER_TIME_IS_LATER (grab->time, time_)))
    {
      grab->serial_end = serial;
      _gdk_x11_roundtrip_async (display, 
				pointer_ungrab_callback,
				NULL);
    }
}

/**
 * gdk_display_keyboard_ungrab:
 * @display: a #GdkDisplay.
 * @time_: a timestap (e.g #GDK_CURRENT_TIME).
 *
 * Release any keyboard grab
 *
 * Since: 2.2
 */
void
gdk_display_keyboard_ungrab (GdkDisplay *display,
			     guint32     time)
{
  Display *xdisplay;
  GdkDisplayX11 *display_x11;
  
  g_return_if_fail (GDK_IS_DISPLAY (display));

  display_x11 = GDK_DISPLAY_X11 (display);
  xdisplay = GDK_DISPLAY_XDISPLAY (display);
  
  XUngrabKeyboard (xdisplay, time);
  XFlush (xdisplay);
  
  if (time == GDK_CURRENT_TIME || 
      display->keyboard_grab.time == GDK_CURRENT_TIME ||
      !XSERVER_TIME_IS_LATER (display->keyboard_grab.time, time))
    _gdk_display_unset_has_keyboard_grab (display, FALSE);
}

/**
 * gdk_display_beep:
 * @display: a #GdkDisplay
 *
 * Emits a short beep on @display
 *
 * Since: 2.2
 */
void
gdk_display_beep (GdkDisplay *display)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));

#ifdef HAVE_XKB
  XkbBell (GDK_DISPLAY_XDISPLAY (display), None, 0, None);
#else
  XBell (GDK_DISPLAY_XDISPLAY (display), 0);
#endif
}

/**
 * gdk_display_sync:
 * @display: a #GdkDisplay
 *
 * Flushes any requests queued for the windowing system and waits until all
 * requests have been handled. This is often used for making sure that the
 * display is synchronized with the current state of the program. Calling
 * gdk_display_sync() before gdk_error_trap_pop() makes sure that any errors
 * generated from earlier requests are handled before the error trap is 
 * removed.
 *
 * This is most useful for X11. On windowing systems where requests are
 * handled synchronously, this function will do nothing.
 *
 * Since: 2.2
 */
void
gdk_display_sync (GdkDisplay *display)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));
  
  XSync (GDK_DISPLAY_XDISPLAY (display), False);
}

/**
 * gdk_display_flush:
 * @display: a #GdkDisplay
 *
 * Flushes any requests queued for the windowing system; this happens automatically
 * when the main loop blocks waiting for new events, but if your application
 * is drawing without returning control to the main loop, you may need
 * to call this function explicitely. A common case where this function
 * needs to be called is when an application is executing drawing commands
 * from a thread other than the thread where the main loop is running.
 *
 * This is most useful for X11. On windowing systems where requests are
 * handled synchronously, this function will do nothing.
 *
 * Since: 2.4
 */
void 
gdk_display_flush (GdkDisplay *display)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));

  if (!display->closed)
    XFlush (GDK_DISPLAY_XDISPLAY (display));
}

/**
 * gdk_display_get_default_group:
 * @display: a #GdkDisplay
 * 
 * Returns the default group leader window for all toplevel windows
 * on @display. This window is implicitly created by GDK. 
 * See gdk_window_set_group().
 * 
 * Return value: The default group leader window for @display
 *
 * Since: 2.4
 **/
GdkWindow *
gdk_display_get_default_group (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return GDK_DISPLAY_X11 (display)->leader_gdk_window;
}

/**
 * gdk_x11_display_grab:
 * @display: a #GdkDisplay 
 * 
 * Call XGrabServer() on @display. 
 * To ungrab the display again, use gdk_x11_display_ungrab(). 
 *
 * gdk_x11_display_grab()/gdk_x11_display_ungrab() calls can be nested.
 *
 * Since: 2.2
 **/
void
gdk_x11_display_grab (GdkDisplay *display)
{
  GdkDisplayX11 *display_x11;
  
  g_return_if_fail (GDK_IS_DISPLAY (display));
  
  display_x11 = GDK_DISPLAY_X11 (display);
  
  if (display_x11->grab_count == 0)
    XGrabServer (display_x11->xdisplay);
  display_x11->grab_count++;
}

/**
 * gdk_x11_display_ungrab:
 * @display: a #GdkDisplay
 * 
 * Ungrab @display after it has been grabbed with 
 * gdk_x11_display_grab(). 
 *
 * Since: 2.2
 **/
void
gdk_x11_display_ungrab (GdkDisplay *display)
{
  GdkDisplayX11 *display_x11;
  
  g_return_if_fail (GDK_IS_DISPLAY (display));
  
  display_x11 = GDK_DISPLAY_X11 (display);;
  g_return_if_fail (display_x11->grab_count > 0);
  
  display_x11->grab_count--;
  if (display_x11->grab_count == 0)
    {
      XUngrabServer (display_x11->xdisplay);
      XFlush (display_x11->xdisplay);
    }
}

static void
gdk_display_x11_dispose (GObject *object)
{
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (object);
  gint           i;

  g_list_foreach (display_x11->input_devices, (GFunc) g_object_run_dispose, NULL);

  for (i = 0; i < ScreenCount (display_x11->xdisplay); i++)
    _gdk_screen_close (display_x11->screens[i]);

  _gdk_events_uninit (GDK_DISPLAY_OBJECT (object));

  G_OBJECT_CLASS (_gdk_display_x11_parent_class)->dispose (object);
}

static void
gdk_display_x11_finalize (GObject *object)
{
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (object);
  gint           i;

  /* Keymap */
  if (display_x11->keymap)
    g_object_unref (display_x11->keymap);

  /* Free motif Dnd */
  if (display_x11->motif_target_lists)
    {
      for (i = 0; i < display_x11->motif_n_target_lists; i++)
        g_list_free (display_x11->motif_target_lists[i]);
      g_free (display_x11->motif_target_lists);
    }

  _gdk_x11_cursor_display_finalize (GDK_DISPLAY_OBJECT(display_x11));

  /* Atom Hashtable */
  g_hash_table_destroy (display_x11->atom_from_virtual);
  g_hash_table_destroy (display_x11->atom_to_virtual);

  /* Leader Window */
  XDestroyWindow (display_x11->xdisplay, display_x11->leader_window);

  /* list of filters for client messages */
  g_list_foreach (display_x11->client_filters, (GFunc) g_free, NULL);
  g_list_free (display_x11->client_filters);

  /* List of event window extraction functions */
  g_slist_foreach (display_x11->event_types, (GFunc)g_free, NULL);
  g_slist_free (display_x11->event_types);

  /* input GdkDevice list */
  g_list_foreach (display_x11->input_devices, (GFunc) g_object_unref, NULL);
  g_list_free (display_x11->input_devices);

  /* input GdkWindow list */
  g_list_foreach (display_x11->input_windows, (GFunc) g_free, NULL);
  g_list_free (display_x11->input_windows);

  /* Free all GdkScreens */
  for (i = 0; i < ScreenCount (display_x11->xdisplay); i++)
    g_object_unref (display_x11->screens[i]);
  g_free (display_x11->screens);

  g_free (display_x11->startup_notification_id);

  /* X ID hashtable */
  g_hash_table_destroy (display_x11->xid_ht);

  XCloseDisplay (display_x11->xdisplay);

  G_OBJECT_CLASS (_gdk_display_x11_parent_class)->finalize (object);
}

/**
 * gdk_x11_lookup_xdisplay:
 * @xdisplay: a pointer to an X Display
 * 
 * Find the #GdkDisplay corresponding to @display, if any exists.
 * 
 * Return value: the #GdkDisplay, if found, otherwise %NULL.
 *
 * Since: 2.2
 **/
GdkDisplay *
gdk_x11_lookup_xdisplay (Display *xdisplay)
{
  GSList *tmp_list;

  for (tmp_list = _gdk_displays; tmp_list; tmp_list = tmp_list->next)
    {
      if (GDK_DISPLAY_XDISPLAY (tmp_list->data) == xdisplay)
	return tmp_list->data;
    }
  
  return NULL;
}

/**
 * _gdk_x11_display_screen_for_xrootwin:
 * @display: a #GdkDisplay
 * @xrootwin: window ID for one of of the screen's of the display.
 * 
 * Given the root window ID of one of the screen's of a #GdkDisplay,
 * finds the screen.
 * 
 * Return value: (transfer none): the #GdkScreen corresponding to
 *     @xrootwin, or %NULL.
 **/
GdkScreen *
_gdk_x11_display_screen_for_xrootwin (GdkDisplay *display,
				      Window      xrootwin)
{
  gint i;

  for (i = 0; i < ScreenCount (GDK_DISPLAY_X11 (display)->xdisplay); i++)
    {
      GdkScreen *screen = gdk_display_get_screen (display, i);
      if (GDK_SCREEN_XROOTWIN (screen) == xrootwin)
	return screen;
    }

  return NULL;
}

/**
 * gdk_x11_display_get_xdisplay:
 * @display: a #GdkDisplay
 * @returns: an X display.
 *
 * Returns the X display of a #GdkDisplay.
 *
 * Since: 2.2
 */
Display *
gdk_x11_display_get_xdisplay (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  return GDK_DISPLAY_X11 (display)->xdisplay;
}

void
_gdk_windowing_set_default_display (GdkDisplay *display)
{
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (display);
  const gchar *startup_id;
  
  if (!display)
    {
      gdk_display = NULL;
      return;
    }

  gdk_display = GDK_DISPLAY_XDISPLAY (display);

  g_free (display_x11->startup_notification_id);
  display_x11->startup_notification_id = NULL;
  
  startup_id = g_getenv ("DESKTOP_STARTUP_ID");
  if (startup_id && *startup_id != '\0')
    {
      gchar *time_str;

      if (!g_utf8_validate (startup_id, -1, NULL))
	g_warning ("DESKTOP_STARTUP_ID contains invalid UTF-8");
      else
	display_x11->startup_notification_id = g_strdup (startup_id);

      /* Find the launch time from the startup_id, if it's there.  Newer spec
       * states that the startup_id is of the form <unique>_TIME<timestamp>
       */
      time_str = g_strrstr (startup_id, "_TIME");
      if (time_str != NULL)
        {
	  gulong retval;
          gchar *end;
          errno = 0;

          /* Skip past the "_TIME" part */
          time_str += 5;

          retval = strtoul (time_str, &end, 0);
          if (end != time_str && errno == 0)
            display_x11->user_time = retval;
        }
      
      /* Clear the environment variable so it won't be inherited by
       * child processes and confuse things.  
       */
      g_unsetenv ("DESKTOP_STARTUP_ID");

      /* Set the startup id on the leader window so it
       * applies to all windows we create on this display
       */
      XChangeProperty (display_x11->xdisplay,
		       display_x11->leader_window,
		       gdk_x11_get_xatom_by_name_for_display (display, "_NET_STARTUP_ID"),
		       gdk_x11_get_xatom_by_name_for_display (display, "UTF8_STRING"), 8,
		       PropModeReplace,
		       (guchar *)startup_id, strlen (startup_id));
    }
}

static void
broadcast_xmessage (GdkDisplay *display,
		    const char *message_type,
		    const char *message_type_begin,
		    const char *message)
{
  Display *xdisplay = GDK_DISPLAY_XDISPLAY (display);
  GdkScreen *screen = gdk_display_get_default_screen (display);
  GdkWindow *root_window = gdk_screen_get_root_window (screen);
  Window xroot_window = GDK_WINDOW_XID (root_window);
  
  Atom type_atom;
  Atom type_atom_begin;
  Window xwindow;

  if (!G_LIKELY (GDK_DISPLAY_X11 (display)->trusted_client))
    return;

  {
    XSetWindowAttributes attrs;

    attrs.override_redirect = True;
    attrs.event_mask = PropertyChangeMask | StructureNotifyMask;

    xwindow =
      XCreateWindow (xdisplay,
                     xroot_window,
                     -100, -100, 1, 1,
                     0,
                     CopyFromParent,
                     CopyFromParent,
                     (Visual *)CopyFromParent,
                     CWOverrideRedirect | CWEventMask,
                     &attrs);
  }

  type_atom = gdk_x11_get_xatom_by_name_for_display (display,
                                                     message_type);
  type_atom_begin = gdk_x11_get_xatom_by_name_for_display (display,
                                                           message_type_begin);
  
  {
    XClientMessageEvent xclient;
    const char *src;
    const char *src_end;
    char *dest;
    char *dest_end;
    
		memset(&xclient, 0, sizeof (xclient));
    xclient.type = ClientMessage;
    xclient.message_type = type_atom_begin;
    xclient.display =xdisplay;
    xclient.window = xwindow;
    xclient.format = 8;

    src = message;
    src_end = message + strlen (message) + 1; /* +1 to include nul byte */
    
    while (src != src_end)
      {
        dest = &xclient.data.b[0];
        dest_end = dest + 20;        
        
        while (dest != dest_end &&
               src != src_end)
          {
            *dest = *src;
            ++dest;
            ++src;
          }

	while (dest != dest_end)
	  {
	    *dest = 0;
	    ++dest;
	  }
        
        XSendEvent (xdisplay,
                    xroot_window,
                    False,
                    PropertyChangeMask,
                    (XEvent *)&xclient);

        xclient.message_type = type_atom;
      }
  }

  XDestroyWindow (xdisplay, xwindow);
  XFlush (xdisplay);
}

/**
 * gdk_x11_display_broadcast_startup_message:
 * @display: a #GdkDisplay
 * @message_type: startup notification message type ("new", "change",
 * or "remove")
 * @...: a list of key/value pairs (as strings), terminated by a
 * %NULL key. (A %NULL value for a key will cause that key to be
 * skipped in the output.)
 *
 * Sends a startup notification message of type @message_type to
 * @display. 
 *
 * This is a convenience function for use by code that implements the
 * freedesktop startup notification specification. Applications should
 * not normally need to call it directly. See the <ulink
 * url="http://standards.freedesktop.org/startup-notification-spec/startup-notification-latest.txt">Startup
 * Notification Protocol specification</ulink> for
 * definitions of the message types and keys that can be used.
 *
 * Since: 2.12
 **/
void
gdk_x11_display_broadcast_startup_message (GdkDisplay *display,
					   const char *message_type,
					   ...)
{
  GString *message;
  va_list ap;
  const char *key, *value, *p;

  message = g_string_new (message_type);
  g_string_append_c (message, ':');

  va_start (ap, message_type);
  while ((key = va_arg (ap, const char *)))
    {
      value = va_arg (ap, const char *);
      if (!value)
	continue;

      g_string_append_printf (message, " %s=\"", key);
      for (p = value; *p; p++)
	{
	  switch (*p)
	    {
	    case ' ':
	    case '"':
	    case '\\':
	      g_string_append_c (message, '\\');
	      break;
	    }

	  g_string_append_c (message, *p);
	}
      g_string_append_c (message, '\"');
    }
  va_end (ap);

  broadcast_xmessage (display,
		      "_NET_STARTUP_INFO",
                      "_NET_STARTUP_INFO_BEGIN",
                      message->str);

  g_string_free (message, TRUE);
}

/**
 * gdk_notify_startup_complete:
 * 
 * Indicates to the GUI environment that the application has finished
 * loading. If the applications opens windows, this function is
 * normally called after opening the application's initial set of
 * windows.
 * 
 * GTK+ will call this function automatically after opening the first
 * #GtkWindow unless gtk_window_set_auto_startup_notification() is called 
 * to disable that feature.
 *
 * Since: 2.2
 **/
void
gdk_notify_startup_complete (void)
{
  GdkDisplay *display;
  GdkDisplayX11 *display_x11;

  display = gdk_display_get_default ();
  if (!display)
    return;
  
  display_x11 = GDK_DISPLAY_X11 (display);

  if (display_x11->startup_notification_id == NULL)
    return;

  gdk_notify_startup_complete_with_id (display_x11->startup_notification_id);
}

/**
 * gdk_notify_startup_complete_with_id:
 * @startup_id: a startup-notification identifier, for which notification
 *              process should be completed
 * 
 * Indicates to the GUI environment that the application has finished
 * loading, using a given identifier.
 * 
 * GTK+ will call this function automatically for #GtkWindow with custom
 * startup-notification identifier unless
 * gtk_window_set_auto_startup_notification() is called to disable
 * that feature.
 *
 * Since: 2.12
 **/
void
gdk_notify_startup_complete_with_id (const gchar* startup_id)
{
  GdkDisplay *display;

  display = gdk_display_get_default ();
  if (!display)
    return;

  gdk_x11_display_broadcast_startup_message (display, "remove",
					     "ID", startup_id,
					     NULL);
}

/**
 * gdk_display_supports_selection_notification:
 * @display: a #GdkDisplay
 * 
 * Returns whether #GdkEventOwnerChange events will be 
 * sent when the owner of a selection changes.
 * 
 * Return value: whether #GdkEventOwnerChange events will 
 *               be sent.
 *
 * Since: 2.6
 **/
gboolean 
gdk_display_supports_selection_notification (GdkDisplay *display)
{
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (display);

  return display_x11->have_xfixes;
}

/**
 * gdk_display_request_selection_notification:
 * @display: a #GdkDisplay
 * @selection: the #GdkAtom naming the selection for which
 *             ownership change notification is requested
 * 
 * Request #GdkEventOwnerChange events for ownership changes
 * of the selection named by the given atom.
 * 
 * Return value: whether #GdkEventOwnerChange events will 
 *               be sent.
 *
 * Since: 2.6
 **/
gboolean
gdk_display_request_selection_notification (GdkDisplay *display,
					    GdkAtom     selection)

{
#ifdef HAVE_XFIXES
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (display);
  Atom atom;

  if (display_x11->have_xfixes)
    {
      atom = gdk_x11_atom_to_xatom_for_display (display, 
						selection);
      XFixesSelectSelectionInput (display_x11->xdisplay, 
				  display_x11->leader_window,
				  atom,
				  XFixesSetSelectionOwnerNotifyMask |
				  XFixesSelectionWindowDestroyNotifyMask |
				  XFixesSelectionClientCloseNotifyMask);
      return TRUE;
    }
  else
#endif
    return FALSE;
}

/**
 * gdk_display_supports_clipboard_persistence
 * @display: a #GdkDisplay
 *
 * Returns whether the speicifed display supports clipboard
 * persistance; i.e. if it's possible to store the clipboard data after an
 * application has quit. On X11 this checks if a clipboard daemon is
 * running.
 *
 * Returns: %TRUE if the display supports clipboard persistance.
 *
 * Since: 2.6
 */
gboolean
gdk_display_supports_clipboard_persistence (GdkDisplay *display)
{
  Atom clipboard_manager;

  /* It might make sense to cache this */
  clipboard_manager = gdk_x11_get_xatom_by_name_for_display (display, "CLIPBOARD_MANAGER");
  return XGetSelectionOwner (GDK_DISPLAY_X11 (display)->xdisplay, clipboard_manager) != None;
}

/**
 * gdk_display_store_clipboard
 * @display:          a #GdkDisplay
 * @clipboard_window: a #GdkWindow belonging to the clipboard owner
 * @time_:            a timestamp
 * @targets:	      an array of targets that should be saved, or %NULL 
 *                    if all available targets should be saved.
 * @n_targets:        length of the @targets array
 *
 * Issues a request to the clipboard manager to store the
 * clipboard data. On X11, this is a special program that works
 * according to the freedesktop clipboard specification, available at
 * <ulink url="http://www.freedesktop.org/Standards/clipboard-manager-spec">
 * http://www.freedesktop.org/Standards/clipboard-manager-spec</ulink>.
 *
 * Since: 2.6
 */
void
gdk_display_store_clipboard (GdkDisplay    *display,
			     GdkWindow     *clipboard_window,
			     guint32        time_,
			     const GdkAtom *targets,
			     gint           n_targets)
{
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (display);
  Atom clipboard_manager, save_targets;

  g_return_if_fail (GDK_WINDOW_IS_X11 (clipboard_window));

  clipboard_manager = gdk_x11_get_xatom_by_name_for_display (display, "CLIPBOARD_MANAGER");
  save_targets = gdk_x11_get_xatom_by_name_for_display (display, "SAVE_TARGETS");

  gdk_error_trap_push ();

  if (XGetSelectionOwner (display_x11->xdisplay, clipboard_manager) != None)
    {
      Atom property_name = None;
      Atom *xatoms;
      int i;
      
      if (n_targets > 0)
	{
	  property_name = gdk_x11_atom_to_xatom_for_display (display, _gdk_selection_property);

	  xatoms = g_new (Atom, n_targets);
	  for (i = 0; i < n_targets; i++)
	    xatoms[i] = gdk_x11_atom_to_xatom_for_display (display, targets[i]);

	  XChangeProperty (display_x11->xdisplay, GDK_WINDOW_XID (clipboard_window),
			   property_name, XA_ATOM,
			   32, PropModeReplace, (guchar *)xatoms, n_targets);
	  g_free (xatoms);

	}
      
      XConvertSelection (display_x11->xdisplay,
			 clipboard_manager, save_targets, property_name,
			 GDK_WINDOW_XID (clipboard_window), time_);
      
    }
  gdk_error_trap_pop ();

}

/**
 * gdk_x11_display_get_user_time:
 * @display: a #GdkDisplay
 *
 * Returns the timestamp of the last user interaction on 
 * @display. The timestamp is taken from events caused
 * by user interaction such as key presses or pointer 
 * movements. See gdk_x11_window_set_user_time().
 *
 * Returns: the timestamp of the last user interaction 
 *
 * Since: 2.8
 */
guint32
gdk_x11_display_get_user_time (GdkDisplay *display)
{
  return GDK_DISPLAY_X11 (display)->user_time;
}

/**
 * gdk_display_supports_shapes:
 * @display: a #GdkDisplay
 *
 * Returns %TRUE if gdk_window_shape_combine_mask() can
 * be used to create shaped windows on @display.
 *
 * Returns: %TRUE if shaped windows are supported 
 *
 * Since: 2.10
 */
gboolean 
gdk_display_supports_shapes (GdkDisplay *display)
{
  return GDK_DISPLAY_X11 (display)->have_shapes;
}

/**
 * gdk_display_supports_input_shapes:
 * @display: a #GdkDisplay
 *
 * Returns %TRUE if gdk_window_input_shape_combine_mask() can
 * be used to modify the input shape of windows on @display.
 *
 * Returns: %TRUE if windows with modified input shape are supported 
 *
 * Since: 2.10
 */
gboolean 
gdk_display_supports_input_shapes (GdkDisplay *display)
{
  return GDK_DISPLAY_X11 (display)->have_input_shapes;
}


/**
 * gdk_x11_display_get_startup_notification_id:
 * @display: a #GdkDisplay
 *
 * Gets the startup notification ID for a display.
 * 
 * Returns: the startup notification ID for @display
 *
 * Since: 2.12
 */
const gchar *
gdk_x11_display_get_startup_notification_id (GdkDisplay *display)
{
  return GDK_DISPLAY_X11 (display)->startup_notification_id;
}

/**
 * gdk_display_supports_composite:
 * @display: a #GdkDisplay
 *
 * Returns %TRUE if gdk_window_set_composited() can be used
 * to redirect drawing on the window using compositing.
 *
 * Currently this only works on X11 with XComposite and
 * XDamage extensions available.
 *
 * Returns: %TRUE if windows may be composited.
 *
 * Since: 2.12
 */
gboolean
gdk_display_supports_composite (GdkDisplay *display)
{
  GdkDisplayX11 *x11_display = GDK_DISPLAY_X11 (display);

  return x11_display->have_xcomposite &&
	 x11_display->have_xdamage &&
	 x11_display->have_xfixes;
}


#define __GDK_DISPLAY_X11_C__
#include "gdkaliasdef.c"
