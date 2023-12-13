/* GTK - The GIMP Toolkit
 * gtktextiterprivate.h Copyright (C) 2000 Red Hat, Inc.
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

#ifndef __GTK_TEXT_ITER_PRIVATE_H__
#define __GTK_TEXT_ITER_PRIVATE_H__

#include <gtk/gtktextiter.h>

G_BEGIN_DECLS

#include <gtk/gtktextiter.h>
#include <gtk/gtktextbtree.h>

GtkTextLineSegment *_gtk_text_iter_get_indexable_segment      (const GtkTextIter *iter);
GtkTextLineSegment *_gtk_text_iter_get_any_segment            (const GtkTextIter *iter);
GtkTextLine *       _gtk_text_iter_get_text_line              (const GtkTextIter *iter);
GtkTextBTree *      _gtk_text_iter_get_btree                  (const GtkTextIter *iter);
gboolean            _gtk_text_iter_forward_indexable_segment  (GtkTextIter       *iter);
gboolean            _gtk_text_iter_backward_indexable_segment (GtkTextIter       *iter);
gint                _gtk_text_iter_get_segment_byte           (const GtkTextIter *iter);
gint                _gtk_text_iter_get_segment_char           (const GtkTextIter *iter);


/* debug */
void _gtk_text_iter_check (const GtkTextIter *iter);

G_END_DECLS

#endif


