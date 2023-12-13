/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-2007 Peter Mattis, Spencer Kimball,
 * Josh MacDonald, Ryan Lortie
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
 * Modified by the GTK+ Team and others 1997-2007.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#include "gdk.h"
#include "gdkprivate-x11.h"
#include "gdkinternals.h"
#include "gdkx.h"
#include "gdkscreen-x11.h"
#include "gdkdisplay-x11.h"
#include "gdkasync.h"

#include "gdkkeysyms.h"

#include "xsettings-client.h"

#include <string.h>

#include "gdkinputprivate.h"
#include "gdksettings.c"
#include "gdkalias.h"


#ifdef HAVE_XKB
#include <X11/XKBlib.h>
#endif

#ifdef HAVE_XSYNC
#include <X11/extensions/sync.h>
#endif

#ifdef HAVE_XFIXES
#include <X11/extensions/Xfixes.h>
#endif

#ifdef HAVE_RANDR
#include <X11/extensions/Xrandr.h>
#endif

#include <X11/Xatom.h>

typedef struct _GdkIOClosure GdkIOClosure;
typedef struct _GdkDisplaySource GdkDisplaySource;
typedef struct _GdkEventTypeX11 GdkEventTypeX11;

struct _GdkIOClosure
{
  GdkInputFunction function;
  GdkInputCondition condition;
  GDestroyNotify notify;
  gpointer data;
};

struct _GdkDisplaySource
{
  GSource source;
  
  GdkDisplay *display;
  GPollFD event_poll_fd;
};

struct _GdkEventTypeX11
{
  gint base;
  gint n_events;
};

/* 
 * Private function declarations
 */

static gint	 gdk_event_apply_filters (XEvent   *xevent,
					  GdkEvent *event,
					  GdkWindow *window);
static gboolean	 gdk_event_translate	 (GdkDisplay *display,
					  GdkEvent   *event, 
					  XEvent     *xevent,
					  gboolean    return_exposes);

static gboolean gdk_event_prepare  (GSource     *source,
				    gint        *timeout);
static gboolean gdk_event_check    (GSource     *source);
static gboolean gdk_event_dispatch (GSource     *source,
				    GSourceFunc  callback,
				    gpointer     user_data);

static GdkFilterReturn gdk_wm_protocols_filter (GdkXEvent *xev,
						GdkEvent  *event,
						gpointer   data);

static GSource *gdk_display_source_new (GdkDisplay *display);
static gboolean gdk_check_xpending     (GdkDisplay *display);

static Bool gdk_xsettings_watch_cb  (Window            window,
				     Bool              is_start,
				     long              mask,
				     void             *cb_data);
static void gdk_xsettings_notify_cb (const char       *name,
				     XSettingsAction   action,
				     XSettingsSetting *setting,
				     void             *data);

/* Private variable declarations
 */

static GList *display_sources;

static GSourceFuncs event_funcs = {
  gdk_event_prepare,
  gdk_event_check,
  gdk_event_dispatch,
  NULL
};

static GSource *
gdk_display_source_new (GdkDisplay *display)
{
  GSource *source = g_source_new (&event_funcs, sizeof (GdkDisplaySource));
  GdkDisplaySource *display_source = (GdkDisplaySource *)source;
  char *name;
  
  name = g_strdup_printf ("GDK X11 Event source (%s)",
			  gdk_display_get_name (display));
  g_source_set_name (source, name);
  g_free (name);
  display_source->display = display;
  
  return source;
}

static gboolean
gdk_check_xpending (GdkDisplay *display)
{
  return XPending (GDK_DISPLAY_XDISPLAY (display));
}

/*********************************************
 * Functions for maintaining the event queue *
 *********************************************/

static void
refcounted_grab_server (Display *xdisplay)
{
  GdkDisplay *display = gdk_x11_lookup_xdisplay (xdisplay);

  gdk_x11_display_grab (display);
}

static void
refcounted_ungrab_server (Display *xdisplay)
{
  GdkDisplay *display = gdk_x11_lookup_xdisplay (xdisplay);
  
  gdk_x11_display_ungrab (display);
}

void
_gdk_x11_events_init_screen (GdkScreen *screen)
{
  GdkScreenX11 *screen_x11 = GDK_SCREEN_X11 (screen);

  /* Keep a flag to avoid extra notifies that we don't need
   */
  screen_x11->xsettings_in_init = TRUE;
  screen_x11->xsettings_client = xsettings_client_new_with_grab_funcs (screen_x11->xdisplay,
						                       screen_x11->screen_num,
						                       gdk_xsettings_notify_cb,
						                       gdk_xsettings_watch_cb,
						                       screen,
                                                                       refcounted_grab_server,
                                                                       refcounted_ungrab_server);
  screen_x11->xsettings_in_init = FALSE;
}

void
_gdk_x11_events_uninit_screen (GdkScreen *screen)
{
  GdkScreenX11 *screen_x11 = GDK_SCREEN_X11 (screen);

  if (screen_x11->xsettings_client)
    {
      xsettings_client_destroy (screen_x11->xsettings_client);
      screen_x11->xsettings_client = NULL;
    }
}

void 
_gdk_events_init (GdkDisplay *display)
{
  GSource *source;
  GdkDisplaySource *display_source;
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (display);
  
  int connection_number = ConnectionNumber (display_x11->xdisplay);
  GDK_NOTE (MISC, g_message ("connection number: %d", connection_number));


  source = display_x11->event_source = gdk_display_source_new (display);
  display_source = (GdkDisplaySource*) source;
  g_source_set_priority (source, GDK_PRIORITY_EVENTS);
  
  display_source->event_poll_fd.fd = connection_number;
  display_source->event_poll_fd.events = G_IO_IN;
  
  g_source_add_poll (source, &display_source->event_poll_fd);
  g_source_set_can_recurse (source, TRUE);
  g_source_attach (source, NULL);

  display_sources = g_list_prepend (display_sources,display_source);

  gdk_display_add_client_message_filter (display,
					 gdk_atom_intern_static_string ("WM_PROTOCOLS"), 
					 gdk_wm_protocols_filter,   
					 NULL);
}

void
_gdk_events_uninit (GdkDisplay *display)
{
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (display);

  if (display_x11->event_source)
    {
      display_sources = g_list_remove (display_sources,
                                       display_x11->event_source);
      g_source_destroy (display_x11->event_source);
      g_source_unref (display_x11->event_source);
      display_x11->event_source = NULL;
    }
}

/**
 * gdk_events_pending:
 * 
 * Checks if any events are ready to be processed for any display.
 * 
 * Return value:  %TRUE if any events are pending.
 **/
gboolean
gdk_events_pending (void)
{
  GList *tmp_list;

  for (tmp_list = display_sources; tmp_list; tmp_list = tmp_list->next)
    {
      GdkDisplaySource *tmp_source = tmp_list->data;
      GdkDisplay *display = tmp_source->display;
      
      if (_gdk_event_queue_find_first (display))
	return TRUE;
    }

  for (tmp_list = display_sources; tmp_list; tmp_list = tmp_list->next)
    {
      GdkDisplaySource *tmp_source = tmp_list->data;
      GdkDisplay *display = tmp_source->display;
      
      if (gdk_check_xpending (display))
	return TRUE;
    }
  
  return FALSE;
}

static Bool
graphics_expose_predicate (Display  *display,
			   XEvent   *xevent,
			   XPointer  arg)
{
  if (xevent->xany.window == GDK_DRAWABLE_XID ((GdkDrawable *)arg) &&
      (xevent->xany.type == GraphicsExpose ||
       xevent->xany.type == NoExpose))
    return True;
  else
    return False;
}

/**
 * gdk_event_get_graphics_expose:
 * @window: the #GdkWindow to wait for the events for.
 * 
 * Waits for a GraphicsExpose or NoExpose event from the X server.
 * This is used in the #GtkText and #GtkCList widgets in GTK+ to make sure any
 * GraphicsExpose events are handled before the widget is scrolled.
 *
 * Return value:  a #GdkEventExpose if a GraphicsExpose was received, or %NULL if a
 * NoExpose event was received.
 *
 * Deprecated: 2.18:
 **/
GdkEvent*
gdk_event_get_graphics_expose (GdkWindow *window)
{
  XEvent xevent;
  GdkEvent *event;
  
  g_return_val_if_fail (window != NULL, NULL);

  XIfEvent (GDK_WINDOW_XDISPLAY (window), &xevent, 
	    graphics_expose_predicate, (XPointer) window);
  
  if (xevent.xany.type == GraphicsExpose)
    {
      event = gdk_event_new (GDK_NOTHING);
      
      if (gdk_event_translate (GDK_WINDOW_DISPLAY (window), event,
			       &xevent, TRUE))
	return event;
      else
	gdk_event_free (event);
    }
  
  return NULL;	
}

static gint
gdk_event_apply_filters (XEvent *xevent,
			 GdkEvent *event,
			 GdkWindow *window)
{
  GList *tmp_list;
  GdkFilterReturn result;
  
  if (window == NULL)
    tmp_list = _gdk_default_filters;
  else
    {
      GdkWindowObject *window_private;
      window_private = (GdkWindowObject *) window;
      tmp_list = window_private->filters;
    }

  
  while (tmp_list)
    {
      GdkEventFilter *filter = (GdkEventFilter*) tmp_list->data;
      GList *node;
      
      if ((filter->flags & GDK_EVENT_FILTER_REMOVED) != 0)
        {
          tmp_list = tmp_list->next;
          continue;
        }

      filter->ref_count++;
      result = filter->function (xevent, event, filter->data);

      /* Protect against unreffing the filter mutating the list */
      node = tmp_list->next;

      _gdk_event_filter_unref (window, filter);

      tmp_list = node;

      if (result != GDK_FILTER_CONTINUE)
        return result;
    }
  
  return GDK_FILTER_CONTINUE;
}

/**
 * gdk_display_add_client_message_filter:
 * @display: a #GdkDisplay for which this message filter applies
 * @message_type: the type of ClientMessage events to receive.
 *   This will be checked against the @message_type field 
 *   of the XClientMessage event struct.
 * @func: the function to call to process the event.
 * @data: user data to pass to @func.
 *
 * Adds a filter to be called when X ClientMessage events are received.
 * See gdk_window_add_filter() if you are interested in filtering other
 * types of events.
 *
 * Since: 2.2
 **/ 
void 
gdk_display_add_client_message_filter (GdkDisplay   *display,
				       GdkAtom       message_type,
				       GdkFilterFunc func,
				       gpointer      data)
{
  GdkClientFilter *filter;
  g_return_if_fail (GDK_IS_DISPLAY (display));
  filter = g_new (GdkClientFilter, 1);

  filter->type = message_type;
  filter->function = func;
  filter->data = data;
  
  GDK_DISPLAY_X11(display)->client_filters = 
    g_list_append (GDK_DISPLAY_X11 (display)->client_filters,
		   filter);
}

/**
 * gdk_add_client_message_filter:
 * @message_type: the type of ClientMessage events to receive. This will be
 *     checked against the <structfield>message_type</structfield> field of the
 *     XClientMessage event struct.
 * @func: the function to call to process the event.
 * @data: user data to pass to @func. 
 * 
 * Adds a filter to the default display to be called when X ClientMessage events
 * are received. See gdk_display_add_client_message_filter().
 **/
void 
gdk_add_client_message_filter (GdkAtom       message_type,
			       GdkFilterFunc func,
			       gpointer      data)
{
  gdk_display_add_client_message_filter (gdk_display_get_default (),
					 message_type, func, data);
}

