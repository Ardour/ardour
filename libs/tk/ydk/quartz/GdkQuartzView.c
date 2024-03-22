/* GdkQuartzView.m
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

#import "GdkQuartzView.h"
#include "gdkregion.h"
#include "gdkregion-generic.h"
#include "gdkwindow-quartz.h"
#include "gdkprivate-quartz.h"
#include "gdkquartz.h"

@implementation GdkQuartzView

-(id)initWithFrame: (NSRect)frameRect
{
  if ((self = [super initWithFrame: frameRect]))
    {
      markedRange = NSMakeRange (NSNotFound, 0);
      selectedRange = NSMakeRange (NSNotFound, 0);
    }

  return self;
}

-(BOOL)acceptsFirstResponder
{
  GDK_NOTE (EVENTS, g_print ("acceptsFirstResponder\n"));
  return YES;
}

-(BOOL)becomeFirstResponder
{
  GDK_NOTE (EVENTS, g_print ("becomeFirstResponder\n"));
  return YES;
}

-(BOOL)resignFirstResponder
{
  GDK_NOTE (EVENTS, g_print ("resignFirstResponder\n"));
  return YES;
}

-(void) keyDown: (NSEvent *) theEvent
{
  GDK_NOTE (EVENTS, g_print ("keyDown\n"));
  [self interpretKeyEvents: [NSArray arrayWithObject: theEvent]];
}

-(void)flagsChanged: (NSEvent *) theEvent
{
}

-(NSUInteger)characterIndexForPoint: (NSPoint)aPoint
{
  GDK_NOTE (EVENTS, g_print ("characterIndexForPoint\n"));
  return 0;
}

-(NSRect)firstRectForCharacterRange: (NSRange)aRange actualRange: (NSRangePointer)actualRange
{
  GDK_NOTE (EVENTS, g_print ("firstRectForCharacterRange\n"));
  gint ns_x, ns_y;
  GdkRectangle *rect;

  rect = g_object_get_data (G_OBJECT (gdk_window), GIC_CURSOR_RECT);
  if (rect)
    {
      _gdk_quartz_window_gdk_xy_to_xy (rect->x, rect->y + rect->height,
				       &ns_x, &ns_y);

      return NSMakeRect (ns_x, ns_y, rect->width, rect->height);
    }
  else
    {
      return NSMakeRect (0, 0, 0, 0);
    }
}

-(NSArray *)validAttributesForMarkedText
{
  GDK_NOTE (EVENTS, g_print ("validAttributesForMarkedText\n"));
  return [NSArray arrayWithObjects: NSUnderlineStyleAttributeName, nil];
}

-(NSAttributedString *)attributedSubstringForProposedRange: (NSRange)aRange actualRange: (NSRangePointer)actualRange
{
  GDK_NOTE (EVENTS, g_print ("attributedSubstringForProposedRange\n"));
  return nil;
}

-(BOOL)hasMarkedText
{
  GDK_NOTE (EVENTS, g_print ("hasMarkedText\n"));
  return markedRange.location != NSNotFound && markedRange.length != 0;
}

-(NSRange)markedRange
{
  GDK_NOTE (EVENTS, g_print ("markedRange\n"));
  return markedRange;
}

-(NSRange)selectedRange
{
  GDK_NOTE (EVENTS, g_print ("selectedRange\n"));
  return selectedRange;
}

-(void)unmarkText
{
  GDK_NOTE (EVENTS, g_print ("unmarkText\n"));
  gchar *prev_str;
  markedRange = selectedRange = NSMakeRange (NSNotFound, 0);

  prev_str = g_object_get_data (G_OBJECT (gdk_window), TIC_MARKED_TEXT);
  if (prev_str)
    g_free (prev_str);
  g_object_set_data (G_OBJECT (gdk_window), TIC_MARKED_TEXT, NULL);
}

-(void)setMarkedText: (id)aString selectedRange: (NSRange)newSelection replacementRange: (NSRange)replacementRange
{
  GDK_NOTE (EVENTS, g_print ("setMarkedText\n"));
  const char *str;
  gchar *prev_str;

  if (replacementRange.location == NSNotFound)
    {
      markedRange = NSMakeRange (newSelection.location, [aString length]);
      selectedRange = NSMakeRange (newSelection.location, newSelection.length);
    }
  else {
      markedRange = NSMakeRange (replacementRange.location, [aString length]);
      selectedRange = NSMakeRange (replacementRange.location + newSelection.location, newSelection.length);
    }

  if ([aString isKindOfClass: [NSAttributedString class]])
    {
      str = [[aString string] UTF8String];
    }
  else {
      str = [aString UTF8String];
    }

  prev_str = g_object_get_data (G_OBJECT (gdk_window), TIC_MARKED_TEXT);
  if (prev_str)
    g_free (prev_str);
  g_object_set_data (G_OBJECT (gdk_window), TIC_MARKED_TEXT, g_strdup (str));
  g_object_set_data (G_OBJECT (gdk_window), TIC_SELECTED_POS,
		     GUINT_TO_POINTER (selectedRange.location));
  g_object_set_data (G_OBJECT (gdk_window), TIC_SELECTED_LEN,
		     GUINT_TO_POINTER (selectedRange.length));

  GDK_NOTE (EVENTS, g_print ("setMarkedText: set %s (%p, nsview %p): %s\n",
			     TIC_MARKED_TEXT, gdk_window, self,
			     str ? str : "(empty)"));

  /* handle text input changes by mouse events */
  if (!GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (gdk_window),
                                            TIC_IN_KEY_DOWN)))
    {
      _gdk_quartz_synthesize_null_key_event(gdk_window);
    }
}

