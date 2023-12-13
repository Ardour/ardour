/* gtktreeprivate.h
 * Copyright (C) 2000  Red Hat, Inc.,  Jonathan Blandford <jrb@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GTK_TREE_PRIVATE_H__
#define __GTK_TREE_PRIVATE_H__


G_BEGIN_DECLS


#include <gtk/gtktreeview.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtkrbtree.h>

#define TREE_VIEW_DRAG_WIDTH 6

typedef enum
{
  GTK_TREE_VIEW_IS_LIST = 1 << 0,
  GTK_TREE_VIEW_SHOW_EXPANDERS = 1 << 1,
  GTK_TREE_VIEW_IN_COLUMN_RESIZE = 1 << 2,
  GTK_TREE_VIEW_ARROW_PRELIT = 1 << 3,
  GTK_TREE_VIEW_HEADERS_VISIBLE = 1 << 4,
  GTK_TREE_VIEW_DRAW_KEYFOCUS = 1 << 5,
  GTK_TREE_VIEW_MODEL_SETUP = 1 << 6,
  GTK_TREE_VIEW_IN_COLUMN_DRAG = 1 << 7
} GtkTreeViewFlags;

typedef enum
{
  GTK_TREE_SELECT_MODE_TOGGLE = 1 << 0,
  GTK_TREE_SELECT_MODE_EXTEND = 1 << 1
}
GtkTreeSelectMode;

enum
{
  DRAG_COLUMN_WINDOW_STATE_UNSET = 0,
  DRAG_COLUMN_WINDOW_STATE_ORIGINAL = 1,
  DRAG_COLUMN_WINDOW_STATE_ARROW = 2,
  DRAG_COLUMN_WINDOW_STATE_ARROW_LEFT = 3,
  DRAG_COLUMN_WINDOW_STATE_ARROW_RIGHT = 4
};

enum
{
  RUBBER_BAND_OFF = 0,
  RUBBER_BAND_MAYBE_START = 1,
  RUBBER_BAND_ACTIVE = 2
};

#define GTK_TREE_VIEW_SET_FLAG(tree_view, flag)   G_STMT_START{ (tree_view->priv->flags|=flag); }G_STMT_END
#define GTK_TREE_VIEW_UNSET_FLAG(tree_view, flag) G_STMT_START{ (tree_view->priv->flags&=~(flag)); }G_STMT_END
#define GTK_TREE_VIEW_FLAG_SET(tree_view, flag)   ((tree_view->priv->flags&flag)==flag)
#define TREE_VIEW_HEADER_HEIGHT(tree_view)        (GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_HEADERS_VISIBLE)?tree_view->priv->header_height:0)
#define TREE_VIEW_COLUMN_REQUESTED_WIDTH(column)  (CLAMP (column->requested_width, (column->min_width!=-1)?column->min_width:column->requested_width, (column->max_width!=-1)?column->max_width:column->requested_width))
#define TREE_VIEW_DRAW_EXPANDERS(tree_view)       (!GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_IS_LIST)&&GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_SHOW_EXPANDERS))

 /* This lovely little value is used to determine how far away from the title bar
  * you can move the mouse and still have a column drag work.
  */
#define TREE_VIEW_COLUMN_DRAG_DEAD_MULTIPLIER(tree_view) (10*TREE_VIEW_HEADER_HEIGHT(tree_view))

typedef struct _GtkTreeViewColumnReorder GtkTreeViewColumnReorder;
struct _GtkTreeViewColumnReorder
{
  gint left_align;
  gint right_align;
  GtkTreeViewColumn *left_column;
  GtkTreeViewColumn *right_column;
};

struct _GtkTreeViewPrivate
{
  GtkTreeModel *model;

  guint flags;
  /* tree information */
  GtkRBTree *tree;

  /* Container info */
  GList *children;
  gint width;
  gint height;

  /* Adjustments */
  GtkAdjustment *hadjustment;
  GtkAdjustment *vadjustment;

  /* Sub windows */
  GdkWindow *bin_window;
  GdkWindow *header_window;

  /* Scroll position state keeping */
  GtkTreeRowReference *top_row;
  gint top_row_dy;
  /* dy == y pos of top_row + top_row_dy */
  /* we cache it for simplicity of the code */
  gint dy;

  guint presize_handler_timer;
  guint validate_rows_timer;
  guint scroll_sync_timer;

  /* Indentation and expander layout */
  gint expander_size;
  GtkTreeViewColumn *expander_column;

