/* GdkQuartzWindow.h
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

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#include <glib.h>

@interface GdkQuartzWindow : NSWindow {
  BOOL    inMove;
  BOOL    inShowOrHide;
  BOOL    initialPositionKnown;

  /* Manually triggered move/resize (not by the window manager) */
  BOOL    inManualMove;
  BOOL    inManualResize;
  BOOL    inTrackManualResize;
  NSPoint initialMoveLocation;
  NSPoint initialResizeLocation;
  NSRect  initialResizeFrame;
}

-(BOOL)isInMove;
-(void)beginManualMove;
-(BOOL)trackManualMove;
-(BOOL)isInManualResize;
-(void)beginManualResize;
-(BOOL)trackManualResize;
-(void)showAndMakeKey:(BOOL)makeKey;
-(void)hide;

@end