-(void)doCommandBySelector: (SEL)aSelector
{
  GDK_NOTE (EVENTS, g_print ("doCommandBySelector\n"));
  if ([self respondsToSelector: aSelector])
    [self performSelector: aSelector];
}

/* This gets called on OS X 10.6 and upwards from interpretKeyEvents */
-(void)insertText: (id)aString replacementRange: (NSRange)replacementRange
{
  [self insertText:aString];
}

/* This gets called on OS X 10.5 from interpretKeyEvents, although 10.5
 * is supposed to support NSTextInputClient  */
-(void)insertText: (id)aString
{
  GDK_NOTE (EVENTS, g_print ("insertText\n"));
  const char *str;
  NSString *string;
  gchar *prev_str;

  if ([self hasMarkedText])
    [self unmarkText];

  if ([aString isKindOfClass: [NSAttributedString class]])
      string = [aString string];
  else
      string = aString;

  NSCharacterSet *ctrlChars = [NSCharacterSet controlCharacterSet];
  NSCharacterSet *wsnlChars = [NSCharacterSet whitespaceAndNewlineCharacterSet];
  if ([string rangeOfCharacterFromSet:ctrlChars].length &&
      [string rangeOfCharacterFromSet:wsnlChars].length == 0)
    {
      /* discard invalid text input with Chinese input methods */
      str = "";
      [self unmarkText];
      NSInputManager *currentInputManager = [NSInputManager currentInputManager];
      [currentInputManager markedTextAbandoned:self];
    }
  else
   {
      str = [string UTF8String];
   }

  prev_str = g_object_get_data (G_OBJECT (gdk_window), TIC_INSERT_TEXT);
  if (prev_str)
    g_free (prev_str);
  g_object_set_data (G_OBJECT (gdk_window), TIC_INSERT_TEXT, g_strdup (str));
  GDK_NOTE (EVENTS, g_print ("insertText: set %s (%p, nsview %p): %s\n",
			     TIC_INSERT_TEXT, gdk_window, self,
			     str ? str : "(empty)"));

  g_object_set_data (G_OBJECT (gdk_window), GIC_FILTER_KEY,
		     GUINT_TO_POINTER (GIC_FILTER_FILTERED));

  /* handle text input changes by mouse events */
  if (!GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (gdk_window),
                                            TIC_IN_KEY_DOWN)))
    {
      _gdk_quartz_synthesize_null_key_event(gdk_window);
    }
}

