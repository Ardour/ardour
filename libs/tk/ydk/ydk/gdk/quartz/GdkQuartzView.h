/* GdkQuartzView.h
 *
 * Copyright (C) 2005 Imendio AB
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

#import <AppKit/AppKit.h>
#include "gdk/gdk.h"

/* Text Input Client */
#define TIC_MARKED_TEXT "tic-marked-text"
#define TIC_SELECTED_POS  "tic-selected-pos"
#define TIC_SELECTED_LEN  "tic-selected-len"
#define TIC_INSERT_TEXT "tic-insert-text"
#define TIC_IN_KEY_DOWN "tic-in-key-down"

/* GtkIMContext */
#define GIC_CURSOR_RECT  "gic-cursor-rect"
#define GIC_FILTER_KEY   "gic-filter-key"
#define GIC_FILTER_PASSTHRU	0
#define GIC_FILTER_FILTERED	1

@interface GdkQuartzView : NSView <NSTextInputClient>
{
  GdkWindow *gdk_window;
  NSTrackingRectTag trackingRect;
  BOOL needsInvalidateShadow;
  NSRange markedRange;
  NSRange selectedRange;
}

- (void)setGdkWindow: (GdkWindow *)window;
- (GdkWindow *)gdkWindow;
- (NSTrackingRectTag)trackingRect;
- (void)setNeedsInvalidateShadow: (BOOL)invalidate;

@end
