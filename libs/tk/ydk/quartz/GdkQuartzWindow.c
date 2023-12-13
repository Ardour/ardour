/* GdkQuartzWindow.m
 *
 * Copyright (C) 2005-2007 Imendio AB
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

#import "GdkQuartzWindow.h"
#include "gdkwindow-quartz.h"
#include "gdkprivate-quartz.h"

@implementation GdkQuartzWindow

- (void)windowWillClose:(NSNotification*)notification
{
  // Clears the delegate when window is going to be closed; since EL
  // Capitan it is possible that the methods of delegate would get
  // called after the window has been closed.
  [self setDelegate:nil];
}

-(BOOL)windowShouldClose:(id)sender
{
  GdkWindow *window = [[self contentView] gdkWindow];
  GdkEvent *event;

  event = gdk_event_new (GDK_DELETE);

  event->any.window = g_object_ref (window);
  event->any.send_event = FALSE;

  _gdk_event_queue_append (gdk_display_get_default (), event);

  return NO;
}

-(void)windowWillMiniaturize:(NSNotification *)aNotification
{
  GdkWindow *window = [[self contentView] gdkWindow];

  _gdk_quartz_window_detach_from_parent (window);
}

-(void)windowDidMiniaturize:(NSNotification *)aNotification
{
  GdkWindow *window = [[self contentView] gdkWindow];

  gdk_synthesize_window_state (window, 0, 
			       GDK_WINDOW_STATE_ICONIFIED);
}

-(void)windowDidDeminiaturize:(NSNotification *)aNotification
{
  GdkWindow *window = [[self contentView] gdkWindow];

  _gdk_quartz_window_attach_to_parent (window);

  gdk_synthesize_window_state (window, GDK_WINDOW_STATE_ICONIFIED, 0);
}

-(void)windowDidBecomeKey:(NSNotification *)aNotification
{
  GdkWindow *window = [[self contentView] gdkWindow];

  _gdk_quartz_events_update_focus_window (window, TRUE);
}

-(void)windowDidResignKey:(NSNotification *)aNotification
{
  GdkWindow *window = [[self contentView] gdkWindow];

  _gdk_quartz_events_update_focus_window (window, FALSE);
}

-(void)windowDidBecomeMain:(NSNotification *)aNotification
{
  GdkWindow *window = [[self contentView] gdkWindow];

  if (![self isVisible])
    {
      /* Note: This is a hack needed because for unknown reasons, hidden
       * windows get shown when clicking the dock icon when the application
       * is not already active.
       */
      [self orderOut:nil];
      return;
    }

  _gdk_quartz_window_did_become_main (window);
}

-(void)windowDidResignMain:(NSNotification *)aNotification
{
  GdkWindow *window;

  window = [[self contentView] gdkWindow];
  _gdk_quartz_window_did_resign_main (window);
}

/* Used in combination with NSLeftMouseUp in sendEvent to keep track
 * of when the window is being moved with the mouse.
 */
-(void)windowWillMove:(NSNotification *)aNotification
{
  inMove = YES;
}

-(void)sendEvent:(NSEvent *)event
{
  switch ([event type])
    {
    case NSLeftMouseUp:
    {
      double time = ((double)[event timestamp]) * 1000.0;

      _gdk_quartz_events_break_all_grabs (time);
      inManualMove = NO;
      inManualResize = NO;
      inMove = NO;
      break;
    }

    case NSLeftMouseDragged:
      if ([self trackManualMove] || [self trackManualResize])
        return;
      break;

    default:
      break;
    }

  [super sendEvent:event];
}

-(BOOL)isInMove
{
  return inMove;
}

