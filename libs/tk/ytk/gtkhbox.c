/* GTK - The GIMP Toolkit
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

#include "gtkhbox.h"
#include "gtkorientable.h"
#include "gtkalias.h"


/**
 * SECTION:gtkhbox
 * @Short_description: A horizontal container box
 * @Title: GtkHBox
 * @See_also: #GtkVBox
 *
 * #GtkHBox is a container that organizes child widgets into a single row.
 *
 * Use the #GtkBox packing interface to determine the arrangement,
 * spacing, width, and alignment of #GtkHBox children.
 *
 * All children are allocated the same height.
 */


G_DEFINE_TYPE (GtkHBox, gtk_hbox, GTK_TYPE_BOX)

static void
gtk_hbox_class_init (GtkHBoxClass *class)
{
}

static void
gtk_hbox_init (GtkHBox *hbox)
{
  gtk_orientable_set_orientation (GTK_ORIENTABLE (hbox),
                                  GTK_ORIENTATION_HORIZONTAL);

  _gtk_box_set_old_defaults (GTK_BOX (hbox));
}

/**
 * gtk_hbox_new:
 * @homogeneous: %TRUE if all children are to be given equal space allotments.
 * @spacing: the number of pixels to place by default between children.
 *
 * Creates a new #GtkHBox.
 *
 * Returns: a new #GtkHBox.
 */
GtkWidget *
gtk_hbox_new (gboolean homogeneous,
	      gint     spacing)
{
  return g_object_new (GTK_TYPE_HBOX,
                       "spacing",     spacing,
                       "homogeneous", homogeneous ? TRUE : FALSE,
                       NULL);
}

#define __GTK_HBOX_C__
#include "gtkaliasdef.c"
