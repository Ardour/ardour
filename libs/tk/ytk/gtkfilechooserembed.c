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

#include "config.h"
#include "gtkfilechooserembed.h"
#include "gtkmarshalers.h"
#include "gtkintl.h"
#include "gtkalias.h"

static void gtk_file_chooser_embed_class_init (gpointer g_iface);
static void delegate_get_default_size         (GtkFileChooserEmbed *chooser_embed,
					       gint                *default_width,
					       gint                *default_height);
static gboolean delegate_should_respond       (GtkFileChooserEmbed *chooser_embed);
static void delegate_initial_focus            (GtkFileChooserEmbed *chooser_embed);
static void delegate_default_size_changed     (GtkFileChooserEmbed *chooser_embed,
					       gpointer             data);
static void delegate_response_requested       (GtkFileChooserEmbed *chooser_embed,
					       gpointer             data);

static GtkFileChooserEmbed *
get_delegate (GtkFileChooserEmbed *receiver)
{
  return g_object_get_data (G_OBJECT (receiver), "gtk-file-chooser-embed-delegate");
}

/**
 * _gtk_file_chooser_embed_delegate_iface_init:
 * @iface: a #GtkFileChoserEmbedIface structure
 * 
 * An interface-initialization function for use in cases where an object is
 * simply delegating the methods, signals of the #GtkFileChooserEmbed interface
 * to another object.  _gtk_file_chooser_embed_set_delegate() must be called on
 * each instance of the object so that the delegate object can be found.
 **/
void
_gtk_file_chooser_embed_delegate_iface_init (GtkFileChooserEmbedIface *iface)
{
  iface->get_default_size = delegate_get_default_size;
  iface->should_respond = delegate_should_respond;
  iface->initial_focus = delegate_initial_focus;
}

/**
 * _gtk_file_chooser_embed_set_delegate:
 * @receiver: a GOobject implementing #GtkFileChooserEmbed
 * @delegate: another GObject implementing #GtkFileChooserEmbed
 *
 * Establishes that calls on @receiver for #GtkFileChooser methods should be
 * delegated to @delegate, and that #GtkFileChooser signals emitted on @delegate
 * should be forwarded to @receiver. Must be used in conjunction with
 * _gtk_file_chooser_embed_delegate_iface_init().
 **/
void
_gtk_file_chooser_embed_set_delegate (GtkFileChooserEmbed *receiver,
				      GtkFileChooserEmbed *delegate)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER_EMBED (receiver));
  g_return_if_fail (GTK_IS_FILE_CHOOSER_EMBED (delegate));
  
  g_object_set_data (G_OBJECT (receiver), I_("gtk-file-chooser-embed-delegate"), delegate);

  g_signal_connect (delegate, "default-size-changed",
		    G_CALLBACK (delegate_default_size_changed), receiver);
  g_signal_connect (delegate, "response-requested",
		    G_CALLBACK (delegate_response_requested), receiver);
}



static void
delegate_get_default_size (GtkFileChooserEmbed *chooser_embed,
			   gint                *default_width,
			   gint                *default_height)
{
  _gtk_file_chooser_embed_get_default_size (get_delegate (chooser_embed), default_width, default_height);
}

static gboolean
delegate_should_respond (GtkFileChooserEmbed *chooser_embed)
{
  return _gtk_file_chooser_embed_should_respond (get_delegate (chooser_embed));
}

static void
delegate_initial_focus (GtkFileChooserEmbed *chooser_embed)
{
  _gtk_file_chooser_embed_initial_focus (get_delegate (chooser_embed));
}

static void
delegate_default_size_changed (GtkFileChooserEmbed *chooser_embed,
			       gpointer             data)
{
  g_signal_emit_by_name (data, "default-size-changed");
}

static void
delegate_response_requested (GtkFileChooserEmbed *chooser_embed,
			     gpointer             data)
{
  g_signal_emit_by_name (data, "response-requested");
}


/* publicly callable functions */

GType
_gtk_file_chooser_embed_get_type (void)
{
  static GType file_chooser_embed_type = 0;

  if (!file_chooser_embed_type)
    {
      const GTypeInfo file_chooser_embed_info =
      {
	sizeof (GtkFileChooserEmbedIface),  /* class_size */
	NULL,                          /* base_init */
	NULL,			       /* base_finalize */
	(GClassInitFunc)gtk_file_chooser_embed_class_init, /* class_init */
      };

      file_chooser_embed_type = g_type_register_static (G_TYPE_INTERFACE,
							I_("GtkFileChooserEmbed"),
							&file_chooser_embed_info, 0);

      g_type_interface_add_prerequisite (file_chooser_embed_type, GTK_TYPE_WIDGET);
    }

  return file_chooser_embed_type;
}

static void
gtk_file_chooser_embed_class_init (gpointer g_iface)
{
  GType iface_type = G_TYPE_FROM_INTERFACE (g_iface);

  g_signal_new (I_("default-size-changed"),
		iface_type,
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (GtkFileChooserEmbedIface, default_size_changed),
		NULL, NULL,
		_gtk_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
  g_signal_new (I_("response-requested"),
		iface_type,
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (GtkFileChooserEmbedIface, response_requested),
		NULL, NULL,
		_gtk_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

void
_gtk_file_chooser_embed_get_default_size (GtkFileChooserEmbed *chooser_embed,
					 gint                *default_width,
					 gint                *default_height)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER_EMBED (chooser_embed));
  g_return_if_fail (default_width != NULL);
  g_return_if_fail (default_height != NULL);

  GTK_FILE_CHOOSER_EMBED_GET_IFACE (chooser_embed)->get_default_size (chooser_embed, default_width, default_height);
}

gboolean
_gtk_file_chooser_embed_should_respond (GtkFileChooserEmbed *chooser_embed)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER_EMBED (chooser_embed), FALSE);

  return GTK_FILE_CHOOSER_EMBED_GET_IFACE (chooser_embed)->should_respond (chooser_embed);
}

void
_gtk_file_chooser_embed_initial_focus (GtkFileChooserEmbed *chooser_embed)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER_EMBED (chooser_embed));

  GTK_FILE_CHOOSER_EMBED_GET_IFACE (chooser_embed)->initial_focus (chooser_embed);
}
