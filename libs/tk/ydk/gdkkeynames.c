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
#include <glib/gprintf.h>
#include <stdlib.h>
#include <string.h>

#include "gdkkeysyms.h"
#include "gdkinternals.h"
#include "gdkalias.h"

/* Key handling not part of the keymap */

#include "keyname-table.h"

#define GDK_NUM_KEYS G_N_ELEMENTS (gdk_keys_by_keyval)

static int
gdk_keys_keyval_compare (const void *pkey, const void *pbase)
{
  return (*(int *) pkey) - ((gdk_key *) pbase)->keyval;
}

/**
 * gdk_keyval_name:
 * @keyval: a key value
 *
 * Converts a key value into a symbolic name.
 *
 * The names are the same as those in the
 * <filename>&lt;gdk/gdkkeysyms.h&gt;</filename> header file
 * but without the leading "GDK_KEY_".
 *
 * Return value: (transfer none): a string containing the name of the key,
 *     or %NULL if @keyval is not a valid key. The string should not be
 *     modified.
 */
gchar*
gdk_keyval_name (guint keyval)
{
  static gchar buf[100];
  gdk_key *found;

  /* Check for directly encoded 24-bit UCS characters: */
  if ((keyval & 0xff000000) == 0x01000000)
    {
      g_sprintf (buf, "U+%.04X", (keyval & 0x00ffffff));
      return buf;
    }

  found = bsearch (&keyval, gdk_keys_by_keyval,
		   GDK_NUM_KEYS, sizeof (gdk_key),
		   gdk_keys_keyval_compare);

  if (found != NULL)
    {
      while ((found > gdk_keys_by_keyval) &&
             ((found - 1)->keyval == keyval))
        found--;
	    
      return (gchar *) (keynames + found->offset);
    }
  else if (keyval != 0)
    {
      g_sprintf (buf, "%#x", keyval);
      return buf;
    }

  return NULL;
}

static int
gdk_keys_name_compare (const void *pkey, const void *pbase)
{
  return strcmp ((const char *) pkey, 
		 (const char *) (keynames + ((const gdk_key *) pbase)->offset));
}

/**
 * gdk_keyval_from_name:
 * @keyval_name: a key name
 *
 * Converts a key name to a key value.
 *
 * The names are the same as those in the
 * <filename>&lt;gdk/gdkkeysyms.h&gt;</filename> header file
 * but without the leading "GDK_KEY_".
 *
 * Returns: the corresponding key value, or %GDK_KEY_VoidSymbol
 *     if the key name is not a valid key
 */
guint
gdk_keyval_from_name (const gchar *keyval_name)
{
  gdk_key *found;

  g_return_val_if_fail (keyval_name != NULL, 0);
  
  found = bsearch (keyval_name, gdk_keys_by_name,
		   GDK_NUM_KEYS, sizeof (gdk_key),
		   gdk_keys_name_compare);
  if (found != NULL)
    return found->keyval;
  else
    return GDK_VoidSymbol;
}

#define __GDK_KEYNAMES_C__
#include "gdkaliasdef.c"