-(void)checkSendEnterNotify
{
  GdkWindow *window = [[self contentView] gdkWindow];
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);

  /* When a new window has been created, and the mouse
   * is in the window area, we will not receive an NSMouseEntered
   * event.  Therefore, we synthesize an enter notify event manually.
   */
  if (!initialPositionKnown)
    {
      initialPositionKnown = YES;

      if (NSPointInRect ([NSEvent mouseLocation], [self frame]))
        {
          NSEvent *event;

          event = [NSEvent enterExitEventWithType: NSMouseEntered
                                         location: [self mouseLocationOutsideOfEventStream]
                                    modifierFlags: 0
                                        timestamp: [[NSApp currentEvent] timestamp]
                                     windowNumber: [impl->toplevel windowNumber]
                                          context: NULL
                                      eventNumber: 0
                                   trackingNumber: [impl->view trackingRect]
                                         userData: nil];

          [NSApp postEvent:event atStart:NO];
        }
    }
}

/* Always update both the position and size. Certain resize operations
 * (e.g. going fullscreen) also move the origin of the window. Move
 * notifications sometimes also indicate a different window size (for
 * example if the window size requested in the configure request was not
 * fully granted).
 */
-(void)handleDidMoveResize
{
  NSRect content_rect = [self contentRectForFrameRect:[self frame]];
  GdkWindow *window = [[self contentView] gdkWindow];
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkEvent *event;

  private->width = content_rect.size.width;
  private->height = content_rect.size.height;

  _gdk_quartz_window_update_position (window);

  [[self contentView] setFrame:NSMakeRect (0, 0, private->width, private->height)];

  _gdk_window_update_size (window);

  /* Synthesize a configure event */
  event = gdk_event_new (GDK_CONFIGURE);
  event->configure.window = g_object_ref (window);
  event->configure.x = private->x;
  event->configure.y = private->y;
  event->configure.width = private->width;
  event->configure.height = private->height;

  _gdk_event_queue_append (gdk_display_get_default (), event);

  [self checkSendEnterNotify];
}

-(void)windowDidMove:(NSNotification *)aNotification
{
  [self handleDidMoveResize];
}

-(void)windowDidResize:(NSNotification *)aNotification
{
  [self handleDidMoveResize];
}

-(id)initWithContentRect:(NSRect)contentRect styleMask:(NSUInteger)styleMask backing:(NSBackingStoreType)backingType defer:(BOOL)flag screen:(NSScreen *)screen
{
  self = [super initWithContentRect:contentRect
	                  styleMask:styleMask
	                    backing:backingType
	                      defer:flag
                             screen:screen];

  [self setAcceptsMouseMovedEvents:YES];
  [self setDelegate:self];
  [self setReleasedWhenClosed:YES];

  NSColorSpace *dcs = [NSColorSpace genericRGBColorSpace];
  [self setColorSpace:dcs];

  return self;
}

-(BOOL)canBecomeMainWindow
{
  GdkWindow *window = [[self contentView] gdkWindow];
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);

  switch (impl->type_hint)
    {
    case GDK_WINDOW_TYPE_HINT_NORMAL:
    case GDK_WINDOW_TYPE_HINT_DIALOG:
      return YES;
      
    case GDK_WINDOW_TYPE_HINT_MENU:
    case GDK_WINDOW_TYPE_HINT_TOOLBAR:
    case GDK_WINDOW_TYPE_HINT_SPLASHSCREEN:
    case GDK_WINDOW_TYPE_HINT_UTILITY:
    case GDK_WINDOW_TYPE_HINT_DOCK:
    case GDK_WINDOW_TYPE_HINT_DESKTOP:
    case GDK_WINDOW_TYPE_HINT_DROPDOWN_MENU:
    case GDK_WINDOW_TYPE_HINT_POPUP_MENU:
    case GDK_WINDOW_TYPE_HINT_TOOLTIP:
    case GDK_WINDOW_TYPE_HINT_NOTIFICATION:
    case GDK_WINDOW_TYPE_HINT_COMBO:
    case GDK_WINDOW_TYPE_HINT_DND:
      return NO;
    }
  
  return YES;
}