  gint level_indentation;

  /* Key navigation (focus), selection */
  gint cursor_offset;

  GtkTreeRowReference *anchor;
  GtkTreeRowReference *cursor;

  GtkTreeViewColumn *focus_column;

  /* Current pressed node, previously pressed, prelight */
  GtkRBNode *button_pressed_node;
  GtkRBTree *button_pressed_tree;

  gint pressed_button;
  gint press_start_x;
  gint press_start_y;

  gint event_last_x;
  gint event_last_y;

  guint last_button_time;
  gint last_button_x;
  gint last_button_y;

  GtkRBNode *prelight_node;
  GtkRBTree *prelight_tree;

  /* Cell Editing */
  GtkTreeViewColumn *edited_column;

  /* The node that's currently being collapsed or expanded */
  GtkRBNode *expanded_collapsed_node;
  GtkRBTree *expanded_collapsed_tree;
  guint expand_collapse_timeout;

  /* Auto expand/collapse timeout in hover mode */
  guint auto_expand_timeout;

  /* Selection information */
  GtkTreeSelection *selection;

  /* Header information */
  gint n_columns;
  GList *columns;
  gint header_height;

  GtkTreeViewColumnDropFunc column_drop_func;
  gpointer column_drop_func_data;
  GDestroyNotify column_drop_func_data_destroy;
  GList *column_drag_info;
  GtkTreeViewColumnReorder *cur_reorder;

  gint prev_width_before_expander;

  /* Interactive Header reordering */
  GdkWindow *drag_window;
  GdkWindow *drag_highlight_window;
  GtkTreeViewColumn *drag_column;
  gint drag_column_x;

  /* Interactive Header Resizing */
  gint drag_pos;
  gint x_drag;

  /* Non-interactive Header Resizing, expand flag support */
  gint prev_width;

  gint last_extra_space;
  gint last_extra_space_per_column;
  gint last_number_of_expand_columns;

  /* ATK Hack */
  GtkTreeDestroyCountFunc destroy_count_func;
  gpointer destroy_count_data;
  GDestroyNotify destroy_count_destroy;

  /* Scroll timeout (e.g. during dnd, rubber banding) */
  guint scroll_timeout;

  /* Row drag-and-drop */
  GtkTreeRowReference *drag_dest_row;
  GtkTreeViewDropPosition drag_dest_pos;
  guint open_dest_timeout;

  /* Rubber banding */
  gint rubber_band_status;
  gint rubber_band_x;
  gint rubber_band_y;
  gint rubber_band_extend;
  gint rubber_band_modify;

  GtkRBNode *rubber_band_start_node;
  GtkRBTree *rubber_band_start_tree;

  GtkRBNode *rubber_band_end_node;
  GtkRBTree *rubber_band_end_tree;

  /* fixed height */
  gint fixed_height;

  /* Scroll-to functionality when unrealized */
  GtkTreeRowReference *scroll_to_path;
  GtkTreeViewColumn *scroll_to_column;
  gfloat scroll_to_row_align;
  gfloat scroll_to_col_align;

  /* Interactive search */
  gint selected_iter;
  gint search_column;
  GtkTreeViewSearchPositionFunc search_position_func;
  GtkTreeViewSearchEqualFunc search_equal_func;
  gpointer search_user_data;
  GDestroyNotify search_destroy;
  gpointer search_position_user_data;
  GDestroyNotify search_position_destroy;
  GtkWidget *search_window;
  GtkWidget *search_entry;
  guint search_entry_changed_id;
  guint typeselect_flush_timeout;

  /* Grid and tree lines */
  GtkTreeViewGridLines grid_lines;
  double grid_line_dashes[2];
  int grid_line_width;

  gboolean tree_lines_enabled;
  double tree_line_dashes[2];
  int tree_line_width;

  /* Row separators */
  GtkTreeViewRowSeparatorFunc row_separator_func;
  gpointer row_separator_data;
  GDestroyNotify row_separator_destroy;

  /* Tooltip support */
  gint tooltip_column;

  /* Here comes the bitfield */
  guint scroll_to_use_align : 1;

  guint fixed_height_mode : 1;
  guint fixed_height_check : 1;

  guint reorderable : 1;
  guint header_has_focus : 1;
  guint drag_column_window_state : 3;
  /* hint to display rows in alternating colors */
  guint has_rules : 1;
  guint mark_rows_col_dirty : 1;

