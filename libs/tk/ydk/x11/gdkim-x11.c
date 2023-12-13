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
#include <stdlib.h>
#include <string.h>

#include "gdkx.h"
#include "gdk.h"		/* For gdk_flush() */
#include "gdkpixmap.h"
#include "gdkinternals.h"
#include "gdkdisplay-x11.h"
#include "gdkalias.h"


/* If this variable is FALSE, it indicates that we should
 * avoid trying to use multibyte conversion functions and
 * assume everything is 1-byte per character
 */
static gboolean gdk_use_mb;

void
_gdk_x11_initialize_locale (void)
{
  wchar_t result;
  gchar *current_locale;
  static char *last_locale = NULL;

  gdk_use_mb = FALSE;

  current_locale = setlocale (LC_ALL, NULL);

  if (last_locale && strcmp (last_locale, current_locale) == 0)
    return;

  g_free (last_locale);
  last_locale = g_strdup (current_locale);

  if (XSupportsLocale ())
    XSetLocaleModifiers ("");

  if ((strcmp (current_locale, "C")) && (strcmp (current_locale, "POSIX")))
    {
      gdk_use_mb = TRUE;

#ifndef X_LOCALE
      /* Detect ancient GNU libc, where mb == UTF8. Not useful unless it's
       * really a UTF8 locale. The below still probably will
       * screw up on Greek, Cyrillic, etc, encoded as UTF8.
       */
      
      if ((MB_CUR_MAX == 2) &&
	  (mbstowcs (&result, "\xdd\xa5", 1) > 0) &&
	  result == 0x765)
	{
	  if ((strlen (current_locale) < 4) ||
	      g_ascii_strcasecmp (current_locale + strlen(current_locale) - 4,
				  "utf8"))
	    gdk_use_mb = FALSE;
	}
#endif /* X_LOCALE */
    }

  GDK_NOTE (XIM,
	    g_message ("%s multi-byte string functions.", 
		       gdk_use_mb ? "Using" : "Not using"));
  
  return;
}

gchar*
gdk_set_locale (void)
{
  if (!setlocale (LC_ALL,""))
    g_warning ("locale not supported by C library");

  _gdk_x11_initialize_locale ();
  
  return setlocale (LC_ALL, NULL);
}

static GdkDisplay *
find_a_display (void)
{
  GdkDisplay *display = gdk_display_get_default ();

  if (!display)
    display = _gdk_displays->data;

  return display;
}

/**
 * gdk_wcstombs:
 * @src: a wide character string.
 * 
 * Converts a wide character string to a multi-byte string.
 * (The function name comes from an acronym of 'Wide Character String TO
 * Multi-Byte String').
 * 
 * Return value: the multi-byte string corresponding to @src, or %NULL if the
 * conversion failed. The returned string should be freed with g_free() when no
 * longer needed.
 **/
gchar *
gdk_wcstombs (const GdkWChar *src)
{
  gchar *mbstr;

  if (gdk_use_mb)
    {
      GdkDisplay *display = find_a_display ();
      Display *xdisplay = GDK_DISPLAY_XDISPLAY (display);
      XTextProperty tpr;

      if (sizeof(wchar_t) != sizeof(GdkWChar))
	{
	  gint i;
	  wchar_t *src_alt;
	  for (i=0; src[i]; i++);
	  src_alt = g_new (wchar_t, i+1);
	  for (; i>=0; i--)
	    src_alt[i] = src[i];
	  if (XwcTextListToTextProperty (xdisplay, &src_alt, 1, XTextStyle, &tpr)
	      != Success)
	    {
	      g_free (src_alt);
	      return NULL;
	    }
	  g_free (src_alt);
	}
      else
	{
	  wchar_t *tmp;
	  
	  if (XwcTextListToTextProperty (xdisplay, &tmp, 1,
					 XTextStyle, &tpr) != Success)
	    {
	      return NULL;
	    }
	  
	  src = (GdkWChar *)tmp;
	}
      /*
       * We must copy the string into an area allocated by glib, because
       * the string 'tpr.value' must be freed by XFree().
       */
      mbstr = g_strdup((gchar *)tpr.value);
      XFree (tpr.value);
    }
  else
    {
      gint length = 0;
      gint i;

      while (src[length] != 0)
	length++;
      
      mbstr = g_new (gchar, length + 1);

      for (i=0; i<length+1; i++)
	mbstr[i] = src[i];
    }

  return mbstr;
}
/**
 * gdk_mbstowcs:
 * @dest: the space to place the converted wide character string into.
 * @src: the multi-byte string to convert, which must be nul-terminated.
 * @dest_max: the maximum number of wide characters to place in @dest.
 * 
 * Converts a multi-byte string to a wide character string.
 * (The function name comes from an acronym of 'Multi-Byte String TO Wide
 * Character String').
 * 
 * Return value: the number of wide characters written into @dest, or -1 if 
 *   the conversion failed.
 **/
  
gint
gdk_mbstowcs (GdkWChar *dest, const gchar *src, gint dest_max)
{
  if (gdk_use_mb)
    {
      GdkDisplay *display = find_a_display ();
      Display *xdisplay = GDK_DISPLAY_XDISPLAY (display);
      XTextProperty tpr;
      wchar_t **wstrs, *wstr_src;
      gint num_wstrs;
      gint len_cpy;
      if (XmbTextListToTextProperty (xdisplay, (char **)&src, 1, XTextStyle,
				     &tpr)
	  != Success)
	{
	  /* NoMem or LocaleNotSupp */
	  return -1;
	}
      if (XwcTextPropertyToTextList (xdisplay, &tpr, &wstrs, &num_wstrs)
	  != Success)
	{
	  /* InvalidChar */
	  XFree(tpr.value);
	  return -1;
	}
      XFree(tpr.value);
      if (num_wstrs == 0)
	return 0;
      wstr_src = wstrs[0];
      for (len_cpy=0; len_cpy<dest_max && wstr_src[len_cpy]; len_cpy++)
	dest[len_cpy] = wstr_src[len_cpy];
      XwcFreeStringList (wstrs);
      return len_cpy;
    }
  else
    {
      gint i;

      for (i=0; i<dest_max && src[i]; i++)
	dest[i] = src[i];

      return i;
    }
}

#define __GDK_IM_X11_C__
#include "gdkaliasdef.c"
