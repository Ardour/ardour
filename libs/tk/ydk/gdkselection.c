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
#include "gdkproperty.h"
#include "gdkdisplay.h"
#include "gdkselection.h"
#include "gdkalias.h"

gboolean
gdk_selection_owner_set (GdkWindow *owner,
			 GdkAtom    selection,
			 guint32    time,
			 gboolean   send_event)
{
  return gdk_selection_owner_set_for_display (gdk_display_get_default (),
					      owner, selection, 
					      time, send_event);
}

GdkWindow*
gdk_selection_owner_get (GdkAtom selection)
{
  return gdk_selection_owner_get_for_display (gdk_display_get_default (), 
					      selection);
}

void
gdk_selection_send_notify (GdkNativeWindow requestor,
			   GdkAtom         selection,
			   GdkAtom         target,
			   GdkAtom         property,
			   guint32         time)
{
  gdk_selection_send_notify_for_display (gdk_display_get_default (), 
					 requestor, selection, 
					 target, property, time);
}

gint
gdk_text_property_to_text_list (GdkAtom       encoding,
				gint          format, 
				const guchar *text,
				gint          length,
				gchar      ***list)
{
  return gdk_text_property_to_text_list_for_display (gdk_display_get_default (),
						     encoding, format, text, length, list);
}

/**
 * gdk_text_property_to_utf8_list:
 * @encoding: an atom representing the encoding of the text
 * @format:   the format of the property
 * @text:     the text to convert
 * @length:   the length of @text, in bytes
 * @list: (allow-none):     location to store the list of strings or %NULL. The
 *            list should be freed with g_strfreev().
 * 
 * Convert a text property in the giving encoding to
 * a list of UTF-8 strings. 
 * 
 * Return value: the number of strings in the resulting
 *               list.
 **/
gint 
gdk_text_property_to_utf8_list (GdkAtom        encoding,
				gint           format,
				const guchar  *text,
				gint           length,
				gchar       ***list)
{
  return gdk_text_property_to_utf8_list_for_display (gdk_display_get_default (),
						     encoding, format, text, length, list);
}

gint
gdk_string_to_compound_text (const gchar *str,
			     GdkAtom     *encoding,
			     gint        *format,
			     guchar     **ctext,
			     gint        *length)
{
  return gdk_string_to_compound_text_for_display (gdk_display_get_default (),
						  str, encoding, format, 
						  ctext, length);
}

/**
 * gdk_utf8_to_compound_text:
 * @str:      a UTF-8 string
 * @encoding: location to store resulting encoding
 * @format:   location to store format of the result
 * @ctext:    location to store the data of the result
 * @length:   location to store the length of the data
 *            stored in @ctext
 * 
 * Convert from UTF-8 to compound text. 
 * 
 * Return value: %TRUE if the conversion succeeded, otherwise
 *               false.
 **/
gboolean
gdk_utf8_to_compound_text (const gchar *str,
			   GdkAtom     *encoding,
			   gint        *format,
			   guchar     **ctext,
			   gint        *length)
{
  return gdk_utf8_to_compound_text_for_display (gdk_display_get_default (),
						str, encoding, format, 
						ctext, length);
}

#define __GDK_SELECTION_C__
#include "gdkaliasdef.c"
