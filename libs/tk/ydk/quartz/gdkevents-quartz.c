/* gdkevents-quartz.c
 *
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998-2002 Tor Lillqvist
 * Copyright (C) 2005-2008 Imendio AB
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include <sys/types.h>
#include <sys/sysctl.h>
#include <pthread.h>
#include <unistd.h>

#import <Cocoa/Cocoa.h>
#include <Carbon/Carbon.h>

#include "gdkscreen.h"
#include "gdkkeysyms.h"
#include "gdkprivate-quartz.h"
#include "gdkinputprivate.h"

#define GRIP_WIDTH 15
#define GRIP_HEIGHT 15
#define GDK_LION_RESIZE 5

#define WINDOW_IS_TOPLEVEL(window)                      \
    (GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD &&    \
     GDK_WINDOW_TYPE (window) != GDK_WINDOW_FOREIGN &&  \
     GDK_WINDOW_TYPE (window) != GDK_WINDOW_OFFSCREEN)

/* This is the NSView not owned by GDK where a mouse down event occurs */
static NSView *foreign_mouse_down_view;

/* This is the window corresponding to the key window */
static GdkWindow   *current_keyboard_window;

/* This is the event mask from the last event */
static GdkEventMask current_event_mask;

static void append_event                        (GdkEvent  *event,
                                                 gboolean   windowing);

static GdkWindow *find_toplevel_under_pointer   (GdkDisplay *display,
                                                 NSPoint     screen_point,
                                                 gint       *x,
                                                 gint       *y);


NSEvent *
gdk_quartz_event_get_nsevent (GdkEvent *event)
{
  /* FIXME: If the event here is unallocated, we crash. */
  return ((GdkEventPrivate *) event)->windowing_data;
}

static void
gdk_quartz_ns_notification_callback (CFNotificationCenterRef  center,
                                     void                    *observer,
                                     CFStringRef              name,
                                     const void              *object,
                                     CFDictionaryRef          userInfo)
{
  GdkEvent new_event;

  new_event.type = GDK_SETTING;
  new_event.setting.window = gdk_screen_get_root_window (_gdk_screen);
  new_event.setting.send_event = FALSE;
  new_event.setting.action = GDK_SETTING_ACTION_CHANGED;
  new_event.setting.name = NULL;

  /* Translate name */
  if (CFStringCompare (name,
                       CFSTR("AppleNoRedisplayAppearancePreferenceChanged"),
                       0) == kCFCompareEqualTo)
    new_event.setting.name = "gtk-primary-button-warps-slider";

  if (!new_event.setting.name)
    return;

  gdk_event_put (&new_event);
}

static void
gdk_quartz_events_init_notifications (void)
{
  static gboolean notifications_initialized = FALSE;

  if (notifications_initialized)
    return;
  notifications_initialized = TRUE;

  /* Initialize any handlers for notifications we want to push to GTK
   * through GdkEventSettings.
   */

  /* This is an undocumented *distributed* notification to listen for changes
   * in scrollbar jump behavior. It is used by LibreOffice and WebKit as well.
   */
  CFNotificationCenterAddObserver (CFNotificationCenterGetDistributedCenter (),
                                   NULL,
                                   &gdk_quartz_ns_notification_callback,
                                   CFSTR ("AppleNoRedisplayAppearancePreferenceChanged"),
                                   NULL,
                                   CFNotificationSuspensionBehaviorDeliverImmediately);
}

void
_gdk_events_init (void)
{
  _gdk_quartz_event_loop_init ();
  gdk_quartz_events_init_notifications ();

  current_keyboard_window = g_object_ref (_gdk_root);
}

gboolean
gdk_events_pending (void)
{
  return (_gdk_event_queue_find_first (_gdk_display) ||
	  (_gdk_quartz_event_loop_check_pending ()));
}

GdkEvent*
gdk_event_get_graphics_expose (GdkWindow *window)
{
  /* FIXME: Implement */
  return NULL;
}

GdkGrabStatus
gdk_keyboard_grab (GdkWindow  *window,
		   gint        owner_events,
		   guint32     time)
{
  GdkDisplay *display;
  GdkWindow  *toplevel;

  g_return_val_if_fail (window != NULL, 0);
  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);

  display = gdk_drawable_get_display (window);
  toplevel = gdk_window_get_effective_toplevel (window);

  _gdk_display_set_has_keyboard_grab (display,
                                      window,
                                      toplevel,
                                      owner_events,
                                      0,
                                      time);

  return GDK_GRAB_SUCCESS;
}

void
gdk_display_keyboard_ungrab (GdkDisplay *display,
			     guint32     time)
{
  _gdk_display_unset_has_keyboard_grab (display, FALSE);
}

void
gdk_display_pointer_ungrab (GdkDisplay *display,
			    guint32     time)
{
  GdkPointerGrabInfo *grab;

  grab = _gdk_display_get_last_pointer_grab (display);
  if (grab)
    grab->serial_end = 0;

  _gdk_display_pointer_grab_update (display, 0);
}

GdkGrabStatus
_gdk_windowing_pointer_grab (GdkWindow    *window,
                             GdkWindow    *native,
                             gboolean	   owner_events,
                             GdkEventMask  event_mask,
                             GdkWindow    *confine_to,
                             GdkCursor    *cursor,
                             guint32       time)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);
  g_return_val_if_fail (confine_to == NULL || GDK_IS_WINDOW (confine_to), 0);

  _gdk_display_add_pointer_grab (_gdk_display,
                                 window,
                                 native,
                                 owner_events,
                                 event_mask,
                                 0,
                                 time,
                                 FALSE);

  return GDK_GRAB_SUCCESS;
}

void
_gdk_quartz_events_break_all_grabs (guint32 time)
{
  GdkPointerGrabInfo *grab;

  if (_gdk_display->keyboard_grab.window)
    _gdk_display_unset_has_keyboard_grab (_gdk_display, FALSE);

  grab = _gdk_display_get_last_pointer_grab (_gdk_display);
  if (grab)
    {
      grab->serial_end = 0;
      grab->implicit_ungrab = TRUE;
    }

  _gdk_display_pointer_grab_update (_gdk_display, 0);
}

static void
fixup_event (GdkEvent *event)
{
  if (event->any.window)
    g_object_ref (event->any.window);
  if (((event->any.type == GDK_ENTER_NOTIFY) ||
       (event->any.type == GDK_LEAVE_NOTIFY)) &&
      (event->crossing.subwindow != NULL))
    g_object_ref (event->crossing.subwindow);
  event->any.send_event = FALSE;
}

static void
append_event (GdkEvent *event,
              gboolean  windowing)
{
  GList *node;

  fixup_event (event);
  node = _gdk_event_queue_append (_gdk_display, event);

  if (windowing)
    _gdk_windowing_got_event (_gdk_display, node, event, 0);
}

