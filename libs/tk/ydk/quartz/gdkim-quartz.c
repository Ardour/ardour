/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#include <locale.h>

#include "gdki18n.h"
#include "gdkinternals.h"
#include "gdkprivate-quartz.h"

gchar*
gdk_set_locale (void)
{
  if (!setlocale (LC_ALL,""))
    g_warning ("locale not supported by C library");
  
  return setlocale (LC_ALL, NULL);
}

gchar *
gdk_wcstombs (const GdkWChar *src)
{
  gchar *mbstr;

  gint length = 0;
  gint i;

  while (src[length] != 0)
    length++;
  
  mbstr = g_new (gchar, length + 1);
  
  for (i = 0; i < length + 1; i++)
    mbstr[i] = src[i];

  return mbstr;
}

gint
gdk_mbstowcs (GdkWChar *dest, const gchar *src, gint dest_max)
{
  gint i;
  
  for (i = 0; i < dest_max && src[i]; i++)
    dest[i] = src[i];

  return i;
}

