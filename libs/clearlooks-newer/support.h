#ifndef SUPPORT_H
#define SUPPORT_H

#include <gtk/gtk.h>
#include <math.h>
#include <string.h>

#include "clearlooks_types.h"

#define RADIO_SIZE 13
#define CHECK_SIZE 13

GE_INTERNAL void              clearlooks_treeview_get_header_index (GtkTreeView  *tv,
                                                 GtkWidget    *header,
                                                 gint         *column_index,
                                                 gint         *columns,
                                                 gboolean     *resizable);

GE_INTERNAL void              clearlooks_clist_get_header_index    (GtkCList     *clist,
                                                 GtkWidget    *button,
                                                 gint         *column_index,
                                                 gint         *columns);
#ifdef DEVELOPMENT
#warning clearlooks_get_parent_bg is a bad hack - find out why its needed, and figure out a better way.
#endif

GE_INTERNAL void              clearlooks_get_parent_bg      (const GtkWidget *widget,
                                                 CairoColor      *color);

GE_INTERNAL ClearlooksStepper clearlooks_scrollbar_get_stepper         (GtkWidget       *widget,
                                                 GdkRectangle    *stepper);
GE_INTERNAL ClearlooksStepper clearlooks_scrollbar_visible_steppers    (GtkWidget       *widget);
GE_INTERNAL ClearlooksJunction clearlooks_scrollbar_get_junction       (GtkWidget    *widget);

GE_INTERNAL void clearlooks_set_toolbar_parameters (ToolbarParameters *toolbar, GtkWidget *widget, GdkWindow *window, gint x, gint y);
GE_INTERNAL void clearlooks_get_notebook_tab_position (GtkWidget *widget, gboolean *start, gboolean *end);

#endif /* SUPPORT_H */