static gint
gdk_event_apply_filters (NSEvent *nsevent,
			 GdkEvent *event,
			 GList **filters)
{
  GList *tmp_list;
  GdkFilterReturn result;
  
  tmp_list = *filters;

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
      result = filter->function (nsevent, event, filter->data);

      /* get the next node after running the function since the
         function may add or remove a next node */
      node = tmp_list;
      tmp_list = tmp_list->next;

      filter->ref_count--;
      if (filter->ref_count == 0)
        {
          *filters = g_list_remove_link (*filters, node);
          g_list_free_1 (node);
          g_free (filter);
        }

      if (result != GDK_FILTER_CONTINUE)
        return result;
    }

  return GDK_FILTER_CONTINUE;
}

static guint32
get_time_from_ns_event (NSEvent *event)
{
  double time = [event timestamp];

  /* cast via double->uint64 conversion to make sure that it is
   * wrapped on 32-bit machines when it overflows
   */
  return (guint32) (guint64) (time * 1000.0);
}

static int
get_mouse_button_from_ns_event (NSEvent *event)
{
  NSInteger button;

  button = [event buttonNumber];

  switch (button)
    {
    case 0:
      return 1;
    case 1:
      return 3;
    case 2:
      return 2;
    default:
      return button + 1;
    }
}

static GdkModifierType
get_mouse_button_modifiers_from_ns_buttons (NSUInteger nsbuttons)
{
  GdkModifierType modifiers = 0;

  if (nsbuttons & (1 << 0))
    modifiers |= GDK_BUTTON1_MASK;
  if (nsbuttons & (1 << 1))
    modifiers |= GDK_BUTTON3_MASK;
  if (nsbuttons & (1 << 2))
    modifiers |= GDK_BUTTON2_MASK;
  if (nsbuttons & (1 << 3))
    modifiers |= GDK_BUTTON4_MASK;
  if (nsbuttons & (1 << 4))
    modifiers |= GDK_BUTTON5_MASK;

  return modifiers;
}

static GdkModifierType
get_mouse_button_modifiers_from_ns_event (NSEvent *event)
{
  int button;
  GdkModifierType state = 0;

  /* This maps buttons 1 to 5 to GDK_BUTTON[1-5]_MASK */
  button = get_mouse_button_from_ns_event (event);
  if (button >= 1 && button <= 5)
    state = (1 << (button + 7));

  return state;
}

static GdkModifierType
get_keyboard_modifiers_from_ns_flags (NSUInteger nsflags)
{
  GdkModifierType modifiers = 0;

  if (nsflags & NSAlphaShiftKeyMask)
    modifiers |= GDK_LOCK_MASK;
  if (nsflags & NSShiftKeyMask)
    modifiers |= GDK_SHIFT_MASK;
  if (nsflags & NSControlKeyMask)
    modifiers |= GDK_CONTROL_MASK;
  if (nsflags & NSAlternateKeyMask)
    modifiers |= GDK_MOD1_MASK;
  if (nsflags & NSCommandKeyMask)
    modifiers |= GDK_MOD2_MASK;

  return modifiers;
}

static GdkModifierType
get_keyboard_modifiers_from_ns_event (NSEvent *nsevent)
{
  return get_keyboard_modifiers_from_ns_flags ([nsevent modifierFlags]);
}

/* Return an event mask from an NSEvent */
static GdkEventMask
get_event_mask_from_ns_event (NSEvent *nsevent)
{
  switch ([nsevent type])
    {
    case NSLeftMouseDown:
    case NSRightMouseDown:
    case NSOtherMouseDown:
      return GDK_BUTTON_PRESS_MASK;
    case NSLeftMouseUp:
    case NSRightMouseUp:
    case NSOtherMouseUp:
      return GDK_BUTTON_RELEASE_MASK;
    case NSMouseMoved:
      return GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK;
    case NSScrollWheel:
      /* Since applications that want button press events can get
       * scroll events on X11 (since scroll wheel events are really
       * button press events there), we need to use GDK_BUTTON_PRESS_MASK too.
       */
      return GDK_SCROLL_MASK | GDK_BUTTON_PRESS_MASK;
    case NSLeftMouseDragged:
      return (GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK |
	      GDK_BUTTON_MOTION_MASK | GDK_BUTTON1_MOTION_MASK | 
	      GDK_BUTTON1_MASK);
    case NSRightMouseDragged:
      return (GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK |
	      GDK_BUTTON_MOTION_MASK | GDK_BUTTON3_MOTION_MASK | 
	      GDK_BUTTON3_MASK);
    case NSOtherMouseDragged:
      {
	GdkEventMask mask;

	mask = (GDK_POINTER_MOTION_MASK |
		GDK_POINTER_MOTION_HINT_MASK |
		GDK_BUTTON_MOTION_MASK);

	if (get_mouse_button_from_ns_event (nsevent) == 2)
	  mask |= (GDK_BUTTON2_MOTION_MASK | GDK_BUTTON2_MOTION_MASK | 
		   GDK_BUTTON2_MASK);

	return mask;
      }
    case NSKeyDown:
    case NSKeyUp:
    case NSFlagsChanged:
      {
        switch (_gdk_quartz_keys_event_type (nsevent))
	  {
	  case GDK_KEY_PRESS:
	    return GDK_KEY_PRESS_MASK;
	  case GDK_KEY_RELEASE:
	    return GDK_KEY_RELEASE_MASK;
	  case GDK_NOTHING:
	    return 0;
	  default:
	    g_assert_not_reached ();
	  }
      }
      break;

    case NSMouseEntered:
      return GDK_ENTER_NOTIFY_MASK;

    case NSMouseExited:
      return GDK_LEAVE_NOTIFY_MASK;

    default:
      g_assert_not_reached ();
    }

  return 0;
}

static void
get_window_point_from_screen_point (GdkWindow *window,
                                    NSPoint    screen_point,
                                    gint      *x,
                                    gint      *y)
{
  NSPoint point;
  NSWindow *nswindow;
  GdkWindowObject *private;

  private = (GdkWindowObject *)window;
  nswindow = ((GdkWindowImplQuartz *)private->impl)->toplevel;

  point = [nswindow convertScreenToBase:screen_point];

  *x = point.x;
  *y = private->height - point.y;
}

static gboolean
is_mouse_button_press_event (NSEventType type)
{
  switch (type)
    {
      case NSLeftMouseDown:
      case NSRightMouseDown:
      case NSOtherMouseDown:
        return TRUE;
    }

  return FALSE;
}

