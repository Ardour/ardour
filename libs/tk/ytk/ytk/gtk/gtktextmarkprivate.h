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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
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

#ifndef __GTK_TEXT_MARK_PRIVATE_H__
#define __GTK_TEXT_MARK_PRIVATE_H__

#include <gtk/gtktexttypes.h>
#include <gtk/gtktextlayout.h>

G_BEGIN_DECLS

#define GTK_IS_TEXT_MARK_SEGMENT(mark) (((GtkTextLineSegment*)mark)->type == &gtk_text_left_mark_type || \
                                ((GtkTextLineSegment*)mark)->type == &gtk_text_right_mark_type)

/*
 * The data structure below defines line segments that represent
 * marks.  There is one of these for each mark in the text.
 */

struct _GtkTextMarkBody {
  GtkTextMark *obj;
  gchar *name;
  GtkTextBTree *tree;
  GtkTextLine *line;
  guint visible : 1;
  guint not_deleteable : 1;
};

void _gtk_mark_segment_set_tree (GtkTextLineSegment *mark,
				 GtkTextBTree       *tree);

G_END_DECLS

#endif



