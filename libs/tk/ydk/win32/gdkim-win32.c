/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998-2004 Tor Lillqvist
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

#include <stdlib.h>
#include <string.h>
#include <locale.h>

#include "gdkpixmap.h"
#include "gdkinternals.h"
#include "gdki18n.h"
#include "gdkwin32.h"

gchar*
gdk_set_locale (void)
{
  if (!setlocale (LC_ALL, ""))
    g_warning ("locale not supported by C library");
  
  return g_win32_getlocale ();
}

gchar *
gdk_wcstombs (const GdkWChar *src)
{
  const gchar *charset;

  g_get_charset (&charset);
  return g_convert ((char *) src, -1, charset, "UCS-4LE", NULL, NULL, NULL);
}

gint
gdk_mbstowcs (GdkWChar    *dest,
	      const gchar *src,
	      gint         dest_max)
{
  gint retval;
  gsize nwritten;
  gint n_ucs4;
  gunichar *ucs4;
  const gchar *charset;

  g_get_charset (&charset);
  ucs4 = (gunichar *) g_convert (src, -1, "UCS-4LE", charset, NULL, &nwritten, NULL);
  n_ucs4 = nwritten * sizeof (GdkWChar);

  retval = MIN (dest_max, n_ucs4);
  memmove (dest, ucs4, retval * sizeof (GdkWChar));
  g_free (ucs4);

  return retval;
}