static GdkWindow *
get_toplevel_from_ns_event (NSEvent *nsevent,
                            NSPoint *screen_point,
                            gint    *x,
                            gint    *y)
{
  GdkWindow *toplevel = NULL;

  if ([nsevent window])
    {
      GdkQuartzView *view;
      GdkWindowObject *private;
      NSPoint point, view_point;
      NSRect view_frame;

      view = (GdkQuartzView *)[[nsevent window] contentView];

      toplevel = [view gdkWindow];
      private = GDK_WINDOW_OBJECT (toplevel);

      point = [nsevent locationInWindow];
      view_point = [view convertPoint:point fromView:nil];
      view_frame = [view frame];

      /* NSEvents come in with a window set, but with window coordinates
       * out of window bounds. For e.g. moved events this is fine, we use
       * this information to properly handle enter/leave notify and motion
       * events. For mouse button press/release, we want to avoid forwarding
       * these events however, because the window they relate to is not the
       * window set in the event. This situation appears to occur when button
       * presses come in just before (or just after?) a window is resized and
       * also when a button press occurs on the OS X window titlebar.
       *
       * By setting toplevel to NULL, we do another attempt to get the right
       * toplevel window below.
       */
      if (is_mouse_button_press_event ([nsevent type]) &&
          (view_point.x < view_frame.origin.x ||
           view_point.x >= view_frame.origin.x + view_frame.size.width ||
           view_point.y < view_frame.origin.y ||
           view_point.y >= view_frame.origin.y + view_frame.size.height))
        {
          toplevel = NULL;

          /* This is a hack for button presses to break all grabs. E.g. if
           * a menu is open and one clicks on the title bar (or anywhere
           * out of window bounds), we really want to pop down the menu (by
           * breaking the grabs) before OS X handles the action of the title
           * bar button.
           *
           * Because we cannot ingest this event into GDK, we have to do it
           * here, not very nice.
           */
          _gdk_quartz_events_break_all_grabs (get_time_from_ns_event (nsevent));
        }
      else
        {
          *screen_point = [[nsevent window] convertBaseToScreen:point];

          *x = point.x;
          *y = private->height - point.y;
        }
    }

  if (!toplevel)
    {
      /* Fallback used when no NSWindow set.  This happens e.g. when
       * we allow motion events without a window set in gdk_event_translate()
       * that occur immediately after the main menu bar was clicked/used.
       * This fallback will not return coordinates contained in a window's
       * titlebar.
       */
      *screen_point = [NSEvent mouseLocation];
      toplevel = find_toplevel_under_pointer (_gdk_display,
                                              *screen_point,
                                              x, y);
    }

  return toplevel;
}

static GdkEvent *
create_focus_event (GdkWindow *window,
		    gboolean   in)
{
  GdkEvent *event;

  event = gdk_event_new (GDK_FOCUS_CHANGE);
  event->focus_change.window = window;
  event->focus_change.in = in;

  return event;
}


static void
generate_motion_event (GdkWindow *window)
{
  NSPoint screen_point;
  GdkEvent *event;
  gint x, y, x_root, y_root;

  event = gdk_event_new (GDK_MOTION_NOTIFY);
  event->any.window = NULL;
  event->any.send_event = TRUE;

  screen_point = [NSEvent mouseLocation];

  _gdk_quartz_window_nspoint_to_gdk_xy (screen_point, &x_root, &y_root);
  get_window_point_from_screen_point (window, screen_point, &x, &y);

  event->any.type = GDK_MOTION_NOTIFY;
  event->motion.window = window;
  event->motion.time = get_time_from_ns_event ([NSApp currentEvent]);
  event->motion.x = x;
  event->motion.y = y;
  event->motion.x_root = x_root;
  event->motion.y_root = y_root;
  /* FIXME event->axes */
  event->motion.state = _gdk_quartz_events_get_current_keyboard_modifiers () |
                        _gdk_quartz_events_get_current_mouse_modifiers ();
  event->motion.is_hint = FALSE;
  event->motion.device = _gdk_display->core_pointer;

  append_event (event, TRUE);
}

/* Note: Used to both set a new focus window and to unset the old one. */
void
_gdk_quartz_events_update_focus_window (GdkWindow *window,
					gboolean   got_focus)
{
  GdkEvent *event;

  if (got_focus && window == current_keyboard_window)
    return;

  /* FIXME: Don't do this when grabbed? Or make GdkQuartzWindow
   * disallow it in the first place instead?
   */
  
  if (!got_focus && window == current_keyboard_window)
    {
      event = create_focus_event (current_keyboard_window, FALSE);
      append_event (event, FALSE);
      g_object_unref (current_keyboard_window);
      current_keyboard_window = NULL;
    }

  if (got_focus)
    {
      if (current_keyboard_window)
	{
	  event = create_focus_event (current_keyboard_window, FALSE);
	  append_event (event, FALSE);
	  g_object_unref (current_keyboard_window);
	  current_keyboard_window = NULL;
	}
      
      event = create_focus_event (window, TRUE);
      append_event (event, FALSE);
      current_keyboard_window = g_object_ref (window);

      /* We just became the active window.  Unlike X11, Mac OS X does
       * not send us motion events while the window does not have focus
       * ("is not key").  We send a dummy motion notify event now, so that
       * everything in the window is set to correct state.
       */
      generate_motion_event (window);
    }
}

void
_gdk_quartz_events_send_map_event (GdkWindow *window)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);

  if (!impl->toplevel)
    return;

  if (private->event_mask & GDK_STRUCTURE_MASK)
    {
      GdkEvent event;

      event.any.type = GDK_MAP;
      event.any.window = window;
  
      gdk_event_put (&event);
    }
}

static GdkWindow *
find_toplevel_under_pointer (GdkDisplay *display,
                             NSPoint     screen_point,
                             gint       *x,
                             gint       *y)
{
  GdkWindow *toplevel;

  toplevel = display->pointer_info.toplevel_under_pointer;
  if (toplevel && WINDOW_IS_TOPLEVEL (toplevel))
    get_window_point_from_screen_point (toplevel, screen_point, x, y);

  if (toplevel)
    {
      /* If the coordinates are out of window bounds, this toplevel is not
       * under the pointer and we thus return NULL. This can occur when
       * toplevel under pointer has not yet been updated due to a very recent
       * window resize. Alternatively, we should no longer be relying on
       * the toplevel_under_pointer value which is maintained in gdkwindow.c.
       */
      GdkWindowObject *private = GDK_WINDOW_OBJECT (toplevel);
      if (*x < 0 || *y < 0 || *x >= private->width || *y >= private->height)
        return NULL;
    }

  return toplevel;
}

/* This function finds the correct window to send an event to, taking
 * into account grabs, event propagation, and event masks.
 */
