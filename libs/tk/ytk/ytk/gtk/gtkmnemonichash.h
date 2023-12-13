/* gtkmnemonichash.h: Sets of mnemonics with cycling
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

#ifndef __GTK_MNEMONIC_HASH_H__
#define __GTK_MNEMONIC_HASH_H__

#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

typedef struct _GtkMnemnonicHash GtkMnemonicHash;

typedef void (*GtkMnemonicHashForeach) (guint      keyval,
					GSList    *targets,
					gpointer   data);

GtkMnemonicHash *_gtk_mnemonic_hash_new      (void);
void             _gtk_mnemonic_hash_free     (GtkMnemonicHash        *mnemonic_hash);
void             _gtk_mnemonic_hash_add      (GtkMnemonicHash        *mnemonic_hash,
					      guint                   keyval,
					      GtkWidget              *target);
void             _gtk_mnemonic_hash_remove   (GtkMnemonicHash        *mnemonic_hash,
					      guint                   keyval,
					      GtkWidget              *target);
gboolean         _gtk_mnemonic_hash_activate (GtkMnemonicHash        *mnemonic_hash,
					      guint                   keyval);
GSList *         _gtk_mnemonic_hash_lookup   (GtkMnemonicHash        *mnemonic_hash,
					      guint                   keyval);
void             _gtk_mnemonic_hash_foreach  (GtkMnemonicHash        *mnemonic_hash,
					      GtkMnemonicHashForeach  func,
					      gpointer                func_data);

G_END_DECLS

#endif /* __GTK_MNEMONIC_HASH_H__ */