static void
do_net_wm_state_changes (GdkWindow *window)
{
  GdkToplevelX11 *toplevel = _gdk_x11_window_get_toplevel (window);
  GdkWindowState old_state;
  
  if (GDK_WINDOW_DESTROYED (window) ||
      gdk_window_get_window_type (window) != GDK_WINDOW_TOPLEVEL)
    return;
  
  old_state = gdk_window_get_state (window);

  /* For found_sticky to remain TRUE, we have to also be on desktop
   * 0xFFFFFFFF
   */
  if (old_state & GDK_WINDOW_STATE_STICKY)
    {
      if (!(toplevel->have_sticky && toplevel->on_all_desktops))
        gdk_synthesize_window_state (window,
                                     GDK_WINDOW_STATE_STICKY,
                                     0);
    }
  else
    {
      if (toplevel->have_sticky && toplevel->on_all_desktops)
        gdk_synthesize_window_state (window,
                                     0,
                                     GDK_WINDOW_STATE_STICKY);
    }

  if (old_state & GDK_WINDOW_STATE_FULLSCREEN)
    {
      if (!toplevel->have_fullscreen)
        gdk_synthesize_window_state (window,
                                     GDK_WINDOW_STATE_FULLSCREEN,
                                     0);
    }
  else
    {
      if (toplevel->have_fullscreen)
        gdk_synthesize_window_state (window,
                                     0,
                                     GDK_WINDOW_STATE_FULLSCREEN);
    }
  
  /* Our "maximized" means both vertical and horizontal; if only one,
   * we don't expose that via GDK
   */
  if (old_state & GDK_WINDOW_STATE_MAXIMIZED)
    {
      if (!(toplevel->have_maxvert && toplevel->have_maxhorz))
        gdk_synthesize_window_state (window,
                                     GDK_WINDOW_STATE_MAXIMIZED,
                                     0);
    }
  else
    {
      if (toplevel->have_maxvert && toplevel->have_maxhorz)
        gdk_synthesize_window_state (window,
                                     0,
                                     GDK_WINDOW_STATE_MAXIMIZED);
    }

  if (old_state & GDK_WINDOW_STATE_ICONIFIED)
    {
      if (!toplevel->have_hidden)
        gdk_synthesize_window_state (window,
                                     GDK_WINDOW_STATE_ICONIFIED,
                                     0);
    }
  else
    {
      if (toplevel->have_hidden)
        gdk_synthesize_window_state (window,
                                     0,
                                     GDK_WINDOW_STATE_ICONIFIED);
    }
}

static void
gdk_check_wm_desktop_changed (GdkWindow *window)
{
  GdkToplevelX11 *toplevel = _gdk_x11_window_get_toplevel (window);
  GdkDisplay *display = GDK_WINDOW_DISPLAY (window);

  Atom type;
  gint format;
  gulong nitems;
  gulong bytes_after;
  guchar *data;
  gulong *desktop;

  type = None;
  gdk_error_trap_push ();
  XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display), 
                      GDK_WINDOW_XID (window),
                      gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_DESKTOP"),
                      0, G_MAXLONG, False, XA_CARDINAL, &type, 
                      &format, &nitems,
                      &bytes_after, &data);
  gdk_error_trap_pop ();

  if (type != None)
    {
      desktop = (gulong *)data;
      toplevel->on_all_desktops = ((*desktop & 0xFFFFFFFF) == 0xFFFFFFFF);
      XFree (desktop);
    }
  else
    toplevel->on_all_desktops = FALSE;
      
  do_net_wm_state_changes (window);
}

static void
gdk_check_wm_state_changed (GdkWindow *window)
{
  GdkToplevelX11 *toplevel = _gdk_x11_window_get_toplevel (window);
  GdkDisplay *display = GDK_WINDOW_DISPLAY (window);
  
  Atom type;
  gint format;
  gulong nitems;
  gulong bytes_after;
  guchar *data;
  Atom *atoms = NULL;
  gulong i;

  gboolean had_sticky = toplevel->have_sticky;

  toplevel->have_sticky = FALSE;
  toplevel->have_maxvert = FALSE;
  toplevel->have_maxhorz = FALSE;
  toplevel->have_fullscreen = FALSE;
  toplevel->have_hidden = FALSE;

  type = None;
  gdk_error_trap_push ();
  XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display), GDK_WINDOW_XID (window),
		      gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_STATE"),
		      0, G_MAXLONG, False, XA_ATOM, &type, &format, &nitems,
		      &bytes_after, &data);
  gdk_error_trap_pop ();

  if (type != None)
    {
      Atom sticky_atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_STATE_STICKY");
      Atom maxvert_atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_STATE_MAXIMIZED_VERT");
      Atom maxhorz_atom	= gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_STATE_MAXIMIZED_HORZ");
      Atom fullscreen_atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_STATE_FULLSCREEN");
      Atom hidden_atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_STATE_HIDDEN");

      atoms = (Atom *)data;

      i = 0;
      while (i < nitems)
        {
          if (atoms[i] == sticky_atom)
            toplevel->have_sticky = TRUE;
          else if (atoms[i] == maxvert_atom)
            toplevel->have_maxvert = TRUE;
          else if (atoms[i] == maxhorz_atom)
            toplevel->have_maxhorz = TRUE;
          else if (atoms[i] == fullscreen_atom)
            toplevel->have_fullscreen = TRUE;
          else if (atoms[i] == hidden_atom)
            toplevel->have_hidden = TRUE;
          
          ++i;
        }

      XFree (atoms);
    }

  /* When have_sticky is turned on, we have to check the DESKTOP property
   * as well.
   */
  if (toplevel->have_sticky && !had_sticky)
    gdk_check_wm_desktop_changed (window);
  else
    do_net_wm_state_changes (window);
}

#define HAS_FOCUS(toplevel)                           \
  ((toplevel)->has_focus || (toplevel)->has_pointer_focus)

static void
generate_focus_event (GdkWindow *window,
		      gboolean   in)
{
  GdkEvent event;
  
  event.type = GDK_FOCUS_CHANGE;
  event.focus_change.window = window;
  event.focus_change.send_event = FALSE;
  event.focus_change.in = in;
  
  gdk_event_put (&event);
}

static gboolean
set_screen_from_root (GdkDisplay *display,
		      GdkEvent   *event,
		      Window      xrootwin)
{
  GdkScreen *screen;

  screen = _gdk_x11_display_screen_for_xrootwin (display, xrootwin);

  if (screen)
    {
      gdk_event_set_screen (event, screen);

      return TRUE;
    }
  
  return FALSE;
}

static void
translate_key_event (GdkDisplay *display,
		     GdkEvent   *event,
		     XEvent     *xevent)
{
  GdkKeymap *keymap = gdk_keymap_get_for_display (display);
  gunichar c = 0;
  gchar buf[7];
  GdkModifierType consumed, state;

  event->key.type = xevent->xany.type == KeyPress ? GDK_KEY_PRESS : GDK_KEY_RELEASE;
  event->key.time = xevent->xkey.time;

  event->key.state = (GdkModifierType) xevent->xkey.state;
  event->key.group = _gdk_x11_get_group_for_state (display, xevent->xkey.state);
  event->key.hardware_keycode = xevent->xkey.keycode;

  event->key.keyval = GDK_VoidSymbol;

  gdk_keymap_translate_keyboard_state (keymap,
				       event->key.hardware_keycode,
				       event->key.state,
				       event->key.group,
				       &event->key.keyval,
                                       NULL, NULL, &consumed);
   state = event->key.state & ~consumed;
   _gdk_keymap_add_virtual_modifiers_compat (keymap, &state);
   event->key.state |= state;

  event->key.is_modifier = _gdk_keymap_key_is_modifier (keymap, event->key.hardware_keycode);

  /* Fill in event->string crudely, since various programs
   * depend on it.
   */
  event->key.string = NULL;
  
  if (event->key.keyval != GDK_VoidSymbol)
    c = gdk_keyval_to_unicode (event->key.keyval);

  if (c)
    {
      gsize bytes_written;
      gint len;

      /* Apply the control key - Taken from Xlib
       */
      if (event->key.state & GDK_CONTROL_MASK)
	{
	  if ((c >= '@' && c < '\177') || c == ' ') c &= 0x1F;
	  else if (c == '2')
	    {
	      event->key.string = g_memdup ("\0\0", 2);
	      event->key.length = 1;
	      buf[0] = '\0';
	      goto out;
	    }
	  else if (c >= '3' && c <= '7') c -= ('3' - '\033');
	  else if (c == '8') c = '\177';
	  else if (c == '/') c = '_' & 0x1F;
	}
      
      len = g_unichar_to_utf8 (c, buf);
      buf[len] = '\0';
      
      event->key.string = g_locale_from_utf8 (buf, len,
					      NULL, &bytes_written,
					      NULL);
      if (event->key.string)
	event->key.length = bytes_written;
    }
  else if (event->key.keyval == GDK_Escape)
    {
      event->key.length = 1;
      event->key.string = g_strdup ("\033");
    }
  else if (event->key.keyval == GDK_Return ||
	  event->key.keyval == GDK_KP_Enter)
    {
      event->key.length = 1;
      event->key.string = g_strdup ("\r");
    }

  if (!event->key.string)
    {
      event->key.length = 0;
      event->key.string = g_strdup ("");
    }
  
 out:
#ifdef G_ENABLE_DEBUG
  if (_gdk_debug_flags & GDK_DEBUG_EVENTS)
    {
      g_message ("%s:\t\twindow: %ld	 key: %12s  %d",
		 event->type == GDK_KEY_PRESS ? "key press  " : "key release",
		 xevent->xkey.window,
		 event->key.keyval ? gdk_keyval_name (event->key.keyval) : "(none)",
		 event->key.keyval);
  
      if (event->key.length > 0)
	g_message ("\t\tlength: %4d string: \"%s\"",
		   event->key.length, buf);
    }
#endif /* G_ENABLE_DEBUG */  
  return;
}

/**
 * gdk_x11_register_standard_event_type:
 * @display: a #GdkDisplay
 * @event_base: first event type code to register
 * @n_events: number of event type codes to register
 * 
 * Registers interest in receiving extension events with type codes
 * between @event_base and <literal>event_base + n_events - 1</literal>.
 * The registered events must have the window field in the same place
 * as core X events (this is not the case for e.g. XKB extension events).
 *
 * If an event type is registered, events of this type will go through
 * global and window-specific filters (see gdk_window_add_filter()). 
 * Unregistered events will only go through global filters.
 * GDK may register the events of some X extensions on its own.
 * 
 * This function should only be needed in unusual circumstances, e.g.
 * when filtering XInput extension events on the root window.
 *
 * Since: 2.4
 **/
void
gdk_x11_register_standard_event_type (GdkDisplay          *display,
				      gint                 event_base,
				      gint                 n_events)
{
  GdkEventTypeX11 *event_type;
  GdkDisplayX11 *display_x11;

  display_x11 = GDK_DISPLAY_X11 (display);
  event_type = g_new (GdkEventTypeX11, 1);

  event_type->base = event_base;
  event_type->n_events = n_events;

  display_x11->event_types = g_slist_prepend (display_x11->event_types, event_type);
}

/* Return the window this has to do with, if any, rather
 * than the frame or root window that was selecting
 * for substructure
 */
static void
get_real_window (GdkDisplay *display,
		 XEvent     *event,
		 Window     *event_window,
		 Window     *filter_window)
{
  /* Core events all have an event->xany.window field, but that's
   * not true for extension events
   */
  if (event->type >= KeyPress &&
      event->type <= MappingNotify)
    {
      *filter_window = event->xany.window;
      switch (event->type)
	{      
	case CreateNotify:
	  *event_window = event->xcreatewindow.window;
	  break;
	case DestroyNotify:
	  *event_window = event->xdestroywindow.window;
	  break;
	case UnmapNotify:
	  *event_window = event->xunmap.window;
	  break;
	case MapNotify:
	  *event_window = event->xmap.window;
	  break;
	case MapRequest:
	  *event_window = event->xmaprequest.window;
	  break;
	case ReparentNotify:
	  *event_window = event->xreparent.window;
	  break;
	case ConfigureNotify:
	  *event_window = event->xconfigure.window;
	  break;
	case ConfigureRequest:
	  *event_window = event->xconfigurerequest.window;
	  break;
	case GravityNotify:
	  *event_window = event->xgravity.window;
	  break;
	case CirculateNotify:
	  *event_window = event->xcirculate.window;
	  break;
	case CirculateRequest:
	  *event_window = event->xcirculaterequest.window;
	  break;
	default:
	  *event_window = event->xany.window;
	}
    }
  else
    {
      GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (display);
      GSList *tmp_list;

      for (tmp_list = display_x11->event_types;
	   tmp_list;
	   tmp_list = tmp_list->next)
	{
	  GdkEventTypeX11 *event_type = tmp_list->data;

	  if (event->type >= event_type->base &&
	      event->type < event_type->base + event_type->n_events)
	    {
	      *event_window = event->xany.window;
	      *filter_window = event->xany.window;
	      return;
	    }
	}
	   
      *event_window = None;
      *filter_window = None;
    }
}