-(BOOL)canBecomeKeyWindow
{
  GdkWindow *window = [[self contentView] gdkWindow];
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);

  if (!private->accept_focus)
    return NO;

  /* Popup windows should not be able to get focused in the window
   * manager sense, it's only handled through grabs.
   */
  if (private->window_type == GDK_WINDOW_TEMP)
    return NO;

  switch (impl->type_hint)
    {
    case GDK_WINDOW_TYPE_HINT_NORMAL:
    case GDK_WINDOW_TYPE_HINT_DIALOG:
    case GDK_WINDOW_TYPE_HINT_MENU:
    case GDK_WINDOW_TYPE_HINT_TOOLBAR:
    case GDK_WINDOW_TYPE_HINT_UTILITY:
    case GDK_WINDOW_TYPE_HINT_DOCK:
    case GDK_WINDOW_TYPE_HINT_DESKTOP:
    case GDK_WINDOW_TYPE_HINT_DROPDOWN_MENU:
    case GDK_WINDOW_TYPE_HINT_POPUP_MENU:
    case GDK_WINDOW_TYPE_HINT_COMBO:
      return YES;
      
    case GDK_WINDOW_TYPE_HINT_SPLASHSCREEN:
    case GDK_WINDOW_TYPE_HINT_TOOLTIP:
    case GDK_WINDOW_TYPE_HINT_NOTIFICATION:
    case GDK_WINDOW_TYPE_HINT_DND:
      return NO;
    }
  
  return YES;
}

- (void)showAndMakeKey:(BOOL)makeKey
{
  GdkWindow *window = [[self contentView] gdkWindow];
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);

  inShowOrHide = YES;

  if (makeKey)
    [impl->toplevel makeKeyAndOrderFront:impl->toplevel];
  else
    [impl->toplevel orderFront:nil];

  inShowOrHide = NO;

  [self checkSendEnterNotify];
}

- (void)hide
{
  GdkWindow *window = [[self contentView] gdkWindow];
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);

  inShowOrHide = YES;
  [impl->toplevel orderOut:nil];
  inShowOrHide = NO;

  initialPositionKnown = NO;
}

- (BOOL)trackManualMove
{
  NSPoint currentLocation;
  NSPoint newOrigin;
  NSRect screenFrame = [[NSScreen mainScreen] visibleFrame];
  NSRect windowFrame = [self frame];

  if (!inManualMove)
    return NO;

  currentLocation = [self convertBaseToScreen:[self mouseLocationOutsideOfEventStream]];
  newOrigin.x = currentLocation.x - initialMoveLocation.x;
  newOrigin.y = currentLocation.y - initialMoveLocation.y;

  /* Clamp vertical position to below the menu bar. */
  if (newOrigin.y + windowFrame.size.height > screenFrame.origin.y + screenFrame.size.height)
    newOrigin.y = screenFrame.origin.y + screenFrame.size.height - windowFrame.size.height;

  [self setFrameOrigin:newOrigin];

  return YES;
}

-(void)beginManualMove
{
  NSRect frame = [self frame];

  if (inMove || inManualMove || inManualResize)
    return;

  inManualMove = YES;

  initialMoveLocation = [self convertBaseToScreen:[self mouseLocationOutsideOfEventStream]];
  initialMoveLocation.x -= frame.origin.x;
  initialMoveLocation.y -= frame.origin.y;
}

- (BOOL)trackManualResize
{
  NSPoint currentLocation;
  NSRect newFrame;
  float dx, dy;
  NSSize min_size;

  if (!inManualResize || inTrackManualResize)
    return NO;

  inTrackManualResize = YES;

  currentLocation = [self convertBaseToScreen:[self mouseLocationOutsideOfEventStream]];
  currentLocation.x -= initialResizeFrame.origin.x;
  currentLocation.y -= initialResizeFrame.origin.y;

  dx = currentLocation.x - initialResizeLocation.x;
  dy = -(currentLocation.y - initialResizeLocation.y);

  newFrame = initialResizeFrame;
  newFrame.size.width = initialResizeFrame.size.width + dx;
  newFrame.size.height = initialResizeFrame.size.height + dy;

  min_size = [self contentMinSize];
  if (newFrame.size.width < min_size.width)
    newFrame.size.width = min_size.width;
  if (newFrame.size.height < min_size.height)
    newFrame.size.height = min_size.height;

  /* We could also apply aspect ratio:
     newFrame.size.height = newFrame.size.width / [self aspectRatio].width * [self aspectRatio].height;
  */

  dy = newFrame.size.height - initialResizeFrame.size.height;

  newFrame.origin.x = initialResizeFrame.origin.x;
  newFrame.origin.y = initialResizeFrame.origin.y - dy;

  [self setFrame:newFrame display:YES];

  /* Let the resizing be handled by GTK+. */
  if (g_main_context_pending (NULL))
    g_main_context_iteration (NULL, FALSE);

  inTrackManualResize = NO;

  return YES;
}

