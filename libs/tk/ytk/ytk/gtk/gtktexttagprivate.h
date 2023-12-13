/* GTK - The GIMP Toolkit
 * gtktexttagprivate.h Copyright (C) 2000 Red Hat, Inc.
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

#ifndef __GTK_TEXT_TAG_PRIVATE_H__
#define __GTK_TEXT_TAG_PRIVATE_H__

#include <gtk/gtk.h>

typedef struct _GtkTextBTreeNode GtkTextBTreeNode;

/* values should already have desired defaults; this function will override
 * the defaults with settings in the given tags, which should be sorted in
 * ascending order of priority
*/
void _gtk_text_attributes_fill_from_tags   (GtkTextAttributes   *values,
                                            GtkTextTag         **tags,
                                            guint                n_tags);
void _gtk_text_tag_array_sort              (GtkTextTag         **tag_array_p,
                                            guint                len);

/* ensure colors are allocated, etc. for drawing */
void                _gtk_text_attributes_realize   (GtkTextAttributes *values,
                                                    GdkColormap       *cmap,
                                                    GdkVisual         *visual);

/* free the stuff again */
void                _gtk_text_attributes_unrealize (GtkTextAttributes *values,
                                                    GdkColormap       *cmap,
                                                    GdkVisual         *visual);

gboolean _gtk_text_tag_affects_size               (GtkTextTag *tag);
gboolean _gtk_text_tag_affects_nonsize_appearance (GtkTextTag *tag);

#endif