  /* for DnD */
  guint empty_view_drop : 1;

  guint modify_selection_pressed : 1;
  guint extend_selection_pressed : 1;

  guint init_hadjust_value : 1;

  guint in_top_row_to_dy : 1;

  /* interactive search */
  guint enable_search : 1;
  guint disable_popdown : 1;
  guint search_custom_entry_set : 1;
  
  guint hover_selection : 1;
  guint hover_expand : 1;
  guint imcontext_changed : 1;

  guint rubber_banding_enable : 1;

  guint in_grab : 1;

  guint post_validation_flag : 1;

  /* Whether our key press handler is to avoid sending an unhandled binding to the search entry */
  guint search_entry_avoid_unhandled_binding : 1;
};

#ifdef __GNUC__

#define TREE_VIEW_INTERNAL_ASSERT(expr, ret)     G_STMT_START{          \
     if (!(expr))                                                       \
       {                                                                \
         g_log (G_LOG_DOMAIN,                                           \
                G_LOG_LEVEL_CRITICAL,                                   \
		"%s (%s): assertion `%s' failed.\n"                     \
	        "There is a disparity between the internal view of the GtkTreeView,\n"    \
		"and the GtkTreeModel.  This generally means that the model has changed\n"\
		"without letting the view know.  Any display from now on is likely to\n"  \
		"be incorrect.\n",                                                        \
                G_STRLOC,                                               \
                G_STRFUNC,                                              \
                #expr);                                                 \
         return ret;                                                    \
       };                               }G_STMT_END

#define TREE_VIEW_INTERNAL_ASSERT_VOID(expr)     G_STMT_START{          \
     if (!(expr))                                                       \
       {                                                                \
         g_log (G_LOG_DOMAIN,                                           \
                G_LOG_LEVEL_CRITICAL,                                   \
		"%s (%s): assertion `%s' failed.\n"                     \
	        "There is a disparity between the internal view of the GtkTreeView,\n"    \
		"and the GtkTreeModel.  This generally means that the model has changed\n"\
		"without letting the view know.  Any display from now on is likely to\n"  \
		"be incorrect.\n",                                                        \
                G_STRLOC,                                               \
                G_STRFUNC,                                              \
                #expr);                                                 \
         return;                                                        \
       };                               }G_STMT_END

#else

#define TREE_VIEW_INTERNAL_ASSERT(expr, ret)     G_STMT_START{          \
     if (!(expr))                                                       \
       {                                                                \
         g_log (G_LOG_DOMAIN,                                           \
                G_LOG_LEVEL_CRITICAL,                                   \
		"file %s: line %d: assertion `%s' failed.\n"       \
	        "There is a disparity between the internal view of the GtkTreeView,\n"    \
		"and the GtkTreeModel.  This generally means that the model has changed\n"\
		"without letting the view know.  Any display from now on is likely to\n"  \
		"be incorrect.\n",                                                        \
                __FILE__,                                               \
                __LINE__,                                               \
                #expr);                                                 \
         return ret;                                                    \
       };                               }G_STMT_END

#define TREE_VIEW_INTERNAL_ASSERT_VOID(expr)     G_STMT_START{          \
     if (!(expr))                                                       \
       {                                                                \
         g_log (G_LOG_DOMAIN,                                           \
                G_LOG_LEVEL_CRITICAL,                                   \
		"file %s: line %d: assertion '%s' failed.\n"            \
	        "There is a disparity between the internal view of the GtkTreeView,\n"    \
		"and the GtkTreeModel.  This generally means that the model has changed\n"\
		"without letting the view know.  Any display from now on is likely to\n"  \
		"be incorrect.\n",                                                        \
                __FILE__,                                               \
                __LINE__,                                               \
                #expr);                                                 \
         return;                                                        \
       };                               }G_STMT_END
#endif


/* functions that shouldn't be exported */
void         _gtk_tree_selection_internal_select_node (GtkTreeSelection  *selection,
						       GtkRBNode         *node,
						       GtkRBTree         *tree,
						       GtkTreePath       *path,
                                                       GtkTreeSelectMode  mode,
						       gboolean           override_browse_mode);
void         _gtk_tree_selection_emit_changed         (GtkTreeSelection  *selection);
gboolean     _gtk_tree_view_find_node                 (GtkTreeView       *tree_view,
						       GtkTreePath       *path,
						       GtkRBTree        **tree,
						       GtkRBNode        **node);
