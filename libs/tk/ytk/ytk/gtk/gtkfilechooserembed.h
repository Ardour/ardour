/* GTK - The GIMP Toolkit
 * gtkfilechooserembed.h: Abstract sizing interface for file selector implementations
 * Copyright (C) 2004, Red Hat, Inc.
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

#ifndef __GTK_FILE_CHOOSER_EMBED_H__
#define __GTK_FILE_CHOOSER_EMBED_H__

#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

#define GTK_TYPE_FILE_CHOOSER_EMBED             (_gtk_file_chooser_embed_get_type ())
#define GTK_FILE_CHOOSER_EMBED(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_FILE_CHOOSER_EMBED, GtkFileChooserEmbed))
#define GTK_IS_FILE_CHOOSER_EMBED(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_FILE_CHOOSER_EMBED))
#define GTK_FILE_CHOOSER_EMBED_GET_IFACE(obj)   (G_TYPE_INSTANCE_GET_INTERFACE ((obj), GTK_TYPE_FILE_CHOOSER_EMBED, GtkFileChooserEmbedIface))

typedef struct _GtkFileChooserEmbed      GtkFileChooserEmbed;
typedef struct _GtkFileChooserEmbedIface GtkFileChooserEmbedIface;


struct _GtkFileChooserEmbedIface
{
  GTypeInterface base_iface;

  /* Methods
   */
  void (*get_default_size)        (GtkFileChooserEmbed *chooser_embed,
				   gint                *default_width,
				   gint                *default_height);

  gboolean (*should_respond)      (GtkFileChooserEmbed *chooser_embed);

  void (*initial_focus)           (GtkFileChooserEmbed *chooser_embed);
  /* Signals
   */
  void (*default_size_changed)    (GtkFileChooserEmbed *chooser_embed);
  void (*response_requested)      (GtkFileChooserEmbed *chooser_embed);
};

GType _gtk_file_chooser_embed_get_type (void) G_GNUC_CONST;

void  _gtk_file_chooser_embed_get_default_size    (GtkFileChooserEmbed *chooser_embed,
						   gint                *default_width,
						   gint                *default_height);
gboolean _gtk_file_chooser_embed_should_respond (GtkFileChooserEmbed *chooser_embed);

void _gtk_file_chooser_embed_initial_focus (GtkFileChooserEmbed *chooser_embed);

void _gtk_file_chooser_embed_delegate_iface_init  (GtkFileChooserEmbedIface *iface);
void _gtk_file_chooser_embed_set_delegate         (GtkFileChooserEmbed *receiver,
						   GtkFileChooserEmbed *delegate);

G_END_DECLS

#endif /* __GTK_FILE_CHOOSER_EMBED_H__ */