-(void)deleteBackward: (id)sender
{
  GDK_NOTE (EVENTS, g_print ("deleteBackward\n"));
  g_object_set_data (G_OBJECT (gdk_window), GIC_FILTER_KEY,
		     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)deleteForward: (id)sender
{
  GDK_NOTE (EVENTS, g_print ("deleteForward\n"));
  g_object_set_data (G_OBJECT (gdk_window), GIC_FILTER_KEY,
		     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)deleteToBeginningOfLine: (id)sender
{
  GDK_NOTE (EVENTS, g_print ("deleteToBeginningOfLine\n"));
  g_object_set_data (G_OBJECT (gdk_window), GIC_FILTER_KEY,
		     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)deleteToEndOfLine: (id)sender
{
  GDK_NOTE (EVENTS, g_print ("deleteToEndOfLine\n"));
  g_object_set_data (G_OBJECT (gdk_window), GIC_FILTER_KEY,
		     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)deleteWordBackward: (id)sender
{
  GDK_NOTE (EVENTS, g_print ("deleteWordBackward\n"));
  g_object_set_data (G_OBJECT (gdk_window), GIC_FILTER_KEY,
		     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)deleteWordForward: (id)sender
{
  GDK_NOTE (EVENTS, g_print ("deleteWordForward\n"));
  g_object_set_data (G_OBJECT (gdk_window), GIC_FILTER_KEY,
		     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)insertBacktab: (id)sender
{
  GDK_NOTE (EVENTS, g_print ("insertBacktab\n"));
  g_object_set_data (G_OBJECT (gdk_window), GIC_FILTER_KEY,
		     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)insertNewline: (id)sender
{
  GDK_NOTE (EVENTS, g_print ("insertNewline\n"));
  g_object_set_data (G_OBJECT (gdk_window), GIC_FILTER_KEY, GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)insertTab: (id)sender
{
  GDK_NOTE (EVENTS, g_print ("insertTab\n"));
  g_object_set_data (G_OBJECT (gdk_window), GIC_FILTER_KEY,
		     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveBackward: (id)sender
{
  GDK_NOTE (EVENTS, g_print ("moveBackward\n"));
  g_object_set_data (G_OBJECT (gdk_window), GIC_FILTER_KEY,
		     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveBackwardAndModifySelection: (id)sender
{
  GDK_NOTE (EVENTS, g_print ("moveBackwardAndModifySelection\n"));
  g_object_set_data (G_OBJECT (gdk_window), GIC_FILTER_KEY,
		     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveDown: (id)sender
{
  GDK_NOTE (EVENTS, g_print ("moveDown\n"));
  g_object_set_data (G_OBJECT (gdk_window), GIC_FILTER_KEY,
		     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveDownAndModifySelection: (id)sender
{
  GDK_NOTE (EVENTS, g_print ("moveDownAndModifySelection\n"));
  g_object_set_data (G_OBJECT (gdk_window), GIC_FILTER_KEY,
		     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveForward: (id)sender
{
  GDK_NOTE (EVENTS, g_print ("moveForward\n"));
  g_object_set_data (G_OBJECT (gdk_window), GIC_FILTER_KEY,
		     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveForwardAndModifySelection: (id)sender
{
  GDK_NOTE (EVENTS, g_print ("moveForwardAndModifySelection\n"));
  g_object_set_data (G_OBJECT (gdk_window), GIC_FILTER_KEY,
		     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveLeft: (id)sender
{
  GDK_NOTE (EVENTS, g_print ("moveLeft\n"));
  g_object_set_data (G_OBJECT (gdk_window), GIC_FILTER_KEY,
		     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveLeftAndModifySelection: (id)sender
{
  GDK_NOTE (EVENTS, g_print ("moveLeftAndModifySelection\n"));
  g_object_set_data (G_OBJECT (gdk_window), GIC_FILTER_KEY,
		     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveRight: (id)sender
{
  GDK_NOTE (EVENTS, g_print ("moveRight\n"));
  g_object_set_data (G_OBJECT (gdk_window), GIC_FILTER_KEY,
		     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveRightAndModifySelection: (id)sender
{
  GDK_NOTE (EVENTS, g_print ("moveRightAndModifySelection\n"));
  g_object_set_data (G_OBJECT (gdk_window), GIC_FILTER_KEY,
		     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveToBeginningOfDocument: (id)sender
{
  GDK_NOTE (EVENTS, g_print ("moveToBeginningOfDocument\n"));
  g_object_set_data (G_OBJECT (gdk_window), GIC_FILTER_KEY,
		     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveToBeginningOfDocumentAndModifySelection: (id)sender
{
  GDK_NOTE (EVENTS, g_print ("moveToBeginningOfDocumentAndModifySelection\n"));
  g_object_set_data (G_OBJECT (gdk_window), GIC_FILTER_KEY,
		     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveToBeginningOfLine: (id)sender
{
  GDK_NOTE (EVENTS, g_print ("moveToBeginningOfLine\n"));
  g_object_set_data (G_OBJECT (gdk_window), GIC_FILTER_KEY,
		     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveToBeginningOfLineAndModifySelection: (id)sender
{
  GDK_NOTE (EVENTS, g_print ("moveToBeginningOfLineAndModifySelection\n"));
  g_object_set_data (G_OBJECT (gdk_window), GIC_FILTER_KEY,
		     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveToEndOfDocument: (id)sender
{
  GDK_NOTE (EVENTS, g_print ("moveToEndOfDocument\n"));
  g_object_set_data (G_OBJECT (gdk_window), GIC_FILTER_KEY,
		     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveToEndOfDocumentAndModifySelection: (id)sender
{
  GDK_NOTE (EVENTS, g_print ("moveToEndOfDocumentAndModifySelection\n"));
  g_object_set_data (G_OBJECT (gdk_window), GIC_FILTER_KEY,
		     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveToEndOfLine: (id)sender
{
  GDK_NOTE (EVENTS, g_print ("moveToEndOfLine\n"));
  g_object_set_data (G_OBJECT (gdk_window), GIC_FILTER_KEY,
		     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveToEndOfLineAndModifySelection: (id)sender
{
  GDK_NOTE (EVENTS, g_print ("moveToEndOfLineAndModifySelection\n"));
  g_object_set_data (G_OBJECT (gdk_window), GIC_FILTER_KEY,
		     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveUp: (id)sender
{
  GDK_NOTE (EVENTS, g_print ("moveUp\n"));
  g_object_set_data (G_OBJECT (gdk_window), GIC_FILTER_KEY,
		     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveUpAndModifySelection: (id)sender
{
  GDK_NOTE (EVENTS, g_print ("moveUpAndModifySelection\n"));
  g_object_set_data (G_OBJECT (gdk_window), GIC_FILTER_KEY,
		     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveWordBackward: (id)sender
{
  GDK_NOTE (EVENTS, g_print ("moveWordBackward\n"));
  g_object_set_data (G_OBJECT (gdk_window), GIC_FILTER_KEY,
		     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveWordBackwardAndModifySelection: (id)sender
{
  GDK_NOTE (EVENTS, g_print ("moveWordBackwardAndModifySelection\n"));
  g_object_set_data (G_OBJECT (gdk_window), GIC_FILTER_KEY,
		     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveWordForward: (id)sender
{
  GDK_NOTE (EVENTS, g_print ("moveWordForward\n"));
  g_object_set_data (G_OBJECT (gdk_window), GIC_FILTER_KEY,
		     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveWordForwardAndModifySelection: (id)sender
{
  GDK_NOTE (EVENTS, g_print ("moveWordForwardAndModifySelection\n"));
  g_object_set_data (G_OBJECT (gdk_window), GIC_FILTER_KEY,
		     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveWordLeft: (id)sender
{
  GDK_NOTE (EVENTS, g_print ("moveWordLeft\n"));
  g_object_set_data (G_OBJECT (gdk_window), GIC_FILTER_KEY,
		     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveWordLeftAndModifySelection: (id)sender
{
  GDK_NOTE (EVENTS, g_print ("moveWordLeftAndModifySelection\n"));
  g_object_set_data (G_OBJECT (gdk_window), GIC_FILTER_KEY,
		     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveWordRight: (id)sender
{
  GDK_NOTE (EVENTS, g_print ("moveWordRight\n"));
  g_object_set_data (G_OBJECT (gdk_window), GIC_FILTER_KEY,
		     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveWordRightAndModifySelection: (id)sender
{
  GDK_NOTE (EVENTS, g_print ("moveWordRightAndModifySelection\n"));
  g_object_set_data (G_OBJECT (gdk_window), GIC_FILTER_KEY,
		     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)pageDown: (id)sender
{
  GDK_NOTE (EVENTS, g_print ("pageDown\n"));
  g_object_set_data (G_OBJECT (gdk_window), GIC_FILTER_KEY,
		     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)pageDownAndModifySelection: (id)sender
{
  GDK_NOTE (EVENTS, g_print ("pageDownAndModifySelection\n"));
  g_object_set_data (G_OBJECT (gdk_window), GIC_FILTER_KEY,
		     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)pageUp: (id)sender
{
  GDK_NOTE (EVENTS, g_print ("pageUp\n"));
  g_object_set_data (G_OBJECT (gdk_window), GIC_FILTER_KEY,
		     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)pageUpAndModifySelection: (id)sender
{
  GDK_NOTE (EVENTS, g_print ("pageUpAndModifySelection\n"));
  g_object_set_data (G_OBJECT (gdk_window), GIC_FILTER_KEY,
		     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)selectAll: (id)sender
{
  GDK_NOTE (EVENTS, g_print ("selectAll\n"));
  g_object_set_data (G_OBJECT (gdk_window), GIC_FILTER_KEY,
		     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)selectLine: (id)sender
{
  GDK_NOTE (EVENTS, g_print ("selectLine\n"));
  g_object_set_data (G_OBJECT (gdk_window), GIC_FILTER_KEY,
		     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)selectWord: (id)sender
{
  GDK_NOTE (EVENTS, g_print ("selectWord\n"));
  g_object_set_data (G_OBJECT (gdk_window), GIC_FILTER_KEY,
		     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)noop: (id)sender
{
  GDK_NOTE (EVENTS, g_print ("noop\n"));
}

/* --------------------------------------------------------------- */

-(void)dealloc
{
  if (trackingRect)
    {
      [self removeTrackingRect: trackingRect];
#if MAC_OS_X_VERSION_MIN_REQUIRED >= 10500
      [(NSTrackingArea*)trackingRect release];
#endif
      trackingRect = 0;
    }

  [super dealloc];
}

-(void)setGdkWindow: (GdkWindow *)window
{
  gdk_window = window;
}

-(GdkWindow *)gdkWindow
{
  return gdk_window;
}

-(NSTrackingRectTag)trackingRect
{
  return trackingRect;
}

-(BOOL)isFlipped
{
  return YES;
}

-(BOOL)isOpaque
{
  if (GDK_WINDOW_DESTROYED (gdk_window))
    return YES;

  /* A view is opaque if its GdkWindow doesn't have the RGBA colormap */
  return gdk_drawable_get_colormap (gdk_window) !=
    gdk_screen_get_rgba_colormap (_gdk_screen);
}

- (void) setNeedsDisplay:(BOOL)yn
{
  GdkWindowObject *private;
  GdkWindowImplQuartz *impl;
  NSRect nsrect = [self bounds];
  GdkRectangle r = { nsrect.origin.x, nsrect.origin.y, nsrect.size.width, nsrect.size.height };
  
  private = GDK_WINDOW_OBJECT (gdk_window);
  impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);

  GDK_NOTE (EVENTS, g_print ("setNeedsDisplay, current NDR %p\n", impl->needs_display_region));

  if (!impl->needs_display_region)
    impl->needs_display_region = gdk_region_rectangle (&r);
  else 
    gdk_region_union_with_rect (impl->needs_display_region, &r);

#if 0
  gint nrects;
  GdkRectangle* rects;

  gdk_region_get_rectangles (impl->needs_display_region, &rects, &nrects);
  printf ("SND %p INVALID with %d rects\n", impl, nrects);
  for (gint n = 0; n < nrects; ++n) {
    printf ("\t%d,%d %d x %d\n", rects[n].x, rects[n].y, rects[n].width, rects[n].height);
  }
#endif

  [super setNeedsDisplay:yn];
}

- (void) setNeedsDisplayInRect:(NSRect)rect
{
  GdkWindowObject *private;
  GdkWindowImplQuartz *impl;
  GdkRectangle r = { rect.origin.x, rect.origin.y, rect.size.width, rect.size.height };

  if (r.width >= 2147483647 || r.height >= 2147483647) {
    NSRect bounds = [self bounds];
    r.x = 0;
    r.y = 0;
    r.width = bounds.size.width;
    r.height = bounds.size.height;
  }
  
  private = GDK_WINDOW_OBJECT (gdk_window);
  impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);

  GDK_NOTE (EVENTS, g_print ("setNeedsDisplayInRect, current NDR %p\n", impl->needs_display_region));

  if (!impl->needs_display_region)
    impl->needs_display_region = gdk_region_rectangle (&r);
  else 
    gdk_region_union_with_rect (impl->needs_display_region, &r);

#if 0
  gint nrects;
  GdkRectangle* rects;

  gdk_region_get_rectangles (impl->needs_display_region, &rects, &nrects);
  printf ("SNDiR %p INVALID with %d rects\n", impl, nrects);
  for (gint n = 0; n < nrects; ++n) {
    printf ("\t%d,%d %d x %d\n", rects[n].x, rects[n].y, rects[n].width, rects[n].height);
  }
#endif

  
  [super setNeedsDisplayInRect:rect];
}

-(void)drawRect: (NSRect)rect
{
  GdkRectangle gdk_rect;
  GdkWindowObject *private = GDK_WINDOW_OBJECT (gdk_window);
  GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);
  const NSRect *drawn_rects;
  NSInteger count;
  int i;
  GdkRegion *region;

  if (GDK_WINDOW_DESTROYED (gdk_window))
    return;

  if (! (private->event_mask & GDK_EXPOSURE_MASK))
    return;

  if (NSEqualRects (rect, NSZeroRect))
    return;

  if (!GDK_WINDOW_IS_MAPPED (gdk_window) && ((gdk_quartz_osx_version() >= GDK_OSX_LEOPARD) && [self wantsLayer]))
    {
      /* If the window is not yet mapped, clip_region_with_children
       * will be empty causing the usual code below to draw nothing.
       * To not see garbage on the screen, we draw an aesthetic color
       * here. The garbage would be visible if any widget enabled
       * the NSView's CALayer in order to add sublayers for custom
       * native rendering.
       */
      [NSGraphicsContext saveGraphicsState];

      if (impl->background_color_set)
        [[NSColor colorWithDeviceRed:impl->background_color.red / 65535.0
                               green:impl->background_color.green / 65535.0
                                blue:impl->background_color.blue / 65535.0
                               alpha:1.0]
         setFill];
      else
        [[NSColor windowBackgroundColor] setFill];
      [NSBezierPath fillRect: rect];

      [NSGraphicsContext restoreGraphicsState];

      return;
    }

  if (!impl->needs_display_region || gdk_quartz_get_use_cocoa_invalidation()) {
    [self getRectsBeingDrawn: &drawn_rects count: &count];
    region = gdk_region_new ();

    for (i = 0; i < count; i++)
      {
	gdk_rect.x = drawn_rects[i].origin.x;
	gdk_rect.y = drawn_rects[i].origin.y;
	gdk_rect.width = drawn_rects[i].size.width;
	gdk_rect.height = drawn_rects[i].size.height;

	gdk_region_union_with_rect (region, &gdk_rect);
    }
  } else {
    region = impl->needs_display_region;
  }

#if 0
  gint nrects;
  GdkRectangle* rects;

  gdk_region_get_rectangles (region, &rects, &nrects);
  printf ("%p drawRect with %d rects\n", impl, nrects);
  for (gint n = 0; n < nrects; ++n) {
    printf ("\t%d,%d %d x %d\n", rects[n].x, rects[n].y, rects[n].width, rects[n].height);
  }
#endif
  
  impl->in_paint_rect_count++;
  /* this essentially generates an expose event */
  _gdk_window_process_updates_recurse (gdk_window, region);
  impl->in_paint_rect_count--;

  if (impl->needs_display_region)
    {
      impl->needs_display_region = NULL;
    }
  
  gdk_region_destroy (region);

  if (needsInvalidateShadow)
    {
      [[self window] invalidateShadow];
      needsInvalidateShadow = NO;
    }
}

- (void) reshape
{
  GdkWindowObject *private;
  GdkWindowImplQuartz *impl;
  GdkRectangle r;

  GDK_NOTE (EVENTS, g_print ("reshape\n"));

  [super reshape];

  NSRect bounds = [self bounds];

  private = GDK_WINDOW_OBJECT (gdk_window);
  impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);

  if (impl->needs_display_region) {
    gdk_region_destroy (impl->needs_display_region);
  }

  r.x = 0;
  r.y = 0;
  r.width = bounds.size.width;
  r.height = bounds.size.height;

  if (impl->needs_display_region)
    gdk_region_destroy (impl->needs_display_region);
  
  impl->needs_display_region = gdk_region_rectangle (&r);

#if 0
  gint nrects;
  GdkRectangle* rects;

  gdk_region_get_rectangles (impl->needs_display_region, &rects, &nrects);
  printf ("RS %p INVALID with %d rects\n", impl, nrects);
  for (gint n = 0; n < nrects; ++n) {
    printf ("\t%d,%d %d x %d\n", rects[n].x, rects[n].y, rects[n].width, rects[n].height);
  }
#endif

}

-(void)setNeedsInvalidateShadow: (BOOL)invalidate
{
  needsInvalidateShadow = invalidate;
}

/* For information on setting up tracking rects properly, see here:
 * http://developer.apple.com/documentation/Cocoa/Conceptual/EventOverview/EventOverview.pdf
 */
-(void)updateTrackingRect
{
  GdkWindowObject *private = GDK_WINDOW_OBJECT (gdk_window);
  GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);
  NSRect rect;

#if MAC_OS_X_VERSION_MIN_REQUIRED >= 10500
  NSTrackingArea *trackingArea;
#endif

  if (!impl || !impl->toplevel)
    return;

  if (trackingRect)
    {
      [self removeTrackingRect: trackingRect];
#if MAC_OS_X_VERSION_MIN_REQUIRED >= 10500
      [(NSTrackingArea*)trackingRect release];
#endif
      trackingRect = 0;
    }

  if (!impl->toplevel)
    return;

  /* Note, if we want to set assumeInside we can use:
   * NSPointInRect ([[self window] convertScreenToBase:[NSEvent mouseLocation]], rect)
   */

  rect = [self bounds];

#if MAC_OS_X_VERSION_MIN_REQUIRED >= 10500
  trackingArea = [[NSTrackingArea alloc] initWithRect: rect
                  options: NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved | NSTrackingCursorUpdate | NSTrackingActiveInActiveApp | NSTrackingInVisibleRect | NSTrackingEnabledDuringMouseDrag
                  owner: self
                  userInfo: nil];
  [self addTrackingArea: trackingArea];
  trackingRect = (NSInteger)[trackingArea retain];
#else
  trackingRect = [self addTrackingRect: rect
		  owner: self
		  userData: nil
		  assumeInside: NO];
#endif
}

-(void)viewDidMoveToWindow
{
  if (![self window]) /* We are destroyed already */
    return;

  [self updateTrackingRect];
}

-(void)viewWillMoveToWindow: (NSWindow *)newWindow
{
  if (newWindow == nil && trackingRect)
    {
      [self removeTrackingRect: trackingRect];
#if MAC_OS_X_VERSION_MIN_REQUIRED >= 10500
			[(NSTrackingArea*)trackingRect release];
#endif

      trackingRect = 0;
    }
}

-(void)setFrame: (NSRect)frame
{
  [super setFrame: frame];

  if ([self window])
    [self updateTrackingRect];
}

@end
