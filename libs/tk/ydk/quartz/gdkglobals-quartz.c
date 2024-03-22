/* gdkglobals-quartz.c
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

#include "config.h"
#include "gdktypes.h"
#include "gdkprivate.h"
#include "gdkquartz.h"

GdkDisplay *_gdk_display = NULL;
GdkScreen *_gdk_screen = NULL;
GdkWindow *_gdk_root = NULL;

static int _gdk_quartz_use_cocoa_invalidation = FALSE;

GdkOSXVersion
gdk_quartz_osx_version (void)
{
  static gint32 vkey = GDK_OSX_UNSUPPORTED;

  if (vkey == GDK_OSX_UNSUPPORTED)
    {
#if MAC_OS_X_VERSION_MIN_REQUIRED < 101000
      OSErr err = Gestalt (gestaltSystemVersionMinor, (SInt32*)&vkey);

      g_return_val_if_fail (err == noErr, GDK_OSX_UNSUPPORTED);
#else
      NSOperatingSystemVersion version;

      version = [[NSProcessInfo processInfo] operatingSystemVersion];
      vkey = version.majorVersion == 10 ? version.minorVersion : version.majorVersion + 5;
#endif
    }

  if (vkey < GDK_OSX_MIN)
    return GDK_OSX_UNSUPPORTED;
  else if (vkey > GDK_OSX_CURRENT)
    return GDK_OSX_NEW;
  else
    return vkey;
}

void
gdk_quartz_set_use_cocoa_invalidation (int yn)
{
	_gdk_quartz_use_cocoa_invalidation = yn;
}

int
gdk_quartz_get_use_cocoa_invalidation (void)
{
	return _gdk_quartz_use_cocoa_invalidation;
}