GtkTreePath *_gtk_tree_view_find_path                 (GtkTreeView       *tree_view,
						       GtkRBTree         *tree,
						       GtkRBNode         *node);
void         _gtk_tree_view_child_move_resize         (GtkTreeView       *tree_view,
						       GtkWidget         *widget,
						       gint               x,
						       gint               y,
						       gint               width,
						       gint               height);
void         _gtk_tree_view_queue_draw_node           (GtkTreeView       *tree_view,
						       GtkRBTree         *tree,
						       GtkRBNode         *node,
						       const GdkRectangle *clip_rect);

void _gtk_tree_view_column_realize_button   (GtkTreeViewColumn *column);
void _gtk_tree_view_column_unrealize_button (GtkTreeViewColumn *column);
void _gtk_tree_view_column_set_tree_view    (GtkTreeViewColumn *column,
					     GtkTreeView       *tree_view);
void _gtk_tree_view_column_unset_model      (GtkTreeViewColumn *column,
					     GtkTreeModel      *old_model);
void _gtk_tree_view_column_unset_tree_view  (GtkTreeViewColumn *column);
void _gtk_tree_view_column_set_width        (GtkTreeViewColumn *column,
					     gint               width);
void _gtk_tree_view_column_start_drag       (GtkTreeView       *tree_view,
					     GtkTreeViewColumn *column);
gboolean _gtk_tree_view_column_cell_event   (GtkTreeViewColumn  *tree_column,
					     GtkCellEditable   **editable_widget,
					     GdkEvent           *event,
					     gchar              *path_string,
					     const GdkRectangle *background_area,
					     const GdkRectangle *cell_area,
					     guint               flags);
void _gtk_tree_view_column_start_editing (GtkTreeViewColumn *tree_column,
					  GtkCellEditable   *editable_widget);
void _gtk_tree_view_column_stop_editing  (GtkTreeViewColumn *tree_column);
void _gtk_tree_view_install_mark_rows_col_dirty (GtkTreeView *tree_view);
void             _gtk_tree_view_column_autosize          (GtkTreeView       *tree_view,
							  GtkTreeViewColumn *column);

gboolean         _gtk_tree_view_column_has_editable_cell (GtkTreeViewColumn *column);
GtkCellRenderer *_gtk_tree_view_column_get_edited_cell   (GtkTreeViewColumn *column);
gint             _gtk_tree_view_column_count_special_cells (GtkTreeViewColumn *column);
GtkCellRenderer *_gtk_tree_view_column_get_cell_at_pos   (GtkTreeViewColumn *column,
							  gint               x);

GtkTreeSelection* _gtk_tree_selection_new                (void);
GtkTreeSelection* _gtk_tree_selection_new_with_tree_view (GtkTreeView      *tree_view);
void              _gtk_tree_selection_set_tree_view      (GtkTreeSelection *selection,
                                                          GtkTreeView      *tree_view);
gboolean          _gtk_tree_selection_row_is_selectable  (GtkTreeSelection *selection,
							  GtkRBNode        *node,
							  GtkTreePath      *path);

void		  _gtk_tree_view_column_cell_render      (GtkTreeViewColumn  *tree_column,
							  GdkWindow          *window,
							  const GdkRectangle *background_area,
							  const GdkRectangle *cell_area,
							  const GdkRectangle *expose_area,
							  guint               flags);
void		  _gtk_tree_view_column_get_focus_area   (GtkTreeViewColumn  *tree_column,
							  const GdkRectangle *background_area,
							  const GdkRectangle *cell_area,
							  GdkRectangle       *focus_area);
gboolean	  _gtk_tree_view_column_cell_focus       (GtkTreeViewColumn  *tree_column,
							  gint                direction,
							  gboolean            left,
							  gboolean            right);
void		  _gtk_tree_view_column_cell_draw_focus  (GtkTreeViewColumn  *tree_column,
							  GdkWindow          *window,
							  const GdkRectangle *background_area,
							  const GdkRectangle *cell_area,
							  const GdkRectangle *expose_area,
							  guint               flags);
void		  _gtk_tree_view_column_cell_set_dirty	 (GtkTreeViewColumn  *tree_column,
							  gboolean            install_handler);
void              _gtk_tree_view_column_get_neighbor_sizes (GtkTreeViewColumn *column,
							    GtkCellRenderer   *cell,
							    gint              *left,
							    gint              *right);


G_END_DECLS


#endif /* __GTK_TREE_PRIVATE_H__ */