#ifdef G_ENABLE_DEBUG
static const char notify_modes[][19] = {
  "NotifyNormal",
  "NotifyGrab",
  "NotifyUngrab",
  "NotifyWhileGrabbed"
};

static const char notify_details[][23] = {
  "NotifyAncestor",
  "NotifyVirtual",
  "NotifyInferior",
  "NotifyNonlinear",
  "NotifyNonlinearVirtual",
  "NotifyPointer",
  "NotifyPointerRoot",
  "NotifyDetailNone"
};
#endif

static void
set_user_time (GdkWindow *window,
	       GdkEvent  *event)
{
  g_return_if_fail (event != NULL);

  window = gdk_window_get_toplevel (event->client.window);
  g_return_if_fail (GDK_IS_WINDOW (window));

  /* If an event doesn't have a valid timestamp, we shouldn't use it
   * to update the latest user interaction time.
   */
  if (gdk_event_get_time (event) != GDK_CURRENT_TIME)
    gdk_x11_window_set_user_time (gdk_window_get_toplevel (window),
                                  gdk_event_get_time (event));
}

static gboolean
is_parent_of (GdkWindow *parent,
              GdkWindow *child)
{
  GdkWindow *w;

  w = child;
  while (w != NULL)
    {
      if (w == parent)
	return TRUE;

      w = gdk_window_get_parent (w);
    }

  return FALSE;
}

