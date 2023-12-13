/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2010 Red Hat, Inc.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GDK_WIN32_KEYS_H__
#define __GDK_WIN32_KEYS_H__


#include <gdk/gdk.h>

G_BEGIN_DECLS

/**
 * GdkWin32KeymapMatch:
 * @GDK_WIN32_KEYMAP_MATCH_NONE: no matches found. Output is not valid.
 * @GDK_WIN32_KEYMAP_MATCH_INCOMPLETE: the sequence matches so far, but is incomplete. Output is not valid.
 * @GDK_WIN32_KEYMAP_MATCH_PARTIAL: the sequence matches up to the last key,
 *     which does not match. Output is valid.
 * @GDK_WIN32_KEYMAP_MATCH_EXACT: the sequence matches exactly. Output is valid.
 *
 * An enumeration describing the result of a deadkey combination matching.
 */
typedef enum
{
  GDK_WIN32_KEYMAP_MATCH_NONE,
  GDK_WIN32_KEYMAP_MATCH_INCOMPLETE,
  GDK_WIN32_KEYMAP_MATCH_PARTIAL,
  GDK_WIN32_KEYMAP_MATCH_EXACT
} GdkWin32KeymapMatch;

#ifdef GDK_COMPILATION
typedef struct _GdkWin32Keymap GdkWin32Keymap;
#else
typedef GdkKeymap GdkWin32Keymap;
#endif
typedef struct _GdkWin32KeymapClass GdkWin32KeymapClass;

#define GDK_TYPE_WIN32_KEYMAP              (gdk_win32_keymap_get_type())
#define GDK_WIN32_KEYMAP(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_WIN32_KEYMAP, GdkWin32Keymap))
#define GDK_WIN32_KEYMAP_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_WIN32_KEYMAP, GdkWin32KeymapClass))
#define GDK_IS_WIN32_KEYMAP(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_WIN32_KEYMAP))
#define GDK_IS_WIN32_KEYMAP_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_WIN32_KEYMAP))
#define GDK_WIN32_KEYMAP_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_WIN32_KEYMAP, GdkWin32KeymapClass))

GType gdk_win32_keymap_get_type (void);

GdkWin32KeymapMatch gdk_win32_keymap_check_compose (GdkWin32Keymap *keymap,
                                                    guint          *compose_buffer,
                                                    gsize           compose_buffer_len,
                                                    guint16        *output,
                                                    gsize          *output_len);

G_END_DECLS

#endif /* __GDK_WIN32_KEYMAP_H__ */