-(BOOL)isInManualResize
{
  return inManualResize;
}

-(void)beginManualResize
{
  if (inMove || inManualMove || inManualResize)
    return;

  inManualResize = YES;

  initialResizeFrame = [self frame];
  initialResizeLocation = [self convertBaseToScreen:[self mouseLocationOutsideOfEventStream]];
  initialResizeLocation.x -= initialResizeFrame.origin.x;
  initialResizeLocation.y -= initialResizeFrame.origin.y;
}



static GdkDragContext *current_context = NULL;

static GdkDragAction
drag_operation_to_drag_action (NSDragOperation operation)
{
  GdkDragAction result = 0;

  /* GDK and Quartz drag operations do not map 1:1.
   * This mapping represents about the best that we
   * can come up.
   *
   * Note that NSDragOperationPrivate and GDK_ACTION_PRIVATE
   * have almost opposite meanings: the GDK one means that the
   * destination is solely responsible for the action; the Quartz
   * one means that the source and destination will agree
   * privately on the action. NSOperationGeneric is close in meaning
   * to GDK_ACTION_PRIVATE but there is a problem: it will be
   * sent for any ordinary drag, and likely not understood
   * by any intra-widget drag (since the source & dest are the
   * same).
   */

  if (operation & NSDragOperationGeneric)
    result |= GDK_ACTION_MOVE;
  if (operation & NSDragOperationCopy)
    result |= GDK_ACTION_COPY;
  if (operation & NSDragOperationMove)
    result |= GDK_ACTION_MOVE;
  if (operation & NSDragOperationLink)
    result |= GDK_ACTION_LINK;

  return result;
}

static NSDragOperation
drag_action_to_drag_operation (GdkDragAction action)
{
  NSDragOperation result = 0;

  if (action & GDK_ACTION_COPY)
    result |= NSDragOperationCopy;
  if (action & GDK_ACTION_LINK)
    result |= NSDragOperationLink;
  if (action & GDK_ACTION_MOVE)
    result |= NSDragOperationMove;

  return result;
}

static void
update_context_from_dragging_info (id <NSDraggingInfo> sender)
{
  g_assert (current_context != NULL);

  GDK_DRAG_CONTEXT_PRIVATE (current_context)->dragging_info = sender;
  current_context->suggested_action = drag_operation_to_drag_action ([sender draggingSourceOperationMask]);
  current_context->actions = current_context->suggested_action;
}

- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender
{
  GdkEvent event;

  if (current_context)
    g_object_unref (current_context);
  
  current_context = gdk_drag_context_new ();
  update_context_from_dragging_info (sender);

  event.dnd.type = GDK_DRAG_ENTER;
  event.dnd.window = g_object_ref ([[self contentView] gdkWindow]);
  event.dnd.send_event = FALSE;
  event.dnd.context = current_context;
  event.dnd.time = GDK_CURRENT_TIME;

  (*_gdk_event_func) (&event, _gdk_event_data);

  return NSDragOperationNone;
}

- (void)draggingEnded:(id <NSDraggingInfo>)sender
{
  /* leave a note for the source about what action was taken */
  if (_gdk_quartz_drag_source_context && current_context)
   _gdk_quartz_drag_source_context->action = current_context->action;

  if (current_context)
    g_object_unref (current_context);
  current_context = NULL;
}

