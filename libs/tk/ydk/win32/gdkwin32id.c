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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
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
#include <gdk/gdk.h>

#include "gdkprivate-win32.h"

static GHashTable *handle_ht = NULL;

static guint
gdk_handle_hash (HANDLE *handle)
{
#ifdef _WIN64
  return ((guint *) handle)[0] ^ ((guint *) handle)[1];
#else
  return (guint) *handle;
#endif
}

static gint
gdk_handle_equal (HANDLE *a,
		  HANDLE *b)
{
  return (*a == *b);
}

void
gdk_win32_handle_table_insert (HANDLE  *handle,
			       gpointer data)
{
  g_return_if_fail (handle != NULL);

  if (!handle_ht)
    handle_ht = g_hash_table_new ((GHashFunc) gdk_handle_hash,
				  (GEqualFunc) gdk_handle_equal);

  g_hash_table_insert (handle_ht, handle, data);
}

void
gdk_win32_handle_table_remove (HANDLE handle)
{
  if (!handle_ht)
    handle_ht = g_hash_table_new ((GHashFunc) gdk_handle_hash,
				  (GEqualFunc) gdk_handle_equal);

  g_hash_table_remove (handle_ht, &handle);
}

gpointer
gdk_win32_handle_table_lookup (GdkNativeWindow handle)
{
  gpointer data = NULL;

  if (handle_ht)
    data = g_hash_table_lookup (handle_ht, &handle);
  
  return data;
}
