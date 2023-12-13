/* gdkapplaunchcontext-win32.c - Gtk+ implementation for GAppLaunchContext

   Copyright (C) 2007 Red Hat, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Matthias Clasen <mclasen@redhat.com>
*/

#include "config.h"

#include "gdkapplaunchcontext.h"


char *
_gdk_windowing_get_startup_notify_id (GAppLaunchContext *context,
                                      GAppInfo          *info,
                                      GList             *files)
{
	return NULL;
}

void
_gdk_windowing_launch_failed (GAppLaunchContext *context,
                              const char        *startup_notify_id)
{
}