static gboolean
gdk_event_translate (GdkDisplay *display,
		     GdkEvent   *event,
		     XEvent     *xevent,
		     gboolean    return_exposes)
{
  
  GdkWindow *window;
  GdkWindowObject *window_private;
  GdkWindow *filter_window;
  GdkWindowImplX11 *window_impl = NULL;
  gboolean return_val;
  GdkScreen *screen = NULL;
  GdkScreenX11 *screen_x11 = NULL;
  GdkToplevelX11 *toplevel = NULL;
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (display);
  Window xwindow, filter_xwindow;
  
  return_val = FALSE;

  /* init these, since the done: block uses them */
  window = NULL;
  window_private = NULL;
  event->any.window = NULL;

  if (_gdk_default_filters)
    {
      /* Apply global filters */
      GdkFilterReturn result;
      result = gdk_event_apply_filters (xevent, event, NULL);
      
      if (result != GDK_FILTER_CONTINUE)
        {
          return_val = (result == GDK_FILTER_TRANSLATE) ? TRUE : FALSE;
          goto done;
        }
    }  

  /* Find the GdkWindow that this event relates to.
   * Basically this means substructure events
   * are reported same as structure events
   */
  get_real_window (display, xevent, &xwindow, &filter_xwindow);
  
  window = gdk_window_lookup_for_display (display, xwindow);
  /* We may receive events such as NoExpose/GraphicsExpose
   * and ShmCompletion for pixmaps
   */
  if (window && !GDK_IS_WINDOW (window))
    window = NULL;
  window_private = (GdkWindowObject *) window;

  /* We always run the filters for the window where the event
   * is delivered, not the window that it relates to
   */
  if (filter_xwindow == xwindow)
    filter_window = window;
  else
    {
      filter_window = gdk_window_lookup_for_display (display, filter_xwindow);
      if (filter_window && !GDK_IS_WINDOW (filter_window))
	filter_window = NULL;
    }

  if (window)
    {
      screen = GDK_WINDOW_SCREEN (window);
      screen_x11 = GDK_SCREEN_X11 (screen);
      toplevel = _gdk_x11_window_get_toplevel (window);
    }
    
  if (window != NULL)
    {
      /* Apply keyboard grabs to non-native windows */
      if (/* Is key event */
	  (xevent->type == KeyPress || xevent->type == KeyRelease) &&
	  /* And we have a grab */
	  display->keyboard_grab.window != NULL &&
	  (
	   /* The window is not a descendant of the grabbed window */
	   !is_parent_of ((GdkWindow *)display->keyboard_grab.window, window) ||
	   /* Or owner event is false */
	   !display->keyboard_grab.owner_events
	   )
	  )
        {
	  /* Report key event against grab window */
          window = display->keyboard_grab.window;;
          window_private = (GdkWindowObject *) window;
        }

      window_impl = GDK_WINDOW_IMPL_X11 (window_private->impl);
      
      /* Move key events on focus window to the real toplevel, and
       * filter out all other events on focus window
       */          
      if (toplevel && xwindow == toplevel->focus_window)
	{
	  switch (xevent->type)
	    {
	    case KeyPress:
	    case KeyRelease:
	      xwindow = GDK_WINDOW_XID (window);
	      xevent->xany.window = xwindow;
	      break;
	    default:
	      return FALSE;
	    }
	}

      g_object_ref (window);
    }

  event->any.window = window;
  event->any.send_event = xevent->xany.send_event ? TRUE : FALSE;
  
  if (window_private && GDK_WINDOW_DESTROYED (window))
    {
      if (xevent->type != DestroyNotify)
	{
	  return_val = FALSE;
	  goto done;
	}
    }
  else if (filter_window)
    {
      /* Apply per-window filters */
      GdkWindowObject *filter_private = (GdkWindowObject *) filter_window;
      GdkFilterReturn result;

      if (filter_private->filters)
	{
	  g_object_ref (filter_window);
	  
	  result = gdk_event_apply_filters (xevent, event,
					    filter_window);
	  
	  g_object_unref (filter_window);
      
	  if (result != GDK_FILTER_CONTINUE)
	    {
	      return_val = (result == GDK_FILTER_TRANSLATE) ? TRUE : FALSE;
	      goto done;
	    }
	}
    }

  if (xevent->type == DestroyNotify)
    {
      int i, n;

      n = gdk_display_get_n_screens (display);
      for (i = 0; i < n; i++)
        {
          screen = gdk_display_get_screen (display, i);
          screen_x11 = GDK_SCREEN_X11 (screen);

          if (screen_x11->wmspec_check_window == xwindow)
            {
              screen_x11->wmspec_check_window = None;
              screen_x11->last_wmspec_check_time = 0;
              g_free (screen_x11->window_manager_name);
              screen_x11->window_manager_name = g_strdup ("unknown");

              /* careful, reentrancy */
              _gdk_x11_screen_window_manager_changed (screen);

              return_val = FALSE;
              goto done;
            }
        }
    }

  if (window &&
      (xevent->xany.type == MotionNotify ||
       xevent->xany.type == ButtonRelease))
    {
      if (_gdk_moveresize_handle_event (xevent))
	{
          return_val = FALSE;
          goto done;
        }
    }
  
  /* We do a "manual" conversion of the XEvent to a
   *  GdkEvent. The structures are mostly the same so
   *  the conversion is fairly straightforward. We also
   *  optionally print debugging info regarding events
   *  received.
   */

  return_val = TRUE;

  switch (xevent->type)
    {
    case KeyPress:
      if (window_private == NULL)
        {
          return_val = FALSE;
          break;
        }
      translate_key_event (display, event, xevent);
      set_user_time (window, event);
      break;

    case KeyRelease:
      if (window_private == NULL)
        {
          return_val = FALSE;
          break;
        }
      
      /* Emulate detectable auto-repeat by checking to see
       * if the next event is a key press with the same
       * keycode and timestamp, and if so, ignoring the event.
       */

      if (!display_x11->have_xkb_autorepeat && XPending (xevent->xkey.display))
	{
	  XEvent next_event;

	  XPeekEvent (xevent->xkey.display, &next_event);

	  if (next_event.type == KeyPress &&
	      next_event.xkey.keycode == xevent->xkey.keycode &&
	      next_event.xkey.time == xevent->xkey.time)
	    {
	      return_val = FALSE;
	      break;
	    }
	}

      translate_key_event (display, event, xevent);
      break;
      
    case ButtonPress:
      GDK_NOTE (EVENTS, 
		g_message ("button press:\t\twindow: %ld  x,y: %d %d  button: %d",
			   xevent->xbutton.window,
			   xevent->xbutton.x, xevent->xbutton.y,
			   xevent->xbutton.button));
      
      if (window_private == NULL)
	{
	  return_val = FALSE;
	  break;
	}
      
      /* If we get a ButtonPress event where the button is 4 or 5,
	 it's a Scroll event */
      switch (xevent->xbutton.button)
        {
        case 4: /* up */
        case 5: /* down */
        case 6: /* left */
        case 7: /* right */
	  event->scroll.type = GDK_SCROLL;

          if (xevent->xbutton.button == 4)
            event->scroll.direction = GDK_SCROLL_UP;
          else if (xevent->xbutton.button == 5)
            event->scroll.direction = GDK_SCROLL_DOWN;
          else if (xevent->xbutton.button == 6)
            event->scroll.direction = GDK_SCROLL_LEFT;
          else
            event->scroll.direction = GDK_SCROLL_RIGHT;

	  event->scroll.window = window;
	  event->scroll.time = xevent->xbutton.time;
	  event->scroll.x = xevent->xbutton.x;
	  event->scroll.y = xevent->xbutton.y;
	  event->scroll.x_root = (gfloat)xevent->xbutton.x_root;
	  event->scroll.y_root = (gfloat)xevent->xbutton.y_root;
	  event->scroll.state = (GdkModifierType) xevent->xbutton.state;
	  event->scroll.device = display->core_pointer;
	  
	  if (!set_screen_from_root (display, event, xevent->xbutton.root))
	    {
	      return_val = FALSE;
	      break;
	    }
	  
          break;
          
        default:
	  event->button.type = GDK_BUTTON_PRESS;
	  event->button.window = window;
	  event->button.time = xevent->xbutton.time;
	  event->button.x = xevent->xbutton.x;
	  event->button.y = xevent->xbutton.y;
	  event->button.x_root = (gfloat)xevent->xbutton.x_root;
	  event->button.y_root = (gfloat)xevent->xbutton.y_root;
	  event->button.axes = NULL;
	  event->button.state = (GdkModifierType) xevent->xbutton.state;
	  event->button.button = xevent->xbutton.button;
	  event->button.device = display->core_pointer;
	  
	  if (!set_screen_from_root (display, event, xevent->xbutton.root))
	    {
	      return_val = FALSE;
	      break;
	    }
          break;
	}

      set_user_time (window, event);

      break;
      
    case ButtonRelease:
      GDK_NOTE (EVENTS, 
		g_message ("button release:\twindow: %ld  x,y: %d %d  button: %d",
			   xevent->xbutton.window,
			   xevent->xbutton.x, xevent->xbutton.y,
			   xevent->xbutton.button));
      
      if (window_private == NULL)
	{
	  return_val = FALSE;
	  break;
	}
      
      /* We treat button presses as scroll wheel events, so ignore the release */
      if (xevent->xbutton.button == 4 || xevent->xbutton.button == 5 ||
          xevent->xbutton.button == 6 || xevent->xbutton.button ==7)
	{
	  return_val = FALSE;
	  break;
	}

      event->button.type = GDK_BUTTON_RELEASE;
      event->button.window = window;
      event->button.time = xevent->xbutton.time;
      event->button.x = xevent->xbutton.x;
      event->button.y = xevent->xbutton.y;
      event->button.x_root = (gfloat)xevent->xbutton.x_root;
      event->button.y_root = (gfloat)xevent->xbutton.y_root;
      event->button.axes = NULL;
      event->button.state = (GdkModifierType) xevent->xbutton.state;
      event->button.button = xevent->xbutton.button;
      event->button.device = display->core_pointer;

      if (!set_screen_from_root (display, event, xevent->xbutton.root))
	return_val = FALSE;
      
      break;
      
    case MotionNotify:
      GDK_NOTE (EVENTS,
		g_message ("motion notify:\t\twindow: %ld  x,y: %d %d  hint: %s", 
			   xevent->xmotion.window,
			   xevent->xmotion.x, xevent->xmotion.y,
			   (xevent->xmotion.is_hint) ? "true" : "false"));
      
      if (window_private == NULL)
	{
	  return_val = FALSE;
	  break;
	}

      event->motion.type = GDK_MOTION_NOTIFY;
      event->motion.window = window;
      event->motion.time = xevent->xmotion.time;
      event->motion.x = xevent->xmotion.x;
      event->motion.y = xevent->xmotion.y;
      event->motion.x_root = (gfloat)xevent->xmotion.x_root;
      event->motion.y_root = (gfloat)xevent->xmotion.y_root;
      event->motion.axes = NULL;
      event->motion.state = (GdkModifierType) xevent->xmotion.state;
      event->motion.is_hint = xevent->xmotion.is_hint;
      event->motion.device = display->core_pointer;
      
      if (!set_screen_from_root (display, event, xevent->xbutton.root))
	{
	  return_val = FALSE;
	  break;
	}
            
      break;
      
    case EnterNotify:
      GDK_NOTE (EVENTS,
                g_message ("enter notify:\t\twindow: %ld  detail: %d subwin: %ld mode: %d",
                           xevent->xcrossing.window,
                           xevent->xcrossing.detail,
                           xevent->xcrossing.subwindow,
                           xevent->xcrossing.mode));

      if (window_private == NULL)
        {
          return_val = FALSE;
          break;
        }
      
      if (!set_screen_from_root (display, event, xevent->xbutton.root))
	{
	  return_val = FALSE;
	  break;
	}
      
      /* Handle focusing (in the case where no window manager is running */
      if (toplevel && xevent->xcrossing.detail != NotifyInferior)
	{
	  toplevel->has_pointer = TRUE;

	  if (xevent->xcrossing.focus && !toplevel->has_focus_window)
	    {
	      gboolean had_focus = HAS_FOCUS (toplevel);
	      
	      toplevel->has_pointer_focus = TRUE;
	      
	      if (HAS_FOCUS (toplevel) != had_focus)
		generate_focus_event (window, TRUE);
	    }
	}

      event->crossing.type = GDK_ENTER_NOTIFY;
      event->crossing.window = window;
      
      /* If the subwindow field of the XEvent is non-NULL, then
       *  lookup the corresponding GdkWindow.
       */
      if (xevent->xcrossing.subwindow != None)
	event->crossing.subwindow = gdk_window_lookup_for_display (display, xevent->xcrossing.subwindow);
      else
	event->crossing.subwindow = NULL;
      
      event->crossing.time = xevent->xcrossing.time;
      event->crossing.x = xevent->xcrossing.x;
      event->crossing.y = xevent->xcrossing.y;
      event->crossing.x_root = xevent->xcrossing.x_root;
      event->crossing.y_root = xevent->xcrossing.y_root;
      
      /* Translate the crossing mode into Gdk terms.
       */
      switch (xevent->xcrossing.mode)
	{
	case NotifyNormal:
	  event->crossing.mode = GDK_CROSSING_NORMAL;
	  break;
	case NotifyGrab:
	  event->crossing.mode = GDK_CROSSING_GRAB;
	  break;
	case NotifyUngrab:
	  event->crossing.mode = GDK_CROSSING_UNGRAB;
	  break;
	};
      
      /* Translate the crossing detail into Gdk terms.
       */
      switch (xevent->xcrossing.detail)
	{
	case NotifyInferior:
	  event->crossing.detail = GDK_NOTIFY_INFERIOR;
	  break;
	case NotifyAncestor:
	  event->crossing.detail = GDK_NOTIFY_ANCESTOR;
	  break;
	case NotifyVirtual:
	  event->crossing.detail = GDK_NOTIFY_VIRTUAL;
	  break;
	case NotifyNonlinear:
	  event->crossing.detail = GDK_NOTIFY_NONLINEAR;
	  break;
	case NotifyNonlinearVirtual:
	  event->crossing.detail = GDK_NOTIFY_NONLINEAR_VIRTUAL;
	  break;
	default:
	  event->crossing.detail = GDK_NOTIFY_UNKNOWN;
	  break;
	}
      
      event->crossing.focus = xevent->xcrossing.focus;
      event->crossing.state = xevent->xcrossing.state;
  
      break;
      
    case LeaveNotify:
      GDK_NOTE (EVENTS, 
                g_message ("leave notify:\t\twindow: %ld  detail: %d subwin: %ld mode: %d",
                           xevent->xcrossing.window,
                           xevent->xcrossing.detail,
                           xevent->xcrossing.subwindow,
                           xevent->xcrossing.mode));

      if (window_private == NULL)
        {
          return_val = FALSE;
          break;
        }

      if (!set_screen_from_root (display, event, xevent->xbutton.root))
	{
	  return_val = FALSE;
	  break;
	}
                  
      /* Handle focusing (in the case where no window manager is running */
      if (toplevel && xevent->xcrossing.detail != NotifyInferior)
	{
	  toplevel->has_pointer = FALSE;

	  if (xevent->xcrossing.focus && !toplevel->has_focus_window)
	    {
	      gboolean had_focus = HAS_FOCUS (toplevel);
	      
	      toplevel->has_pointer_focus = FALSE;
	      
	      if (HAS_FOCUS (toplevel) != had_focus)
		generate_focus_event (window, FALSE);
	    }
	}

      event->crossing.type = GDK_LEAVE_NOTIFY;
      event->crossing.window = window;
      
      /* If the subwindow field of the XEvent is non-NULL, then
       *  lookup the corresponding GdkWindow.
       */
      if (xevent->xcrossing.subwindow != None)
	event->crossing.subwindow = gdk_window_lookup_for_display (display, xevent->xcrossing.subwindow);
      else
	event->crossing.subwindow = NULL;
      
      event->crossing.time = xevent->xcrossing.time;
      event->crossing.x = xevent->xcrossing.x;
      event->crossing.y = xevent->xcrossing.y;
      event->crossing.x_root = xevent->xcrossing.x_root;
      event->crossing.y_root = xevent->xcrossing.y_root;
      
      /* Translate the crossing mode into Gdk terms.
       */
      switch (xevent->xcrossing.mode)
	{
	case NotifyNormal:
	  event->crossing.mode = GDK_CROSSING_NORMAL;
	  break;
	case NotifyGrab:
	  event->crossing.mode = GDK_CROSSING_GRAB;
	  break;
	case NotifyUngrab:
	  event->crossing.mode = GDK_CROSSING_UNGRAB;
	  break;
	};
      
      /* Translate the crossing detail into Gdk terms.
       */
      switch (xevent->xcrossing.detail)
	{
	case NotifyInferior:
	  event->crossing.detail = GDK_NOTIFY_INFERIOR;
	  break;
	case NotifyAncestor:
	  event->crossing.detail = GDK_NOTIFY_ANCESTOR;
	  break;
	case NotifyVirtual:
	  event->crossing.detail = GDK_NOTIFY_VIRTUAL;
	  break;
	case NotifyNonlinear:
	  event->crossing.detail = GDK_NOTIFY_NONLINEAR;
	  break;
	case NotifyNonlinearVirtual:
	  event->crossing.detail = GDK_NOTIFY_NONLINEAR_VIRTUAL;
	  break;
	default:
	  event->crossing.detail = GDK_NOTIFY_UNKNOWN;
	  break;
	}
      
      event->crossing.focus = xevent->xcrossing.focus;
      event->crossing.state = xevent->xcrossing.state;
      
      break;
      
      /* We only care about focus events that indicate that _this_
       * window (not a ancestor or child) got or lost the focus
       */
    case FocusIn:
      GDK_NOTE (EVENTS,
		g_message ("focus in:\t\twindow: %ld, detail: %s, mode: %s",
			   xevent->xfocus.window,
			   notify_details[xevent->xfocus.detail],
			   notify_modes[xevent->xfocus.mode]));
      
      if (toplevel)
	{
	  gboolean had_focus = HAS_FOCUS (toplevel);
	  
	  switch (xevent->xfocus.detail)
	    {
	    case NotifyAncestor:
	    case NotifyVirtual:
	      /* When the focus moves from an ancestor of the window to
	       * the window or a descendent of the window, *and* the
	       * pointer is inside the window, then we were previously
	       * receiving keystroke events in the has_pointer_focus
	       * case and are now receiving them in the
	       * has_focus_window case.
	       */
	      if (toplevel->has_pointer &&
		  xevent->xfocus.mode != NotifyGrab &&
		  xevent->xfocus.mode != NotifyUngrab)
		toplevel->has_pointer_focus = FALSE;
	      
	      /* fall through */
	    case NotifyNonlinear:
	    case NotifyNonlinearVirtual:
	      if (xevent->xfocus.mode != NotifyGrab &&
		  xevent->xfocus.mode != NotifyUngrab)
		toplevel->has_focus_window = TRUE;
	      /* We pretend that the focus moves to the grab
	       * window, so we pay attention to NotifyGrab
	       * NotifyUngrab, and ignore NotifyWhileGrabbed
	       */
	      if (xevent->xfocus.mode != NotifyWhileGrabbed)
		toplevel->has_focus = TRUE;
	      break;
	    case NotifyPointer:
	      /* The X server sends NotifyPointer/NotifyGrab,
	       * but the pointer focus is ignored while a
	       * grab is in effect
	       */
	      if (xevent->xfocus.mode != NotifyGrab &&
		  xevent->xfocus.mode != NotifyUngrab)
		toplevel->has_pointer_focus = TRUE;
	      break;
	    case NotifyInferior:
	    case NotifyPointerRoot:
	    case NotifyDetailNone:
	      break;
	    }

	  if (HAS_FOCUS (toplevel) != had_focus)
	    generate_focus_event (window, TRUE);
	}
      break;
    case FocusOut:
      GDK_NOTE (EVENTS,
		g_message ("focus out:\t\twindow: %ld, detail: %s, mode: %s",
			   xevent->xfocus.window,
			   notify_details[xevent->xfocus.detail],
			   notify_modes[xevent->xfocus.mode]));
      
      if (toplevel)
	{
	  gboolean had_focus = HAS_FOCUS (toplevel);
	    
	  switch (xevent->xfocus.detail)
	    {
	    case NotifyAncestor:
	    case NotifyVirtual:
	      /* When the focus moves from the window or a descendent
	       * of the window to an ancestor of the window, *and* the
	       * pointer is inside the window, then we were previously
	       * receiving keystroke events in the has_focus_window
	       * case and are now receiving them in the
	       * has_pointer_focus case.
	       */
	      if (toplevel->has_pointer &&
		  xevent->xfocus.mode != NotifyGrab &&
		  xevent->xfocus.mode != NotifyUngrab)
		toplevel->has_pointer_focus = TRUE;

	      /* fall through */
	    case NotifyNonlinear:
	    case NotifyNonlinearVirtual:
	      if (xevent->xfocus.mode != NotifyGrab &&
		  xevent->xfocus.mode != NotifyUngrab)
		toplevel->has_focus_window = FALSE;
	      if (xevent->xfocus.mode != NotifyWhileGrabbed)
		toplevel->has_focus = FALSE;
	      break;
	    case NotifyPointer:
	      if (xevent->xfocus.mode != NotifyGrab &&
		  xevent->xfocus.mode != NotifyUngrab)
		toplevel->has_pointer_focus = FALSE;
	    break;
	    case NotifyInferior:
	    case NotifyPointerRoot:
	    case NotifyDetailNone:
	      break;
	    }

	  if (HAS_FOCUS (toplevel) != had_focus)
	    generate_focus_event (window, FALSE);
	}
      break;

#if 0      
 	  /* gdk_keyboard_grab() causes following events. These events confuse
 	   * the XIM focus, so ignore them.
 	   */
 	  if (xevent->xfocus.mode == NotifyGrab ||
 	      xevent->xfocus.mode == NotifyUngrab)
 	    break;
#endif

    case KeymapNotify:
      GDK_NOTE (EVENTS,
		g_message ("keymap notify"));

      /* Not currently handled */
      return_val = FALSE;
      break;
      
    case Expose:
      GDK_NOTE (EVENTS,
		g_message ("expose:\t\twindow: %ld  %d	x,y: %d %d  w,h: %d %d%s",
			   xevent->xexpose.window, xevent->xexpose.count,
			   xevent->xexpose.x, xevent->xexpose.y,
			   xevent->xexpose.width, xevent->xexpose.height,
			   event->any.send_event ? " (send)" : ""));
     
      if (window_private == NULL)
        {
          return_val = FALSE;
          break;
        }

      {
	GdkRectangle expose_rect;

	expose_rect.x = xevent->xexpose.x;
	expose_rect.y = xevent->xexpose.y;
	expose_rect.width = xevent->xexpose.width;
	expose_rect.height = xevent->xexpose.height;

	_gdk_window_process_expose (window, xevent->xexpose.serial, &expose_rect);
	 return_val = FALSE;
      }

      break;
      
    case GraphicsExpose:
      {
	GdkRectangle expose_rect;

        GDK_NOTE (EVENTS,
		  g_message ("graphics expose:\tdrawable: %ld",
			     xevent->xgraphicsexpose.drawable));
 
        if (window_private == NULL)
          {
            return_val = FALSE;
            break;
          }
        
	expose_rect.x = xevent->xgraphicsexpose.x;
	expose_rect.y = xevent->xgraphicsexpose.y;
	expose_rect.width = xevent->xgraphicsexpose.width;
	expose_rect.height = xevent->xgraphicsexpose.height;
	    
	if (return_exposes)
	  {
	    event->expose.type = GDK_EXPOSE;
	    event->expose.area = expose_rect;
	    event->expose.region = gdk_region_rectangle (&expose_rect);
	    event->expose.window = window;
	    event->expose.count = xevent->xgraphicsexpose.count;

	    return_val = TRUE;
	  }
	else
	  {
	    _gdk_window_process_expose (window, xevent->xgraphicsexpose.serial, &expose_rect);
	    
	    return_val = FALSE;
	  }
	
      }
      break;
      
    case NoExpose:
      GDK_NOTE (EVENTS,
		g_message ("no expose:\t\tdrawable: %ld",
			   xevent->xnoexpose.drawable));
      
      event->no_expose.type = GDK_NO_EXPOSE;
      event->no_expose.window = window;
      
      break;
      
    case VisibilityNotify:
#ifdef G_ENABLE_DEBUG
      if (_gdk_debug_flags & GDK_DEBUG_EVENTS)
	switch (xevent->xvisibility.state)
	  {
	  case VisibilityFullyObscured:
	    g_message ("visibility notify:\twindow: %ld	 none",
		       xevent->xvisibility.window);
	    break;
	  case VisibilityPartiallyObscured:
	    g_message ("visibility notify:\twindow: %ld	 partial",
		       xevent->xvisibility.window);
	    break;
	  case VisibilityUnobscured:
	    g_message ("visibility notify:\twindow: %ld	 full",
		       xevent->xvisibility.window);
	    break;
	  }
#endif /* G_ENABLE_DEBUG */
      
      if (window_private == NULL)
        {
          return_val = FALSE;
          break;
        }
      
      event->visibility.type = GDK_VISIBILITY_NOTIFY;
      event->visibility.window = window;
      
      switch (xevent->xvisibility.state)
	{
	case VisibilityFullyObscured:
	  event->visibility.state = GDK_VISIBILITY_FULLY_OBSCURED;
	  break;
	  
	case VisibilityPartiallyObscured:
	  event->visibility.state = GDK_VISIBILITY_PARTIAL;
	  break;
	  
	case VisibilityUnobscured:
	  event->visibility.state = GDK_VISIBILITY_UNOBSCURED;
	  break;
	}
      
      break;
      
    case CreateNotify:
      GDK_NOTE (EVENTS,
		g_message ("create notify:\twindow: %ld  x,y: %d %d	w,h: %d %d  b-w: %d  parent: %ld	 ovr: %d",
			   xevent->xcreatewindow.window,
			   xevent->xcreatewindow.x,
			   xevent->xcreatewindow.y,
			   xevent->xcreatewindow.width,
			   xevent->xcreatewindow.height,
			   xevent->xcreatewindow.border_width,
			   xevent->xcreatewindow.parent,
			   xevent->xcreatewindow.override_redirect));
      /* not really handled */
      break;
      
    case DestroyNotify:
      GDK_NOTE (EVENTS,
		g_message ("destroy notify:\twindow: %ld",
			   xevent->xdestroywindow.window));

      /* Ignore DestroyNotify from SubstructureNotifyMask */
      if (xevent->xdestroywindow.window == xevent->xdestroywindow.event)
	{
	  event->any.type = GDK_DESTROY;
	  event->any.window = window;
	  
	  return_val = window_private && !GDK_WINDOW_DESTROYED (window);
	  
	  if (window && GDK_WINDOW_XID (window) != screen_x11->xroot_window)
	    gdk_window_destroy_notify (window);
	}
      else
	return_val = FALSE;
      
      break;
      
    case UnmapNotify:
      GDK_NOTE (EVENTS,
		g_message ("unmap notify:\t\twindow: %ld",
			   xevent->xmap.window));
      
      event->any.type = GDK_UNMAP;
      event->any.window = window;      

      /* If the WM supports the _NET_WM_STATE_HIDDEN hint, we do not want to
       * interpret UnmapNotify events as implying iconic state.
       * http://bugzilla.gnome.org/show_bug.cgi?id=590726.
       */
      if (screen &&
          !gdk_x11_screen_supports_net_wm_hint (screen,
                                                gdk_atom_intern_static_string ("_NET_WM_STATE_HIDDEN")))
        {
          /* If we are shown (not withdrawn) and get an unmap, it means we were
           * iconified in the X sense. If we are withdrawn, and get an unmap, it
           * means we hid the window ourselves, so we will have already flipped
           * the iconified bit off.
           */
          if (window && GDK_WINDOW_IS_MAPPED (window))
            gdk_synthesize_window_state (window,
                                         0,
                                         GDK_WINDOW_STATE_ICONIFIED);
        }

      if (window)
        _gdk_xgrab_check_unmap (window, xevent->xany.serial);

      break;
      
    case MapNotify:
      GDK_NOTE (EVENTS,
		g_message ("map notify:\t\twindow: %ld",
			   xevent->xmap.window));
      
      event->any.type = GDK_MAP;
      event->any.window = window;

      /* Unset iconified if it was set */
      if (window && (((GdkWindowObject*)window)->state & GDK_WINDOW_STATE_ICONIFIED))
        gdk_synthesize_window_state (window,
                                     GDK_WINDOW_STATE_ICONIFIED,
                                     0);
      
      break;
      
    case ReparentNotify:
      GDK_NOTE (EVENTS,
		g_message ("reparent notify:\twindow: %ld  x,y: %d %d  parent: %ld	ovr: %d",
			   xevent->xreparent.window,
			   xevent->xreparent.x,
			   xevent->xreparent.y,
			   xevent->xreparent.parent,
			   xevent->xreparent.override_redirect));

      /* Not currently handled */
      return_val = FALSE;
      break;
      
    case ConfigureNotify:
      GDK_NOTE (EVENTS,
		g_message ("configure notify:\twindow: %ld  x,y: %d %d	w,h: %d %d  b-w: %d  above: %ld	 ovr: %d%s",
			   xevent->xconfigure.window,
			   xevent->xconfigure.x,
			   xevent->xconfigure.y,
			   xevent->xconfigure.width,
			   xevent->xconfigure.height,
			   xevent->xconfigure.border_width,
			   xevent->xconfigure.above,
			   xevent->xconfigure.override_redirect,
			   !window
			   ? " (discarding)"
			   : GDK_WINDOW_TYPE (window) == GDK_WINDOW_CHILD
			   ? " (discarding child)"
			   : xevent->xconfigure.event != xevent->xconfigure.window
			   ? " (discarding substructure)"
			   : ""));
      if (window && GDK_WINDOW_TYPE (window) == GDK_WINDOW_ROOT)
        { 
	  window_private->width = xevent->xconfigure.width;
	  window_private->height = xevent->xconfigure.height;

	  _gdk_window_update_size (window);
	  _gdk_x11_drawable_update_size (window_private->impl);
	  _gdk_x11_screen_size_changed (screen, xevent);
        }

      if (window &&
	  xevent->xconfigure.event == xevent->xconfigure.window &&
	  !GDK_WINDOW_DESTROYED (window) &&
	  window_private->input_window != NULL)
	_gdk_input_configure_event (&xevent->xconfigure, window);
      
#ifdef HAVE_XSYNC
      if (toplevel && display_x11->use_sync && !XSyncValueIsZero (toplevel->pending_counter_value))
	{
	  toplevel->current_counter_value = toplevel->pending_counter_value;
	  XSyncIntToValue (&toplevel->pending_counter_value, 0);
	}
#endif

      if (!window ||
	  xevent->xconfigure.event != xevent->xconfigure.window ||
          GDK_WINDOW_TYPE (window) == GDK_WINDOW_CHILD ||
          GDK_WINDOW_TYPE (window) == GDK_WINDOW_ROOT)
	return_val = FALSE;
      else
	{
	  event->configure.type = GDK_CONFIGURE;
	  event->configure.window = window;
	  event->configure.width = xevent->xconfigure.width;
	  event->configure.height = xevent->xconfigure.height;
	  
	  if (!xevent->xconfigure.send_event &&
	      !xevent->xconfigure.override_redirect &&
	      !GDK_WINDOW_DESTROYED (window))
	    {
	      gint tx = 0;
	      gint ty = 0;
	      Window child_window = 0;

	      gdk_error_trap_push ();
	      if (XTranslateCoordinates (GDK_DRAWABLE_XDISPLAY (window),
					 GDK_DRAWABLE_XID (window),
					 screen_x11->xroot_window,
					 0, 0,
					 &tx, &ty,
					 &child_window))
		{
		  event->configure.x = tx;
		  event->configure.y = ty;
		}
	      gdk_error_trap_pop ();
	    }
	  else
	    {
	      event->configure.x = xevent->xconfigure.x;
	      event->configure.y = xevent->xconfigure.y;
	    }
	  window_private->x = event->configure.x;
	  window_private->y = event->configure.y;
	  window_private->width = xevent->xconfigure.width;
	  window_private->height = xevent->xconfigure.height;
	  
	  _gdk_window_update_size (window);
	  _gdk_x11_drawable_update_size (window_private->impl);
	  
	  if (window_private->resize_count >= 1)
	    {
	      window_private->resize_count -= 1;

	      if (window_private->resize_count == 0)
		_gdk_moveresize_configure_done (display, window);
	    }
	}
      break;
      
    case PropertyNotify:
      GDK_NOTE (EVENTS,
		g_message ("property notify:\twindow: %ld, atom(%ld): %s%s%s",
			   xevent->xproperty.window,
			   xevent->xproperty.atom,
			   "\"",
			   gdk_x11_get_xatom_name_for_display (display, xevent->xproperty.atom),
			   "\""));

      if (window_private == NULL)
        {
	  return_val = FALSE;
          break;
        }

      /* We compare with the serial of the last time we mapped the
       * window to avoid refetching properties that we set ourselves
       */
      if (toplevel &&
	  xevent->xproperty.serial >= toplevel->map_serial)
	{
	  if (xevent->xproperty.atom == gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_STATE"))
	    gdk_check_wm_state_changed (window);
	  
	  if (xevent->xproperty.atom == gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_DESKTOP"))
	    gdk_check_wm_desktop_changed (window);
	}
      
      if (window_private->event_mask & GDK_PROPERTY_CHANGE_MASK) 
	{
	  event->property.type = GDK_PROPERTY_NOTIFY;
	  event->property.window = window;
	  event->property.atom = gdk_x11_xatom_to_atom_for_display (display, xevent->xproperty.atom);
	  event->property.time = xevent->xproperty.time;
	  event->property.state = xevent->xproperty.state;
	}
      else
	return_val = FALSE;

      break;
      
    case SelectionClear:
      GDK_NOTE (EVENTS,
		g_message ("selection clear:\twindow: %ld",
			   xevent->xproperty.window));

      if (_gdk_selection_filter_clear_event (&xevent->xselectionclear))
	{
	  event->selection.type = GDK_SELECTION_CLEAR;
	  event->selection.window = window;
	  event->selection.selection = gdk_x11_xatom_to_atom_for_display (display, xevent->xselectionclear.selection);
	  event->selection.time = xevent->xselectionclear.time;
	}
      else
	return_val = FALSE;
	  
      break;
      
    case SelectionRequest:
      GDK_NOTE (EVENTS,
		g_message ("selection request:\twindow: %ld",
			   xevent->xproperty.window));
      
      event->selection.type = GDK_SELECTION_REQUEST;
      event->selection.window = window;
      event->selection.selection = gdk_x11_xatom_to_atom_for_display (display, xevent->xselectionrequest.selection);
      event->selection.target = gdk_x11_xatom_to_atom_for_display (display, xevent->xselectionrequest.target);
      event->selection.property = gdk_x11_xatom_to_atom_for_display (display, xevent->xselectionrequest.property);
      event->selection.requestor = xevent->xselectionrequest.requestor;
      event->selection.time = xevent->xselectionrequest.time;
      
      break;
      
    case SelectionNotify:
      GDK_NOTE (EVENTS,
		g_message ("selection notify:\twindow: %ld",
			   xevent->xproperty.window));
      
      
      event->selection.type = GDK_SELECTION_NOTIFY;
      event->selection.window = window;
      event->selection.selection = gdk_x11_xatom_to_atom_for_display (display, xevent->xselection.selection);
      event->selection.target = gdk_x11_xatom_to_atom_for_display (display, xevent->xselection.target);
      if (xevent->xselection.property == None)
        event->selection.property = GDK_NONE;
      else
        event->selection.property = gdk_x11_xatom_to_atom_for_display (display, xevent->xselection.property);
      event->selection.time = xevent->xselection.time;
      
      break;
      
    case ColormapNotify:
      GDK_NOTE (EVENTS,
		g_message ("colormap notify:\twindow: %ld",
			   xevent->xcolormap.window));
      
      /* Not currently handled */
      return_val = FALSE;
      break;
      
    case ClientMessage:
      {
	GList *tmp_list;
	GdkFilterReturn result = GDK_FILTER_CONTINUE;
	GdkAtom message_type = gdk_x11_xatom_to_atom_for_display (display, xevent->xclient.message_type);

	GDK_NOTE (EVENTS,
		  g_message ("client message:\twindow: %ld",
			     xevent->xclient.window));
	
	tmp_list = display_x11->client_filters;
	while (tmp_list)
	  {
	    GdkClientFilter *filter = tmp_list->data;
	    tmp_list = tmp_list->next;
	    
	    if (filter->type == message_type)
	      {
		result = (*filter->function) (xevent, event, filter->data);
		if (result != GDK_FILTER_CONTINUE)
		  break;
	      }
	  }

	switch (result)
	  {
	  case GDK_FILTER_REMOVE:
	    return_val = FALSE;
	    break;
	  case GDK_FILTER_TRANSLATE:
	    return_val = TRUE;
	    break;
	  case GDK_FILTER_CONTINUE:
	    /* Send unknown ClientMessage's on to Gtk for it to use */
            if (window_private == NULL)
              {
                return_val = FALSE;
              }
            else
              {
                event->client.type = GDK_CLIENT_EVENT;
                event->client.window = window;
                event->client.message_type = message_type;
                event->client.data_format = xevent->xclient.format;
                memcpy(&event->client.data, &xevent->xclient.data,
                       sizeof(event->client.data));
              }
            break;
          }
      }
      
      break;
      
    case MappingNotify:
      GDK_NOTE (EVENTS,
		g_message ("mapping notify"));
      
      /* Let XLib know that there is a new keyboard mapping.
       */
      XRefreshKeyboardMapping (&xevent->xmapping);
      _gdk_keymap_keys_changed (display);
      return_val = FALSE;
      break;

    default:
#ifdef HAVE_XKB
      if (xevent->type == display_x11->xkb_event_type)
	{
	  XkbEvent *xkb_event = (XkbEvent *)xevent;

	  switch (xkb_event->any.xkb_type)
	    {
	    case XkbNewKeyboardNotify:
	    case XkbMapNotify:
	      _gdk_keymap_keys_changed (display);

	      return_val = FALSE;
	      break;
	      
	    case XkbStateNotify:
	      _gdk_keymap_state_changed (display, xevent);
	      break;
	    }
	}
      else
#endif
#ifdef HAVE_XFIXES
      if (xevent->type - display_x11->xfixes_event_base == XFixesSelectionNotify)
	{
	  XFixesSelectionNotifyEvent *selection_notify = (XFixesSelectionNotifyEvent *)xevent;

	  _gdk_x11_screen_process_owner_change (screen, xevent);
	  
	  event->owner_change.type = GDK_OWNER_CHANGE;
	  event->owner_change.window = window;
	  event->owner_change.owner = selection_notify->owner;
	  event->owner_change.reason = selection_notify->subtype;
	  event->owner_change.selection = 
	    gdk_x11_xatom_to_atom_for_display (display, 
					       selection_notify->selection);
	  event->owner_change.time = selection_notify->timestamp;
	  event->owner_change.selection_time = selection_notify->selection_timestamp;
	  
	  return_val = TRUE;
	}
      else
#endif
#ifdef HAVE_RANDR
      if (xevent->type - display_x11->xrandr_event_base == RRScreenChangeNotify ||
          xevent->type - display_x11->xrandr_event_base == RRNotify)
	{
          if (screen)
            _gdk_x11_screen_size_changed (screen, xevent);
	}
      else
#endif
#if defined(HAVE_XCOMPOSITE) && defined (HAVE_XDAMAGE) && defined (HAVE_XFIXES)
      if (display_x11->have_xdamage && window_private && window_private->composited &&
	  xevent->type == display_x11->xdamage_event_base + XDamageNotify &&
	  ((XDamageNotifyEvent *) xevent)->damage == window_impl->damage)
	{
	  XDamageNotifyEvent *damage_event = (XDamageNotifyEvent *) xevent;
	  XserverRegion repair;
	  GdkRectangle rect;

	  rect.x = window_private->x + damage_event->area.x;
	  rect.y = window_private->y + damage_event->area.y;
	  rect.width = damage_event->area.width;
	  rect.height = damage_event->area.height;

	  repair = XFixesCreateRegion (display_x11->xdisplay,
				       &damage_event->area, 1);
	  XDamageSubtract (display_x11->xdisplay,
			   window_impl->damage,
			   repair, None);
	  XFixesDestroyRegion (display_x11->xdisplay, repair);

	  if (window_private->parent != NULL)
	    _gdk_window_process_expose (GDK_WINDOW (window_private->parent),
					damage_event->serial, &rect);

	  return_val = TRUE;
	}
      else
#endif
	{
	  /* something else - (e.g., a Xinput event) */
	  
	  if (window_private &&
	      !GDK_WINDOW_DESTROYED (window_private) &&
	      window_private->input_window)
	    return_val = _gdk_input_other_event (event, xevent, window);
	  else
	    return_val = FALSE;
	  
	  break;
	}
    }

 done:
  if (return_val)
    {
      if (event->any.window)
	g_object_ref (event->any.window);
      if (((event->any.type == GDK_ENTER_NOTIFY) ||
	   (event->any.type == GDK_LEAVE_NOTIFY)) &&
	  (event->crossing.subwindow != NULL))
	g_object_ref (event->crossing.subwindow);
    }
  else
    {
      /* Mark this event as having no resources to be freed */
      event->any.window = NULL;
      event->any.type = GDK_NOTHING;
    }
  
  if (window)
    g_object_unref (window);

  return return_val;
}

static GdkFilterReturn
gdk_wm_protocols_filter (GdkXEvent *xev,
			 GdkEvent  *event,
			 gpointer data)
{
  XEvent *xevent = (XEvent *)xev;
  GdkWindow *win = event->any.window;
  GdkDisplay *display;
  Atom atom;

  if (!win)
      return GDK_FILTER_REMOVE;    

  display = GDK_WINDOW_DISPLAY (win);
  atom = (Atom)xevent->xclient.data.l[0];

  if (atom == gdk_x11_get_xatom_by_name_for_display (display, "WM_DELETE_WINDOW"))
    {
  /* The delete window request specifies a window
   *  to delete. We don't actually destroy the
   *  window because "it is only a request". (The
   *  window might contain vital data that the
   *  program does not want destroyed). Instead
   *  the event is passed along to the program,
   *  which should then destroy the window.
   */
      GDK_NOTE (EVENTS,
		g_message ("delete window:\t\twindow: %ld",
			   xevent->xclient.window));
      
      event->any.type = GDK_DELETE;

      gdk_x11_window_set_user_time (win, xevent->xclient.data.l[1]);

      return GDK_FILTER_TRANSLATE;
    }
  else if (atom == gdk_x11_get_xatom_by_name_for_display (display, "WM_TAKE_FOCUS"))
    {
      GdkToplevelX11 *toplevel = _gdk_x11_window_get_toplevel (event->any.window);
      GdkWindowObject *private = (GdkWindowObject *)win;

      /* There is no way of knowing reliably whether we are viewable;
       * _gdk_x11_set_input_focus_safe() traps errors asynchronously.
       */
      if (toplevel && private->accept_focus)
	_gdk_x11_set_input_focus_safe (display, toplevel->focus_window,
				       RevertToParent,
				       xevent->xclient.data.l[1]);

      return GDK_FILTER_REMOVE;
    }
  else if (atom == gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_PING") &&
	   !_gdk_x11_display_is_root_window (display,
					     xevent->xclient.window))
    {
      XClientMessageEvent xclient = xevent->xclient;
      
      xclient.window = GDK_WINDOW_XROOTWIN (win);
      XSendEvent (GDK_WINDOW_XDISPLAY (win), 
		  xclient.window,
		  False, 
		  SubstructureRedirectMask | SubstructureNotifyMask, (XEvent *)&xclient);

      return GDK_FILTER_REMOVE;
    }
  else if (atom == gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_SYNC_REQUEST") &&
	   GDK_DISPLAY_X11 (display)->use_sync)
    {
      GdkToplevelX11 *toplevel = _gdk_x11_window_get_toplevel (event->any.window);
      if (toplevel)
	{
#ifdef HAVE_XSYNC
	  XSyncIntsToValue (&toplevel->pending_counter_value, 
			    xevent->xclient.data.l[2], 
			    xevent->xclient.data.l[3]);
#endif
	}
      return GDK_FILTER_REMOVE;
    }
  
  return GDK_FILTER_CONTINUE;
}

void
_gdk_events_queue (GdkDisplay *display)
{
  GList *node;
  GdkEvent *event;
  XEvent xevent;
  Display *xdisplay = GDK_DISPLAY_XDISPLAY (display);

  while (!_gdk_event_queue_find_first(display) && XPending (xdisplay))
    {
      XNextEvent (xdisplay, &xevent);

      switch (xevent.type)
	{
	case KeyPress:
	case KeyRelease:
	  break;
	default:
	  if (XFilterEvent (&xevent, None))
	    continue;
	}
      
      event = gdk_event_new (GDK_NOTHING);
      
      event->any.window = NULL;
      event->any.send_event = xevent.xany.send_event ? TRUE : FALSE;

      ((GdkEventPrivate *)event)->flags |= GDK_EVENT_PENDING;

      node = _gdk_event_queue_append (display, event);

      if (gdk_event_translate (display, event, &xevent, FALSE))
	{
	  ((GdkEventPrivate *)event)->flags &= ~GDK_EVENT_PENDING;
          _gdk_windowing_got_event (display, node, event, xevent.xany.serial);
	}
      else
	{
	  _gdk_event_queue_remove_link (display, node);
	  g_list_free_1 (node);
	  gdk_event_free (event);
	}
    }
}

static gboolean  
gdk_event_prepare (GSource  *source,
		   gint     *timeout)
{
  GdkDisplay *display = ((GdkDisplaySource*)source)->display;
  gboolean retval;
  
  GDK_THREADS_ENTER ();

  *timeout = -1;
  retval = (_gdk_event_queue_find_first (display) != NULL || 
	    gdk_check_xpending (display));
  
  GDK_THREADS_LEAVE ();

  return retval;
}

static gboolean  
gdk_event_check (GSource *source) 
{
  GdkDisplaySource *display_source = (GdkDisplaySource*)source;
  gboolean retval;

  GDK_THREADS_ENTER ();

  if (display_source->event_poll_fd.revents & G_IO_IN)
    retval = (_gdk_event_queue_find_first (display_source->display) != NULL || 
	      gdk_check_xpending (display_source->display));
  else
    retval = FALSE;

  GDK_THREADS_LEAVE ();

  return retval;
}

static gboolean  
gdk_event_dispatch (GSource    *source,
		    GSourceFunc callback,
		    gpointer    user_data)
{
  GdkDisplay *display = ((GdkDisplaySource*)source)->display;
  GdkEvent *event;
 
  GDK_THREADS_ENTER ();

  _gdk_events_queue (display);
  event = _gdk_event_unqueue (display);

  if (event)
    {
      if (_gdk_event_func)
	(*_gdk_event_func) (event, _gdk_event_data);
      
      gdk_event_free (event);
    }
  
  GDK_THREADS_LEAVE ();

  return TRUE;
}

/**
 * gdk_event_send_client_message_for_display:
 * @display: the #GdkDisplay for the window where the message is to be sent.
 * @event: the #GdkEvent to send, which should be a #GdkEventClient.
 * @winid: the window to send the client message to.
 *
 * On X11, sends an X ClientMessage event to a given window. On
 * Windows, sends a message registered with the name
 * GDK_WIN32_CLIENT_MESSAGE.
 *
 * This could be used for communicating between different
 * applications, though the amount of data is limited to 20 bytes on
 * X11, and to just four bytes on Windows.
 *
 * Returns: non-zero on success.
 *
 * Since: 2.2
 */
gboolean
gdk_event_send_client_message_for_display (GdkDisplay     *display,
					   GdkEvent       *event,
					   GdkNativeWindow winid)
{
  XEvent sev;
  
  g_return_val_if_fail(event != NULL, FALSE);

  /* Set up our event to send, with the exception of its target window */
  sev.xclient.type = ClientMessage;
  sev.xclient.display = GDK_DISPLAY_XDISPLAY (display);
  sev.xclient.format = event->client.data_format;
  sev.xclient.window = winid;
  memcpy(&sev.xclient.data, &event->client.data, sizeof (sev.xclient.data));
  sev.xclient.message_type = gdk_x11_atom_to_xatom_for_display (display, event->client.message_type);
  
  return _gdk_send_xevent (display, winid, False, NoEventMask, &sev);
}



/* Sends a ClientMessage to all toplevel client windows */
static gboolean
gdk_event_send_client_message_to_all_recurse (GdkDisplay *display,
					      XEvent     *xev, 
					      guint32     xid,
					      guint       level)
{
  Atom type = None;
  int format;
  unsigned long nitems, after;
  unsigned char *data;
  Window *ret_children, ret_root, ret_parent;
  unsigned int ret_nchildren;
  gboolean send = FALSE;
  gboolean found = FALSE;
  gboolean result = FALSE;
  int i;
  
  gdk_error_trap_push ();
  
  if (XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display), xid, 
			  gdk_x11_get_xatom_by_name_for_display (display, "WM_STATE"),
			  0, 0, False, AnyPropertyType,
			  &type, &format, &nitems, &after, &data) != Success)
    goto out;
  
  if (type)
    {
      send = TRUE;
      XFree (data);
    }
  else
    {
      /* OK, we're all set, now let's find some windows to send this to */
      if (!XQueryTree (GDK_DISPLAY_XDISPLAY (display), xid,
		      &ret_root, &ret_parent,
		      &ret_children, &ret_nchildren))	
	goto out;

      for(i = 0; i < ret_nchildren; i++)
	if (gdk_event_send_client_message_to_all_recurse (display, xev, ret_children[i], level + 1))
	  found = TRUE;

      XFree (ret_children);
    }

  if (send || (!found && (level == 1)))
    {
      xev->xclient.window = xid;
      _gdk_send_xevent (display, xid, False, NoEventMask, xev);
    }

  result = send || found;

 out:
  gdk_error_trap_pop ();

  return result;
}

