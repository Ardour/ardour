/* GTK - The GIMP Toolkit
 * gtktexttypes.c Copyright (C) 2000 Red Hat, Inc.
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
#include "gtktexttypes.h"
#include "gtkalias.h"

/* These are used to represent embedded non-character objects
 * if you return a string representation of a text buffer
 */
const gchar gtk_text_unknown_char_utf8[] = { '\xEF', '\xBF', '\xBC', '\0' };

static inline gboolean
inline_byte_begins_utf8_char (const gchar *byte)
{
  return ((*byte & 0xC0) != 0x80);
}

gboolean
gtk_text_byte_begins_utf8_char (const gchar *byte)
{
  return inline_byte_begins_utf8_char (byte);
}

#define __GTK_TEXT_TYPES_C__
#include "gtkaliasdef.c"