static GdkWindow *
find_window_for_ns_event (NSEvent *nsevent, 
                          gint    *x, 
                          gint    *y,
                          gint    *x_root,
                          gint    *y_root)
{
  GdkQuartzView *view;
  GdkWindow *toplevel;
  NSPoint screen_point;
  NSEventType event_type;

  event_type = [nsevent type];

  if (foreign_mouse_down_view) {
	  switch (event_type) {
	  case NSLeftMouseUp:
	  case NSRightMouseUp:
	  case NSOtherMouseUp:
		  /* mouse up happened, foreign view needs to handle it
		     but we will also assume that it does (e.g. ends
		     a drag and whatever goes with it) and so we reset
		     foreign_mouse_down_view.
		  */
		  foreign_mouse_down_view = 0;
		  return NULL;

	  default:
		  /* foreign view needs to handle this */
		  return NULL;
	  }
  }

  view = (GdkQuartzView *)[[nsevent window] contentView];

  toplevel = get_toplevel_from_ns_event (nsevent, &screen_point, x, y);
  if (!toplevel) 
     return NULL;

  _gdk_quartz_window_nspoint_to_gdk_xy (screen_point, x_root, y_root);


  switch (event_type)
    {
    case NSLeftMouseDown:
    case NSRightMouseDown:
    case NSOtherMouseDown:
    case NSLeftMouseUp:
    case NSRightMouseUp:
    case NSOtherMouseUp:
    case NSMouseMoved:
    case NSScrollWheel:
    case NSLeftMouseDragged:
    case NSRightMouseDragged:
    case NSOtherMouseDragged:
      {
	GdkDisplay *display;
        GdkPointerGrabInfo *grab;

        display = gdk_drawable_get_display (toplevel);

	/* From the docs for XGrabPointer:
	 *
	 * If owner_events is True and if a generated pointer event
	 * would normally be reported to this client, it is reported
	 * as usual. Otherwise, the event is reported with respect to
	 * the grab_window and is reported only if selected by
	 * event_mask. For either value of owner_events, unreported
	 * events are discarded.
	 */
        grab = _gdk_display_get_last_pointer_grab (display);
	if (WINDOW_IS_TOPLEVEL (toplevel) && grab)
	  {
            /* Implicit grabs do not go through XGrabPointer and thus the
             * event mask should not be checked.
             */
	    if (!grab->implicit
	        && (grab->event_mask & get_event_mask_from_ns_event (nsevent)) == 0)
		 return NULL;

	    if (grab->owner_events)
              {
                /* For owner events, we need to use the toplevel under the
                 * pointer, not the window from the NSEvent, since that is
                 * reported with respect to the key window, which could be
                 * wrong.
                 */
                GdkWindow *toplevel_under_pointer;
                gint x_tmp, y_tmp;

                toplevel_under_pointer = find_toplevel_under_pointer (display,
                                                                      screen_point,
                                                                      &x_tmp, &y_tmp);
                if (toplevel_under_pointer)
                  {
                    toplevel = toplevel_under_pointer;
                    *x = x_tmp;
                    *y = y_tmp;
                  }

                return toplevel;
              }
            else
              {
                /* Finally check the grab window. */
		GdkWindow *grab_toplevel;

		grab_toplevel = gdk_window_get_effective_toplevel (grab->window);
                get_window_point_from_screen_point (grab_toplevel,
                                                    screen_point, x, y);

		return grab_toplevel;
	      }

	    return NULL;
	  }
	else 
	  {
	    /* The non-grabbed case. */
            GdkWindow *toplevel_under_pointer;
            gint x_tmp, y_tmp;

            /* Ignore all events but mouse moved that might be on the title
             * bar (above the content view). The reason is that otherwise
             * gdk gets confused about getting e.g. button presses with no
             * window (the title bar is not known to it).
             */
            if (event_type != NSMouseMoved)
              if (*y < 0)
                return NULL;

            /* As for owner events, we need to use the toplevel under the
             * pointer, not the window from the NSEvent.
             */
            toplevel_under_pointer = find_toplevel_under_pointer (display,
                                                                  screen_point,
                                                                  &x_tmp, &y_tmp);
            if (toplevel_under_pointer
                && WINDOW_IS_TOPLEVEL (toplevel_under_pointer))
              {
                GdkWindowObject *toplevel_private;
                GdkWindowImplQuartz *toplevel_impl;

                toplevel = toplevel_under_pointer;

                toplevel_private = (GdkWindowObject *)toplevel;
                toplevel_impl = (GdkWindowImplQuartz *)toplevel_private->impl;

		{
			unsigned int subviews = [[toplevel_impl->view subviews] count];
			unsigned int si;

			for (si = 0; si < subviews; ++si) {
				NSView* sv = [[toplevel_impl->view subviews] objectAtIndex:si];
				if ([sv tag] == ARDOUR_CANVAS_NSVIEW_TAG)
				  {
				     continue;
				  }
 				NSRect r = [sv frame];
				if (r.origin.x <= *x && r.origin.x + r.size.width >= *x &&
				    r.origin.y <= *y && r.origin.y + r.size.height >= *y) {
					/* event is within subview, forward back to Cocoa */

					switch (event_type)
					  {
					      case NSLeftMouseDown:
					      case NSRightMouseDown:
					      case NSOtherMouseDown:
					 	 foreign_mouse_down_view = sv;
						 break;
					      default:
						 break;
					  }

					return NULL;
				}
			}
		}

                *x = x_tmp;
                *y = y_tmp;
              }
            return toplevel;
	  }
      }
      break;

    case NSMouseEntered:
    case NSMouseExited:
      /* Only handle our own entered/exited events, not the ones for the
       * titlebar buttons.
       */
      if ([view trackingRect] == nsevent.trackingNumber)
        return toplevel;

      /* MacOS 13 isn't sending the trackingArea events so we have to
       * rely on the cursorRect events that we discarded in earlier
       * macOS versions. These trigger 4 pixels out from the window's
       * frame so we obtain that rect and adjust it for hit testing.
       */
      if (!nsevent.trackingArea && gdk_quartz_osx_version() >= GDK_OSX_VENTURA)
        {
          static const int border_width = 4;
          NSRect frame = nsevent.window.frame;
          gboolean inside, at_edge;

          frame.origin.x -= border_width;
          frame.origin.y -= border_width;
          frame.size.width += 2 * border_width;
          frame.size.height += 2 * border_width;
          inside =
               screen_point.x >= frame.origin.x &&
               screen_point.x <= frame.origin.x + frame.size.width &&
               screen_point.y >= frame.origin.y &&
               screen_point.y <= frame.origin.y + frame.size.height;
          at_edge =
               screen_point.x >= frame.origin.x - 1 &&
               screen_point.x <= frame.origin.x + frame.size.width + 1 &&
               screen_point.y >= frame.origin.y - 1 &&
               screen_point.y <= frame.origin.y + frame.size.height + 1;

          if ((event_type == NSMouseEntered && inside) ||
              at_edge)
            return toplevel;
          else
            return NULL;
        }

      return NULL;

    case NSKeyDown:
    case NSKeyUp:
    case NSFlagsChanged:
      if (_gdk_display->keyboard_grab.window && !_gdk_display->keyboard_grab.owner_events)
        return gdk_window_get_effective_toplevel (_gdk_display->keyboard_grab.window);

      return toplevel;

    default:
      /* Ignore everything else. */
      break;
    }

  return NULL;
}