/**
 * gdk_screen_broadcast_client_message:
 * @screen: the #GdkScreen where the event will be broadcasted.
 * @event: the #GdkEvent.
 *
 * On X11, sends an X ClientMessage event to all toplevel windows on
 * @screen. 
 *
 * Toplevel windows are determined by checking for the WM_STATE property, 
 * as described in the Inter-Client Communication Conventions Manual (ICCCM).
 * If no windows are found with the WM_STATE property set, the message is 
 * sent to all children of the root window.
 *
 * On Windows, broadcasts a message registered with the name
 * GDK_WIN32_CLIENT_MESSAGE to all top-level windows. The amount of
 * data is limited to one long, i.e. four bytes.
 *
 * Since: 2.2
 */

void
gdk_screen_broadcast_client_message (GdkScreen *screen, 
				     GdkEvent  *event)
{
  XEvent sev;
  GdkWindow *root_window;

  g_return_if_fail (event != NULL);
  
  root_window = gdk_screen_get_root_window (screen);
  
  /* Set up our event to send, with the exception of its target window */
  sev.xclient.type = ClientMessage;
  sev.xclient.display = GDK_WINDOW_XDISPLAY (root_window);
  sev.xclient.format = event->client.data_format;
  memcpy(&sev.xclient.data, &event->client.data, sizeof (sev.xclient.data));
  sev.xclient.message_type = 
    gdk_x11_atom_to_xatom_for_display (GDK_WINDOW_DISPLAY (root_window),
				       event->client.message_type);

  gdk_event_send_client_message_to_all_recurse (gdk_screen_get_display (screen),
						&sev, 
						GDK_WINDOW_XID (root_window), 
						0);
}

