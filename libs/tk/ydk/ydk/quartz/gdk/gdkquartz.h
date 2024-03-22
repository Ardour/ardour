/* gdkquartz.h
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

#ifndef __GDK_QUARTZ_H__
#define __GDK_QUARTZ_H__

#include <AppKit/AppKit.h>
#include <gdk/gdkprivate.h>

G_BEGIN_DECLS

/* NSInteger only exists in Leopard and newer.  This check has to be
 * done after inclusion of the system headers.  If NSInteger has not
 * been defined, we know for sure that we are on 32-bit.
 */
#ifndef NSINTEGER_DEFINED
typedef int NSInteger;
typedef unsigned int NSUInteger;
#endif

#ifndef CGFLOAT_DEFINED
typedef float CGFloat;
#endif

typedef enum
{
  GDK_OSX_UNSUPPORTED = 0,
  GDK_OSX_MIN = 4,
  GDK_OSX_TIGER = 4,
  GDK_OSX_LEOPARD = 5,
  GDK_OSX_SNOW_LEOPARD = 6,
  GDK_OSX_LION = 7,
  GDK_OSX_MOUNTAIN_LION = 8,
  GDK_OSX_MAVERICKS = 9,
  GDK_OSX_YOSEMITE = 10,
  GDK_OSX_EL_CAPITAN = 11,
  GDK_OSX_SIERRA = 12,
  GDK_OSX_HIGH_SIERRA = 13,
  GDK_OSX_MOJAVE = 14,
  GDK_OSX_CATALINA = 15,
  GDK_OSX_BIGSUR = 16,
  GDK_OSX_MONTEREY = 17,
  GDK_OSX_VENTURA = 18,
  GDK_OSX_CURRENT = 18,
  GDK_OSX_NEW = 99
} GdkOSXVersion;

NSWindow *gdk_quartz_window_get_nswindow                        (GdkWindow      *window);
NSView   *gdk_quartz_window_get_nsview                          (GdkWindow      *window);
NSImage  *gdk_quartz_pixbuf_to_ns_image_libgtk_only             (GdkPixbuf      *pixbuf);
id        gdk_quartz_drag_context_get_dragging_info_libgtk_only (GdkDragContext *context);
NSEvent  *gdk_quartz_event_get_nsevent                          (GdkEvent       *event);
GdkOSXVersion gdk_quartz_osx_version                            (void);
void      gdk_quartz_set_use_cocoa_invalidation                 (int);
int       gdk_quartz_get_use_cocoa_invalidation                 (void);

GdkAtom   gdk_quartz_pasteboard_type_to_atom_libgtk_only        (NSString       *type);
NSString *gdk_quartz_target_to_pasteboard_type_libgtk_only      (const gchar    *target);
NSString *gdk_quartz_atom_to_pasteboard_type_libgtk_only        (GdkAtom         atom);

G_END_DECLS

#endif /* __GDK_QUARTZ_H__ */
