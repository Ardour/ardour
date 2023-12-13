/* GtkToolPalette -- A tool palette with categories and DnD support
 * Copyright (C) 2008  Openismus GmbH
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Authors:
 *      Mathias Hasselmann
 */

#ifndef __GTK_TOOL_PALETTE_PRIVATE_H__
#define __GTK_TOOL_PALETTE_PRIVATE_H__

#include <gtk/gtk.h>

void _gtk_tool_palette_get_item_size           (GtkToolPalette   *palette,
                                                GtkRequisition   *item_size,
                                                gboolean          homogeneous_only,
                                                gint             *requested_rows);
void _gtk_tool_palette_child_set_drag_source   (GtkWidget        *widget,
                                                gpointer          data);
void _gtk_tool_palette_set_expanding_child     (GtkToolPalette   *palette,
                                                GtkWidget        *widget);

void _gtk_tool_item_group_palette_reconfigured (GtkToolItemGroup *group);
void _gtk_tool_item_group_item_size_request    (GtkToolItemGroup *group,
                                                GtkRequisition   *item_size,
                                                gboolean          homogeneous_only,
                                                gint             *requested_rows);
gint _gtk_tool_item_group_get_height_for_width (GtkToolItemGroup *group,
                                                gint              width);
gint _gtk_tool_item_group_get_width_for_height (GtkToolItemGroup *group,
                                                gint              height);
void _gtk_tool_item_group_paint                (GtkToolItemGroup *group,
                                                cairo_t          *cr);
gint _gtk_tool_item_group_get_size_for_limit   (GtkToolItemGroup *group,
                                                gint              limit,
                                                gboolean          vertical,
                                                gboolean          animation);


GtkSizeGroup *_gtk_tool_palette_get_size_group (GtkToolPalette   *palette);

#endif /* __GTK_TOOL_PALETTE_PRIVATE_H__ */