/*
 *--------------------------------------------------------------
 * gdk_flush
 *
 *   Flushes the Xlib output buffer and then waits
 *   until all requests have been received and processed
 *   by the X server. The only real use for this function
 *   is in dealing with XShm.
 *
 * Arguments:
 *
 * Results:
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

void
gdk_flush (void)
{
  GSList *tmp_list = _gdk_displays;
  
  while (tmp_list)
    {
      XSync (GDK_DISPLAY_XDISPLAY (tmp_list->data), False);
      tmp_list = tmp_list->next;
    }
}

static Bool
timestamp_predicate (Display *display,
		     XEvent  *xevent,
		     XPointer arg)
{
  Window xwindow = GPOINTER_TO_UINT (arg);
  GdkDisplay *gdk_display = gdk_x11_lookup_xdisplay (display);

  if (xevent->type == PropertyNotify &&
      xevent->xproperty.window == xwindow &&
      xevent->xproperty.atom == gdk_x11_get_xatom_by_name_for_display (gdk_display,
								       "GDK_TIMESTAMP_PROP"))
    return True;

  return False;
}

/**
 * gdk_x11_get_server_time:
 * @window: a #GdkWindow, used for communication with the server.
 *          The window must have GDK_PROPERTY_CHANGE_MASK in its
 *          events mask or a hang will result.
 * 
 * Routine to get the current X server time stamp. 
 * 
 * Return value: the time stamp.
 **/