- (void)draggingExited:(id <NSDraggingInfo>)sender
{
  GdkEvent event;
  
  event.dnd.type = GDK_DRAG_LEAVE;
  event.dnd.window = g_object_ref ([[self contentView] gdkWindow]);
  event.dnd.send_event = FALSE;
  event.dnd.context = current_context;
  event.dnd.time = GDK_CURRENT_TIME;

  (*_gdk_event_func) (&event, _gdk_event_data);
  
  g_object_unref (current_context);
  current_context = NULL;
}

- (NSDragOperation)draggingUpdated:(id <NSDraggingInfo>)sender
{
  NSPoint point = [sender draggingLocation];
  NSPoint screen_point = [self convertBaseToScreen:point];
  GdkEvent event;
  int gx, gy;

  update_context_from_dragging_info (sender);
  _gdk_quartz_window_nspoint_to_gdk_xy (screen_point, &gx, &gy);

  event.dnd.type = GDK_DRAG_MOTION;
  event.dnd.window = g_object_ref ([[self contentView] gdkWindow]);
  event.dnd.send_event = FALSE;
  event.dnd.context = current_context;
  event.dnd.time = GDK_CURRENT_TIME;
  event.dnd.x_root = gx;
  event.dnd.y_root = gy;

  (*_gdk_event_func) (&event, _gdk_event_data);

  g_object_unref (event.dnd.window);

  return drag_action_to_drag_operation (current_context->action);
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
  NSPoint point = [sender draggingLocation];
  NSPoint screen_point = [self convertBaseToScreen:point];
  GdkEvent event;
  int gy, gx;

  update_context_from_dragging_info (sender);
  _gdk_quartz_window_nspoint_to_gdk_xy (screen_point, &gx, &gy);

  event.dnd.type = GDK_DROP_START;
  event.dnd.window = g_object_ref ([[self contentView] gdkWindow]);
  event.dnd.send_event = FALSE;
  event.dnd.context = current_context;
  event.dnd.time = GDK_CURRENT_TIME;
  event.dnd.x_root = gx;
  event.dnd.y_root = gy;

  (*_gdk_event_func) (&event, _gdk_event_data);

  g_object_unref (event.dnd.window);

  g_object_unref (current_context);
  current_context = NULL;

  return YES;
}

- (BOOL)wantsPeriodicDraggingUpdates
{
  return NO;
}

- (void)draggedImage:(NSImage *)anImage endedAt:(NSPoint)aPoint operation:(NSDragOperation)operation
{
  GdkEvent event;
  GdkScreen *screen;

  g_assert (_gdk_quartz_drag_source_context != NULL);

  event.dnd.type = GDK_DROP_FINISHED;
  event.dnd.window = g_object_ref ([[self contentView] gdkWindow]);
  event.dnd.send_event = FALSE;
  event.dnd.context = _gdk_quartz_drag_source_context;

  screen = gdk_window_get_screen (event.dnd.window);

  if (screen)
    {
      GList* windows, *list;
      gint gx, gy;

      event.dnd.context->dest_window = NULL;

      windows = gdk_screen_get_toplevel_windows (screen);
      _gdk_quartz_window_nspoint_to_gdk_xy (aPoint, &gx, &gy);

      for (list = windows; list; list = list->next) 
        {
          GdkWindow* win = (GdkWindow*) list->data;
          gint wx, wy;
          gint ww, wh;

          gdk_window_get_root_origin (win, &wx, &wy);
          ww = gdk_window_get_width (win);
          wh = gdk_window_get_height (win);

          if (gx > wx && gy > wy && gx <= wx + ww && gy <= wy + wh)
            event.dnd.context->dest_window = win;
        }
    }

  (*_gdk_event_func) (&event, _gdk_event_data);

  g_object_unref (event.dnd.window);

  g_object_unref (_gdk_quartz_drag_source_context);
  _gdk_quartz_drag_source_context = NULL;
}

@end