static void
fill_crossing_event (GdkWindow       *toplevel,
                     GdkEvent        *event,
                     NSEvent         *nsevent,
                     gint             x,
                     gint             y,
                     gint             x_root,
                     gint             y_root,
                     GdkEventType     event_type,
                     GdkCrossingMode  mode,
                     GdkNotifyType    detail)
{
  event->any.type = event_type;
  event->crossing.window = toplevel;
  event->crossing.subwindow = NULL;
  event->crossing.time = get_time_from_ns_event (nsevent);
  event->crossing.x = x;
  event->crossing.y = y;
  event->crossing.x_root = x_root;
  event->crossing.y_root = y_root;
  event->crossing.mode = mode;
  event->crossing.detail = detail;
  event->crossing.state = get_keyboard_modifiers_from_ns_event (nsevent) |
                         _gdk_quartz_events_get_current_mouse_modifiers ();

  /* FIXME: Focus and button state? */
}

static void
fill_button_event (GdkWindow *window,
                   GdkEvent  *event,
                   NSEvent   *nsevent,
                   gint       x,
                   gint       y,
                   gint       x_root,
                   gint       y_root)
{
  GdkEventType type;
  gint state;

  state = get_keyboard_modifiers_from_ns_event (nsevent) |
         _gdk_quartz_events_get_current_mouse_modifiers ();

  switch ([nsevent type])
    {
    case NSLeftMouseDown:
    case NSRightMouseDown:
    case NSOtherMouseDown:
      type = GDK_BUTTON_PRESS;
      state &= ~get_mouse_button_modifiers_from_ns_event (nsevent);
      break;

    case NSLeftMouseUp:
    case NSRightMouseUp:
    case NSOtherMouseUp:
      type = GDK_BUTTON_RELEASE;
      state |= get_mouse_button_modifiers_from_ns_event (nsevent);
      break;

    default:
      g_assert_not_reached ();
    }

  event->any.type = type;
  event->button.window = window;
  event->button.time = get_time_from_ns_event (nsevent);
  event->button.x = x;
  event->button.y = y;
  event->button.x_root = x_root;
  event->button.y_root = y_root;
  /* FIXME event->axes */
  event->button.state = state;
  event->button.button = get_mouse_button_from_ns_event (nsevent);
  event->button.device = _gdk_display->core_pointer;
}

static void
fill_motion_event (GdkWindow *window,
                   GdkEvent  *event,
                   NSEvent   *nsevent,
                   gint       x,
                   gint       y,
                   gint       x_root,
                   gint       y_root)
{
  GdkModifierType state;

  state = get_keyboard_modifiers_from_ns_event (nsevent);

  switch ([nsevent type])
    {
    case NSLeftMouseDragged:
    case NSRightMouseDragged:
    case NSOtherMouseDragged:
      state |= get_mouse_button_modifiers_from_ns_event (nsevent);
      break;

    case NSMouseMoved:
    default:
      break;
    }

  event->any.type = GDK_MOTION_NOTIFY;
  event->motion.window = window;
  event->motion.time = get_time_from_ns_event (nsevent);
  event->motion.x = x;
  event->motion.y = y;
  event->motion.x_root = x_root;
  event->motion.y_root = y_root;
  /* FIXME event->axes */
  event->motion.state = get_keyboard_modifiers_from_ns_event (nsevent) |
                        _gdk_quartz_events_get_current_mouse_modifiers ();
  event->motion.is_hint = FALSE;
  event->motion.device = _gdk_display->core_pointer;
}

static void
fill_scroll_event (GdkWindow          *window,
                   GdkEvent           *event,
                   NSEvent            *nsevent,
                   gint                x,
                   gint                y,
                   gint                x_root,
                   gint                y_root,
                   gboolean            has_deltas,
                   gdouble             delta_x,
                   gdouble             delta_y,
                   GdkScrollDirection  direction)
{
  GdkWindowObject *private;

  private = GDK_WINDOW_OBJECT (window);

  event->any.type = GDK_SCROLL;
  event->scroll.window = window;
  event->scroll.time = get_time_from_ns_event (nsevent);
  event->scroll.x = x;
  event->scroll.y = y;
  event->scroll.x_root = x_root;
  event->scroll.y_root = y_root;
  event->scroll.state = get_keyboard_modifiers_from_ns_event (nsevent);
  event->scroll.direction = direction;
  event->scroll.device = _gdk_display->core_pointer;
  event->scroll.has_deltas = has_deltas;
  event->scroll.delta_x = delta_x;
  event->scroll.delta_y = delta_y;
}