guint32
gdk_x11_get_server_time (GdkWindow *window)
{
  Display *xdisplay;
  Window   xwindow;
  guchar c = 'a';
  XEvent xevent;
  Atom timestamp_prop_atom;

  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);
  g_return_val_if_fail (!GDK_WINDOW_DESTROYED (window), 0);

  xdisplay = GDK_WINDOW_XDISPLAY (window);
  xwindow = GDK_WINDOW_XWINDOW (window);
  timestamp_prop_atom = 
    gdk_x11_get_xatom_by_name_for_display (GDK_WINDOW_DISPLAY (window),
					   "GDK_TIMESTAMP_PROP");
  
  XChangeProperty (xdisplay, xwindow, timestamp_prop_atom,
		   timestamp_prop_atom,
		   8, PropModeReplace, &c, 1);

  XIfEvent (xdisplay, &xevent,
	    timestamp_predicate, GUINT_TO_POINTER(xwindow));

  return xevent.xproperty.time;
}

static void
fetch_net_wm_check_window (GdkScreen *screen)
{
  GdkScreenX11 *screen_x11;
  GdkDisplay *display;
  Atom type;
  gint format;
  gulong n_items;
  gulong bytes_after;
  guchar *data;
  Window *xwindow;
  GTimeVal tv;
  gint error;

  screen_x11 = GDK_SCREEN_X11 (screen);
  display = screen_x11->display;

  g_return_if_fail (GDK_DISPLAY_X11 (display)->trusted_client);
  
  g_get_current_time (&tv);

  if (ABS  (tv.tv_sec - screen_x11->last_wmspec_check_time) < 15)
    return; /* we've checked recently */

  screen_x11->last_wmspec_check_time = tv.tv_sec;

  data = NULL;
  XGetWindowProperty (screen_x11->xdisplay, screen_x11->xroot_window,
		      gdk_x11_get_xatom_by_name_for_display (display, "_NET_SUPPORTING_WM_CHECK"),
		      0, G_MAXLONG, False, XA_WINDOW, &type, &format,
		      &n_items, &bytes_after, &data);
  
  if (type != XA_WINDOW)
    {
      if (data)
        XFree (data);
      return;
    }

  xwindow = (Window *)data;

  if (screen_x11->wmspec_check_window == *xwindow)
    {
      XFree (xwindow);
      return;
    }

  gdk_error_trap_push ();

  /* Find out if this WM goes away, so we can reset everything. */
  XSelectInput (screen_x11->xdisplay, *xwindow, StructureNotifyMask);
  gdk_display_sync (display);

  error = gdk_error_trap_pop ();
  if (!error)
    {
      screen_x11->wmspec_check_window = *xwindow;
      screen_x11->need_refetch_net_supported = TRUE;
      screen_x11->need_refetch_wm_name = TRUE;

      /* Careful, reentrancy */
      _gdk_x11_screen_window_manager_changed (GDK_SCREEN (screen_x11));
    }
  else if (error == BadWindow)
    {
      /* Leftover property, try again immediately, new wm may be starting up */
      screen_x11->last_wmspec_check_time = 0;
    }

  XFree (xwindow);
}

/**
 * gdk_x11_screen_get_window_manager_name:
 * @screen: a #GdkScreen 
 * 
 * Returns the name of the window manager for @screen. 
 * 
 * Return value: the name of the window manager screen @screen, or 
 * "unknown" if the window manager is unknown. The string is owned by GDK
 * and should not be freed.
 *
 * Since: 2.2
 **/
