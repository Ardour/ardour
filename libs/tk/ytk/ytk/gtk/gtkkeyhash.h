/* gtkkeyhash.h: Keymap aware matching of key bindings
 *
 * GTK - The GIMP Toolkit
 * Copyright (C) 2002, Red Hat Inc.
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

#ifndef __GTK_KEY_HASH_H__
#define __GTK_KEY_HASH_H__

#include <gdk/gdk.h>

G_BEGIN_DECLS

typedef struct _GtkKeyHash GtkKeyHash;

GtkKeyHash *_gtk_key_hash_new           (GdkKeymap       *keymap,
					 GDestroyNotify   item_destroy_notify);
void        _gtk_key_hash_add_entry     (GtkKeyHash      *key_hash,
					 guint            keyval,
					 GdkModifierType  modifiers,
					 gpointer         value);
void        _gtk_key_hash_remove_entry  (GtkKeyHash      *key_hash,
					 gpointer         value);
GSList *    _gtk_key_hash_lookup        (GtkKeyHash      *key_hash,
					 guint16          hardware_keycode,
					 GdkModifierType  state,
					 GdkModifierType  mask,
					 gint             group);
GSList *    _gtk_key_hash_lookup_keyval (GtkKeyHash      *key_hash,
					 guint            keyval,
					 GdkModifierType  modifiers);
void        _gtk_key_hash_free          (GtkKeyHash      *key_hash);

G_END_DECLS

#endif /* __GTK_KEY_HASH_H__ */