static void
fill_key_event (GdkWindow    *window,
                GdkEvent     *event,
                NSEvent      *nsevent,
                GdkEventType  type)
{
  GdkEventPrivate *priv;
  gchar buf[7];
  gunichar c = 0;

  priv = (GdkEventPrivate *) event;
  priv->windowing_data = [nsevent retain];

  event->any.type = type;
  event->key.window = window;
  event->key.time = get_time_from_ns_event (nsevent);
  event->key.state = get_keyboard_modifiers_from_ns_event (nsevent);
  event->key.hardware_keycode = [nsevent keyCode];
  event->key.group = ([nsevent modifierFlags] & NSAlternateKeyMask) ? 1 : 0;
  event->key.keyval = GDK_VoidSymbol;
  
  gdk_keymap_translate_keyboard_state (NULL,
				       event->key.hardware_keycode,
				       event->key.state, 
				       event->key.group,
				       &event->key.keyval,
				       NULL, NULL, NULL);

  event->key.is_modifier = _gdk_quartz_keys_is_modifier (event->key.hardware_keycode);

  /* If the key press is a modifier, the state should include the mask
   * for that modifier but only for releases, not presses. This
   * matches the X11 backend behavior.
   */
  if (event->key.is_modifier)
    {
      int mask = 0;

      switch (event->key.keyval)
        {
        case GDK_Meta_R:
        case GDK_Meta_L:
          mask = GDK_MOD2_MASK;
          break;
        case GDK_Shift_R:
        case GDK_Shift_L:
          mask = GDK_SHIFT_MASK;
          break;
        case GDK_Caps_Lock:
          mask = GDK_LOCK_MASK;
          break;
        case GDK_Alt_R:
        case GDK_Alt_L:
          mask = GDK_MOD1_MASK;
          break;
        case GDK_Control_R:
        case GDK_Control_L:
          mask = GDK_CONTROL_MASK;
          break;
        default:
          mask = 0;
        }

      if (type == GDK_KEY_PRESS)
        event->key.state &= ~mask;
      else if (type == GDK_KEY_RELEASE)
        event->key.state |= mask;
    }

  event->key.state |= _gdk_quartz_events_get_current_mouse_modifiers ();
  event->key.string = NULL;

  /* Fill in ->string since apps depend on it, taken from the x11 backend. */
  if (event->key.keyval != GDK_VoidSymbol)
    c = gdk_keyval_to_unicode (event->key.keyval);

  if (c)
    {
      gsize bytes_written;
      gint len;

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

  GDK_NOTE(EVENTS,
    g_message ("key %s:\t\twindow: %p  key: %12s  %d",
	  type == GDK_KEY_PRESS ? "press" : "release",
	  event->key.window,
	  event->key.keyval ? gdk_keyval_name (event->key.keyval) : "(none)",
	  event->key.keyval));
}

static gboolean
synthesize_crossing_event (GdkWindow *window,
                           GdkEvent  *event,
                           NSEvent   *nsevent,
                           gint       x,
                           gint       y,
                           gint       x_root,
                           gint       y_root)
{
  GdkWindowObject *private;

  private = GDK_WINDOW_OBJECT (window);

  switch ([nsevent type])
    {
    case NSMouseEntered:
      /* Enter events are considered always to be from another toplevel
       * window, this shouldn't negatively affect any app or gtk code,
       * and is the only way to make GtkMenu work. EEK EEK EEK.
       */
      if (!(private->event_mask & GDK_ENTER_NOTIFY_MASK))
        return FALSE;

      fill_crossing_event (window, event, nsevent,
                           x, y,
                           x_root, y_root,
                           GDK_ENTER_NOTIFY,
                           GDK_CROSSING_NORMAL,
                           GDK_NOTIFY_NONLINEAR);
      return TRUE;

    case NSMouseExited:
      /* See above */
      if (!(private->event_mask & GDK_LEAVE_NOTIFY_MASK))
        return FALSE;

      fill_crossing_event (window, event, nsevent,
                           x, y,
                           x_root, y_root,
                           GDK_LEAVE_NOTIFY,
                           GDK_CROSSING_NORMAL,
                           GDK_NOTIFY_NONLINEAR);
      return TRUE;

    default:
      break;
    }

  return FALSE;
}

void
_gdk_quartz_synthesize_null_key_event (GdkWindow *window)
{
  GdkEvent *event;

  event = gdk_event_new (GDK_KEY_PRESS);
  event->any.type = GDK_KEY_PRESS;
  event->key.window = window;
  event->key.state = 0;
  event->key.hardware_keycode = 0;
  event->key.group = 0;
  event->key.keyval = GDK_VoidSymbol;
  append_event(event, FALSE);
}

GdkEventMask 
_gdk_quartz_events_get_current_event_mask (void)
{
  return current_event_mask;
}

GdkModifierType
_gdk_quartz_events_get_current_keyboard_modifiers (void)
{
  if (gdk_quartz_osx_version () >= GDK_OSX_SNOW_LEOPARD)
    {
      return get_keyboard_modifiers_from_ns_flags ([NSClassFromString(@"NSEvent") modifierFlags]);
    }
  else
    {
      guint carbon_modifiers = GetCurrentKeyModifiers ();
      GdkModifierType modifiers = 0;

      if (carbon_modifiers & alphaLock)
        modifiers |= GDK_LOCK_MASK;
      if (carbon_modifiers & shiftKey)
        modifiers |= GDK_SHIFT_MASK;
      if (carbon_modifiers & controlKey)
        modifiers |= GDK_CONTROL_MASK;
      if (carbon_modifiers & optionKey)
        modifiers |= GDK_MOD1_MASK;
      if (carbon_modifiers & cmdKey)
        modifiers |= GDK_MOD2_MASK;

      return modifiers;
    }
}

GdkModifierType
_gdk_quartz_events_get_current_mouse_modifiers (void)
{
  if (gdk_quartz_osx_version () >= GDK_OSX_SNOW_LEOPARD)
    {
      return get_mouse_button_modifiers_from_ns_buttons ([NSClassFromString(@"NSEvent") pressedMouseButtons]);
    }
  else
    {
      return get_mouse_button_modifiers_from_ns_buttons (GetCurrentButtonState ());
    }
}

/* Detect window resizing */

static gboolean
test_resize (NSEvent *event, GdkWindow *toplevel, gint x, gint y)
{
  GdkWindowObject *toplevel_private;
  GdkWindowImplQuartz *toplevel_impl;
  gboolean lion;

  /* Resizing from the resize indicator only begins if an NSLeftMouseButton
   * event is received in the resizing area.
   */
  toplevel_private = (GdkWindowObject *)toplevel;
  toplevel_impl = (GdkWindowImplQuartz *)toplevel_private->impl;
  if ([event type] == NSLeftMouseDown &&
      [toplevel_impl->toplevel showsResizeIndicator])
    {
      NSRect frame;

      /* If the resize indicator is visible and the event
       * is in the lower right 15x15 corner, we leave these
       * events to Cocoa as to be handled as resize events.
       * Applications may have widgets in this area.  These
       * will most likely be larger than 15x15 and for
       * scroll bars there are also other means to move
       * the scroll bar.  Since the resize indicator is
       * the only way of resizing windows on Mac OS, it
       * is too important to not make functional.
       */
      frame = [toplevel_impl->view bounds];
      if (x > frame.size.width - GRIP_WIDTH &&
          x < frame.size.width &&
          y > frame.size.height - GRIP_HEIGHT &&
          y < frame.size.height)
        return TRUE;
     }

  /* If we're on Lion and within 5 pixels of an edge,
   * then assume that the user wants to resize, and
   * return NULL to let Quartz get on with it. We check
   * the selector isRestorable to see if we're on 10.7.
   * This extra check is in case the user starts
   * dragging before GDK recognizes the grab.
   *
   * We perform this check for a button press of all buttons, because we
   * do receive, for instance, a right mouse down event for a GDK window
   * for x-coordinate range [-3, 0], but we do not want to forward this
   * into GDK. Forwarding such events into GDK will confuse the pointer
   * window finding code, because there are no GdkWindows present in
   * the range [-3, 0].
   */
  lion = gdk_quartz_osx_version () >= GDK_OSX_LION;
  if (lion &&
      ([event type] == NSLeftMouseDown ||
       [event type] == NSRightMouseDown ||
       [event type] == NSOtherMouseDown))
    {
      if (x < GDK_LION_RESIZE ||
          x > toplevel_private->width - GDK_LION_RESIZE ||
          y > toplevel_private->height - GDK_LION_RESIZE)
        return TRUE;
    }

  return FALSE;
}

static gboolean
gdk_event_translate (GdkEvent *event,
                     NSEvent  *nsevent)
{
  NSEventType event_type;
  NSWindow *nswindow;
  GdkWindow *window;
  int x, y;
  int x_root, y_root;
  gboolean return_val;
  GdkEvent *input_event;

  /* There is no support for real desktop wide grabs, so we break
   * grabs when the application loses focus (gets deactivated).
   */
  event_type = [nsevent type];
  if (event_type == NSAppKitDefined)
    {
      if ([nsevent subtype] == NSApplicationDeactivatedEventType)
        _gdk_quartz_events_break_all_grabs (get_time_from_ns_event (nsevent));

      /* This could potentially be used to break grabs when clicking
       * on the title. The subtype 20 is undocumented so it's probably
       * not a good idea: else if (subtype == 20) break_all_grabs ();
       */

      /* Leave all AppKit events to AppKit. */
      return FALSE;
    }

  if (_gdk_default_filters)
    {
      /* Apply global filters */
      GdkFilterReturn result;

      result = gdk_event_apply_filters (nsevent, event, &_gdk_default_filters);
      if (result != GDK_FILTER_CONTINUE)
        {
          return_val = (result == GDK_FILTER_TRANSLATE) ? TRUE : FALSE;
          goto done;
        }
    }

  nswindow = [nsevent window];

  /* Ignore events for windows not created by GDK. */
  if (nswindow && ![[nswindow contentView] isKindOfClass:[GdkQuartzView class]])
    return FALSE;

  /* Ignore events for ones with no windows */
  if (!nswindow)
    {
      GdkWindow *toplevel = NULL;

      if (event_type == NSMouseMoved)
        {
          /* Motion events received after clicking the menu bar do not have the
           * window field set.  Instead of giving up on the event immediately,
           * we first check whether this event is within our window bounds.
           */
          NSPoint screen_point = [NSEvent mouseLocation];
          gint x_tmp, y_tmp;

          toplevel = find_toplevel_under_pointer (_gdk_display,
                                                  screen_point,
                                                  &x_tmp, &y_tmp);
        }

      if (!toplevel)
        return FALSE;
    }

  /* Ignore events and break grabs while the window is being
   * dragged. This is a workaround for the window getting events for
   * the window title.
   */
  if ([(GdkQuartzWindow *)nswindow isInMove])
    {
      _gdk_quartz_events_break_all_grabs (get_time_from_ns_event (nsevent));
      return FALSE;
    }

  /* Also when in a manual resize, we ignore events so that these are
   * pushed to GdkQuartzWindow's sendEvent handler.
   */
  if ([(GdkQuartzWindow *)nswindow isInManualResize])
    return FALSE;

  /* Find the right GDK window to send the event to, taking grabs and
   * event masks into consideration.
   */
  window = find_window_for_ns_event (nsevent, &x, &y, &x_root, &y_root);
  if (!window)
    return FALSE;

  /* Quartz handles resizing on its own, so we want to stay out of the way. */
  if (test_resize (nsevent, window, x, y))
    return FALSE;

  /* Apply any window filters. */
  if (GDK_IS_WINDOW (window))
    {
      GdkWindowObject *filter_private = (GdkWindowObject *) window;
      GdkFilterReturn result;

      if (filter_private->filters)
        {
          g_object_ref (window);

          result = gdk_event_apply_filters (nsevent, event, &filter_private->filters);

          g_object_unref (window);

          if (result != GDK_FILTER_CONTINUE)
            {
              return_val = (result == GDK_FILTER_TRANSLATE) ? TRUE : FALSE;
              goto done;
            }
        }
    }

  /* If the app is not active leave the event to AppKit so the window gets
   * focused correctly and don't do click-through (so we behave like most
   * native apps). If the app is active, we focus the window and then handle
   * the event, also to match native apps.
   */
  if ((event_type == NSRightMouseDown ||
       event_type == NSOtherMouseDown ||
       event_type == NSLeftMouseDown))
    {
      GdkWindowObject *private = (GdkWindowObject *)window;
      GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);

      if (![NSApp isActive])
        {
          [NSApp activateIgnoringOtherApps:YES];
          return FALSE;
        }
      else if (![impl->toplevel isKeyWindow])
        {
          GdkPointerGrabInfo *grab;

          grab = _gdk_display_get_last_pointer_grab (_gdk_display);
          if (!grab)
            [impl->toplevel makeKeyWindow];
        }
    }

  current_event_mask = get_event_mask_from_ns_event (nsevent);

  return_val = TRUE;

  switch (event_type)
    {
    case NSLeftMouseDown:
    case NSRightMouseDown:
    case NSOtherMouseDown:
    case NSLeftMouseUp:
    case NSRightMouseUp:
    case NSOtherMouseUp:
      fill_button_event (window, event, nsevent, x, y, x_root, y_root);

      input_event = gdk_event_new (GDK_NOTHING);
      if (_gdk_input_fill_quartz_input_event (event, nsevent, input_event))
        append_event (input_event, TRUE);
      else
        gdk_event_free (input_event);
      break;

    case NSLeftMouseDragged:
    case NSRightMouseDragged:
    case NSOtherMouseDragged:
    case NSMouseMoved:
      fill_motion_event (window, event, nsevent, x, y, x_root, y_root);

      input_event = gdk_event_new (GDK_NOTHING);
      if (_gdk_input_fill_quartz_input_event (event, nsevent, input_event))
        append_event (input_event, TRUE);
      else
        gdk_event_free (input_event);
      break;

    case NSScrollWheel:
      {
        GdkScrollDirection direction;
	float dx;
	float dy;

#if GTK_OSX_MIN >= 7
	if (gdk_quartz_osx_version() >= GDK_OSX_LION &&
	    [nsevent hasPreciseScrollingDeltas])
	  {
	    dx = [nsevent scrollingDeltaX];
	    dy = [nsevent scrollingDeltaY];

            if (fabs (dy) > fabs (dx))
              {
                if (dy < 0.0)
                  direction = GDK_SCROLL_DOWN;
                else
                  direction = GDK_SCROLL_UP;
              }
            else
              {
                if (dx < 0.0)
                  direction = GDK_SCROLL_RIGHT;
                else
                  direction = GDK_SCROLL_LEFT;
              }

            fill_scroll_event (window, event, nsevent, x, y, x_root, y_root,
                               TRUE, -dx, -dy, direction);
	  }
	else
	  {
#endif /* earlier than Lion */
	    dx = [nsevent deltaX];
	    dy = [nsevent deltaY];

            if (dy != 0.0)
              {
                if (dy < 0.0)
                  direction = GDK_SCROLL_DOWN;
                else
                  direction = GDK_SCROLL_UP;

                fill_scroll_event (window, event, nsevent, x, y, x_root, y_root,
                                   FALSE, 0.0, fabs (dy), direction);
              }
            else if (dx != 0.0)
              {
                if (dx < 0.0)
                  direction = GDK_SCROLL_RIGHT;
                else
                  direction = GDK_SCROLL_LEFT;

                fill_scroll_event (window, event, nsevent, x, y, x_root, y_root,
                                   FALSE, fabs (dx), 0.0, direction);
              }
#if GTK_OSX_MIN >= 7
          }
#endif
      }
      break;

    case NSMouseExited:
      if (WINDOW_IS_TOPLEVEL (window))
          [[NSCursor arrowCursor] set];
      /* fall through */
    case NSMouseEntered:
      return_val = synthesize_crossing_event (window, event, nsevent, x, y, x_root, y_root);
      break;

    case NSKeyDown:
    case NSKeyUp:
    case NSFlagsChanged:
      {
        GdkEventType type;

        type = _gdk_quartz_keys_event_type (nsevent);
        if (type == GDK_NOTHING)
          return_val = FALSE;
        else
          fill_key_event (window, event, nsevent, type);
      }
      break;

    case NSTabletProximity:
      _gdk_input_quartz_tablet_proximity ([nsevent pointingDeviceType]);
      return_val = FALSE;
      break;

    default:
      /* Ignore everything elsee. */
      return_val = FALSE;
      break;
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

  return return_val;
}

void
_gdk_events_queue (GdkDisplay *display)
{  
  NSEvent *nsevent;

  nsevent = _gdk_quartz_event_loop_get_pending ();
  if (nsevent)
    {
      GdkEvent *event;
      GList *node;

      event = gdk_event_new (GDK_NOTHING);

      event->any.window = NULL;
      event->any.send_event = FALSE;

      ((GdkEventPrivate *)event)->flags |= GDK_EVENT_PENDING;

      node = _gdk_event_queue_append (display, event);

      if (gdk_event_translate (event, nsevent))
        {
	  ((GdkEventPrivate *)event)->flags &= ~GDK_EVENT_PENDING;
          _gdk_windowing_got_event (display, node, event, 0);
        }
      else
        {
	  _gdk_event_queue_remove_link (display, node);
	  g_list_free_1 (node);
	  gdk_event_free (event);

          GDK_THREADS_LEAVE ();
          [NSApp sendEvent:nsevent];
          GDK_THREADS_ENTER ();
        }

      _gdk_quartz_event_loop_release_event (nsevent);
    }
}

void
gdk_flush (void)
{
  /* Not supported. */
}

void
gdk_display_add_client_message_filter (GdkDisplay   *display,
				       GdkAtom       message_type,
				       GdkFilterFunc func,
				       gpointer      data)
{
  /* Not supported. */
}

void
gdk_add_client_message_filter (GdkAtom       message_type,
			       GdkFilterFunc func,
			       gpointer      data)
{
  /* Not supported. */
}

void
gdk_display_sync (GdkDisplay *display)
{
  /* Not supported. */
}

void
gdk_display_flush (GdkDisplay *display)
{
  /* Not supported. */
}

gboolean
gdk_event_send_client_message_for_display (GdkDisplay      *display,
					   GdkEvent        *event,
					   GdkNativeWindow  winid)
{
  /* Not supported. */
  return FALSE;
}

void
gdk_screen_broadcast_client_message (GdkScreen *screen,
				     GdkEvent  *event)
{
  /* Not supported. */
}

gboolean
gdk_screen_get_setting (GdkScreen   *screen,
			const gchar *name,
			GValue      *value)
{
  if (strcmp (name, "gtk-double-click-time") == 0)
    {
      NSUserDefaults *defaults;
      float t;

      GDK_QUARTZ_ALLOC_POOL;

      defaults = [NSUserDefaults standardUserDefaults];
            
      t = [defaults floatForKey:@"com.apple.mouse.doubleClickThreshold"];
      if (t == 0.0)
	{
	  /* No user setting, use the default in OS X. */
	  t = 0.5;
	}

      GDK_QUARTZ_RELEASE_POOL;

      g_value_set_int (value, t * 1000);

      return TRUE;
    }
  else if (strcmp (name, "gtk-font-name") == 0)
    {
      NSString *name;
      char *str;

      GDK_QUARTZ_ALLOC_POOL;

      name = [[NSFont systemFontOfSize:0] familyName];

      /* Let's try to use the "views" font size (12pt) by default. This is
       * used for lists/text/other "content" which is the largest parts of
       * apps, using the "regular control" size (13pt) looks a bit out of
       * place. We might have to tweak this.
       */

      /* The size has to be hardcoded as there doesn't seem to be a way to
       * get the views font size programmatically.
       */
      str = g_strdup_printf ("%s 12", [name UTF8String]);
      g_value_set_string (value, str);
      g_free (str);

      GDK_QUARTZ_RELEASE_POOL;

      return TRUE;
    }
  else if (strcmp (name, "gtk-primary-button-warps-slider") == 0)
    {
      GDK_QUARTZ_ALLOC_POOL;

      BOOL setting = [[NSUserDefaults standardUserDefaults] boolForKey:@"AppleScrollerPagingBehavior"];

      /* If the Apple property is YES, it means "warp" */
      g_value_set_boolean (value, setting == YES);

      GDK_QUARTZ_RELEASE_POOL;

      return TRUE;
    }
  
  /* FIXME: Add more settings */

  return FALSE;
}

void
_gdk_windowing_event_data_copy (const GdkEvent *src,
                                GdkEvent       *dst)
{
  GdkEventPrivate *priv_src = (GdkEventPrivate *) src;
  GdkEventPrivate *priv_dst = (GdkEventPrivate *) dst;

  if (priv_src->windowing_data)
    {
      priv_dst->windowing_data = priv_src->windowing_data;
      [(NSEvent *)priv_dst->windowing_data retain];
    }
}

void
_gdk_windowing_event_data_free (GdkEvent *event)
{
  GdkEventPrivate *priv = (GdkEventPrivate *) event;

  if (priv->windowing_data)
    {
      [(NSEvent *)priv->windowing_data release];
      priv->windowing_data = NULL;
    }
}