const char*
gdk_x11_screen_get_window_manager_name (GdkScreen *screen)
{
  GdkScreenX11 *screen_x11;

  screen_x11 = GDK_SCREEN_X11 (screen);
  
  if (!G_LIKELY (GDK_DISPLAY_X11 (screen_x11->display)->trusted_client))
    return screen_x11->window_manager_name;

  fetch_net_wm_check_window (screen);

  if (screen_x11->need_refetch_wm_name)
    {
      /* Get the name of the window manager */
      screen_x11->need_refetch_wm_name = FALSE;

      g_free (screen_x11->window_manager_name);
      screen_x11->window_manager_name = g_strdup ("unknown");
      
      if (screen_x11->wmspec_check_window != None)
        {
          Atom type;
          gint format;
          gulong n_items;
          gulong bytes_after;
          gchar *name;
          
          name = NULL;

	  gdk_error_trap_push ();
          
          XGetWindowProperty (GDK_DISPLAY_XDISPLAY (screen_x11->display),
                              screen_x11->wmspec_check_window,
                              gdk_x11_get_xatom_by_name_for_display (screen_x11->display,
                                                                     "_NET_WM_NAME"),
                              0, G_MAXLONG, False,
                              gdk_x11_get_xatom_by_name_for_display (screen_x11->display,
                                                                     "UTF8_STRING"),
                              &type, &format, 
                              &n_items, &bytes_after,
                              (guchar **)&name);
          
          gdk_display_sync (screen_x11->display);
          
          gdk_error_trap_pop ();
          
          if (name != NULL)
            {
              g_free (screen_x11->window_manager_name);
              screen_x11->window_manager_name = g_strdup (name);
              XFree (name);
            }
        }
    }
  
  return GDK_SCREEN_X11 (screen)->window_manager_name;
}

typedef struct _NetWmSupportedAtoms NetWmSupportedAtoms;

struct _NetWmSupportedAtoms
{
  Atom *atoms;
  gulong n_atoms;
};

static void
cleanup_atoms(gpointer data)
{
  NetWmSupportedAtoms *supported_atoms = data;
  if (supported_atoms->atoms)
      XFree (supported_atoms->atoms);
  g_free (supported_atoms);
}

/**
 * gdk_x11_screen_supports_net_wm_hint:
 * @screen: the relevant #GdkScreen.
 * @property: a property atom.
 * 
 * This function is specific to the X11 backend of GDK, and indicates
 * whether the window manager supports a certain hint from the
 * Extended Window Manager Hints Specification. You can find this
 * specification on 
 * <ulink url="http://www.freedesktop.org">http://www.freedesktop.org</ulink>.
 *
 * When using this function, keep in mind that the window manager
 * can change over time; so you shouldn't use this function in
 * a way that impacts persistent application state. A common bug
 * is that your application can start up before the window manager
 * does when the user logs in, and before the window manager starts
 * gdk_x11_screen_supports_net_wm_hint() will return %FALSE for every property.
 * You can monitor the window_manager_changed signal on #GdkScreen to detect
 * a window manager change.
 * 
 * Return value: %TRUE if the window manager supports @property
 *
 * Since: 2.2
 **/
gboolean
gdk_x11_screen_supports_net_wm_hint (GdkScreen *screen,
				     GdkAtom    property)
{
  gulong i;
  GdkScreenX11 *screen_x11;
  NetWmSupportedAtoms *supported_atoms;
  GdkDisplay *display;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), FALSE);
  
  screen_x11 = GDK_SCREEN_X11 (screen);
  display = screen_x11->display;

  if (!G_LIKELY (GDK_DISPLAY_X11 (display)->trusted_client))
    return FALSE;

  supported_atoms = g_object_get_data (G_OBJECT (screen), "gdk-net-wm-supported-atoms");
  if (!supported_atoms)
    {
      supported_atoms = g_new0 (NetWmSupportedAtoms, 1);
      g_object_set_data_full (G_OBJECT (screen), "gdk-net-wm-supported-atoms", supported_atoms, cleanup_atoms);
    }

  fetch_net_wm_check_window (screen);

  if (screen_x11->wmspec_check_window == None)
    return FALSE;
  
  if (screen_x11->need_refetch_net_supported)
    {
      /* WM has changed since we last got the supported list,
       * refetch it.
       */
      Atom type;
      gint format;
      gulong bytes_after;

      screen_x11->need_refetch_net_supported = FALSE;
      
      if (supported_atoms->atoms)
        XFree (supported_atoms->atoms);
      
      supported_atoms->atoms = NULL;
      supported_atoms->n_atoms = 0;
      
      XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display), screen_x11->xroot_window,
                          gdk_x11_get_xatom_by_name_for_display (display, "_NET_SUPPORTED"),
                          0, G_MAXLONG, False, XA_ATOM, &type, &format, 
                          &supported_atoms->n_atoms, &bytes_after,
                          (guchar **)&supported_atoms->atoms);
      
      if (type != XA_ATOM)
        return FALSE;
    }
  
  if (supported_atoms->atoms == NULL)
    return FALSE;
  
  i = 0;
  while (i < supported_atoms->n_atoms)
    {
      if (supported_atoms->atoms[i] == gdk_x11_atom_to_xatom_for_display (display, property))
        return TRUE;
      
      ++i;
    }
  
  return FALSE;
}

/**
 * gdk_net_wm_supports:
 * @property: a property atom.
 * 
 * This function is specific to the X11 backend of GDK, and indicates
 * whether the window manager for the default screen supports a certain
 * hint from the Extended Window Manager Hints Specification. See
 * gdk_x11_screen_supports_net_wm_hint() for complete details.
 * 
 * Return value: %TRUE if the window manager supports @property
 *
 * Deprecated:2.24: Use gdk_x11_screen_supports_net_wm_hint() instead
 **/
gboolean
gdk_net_wm_supports (GdkAtom property)
{
  return gdk_x11_screen_supports_net_wm_hint (gdk_screen_get_default (), property);
}


static void
gdk_xsettings_notify_cb (const char       *name,
			 XSettingsAction   action,
			 XSettingsSetting *setting,
			 void             *data)
{
  GdkEvent new_event;
  GdkScreen *screen = data;
  GdkScreenX11 *screen_x11 = data;
  int i;

  if (screen_x11->xsettings_in_init)
    return;
  
  new_event.type = GDK_SETTING;
  new_event.setting.window = gdk_screen_get_root_window (screen);
  new_event.setting.send_event = FALSE;
  new_event.setting.name = NULL;

  for (i = 0; i < GDK_SETTINGS_N_ELEMENTS() ; i++)
    if (strcmp (GDK_SETTINGS_X_NAME (i), name) == 0)
      {
	new_event.setting.name = (char*) GDK_SETTINGS_GDK_NAME (i);
	break;
      }
  
  if (!new_event.setting.name)
    return;
  
  switch (action)
    {
    case XSETTINGS_ACTION_NEW:
      new_event.setting.action = GDK_SETTING_ACTION_NEW;
      break;
    case XSETTINGS_ACTION_CHANGED:
      new_event.setting.action = GDK_SETTING_ACTION_CHANGED;
      break;
    case XSETTINGS_ACTION_DELETED:
      new_event.setting.action = GDK_SETTING_ACTION_DELETED;
      break;
    }

  gdk_event_put (&new_event);
}

static gboolean
check_transform (const gchar *xsettings_name,
		 GType        src_type,
		 GType        dest_type)
{
  if (!g_value_type_transformable (src_type, dest_type))
    {
      g_warning ("Cannot transform xsetting %s of type %s to type %s\n",
		 xsettings_name,
		 g_type_name (src_type),
		 g_type_name (dest_type));
      return FALSE;
    }
  else
    return TRUE;
}

/**
 * gdk_screen_get_setting:
 * @screen: the #GdkScreen where the setting is located
 * @name: the name of the setting
 * @value: location to store the value of the setting
 *
 * Retrieves a desktop-wide setting such as double-click time
 * for the #GdkScreen @screen. 
 *
 * FIXME needs a list of valid settings here, or a link to 
 * more information.
 * 
 * Returns: %TRUE if the setting existed and a value was stored
 *   in @value, %FALSE otherwise.
 *
 * Since: 2.2
 **/
gboolean
gdk_screen_get_setting (GdkScreen   *screen,
			const gchar *name,
			GValue      *value)
{

  const char *xsettings_name = NULL;
  XSettingsResult result;
  XSettingsSetting *setting = NULL;
  GdkScreenX11 *screen_x11;
  gboolean success = FALSE;
  gint i;
  GValue tmp_val = { 0, };
  
  g_return_val_if_fail (GDK_IS_SCREEN (screen), FALSE);
  
  screen_x11 = GDK_SCREEN_X11 (screen);

  for (i = 0; i < GDK_SETTINGS_N_ELEMENTS(); i++)
    if (strcmp (GDK_SETTINGS_GDK_NAME (i), name) == 0)
      {
	xsettings_name = GDK_SETTINGS_X_NAME (i);
	break;
      }

  if (!xsettings_name)
    goto out;

  result = xsettings_client_get_setting (screen_x11->xsettings_client, 
					 xsettings_name, &setting);
  if (result != XSETTINGS_SUCCESS)
    goto out;

  switch (setting->type)
    {
    case XSETTINGS_TYPE_INT:
      if (check_transform (xsettings_name, G_TYPE_INT, G_VALUE_TYPE (value)))
	{
	  g_value_init (&tmp_val, G_TYPE_INT);
	  g_value_set_int (&tmp_val, setting->data.v_int);
	  g_value_transform (&tmp_val, value);

	  success = TRUE;
	}
      break;
    case XSETTINGS_TYPE_STRING:
      if (check_transform (xsettings_name, G_TYPE_STRING, G_VALUE_TYPE (value)))
	{
	  g_value_init (&tmp_val, G_TYPE_STRING);
	  g_value_set_string (&tmp_val, setting->data.v_string);
	  g_value_transform (&tmp_val, value);

	  success = TRUE;
	}
      break;
    case XSETTINGS_TYPE_COLOR:
      if (!check_transform (xsettings_name, GDK_TYPE_COLOR, G_VALUE_TYPE (value)))
	{
	  GdkColor color;
	  
	  g_value_init (&tmp_val, GDK_TYPE_COLOR);

	  color.pixel = 0;
	  color.red = setting->data.v_color.red;
	  color.green = setting->data.v_color.green;
	  color.blue = setting->data.v_color.blue;
	  
	  g_value_set_boxed (&tmp_val, &color);
	  
	  g_value_transform (&tmp_val, value);
	  
	  success = TRUE;
	}
      break;
    }
  
  g_value_unset (&tmp_val);

 out:
  if (setting)
    xsettings_setting_free (setting);

  if (success)
    return TRUE;
  else
    return _gdk_x11_get_xft_setting (screen, name, value);
}

static GdkFilterReturn 
gdk_xsettings_client_event_filter (GdkXEvent *xevent,
				   GdkEvent  *event,
				   gpointer   data)
{
  GdkScreenX11 *screen = data;
  
  if (xsettings_client_process_event (screen->xsettings_client, (XEvent *)xevent))
    return GDK_FILTER_REMOVE;
  else
    return GDK_FILTER_CONTINUE;
}

static Bool
gdk_xsettings_watch_cb (Window   window,
			Bool	 is_start,
			long     mask,
			void    *cb_data)
{
  GdkWindow *gdkwin;
  GdkScreen *screen = cb_data;

  gdkwin = gdk_window_lookup_for_display (gdk_screen_get_display (screen), window);

  if (is_start)
    {
      if (gdkwin)
	g_object_ref (gdkwin);
      else
	{
	  gdkwin = gdk_window_foreign_new_for_display (gdk_screen_get_display (screen), window);
	  
	  /* gdk_window_foreign_new_for_display() can fail and return NULL if the
	   * window has already been destroyed.
	   */
	  if (!gdkwin)
	    return False;
	}

      gdk_window_add_filter (gdkwin, gdk_xsettings_client_event_filter, screen);
    }
  else
    {
      if (!gdkwin)
	{
	  /* gdkwin should not be NULL here, since if starting the watch succeeded
	   * we have a reference on the window. It might mean that the caller didn't
	   * remove the watch when it got a DestroyNotify event. Or maybe the
	   * caller ignored the return value when starting the watch failed.
	   */
	  g_warning ("gdk_xsettings_watch_cb(): Couldn't find window to unwatch");
	  return False;
	}
      
      gdk_window_remove_filter (gdkwin, gdk_xsettings_client_event_filter, screen);
      g_object_unref (gdkwin);
    }

  return True;
}

void
_gdk_windowing_event_data_copy (const GdkEvent *src,
                                GdkEvent       *dst)
{
}

void
_gdk_windowing_event_data_free (GdkEvent *event)
{
}

#define __GDK_EVENTS_X11_C__
#include "gdkaliasdef.c"
