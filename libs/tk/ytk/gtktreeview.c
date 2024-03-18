/* gtktreeview.c
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


#include "config.h"
#include <string.h>
#include <gdk/gdkkeysyms.h>

#include "gtktreeview.h"
#include "gtkrbtree.h"
#include "gtktreednd.h"
#include "gtktreeprivate.h"
#include "gtkcellrenderer.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkbuildable.h"
#include "gtkbutton.h"
#include "gtkalignment.h"
#include "gtklabel.h"
#include "gtkhbox.h"
#include "gtkvbox.h"
#include "gtkarrow.h"
#include "gtkintl.h"
#include "gtkbindings.h"
#include "gtkcontainer.h"
#include "gtkentry.h"
#include "gtkframe.h"
#include "gtktreemodelsort.h"
#include "gtktooltip.h"
#include "gtkprivate.h"
#include "gtkalias.h"

#define GTK_TREE_VIEW_PRIORITY_VALIDATE (GDK_PRIORITY_REDRAW + 5)
#define GTK_TREE_VIEW_PRIORITY_SCROLL_SYNC (GTK_TREE_VIEW_PRIORITY_VALIDATE + 2)
#define GTK_TREE_VIEW_TIME_MS_PER_IDLE 30
#define SCROLL_EDGE_SIZE 15
#define EXPANDER_EXTRA_PADDING 4
#define GTK_TREE_VIEW_SEARCH_DIALOG_TIMEOUT 5000
#define AUTO_EXPAND_TIMEOUT 500

/* The "background" areas of all rows/cells add up to cover the entire tree.
 * The background includes all inter-row and inter-cell spacing.
 * The "cell" areas are the cell_area passed in to gtk_cell_renderer_render(),
 * i.e. just the cells, no spacing.
 */

#define BACKGROUND_HEIGHT(node) (GTK_RBNODE_GET_HEIGHT (node))
#define CELL_HEIGHT(node, separator) ((BACKGROUND_HEIGHT (node)) - (separator))

/* Translate from bin_window coordinates to rbtree (tree coordinates) and
 * vice versa.
 */
#define TREE_WINDOW_Y_TO_RBTREE_Y(tree_view,y) ((y) + tree_view->priv->dy)
#define RBTREE_Y_TO_TREE_WINDOW_Y(tree_view,y) ((y) - tree_view->priv->dy)

/* This is in bin_window coordinates */
#define BACKGROUND_FIRST_PIXEL(tree_view,tree,node) (RBTREE_Y_TO_TREE_WINDOW_Y (tree_view, _gtk_rbtree_node_find_offset ((tree), (node))))
#define CELL_FIRST_PIXEL(tree_view,tree,node,separator) (BACKGROUND_FIRST_PIXEL (tree_view,tree,node) + separator/2)

#define ROW_HEIGHT(tree_view,height) \
  ((height > 0) ? (height) : (tree_view)->priv->expander_size)


typedef struct _GtkTreeViewChild GtkTreeViewChild;
struct _GtkTreeViewChild
{
  GtkWidget *widget;
  gint x;
  gint y;
  gint width;
  gint height;
};


typedef struct _TreeViewDragInfo TreeViewDragInfo;
struct _TreeViewDragInfo
{
  GdkModifierType start_button_mask;
  GtkTargetList *_unused_source_target_list;
  GdkDragAction source_actions;

  GtkTargetList *_unused_dest_target_list;

  guint source_set : 1;
  guint dest_set : 1;
};


/* Signals */
enum
{
  ROW_ACTIVATED,
  TEST_EXPAND_ROW,
  TEST_COLLAPSE_ROW,
  ROW_EXPANDED,
  ROW_COLLAPSED,
  COLUMNS_CHANGED,
  CURSOR_CHANGED,
  MOVE_CURSOR,
  SELECT_ALL,
  UNSELECT_ALL,
  SELECT_CURSOR_ROW,
  TOGGLE_CURSOR_ROW,
  EXPAND_COLLAPSE_CURSOR_ROW,
  SELECT_CURSOR_PARENT,
  START_INTERACTIVE_SEARCH,
  LAST_SIGNAL
};

/* Properties */
enum {
  PROP_0,
  PROP_MODEL,
  PROP_HADJUSTMENT,
  PROP_VADJUSTMENT,
  PROP_HEADERS_VISIBLE,
  PROP_HEADERS_CLICKABLE,
  PROP_EXPANDER_COLUMN,
  PROP_REORDERABLE,
  PROP_RULES_HINT,
  PROP_ENABLE_SEARCH,
  PROP_SEARCH_COLUMN,
  PROP_FIXED_HEIGHT_MODE,
  PROP_HOVER_SELECTION,
  PROP_HOVER_EXPAND,
  PROP_SHOW_EXPANDERS,
  PROP_LEVEL_INDENTATION,
  PROP_RUBBER_BANDING,
  PROP_ENABLE_GRID_LINES,
  PROP_ENABLE_TREE_LINES,
  PROP_TOOLTIP_COLUMN
};

/* object signals */
static void     gtk_tree_view_finalize             (GObject          *object);
static void     gtk_tree_view_set_property         (GObject         *object,
						    guint            prop_id,
						    const GValue    *value,
						    GParamSpec      *pspec);
static void     gtk_tree_view_get_property         (GObject         *object,
						    guint            prop_id,
						    GValue          *value,
						    GParamSpec      *pspec);

/* gtkobject signals */
static void     gtk_tree_view_destroy              (GtkObject        *object);

/* gtkwidget signals */
static void     gtk_tree_view_realize              (GtkWidget        *widget);
static void     gtk_tree_view_unrealize            (GtkWidget        *widget);
static void     gtk_tree_view_map                  (GtkWidget        *widget);
static void     gtk_tree_view_size_request         (GtkWidget        *widget,
						    GtkRequisition   *requisition);
static void     gtk_tree_view_size_allocate        (GtkWidget        *widget,
						    GtkAllocation    *allocation);
static gboolean gtk_tree_view_expose               (GtkWidget        *widget,
						    GdkEventExpose   *event);
static gboolean gtk_tree_view_key_press            (GtkWidget        *widget,
						    GdkEventKey      *event);
static gboolean gtk_tree_view_key_release          (GtkWidget        *widget,
						    GdkEventKey      *event);
static gboolean gtk_tree_view_motion               (GtkWidget        *widget,
						    GdkEventMotion   *event);
static gboolean gtk_tree_view_enter_notify         (GtkWidget        *widget,
						    GdkEventCrossing *event);
static gboolean gtk_tree_view_leave_notify         (GtkWidget        *widget,
						    GdkEventCrossing *event);
static gboolean gtk_tree_view_button_press         (GtkWidget        *widget,
						    GdkEventButton   *event);
static gboolean gtk_tree_view_button_release       (GtkWidget        *widget,
						    GdkEventButton   *event);
static gboolean gtk_tree_view_grab_broken          (GtkWidget          *widget,
						    GdkEventGrabBroken *event);
#if 0
static gboolean gtk_tree_view_configure            (GtkWidget         *widget,
						    GdkEventConfigure *event);
#endif

static void     gtk_tree_view_set_focus_child      (GtkContainer     *container,
						    GtkWidget        *child);
static gint     gtk_tree_view_focus_out            (GtkWidget        *widget,
						    GdkEventFocus    *event);
static gint     gtk_tree_view_focus                (GtkWidget        *widget,
						    GtkDirectionType  direction);
static void     gtk_tree_view_grab_focus           (GtkWidget        *widget);
static void     gtk_tree_view_style_set            (GtkWidget        *widget,
						    GtkStyle         *previous_style);
static void     gtk_tree_view_grab_notify          (GtkWidget        *widget,
						    gboolean          was_grabbed);
static void     gtk_tree_view_state_changed        (GtkWidget        *widget,
						    GtkStateType      previous_state);

/* container signals */
static void     gtk_tree_view_remove               (GtkContainer     *container,
						    GtkWidget        *widget);
static void     gtk_tree_view_forall               (GtkContainer     *container,
						    gboolean          include_internals,
						    GtkCallback       callback,
						    gpointer          callback_data);

/* Source side drag signals */
static void gtk_tree_view_drag_begin       (GtkWidget        *widget,
                                            GdkDragContext   *context);
static void gtk_tree_view_drag_end         (GtkWidget        *widget,
                                            GdkDragContext   *context);
static void gtk_tree_view_drag_data_get    (GtkWidget        *widget,
                                            GdkDragContext   *context,
                                            GtkSelectionData *selection_data,
                                            guint             info,
                                            guint             time);
static void gtk_tree_view_drag_data_delete (GtkWidget        *widget,
                                            GdkDragContext   *context);

/* Target side drag signals */
static void     gtk_tree_view_drag_leave         (GtkWidget        *widget,
                                                  GdkDragContext   *context,
                                                  guint             time);
static gboolean gtk_tree_view_drag_motion        (GtkWidget        *widget,
                                                  GdkDragContext   *context,
                                                  gint              x,
                                                  gint              y,
                                                  guint             time);
static gboolean gtk_tree_view_drag_drop          (GtkWidget        *widget,
                                                  GdkDragContext   *context,
                                                  gint              x,
                                                  gint              y,
                                                  guint             time);
static void     gtk_tree_view_drag_data_received (GtkWidget        *widget,
                                                  GdkDragContext   *context,
                                                  gint              x,
                                                  gint              y,
                                                  GtkSelectionData *selection_data,
                                                  guint             info,
                                                  guint             time);

/* tree_model signals */
static void gtk_tree_view_set_adjustments                 (GtkTreeView     *tree_view,
							   GtkAdjustment   *hadj,
							   GtkAdjustment   *vadj);
static gboolean gtk_tree_view_real_move_cursor            (GtkTreeView     *tree_view,
							   GtkMovementStep  step,
							   gint             count);
static gboolean gtk_tree_view_real_select_all             (GtkTreeView     *tree_view);
static gboolean gtk_tree_view_real_unselect_all           (GtkTreeView     *tree_view);
static gboolean gtk_tree_view_real_select_cursor_row      (GtkTreeView     *tree_view,
							   gboolean         start_editing);
static gboolean gtk_tree_view_real_toggle_cursor_row      (GtkTreeView     *tree_view);
static gboolean gtk_tree_view_real_expand_collapse_cursor_row (GtkTreeView     *tree_view,
							       gboolean         logical,
							       gboolean         expand,
							       gboolean         open_all);
static gboolean gtk_tree_view_real_select_cursor_parent   (GtkTreeView     *tree_view);
static void gtk_tree_view_row_changed                     (GtkTreeModel    *model,
							   GtkTreePath     *path,
							   GtkTreeIter     *iter,
							   gpointer         data);
static void gtk_tree_view_row_inserted                    (GtkTreeModel    *model,
							   GtkTreePath     *path,
							   GtkTreeIter     *iter,
							   gpointer         data);
static void gtk_tree_view_row_has_child_toggled           (GtkTreeModel    *model,
							   GtkTreePath     *path,
							   GtkTreeIter     *iter,
							   gpointer         data);
static void gtk_tree_view_row_deleted                     (GtkTreeModel    *model,
							   GtkTreePath     *path,
							   gpointer         data);
static void gtk_tree_view_rows_reordered                  (GtkTreeModel    *model,
							   GtkTreePath     *parent,
							   GtkTreeIter     *iter,
							   gint            *new_order,
							   gpointer         data);

/* Incremental reflow */
static gboolean validate_row             (GtkTreeView *tree_view,
					  GtkRBTree   *tree,
					  GtkRBNode   *node,
					  GtkTreeIter *iter,
					  GtkTreePath *path);
static void     validate_visible_area    (GtkTreeView *tree_view);
static gboolean validate_rows_handler    (GtkTreeView *tree_view);
static gboolean do_validate_rows         (GtkTreeView *tree_view,
					  gboolean     size_request);
static gboolean validate_rows            (GtkTreeView *tree_view);
static gboolean presize_handler_callback (gpointer     data);
static void     install_presize_handler  (GtkTreeView *tree_view);
static void     install_scroll_sync_handler (GtkTreeView *tree_view);
static void     gtk_tree_view_set_top_row   (GtkTreeView *tree_view,
					     GtkTreePath *path,
					     gint         offset);
static void	gtk_tree_view_dy_to_top_row (GtkTreeView *tree_view);
static void     gtk_tree_view_top_row_to_dy (GtkTreeView *tree_view);
static void     invalidate_empty_focus      (GtkTreeView *tree_view);

/* Internal functions */
static gboolean gtk_tree_view_is_expander_column             (GtkTreeView        *tree_view,
							      GtkTreeViewColumn  *column);
static void     gtk_tree_view_add_move_binding               (GtkBindingSet      *binding_set,
							      guint               keyval,
							      guint               modmask,
							      gboolean            add_shifted_binding,
							      GtkMovementStep     step,
							      gint                count);
static gint     gtk_tree_view_unref_and_check_selection_tree (GtkTreeView        *tree_view,
							      GtkRBTree          *tree);
static void     gtk_tree_view_queue_draw_path                (GtkTreeView        *tree_view,
							      GtkTreePath        *path,
							      const GdkRectangle *clip_rect);
static void     gtk_tree_view_queue_draw_arrow               (GtkTreeView        *tree_view,
							      GtkRBTree          *tree,
							      GtkRBNode          *node,
							      const GdkRectangle *clip_rect);
static void     gtk_tree_view_draw_arrow                     (GtkTreeView        *tree_view,
							      GtkRBTree          *tree,
							      GtkRBNode          *node,
							      gint                x,
							      gint                y);
static void     gtk_tree_view_get_arrow_xrange               (GtkTreeView        *tree_view,
							      GtkRBTree          *tree,
							      gint               *x1,
							      gint               *x2);
static gint     gtk_tree_view_new_column_width               (GtkTreeView        *tree_view,
							      gint                i,
							      gint               *x);
static void     gtk_tree_view_adjustment_changed             (GtkAdjustment      *adjustment,
							      GtkTreeView        *tree_view);
static void     gtk_tree_view_build_tree                     (GtkTreeView        *tree_view,
							      GtkRBTree          *tree,
							      GtkTreeIter        *iter,
							      gint                depth,
							      gboolean            recurse);
static void     gtk_tree_view_clamp_node_visible             (GtkTreeView        *tree_view,
							      GtkRBTree          *tree,
							      GtkRBNode          *node);
static void     gtk_tree_view_clamp_column_visible           (GtkTreeView        *tree_view,
							      GtkTreeViewColumn  *column,
							      gboolean            focus_to_cell);
static gboolean gtk_tree_view_maybe_begin_dragging_row       (GtkTreeView        *tree_view,
							      GdkEventMotion     *event);
static void     gtk_tree_view_focus_to_cursor                (GtkTreeView        *tree_view);
static void     gtk_tree_view_move_cursor_up_down            (GtkTreeView        *tree_view,
							      gint                count);
static void     gtk_tree_view_move_cursor_page_up_down       (GtkTreeView        *tree_view,
							      gint                count);
static void     gtk_tree_view_move_cursor_left_right         (GtkTreeView        *tree_view,
							      gint                count);
static void     gtk_tree_view_move_cursor_start_end          (GtkTreeView        *tree_view,
							      gint                count);
static gboolean gtk_tree_view_real_collapse_row              (GtkTreeView        *tree_view,
							      GtkTreePath        *path,
							      GtkRBTree          *tree,
							      GtkRBNode          *node,
							      gboolean            animate);
static gboolean gtk_tree_view_real_expand_row                (GtkTreeView        *tree_view,
							      GtkTreePath        *path,
							      GtkRBTree          *tree,
							      GtkRBNode          *node,
							      gboolean            open_all,
							      gboolean            animate);
static void     gtk_tree_view_real_set_cursor                (GtkTreeView        *tree_view,
							      GtkTreePath        *path,
							      gboolean            clear_and_select,
							      gboolean            clamp_node);
static gboolean gtk_tree_view_has_special_cell               (GtkTreeView        *tree_view);
static void     column_sizing_notify                         (GObject            *object,
                                                              GParamSpec         *pspec,
                                                              gpointer            data);
static gboolean expand_collapse_timeout                      (gpointer            data);
static void     add_expand_collapse_timeout                  (GtkTreeView        *tree_view,
                                                              GtkRBTree          *tree,
                                                              GtkRBNode          *node,
                                                              gboolean            expand);
static void     remove_expand_collapse_timeout               (GtkTreeView        *tree_view);
static void     cancel_arrow_animation                       (GtkTreeView        *tree_view);
static gboolean do_expand_collapse                           (GtkTreeView        *tree_view);
static void     gtk_tree_view_stop_rubber_band               (GtkTreeView        *tree_view);
static void     update_prelight                              (GtkTreeView        *tree_view,
                                                              int                 x,
                                                              int                 y);

/* interactive search */
static void     gtk_tree_view_ensure_interactive_directory (GtkTreeView *tree_view);
static void     gtk_tree_view_search_dialog_hide     (GtkWidget        *search_dialog,
							 GtkTreeView      *tree_view);
static void     gtk_tree_view_search_position_func      (GtkTreeView      *tree_view,
							 GtkWidget        *search_dialog,
							 gpointer          user_data);
static void     gtk_tree_view_search_disable_popdown    (GtkEntry         *entry,
							 GtkMenu          *menu,
							 gpointer          data);
static void     gtk_tree_view_search_preedit_changed    (GtkIMContext     *im_context,
							 GtkTreeView      *tree_view);
static void     gtk_tree_view_search_activate           (GtkEntry         *entry,
							 GtkTreeView      *tree_view);
static gboolean gtk_tree_view_real_search_enable_popdown(gpointer          data);
static void     gtk_tree_view_search_enable_popdown     (GtkWidget        *widget,
							 gpointer          data);
static gboolean gtk_tree_view_search_delete_event       (GtkWidget        *widget,
							 GdkEventAny      *event,
							 GtkTreeView      *tree_view);
static gboolean gtk_tree_view_search_button_press_event (GtkWidget        *widget,
							 GdkEventButton   *event,
							 GtkTreeView      *tree_view);
static gboolean gtk_tree_view_search_scroll_event       (GtkWidget        *entry,
							 GdkEventScroll   *event,
							 GtkTreeView      *tree_view);
static gboolean gtk_tree_view_search_key_press_event    (GtkWidget        *entry,
							 GdkEventKey      *event,
							 GtkTreeView      *tree_view);
static gboolean gtk_tree_view_search_move               (GtkWidget        *window,
							 GtkTreeView      *tree_view,
							 gboolean          up);
static gboolean gtk_tree_view_search_equal_func         (GtkTreeModel     *model,
							 gint              column,
							 const gchar      *key,
							 GtkTreeIter      *iter,
							 gpointer          search_data);
static gboolean gtk_tree_view_search_iter               (GtkTreeModel     *model,
							 GtkTreeSelection *selection,
							 GtkTreeIter      *iter,
							 const gchar      *text,
							 gint             *count,
							 gint              n);
static void     gtk_tree_view_search_init               (GtkWidget        *entry,
							 GtkTreeView      *tree_view);
static void     gtk_tree_view_put                       (GtkTreeView      *tree_view,
							 GtkWidget        *child_widget,
							 gint              x,
							 gint              y,
							 gint              width,
							 gint              height);
static gboolean gtk_tree_view_start_editing             (GtkTreeView      *tree_view,
							 GtkTreePath      *cursor_path);
static void gtk_tree_view_real_start_editing (GtkTreeView       *tree_view,
					      GtkTreeViewColumn *column,
					      GtkTreePath       *path,
					      GtkCellEditable   *cell_editable,
					      GdkRectangle      *cell_area,
					      GdkEvent          *event,
					      guint              flags);
static void gtk_tree_view_stop_editing                  (GtkTreeView *tree_view,
							 gboolean     cancel_editing);
static gboolean gtk_tree_view_real_start_interactive_search (GtkTreeView *tree_view,
							     gboolean     keybinding);
static gboolean gtk_tree_view_start_interactive_search      (GtkTreeView *tree_view);
static GtkTreeViewColumn *gtk_tree_view_get_drop_column (GtkTreeView       *tree_view,
							 GtkTreeViewColumn *column,
							 gint               drop_position);

/* GtkBuildable */
static void     gtk_tree_view_buildable_add_child          (GtkBuildable      *tree_view,
							    GtkBuilder        *builder,
							    GObject           *child,
							    const gchar       *type);
static GObject *gtk_tree_view_buildable_get_internal_child (GtkBuildable      *buildable,
							    GtkBuilder        *builder,
							    const gchar       *childname);
static void     gtk_tree_view_buildable_init               (GtkBuildableIface *iface);


static gboolean scroll_row_timeout                   (gpointer     data);
static void     add_scroll_timeout                   (GtkTreeView *tree_view);
static void     remove_scroll_timeout                (GtkTreeView *tree_view);

static guint tree_view_signals [LAST_SIGNAL] = { 0 };



/* GType Methods
 */

G_DEFINE_TYPE_WITH_CODE (GtkTreeView, gtk_tree_view, GTK_TYPE_CONTAINER,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
						gtk_tree_view_buildable_init))

static void
gtk_tree_view_class_init (GtkTreeViewClass *class)
{
  GObjectClass *o_class;
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;
  GtkBindingSet *binding_set;

  binding_set = gtk_binding_set_by_class (class);

  o_class = (GObjectClass *) class;
  object_class = (GtkObjectClass *) class;
  widget_class = (GtkWidgetClass *) class;
  container_class = (GtkContainerClass *) class;

  /* GObject signals */
  o_class->set_property = gtk_tree_view_set_property;
  o_class->get_property = gtk_tree_view_get_property;
  o_class->finalize = gtk_tree_view_finalize;

  /* GtkObject signals */
  object_class->destroy = gtk_tree_view_destroy;

  /* GtkWidget signals */
  widget_class->map = gtk_tree_view_map;
  widget_class->realize = gtk_tree_view_realize;
  widget_class->unrealize = gtk_tree_view_unrealize;
  widget_class->size_request = gtk_tree_view_size_request;
  widget_class->size_allocate = gtk_tree_view_size_allocate;
  widget_class->button_press_event = gtk_tree_view_button_press;
  widget_class->button_release_event = gtk_tree_view_button_release;
  widget_class->grab_broken_event = gtk_tree_view_grab_broken;
  /*widget_class->configure_event = gtk_tree_view_configure;*/
  widget_class->motion_notify_event = gtk_tree_view_motion;
  widget_class->expose_event = gtk_tree_view_expose;
  widget_class->key_press_event = gtk_tree_view_key_press;
  widget_class->key_release_event = gtk_tree_view_key_release;
  widget_class->enter_notify_event = gtk_tree_view_enter_notify;
  widget_class->leave_notify_event = gtk_tree_view_leave_notify;
  widget_class->focus_out_event = gtk_tree_view_focus_out;
  widget_class->drag_begin = gtk_tree_view_drag_begin;
  widget_class->drag_end = gtk_tree_view_drag_end;
  widget_class->drag_data_get = gtk_tree_view_drag_data_get;
  widget_class->drag_data_delete = gtk_tree_view_drag_data_delete;
  widget_class->drag_leave = gtk_tree_view_drag_leave;
  widget_class->drag_motion = gtk_tree_view_drag_motion;
  widget_class->drag_drop = gtk_tree_view_drag_drop;
  widget_class->drag_data_received = gtk_tree_view_drag_data_received;
  widget_class->focus = gtk_tree_view_focus;
  widget_class->grab_focus = gtk_tree_view_grab_focus;
  widget_class->style_set = gtk_tree_view_style_set;
  widget_class->grab_notify = gtk_tree_view_grab_notify;
  widget_class->state_changed = gtk_tree_view_state_changed;

  /* GtkContainer signals */
  container_class->remove = gtk_tree_view_remove;
  container_class->forall = gtk_tree_view_forall;
  container_class->set_focus_child = gtk_tree_view_set_focus_child;

  class->set_scroll_adjustments = gtk_tree_view_set_adjustments;
  class->move_cursor = gtk_tree_view_real_move_cursor;
  class->select_all = gtk_tree_view_real_select_all;
  class->unselect_all = gtk_tree_view_real_unselect_all;
  class->select_cursor_row = gtk_tree_view_real_select_cursor_row;
  class->toggle_cursor_row = gtk_tree_view_real_toggle_cursor_row;
  class->expand_collapse_cursor_row = gtk_tree_view_real_expand_collapse_cursor_row;
  class->select_cursor_parent = gtk_tree_view_real_select_cursor_parent;
  class->start_interactive_search = gtk_tree_view_start_interactive_search;

  /* Properties */

  g_object_class_install_property (o_class,
                                   PROP_MODEL,
                                   g_param_spec_object ("model",
							P_("TreeView Model"),
							P_("The model for the tree view"),
							GTK_TYPE_TREE_MODEL,
							GTK_PARAM_READWRITE));

  g_object_class_install_property (o_class,
                                   PROP_HADJUSTMENT,
                                   g_param_spec_object ("hadjustment",
							P_("Horizontal Adjustment"),
                                                        P_("Horizontal Adjustment for the widget"),
                                                        GTK_TYPE_ADJUSTMENT,
                                                        GTK_PARAM_READWRITE));

  g_object_class_install_property (o_class,
                                   PROP_VADJUSTMENT,
                                   g_param_spec_object ("vadjustment",
							P_("Vertical Adjustment"),
                                                        P_("Vertical Adjustment for the widget"),
                                                        GTK_TYPE_ADJUSTMENT,
                                                        GTK_PARAM_READWRITE));

  g_object_class_install_property (o_class,
                                   PROP_HEADERS_VISIBLE,
                                   g_param_spec_boolean ("headers-visible",
							 P_("Headers Visible"),
							 P_("Show the column header buttons"),
							 TRUE,
							 GTK_PARAM_READWRITE));

  g_object_class_install_property (o_class,
                                   PROP_HEADERS_CLICKABLE,
                                   g_param_spec_boolean ("headers-clickable",
							 P_("Headers Clickable"),
							 P_("Column headers respond to click events"),
							 TRUE,
							 GTK_PARAM_READWRITE));

  g_object_class_install_property (o_class,
                                   PROP_EXPANDER_COLUMN,
                                   g_param_spec_object ("expander-column",
							P_("Expander Column"),
							P_("Set the column for the expander column"),
							GTK_TYPE_TREE_VIEW_COLUMN,
							GTK_PARAM_READWRITE));

  g_object_class_install_property (o_class,
                                   PROP_REORDERABLE,
                                   g_param_spec_boolean ("reorderable",
							 P_("Reorderable"),
							 P_("View is reorderable"),
							 FALSE,
							 GTK_PARAM_READWRITE));

  g_object_class_install_property (o_class,
                                   PROP_RULES_HINT,
                                   g_param_spec_boolean ("rules-hint",
							 P_("Rules Hint"),
							 P_("Set a hint to the theme engine to draw rows in alternating colors"),
							 FALSE,
							 GTK_PARAM_READWRITE));

    g_object_class_install_property (o_class,
				     PROP_ENABLE_SEARCH,
				     g_param_spec_boolean ("enable-search",
							   P_("Enable Search"),
							   P_("View allows user to search through columns interactively"),
							   TRUE,
							   GTK_PARAM_READWRITE));

    g_object_class_install_property (o_class,
				     PROP_SEARCH_COLUMN,
				     g_param_spec_int ("search-column",
						       P_("Search Column"),
						       P_("Model column to search through during interactive search"),
						       -1,
						       G_MAXINT,
						       -1,
						       GTK_PARAM_READWRITE));

    /**
     * GtkTreeView:fixed-height-mode:
     *
     * Setting the ::fixed-height-mode property to %TRUE speeds up 
     * #GtkTreeView by assuming that all rows have the same height. 
     * Only enable this option if all rows are the same height.  
     * Please see gtk_tree_view_set_fixed_height_mode() for more 
     * information on this option.
     *
     * Since: 2.4
     **/
    g_object_class_install_property (o_class,
                                     PROP_FIXED_HEIGHT_MODE,
                                     g_param_spec_boolean ("fixed-height-mode",
                                                           P_("Fixed Height Mode"),
                                                           P_("Speeds up GtkTreeView by assuming that all rows have the same height"),
                                                           FALSE,
                                                           GTK_PARAM_READWRITE));
    
    /**
     * GtkTreeView:hover-selection:
     * 
     * Enables of disables the hover selection mode of @tree_view.
     * Hover selection makes the selected row follow the pointer.
     * Currently, this works only for the selection modes 
     * %GTK_SELECTION_SINGLE and %GTK_SELECTION_BROWSE.
     *
     * This mode is primarily intended for treeviews in popups, e.g.
     * in #GtkComboBox or #GtkEntryCompletion.
     *
     * Since: 2.6
     */
    g_object_class_install_property (o_class,
                                     PROP_HOVER_SELECTION,
                                     g_param_spec_boolean ("hover-selection",
                                                           P_("Hover Selection"),
                                                           P_("Whether the selection should follow the pointer"),
                                                           FALSE,
                                                           GTK_PARAM_READWRITE));

    /**
     * GtkTreeView:hover-expand:
     * 
     * Enables of disables the hover expansion mode of @tree_view.
     * Hover expansion makes rows expand or collapse if the pointer moves 
     * over them.
     *
     * This mode is primarily intended for treeviews in popups, e.g.
     * in #GtkComboBox or #GtkEntryCompletion.
     *
     * Since: 2.6
     */
    g_object_class_install_property (o_class,
                                     PROP_HOVER_EXPAND,
                                     g_param_spec_boolean ("hover-expand",
                                                           P_("Hover Expand"),
                                                           P_("Whether rows should be expanded/collapsed when the pointer moves over them"),
                                                           FALSE,
                                                           GTK_PARAM_READWRITE));

    /**
     * GtkTreeView:show-expanders:
     *
     * %TRUE if the view has expanders.
     *
     * Since: 2.12
     */
    g_object_class_install_property (o_class,
				     PROP_SHOW_EXPANDERS,
				     g_param_spec_boolean ("show-expanders",
							   P_("Show Expanders"),
							   P_("View has expanders"),
							   TRUE,
							   GTK_PARAM_READWRITE));

    /**
     * GtkTreeView:level-indentation:
     *
     * Extra indentation for each level.
     *
     * Since: 2.12
     */
    g_object_class_install_property (o_class,
				     PROP_LEVEL_INDENTATION,
				     g_param_spec_int ("level-indentation",
						       P_("Level Indentation"),
						       P_("Extra indentation for each level"),
						       0,
						       G_MAXINT,
						       0,
						       GTK_PARAM_READWRITE));

    g_object_class_install_property (o_class,
                                     PROP_RUBBER_BANDING,
                                     g_param_spec_boolean ("rubber-banding",
                                                           P_("Rubber Banding"),
                                                           P_("Whether to enable selection of multiple items by dragging the mouse pointer"),
                                                           FALSE,
                                                           GTK_PARAM_READWRITE));

    g_object_class_install_property (o_class,
                                     PROP_ENABLE_GRID_LINES,
                                     g_param_spec_enum ("enable-grid-lines",
							P_("Enable Grid Lines"),
							P_("Whether grid lines should be drawn in the tree view"),
							GTK_TYPE_TREE_VIEW_GRID_LINES,
							GTK_TREE_VIEW_GRID_LINES_NONE,
							GTK_PARAM_READWRITE));

    g_object_class_install_property (o_class,
                                     PROP_ENABLE_TREE_LINES,
                                     g_param_spec_boolean ("enable-tree-lines",
                                                           P_("Enable Tree Lines"),
                                                           P_("Whether tree lines should be drawn in the tree view"),
                                                           FALSE,
                                                           GTK_PARAM_READWRITE));

    g_object_class_install_property (o_class,
				     PROP_TOOLTIP_COLUMN,
				     g_param_spec_int ("tooltip-column",
						       P_("Tooltip Column"),
						       P_("The column in the model containing the tooltip texts for the rows"),
						       -1,
						       G_MAXINT,
						       -1,
						       GTK_PARAM_READWRITE));

  /* Style properties */
#define _TREE_VIEW_EXPANDER_SIZE 12
#define _TREE_VIEW_VERTICAL_SEPARATOR 2
#define _TREE_VIEW_HORIZONTAL_SEPARATOR 2

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("expander-size",
							     P_("Expander Size"),
							     P_("Size of the expander arrow"),
							     0,
							     G_MAXINT,
							     _TREE_VIEW_EXPANDER_SIZE,
							     GTK_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("vertical-separator",
							     P_("Vertical Separator Width"),
							     P_("Vertical space between cells.  Must be an even number"),
							     0,
							     G_MAXINT,
							     _TREE_VIEW_VERTICAL_SEPARATOR,
							     GTK_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("horizontal-separator",
							     P_("Horizontal Separator Width"),
							     P_("Horizontal space between cells.  Must be an even number"),
							     0,
							     G_MAXINT,
							     _TREE_VIEW_HORIZONTAL_SEPARATOR,
							     GTK_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_boolean ("allow-rules",
								 P_("Allow Rules"),
								 P_("Allow drawing of alternating color rows"),
								 TRUE,
								 GTK_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_boolean ("indent-expanders",
								 P_("Indent Expanders"),
								 P_("Make the expanders indented"),
								 TRUE,
								 GTK_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_boxed ("even-row-color",
                                                               P_("Even Row Color"),
                                                               P_("Color to use for even rows"),
							       GDK_TYPE_COLOR,
							       GTK_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_boxed ("odd-row-color",
                                                               P_("Odd Row Color"),
                                                               P_("Color to use for odd rows"),
							       GDK_TYPE_COLOR,
							       GTK_PARAM_READABLE));

  /**
   * GtkTreeView:row-ending-details:
   *
   * Enable extended row background themeing
   *
   * Deprecated: 2.22: This style property will be removed in GTK+ 3
   */
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_boolean ("row-ending-details",
								 P_("Row Ending details"),
								 P_("Enable extended row background theming"),
								 FALSE,
								 GTK_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("grid-line-width",
							     P_("Grid line width"),
							     P_("Width, in pixels, of the tree view grid lines"),
							     0, G_MAXINT, 1,
							     GTK_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("tree-line-width",
							     P_("Tree line width"),
							     P_("Width, in pixels, of the tree view lines"),
							     0, G_MAXINT, 1,
							     GTK_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_string ("grid-line-pattern",
								P_("Grid line pattern"),
								P_("Dash pattern used to draw the tree view grid lines"),
								"\1\1",
								GTK_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_string ("tree-line-pattern",
								P_("Tree line pattern"),
								P_("Dash pattern used to draw the tree view lines"),
								"\1\1",
								GTK_PARAM_READABLE));

  /* Signals */
  /**
   * GtkTreeView::set-scroll-adjustments
   * @horizontal: the horizontal #GtkAdjustment
   * @vertical: the vertical #GtkAdjustment
   *
   * Set the scroll adjustments for the tree view. Usually scrolled containers
   * like #GtkScrolledWindow will emit this signal to connect two instances
   * of #GtkScrollbar to the scroll directions of the #GtkTreeView.
   */
  widget_class->set_scroll_adjustments_signal =
    g_signal_new (I_("set-scroll-adjustments"),
		  G_TYPE_FROM_CLASS (o_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkTreeViewClass, set_scroll_adjustments),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT_OBJECT,
		  G_TYPE_NONE, 2,
		  GTK_TYPE_ADJUSTMENT,
		  GTK_TYPE_ADJUSTMENT);

  /**
   * GtkTreeView::row-activated:
   * @tree_view: the object on which the signal is emitted
   * @path: the #GtkTreePath for the activated row
   * @column: the #GtkTreeViewColumn in which the activation occurred
   *
   * The "row-activated" signal is emitted when the method
   * gtk_tree_view_row_activated() is called or the user double clicks 
   * a treeview row. It is also emitted when a non-editable row is 
   * selected and one of the keys: Space, Shift+Space, Return or 
   * Enter is pressed.
   * 
   * For selection handling refer to the <link linkend="TreeWidget">tree 
   * widget conceptual overview</link> as well as #GtkTreeSelection.
   */
  tree_view_signals[ROW_ACTIVATED] =
    g_signal_new (I_("row-activated"),
		  G_TYPE_FROM_CLASS (o_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkTreeViewClass, row_activated),
		  NULL, NULL,
		  _gtk_marshal_VOID__BOXED_OBJECT,
		  G_TYPE_NONE, 2,
		  GTK_TYPE_TREE_PATH,
		  GTK_TYPE_TREE_VIEW_COLUMN);

  /**
   * GtkTreeView::test-expand-row:
   * @tree_view: the object on which the signal is emitted
   * @iter: the tree iter of the row to expand
   * @path: a tree path that points to the row 
   * 
   * The given row is about to be expanded (show its children nodes). Use this
   * signal if you need to control the expandability of individual rows.
   *
   * Returns: %FALSE to allow expansion, %TRUE to reject
   */
  tree_view_signals[TEST_EXPAND_ROW] =
    g_signal_new (I_("test-expand-row"),
		  G_TYPE_FROM_CLASS (o_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkTreeViewClass, test_expand_row),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED_BOXED,
		  G_TYPE_BOOLEAN, 2,
		  GTK_TYPE_TREE_ITER,
		  GTK_TYPE_TREE_PATH);

  /**
   * GtkTreeView::test-collapse-row:
   * @tree_view: the object on which the signal is emitted
   * @iter: the tree iter of the row to collapse
   * @path: a tree path that points to the row 
   * 
   * The given row is about to be collapsed (hide its children nodes). Use this
   * signal if you need to control the collapsibility of individual rows.
   *
   * Returns: %FALSE to allow collapsing, %TRUE to reject
   */
  tree_view_signals[TEST_COLLAPSE_ROW] =
    g_signal_new (I_("test-collapse-row"),
		  G_TYPE_FROM_CLASS (o_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkTreeViewClass, test_collapse_row),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED_BOXED,
		  G_TYPE_BOOLEAN, 2,
		  GTK_TYPE_TREE_ITER,
		  GTK_TYPE_TREE_PATH);

  /**
   * GtkTreeView::row-expanded:
   * @tree_view: the object on which the signal is emitted
   * @iter: the tree iter of the expanded row
   * @path: a tree path that points to the row 
   * 
   * The given row has been expanded (child nodes are shown).
   */
  tree_view_signals[ROW_EXPANDED] =
    g_signal_new (I_("row-expanded"),
		  G_TYPE_FROM_CLASS (o_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkTreeViewClass, row_expanded),
		  NULL, NULL,
		  _gtk_marshal_VOID__BOXED_BOXED,
		  G_TYPE_NONE, 2,
		  GTK_TYPE_TREE_ITER,
		  GTK_TYPE_TREE_PATH);

  /**
   * GtkTreeView::row-collapsed:
   * @tree_view: the object on which the signal is emitted
   * @iter: the tree iter of the collapsed row
   * @path: a tree path that points to the row 
   * 
   * The given row has been collapsed (child nodes are hidden).
   */
  tree_view_signals[ROW_COLLAPSED] =
    g_signal_new (I_("row-collapsed"),
		  G_TYPE_FROM_CLASS (o_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkTreeViewClass, row_collapsed),
		  NULL, NULL,
		  _gtk_marshal_VOID__BOXED_BOXED,
		  G_TYPE_NONE, 2,
		  GTK_TYPE_TREE_ITER,
		  GTK_TYPE_TREE_PATH);

  /**
   * GtkTreeView::columns-changed:
   * @tree_view: the object on which the signal is emitted 
   * 
   * The number of columns of the treeview has changed.
   */
  tree_view_signals[COLUMNS_CHANGED] =
    g_signal_new (I_("columns-changed"),
		  G_TYPE_FROM_CLASS (o_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkTreeViewClass, columns_changed),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * GtkTreeView::cursor-changed:
   * @tree_view: the object on which the signal is emitted
   * 
   * The position of the cursor (focused cell) has changed.
   */
  tree_view_signals[CURSOR_CHANGED] =
    g_signal_new (I_("cursor-changed"),
		  G_TYPE_FROM_CLASS (o_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkTreeViewClass, cursor_changed),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  tree_view_signals[MOVE_CURSOR] =
    g_signal_new (I_("move-cursor"),
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkTreeViewClass, move_cursor),
		  NULL, NULL,
		  _gtk_marshal_BOOLEAN__ENUM_INT,
		  G_TYPE_BOOLEAN, 2,
		  GTK_TYPE_MOVEMENT_STEP,
		  G_TYPE_INT);

  tree_view_signals[SELECT_ALL] =
    g_signal_new (I_("select-all"),
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkTreeViewClass, select_all),
		  NULL, NULL,
		  _gtk_marshal_BOOLEAN__VOID,
		  G_TYPE_BOOLEAN, 0);

  tree_view_signals[UNSELECT_ALL] =
    g_signal_new (I_("unselect-all"),
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkTreeViewClass, unselect_all),
		  NULL, NULL,
		  _gtk_marshal_BOOLEAN__VOID,
		  G_TYPE_BOOLEAN, 0);

  tree_view_signals[SELECT_CURSOR_ROW] =
    g_signal_new (I_("select-cursor-row"),
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkTreeViewClass, select_cursor_row),
		  NULL, NULL,
		  _gtk_marshal_BOOLEAN__BOOLEAN,
		  G_TYPE_BOOLEAN, 1,
		  G_TYPE_BOOLEAN);

  tree_view_signals[TOGGLE_CURSOR_ROW] =
    g_signal_new (I_("toggle-cursor-row"),
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkTreeViewClass, toggle_cursor_row),
		  NULL, NULL,
		  _gtk_marshal_BOOLEAN__VOID,
		  G_TYPE_BOOLEAN, 0);

  tree_view_signals[EXPAND_COLLAPSE_CURSOR_ROW] =
    g_signal_new (I_("expand-collapse-cursor-row"),
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkTreeViewClass, expand_collapse_cursor_row),
		  NULL, NULL,
		  _gtk_marshal_BOOLEAN__BOOLEAN_BOOLEAN_BOOLEAN,
		  G_TYPE_BOOLEAN, 3,
		  G_TYPE_BOOLEAN,
		  G_TYPE_BOOLEAN,
		  G_TYPE_BOOLEAN);

  tree_view_signals[SELECT_CURSOR_PARENT] =
    g_signal_new (I_("select-cursor-parent"),
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkTreeViewClass, select_cursor_parent),
		  NULL, NULL,
		  _gtk_marshal_BOOLEAN__VOID,
		  G_TYPE_BOOLEAN, 0);

  tree_view_signals[START_INTERACTIVE_SEARCH] =
    g_signal_new (I_("start-interactive-search"),
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkTreeViewClass, start_interactive_search),
		  NULL, NULL,
		  _gtk_marshal_BOOLEAN__VOID,
		  G_TYPE_BOOLEAN, 0);

  /* Key bindings */
  gtk_tree_view_add_move_binding (binding_set, GDK_Up, 0, TRUE,
				  GTK_MOVEMENT_DISPLAY_LINES, -1);
  gtk_tree_view_add_move_binding (binding_set, GDK_KP_Up, 0, TRUE,
				  GTK_MOVEMENT_DISPLAY_LINES, -1);

  gtk_tree_view_add_move_binding (binding_set, GDK_Down, 0, TRUE,
				  GTK_MOVEMENT_DISPLAY_LINES, 1);
  gtk_tree_view_add_move_binding (binding_set, GDK_KP_Down, 0, TRUE,
				  GTK_MOVEMENT_DISPLAY_LINES, 1);

  gtk_tree_view_add_move_binding (binding_set, GDK_p, GDK_CONTROL_MASK, FALSE,
				  GTK_MOVEMENT_DISPLAY_LINES, -1);

  gtk_tree_view_add_move_binding (binding_set, GDK_n, GDK_CONTROL_MASK, FALSE,
				  GTK_MOVEMENT_DISPLAY_LINES, 1);

  gtk_tree_view_add_move_binding (binding_set, GDK_Home, 0, TRUE,
				  GTK_MOVEMENT_BUFFER_ENDS, -1);
  gtk_tree_view_add_move_binding (binding_set, GDK_KP_Home, 0, TRUE,
				  GTK_MOVEMENT_BUFFER_ENDS, -1);

  gtk_tree_view_add_move_binding (binding_set, GDK_End, 0, TRUE,
				  GTK_MOVEMENT_BUFFER_ENDS, 1);
  gtk_tree_view_add_move_binding (binding_set, GDK_KP_End, 0, TRUE,
				  GTK_MOVEMENT_BUFFER_ENDS, 1);

  gtk_tree_view_add_move_binding (binding_set, GDK_Page_Up, 0, TRUE,
				  GTK_MOVEMENT_PAGES, -1);
  gtk_tree_view_add_move_binding (binding_set, GDK_KP_Page_Up, 0, TRUE,
				  GTK_MOVEMENT_PAGES, -1);

  gtk_tree_view_add_move_binding (binding_set, GDK_Page_Down, 0, TRUE,
				  GTK_MOVEMENT_PAGES, 1);
  gtk_tree_view_add_move_binding (binding_set, GDK_KP_Page_Down, 0, TRUE,
				  GTK_MOVEMENT_PAGES, 1);


  gtk_binding_entry_add_signal (binding_set, GDK_Right, 0, "move-cursor", 2,
				G_TYPE_ENUM, GTK_MOVEMENT_VISUAL_POSITIONS,
				G_TYPE_INT, 1);

  gtk_binding_entry_add_signal (binding_set, GDK_Left, 0, "move-cursor", 2,
				G_TYPE_ENUM, GTK_MOVEMENT_VISUAL_POSITIONS,
				G_TYPE_INT, -1);

  gtk_binding_entry_add_signal (binding_set, GDK_KP_Right, 0, "move-cursor", 2,
				G_TYPE_ENUM, GTK_MOVEMENT_VISUAL_POSITIONS,
				G_TYPE_INT, 1);

  gtk_binding_entry_add_signal (binding_set, GDK_KP_Left, 0, "move-cursor", 2,
				G_TYPE_ENUM, GTK_MOVEMENT_VISUAL_POSITIONS,
				G_TYPE_INT, -1);

  gtk_binding_entry_add_signal (binding_set, GDK_Right, GDK_CONTROL_MASK,
                                "move-cursor", 2,
				G_TYPE_ENUM, GTK_MOVEMENT_VISUAL_POSITIONS,
				G_TYPE_INT, 1);

  gtk_binding_entry_add_signal (binding_set, GDK_Left, GDK_CONTROL_MASK,
                                "move-cursor", 2,
				G_TYPE_ENUM, GTK_MOVEMENT_VISUAL_POSITIONS,
				G_TYPE_INT, -1);

  gtk_binding_entry_add_signal (binding_set, GDK_KP_Right, GDK_CONTROL_MASK,
                                "move-cursor", 2,
				G_TYPE_ENUM, GTK_MOVEMENT_VISUAL_POSITIONS,
				G_TYPE_INT, 1);

  gtk_binding_entry_add_signal (binding_set, GDK_KP_Left, GDK_CONTROL_MASK,
                                "move-cursor", 2,
				G_TYPE_ENUM, GTK_MOVEMENT_VISUAL_POSITIONS,
				G_TYPE_INT, -1);

  gtk_binding_entry_add_signal (binding_set, GDK_space, GDK_CONTROL_MASK, "toggle-cursor-row", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Space, GDK_CONTROL_MASK, "toggle-cursor-row", 0);

  gtk_binding_entry_add_signal (binding_set, GDK_a, GDK_CONTROL_MASK, "select-all", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_slash, GDK_CONTROL_MASK, "select-all", 0);

  gtk_binding_entry_add_signal (binding_set, GDK_A, GDK_CONTROL_MASK | GDK_SHIFT_MASK, "unselect-all", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_backslash, GDK_CONTROL_MASK, "unselect-all", 0);

  gtk_binding_entry_add_signal (binding_set, GDK_space, GDK_SHIFT_MASK, "select-cursor-row", 1,
				G_TYPE_BOOLEAN, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Space, GDK_SHIFT_MASK, "select-cursor-row", 1,
				G_TYPE_BOOLEAN, TRUE);

  gtk_binding_entry_add_signal (binding_set, GDK_space, 0, "select-cursor-row", 1,
				G_TYPE_BOOLEAN, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Space, 0, "select-cursor-row", 1,
				G_TYPE_BOOLEAN, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_Return, 0, "select-cursor-row", 1,
				G_TYPE_BOOLEAN, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_ISO_Enter, 0, "select-cursor-row", 1,
				G_TYPE_BOOLEAN, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Enter, 0, "select-cursor-row", 1,
				G_TYPE_BOOLEAN, TRUE);

  /* expand and collapse rows */
  gtk_binding_entry_add_signal (binding_set, GDK_plus, 0, "expand-collapse-cursor-row", 3,
				G_TYPE_BOOLEAN, TRUE,
				G_TYPE_BOOLEAN, TRUE,
				G_TYPE_BOOLEAN, FALSE);

  gtk_binding_entry_add_signal (binding_set, GDK_asterisk, 0,
                                "expand-collapse-cursor-row", 3,
                                G_TYPE_BOOLEAN, TRUE,
                                G_TYPE_BOOLEAN, TRUE,
                                G_TYPE_BOOLEAN, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Multiply, 0,
                                "expand-collapse-cursor-row", 3,
                                G_TYPE_BOOLEAN, TRUE,
                                G_TYPE_BOOLEAN, TRUE,
                                G_TYPE_BOOLEAN, TRUE);

  gtk_binding_entry_add_signal (binding_set, GDK_slash, 0,
                                "expand-collapse-cursor-row", 3,
                                G_TYPE_BOOLEAN, TRUE,
                                G_TYPE_BOOLEAN, FALSE,
                                G_TYPE_BOOLEAN, FALSE);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Divide, 0,
                                "expand-collapse-cursor-row", 3,
                                G_TYPE_BOOLEAN, TRUE,
                                G_TYPE_BOOLEAN, FALSE,
                                G_TYPE_BOOLEAN, FALSE);

  /* Not doable on US keyboards */
  gtk_binding_entry_add_signal (binding_set, GDK_plus, GDK_SHIFT_MASK, "expand-collapse-cursor-row", 3,
				G_TYPE_BOOLEAN, TRUE,
				G_TYPE_BOOLEAN, TRUE,
				G_TYPE_BOOLEAN, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Add, 0, "expand-collapse-cursor-row", 3,
				G_TYPE_BOOLEAN, TRUE,
				G_TYPE_BOOLEAN, TRUE,
				G_TYPE_BOOLEAN, FALSE);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Add, GDK_SHIFT_MASK, "expand-collapse-cursor-row", 3,
				G_TYPE_BOOLEAN, TRUE,
				G_TYPE_BOOLEAN, TRUE,
				G_TYPE_BOOLEAN, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Add, GDK_SHIFT_MASK, "expand-collapse-cursor-row", 3,
				G_TYPE_BOOLEAN, TRUE,
				G_TYPE_BOOLEAN, TRUE,
				G_TYPE_BOOLEAN, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_Right, GDK_SHIFT_MASK,
                                "expand-collapse-cursor-row", 3,
				G_TYPE_BOOLEAN, FALSE,
				G_TYPE_BOOLEAN, TRUE,
				G_TYPE_BOOLEAN, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Right, GDK_SHIFT_MASK,
                                "expand-collapse-cursor-row", 3,
				G_TYPE_BOOLEAN, FALSE,
				G_TYPE_BOOLEAN, TRUE,
				G_TYPE_BOOLEAN, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_Right,
                                GDK_CONTROL_MASK | GDK_SHIFT_MASK,
                                "expand-collapse-cursor-row", 3,
				G_TYPE_BOOLEAN, FALSE,
				G_TYPE_BOOLEAN, TRUE,
				G_TYPE_BOOLEAN, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Right,
                                GDK_CONTROL_MASK | GDK_SHIFT_MASK,
                                "expand-collapse-cursor-row", 3,
				G_TYPE_BOOLEAN, FALSE,
				G_TYPE_BOOLEAN, TRUE,
				G_TYPE_BOOLEAN, TRUE);

  gtk_binding_entry_add_signal (binding_set, GDK_minus, 0, "expand-collapse-cursor-row", 3,
				G_TYPE_BOOLEAN, TRUE,
				G_TYPE_BOOLEAN, FALSE,
				G_TYPE_BOOLEAN, FALSE);
  gtk_binding_entry_add_signal (binding_set, GDK_minus, GDK_SHIFT_MASK, "expand-collapse-cursor-row", 3,
				G_TYPE_BOOLEAN, TRUE,
				G_TYPE_BOOLEAN, FALSE,
				G_TYPE_BOOLEAN, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Subtract, 0, "expand-collapse-cursor-row", 3,
				G_TYPE_BOOLEAN, TRUE,
				G_TYPE_BOOLEAN, FALSE,
				G_TYPE_BOOLEAN, FALSE);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Subtract, GDK_SHIFT_MASK, "expand-collapse-cursor-row", 3,
				G_TYPE_BOOLEAN, TRUE,
				G_TYPE_BOOLEAN, FALSE,
				G_TYPE_BOOLEAN, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_Left, GDK_SHIFT_MASK,
                                "expand-collapse-cursor-row", 3,
				G_TYPE_BOOLEAN, FALSE,
				G_TYPE_BOOLEAN, FALSE,
				G_TYPE_BOOLEAN, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Left, GDK_SHIFT_MASK,
                                "expand-collapse-cursor-row", 3,
				G_TYPE_BOOLEAN, FALSE,
				G_TYPE_BOOLEAN, FALSE,
				G_TYPE_BOOLEAN, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_Left,
                                GDK_CONTROL_MASK | GDK_SHIFT_MASK,
                                "expand-collapse-cursor-row", 3,
				G_TYPE_BOOLEAN, FALSE,
				G_TYPE_BOOLEAN, FALSE,
				G_TYPE_BOOLEAN, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Left,
                                GDK_CONTROL_MASK | GDK_SHIFT_MASK,
                                "expand-collapse-cursor-row", 3,
				G_TYPE_BOOLEAN, FALSE,
				G_TYPE_BOOLEAN, FALSE,
				G_TYPE_BOOLEAN, TRUE);

  gtk_binding_entry_add_signal (binding_set, GDK_BackSpace, 0, "select-cursor-parent", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_BackSpace, GDK_CONTROL_MASK, "select-cursor-parent", 0);

  gtk_binding_entry_add_signal (binding_set, GDK_f, GDK_CONTROL_MASK, "start-interactive-search", 0);

  gtk_binding_entry_add_signal (binding_set, GDK_F, GDK_CONTROL_MASK, "start-interactive-search", 0);

  g_type_class_add_private (o_class, sizeof (GtkTreeViewPrivate));
}

static void
gtk_tree_view_init (GtkTreeView *tree_view)
{
  tree_view->priv = G_TYPE_INSTANCE_GET_PRIVATE (tree_view, GTK_TYPE_TREE_VIEW, GtkTreeViewPrivate);

  gtk_widget_set_can_focus (GTK_WIDGET (tree_view), TRUE);
  gtk_widget_set_redraw_on_allocate (GTK_WIDGET (tree_view), FALSE);

  tree_view->priv->flags =  GTK_TREE_VIEW_SHOW_EXPANDERS
                            | GTK_TREE_VIEW_DRAW_KEYFOCUS
                            | GTK_TREE_VIEW_HEADERS_VISIBLE;

  /* We need some padding */
  tree_view->priv->dy = 0;
  tree_view->priv->cursor_offset = 0;
  tree_view->priv->n_columns = 0;
  tree_view->priv->header_height = 1;
  tree_view->priv->x_drag = 0;
  tree_view->priv->drag_pos = -1;
  tree_view->priv->header_has_focus = FALSE;
  tree_view->priv->pressed_button = -1;
  tree_view->priv->press_start_x = -1;
  tree_view->priv->press_start_y = -1;
  tree_view->priv->reorderable = FALSE;
  tree_view->priv->presize_handler_timer = 0;
  tree_view->priv->scroll_sync_timer = 0;
  tree_view->priv->fixed_height = -1;
  tree_view->priv->fixed_height_mode = FALSE;
  tree_view->priv->fixed_height_check = 0;
  gtk_tree_view_set_adjustments (tree_view, NULL, NULL);
  tree_view->priv->selection = _gtk_tree_selection_new_with_tree_view (tree_view);
  tree_view->priv->enable_search = TRUE;
  tree_view->priv->search_column = -1;
  tree_view->priv->search_position_func = gtk_tree_view_search_position_func;
  tree_view->priv->search_equal_func = gtk_tree_view_search_equal_func;
  tree_view->priv->search_custom_entry_set = FALSE;
  tree_view->priv->typeselect_flush_timeout = 0;
  tree_view->priv->init_hadjust_value = TRUE;    
  tree_view->priv->width = 0;
          
  tree_view->priv->hover_selection = FALSE;
  tree_view->priv->hover_expand = FALSE;

  tree_view->priv->level_indentation = 0;

  tree_view->priv->rubber_banding_enable = FALSE;

  tree_view->priv->grid_lines = GTK_TREE_VIEW_GRID_LINES_NONE;
  tree_view->priv->tree_lines_enabled = FALSE;

  tree_view->priv->tooltip_column = -1;

  tree_view->priv->post_validation_flag = FALSE;

  tree_view->priv->last_button_x = -1;
  tree_view->priv->last_button_y = -1;

  tree_view->priv->event_last_x = -10000;
  tree_view->priv->event_last_y = -10000;
}



/* GObject Methods
 */

static void
gtk_tree_view_set_property (GObject         *object,
			    guint            prop_id,
			    const GValue    *value,
			    GParamSpec      *pspec)
{
  GtkTreeView *tree_view;

  tree_view = GTK_TREE_VIEW (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      gtk_tree_view_set_model (tree_view, g_value_get_object (value));
      break;
    case PROP_HADJUSTMENT:
      gtk_tree_view_set_hadjustment (tree_view, g_value_get_object (value));
      break;
    case PROP_VADJUSTMENT:
      gtk_tree_view_set_vadjustment (tree_view, g_value_get_object (value));
      break;
    case PROP_HEADERS_VISIBLE:
      gtk_tree_view_set_headers_visible (tree_view, g_value_get_boolean (value));
      break;
    case PROP_HEADERS_CLICKABLE:
      gtk_tree_view_set_headers_clickable (tree_view, g_value_get_boolean (value));
      break;
    case PROP_EXPANDER_COLUMN:
      gtk_tree_view_set_expander_column (tree_view, g_value_get_object (value));
      break;
    case PROP_REORDERABLE:
      gtk_tree_view_set_reorderable (tree_view, g_value_get_boolean (value));
      break;
    case PROP_RULES_HINT:
      gtk_tree_view_set_rules_hint (tree_view, g_value_get_boolean (value));
      break;
    case PROP_ENABLE_SEARCH:
      gtk_tree_view_set_enable_search (tree_view, g_value_get_boolean (value));
      break;
    case PROP_SEARCH_COLUMN:
      gtk_tree_view_set_search_column (tree_view, g_value_get_int (value));
      break;
    case PROP_FIXED_HEIGHT_MODE:
      gtk_tree_view_set_fixed_height_mode (tree_view, g_value_get_boolean (value));
      break;
    case PROP_HOVER_SELECTION:
      tree_view->priv->hover_selection = g_value_get_boolean (value);
      break;
    case PROP_HOVER_EXPAND:
      tree_view->priv->hover_expand = g_value_get_boolean (value);
      break;
    case PROP_SHOW_EXPANDERS:
      gtk_tree_view_set_show_expanders (tree_view, g_value_get_boolean (value));
      break;
    case PROP_LEVEL_INDENTATION:
      tree_view->priv->level_indentation = g_value_get_int (value);
      break;
    case PROP_RUBBER_BANDING:
      tree_view->priv->rubber_banding_enable = g_value_get_boolean (value);
      break;
    case PROP_ENABLE_GRID_LINES:
      gtk_tree_view_set_grid_lines (tree_view, g_value_get_enum (value));
      break;
    case PROP_ENABLE_TREE_LINES:
      gtk_tree_view_set_enable_tree_lines (tree_view, g_value_get_boolean (value));
      break;
    case PROP_TOOLTIP_COLUMN:
      gtk_tree_view_set_tooltip_column (tree_view, g_value_get_int (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_tree_view_get_property (GObject    *object,
			    guint       prop_id,
			    GValue     *value,
			    GParamSpec *pspec)
{
  GtkTreeView *tree_view;

  tree_view = GTK_TREE_VIEW (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      g_value_set_object (value, tree_view->priv->model);
      break;
    case PROP_HADJUSTMENT:
      g_value_set_object (value, tree_view->priv->hadjustment);
      break;
    case PROP_VADJUSTMENT:
      g_value_set_object (value, tree_view->priv->vadjustment);
      break;
    case PROP_HEADERS_VISIBLE:
      g_value_set_boolean (value, gtk_tree_view_get_headers_visible (tree_view));
      break;
    case PROP_HEADERS_CLICKABLE:
      g_value_set_boolean (value, gtk_tree_view_get_headers_clickable (tree_view));
      break;
    case PROP_EXPANDER_COLUMN:
      g_value_set_object (value, tree_view->priv->expander_column);
      break;
    case PROP_REORDERABLE:
      g_value_set_boolean (value, tree_view->priv->reorderable);
      break;
    case PROP_RULES_HINT:
      g_value_set_boolean (value, tree_view->priv->has_rules);
      break;
    case PROP_ENABLE_SEARCH:
      g_value_set_boolean (value, tree_view->priv->enable_search);
      break;
    case PROP_SEARCH_COLUMN:
      g_value_set_int (value, tree_view->priv->search_column);
      break;
    case PROP_FIXED_HEIGHT_MODE:
      g_value_set_boolean (value, tree_view->priv->fixed_height_mode);
      break;
    case PROP_HOVER_SELECTION:
      g_value_set_boolean (value, tree_view->priv->hover_selection);
      break;
    case PROP_HOVER_EXPAND:
      g_value_set_boolean (value, tree_view->priv->hover_expand);
      break;
    case PROP_SHOW_EXPANDERS:
      g_value_set_boolean (value, GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_SHOW_EXPANDERS));
      break;
    case PROP_LEVEL_INDENTATION:
      g_value_set_int (value, tree_view->priv->level_indentation);
      break;
    case PROP_RUBBER_BANDING:
      g_value_set_boolean (value, tree_view->priv->rubber_banding_enable);
      break;
    case PROP_ENABLE_GRID_LINES:
      g_value_set_enum (value, tree_view->priv->grid_lines);
      break;
    case PROP_ENABLE_TREE_LINES:
      g_value_set_boolean (value, tree_view->priv->tree_lines_enabled);
      break;
    case PROP_TOOLTIP_COLUMN:
      g_value_set_int (value, tree_view->priv->tooltip_column);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_tree_view_finalize (GObject *object)
{
  G_OBJECT_CLASS (gtk_tree_view_parent_class)->finalize (object);
}


static GtkBuildableIface *parent_buildable_iface;

static void
gtk_tree_view_buildable_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);
  iface->add_child = gtk_tree_view_buildable_add_child;
  iface->get_internal_child = gtk_tree_view_buildable_get_internal_child;
}

static void
gtk_tree_view_buildable_add_child (GtkBuildable *tree_view,
				   GtkBuilder  *builder,
				   GObject     *child,
				   const gchar *type)
{
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), GTK_TREE_VIEW_COLUMN (child));
}

static GObject *
gtk_tree_view_buildable_get_internal_child (GtkBuildable      *buildable,
					    GtkBuilder        *builder,
					    const gchar       *childname)
{
    if (strcmp (childname, "selection") == 0)
      return G_OBJECT (GTK_TREE_VIEW (buildable)->priv->selection);
    
    return parent_buildable_iface->get_internal_child (buildable,
						       builder,
						       childname);
}

/* GtkObject Methods
 */

static void
gtk_tree_view_free_rbtree (GtkTreeView *tree_view)
{
  _gtk_rbtree_free (tree_view->priv->tree);
  
  tree_view->priv->tree = NULL;
  tree_view->priv->button_pressed_node = NULL;
  tree_view->priv->button_pressed_tree = NULL;
  tree_view->priv->prelight_tree = NULL;
  tree_view->priv->prelight_node = NULL;
  tree_view->priv->expanded_collapsed_node = NULL;
  tree_view->priv->expanded_collapsed_tree = NULL;
}

static void
gtk_tree_view_destroy (GtkObject *object)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (object);
  GList *list;

  gtk_tree_view_stop_editing (tree_view, TRUE);

  if (tree_view->priv->columns != NULL)
    {
      list = tree_view->priv->columns;
      while (list)
	{
	  GtkTreeViewColumn *column;
	  column = GTK_TREE_VIEW_COLUMN (list->data);
	  list = list->next;
	  gtk_tree_view_remove_column (tree_view, column);
	}
      tree_view->priv->columns = NULL;
    }

  if (tree_view->priv->tree != NULL)
    {
      gtk_tree_view_unref_and_check_selection_tree (tree_view, tree_view->priv->tree);

      gtk_tree_view_free_rbtree (tree_view);
    }

  if (tree_view->priv->selection != NULL)
    {
      _gtk_tree_selection_set_tree_view (tree_view->priv->selection, NULL);
      g_object_unref (tree_view->priv->selection);
      tree_view->priv->selection = NULL;
    }

  if (tree_view->priv->scroll_to_path != NULL)
    {
      gtk_tree_row_reference_free (tree_view->priv->scroll_to_path);
      tree_view->priv->scroll_to_path = NULL;
    }

  if (tree_view->priv->drag_dest_row != NULL)
    {
      gtk_tree_row_reference_free (tree_view->priv->drag_dest_row);
      tree_view->priv->drag_dest_row = NULL;
    }

  if (tree_view->priv->top_row != NULL)
    {
      gtk_tree_row_reference_free (tree_view->priv->top_row);
      tree_view->priv->top_row = NULL;
    }

  if (tree_view->priv->column_drop_func_data &&
      tree_view->priv->column_drop_func_data_destroy)
    {
      tree_view->priv->column_drop_func_data_destroy (tree_view->priv->column_drop_func_data);
      tree_view->priv->column_drop_func_data = NULL;
    }

  if (tree_view->priv->destroy_count_destroy &&
      tree_view->priv->destroy_count_data)
    {
      tree_view->priv->destroy_count_destroy (tree_view->priv->destroy_count_data);
      tree_view->priv->destroy_count_data = NULL;
    }

  gtk_tree_row_reference_free (tree_view->priv->cursor);
  tree_view->priv->cursor = NULL;

  gtk_tree_row_reference_free (tree_view->priv->anchor);
  tree_view->priv->anchor = NULL;

  /* destroy interactive search dialog */
  if (tree_view->priv->search_window)
    {
      gtk_widget_destroy (tree_view->priv->search_window);
      tree_view->priv->search_window = NULL;
      tree_view->priv->search_entry = NULL;
      if (tree_view->priv->typeselect_flush_timeout)
	{
	  g_source_remove (tree_view->priv->typeselect_flush_timeout);
	  tree_view->priv->typeselect_flush_timeout = 0;
	}
    }

  if (tree_view->priv->search_destroy && tree_view->priv->search_user_data)
    {
      tree_view->priv->search_destroy (tree_view->priv->search_user_data);
      tree_view->priv->search_user_data = NULL;
    }

  if (tree_view->priv->search_position_destroy && tree_view->priv->search_position_user_data)
    {
      tree_view->priv->search_position_destroy (tree_view->priv->search_position_user_data);
      tree_view->priv->search_position_user_data = NULL;
    }

  if (tree_view->priv->row_separator_destroy && tree_view->priv->row_separator_data)
    {
      tree_view->priv->row_separator_destroy (tree_view->priv->row_separator_data);
      tree_view->priv->row_separator_data = NULL;
    }
  
  gtk_tree_view_set_model (tree_view, NULL);

  if (tree_view->priv->hadjustment)
    {
      g_object_unref (tree_view->priv->hadjustment);
      tree_view->priv->hadjustment = NULL;
    }
  if (tree_view->priv->vadjustment)
    {
      g_object_unref (tree_view->priv->vadjustment);
      tree_view->priv->vadjustment = NULL;
    }

  GTK_OBJECT_CLASS (gtk_tree_view_parent_class)->destroy (object);
}



/* GtkWidget Methods
 */

/* GtkWidget::map helper */
static void
gtk_tree_view_map_buttons (GtkTreeView *tree_view)
{
  GList *list;

  g_return_if_fail (gtk_widget_get_mapped (GTK_WIDGET (tree_view)));

  if (GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_HEADERS_VISIBLE))
    {
      GtkTreeViewColumn *column;

      for (list = tree_view->priv->columns; list; list = list->next)
	{
	  column = list->data;
          if (gtk_widget_get_visible (column->button) &&
              !gtk_widget_get_mapped (column->button))
            gtk_widget_map (column->button);
	}
      for (list = tree_view->priv->columns; list; list = list->next)
	{
	  column = list->data;
	  if (column->visible == FALSE)
	    continue;
	  if (column->resizable)
	    {
	      gdk_window_raise (column->window);
	      gdk_window_show (column->window);
	    }
	  else
	    gdk_window_hide (column->window);
	}
      gdk_window_show (tree_view->priv->header_window);
    }
}

static void
gtk_tree_view_map (GtkWidget *widget)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (widget);
  GList *tmp_list;

  gtk_widget_set_mapped (widget, TRUE);

  tmp_list = tree_view->priv->children;
  while (tmp_list)
    {
      GtkTreeViewChild *child = tmp_list->data;
      tmp_list = tmp_list->next;

      if (gtk_widget_get_visible (child->widget))
	{
	  if (!gtk_widget_get_mapped (child->widget))
	    gtk_widget_map (child->widget);
	}
    }
  gdk_window_show (tree_view->priv->bin_window);

  gtk_tree_view_map_buttons (tree_view);

  gdk_window_show (widget->window);
}

static void
gtk_tree_view_realize (GtkWidget *widget)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (widget);
  GList *tmp_list;
  GdkWindowAttr attributes;
  gint attributes_mask;

  gtk_widget_set_realized (widget, TRUE);

  /* Make the main, clipping window */
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = GDK_VISIBILITY_NOTIFY_MASK;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget),
				   &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, widget);

  /* Make the window for the tree */
  attributes.x = 0;
  attributes.y = TREE_VIEW_HEADER_HEIGHT (tree_view);
  attributes.width = MAX (tree_view->priv->width, widget->allocation.width);
  attributes.height = widget->allocation.height;
  attributes.event_mask = (GDK_EXPOSURE_MASK |
                           GDK_SCROLL_MASK |
                           GDK_POINTER_MOTION_MASK |
                           GDK_ENTER_NOTIFY_MASK |
                           GDK_LEAVE_NOTIFY_MASK |
                           GDK_BUTTON_PRESS_MASK |
                           GDK_BUTTON_RELEASE_MASK |
                           gtk_widget_get_events (widget));

  tree_view->priv->bin_window = gdk_window_new (widget->window,
						&attributes, attributes_mask);
  gdk_window_set_user_data (tree_view->priv->bin_window, widget);

  /* Make the column header window */
  attributes.x = 0;
  attributes.y = 0;
  attributes.width = MAX (tree_view->priv->width, widget->allocation.width);
  attributes.height = tree_view->priv->header_height;
  attributes.event_mask = (GDK_EXPOSURE_MASK |
                           GDK_SCROLL_MASK |
                           GDK_ENTER_NOTIFY_MASK |
                           GDK_LEAVE_NOTIFY_MASK |
                           GDK_BUTTON_PRESS_MASK |
                           GDK_BUTTON_RELEASE_MASK |
                           GDK_KEY_PRESS_MASK |
                           GDK_KEY_RELEASE_MASK |
                           gtk_widget_get_events (widget));

  tree_view->priv->header_window = gdk_window_new (widget->window,
						   &attributes, attributes_mask);
  gdk_window_set_user_data (tree_view->priv->header_window, widget);

  /* Add them all up. */
  widget->style = gtk_style_attach (widget->style, widget->window);
  gdk_window_set_back_pixmap (widget->window, NULL, FALSE);
  gdk_window_set_background (tree_view->priv->bin_window, &widget->style->base[widget->state]);
  gtk_style_set_background (widget->style, tree_view->priv->header_window, GTK_STATE_NORMAL);

  tmp_list = tree_view->priv->children;
  while (tmp_list)
    {
      GtkTreeViewChild *child = tmp_list->data;
      tmp_list = tmp_list->next;

      gtk_widget_set_parent_window (child->widget, tree_view->priv->bin_window);
    }

  for (tmp_list = tree_view->priv->columns; tmp_list; tmp_list = tmp_list->next)
    _gtk_tree_view_column_realize_button (GTK_TREE_VIEW_COLUMN (tmp_list->data));

  /* Need to call those here, since they create GCs */
  gtk_tree_view_set_grid_lines (tree_view, tree_view->priv->grid_lines);
  gtk_tree_view_set_enable_tree_lines (tree_view, tree_view->priv->tree_lines_enabled);

  install_presize_handler (tree_view); 
}

static void
gtk_tree_view_unrealize (GtkWidget *widget)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (widget);
  GtkTreeViewPrivate *priv = tree_view->priv;
  GList *list;

  if (priv->scroll_timeout != 0)
    {
      g_source_remove (priv->scroll_timeout);
      priv->scroll_timeout = 0;
    }

  if (priv->auto_expand_timeout != 0)
    {
      g_source_remove (priv->auto_expand_timeout);
      priv->auto_expand_timeout = 0;
    }

  if (priv->open_dest_timeout != 0)
    {
      g_source_remove (priv->open_dest_timeout);
      priv->open_dest_timeout = 0;
    }

  remove_expand_collapse_timeout (tree_view);
  
  if (priv->presize_handler_timer != 0)
    {
      g_source_remove (priv->presize_handler_timer);
      priv->presize_handler_timer = 0;
    }

  if (priv->validate_rows_timer != 0)
    {
      g_source_remove (priv->validate_rows_timer);
      priv->validate_rows_timer = 0;
    }

  if (priv->scroll_sync_timer != 0)
    {
      g_source_remove (priv->scroll_sync_timer);
      priv->scroll_sync_timer = 0;
    }

  if (priv->typeselect_flush_timeout)
    {
      g_source_remove (priv->typeselect_flush_timeout);
      priv->typeselect_flush_timeout = 0;
    }
  
  for (list = priv->columns; list; list = list->next)
    _gtk_tree_view_column_unrealize_button (GTK_TREE_VIEW_COLUMN (list->data));

  gdk_window_set_user_data (priv->bin_window, NULL);
  gdk_window_destroy (priv->bin_window);
  priv->bin_window = NULL;

  gdk_window_set_user_data (priv->header_window, NULL);
  gdk_window_destroy (priv->header_window);
  priv->header_window = NULL;

  if (priv->drag_window)
    {
      gdk_window_set_user_data (priv->drag_window, NULL);
      gdk_window_destroy (priv->drag_window);
      priv->drag_window = NULL;
    }

  if (priv->drag_highlight_window)
    {
      gdk_window_set_user_data (priv->drag_highlight_window, NULL);
      gdk_window_destroy (priv->drag_highlight_window);
      priv->drag_highlight_window = NULL;
    }

  GTK_WIDGET_CLASS (gtk_tree_view_parent_class)->unrealize (widget);
}

/* GtkWidget::size_request helper */
static void
gtk_tree_view_size_request_columns (GtkTreeView *tree_view)
{
  GList *list;

  tree_view->priv->header_height = 0;

  if (tree_view->priv->model)
    {
      for (list = tree_view->priv->columns; list; list = list->next)
        {
          GtkRequisition requisition;
          GtkTreeViewColumn *column = list->data;

	  if (column->button == NULL)
	    continue;

          column = list->data;
	  
          gtk_widget_size_request (column->button, &requisition);
	  column->button_request = requisition.width;
          tree_view->priv->header_height = MAX (tree_view->priv->header_height, requisition.height);
        }
    }
}


/* Called only by ::size_request */
static void
gtk_tree_view_update_size (GtkTreeView *tree_view)
{
  GList *list;
  GtkTreeViewColumn *column;
  gint i;

  if (tree_view->priv->model == NULL)
    {
      tree_view->priv->width = 0;
      tree_view->priv->prev_width = 0;                   
      tree_view->priv->height = 0;
      return;
    }

  tree_view->priv->prev_width = tree_view->priv->width;  
  tree_view->priv->width = 0;

  /* keep this in sync with size_allocate below */
  for (list = tree_view->priv->columns, i = 0; list; list = list->next, i++)
    {
      gint real_requested_width = 0;
      column = list->data;
      if (!column->visible)
	continue;

      if (column->use_resized_width)
	{
	  real_requested_width = column->resized_width;
	}
      else if (column->column_type == GTK_TREE_VIEW_COLUMN_FIXED)
	{
	  real_requested_width = column->fixed_width;
	}
      else if (GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_HEADERS_VISIBLE))
	{
	  real_requested_width = MAX (column->requested_width, column->button_request);
	}
      else
	{
	  real_requested_width = column->requested_width;
	}

      if (column->min_width != -1)
	real_requested_width = MAX (real_requested_width, column->min_width);
      if (column->max_width != -1)
	real_requested_width = MIN (real_requested_width, column->max_width);

      tree_view->priv->width += real_requested_width;
    }

  if (tree_view->priv->tree == NULL)
    tree_view->priv->height = 0;
  else
    tree_view->priv->height = tree_view->priv->tree->root->offset;
}

static void
gtk_tree_view_size_request (GtkWidget      *widget,
			    GtkRequisition *requisition)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (widget);
  GList *tmp_list;

  /* we validate some rows initially just to make sure we have some size. 
   * In practice, with a lot of static lists, this should get a good width.
   */
  do_validate_rows (tree_view, FALSE);
  gtk_tree_view_size_request_columns (tree_view);
  gtk_tree_view_update_size (GTK_TREE_VIEW (widget));

  requisition->width = tree_view->priv->width;
  requisition->height = tree_view->priv->height + TREE_VIEW_HEADER_HEIGHT (tree_view);

  tmp_list = tree_view->priv->children;

  while (tmp_list)
    {
      GtkTreeViewChild *child = tmp_list->data;
      GtkRequisition child_requisition;

      tmp_list = tmp_list->next;

      if (gtk_widget_get_visible (child->widget))
        gtk_widget_size_request (child->widget, &child_requisition);
    }
}

static int
gtk_tree_view_calculate_width_before_expander (GtkTreeView *tree_view)
{
  int width = 0;
  GList *list;
  gboolean rtl;

  rtl = (gtk_widget_get_direction (GTK_WIDGET (tree_view)) == GTK_TEXT_DIR_RTL);
  for (list = (rtl ? g_list_last (tree_view->priv->columns) : g_list_first (tree_view->priv->columns));
       list->data != tree_view->priv->expander_column;
       list = (rtl ? list->prev : list->next))
    {
      GtkTreeViewColumn *column = list->data;

      width += column->width;
    }

  return width;
}

static void
invalidate_column (GtkTreeView       *tree_view,
                   GtkTreeViewColumn *column)
{
  gint column_offset = 0;
  GList *list;
  GtkWidget *widget = GTK_WIDGET (tree_view);
  gboolean rtl;

  if (!gtk_widget_get_realized (widget))
    return;

  rtl = (gtk_widget_get_direction (GTK_WIDGET (tree_view)) == GTK_TEXT_DIR_RTL);
  for (list = (rtl ? g_list_last (tree_view->priv->columns) : g_list_first (tree_view->priv->columns));
       list;
       list = (rtl ? list->prev : list->next))
    {
      GtkTreeViewColumn *tmpcolumn = list->data;
      if (tmpcolumn == column)
	{
	  GdkRectangle invalid_rect;
	  
	  invalid_rect.x = column_offset;
	  invalid_rect.y = 0;
	  invalid_rect.width = column->width;
	  invalid_rect.height = widget->allocation.height;
	  
	  gdk_window_invalidate_rect (widget->window, &invalid_rect, TRUE);
	  break;
	}
      
      column_offset += tmpcolumn->width;
    }
}

static void
invalidate_last_column (GtkTreeView *tree_view)
{
  GList *last_column;
  gboolean rtl;

  rtl = (gtk_widget_get_direction (GTK_WIDGET (tree_view)) == GTK_TEXT_DIR_RTL);

  for (last_column = (rtl ? g_list_first (tree_view->priv->columns) : g_list_last (tree_view->priv->columns));
       last_column;
       last_column = (rtl ? last_column->next : last_column->prev))
    {
      if (GTK_TREE_VIEW_COLUMN (last_column->data)->visible)
        {
          invalidate_column (tree_view, last_column->data);
          return;
        }
    }
}

static gint
gtk_tree_view_get_real_requested_width_from_column (GtkTreeView       *tree_view,
                                                    GtkTreeViewColumn *column)
{
  gint real_requested_width;

  if (column->use_resized_width)
    {
      real_requested_width = column->resized_width;
    }
  else if (column->column_type == GTK_TREE_VIEW_COLUMN_FIXED)
    {
      real_requested_width = column->fixed_width;
    }
  else if (GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_HEADERS_VISIBLE))
    {
      real_requested_width = MAX (column->requested_width, column->button_request);
    }
  else
    {
      real_requested_width = column->requested_width;
      if (real_requested_width < 0)
        real_requested_width = 0;
    }

  if (column->min_width != -1)
    real_requested_width = MAX (real_requested_width, column->min_width);
  if (column->max_width != -1)
    real_requested_width = MIN (real_requested_width, column->max_width);

  return real_requested_width;
}

/* GtkWidget::size_allocate helper */
static void
gtk_tree_view_size_allocate_columns (GtkWidget *widget,
				     gboolean  *width_changed)
{
  GtkTreeView *tree_view;
  GList *list, *first_column, *last_column;
  GtkTreeViewColumn *column;
  GtkAllocation allocation;
  gint width = 0;
  gint extra, extra_per_column, extra_for_last;
  gint full_requested_width = 0;
  gint number_of_expand_columns = 0;
  gboolean column_changed = FALSE;
  gboolean rtl;
  gboolean update_expand;
  
  tree_view = GTK_TREE_VIEW (widget);

  for (last_column = g_list_last (tree_view->priv->columns);
       last_column && !(GTK_TREE_VIEW_COLUMN (last_column->data)->visible);
       last_column = last_column->prev)
    ;
  if (last_column == NULL)
    return;

  for (first_column = g_list_first (tree_view->priv->columns);
       first_column && !(GTK_TREE_VIEW_COLUMN (first_column->data)->visible);
       first_column = first_column->next)
    ;

  allocation.y = 0;
  allocation.height = tree_view->priv->header_height;

  rtl = (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL);

  /* find out how many extra space and expandable columns we have */
  for (list = tree_view->priv->columns; list != last_column->next; list = list->next)
    {
      column = (GtkTreeViewColumn *)list->data;

      if (!column->visible)
	continue;

      full_requested_width += gtk_tree_view_get_real_requested_width_from_column (tree_view, column);

      if (column->expand)
	number_of_expand_columns++;
    }

  /* Only update the expand value if the width of the widget has changed,
   * or the number of expand columns has changed, or if there are no expand
   * columns, or if we didn't have an size-allocation yet after the
   * last validated node.
   */
  update_expand = (width_changed && *width_changed == TRUE)
      || number_of_expand_columns != tree_view->priv->last_number_of_expand_columns
      || number_of_expand_columns == 0
      || tree_view->priv->post_validation_flag == TRUE;

  tree_view->priv->post_validation_flag = FALSE;

  if (!update_expand)
    {
      extra = tree_view->priv->last_extra_space;
      extra_for_last = MAX (widget->allocation.width - full_requested_width - extra, 0);
    }
  else
    {
      extra = MAX (widget->allocation.width - full_requested_width, 0);
      extra_for_last = 0;

      tree_view->priv->last_extra_space = extra;
    }

  if (number_of_expand_columns > 0)
    extra_per_column = extra/number_of_expand_columns;
  else
    extra_per_column = 0;

  if (update_expand)
    {
      tree_view->priv->last_extra_space_per_column = extra_per_column;
      tree_view->priv->last_number_of_expand_columns = number_of_expand_columns;
    }

  for (list = (rtl ? last_column : first_column); 
       list != (rtl ? first_column->prev : last_column->next);
       list = (rtl ? list->prev : list->next)) 
    {
      gint real_requested_width = 0;
      gint old_width;

      column = list->data;
      old_width = column->width;

      if (!column->visible)
	continue;

      /* We need to handle the dragged button specially.
       */
      if (column == tree_view->priv->drag_column)
	{
	  GtkAllocation drag_allocation;
          drag_allocation.width = gdk_window_get_width (tree_view->priv->drag_window);
          drag_allocation.height = gdk_window_get_height (tree_view->priv->drag_window);
	  drag_allocation.x = 0;
	  drag_allocation.y = 0;
	  gtk_widget_size_allocate (tree_view->priv->drag_column->button,
				    &drag_allocation);
	  width += drag_allocation.width;
	  continue;
	}

      real_requested_width = gtk_tree_view_get_real_requested_width_from_column (tree_view, column);

      allocation.x = width;
      column->width = real_requested_width;

      if (column->expand)
	{
	  if (number_of_expand_columns == 1)
	    {
	      /* We add the remander to the last column as
	       * */
	      column->width += extra;
	    }
	  else
	    {
	      column->width += extra_per_column;
	      extra -= extra_per_column;
	      number_of_expand_columns --;
	    }
	}
      else if (number_of_expand_columns == 0 &&
	       list == last_column)
	{
	  column->width += extra;
	}

      /* In addition to expand, the last column can get even more
       * extra space so all available space is filled up.
       */
      if (extra_for_last > 0 && list == last_column)
	column->width += extra_for_last;

      g_object_notify (G_OBJECT (column), "width");

      allocation.width = column->width;
      width += column->width;

      if (column->width > old_width)
        column_changed = TRUE;

      gtk_widget_size_allocate (column->button, &allocation);

      if (column->window)
	gdk_window_move_resize (column->window,
                                allocation.x + (rtl ? 0 : allocation.width) - TREE_VIEW_DRAG_WIDTH/2,
				allocation.y,
                                TREE_VIEW_DRAG_WIDTH, allocation.height);
    }

  /* We change the width here.  The user might have been resizing columns,
   * so the total width of the tree view changes.
   */
  tree_view->priv->width = width;
  if (width_changed)
    *width_changed = TRUE;

  if (column_changed)
    gtk_widget_queue_draw (GTK_WIDGET (tree_view));
}


static void
gtk_tree_view_size_allocate (GtkWidget     *widget,
			     GtkAllocation *allocation)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (widget);
  GList *tmp_list;
  gboolean width_changed = FALSE;
  gint old_width = widget->allocation.width;

  if (allocation->width != widget->allocation.width)
    width_changed = TRUE;

  widget->allocation = *allocation;

  tmp_list = tree_view->priv->children;

  while (tmp_list)
    {
      GtkAllocation allocation;

      GtkTreeViewChild *child = tmp_list->data;
      tmp_list = tmp_list->next;

      /* totally ignore our child's requisition */
      allocation.x = child->x;
      allocation.y = child->y;
      allocation.width = child->width;
      allocation.height = child->height;
      gtk_widget_size_allocate (child->widget, &allocation);
    }

  /* We size-allocate the columns first because the width of the
   * tree view (used in updating the adjustments below) might change.
   */
  gtk_tree_view_size_allocate_columns (widget, &width_changed);

  tree_view->priv->hadjustment->page_size = allocation->width;
  tree_view->priv->hadjustment->page_increment = allocation->width * 0.9;
  tree_view->priv->hadjustment->step_increment = allocation->width * 0.1;
  tree_view->priv->hadjustment->lower = 0;
  tree_view->priv->hadjustment->upper = MAX (tree_view->priv->hadjustment->page_size, tree_view->priv->width);

  if (gtk_widget_get_direction(widget) == GTK_TEXT_DIR_RTL)   
    {
      if (allocation->width < tree_view->priv->width)
        {
	  if (tree_view->priv->init_hadjust_value)
	    {
	      tree_view->priv->hadjustment->value = MAX (tree_view->priv->width - allocation->width, 0);
	      tree_view->priv->init_hadjust_value = FALSE;
	    }
	  else if (allocation->width != old_width)
	    {
	      tree_view->priv->hadjustment->value = CLAMP (tree_view->priv->hadjustment->value - allocation->width + old_width, 0, tree_view->priv->width - allocation->width);
	    }
	  else
	    tree_view->priv->hadjustment->value = CLAMP (tree_view->priv->width - (tree_view->priv->prev_width - tree_view->priv->hadjustment->value), 0, tree_view->priv->width - allocation->width);
	}
      else
        {
	  tree_view->priv->hadjustment->value = 0;
	  tree_view->priv->init_hadjust_value = TRUE;
	}
    }
  else
    if (tree_view->priv->hadjustment->value + allocation->width > tree_view->priv->width)
      tree_view->priv->hadjustment->value = MAX (tree_view->priv->width - allocation->width, 0);

  gtk_adjustment_changed (tree_view->priv->hadjustment);

  tree_view->priv->vadjustment->page_size = allocation->height - TREE_VIEW_HEADER_HEIGHT (tree_view);
  tree_view->priv->vadjustment->step_increment = tree_view->priv->vadjustment->page_size * 0.1;
  tree_view->priv->vadjustment->page_increment = tree_view->priv->vadjustment->page_size * 0.9;
  tree_view->priv->vadjustment->lower = 0;
  tree_view->priv->vadjustment->upper = MAX (tree_view->priv->vadjustment->page_size, tree_view->priv->height);

  gtk_adjustment_changed (tree_view->priv->vadjustment);

  /* now the adjustments and window sizes are in sync, we can sync toprow/dy again */
  if (tree_view->priv->height <= tree_view->priv->vadjustment->page_size)
    gtk_adjustment_set_value (GTK_ADJUSTMENT (tree_view->priv->vadjustment), 0);
  else if (tree_view->priv->vadjustment->value + tree_view->priv->vadjustment->page_size > tree_view->priv->height)
    gtk_adjustment_set_value (GTK_ADJUSTMENT (tree_view->priv->vadjustment),
                              tree_view->priv->height - tree_view->priv->vadjustment->page_size);
  else if (gtk_tree_row_reference_valid (tree_view->priv->top_row))
    gtk_tree_view_top_row_to_dy (tree_view);
  else
    gtk_tree_view_dy_to_top_row (tree_view);
  
  if (gtk_widget_get_realized (widget))
    {
      gdk_window_move_resize (widget->window,
			      allocation->x, allocation->y,
			      allocation->width, allocation->height);
      gdk_window_move_resize (tree_view->priv->header_window,
			      - (gint) tree_view->priv->hadjustment->value,
			      0,
			      MAX (tree_view->priv->width, allocation->width),
			      tree_view->priv->header_height);
      gdk_window_move_resize (tree_view->priv->bin_window,
			      - (gint) tree_view->priv->hadjustment->value,
			      TREE_VIEW_HEADER_HEIGHT (tree_view),
			      MAX (tree_view->priv->width, allocation->width),
			      allocation->height - TREE_VIEW_HEADER_HEIGHT (tree_view));
    }

  if (tree_view->priv->tree == NULL)
    invalidate_empty_focus (tree_view);

  if (gtk_widget_get_realized (widget))
    {
      gboolean has_expand_column = FALSE;
      for (tmp_list = tree_view->priv->columns; tmp_list; tmp_list = tmp_list->next)
	{
	  if (gtk_tree_view_column_get_expand (GTK_TREE_VIEW_COLUMN (tmp_list->data)))
	    {
	      has_expand_column = TRUE;
	      break;
	    }
	}

      if (width_changed && tree_view->priv->expander_column)
        {
          /* Might seem awkward, but is the best heuristic I could come up
           * with.  Only if the width of the columns before the expander
           * changes, we will update the prelight status.  It is this
           * width that makes the expander move vertically.  Always updating
           * prelight status causes trouble with hover selections.
           */
          gint width_before_expander;

          width_before_expander = gtk_tree_view_calculate_width_before_expander (tree_view);

          if (tree_view->priv->prev_width_before_expander
              != width_before_expander)
              update_prelight (tree_view,
                               tree_view->priv->event_last_x,
                               tree_view->priv->event_last_y);

          tree_view->priv->prev_width_before_expander = width_before_expander;
        }

      /* This little hack only works if we have an LTR locale, and no column has the  */
      if (width_changed)
	{
	  if (gtk_widget_get_direction (GTK_WIDGET (tree_view)) == GTK_TEXT_DIR_LTR &&
	      ! has_expand_column)
	    invalidate_last_column (tree_view);
	  else
	    gtk_widget_queue_draw (widget);
	}
    }
}

/* Grabs the focus and unsets the GTK_TREE_VIEW_DRAW_KEYFOCUS flag */
static void
grab_focus_and_unset_draw_keyfocus (GtkTreeView *tree_view)
{
  GtkWidget *widget = GTK_WIDGET (tree_view);

  if (gtk_widget_get_can_focus (widget) && !gtk_widget_has_focus (widget))
    gtk_widget_grab_focus (widget);
  GTK_TREE_VIEW_UNSET_FLAG (tree_view, GTK_TREE_VIEW_DRAW_KEYFOCUS);
}

static inline gboolean
row_is_separator (GtkTreeView *tree_view,
		  GtkTreeIter *iter,
		  GtkTreePath *path)
{
  gboolean is_separator = FALSE;

  if (tree_view->priv->row_separator_func)
    {
      GtkTreeIter tmpiter;

      if (iter)
        tmpiter = *iter;
      else
        {
          if (!gtk_tree_model_get_iter (tree_view->priv->model, &tmpiter, path))
            return FALSE;
        }

      is_separator = tree_view->priv->row_separator_func (tree_view->priv->model,
                                                          &tmpiter,
                                                          tree_view->priv->row_separator_data);
    }

  return is_separator;
}

static gboolean
gtk_tree_view_button_press (GtkWidget      *widget,
			    GdkEventButton *event)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (widget);
  GList *list;
  GtkTreeViewColumn *column = NULL;
  gint i;
  GdkRectangle background_area;
  GdkRectangle cell_area;
  gint vertical_separator;
  gint horizontal_separator;
  gboolean path_is_selectable;
  gboolean rtl;
  gboolean edits_allowed;

  rtl = (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL);
  gtk_tree_view_stop_editing (tree_view, FALSE);
  gtk_widget_style_get (widget,
			"vertical-separator", &vertical_separator,
			"horizontal-separator", &horizontal_separator,
			NULL);


  /* Because grab_focus can cause reentrancy, we delay grab_focus until after
   * we're done handling the button press.
   */

  if (event->window == tree_view->priv->bin_window)
    {
      GtkRBNode *node;
      GtkRBTree *tree;
      GtkTreePath *path;
      gchar *path_string;
      gint depth;
      gint new_y;
      gint y_offset;
      gint dval;
      gint pre_val, aft_val;
      GtkTreeViewColumn *column = NULL;
      GtkCellRenderer *focus_cell = NULL;
      gint column_handled_click = FALSE;
      gboolean row_double_click = FALSE;
      gboolean rtl;
      gboolean node_selected;

      /* Empty tree? */
      if (tree_view->priv->tree == NULL)
	{
	  grab_focus_and_unset_draw_keyfocus (tree_view);
	  return TRUE;
	}

      /* are we in an arrow? */
      if (tree_view->priv->prelight_node &&
          GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_ARROW_PRELIT) &&
	  TREE_VIEW_DRAW_EXPANDERS (tree_view))
	{
	  if (event->button == 1)
	    {
	      gtk_grab_add (widget);
	      tree_view->priv->button_pressed_node = tree_view->priv->prelight_node;
	      tree_view->priv->button_pressed_tree = tree_view->priv->prelight_tree;
	      gtk_tree_view_draw_arrow (GTK_TREE_VIEW (widget),
					tree_view->priv->prelight_tree,
					tree_view->priv->prelight_node,
					event->x,
					event->y);
	    }

	  grab_focus_and_unset_draw_keyfocus (tree_view);
	  return TRUE;
	}

      /* find the node that was clicked */
      new_y = TREE_WINDOW_Y_TO_RBTREE_Y(tree_view, event->y);
      if (new_y < 0)
	new_y = 0;
      y_offset = -_gtk_rbtree_find_offset (tree_view->priv->tree, new_y, &tree, &node);

      if (node == NULL)
	{
	  /* We clicked in dead space */
	  grab_focus_and_unset_draw_keyfocus (tree_view);
	  return TRUE;
	}

      /* Get the path and the node */
      path = _gtk_tree_view_find_path (tree_view, tree, node);
      path_is_selectable = !row_is_separator (tree_view, NULL, path);

      if (!path_is_selectable)
	{
	  gtk_tree_path_free (path);
	  grab_focus_and_unset_draw_keyfocus (tree_view);
	  return TRUE;
	}

      depth = gtk_tree_path_get_depth (path);
      background_area.y = y_offset + event->y;
      background_area.height = ROW_HEIGHT (tree_view, GTK_RBNODE_GET_HEIGHT (node));
      background_area.x = 0;


      /* Let the column have a chance at selecting it. */
      rtl = (gtk_widget_get_direction (GTK_WIDGET (tree_view)) == GTK_TEXT_DIR_RTL);
      for (list = (rtl ? g_list_last (tree_view->priv->columns) : g_list_first (tree_view->priv->columns));
	   list; list = (rtl ? list->prev : list->next))
	{
	  GtkTreeViewColumn *candidate = list->data;

	  if (!candidate->visible)
	    continue;

	  background_area.width = candidate->width;
	  if ((background_area.x > (gint) event->x) ||
	      (background_area.x + background_area.width <= (gint) event->x))
	    {
	      background_area.x += background_area.width;
	      continue;
	    }

	  /* we found the focus column */
	  column = candidate;
	  cell_area = background_area;
	  cell_area.width -= horizontal_separator;
	  cell_area.height -= vertical_separator;
	  cell_area.x += horizontal_separator/2;
	  cell_area.y += vertical_separator/2;
	  if (gtk_tree_view_is_expander_column (tree_view, column))
	    {
	      if (!rtl)
		cell_area.x += (depth - 1) * tree_view->priv->level_indentation;
	      cell_area.width -= (depth - 1) * tree_view->priv->level_indentation;

              if (TREE_VIEW_DRAW_EXPANDERS (tree_view))
	        {
		  if (!rtl)
		    cell_area.x += depth * tree_view->priv->expander_size;
	          cell_area.width -= depth * tree_view->priv->expander_size;
		}
	    }
	  break;
	}

      if (column == NULL)
	{
	  gtk_tree_path_free (path);
	  grab_focus_and_unset_draw_keyfocus (tree_view);
	  return FALSE;
	}

      tree_view->priv->focus_column = column;

      /* ARDOUR HACK */

      if (g_object_get_data (G_OBJECT(column), "mouse-edits-require-mod1")) {
             edits_allowed = (event->state & GDK_MOD1_MASK);
      } else {
             /* regular GTK design: do edits if none of the default modifiers are active */
             edits_allowed = !(event->state & gtk_accelerator_get_default_mod_mask ());
      }

      /* decide if we edit */
      if (event->type == GDK_BUTTON_PRESS && event->button == 1 && edits_allowed)
	{
	  GtkTreePath *anchor;
	  GtkTreeIter iter;

	  gtk_tree_model_get_iter (tree_view->priv->model, &iter, path);
	  gtk_tree_view_column_cell_set_cell_data (column,
						   tree_view->priv->model,
						   &iter,
						   GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_PARENT),
						   node->children?TRUE:FALSE);

	  if (tree_view->priv->anchor)
	    anchor = gtk_tree_row_reference_get_path (tree_view->priv->anchor);
	  else
	    anchor = NULL;

	  if ((anchor && !gtk_tree_path_compare (anchor, path))
	      || !_gtk_tree_view_column_has_editable_cell (column))
	    {
	      GtkCellEditable *cell_editable = NULL;

	      /* FIXME: get the right flags */
	      guint flags = 0;

	      path_string = gtk_tree_path_to_string (path);

	      if (_gtk_tree_view_column_cell_event (column,
						    &cell_editable,
						    (GdkEvent *)event,
						    path_string,
						    &background_area,
						    &cell_area, flags))
		{
		  if (cell_editable != NULL)
		    {
		      gint left, right;
		      GdkRectangle area;

		      area = cell_area;
		      _gtk_tree_view_column_get_neighbor_sizes (column,	_gtk_tree_view_column_get_edited_cell (column), &left, &right);

		      area.x += left;
		      area.width -= right + left;

		      gtk_tree_view_real_start_editing (tree_view,
							column,
							path,
							cell_editable,
							&area,
							(GdkEvent *)event,
							flags);
		      g_free (path_string);
		      gtk_tree_path_free (path);
		      gtk_tree_path_free (anchor);
		      return TRUE;
		    }
		  column_handled_click = TRUE;
		}
	      g_free (path_string);
	    }
	  if (anchor)
	    gtk_tree_path_free (anchor);
	}

      /* select */
      node_selected = GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_SELECTED);
      pre_val = tree_view->priv->vadjustment->value;

      /* we only handle selection modifications on the first button press
       */
      if (event->type == GDK_BUTTON_PRESS)
        {
          if ((event->state & GTK_MODIFY_SELECTION_MOD_MASK) == GTK_MODIFY_SELECTION_MOD_MASK)
            tree_view->priv->modify_selection_pressed = TRUE;
          if ((event->state & GTK_EXTEND_SELECTION_MOD_MASK) == GTK_EXTEND_SELECTION_MOD_MASK)
            tree_view->priv->extend_selection_pressed = TRUE;

          focus_cell = _gtk_tree_view_column_get_cell_at_pos (column, event->x - background_area.x);
          if (focus_cell)
            gtk_tree_view_column_focus_cell (column, focus_cell);

          if (event->state & GTK_MODIFY_SELECTION_MOD_MASK)
            {
              gtk_tree_view_real_set_cursor (tree_view, path, FALSE, TRUE);
              gtk_tree_view_real_toggle_cursor_row (tree_view);
            }
          else if (event->state & GTK_EXTEND_SELECTION_MOD_MASK)
            {
              gtk_tree_view_real_set_cursor (tree_view, path, FALSE, TRUE);
              gtk_tree_view_real_select_cursor_row (tree_view, FALSE);
            }
          else
            {
              gtk_tree_view_real_set_cursor (tree_view, path, TRUE, TRUE);
            }

          tree_view->priv->modify_selection_pressed = FALSE;
          tree_view->priv->extend_selection_pressed = FALSE;
        }

      /* the treeview may have been scrolled because of _set_cursor,
       * correct here
       */

      aft_val = tree_view->priv->vadjustment->value;
      dval = pre_val - aft_val;

      cell_area.y += dval;
      background_area.y += dval;

      /* Save press to possibly begin a drag
       */
      if (!column_handled_click &&
	  !tree_view->priv->in_grab &&
	  tree_view->priv->pressed_button < 0)
        {
          tree_view->priv->pressed_button = event->button;
          tree_view->priv->press_start_x = event->x;
          tree_view->priv->press_start_y = event->y;

	  if (tree_view->priv->rubber_banding_enable
	      && !node_selected
	      && tree_view->priv->selection->type == GTK_SELECTION_MULTIPLE)
	    {
	      tree_view->priv->press_start_y += tree_view->priv->dy;
	      tree_view->priv->rubber_band_x = event->x;
	      tree_view->priv->rubber_band_y = event->y + tree_view->priv->dy;
	      tree_view->priv->rubber_band_status = RUBBER_BAND_MAYBE_START;

	      if ((event->state & GTK_MODIFY_SELECTION_MOD_MASK) == GTK_MODIFY_SELECTION_MOD_MASK)
		tree_view->priv->rubber_band_modify = TRUE;
	      if ((event->state & GTK_EXTEND_SELECTION_MOD_MASK) == GTK_EXTEND_SELECTION_MOD_MASK)
		tree_view->priv->rubber_band_extend = TRUE;
	    }
        }

      /* Test if a double click happened on the same row. */
      if (event->button == 1 && event->type == GDK_BUTTON_PRESS)
        {
          int double_click_time, double_click_distance;

          g_object_get (gtk_settings_get_default (),
                        "gtk-double-click-time", &double_click_time,
                        "gtk-double-click-distance", &double_click_distance,
                        NULL);

          /* Same conditions as _gdk_event_button_generate */
          if (tree_view->priv->last_button_x != -1 &&
              (event->time < tree_view->priv->last_button_time + double_click_time) &&
              (ABS (event->x - tree_view->priv->last_button_x) <= double_click_distance) &&
              (ABS (event->y - tree_view->priv->last_button_y) <= double_click_distance))
            {
              /* We do no longer compare paths of this row and the
               * row clicked previously.  We use the double click
               * distance to decide whether this is a valid click,
               * allowing the mouse to slightly move over another row.
               */
              row_double_click = TRUE;

              tree_view->priv->last_button_time = 0;
              tree_view->priv->last_button_x = -1;
              tree_view->priv->last_button_y = -1;
            }
          else
            {
              tree_view->priv->last_button_time = event->time;
              tree_view->priv->last_button_x = event->x;
              tree_view->priv->last_button_y = event->y;
            }
        }

      if (row_double_click)
	{
	  gtk_grab_remove (widget);
	  gtk_tree_view_row_activated (tree_view, path, column);

          if (tree_view->priv->pressed_button == event->button)
            tree_view->priv->pressed_button = -1;
	}

      gtk_tree_path_free (path);

      /* If we activated the row through a double click we don't want to grab
       * focus back, as moving focus to another widget is pretty common.
       */
      if (!row_double_click)
	grab_focus_and_unset_draw_keyfocus (tree_view);

      return TRUE;
    }

  /* We didn't click in the window.  Let's check to see if we clicked on a column resize window.
   */
  for (i = 0, list = tree_view->priv->columns; list; list = list->next, i++)
    {
      column = list->data;
      if (event->window == column->window &&
	  column->resizable &&
	  column->window)
	{
	  gpointer drag_data;

	  if (event->type == GDK_2BUTTON_PRESS &&
	      gtk_tree_view_column_get_sizing (column) != GTK_TREE_VIEW_COLUMN_AUTOSIZE)
	    {
	      column->use_resized_width = FALSE;
	      _gtk_tree_view_column_autosize (tree_view, column);
	      return TRUE;
	    }

	  if (gdk_pointer_grab (column->window, FALSE,
				GDK_POINTER_MOTION_HINT_MASK |
				GDK_BUTTON1_MOTION_MASK |
				GDK_BUTTON_RELEASE_MASK,
				NULL, NULL, event->time))
	    return FALSE;

	  gtk_grab_add (widget);
	  GTK_TREE_VIEW_SET_FLAG (tree_view, GTK_TREE_VIEW_IN_COLUMN_RESIZE);
	  column->resized_width = column->width - tree_view->priv->last_extra_space_per_column;

	  /* block attached dnd signal handler */
	  drag_data = g_object_get_data (G_OBJECT (widget), "gtk-site-data");
	  if (drag_data)
	    g_signal_handlers_block_matched (widget,
					     G_SIGNAL_MATCH_DATA,
					     0, 0, NULL, NULL,
					     drag_data);

	  tree_view->priv->drag_pos = i;
	  tree_view->priv->x_drag = column->button->allocation.x + (rtl ? 0 : column->button->allocation.width);

	  if (!gtk_widget_has_focus (widget))
	    gtk_widget_grab_focus (widget);

	  return TRUE;
	}
    }
  return FALSE;
}

/* GtkWidget::button_release_event helper */
static gboolean
gtk_tree_view_button_release_drag_column (GtkWidget      *widget,
					  GdkEventButton *event)
{
  GtkTreeView *tree_view;
  GList *l;
  gboolean rtl;

  tree_view = GTK_TREE_VIEW (widget);

  rtl = (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL);
  gdk_display_pointer_ungrab (gtk_widget_get_display (widget), GDK_CURRENT_TIME);
  gdk_display_keyboard_ungrab (gtk_widget_get_display (widget), GDK_CURRENT_TIME);

  /* Move the button back */
  g_object_ref (tree_view->priv->drag_column->button);
  gtk_container_remove (GTK_CONTAINER (tree_view), tree_view->priv->drag_column->button);
  gtk_widget_set_parent_window (tree_view->priv->drag_column->button, tree_view->priv->header_window);
  gtk_widget_set_parent (tree_view->priv->drag_column->button, GTK_WIDGET (tree_view));
  g_object_unref (tree_view->priv->drag_column->button);
  gtk_widget_queue_resize (widget);
  if (tree_view->priv->drag_column->resizable)
    {
      gdk_window_raise (tree_view->priv->drag_column->window);
      gdk_window_show (tree_view->priv->drag_column->window);
    }
  else
    gdk_window_hide (tree_view->priv->drag_column->window);

  gtk_widget_grab_focus (tree_view->priv->drag_column->button);

  if (rtl)
    {
      if (tree_view->priv->cur_reorder &&
	  tree_view->priv->cur_reorder->right_column != tree_view->priv->drag_column)
	gtk_tree_view_move_column_after (tree_view, tree_view->priv->drag_column,
					 tree_view->priv->cur_reorder->right_column);
    }
  else
    {
      if (tree_view->priv->cur_reorder &&
	  tree_view->priv->cur_reorder->left_column != tree_view->priv->drag_column)
	gtk_tree_view_move_column_after (tree_view, tree_view->priv->drag_column,
					 tree_view->priv->cur_reorder->left_column);
    }
  tree_view->priv->drag_column = NULL;
  gdk_window_hide (tree_view->priv->drag_window);

  for (l = tree_view->priv->column_drag_info; l != NULL; l = l->next)
    g_slice_free (GtkTreeViewColumnReorder, l->data);
  g_list_free (tree_view->priv->column_drag_info);
  tree_view->priv->column_drag_info = NULL;
  tree_view->priv->cur_reorder = NULL;

  if (tree_view->priv->drag_highlight_window)
    gdk_window_hide (tree_view->priv->drag_highlight_window);

  /* Reset our flags */
  tree_view->priv->drag_column_window_state = DRAG_COLUMN_WINDOW_STATE_UNSET;
  GTK_TREE_VIEW_UNSET_FLAG (tree_view, GTK_TREE_VIEW_IN_COLUMN_DRAG);

  return TRUE;
}

/* GtkWidget::button_release_event helper */
static gboolean
gtk_tree_view_button_release_column_resize (GtkWidget      *widget,
					    GdkEventButton *event)
{
  GtkTreeView *tree_view;
  gpointer drag_data;

  tree_view = GTK_TREE_VIEW (widget);

  tree_view->priv->drag_pos = -1;

  /* unblock attached dnd signal handler */
  drag_data = g_object_get_data (G_OBJECT (widget), "gtk-site-data");
  if (drag_data)
    g_signal_handlers_unblock_matched (widget,
				       G_SIGNAL_MATCH_DATA,
				       0, 0, NULL, NULL,
				       drag_data);

  GTK_TREE_VIEW_UNSET_FLAG (tree_view, GTK_TREE_VIEW_IN_COLUMN_RESIZE);
  gtk_grab_remove (widget);
  gdk_display_pointer_ungrab (gdk_window_get_display (event->window),
			      event->time);
  return TRUE;
}

static gboolean
gtk_tree_view_button_release (GtkWidget      *widget,
			      GdkEventButton *event)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (widget);

  if (GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_IN_COLUMN_DRAG))
    return gtk_tree_view_button_release_drag_column (widget, event);

  if (tree_view->priv->rubber_band_status)
    gtk_tree_view_stop_rubber_band (tree_view);

  if (tree_view->priv->pressed_button == event->button)
    tree_view->priv->pressed_button = -1;

  if (GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_IN_COLUMN_RESIZE))
    return gtk_tree_view_button_release_column_resize (widget, event);

  if (tree_view->priv->button_pressed_node == NULL)
    return FALSE;

  if (event->button == 1)
    {
      if (tree_view->priv->button_pressed_node == tree_view->priv->prelight_node &&
          GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_ARROW_PRELIT))
	{
	  GtkTreePath *path = NULL;

	  path = _gtk_tree_view_find_path (tree_view,
					   tree_view->priv->button_pressed_tree,
					   tree_view->priv->button_pressed_node);
	  /* Actually activate the node */
	  if (tree_view->priv->button_pressed_node->children == NULL)
	    gtk_tree_view_real_expand_row (tree_view, path,
					   tree_view->priv->button_pressed_tree,
					   tree_view->priv->button_pressed_node,
					   FALSE, TRUE);
	  else
	    gtk_tree_view_real_collapse_row (GTK_TREE_VIEW (widget), path,
					     tree_view->priv->button_pressed_tree,
					     tree_view->priv->button_pressed_node, TRUE);
	  gtk_tree_path_free (path);
	}

      tree_view->priv->button_pressed_tree = NULL;
      tree_view->priv->button_pressed_node = NULL;

      gtk_grab_remove (widget);
    }

  return TRUE;
}

static gboolean
gtk_tree_view_grab_broken (GtkWidget          *widget,
			   GdkEventGrabBroken *event)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (widget);

  if (GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_IN_COLUMN_DRAG))
    gtk_tree_view_button_release_drag_column (widget, (GdkEventButton *)event);

  if (GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_IN_COLUMN_RESIZE))
    gtk_tree_view_button_release_column_resize (widget, (GdkEventButton *)event);

  return TRUE;
}

#if 0
static gboolean
gtk_tree_view_configure (GtkWidget *widget,
			 GdkEventConfigure *event)
{
  GtkTreeView *tree_view;

  tree_view = GTK_TREE_VIEW (widget);
  tree_view->priv->search_position_func (tree_view, tree_view->priv->search_window);

  return FALSE;
}
#endif

/* GtkWidget::motion_event function set.
 */

static gboolean
coords_are_over_arrow (GtkTreeView *tree_view,
                       GtkRBTree   *tree,
                       GtkRBNode   *node,
                       /* these are in bin window coords */
                       gint         x,
                       gint         y)
{
  GdkRectangle arrow;
  gint x2;

  if (!gtk_widget_get_realized (GTK_WIDGET (tree_view)))
    return FALSE;

  if ((node->flags & GTK_RBNODE_IS_PARENT) == 0)
    return FALSE;

  arrow.y = BACKGROUND_FIRST_PIXEL (tree_view, tree, node);

  arrow.height = ROW_HEIGHT (tree_view, BACKGROUND_HEIGHT (node));

  gtk_tree_view_get_arrow_xrange (tree_view, tree, &arrow.x, &x2);

  arrow.width = x2 - arrow.x;

  return (x >= arrow.x &&
          x < (arrow.x + arrow.width) &&
	  y >= arrow.y &&
	  y < (arrow.y + arrow.height));
}

static gboolean
auto_expand_timeout (gpointer data)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (data);
  GtkTreePath *path;

  if (tree_view->priv->prelight_node)
    {
      path = _gtk_tree_view_find_path (tree_view,
				       tree_view->priv->prelight_tree,
				       tree_view->priv->prelight_node);   

      if (tree_view->priv->prelight_node->children)
	gtk_tree_view_collapse_row (tree_view, path);
      else
	gtk_tree_view_expand_row (tree_view, path, FALSE);

      gtk_tree_path_free (path);
    }

  tree_view->priv->auto_expand_timeout = 0;

  return FALSE;
}

static void
remove_auto_expand_timeout (GtkTreeView *tree_view)
{
  if (tree_view->priv->auto_expand_timeout != 0)
    {
      g_source_remove (tree_view->priv->auto_expand_timeout);
      tree_view->priv->auto_expand_timeout = 0;
    }
}

static void
do_prelight (GtkTreeView *tree_view,
             GtkRBTree   *tree,
             GtkRBNode   *node,
	     /* these are in bin_window coords */
             gint         x,
             gint         y)
{
  if (tree_view->priv->prelight_tree == tree &&
      tree_view->priv->prelight_node == node)
    {
      /*  We are still on the same node,
	  but we might need to take care of the arrow  */

      if (tree && node && TREE_VIEW_DRAW_EXPANDERS (tree_view))
	{
	  gboolean over_arrow;
	  gboolean flag_set;

	  over_arrow = coords_are_over_arrow (tree_view, tree, node, x, y);
	  flag_set = GTK_TREE_VIEW_FLAG_SET (tree_view,
					     GTK_TREE_VIEW_ARROW_PRELIT);

	  if (over_arrow != flag_set)
	    {
	      if (over_arrow)
		GTK_TREE_VIEW_SET_FLAG (tree_view,
					GTK_TREE_VIEW_ARROW_PRELIT);
	      else
		GTK_TREE_VIEW_UNSET_FLAG (tree_view,
					  GTK_TREE_VIEW_ARROW_PRELIT);

	      gtk_tree_view_draw_arrow (tree_view, tree, node, x, y);
	    }
	}

      return;
    }

  if (tree_view->priv->prelight_tree && tree_view->priv->prelight_node)
    {
      /*  Unprelight the old node and arrow  */

      GTK_RBNODE_UNSET_FLAG (tree_view->priv->prelight_node,
			     GTK_RBNODE_IS_PRELIT);

      if (GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_ARROW_PRELIT)
	  && TREE_VIEW_DRAW_EXPANDERS (tree_view))
	{
	  GTK_TREE_VIEW_UNSET_FLAG (tree_view, GTK_TREE_VIEW_ARROW_PRELIT);
	  
	  gtk_tree_view_draw_arrow (tree_view,
				    tree_view->priv->prelight_tree,
				    tree_view->priv->prelight_node,
				    x,
				    y);
	}

      _gtk_tree_view_queue_draw_node (tree_view,
				      tree_view->priv->prelight_tree,
				      tree_view->priv->prelight_node,
				      NULL);
    }


  if (tree_view->priv->hover_expand)
    remove_auto_expand_timeout (tree_view);

  /*  Set the new prelight values  */
  tree_view->priv->prelight_node = node;
  tree_view->priv->prelight_tree = tree;

  if (!node || !tree)
    return;

  /*  Prelight the new node and arrow  */

  if (TREE_VIEW_DRAW_EXPANDERS (tree_view)
      && coords_are_over_arrow (tree_view, tree, node, x, y))
    {
      GTK_TREE_VIEW_SET_FLAG (tree_view, GTK_TREE_VIEW_ARROW_PRELIT);

      gtk_tree_view_draw_arrow (tree_view, tree, node, x, y);
    }

  GTK_RBNODE_SET_FLAG (node, GTK_RBNODE_IS_PRELIT);

  _gtk_tree_view_queue_draw_node (tree_view, tree, node, NULL);

  if (tree_view->priv->hover_expand)
    {
      tree_view->priv->auto_expand_timeout = 
	gdk_threads_add_timeout (AUTO_EXPAND_TIMEOUT, auto_expand_timeout, tree_view);
    }
}

static void
prelight_or_select (GtkTreeView *tree_view,
		    GtkRBTree   *tree,
		    GtkRBNode   *node,
		    /* these are in bin_window coords */
		    gint         x,
		    gint         y)
{
  GtkSelectionMode mode = gtk_tree_selection_get_mode (tree_view->priv->selection);
  
  if (tree_view->priv->hover_selection &&
      (mode == GTK_SELECTION_SINGLE || mode == GTK_SELECTION_BROWSE) &&
      !(tree_view->priv->edited_column &&
	tree_view->priv->edited_column->editable_widget))
    {
      if (node)
	{
	  if (!GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_SELECTED))
	    {
	      GtkTreePath *path;
	      
	      path = _gtk_tree_view_find_path (tree_view, tree, node);
	      gtk_tree_selection_select_path (tree_view->priv->selection, path);
	      if (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_SELECTED))
		{
		  GTK_TREE_VIEW_UNSET_FLAG (tree_view, GTK_TREE_VIEW_DRAW_KEYFOCUS);
		  gtk_tree_view_real_set_cursor (tree_view, path, FALSE, FALSE);
		}
	      gtk_tree_path_free (path);
	    }
	}

      else if (mode == GTK_SELECTION_SINGLE)
	gtk_tree_selection_unselect_all (tree_view->priv->selection);
    }

    do_prelight (tree_view, tree, node, x, y);
}

static void
ensure_unprelighted (GtkTreeView *tree_view)
{
  do_prelight (tree_view,
	       NULL, NULL,
	       -1000, -1000); /* coords not possibly over an arrow */

  g_assert (tree_view->priv->prelight_node == NULL);
}

static void
update_prelight (GtkTreeView *tree_view,
                 gint         x,
                 gint         y)
{
  int new_y;
  GtkRBTree *tree;
  GtkRBNode *node;

  if (tree_view->priv->tree == NULL)
    return;

  if (x == -10000)
    {
      ensure_unprelighted (tree_view);
      return;
    }

  new_y = TREE_WINDOW_Y_TO_RBTREE_Y (tree_view, y);
  if (new_y < 0)
    new_y = 0;

  _gtk_rbtree_find_offset (tree_view->priv->tree,
                           new_y, &tree, &node);

  if (node)
    prelight_or_select (tree_view, tree, node, x, y);
}




/* Our motion arrow is either a box (in the case of the original spot)
 * or an arrow.  It is expander_size wide.
 */
/*
 * 11111111111111
 * 01111111111110
 * 00111111111100
 * 00011111111000
 * 00001111110000
 * 00000111100000
 * 00000111100000
 * 00000111100000
 * ~ ~ ~ ~ ~ ~ ~
 * 00000111100000
 * 00000111100000
 * 00000111100000
 * 00001111110000
 * 00011111111000
 * 00111111111100
 * 01111111111110
 * 11111111111111
 */

static void
gtk_tree_view_motion_draw_column_motion_arrow (GtkTreeView *tree_view)
{
  GtkTreeViewColumnReorder *reorder = tree_view->priv->cur_reorder;
  GtkWidget *widget = GTK_WIDGET (tree_view);
  GdkBitmap *mask = NULL;
  gint x;
  gint y;
  gint width;
  gint height;
  gint arrow_type = DRAG_COLUMN_WINDOW_STATE_UNSET;
  GdkWindowAttr attributes;
  guint attributes_mask;
  cairo_t *cr;

  if (!reorder ||
      reorder->left_column == tree_view->priv->drag_column ||
      reorder->right_column == tree_view->priv->drag_column)
    arrow_type = DRAG_COLUMN_WINDOW_STATE_ORIGINAL;
  else if (reorder->left_column || reorder->right_column)
    {
      GdkRectangle visible_rect;
      gtk_tree_view_get_visible_rect (tree_view, &visible_rect);
      if (reorder->left_column)
	x = reorder->left_column->button->allocation.x + reorder->left_column->button->allocation.width;
      else
	x = reorder->right_column->button->allocation.x;

      if (x < visible_rect.x)
	arrow_type = DRAG_COLUMN_WINDOW_STATE_ARROW_LEFT;
      else if (x > visible_rect.x + visible_rect.width)
	arrow_type = DRAG_COLUMN_WINDOW_STATE_ARROW_RIGHT;
      else
        arrow_type = DRAG_COLUMN_WINDOW_STATE_ARROW;
    }

  /* We want to draw the rectangle over the initial location. */
  if (arrow_type == DRAG_COLUMN_WINDOW_STATE_ORIGINAL)
    {
      if (tree_view->priv->drag_column_window_state != DRAG_COLUMN_WINDOW_STATE_ORIGINAL)
	{
	  if (tree_view->priv->drag_highlight_window)
	    {
	      gdk_window_set_user_data (tree_view->priv->drag_highlight_window,
					NULL);
	      gdk_window_destroy (tree_view->priv->drag_highlight_window);
	    }

	  attributes.window_type = GDK_WINDOW_CHILD;
	  attributes.wclass = GDK_INPUT_OUTPUT;
          attributes.x = tree_view->priv->drag_column_x;
          attributes.y = 0;
	  width = attributes.width = tree_view->priv->drag_column->button->allocation.width;
	  height = attributes.height = tree_view->priv->drag_column->button->allocation.height;
	  attributes.visual = gtk_widget_get_visual (GTK_WIDGET (tree_view));
	  attributes.colormap = gtk_widget_get_colormap (GTK_WIDGET (tree_view));
	  attributes.event_mask = GDK_VISIBILITY_NOTIFY_MASK | GDK_EXPOSURE_MASK | GDK_POINTER_MOTION_MASK;
	  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
	  tree_view->priv->drag_highlight_window = gdk_window_new (tree_view->priv->header_window, &attributes, attributes_mask);
	  gdk_window_set_user_data (tree_view->priv->drag_highlight_window, GTK_WIDGET (tree_view));

	  mask = gdk_pixmap_new (tree_view->priv->drag_highlight_window, width, height, 1);
          cr = gdk_cairo_create (mask);

          cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
          cairo_paint (cr);
          cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
          cairo_rectangle (cr, 1, 1, width - 2, height - 2);
          cairo_stroke (cr);
          cairo_destroy (cr);

	  gdk_window_shape_combine_mask (tree_view->priv->drag_highlight_window,
					 mask, 0, 0);
	  if (mask) g_object_unref (mask);
	  tree_view->priv->drag_column_window_state = DRAG_COLUMN_WINDOW_STATE_ORIGINAL;
	}
    }
  else if (arrow_type == DRAG_COLUMN_WINDOW_STATE_ARROW)
    {
      width = tree_view->priv->expander_size;

      /* Get x, y, width, height of arrow */
      gdk_window_get_origin (tree_view->priv->header_window, &x, &y);
      if (reorder->left_column)
	{
	  x += reorder->left_column->button->allocation.x + reorder->left_column->button->allocation.width - width/2;
	  height = reorder->left_column->button->allocation.height;
	}
      else
	{
	  x += reorder->right_column->button->allocation.x - width/2;
	  height = reorder->right_column->button->allocation.height;
	}
      y -= tree_view->priv->expander_size/2; /* The arrow takes up only half the space */
      height += tree_view->priv->expander_size;

      /* Create the new window */
      if (tree_view->priv->drag_column_window_state != DRAG_COLUMN_WINDOW_STATE_ARROW)
	{
	  if (tree_view->priv->drag_highlight_window)
	    {
	      gdk_window_set_user_data (tree_view->priv->drag_highlight_window,
					NULL);
	      gdk_window_destroy (tree_view->priv->drag_highlight_window);
	    }

	  attributes.window_type = GDK_WINDOW_TEMP;
	  attributes.wclass = GDK_INPUT_OUTPUT;
	  attributes.visual = gtk_widget_get_visual (GTK_WIDGET (tree_view));
	  attributes.colormap = gtk_widget_get_colormap (GTK_WIDGET (tree_view));
	  attributes.event_mask = GDK_VISIBILITY_NOTIFY_MASK | GDK_EXPOSURE_MASK | GDK_POINTER_MOTION_MASK;
	  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
          attributes.x = x;
          attributes.y = y;
	  attributes.width = width;
	  attributes.height = height;
	  tree_view->priv->drag_highlight_window = gdk_window_new (gtk_widget_get_root_window (widget),
								   &attributes, attributes_mask);
	  gdk_window_set_user_data (tree_view->priv->drag_highlight_window, GTK_WIDGET (tree_view));

	  mask = gdk_pixmap_new (tree_view->priv->drag_highlight_window, width, height, 1);
          cr = gdk_cairo_create (mask);

          cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
          cairo_paint (cr);
          cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
          cairo_move_to (cr, 0, 0);
          cairo_line_to (cr, width, 0);
          cairo_line_to (cr, width / 2., width / 2);
          cairo_move_to (cr, 0, height);
          cairo_line_to (cr, width, height);
          cairo_line_to (cr, width / 2., height - width / 2.);
          cairo_fill (cr);

          cairo_destroy (cr);
	  gdk_window_shape_combine_mask (tree_view->priv->drag_highlight_window,
					 mask, 0, 0);
	  if (mask) g_object_unref (mask);
	}

      tree_view->priv->drag_column_window_state = DRAG_COLUMN_WINDOW_STATE_ARROW;
      gdk_window_move (tree_view->priv->drag_highlight_window, x, y);
    }
  else if (arrow_type == DRAG_COLUMN_WINDOW_STATE_ARROW_LEFT ||
	   arrow_type == DRAG_COLUMN_WINDOW_STATE_ARROW_RIGHT)
    {
      width = tree_view->priv->expander_size;

      /* Get x, y, width, height of arrow */
      width = width/2; /* remember, the arrow only takes half the available width */
      gdk_window_get_origin (widget->window, &x, &y);
      if (arrow_type == DRAG_COLUMN_WINDOW_STATE_ARROW_RIGHT)
	x += widget->allocation.width - width;

      if (reorder->left_column)
	height = reorder->left_column->button->allocation.height;
      else
	height = reorder->right_column->button->allocation.height;

      y -= tree_view->priv->expander_size;
      height += 2*tree_view->priv->expander_size;

      /* Create the new window */
      if (tree_view->priv->drag_column_window_state != DRAG_COLUMN_WINDOW_STATE_ARROW_LEFT &&
	  tree_view->priv->drag_column_window_state != DRAG_COLUMN_WINDOW_STATE_ARROW_RIGHT)
	{
	  if (tree_view->priv->drag_highlight_window)
	    {
	      gdk_window_set_user_data (tree_view->priv->drag_highlight_window,
					NULL);
	      gdk_window_destroy (tree_view->priv->drag_highlight_window);
	    }

	  attributes.window_type = GDK_WINDOW_TEMP;
	  attributes.wclass = GDK_INPUT_OUTPUT;
	  attributes.visual = gtk_widget_get_visual (GTK_WIDGET (tree_view));
	  attributes.colormap = gtk_widget_get_colormap (GTK_WIDGET (tree_view));
	  attributes.event_mask = GDK_VISIBILITY_NOTIFY_MASK | GDK_EXPOSURE_MASK | GDK_POINTER_MOTION_MASK;
	  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
          attributes.x = x;
          attributes.y = y;
	  attributes.width = width;
	  attributes.height = height;
	  tree_view->priv->drag_highlight_window = gdk_window_new (NULL, &attributes, attributes_mask);
	  gdk_window_set_user_data (tree_view->priv->drag_highlight_window, GTK_WIDGET (tree_view));

	  mask = gdk_pixmap_new (tree_view->priv->drag_highlight_window, width, height, 1);
          cr = gdk_cairo_create (mask);

          cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
          cairo_paint (cr);
          cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
          /* mirror if we're on the left */
          if (arrow_type == DRAG_COLUMN_WINDOW_STATE_ARROW_LEFT)
            {
              cairo_translate (cr, width, 0);
              cairo_scale (cr, -1, 1);
            }
          cairo_move_to (cr, 0, 0);
          cairo_line_to (cr, width, width);
          cairo_line_to (cr, 0, tree_view->priv->expander_size);
          cairo_move_to (cr, 0, height);
          cairo_line_to (cr, width, height - width);
          cairo_line_to (cr, 0, height - tree_view->priv->expander_size);
          cairo_fill (cr);

          cairo_destroy (cr);
	  gdk_window_shape_combine_mask (tree_view->priv->drag_highlight_window,
					 mask, 0, 0);
	  if (mask) g_object_unref (mask);
	}

      tree_view->priv->drag_column_window_state = arrow_type;
      gdk_window_move (tree_view->priv->drag_highlight_window, x, y);
   }
  else
    {
      g_warning (G_STRLOC"Invalid GtkTreeViewColumnReorder struct");
      gdk_window_hide (tree_view->priv->drag_highlight_window);
      return;
    }

  gdk_window_show (tree_view->priv->drag_highlight_window);
  gdk_window_raise (tree_view->priv->drag_highlight_window);
}

static gboolean
gtk_tree_view_motion_resize_column (GtkWidget      *widget,
				    GdkEventMotion *event)
{
  gint x;
  gint new_width;
  GtkTreeViewColumn *column;
  GtkTreeView *tree_view = GTK_TREE_VIEW (widget);

  column = gtk_tree_view_get_column (tree_view, tree_view->priv->drag_pos);

  if (event->is_hint || event->window != widget->window)
    gtk_widget_get_pointer (widget, &x, NULL);
  else
    x = event->x;

  if (tree_view->priv->hadjustment)
    x += tree_view->priv->hadjustment->value;

  new_width = gtk_tree_view_new_column_width (tree_view,
					      tree_view->priv->drag_pos, &x);
  if (x != tree_view->priv->x_drag &&
      (new_width != column->fixed_width))
    {
      column->use_resized_width = TRUE;
      column->resized_width = new_width;
      if (column->expand)
	column->resized_width -= tree_view->priv->last_extra_space_per_column;
      gtk_widget_queue_resize (widget);
    }

  return FALSE;
}


static void
gtk_tree_view_update_current_reorder (GtkTreeView *tree_view)
{
  GtkTreeViewColumnReorder *reorder = NULL;
  GList *list;
  gint mouse_x;

  gdk_window_get_pointer (tree_view->priv->header_window, &mouse_x, NULL, NULL);
  for (list = tree_view->priv->column_drag_info; list; list = list->next)
    {
      reorder = (GtkTreeViewColumnReorder *) list->data;
      if (mouse_x >= reorder->left_align && mouse_x < reorder->right_align)
	break;
      reorder = NULL;
    }

  /*  if (reorder && reorder == tree_view->priv->cur_reorder)
      return;*/

  tree_view->priv->cur_reorder = reorder;
  gtk_tree_view_motion_draw_column_motion_arrow (tree_view);
}

static void
gtk_tree_view_vertical_autoscroll (GtkTreeView *tree_view)
{
  GdkRectangle visible_rect;
  gint y;
  gint offset;
  gfloat value;

  gdk_window_get_pointer (tree_view->priv->bin_window, NULL, &y, NULL);
  y += tree_view->priv->dy;

  gtk_tree_view_get_visible_rect (tree_view, &visible_rect);

  /* see if we are near the edge. */
  offset = y - (visible_rect.y + 2 * SCROLL_EDGE_SIZE);
  if (offset > 0)
    {
      offset = y - (visible_rect.y + visible_rect.height - 2 * SCROLL_EDGE_SIZE);
      if (offset < 0)
	return;
    }

  value = CLAMP (tree_view->priv->vadjustment->value + offset, 0.0,
		 tree_view->priv->vadjustment->upper - tree_view->priv->vadjustment->page_size);
  gtk_adjustment_set_value (tree_view->priv->vadjustment, value);
}

static gboolean
gtk_tree_view_horizontal_autoscroll (GtkTreeView *tree_view)
{
  GdkRectangle visible_rect;
  gint x;
  gint offset;
  gfloat value;

  gdk_window_get_pointer (tree_view->priv->bin_window, &x, NULL, NULL);

  gtk_tree_view_get_visible_rect (tree_view, &visible_rect);

  /* See if we are near the edge. */
  offset = x - (visible_rect.x + SCROLL_EDGE_SIZE);
  if (offset > 0)
    {
      offset = x - (visible_rect.x + visible_rect.width - SCROLL_EDGE_SIZE);
      if (offset < 0)
	return TRUE;
    }
  offset = offset/3;

  value = CLAMP (tree_view->priv->hadjustment->value + offset,
		 0.0, tree_view->priv->hadjustment->upper - tree_view->priv->hadjustment->page_size);
  gtk_adjustment_set_value (tree_view->priv->hadjustment, value);

  return TRUE;

}

static gboolean
gtk_tree_view_motion_drag_column (GtkWidget      *widget,
				  GdkEventMotion *event)
{
  GtkTreeView *tree_view = (GtkTreeView *) widget;
  GtkTreeViewColumn *column = tree_view->priv->drag_column;
  gint x, y;

  /* Sanity Check */
  if ((column == NULL) ||
      (event->window != tree_view->priv->drag_window))
    return FALSE;

  /* Handle moving the header */
  gdk_window_get_position (tree_view->priv->drag_window, &x, &y);
  x = CLAMP (x + (gint)event->x - column->drag_x, 0,
	     MAX (tree_view->priv->width, GTK_WIDGET (tree_view)->allocation.width) - column->button->allocation.width);
  gdk_window_move (tree_view->priv->drag_window, x, y);
  
  /* autoscroll, if needed */
  gtk_tree_view_horizontal_autoscroll (tree_view);
  /* Update the current reorder position and arrow; */
  gtk_tree_view_update_current_reorder (tree_view);

  return TRUE;
}

static void
gtk_tree_view_stop_rubber_band (GtkTreeView *tree_view)
{
  remove_scroll_timeout (tree_view);
  gtk_grab_remove (GTK_WIDGET (tree_view));

  if (tree_view->priv->rubber_band_status == RUBBER_BAND_ACTIVE)
    {
      GtkTreePath *tmp_path;

      gtk_widget_queue_draw (GTK_WIDGET (tree_view));

      /* The anchor path should be set to the start path */
      tmp_path = _gtk_tree_view_find_path (tree_view,
					   tree_view->priv->rubber_band_start_tree,
					   tree_view->priv->rubber_band_start_node);

      if (tree_view->priv->anchor)
	gtk_tree_row_reference_free (tree_view->priv->anchor);

      tree_view->priv->anchor =
	gtk_tree_row_reference_new_proxy (G_OBJECT (tree_view),
					  tree_view->priv->model,
					  tmp_path);

      gtk_tree_path_free (tmp_path);

      /* ... and the cursor to the end path */
      tmp_path = _gtk_tree_view_find_path (tree_view,
					   tree_view->priv->rubber_band_end_tree,
					   tree_view->priv->rubber_band_end_node);
      gtk_tree_view_real_set_cursor (GTK_TREE_VIEW (tree_view), tmp_path, FALSE, FALSE);
      gtk_tree_path_free (tmp_path);

      _gtk_tree_selection_emit_changed (tree_view->priv->selection);
    }

  /* Clear status variables */
  tree_view->priv->rubber_band_status = RUBBER_BAND_OFF;
  tree_view->priv->rubber_band_extend = FALSE;
  tree_view->priv->rubber_band_modify = FALSE;

  tree_view->priv->rubber_band_start_node = NULL;
  tree_view->priv->rubber_band_start_tree = NULL;
  tree_view->priv->rubber_band_end_node = NULL;
  tree_view->priv->rubber_band_end_tree = NULL;
}

static void
gtk_tree_view_update_rubber_band_selection_range (GtkTreeView *tree_view,
						 GtkRBTree   *start_tree,
						 GtkRBNode   *start_node,
						 GtkRBTree   *end_tree,
						 GtkRBNode   *end_node,
						 gboolean     select,
						 gboolean     skip_start,
						 gboolean     skip_end)
{
  if (start_node == end_node)
    return;

  /* We skip the first node and jump inside the loop */
  if (skip_start)
    goto skip_first;

  do
    {
      /* Small optimization by assuming insensitive nodes are never
       * selected.
       */
      if (!GTK_RBNODE_FLAG_SET (start_node, GTK_RBNODE_IS_SELECTED))
        {
	  GtkTreePath *path;
	  gboolean selectable;

	  path = _gtk_tree_view_find_path (tree_view, start_tree, start_node);
	  selectable = _gtk_tree_selection_row_is_selectable (tree_view->priv->selection, start_node, path);
	  gtk_tree_path_free (path);

	  if (!selectable)
	    goto node_not_selectable;
	}

      if (select)
        {
	  if (tree_view->priv->rubber_band_extend)
            GTK_RBNODE_SET_FLAG (start_node, GTK_RBNODE_IS_SELECTED);
	  else if (tree_view->priv->rubber_band_modify)
	    {
	      /* Toggle the selection state */
	      if (GTK_RBNODE_FLAG_SET (start_node, GTK_RBNODE_IS_SELECTED))
		GTK_RBNODE_UNSET_FLAG (start_node, GTK_RBNODE_IS_SELECTED);
	      else
		GTK_RBNODE_SET_FLAG (start_node, GTK_RBNODE_IS_SELECTED);
	    }
	  else
	    GTK_RBNODE_SET_FLAG (start_node, GTK_RBNODE_IS_SELECTED);
	}
      else
        {
	  /* Mirror the above */
	  if (tree_view->priv->rubber_band_extend)
	    GTK_RBNODE_UNSET_FLAG (start_node, GTK_RBNODE_IS_SELECTED);
	  else if (tree_view->priv->rubber_band_modify)
	    {
	      /* Toggle the selection state */
	      if (GTK_RBNODE_FLAG_SET (start_node, GTK_RBNODE_IS_SELECTED))
		GTK_RBNODE_UNSET_FLAG (start_node, GTK_RBNODE_IS_SELECTED);
	      else
		GTK_RBNODE_SET_FLAG (start_node, GTK_RBNODE_IS_SELECTED);
	    }
	  else
	    GTK_RBNODE_UNSET_FLAG (start_node, GTK_RBNODE_IS_SELECTED);
	}

      _gtk_tree_view_queue_draw_node (tree_view, start_tree, start_node, NULL);

node_not_selectable:
      if (start_node == end_node)
	break;

skip_first:

      if (start_node->children)
        {
	  start_tree = start_node->children;
	  start_node = start_tree->root;
	  while (start_node->left != start_tree->nil)
	    start_node = start_node->left;
	}
      else
        {
	  _gtk_rbtree_next_full (start_tree, start_node, &start_tree, &start_node);

	  if (!start_tree)
	    /* Ran out of tree */
	    break;
	}

      if (skip_end && start_node == end_node)
	break;
    }
  while (TRUE);
}

static void
gtk_tree_view_update_rubber_band_selection (GtkTreeView *tree_view)
{
  GtkRBTree *start_tree, *end_tree;
  GtkRBNode *start_node, *end_node;

  _gtk_rbtree_find_offset (tree_view->priv->tree, MIN (tree_view->priv->press_start_y, tree_view->priv->rubber_band_y), &start_tree, &start_node);
  _gtk_rbtree_find_offset (tree_view->priv->tree, MAX (tree_view->priv->press_start_y, tree_view->priv->rubber_band_y), &end_tree, &end_node);

  /* Handle the start area first */
  if (!tree_view->priv->rubber_band_start_node)
    {
      gtk_tree_view_update_rubber_band_selection_range (tree_view,
						       start_tree,
						       start_node,
						       end_tree,
						       end_node,
						       TRUE,
						       FALSE,
						       FALSE);
    }
  else if (_gtk_rbtree_node_find_offset (start_tree, start_node) <
           _gtk_rbtree_node_find_offset (tree_view->priv->rubber_band_start_tree, tree_view->priv->rubber_band_start_node))
    {
      /* New node is above the old one; selection became bigger */
      gtk_tree_view_update_rubber_band_selection_range (tree_view,
						       start_tree,
						       start_node,
						       tree_view->priv->rubber_band_start_tree,
						       tree_view->priv->rubber_band_start_node,
						       TRUE,
						       FALSE,
						       TRUE);
    }
  else if (_gtk_rbtree_node_find_offset (start_tree, start_node) >
           _gtk_rbtree_node_find_offset (tree_view->priv->rubber_band_start_tree, tree_view->priv->rubber_band_start_node))
    {
      /* New node is below the old one; selection became smaller */
      gtk_tree_view_update_rubber_band_selection_range (tree_view,
						       tree_view->priv->rubber_band_start_tree,
						       tree_view->priv->rubber_band_start_node,
						       start_tree,
						       start_node,
						       FALSE,
						       FALSE,
						       TRUE);
    }

  tree_view->priv->rubber_band_start_tree = start_tree;
  tree_view->priv->rubber_band_start_node = start_node;

  /* Next, handle the end area */
  if (!tree_view->priv->rubber_band_end_node)
    {
      /* In the event this happens, start_node was also NULL; this case is
       * handled above.
       */
    }
  else if (!end_node)
    {
      /* Find the last node in the tree */
      _gtk_rbtree_find_offset (tree_view->priv->tree, tree_view->priv->height - 1,
			       &end_tree, &end_node);

      /* Selection reached end of the tree */
      gtk_tree_view_update_rubber_band_selection_range (tree_view,
						       tree_view->priv->rubber_band_end_tree,
						       tree_view->priv->rubber_band_end_node,
						       end_tree,
						       end_node,
						       TRUE,
						       TRUE,
						       FALSE);
    }
  else if (_gtk_rbtree_node_find_offset (end_tree, end_node) >
           _gtk_rbtree_node_find_offset (tree_view->priv->rubber_band_end_tree, tree_view->priv->rubber_band_end_node))
    {
      /* New node is below the old one; selection became bigger */
      gtk_tree_view_update_rubber_band_selection_range (tree_view,
						       tree_view->priv->rubber_band_end_tree,
						       tree_view->priv->rubber_band_end_node,
						       end_tree,
						       end_node,
						       TRUE,
						       TRUE,
						       FALSE);
    }
  else if (_gtk_rbtree_node_find_offset (end_tree, end_node) <
           _gtk_rbtree_node_find_offset (tree_view->priv->rubber_band_end_tree, tree_view->priv->rubber_band_end_node))
    {
      /* New node is above the old one; selection became smaller */
      gtk_tree_view_update_rubber_band_selection_range (tree_view,
						       end_tree,
						       end_node,
						       tree_view->priv->rubber_band_end_tree,
						       tree_view->priv->rubber_band_end_node,
						       FALSE,
						       TRUE,
						       FALSE);
    }

  tree_view->priv->rubber_band_end_tree = end_tree;
  tree_view->priv->rubber_band_end_node = end_node;
}

static void
gtk_tree_view_update_rubber_band (GtkTreeView *tree_view)
{
  gint x, y;
  GdkRectangle old_area;
  GdkRectangle new_area;
  GdkRectangle common;
  GdkRegion *invalid_region;

  old_area.x = MIN (tree_view->priv->press_start_x, tree_view->priv->rubber_band_x);
  old_area.y = MIN (tree_view->priv->press_start_y, tree_view->priv->rubber_band_y) - tree_view->priv->dy;
  old_area.width = ABS (tree_view->priv->rubber_band_x - tree_view->priv->press_start_x) + 1;
  old_area.height = ABS (tree_view->priv->rubber_band_y - tree_view->priv->press_start_y) + 1;

  gdk_window_get_pointer (tree_view->priv->bin_window, &x, &y, NULL);

  x = MAX (x, 0);
  y = MAX (y, 0) + tree_view->priv->dy;

  new_area.x = MIN (tree_view->priv->press_start_x, x);
  new_area.y = MIN (tree_view->priv->press_start_y, y) - tree_view->priv->dy;
  new_area.width = ABS (x - tree_view->priv->press_start_x) + 1;
  new_area.height = ABS (y - tree_view->priv->press_start_y) + 1;

  invalid_region = gdk_region_rectangle (&old_area);
  gdk_region_union_with_rect (invalid_region, &new_area);

  gdk_rectangle_intersect (&old_area, &new_area, &common);
  if (common.width > 2 && common.height > 2)
    {
      GdkRegion *common_region;

      /* make sure the border is invalidated */
      common.x += 1;
      common.y += 1;
      common.width -= 2;
      common.height -= 2;

      common_region = gdk_region_rectangle (&common);

      gdk_region_subtract (invalid_region, common_region);
      gdk_region_destroy (common_region);
    }

  gdk_window_invalidate_region (tree_view->priv->bin_window, invalid_region, TRUE);

  gdk_region_destroy (invalid_region);

  tree_view->priv->rubber_band_x = x;
  tree_view->priv->rubber_band_y = y;

  gtk_tree_view_update_rubber_band_selection (tree_view);
}

static void
gtk_tree_view_paint_rubber_band (GtkTreeView  *tree_view,
				GdkRectangle *area)
{
  cairo_t *cr;
  GdkRectangle rect;
  GdkRectangle rubber_rect;

  rubber_rect.x = MIN (tree_view->priv->press_start_x, tree_view->priv->rubber_band_x);
  rubber_rect.y = MIN (tree_view->priv->press_start_y, tree_view->priv->rubber_band_y) - tree_view->priv->dy;
  rubber_rect.width = ABS (tree_view->priv->press_start_x - tree_view->priv->rubber_band_x) + 1;
  rubber_rect.height = ABS (tree_view->priv->press_start_y - tree_view->priv->rubber_band_y) + 1;

  if (!gdk_rectangle_intersect (&rubber_rect, area, &rect))
    return;

  cr = gdk_cairo_create (tree_view->priv->bin_window);
  cairo_set_line_width (cr, 1.0);

  cairo_set_source_rgba (cr,
			 GTK_WIDGET (tree_view)->style->fg[GTK_STATE_NORMAL].red / 65535.,
			 GTK_WIDGET (tree_view)->style->fg[GTK_STATE_NORMAL].green / 65535.,
			 GTK_WIDGET (tree_view)->style->fg[GTK_STATE_NORMAL].blue / 65535.,
			 .25);

  gdk_cairo_rectangle (cr, &rect);
  cairo_clip (cr);
  cairo_paint (cr);

  cairo_set_source_rgb (cr,
			GTK_WIDGET (tree_view)->style->fg[GTK_STATE_NORMAL].red / 65535.,
			GTK_WIDGET (tree_view)->style->fg[GTK_STATE_NORMAL].green / 65535.,
			GTK_WIDGET (tree_view)->style->fg[GTK_STATE_NORMAL].blue / 65535.);

  cairo_rectangle (cr,
		   rubber_rect.x + 0.5, rubber_rect.y + 0.5,
		   rubber_rect.width - 1, rubber_rect.height - 1);
  cairo_stroke (cr);

  cairo_destroy (cr);
}

static gboolean
gtk_tree_view_motion_bin_window (GtkWidget      *widget,
				 GdkEventMotion *event)
{
  GtkTreeView *tree_view;
  GtkRBTree *tree;
  GtkRBNode *node;
  gint new_y;

  tree_view = (GtkTreeView *) widget;

  if (tree_view->priv->tree == NULL)
    return FALSE;

  if (tree_view->priv->rubber_band_status == RUBBER_BAND_MAYBE_START)
    {
      gtk_grab_add (GTK_WIDGET (tree_view));
      gtk_tree_view_update_rubber_band (tree_view);

      tree_view->priv->rubber_band_status = RUBBER_BAND_ACTIVE;
    }
  else if (tree_view->priv->rubber_band_status == RUBBER_BAND_ACTIVE)
    {
      gtk_tree_view_update_rubber_band (tree_view);

      add_scroll_timeout (tree_view);
    }

  /* only check for an initiated drag when a button is pressed */
  if (tree_view->priv->pressed_button >= 0
      && !tree_view->priv->rubber_band_status)
    gtk_tree_view_maybe_begin_dragging_row (tree_view, event);

  new_y = TREE_WINDOW_Y_TO_RBTREE_Y(tree_view, event->y);
  if (new_y < 0)
    new_y = 0;

  _gtk_rbtree_find_offset (tree_view->priv->tree, new_y, &tree, &node);

  /* If we are currently pressing down a button, we don't want to prelight anything else. */
  if ((tree_view->priv->button_pressed_node != NULL) &&
      (tree_view->priv->button_pressed_node != node))
    node = NULL;

  tree_view->priv->event_last_x = event->x;
  tree_view->priv->event_last_y = event->y;

  prelight_or_select (tree_view, tree, node, event->x, event->y);

  return TRUE;
}

static gboolean
gtk_tree_view_motion (GtkWidget      *widget,
		      GdkEventMotion *event)
{
  GtkTreeView *tree_view;

  tree_view = (GtkTreeView *) widget;

  /* Resizing a column */
  if (GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_IN_COLUMN_RESIZE))
    return gtk_tree_view_motion_resize_column (widget, event);

  /* Drag column */
  if (GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_IN_COLUMN_DRAG))
    return gtk_tree_view_motion_drag_column (widget, event);

  /* Sanity check it */
  if (event->window == tree_view->priv->bin_window)
    return gtk_tree_view_motion_bin_window (widget, event);

  return FALSE;
}

/* Invalidate the focus rectangle near the edge of the bin_window; used when
 * the tree is empty.
 */
static void
invalidate_empty_focus (GtkTreeView *tree_view)
{
  GdkRectangle area;

  if (!gtk_widget_has_focus (GTK_WIDGET (tree_view)))
    return;

  area.x = 0;
  area.y = 0;
  area.width = gdk_window_get_width (tree_view->priv->bin_window);
  area.height = gdk_window_get_height (tree_view->priv->bin_window);
  gdk_window_invalidate_rect (tree_view->priv->bin_window, &area, FALSE);
}

/* Draws a focus rectangle near the edge of the bin_window; used when the tree
 * is empty.
 */
static void
draw_empty_focus (GtkTreeView *tree_view, GdkRectangle *clip_area)
{
  GtkWidget *widget = GTK_WIDGET (tree_view);
  gint w, h;

  if (!gtk_widget_has_focus (widget))
    return;

  w = gdk_window_get_width (tree_view->priv->bin_window);
  h = gdk_window_get_height (tree_view->priv->bin_window);

  w -= 2;
  h -= 2;

  if (w > 0 && h > 0)
    gtk_paint_focus (gtk_widget_get_style (widget),
		     tree_view->priv->bin_window,
		     gtk_widget_get_state (widget),
		     clip_area,
		     widget,
		     NULL,
		     1, 1, w, h);
}

typedef enum {
  GTK_TREE_VIEW_GRID_LINE,
  GTK_TREE_VIEW_TREE_LINE,
  GTK_TREE_VIEW_FOREGROUND_LINE
} GtkTreeViewLineType;

static void
gtk_tree_view_draw_line (GtkTreeView         *tree_view,
                         GdkWindow           *window,
                         GtkTreeViewLineType  type,
                         int                  x1,
                         int                  y1,
                         int                  x2,
                         int                  y2)
{
  cairo_t *cr;

  cr = gdk_cairo_create (window);

  switch (type)
    {
    case GTK_TREE_VIEW_TREE_LINE:
      cairo_set_source_rgb (cr, 0, 0, 0);
      cairo_set_line_width (cr, tree_view->priv->tree_line_width);
      if (tree_view->priv->tree_line_dashes[0])
        cairo_set_dash (cr, 
                        tree_view->priv->tree_line_dashes,
                        2, 0.5);
      break;
    case GTK_TREE_VIEW_GRID_LINE:
      cairo_set_source_rgb (cr, 0, 0, 0);
      cairo_set_line_width (cr, tree_view->priv->grid_line_width);
      if (tree_view->priv->grid_line_dashes[0])
        cairo_set_dash (cr, 
                        tree_view->priv->grid_line_dashes,
                        2, 0.5);
      break;
    default:
      g_assert_not_reached ();
      /* fall through */
    case GTK_TREE_VIEW_FOREGROUND_LINE:
      cairo_set_line_width (cr, 1.0);
      gdk_cairo_set_source_color (cr,
          &GTK_WIDGET (tree_view)->style->fg[gtk_widget_get_state (GTK_WIDGET (tree_view))]);
      break;
    }

  cairo_move_to (cr, x1 + 0.5, y1 + 0.5);
  cairo_line_to (cr, x2 + 0.5, y2 + 0.5);
  cairo_stroke (cr);

  cairo_destroy (cr);
}
                         
static void
gtk_tree_view_draw_grid_lines (GtkTreeView    *tree_view,
			       GdkEventExpose *event,
			       gint            n_visible_columns)
{
  GList *list = tree_view->priv->columns;
  gint i = 0;
  gint current_x = 0;

  if (tree_view->priv->grid_lines != GTK_TREE_VIEW_GRID_LINES_VERTICAL
      && tree_view->priv->grid_lines != GTK_TREE_VIEW_GRID_LINES_BOTH)
    return;

  /* Only draw the lines for visible rows and columns */
  for (list = tree_view->priv->columns; list; list = list->next)
    {
      GtkTreeViewColumn *column = list->data;

      /* We don't want a line for the last column */
      if (i == n_visible_columns - 1)
	break;

      if (! column->visible)
	continue;

      i++;

      current_x += column->width;

      gtk_tree_view_draw_line (tree_view, event->window,
                               GTK_TREE_VIEW_GRID_LINE,
                               current_x - 1, 0,
                               current_x - 1, tree_view->priv->height);
    }
}

/* Warning: Very scary function.
 * Modify at your own risk
 *
 * KEEP IN SYNC WITH gtk_tree_view_create_row_drag_icon()!
 * FIXME: It's not...
 */
static gboolean
gtk_tree_view_bin_expose (GtkWidget      *widget,
			  GdkEventExpose *event)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (widget);
  GtkTreePath *path;
  GtkRBTree *tree;
  GList *list;
  GtkRBNode *node;
  GtkRBNode *cursor = NULL;
  GtkRBTree *cursor_tree = NULL;
  GtkRBNode *drag_highlight = NULL;
  GtkRBTree *drag_highlight_tree = NULL;
  GtkTreeIter iter;
  gint new_y;
  gint y_offset, cell_offset;
  gint max_height;
  gint depth;
  GdkRectangle background_area;
  GdkRectangle cell_area;
  guint flags;
  gint highlight_x;
  gint expander_cell_width;
  gint bin_window_width;
  gint bin_window_height;
  GtkTreePath *cursor_path;
  GtkTreePath *drag_dest_path;
  GList *first_column, *last_column;
  gint vertical_separator;
  gint horizontal_separator;
  gint focus_line_width;
  gboolean allow_rules;
  gboolean has_special_cell;
  gboolean rtl;
  gint n_visible_columns;
  gint pointer_x, pointer_y;
  gint grid_line_width;
  gboolean got_pointer = FALSE;
  gboolean row_ending_details;
  gboolean draw_vgrid_lines, draw_hgrid_lines;

  rtl = (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL);

  gtk_widget_style_get (widget,
			"horizontal-separator", &horizontal_separator,
			"vertical-separator", &vertical_separator,
			"allow-rules", &allow_rules,
			"focus-line-width", &focus_line_width,
			"row-ending-details", &row_ending_details,
			NULL);

  if (tree_view->priv->tree == NULL)
    {
      draw_empty_focus (tree_view, &event->area);
      return TRUE;
    }

  /* clip event->area to the visible area */
  if (event->area.height < 0)
    return TRUE;

  validate_visible_area (tree_view);

  new_y = TREE_WINDOW_Y_TO_RBTREE_Y (tree_view, event->area.y);

  if (new_y < 0)
    new_y = 0;
  y_offset = -_gtk_rbtree_find_offset (tree_view->priv->tree, new_y, &tree, &node);
  bin_window_width = gdk_window_get_width (tree_view->priv->bin_window);
  bin_window_height = gdk_window_get_height (tree_view->priv->bin_window);

  if (tree_view->priv->height < bin_window_height)
    {
      gtk_paint_flat_box (widget->style,
                          event->window,
                          widget->state,
                          GTK_SHADOW_NONE,
                          &event->area,
                          widget,
                          "cell_even",
                          0, tree_view->priv->height,
                          bin_window_width,
                          bin_window_height - tree_view->priv->height);
    }

  if (node == NULL)
    return TRUE;

  /* find the path for the node */
  path = _gtk_tree_view_find_path ((GtkTreeView *)widget,
				   tree,
				   node);
  gtk_tree_model_get_iter (tree_view->priv->model,
			   &iter,
			   path);
  depth = gtk_tree_path_get_depth (path);
  gtk_tree_path_free (path);
  
  cursor_path = NULL;
  drag_dest_path = NULL;

  if (tree_view->priv->cursor)
    cursor_path = gtk_tree_row_reference_get_path (tree_view->priv->cursor);

  if (cursor_path)
    _gtk_tree_view_find_node (tree_view, cursor_path,
                              &cursor_tree, &cursor);

  if (tree_view->priv->drag_dest_row)
    drag_dest_path = gtk_tree_row_reference_get_path (tree_view->priv->drag_dest_row);

  if (drag_dest_path)
    _gtk_tree_view_find_node (tree_view, drag_dest_path,
                              &drag_highlight_tree, &drag_highlight);

  draw_vgrid_lines =
    tree_view->priv->grid_lines == GTK_TREE_VIEW_GRID_LINES_VERTICAL
    || tree_view->priv->grid_lines == GTK_TREE_VIEW_GRID_LINES_BOTH;
  draw_hgrid_lines =
    tree_view->priv->grid_lines == GTK_TREE_VIEW_GRID_LINES_HORIZONTAL
    || tree_view->priv->grid_lines == GTK_TREE_VIEW_GRID_LINES_BOTH;

  if (draw_vgrid_lines || draw_hgrid_lines)
    gtk_widget_style_get (widget, "grid-line-width", &grid_line_width, NULL);
  
  n_visible_columns = 0;
  for (list = tree_view->priv->columns; list; list = list->next)
    {
      if (! GTK_TREE_VIEW_COLUMN (list->data)->visible)
	continue;
      n_visible_columns ++;
    }

  /* Find the last column */
  for (last_column = g_list_last (tree_view->priv->columns);
       last_column && !(GTK_TREE_VIEW_COLUMN (last_column->data)->visible);
       last_column = last_column->prev)
    ;

  /* and the first */
  for (first_column = g_list_first (tree_view->priv->columns);
       first_column && !(GTK_TREE_VIEW_COLUMN (first_column->data)->visible);
       first_column = first_column->next)
    ;

  /* Actually process the expose event.  To do this, we want to
   * start at the first node of the event, and walk the tree in
   * order, drawing each successive node.
   */

  do
    {
      gboolean parity;
      gboolean is_separator = FALSE;
      gboolean is_first = FALSE;
      gboolean is_last = FALSE;
      
      is_separator = row_is_separator (tree_view, &iter, NULL);

      max_height = ROW_HEIGHT (tree_view, BACKGROUND_HEIGHT (node));

      cell_offset = 0;
      highlight_x = 0; /* should match x coord of first cell */
      expander_cell_width = 0;

      background_area.y = y_offset + event->area.y;
      background_area.height = max_height;

      flags = 0;

      if (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_PRELIT))
	flags |= GTK_CELL_RENDERER_PRELIT;

      if (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_SELECTED))
        flags |= GTK_CELL_RENDERER_SELECTED;

      parity = _gtk_rbtree_node_find_parity (tree, node);

      /* we *need* to set cell data on all cells before the call
       * to _has_special_cell, else _has_special_cell() does not
       * return a correct value.
       */
      for (list = (rtl ? g_list_last (tree_view->priv->columns) : g_list_first (tree_view->priv->columns));
	   list;
	   list = (rtl ? list->prev : list->next))
        {
	  GtkTreeViewColumn *column = list->data;
	  gtk_tree_view_column_cell_set_cell_data (column,
						   tree_view->priv->model,
						   &iter,
						   GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_PARENT),
						   node->children?TRUE:FALSE);
        }

      has_special_cell = gtk_tree_view_has_special_cell (tree_view);

      for (list = (rtl ? g_list_last (tree_view->priv->columns) : g_list_first (tree_view->priv->columns));
	   list;
	   list = (rtl ? list->prev : list->next))
	{
	  GtkTreeViewColumn *column = list->data;
	  const gchar *detail = NULL;
	  GtkStateType state;

	  if (!column->visible)
            continue;

	  if (cell_offset > event->area.x + event->area.width ||
	      cell_offset + column->width < event->area.x)
	    {
	      cell_offset += column->width;
	      continue;
	    }

          if (column->show_sort_indicator)
	    flags |= GTK_CELL_RENDERER_SORTED;
          else
            flags &= ~GTK_CELL_RENDERER_SORTED;

	  if (cursor == node)
            flags |= GTK_CELL_RENDERER_FOCUSED;
          else
            flags &= ~GTK_CELL_RENDERER_FOCUSED;

	  background_area.x = cell_offset;
	  background_area.width = column->width;

          cell_area = background_area;
          cell_area.y += vertical_separator / 2;
          cell_area.x += horizontal_separator / 2;
          cell_area.height -= vertical_separator;
	  cell_area.width -= horizontal_separator;

	  if (draw_vgrid_lines)
	    {
	      if (list == first_column)
	        {
		  cell_area.width -= grid_line_width / 2;
		}
	      else if (list == last_column)
	        {
		  cell_area.x += grid_line_width / 2;
		  cell_area.width -= grid_line_width / 2;
		}
	      else
	        {
	          cell_area.x += grid_line_width / 2;
	          cell_area.width -= grid_line_width;
		}
	    }

	  if (draw_hgrid_lines)
	    {
	      cell_area.y += grid_line_width / 2;
	      cell_area.height -= grid_line_width;
	    }

	  if (gdk_region_rect_in (event->region, &background_area) == GDK_OVERLAP_RECTANGLE_OUT)
	    {
	      cell_offset += column->width;
	      continue;
	    }

	  gtk_tree_view_column_cell_set_cell_data (column,
						   tree_view->priv->model,
						   &iter,
						   GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_PARENT),
						   node->children?TRUE:FALSE);

          /* Select the detail for drawing the cell.  relevant
           * factors are parity, sortedness, and whether to
           * display rules.
           */
          if (allow_rules && tree_view->priv->has_rules)
            {
              if ((flags & GTK_CELL_RENDERER_SORTED) &&
		  n_visible_columns >= 3)
                {
                  if (parity)
                    detail = "cell_odd_ruled_sorted";
                  else
                    detail = "cell_even_ruled_sorted";
                }
              else
                {
                  if (parity)
                    detail = "cell_odd_ruled";
                  else
                    detail = "cell_even_ruled";
                }
            }
          else
            {
              if ((flags & GTK_CELL_RENDERER_SORTED) &&
		  n_visible_columns >= 3)
                {
                  if (parity)
                    detail = "cell_odd_sorted";
                  else
                    detail = "cell_even_sorted";
                }
              else
                {
                  if (parity)
                    detail = "cell_odd";
                  else
                    detail = "cell_even";
                }
            }

          g_assert (detail);

	  if (widget->state == GTK_STATE_INSENSITIVE)
	    state = GTK_STATE_INSENSITIVE;	    
	  else if (flags & GTK_CELL_RENDERER_SELECTED)
	    state = GTK_STATE_SELECTED;
	  else
	    state = GTK_STATE_NORMAL;

	  /* Draw background */
	  if (row_ending_details)
	    {
	      char new_detail[128];

	      is_first = (rtl ? !list->next : !list->prev);
	      is_last = (rtl ? !list->prev : !list->next);

	      /* (I don't like the snprintfs either, but couldn't find a
	       * less messy way).
	       */
	      if (is_first && is_last)
		g_snprintf (new_detail, 127, "%s", detail);
	      else if (is_first)
		g_snprintf (new_detail, 127, "%s_start", detail);
	      else if (is_last)
		g_snprintf (new_detail, 127, "%s_end", detail);
	      else
		g_snprintf (new_detail, 128, "%s_middle", detail);

	      gtk_paint_flat_box (widget->style,
				  event->window,
				  state,
				  GTK_SHADOW_NONE,
				  &event->area,
				  widget,
				  new_detail,
				  background_area.x,
				  background_area.y,
				  background_area.width,
				  background_area.height);
	    }
	  else
	    {
	      gtk_paint_flat_box (widget->style,
				  event->window,
				  state,
				  GTK_SHADOW_NONE,
				  &event->area,
				  widget,
				  detail,
				  background_area.x,
				  background_area.y,
				  background_area.width,
				  background_area.height);
	    }

	  if (gtk_tree_view_is_expander_column (tree_view, column))
	    {
	      if (!rtl)
		cell_area.x += (depth - 1) * tree_view->priv->level_indentation;
	      cell_area.width -= (depth - 1) * tree_view->priv->level_indentation;

              if (TREE_VIEW_DRAW_EXPANDERS(tree_view))
	        {
	          if (!rtl)
		    cell_area.x += depth * tree_view->priv->expander_size;
		  cell_area.width -= depth * tree_view->priv->expander_size;
		}

              /* If we have an expander column, the highlight underline
               * starts with that column, so that it indicates which
               * level of the tree we're dropping at.
               */
              highlight_x = cell_area.x;
	      expander_cell_width = cell_area.width;

	      if (is_separator)
		gtk_paint_hline (widget->style,
				 event->window,
				 state,
				 &cell_area,
				 widget,
				 NULL,
				 cell_area.x,
				 cell_area.x + cell_area.width,
				 cell_area.y + cell_area.height / 2);
	      else
		_gtk_tree_view_column_cell_render (column,
						   event->window,
						   &background_area,
						   &cell_area,
						   &event->area,
						   flags);
	      if (TREE_VIEW_DRAW_EXPANDERS(tree_view)
		  && (node->flags & GTK_RBNODE_IS_PARENT) == GTK_RBNODE_IS_PARENT)
		{
		  if (!got_pointer)
		    {
		      gdk_window_get_pointer (tree_view->priv->bin_window, 
					      &pointer_x, &pointer_y, NULL);
		      got_pointer = TRUE;
		    }

		  gtk_tree_view_draw_arrow (GTK_TREE_VIEW (widget),
					    tree,
					    node,
					    pointer_x, pointer_y);
		}
	    }
	  else
	    {
	      if (is_separator)
		gtk_paint_hline (widget->style,
				 event->window,
				 state,
				 &cell_area,
				 widget,
				 NULL,
				 cell_area.x,
				 cell_area.x + cell_area.width,
				 cell_area.y + cell_area.height / 2);
	      else
		_gtk_tree_view_column_cell_render (column,
						   event->window,
						   &background_area,
						   &cell_area,
						   &event->area,
						   flags);
	    }

	  if (draw_hgrid_lines)
	    {
	      if (background_area.y > 0)
                gtk_tree_view_draw_line (tree_view, event->window,
                                         GTK_TREE_VIEW_GRID_LINE,
                                         background_area.x, background_area.y,
                                         background_area.x + background_area.width,
			                 background_area.y);

	      if (y_offset + max_height >= event->area.height)
                gtk_tree_view_draw_line (tree_view, event->window,
                                         GTK_TREE_VIEW_GRID_LINE,
                                         background_area.x, background_area.y + max_height,
                                         background_area.x + background_area.width,
			                 background_area.y + max_height);
	    }

	  if (gtk_tree_view_is_expander_column (tree_view, column) &&
	      tree_view->priv->tree_lines_enabled)
	    {
	      gint x = background_area.x;
	      gint mult = rtl ? -1 : 1;
	      gint y0 = background_area.y;
	      gint y1 = background_area.y + background_area.height/2;
	      gint y2 = background_area.y + background_area.height;

	      if (rtl)
		x += background_area.width - 1;

	      if ((node->flags & GTK_RBNODE_IS_PARENT) == GTK_RBNODE_IS_PARENT
		  && depth > 1)
	        {
                  gtk_tree_view_draw_line (tree_view, event->window,
                                           GTK_TREE_VIEW_TREE_LINE,
                                           x + tree_view->priv->expander_size * (depth - 1.5) * mult,
                                           y1,
                                           x + tree_view->priv->expander_size * (depth - 1.1) * mult,
                                           y1);
	        }
	      else if (depth > 1)
	        {
                  gtk_tree_view_draw_line (tree_view, event->window,
                                           GTK_TREE_VIEW_TREE_LINE,
                                           x + tree_view->priv->expander_size * (depth - 1.5) * mult,
                                           y1,
                                           x + tree_view->priv->expander_size * (depth - 0.5) * mult,
                                           y1);
		}

	      if (depth > 1)
	        {
		  gint i;
		  GtkRBNode *tmp_node;
		  GtkRBTree *tmp_tree;

	          if (!_gtk_rbtree_next (tree, node))
                    gtk_tree_view_draw_line (tree_view, event->window,
                                             GTK_TREE_VIEW_TREE_LINE,
                                             x + tree_view->priv->expander_size * (depth - 1.5) * mult,
                                             y0,
                                             x + tree_view->priv->expander_size * (depth - 1.5) * mult,
                                             y1);
		  else
                    gtk_tree_view_draw_line (tree_view, event->window,
                                             GTK_TREE_VIEW_TREE_LINE,
                                             x + tree_view->priv->expander_size * (depth - 1.5) * mult,
                                             y0,
                                             x + tree_view->priv->expander_size * (depth - 1.5) * mult,
                                             y2);

		  tmp_node = tree->parent_node;
		  tmp_tree = tree->parent_tree;

		  for (i = depth - 2; i > 0; i--)
		    {
	              if (_gtk_rbtree_next (tmp_tree, tmp_node))
                        gtk_tree_view_draw_line (tree_view, event->window,
                                                 GTK_TREE_VIEW_TREE_LINE,
                                                 x + tree_view->priv->expander_size * (i - 0.5) * mult,
                                                 y0,
                                                 x + tree_view->priv->expander_size * (i - 0.5) * mult,
                                                 y2);

		      tmp_node = tmp_tree->parent_node;
		      tmp_tree = tmp_tree->parent_tree;
		    }
		}
	    }

	  if (node == cursor && has_special_cell &&
	      ((column == tree_view->priv->focus_column &&
		GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_DRAW_KEYFOCUS) &&
		gtk_widget_has_focus (widget)) ||
	       (column == tree_view->priv->edited_column)))
	    {
	      _gtk_tree_view_column_cell_draw_focus (column,
						     event->window,
						     &background_area,
						     &cell_area,
						     &event->area,
						     flags);
	    }

	  cell_offset += column->width;
	}

      if (node == drag_highlight)
        {
          /* Draw indicator for the drop
           */
          gint highlight_y = -1;
	  GtkRBTree *tree = NULL;
	  GtkRBNode *node = NULL;
	  gint width;

          switch (tree_view->priv->drag_dest_pos)
            {
            case GTK_TREE_VIEW_DROP_BEFORE:
              highlight_y = background_area.y - 1;
	      if (highlight_y < 0)
		      highlight_y = 0;
              break;

            case GTK_TREE_VIEW_DROP_AFTER:
              highlight_y = background_area.y + background_area.height - 1;
              break;

            case GTK_TREE_VIEW_DROP_INTO_OR_BEFORE:
            case GTK_TREE_VIEW_DROP_INTO_OR_AFTER:
	      _gtk_tree_view_find_node (tree_view, drag_dest_path, &tree, &node);

	      if (tree == NULL)
		break;
              width = gdk_window_get_width (tree_view->priv->bin_window);

	      if (row_ending_details)
		gtk_paint_focus (widget->style,
			         tree_view->priv->bin_window,
				 gtk_widget_get_state (widget),
				 &event->area,
				 widget,
				 (is_first
				  ? (is_last ? "treeview-drop-indicator" : "treeview-drop-indicator-left" )
				  : (is_last ? "treeview-drop-indicator-right" : "tree-view-drop-indicator-middle" )),
				 0, BACKGROUND_FIRST_PIXEL (tree_view, tree, node)
				 - focus_line_width / 2,
				 width, ROW_HEIGHT (tree_view, BACKGROUND_HEIGHT (node))
			       - focus_line_width + 1);
	      else
		gtk_paint_focus (widget->style,
			         tree_view->priv->bin_window,
				 gtk_widget_get_state (widget),
				 &event->area,
				 widget,
				 "treeview-drop-indicator",
				 0, BACKGROUND_FIRST_PIXEL (tree_view, tree, node)
				 - focus_line_width / 2,
				 width, ROW_HEIGHT (tree_view, BACKGROUND_HEIGHT (node))
				 - focus_line_width + 1);
              break;
            }

          if (highlight_y >= 0)
            {
              gtk_tree_view_draw_line (tree_view, event->window,
                                       GTK_TREE_VIEW_FOREGROUND_LINE,
                                       rtl ? highlight_x + expander_cell_width : highlight_x,
                                       highlight_y,
                                       rtl ? 0 : bin_window_width,
                                       highlight_y);
            }
        }

      /* draw the big row-spanning focus rectangle, if needed */
      if (!has_special_cell && node == cursor &&
	  GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_DRAW_KEYFOCUS) &&
	  gtk_widget_has_focus (widget))
        {
	  gint tmp_y, tmp_height;
	  gint width;
	  GtkStateType focus_rect_state;

	  focus_rect_state =
	    flags & GTK_CELL_RENDERER_SELECTED ? GTK_STATE_SELECTED :
	    (flags & GTK_CELL_RENDERER_PRELIT ? GTK_STATE_PRELIGHT :
	     (flags & GTK_CELL_RENDERER_INSENSITIVE ? GTK_STATE_INSENSITIVE :
	      GTK_STATE_NORMAL));

          width = gdk_window_get_width (tree_view->priv->bin_window);
	  
	  if (draw_hgrid_lines)
	    {
	      tmp_y = BACKGROUND_FIRST_PIXEL (tree_view, tree, node) + grid_line_width / 2;
	      tmp_height = ROW_HEIGHT (tree_view, BACKGROUND_HEIGHT (node)) - grid_line_width;
	    }
	  else
	    {
	      tmp_y = BACKGROUND_FIRST_PIXEL (tree_view, tree, node);
	      tmp_height = ROW_HEIGHT (tree_view, BACKGROUND_HEIGHT (node));
	    }

	  if (row_ending_details)
	    gtk_paint_focus (widget->style,
			     tree_view->priv->bin_window,
			     focus_rect_state,
			     &event->area,
			     widget,
			     (is_first
			      ? (is_last ? "treeview" : "treeview-left" )
			      : (is_last ? "treeview-right" : "treeview-middle" )),
			     0, tmp_y,
			     width, tmp_height);
	  else
	    gtk_paint_focus (widget->style,
			     tree_view->priv->bin_window,
			     focus_rect_state,
			     &event->area,
			     widget,
			     "treeview",
			     0, tmp_y,
			     width, tmp_height);
	}

      y_offset += max_height;
      if (node->children)
	{
	  GtkTreeIter parent = iter;
	  gboolean has_child;

	  tree = node->children;
	  node = tree->root;

          g_assert (node != tree->nil);

	  while (node->left != tree->nil)
	    node = node->left;
	  has_child = gtk_tree_model_iter_children (tree_view->priv->model,
						    &iter,
						    &parent);
	  depth++;

	  /* Sanity Check! */
	  TREE_VIEW_INTERNAL_ASSERT (has_child, FALSE);
	}
      else
	{
	  gboolean done = FALSE;

	  do
	    {
	      node = _gtk_rbtree_next (tree, node);
	      if (node != NULL)
		{
		  gboolean has_next = gtk_tree_model_iter_next (tree_view->priv->model, &iter);
		  done = TRUE;

		  /* Sanity Check! */
		  TREE_VIEW_INTERNAL_ASSERT (has_next, FALSE);
		}
	      else
		{
		  GtkTreeIter parent_iter = iter;
		  gboolean has_parent;

		  node = tree->parent_node;
		  tree = tree->parent_tree;
		  if (tree == NULL)
		    /* we should go to done to free some memory */
		    goto done;
		  has_parent = gtk_tree_model_iter_parent (tree_view->priv->model,
							   &iter,
							   &parent_iter);
		  depth--;

		  /* Sanity check */
		  TREE_VIEW_INTERNAL_ASSERT (has_parent, FALSE);
		}
	    }
	  while (!done);
	}
    }
  while (y_offset < event->area.height);

done:
  gtk_tree_view_draw_grid_lines (tree_view, event, n_visible_columns);

 if (tree_view->priv->rubber_band_status == RUBBER_BAND_ACTIVE)
   {
     GdkRectangle *rectangles;
     gint n_rectangles;

     gdk_region_get_rectangles (event->region,
				&rectangles,
				&n_rectangles);

     while (n_rectangles--)
       gtk_tree_view_paint_rubber_band (tree_view, &rectangles[n_rectangles]);

     g_free (rectangles);
   }

  if (cursor_path)
    gtk_tree_path_free (cursor_path);

  if (drag_dest_path)
    gtk_tree_path_free (drag_dest_path);

  return FALSE;
}

static gboolean
gtk_tree_view_expose (GtkWidget      *widget,
		      GdkEventExpose *event)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (widget);

  if (event->window == tree_view->priv->bin_window)
    {
      gboolean retval;
      GList *tmp_list;

      retval = gtk_tree_view_bin_expose (widget, event);

      /* We can't just chain up to Container::expose as it will try to send the
       * event to the headers, so we handle propagating it to our children
       * (eg. widgets being edited) ourselves.
       */
      tmp_list = tree_view->priv->children;
      while (tmp_list)
	{
	  GtkTreeViewChild *child = tmp_list->data;
	  tmp_list = tmp_list->next;

	  gtk_container_propagate_expose (GTK_CONTAINER (tree_view), child->widget, event);
	}

      return retval;
    }

  else if (event->window == tree_view->priv->header_window)
    {
      GList *list;
      
      for (list = tree_view->priv->columns; list != NULL; list = list->next)
	{
	  GtkTreeViewColumn *column = list->data;

	  if (column == tree_view->priv->drag_column)
	    continue;

	  if (column->visible)
	    gtk_container_propagate_expose (GTK_CONTAINER (tree_view),
					    column->button,
					    event);
	}
    }
  else if (event->window == tree_view->priv->drag_window)
    {
      gtk_container_propagate_expose (GTK_CONTAINER (tree_view),
				      tree_view->priv->drag_column->button,
				      event);
    }
  return TRUE;
}

enum
{
  DROP_HOME,
  DROP_RIGHT,
  DROP_LEFT,
  DROP_END
};

/* returns 0x1 when no column has been found -- yes it's hackish */
static GtkTreeViewColumn *
gtk_tree_view_get_drop_column (GtkTreeView       *tree_view,
			       GtkTreeViewColumn *column,
			       gint               drop_position)
{
  GtkTreeViewColumn *left_column = NULL;
  GtkTreeViewColumn *cur_column = NULL;
  GList *tmp_list;

  if (!column->reorderable)
    return (GtkTreeViewColumn *)0x1;

  switch (drop_position)
    {
      case DROP_HOME:
	/* find first column where we can drop */
	tmp_list = tree_view->priv->columns;
	if (column == GTK_TREE_VIEW_COLUMN (tmp_list->data))
	  return (GtkTreeViewColumn *)0x1;

	while (tmp_list)
	  {
	    g_assert (tmp_list);

	    cur_column = GTK_TREE_VIEW_COLUMN (tmp_list->data);
	    tmp_list = tmp_list->next;

	    if (left_column && left_column->visible == FALSE)
	      continue;

	    if (!tree_view->priv->column_drop_func)
	      return left_column;

	    if (!tree_view->priv->column_drop_func (tree_view, column, left_column, cur_column, tree_view->priv->column_drop_func_data))
	      {
		left_column = cur_column;
		continue;
	      }

	    return left_column;
	  }

	if (!tree_view->priv->column_drop_func)
	  return left_column;

	if (tree_view->priv->column_drop_func (tree_view, column, left_column, NULL, tree_view->priv->column_drop_func_data))
	  return left_column;
	else
	  return (GtkTreeViewColumn *)0x1;
	break;

      case DROP_RIGHT:
	/* find first column after column where we can drop */
	tmp_list = tree_view->priv->columns;

	for (; tmp_list; tmp_list = tmp_list->next)
	  if (GTK_TREE_VIEW_COLUMN (tmp_list->data) == column)
	    break;

	if (!tmp_list || !tmp_list->next)
	  return (GtkTreeViewColumn *)0x1;

	tmp_list = tmp_list->next;
	left_column = GTK_TREE_VIEW_COLUMN (tmp_list->data);
	tmp_list = tmp_list->next;

	while (tmp_list)
	  {
	    g_assert (tmp_list);

	    cur_column = GTK_TREE_VIEW_COLUMN (tmp_list->data);
	    tmp_list = tmp_list->next;

	    if (left_column && left_column->visible == FALSE)
	      {
		left_column = cur_column;
		if (tmp_list)
		  tmp_list = tmp_list->next;
	        continue;
	      }

	    if (!tree_view->priv->column_drop_func)
	      return left_column;

	    if (!tree_view->priv->column_drop_func (tree_view, column, left_column, cur_column, tree_view->priv->column_drop_func_data))
	      {
		left_column = cur_column;
		continue;
	      }

	    return left_column;
	  }

	if (!tree_view->priv->column_drop_func)
	  return left_column;

	if (tree_view->priv->column_drop_func (tree_view, column, left_column, NULL, tree_view->priv->column_drop_func_data))
	  return left_column;
	else
	  return (GtkTreeViewColumn *)0x1;
	break;

      case DROP_LEFT:
	/* find first column before column where we can drop */
	tmp_list = tree_view->priv->columns;

	for (; tmp_list; tmp_list = tmp_list->next)
	  if (GTK_TREE_VIEW_COLUMN (tmp_list->data) == column)
	    break;

	if (!tmp_list || !tmp_list->prev)
	  return (GtkTreeViewColumn *)0x1;

	tmp_list = tmp_list->prev;
	cur_column = GTK_TREE_VIEW_COLUMN (tmp_list->data);
	tmp_list = tmp_list->prev;

	while (tmp_list)
	  {
	    g_assert (tmp_list);

	    left_column = GTK_TREE_VIEW_COLUMN (tmp_list->data);

	    if (left_column && !left_column->visible)
	      {
		/*if (!tmp_list->prev)
		  return (GtkTreeViewColumn *)0x1;
		  */
/*
		cur_column = GTK_TREE_VIEW_COLUMN (tmp_list->prev->data);
		tmp_list = tmp_list->prev->prev;
		continue;*/

		cur_column = left_column;
		if (tmp_list)
		  tmp_list = tmp_list->prev;
		continue;
	      }

	    if (!tree_view->priv->column_drop_func)
	      return left_column;

	    if (tree_view->priv->column_drop_func (tree_view, column, left_column, cur_column, tree_view->priv->column_drop_func_data))
	      return left_column;

	    cur_column = left_column;
	    tmp_list = tmp_list->prev;
	  }

	if (!tree_view->priv->column_drop_func)
	  return NULL;

	if (tree_view->priv->column_drop_func (tree_view, column, NULL, cur_column, tree_view->priv->column_drop_func_data))
	  return NULL;
	else
	  return (GtkTreeViewColumn *)0x1;
	break;

      case DROP_END:
	/* same as DROP_HOME case, but doing it backwards */
	tmp_list = g_list_last (tree_view->priv->columns);
	cur_column = NULL;

	if (column == GTK_TREE_VIEW_COLUMN (tmp_list->data))
	  return (GtkTreeViewColumn *)0x1;

	while (tmp_list)
	  {
	    g_assert (tmp_list);

	    left_column = GTK_TREE_VIEW_COLUMN (tmp_list->data);

	    if (left_column && !left_column->visible)
	      {
		cur_column = left_column;
		tmp_list = tmp_list->prev;
	      }

	    if (!tree_view->priv->column_drop_func)
	      return left_column;

	    if (tree_view->priv->column_drop_func (tree_view, column, left_column, cur_column, tree_view->priv->column_drop_func_data))
	      return left_column;

	    cur_column = left_column;
	    tmp_list = tmp_list->prev;
	  }

	if (!tree_view->priv->column_drop_func)
	  return NULL;

	if (tree_view->priv->column_drop_func (tree_view, column, NULL, cur_column, tree_view->priv->column_drop_func_data))
	  return NULL;
	else
	  return (GtkTreeViewColumn *)0x1;
	break;
    }

  return (GtkTreeViewColumn *)0x1;
}

static gboolean
gtk_tree_view_key_press (GtkWidget   *widget,
			 GdkEventKey *event)
{
  GtkTreeView *tree_view = (GtkTreeView *) widget;

  if (tree_view->priv->rubber_band_status)
    {
      if (event->keyval == GDK_Escape)
	gtk_tree_view_stop_rubber_band (tree_view);

      return TRUE;
    }

  if (GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_IN_COLUMN_DRAG))
    {
      if (event->keyval == GDK_Escape)
	{
	  tree_view->priv->cur_reorder = NULL;
	  gtk_tree_view_button_release_drag_column (widget, NULL);
	}
      return TRUE;
    }

  if (GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_HEADERS_VISIBLE))
    {
      GList *focus_column;
      gboolean rtl;

      rtl = (gtk_widget_get_direction (GTK_WIDGET (tree_view)) == GTK_TEXT_DIR_RTL);

      for (focus_column = tree_view->priv->columns;
           focus_column;
           focus_column = focus_column->next)
        {
          GtkTreeViewColumn *column = GTK_TREE_VIEW_COLUMN (focus_column->data);

          if (gtk_widget_has_focus (column->button))
            break;
        }

      if (focus_column &&
          (event->state & GDK_SHIFT_MASK) && (event->state & GDK_MOD1_MASK) &&
          (event->keyval == GDK_Left || event->keyval == GDK_KP_Left
           || event->keyval == GDK_Right || event->keyval == GDK_KP_Right))
        {
          GtkTreeViewColumn *column = GTK_TREE_VIEW_COLUMN (focus_column->data);

          if (!column->resizable)
            {
              gtk_widget_error_bell (widget);
              return TRUE;
            }

          if (event->keyval == (rtl ? GDK_Right : GDK_Left)
              || event->keyval == (rtl ? GDK_KP_Right : GDK_KP_Left))
            {
              gint old_width = column->resized_width;

              column->resized_width = MAX (column->resized_width,
                                           column->width);
              column->resized_width -= 2;
              if (column->resized_width < 0)
                column->resized_width = 0;

              if (column->min_width == -1)
                column->resized_width = MAX (column->button->requisition.width,
                                             column->resized_width);
              else
                column->resized_width = MAX (column->min_width,
                                             column->resized_width);

              if (column->max_width != -1)
                column->resized_width = MIN (column->resized_width,
                                             column->max_width);

              column->use_resized_width = TRUE;

              if (column->resized_width != old_width)
                gtk_widget_queue_resize (widget);
              else
                gtk_widget_error_bell (widget);
            }
          else if (event->keyval == (rtl ? GDK_Left : GDK_Right)
                   || event->keyval == (rtl ? GDK_KP_Left : GDK_KP_Right))
            {
              gint old_width = column->resized_width;

              column->resized_width = MAX (column->resized_width,
                                           column->width);
              column->resized_width += 2;

              if (column->max_width != -1)
                column->resized_width = MIN (column->resized_width,
                                             column->max_width);

              column->use_resized_width = TRUE;

              if (column->resized_width != old_width)
                gtk_widget_queue_resize (widget);
              else
                gtk_widget_error_bell (widget);
            }

          return TRUE;
        }

      if (focus_column &&
          (event->state & GDK_MOD1_MASK) &&
          (event->keyval == GDK_Left || event->keyval == GDK_KP_Left
           || event->keyval == GDK_Right || event->keyval == GDK_KP_Right
           || event->keyval == GDK_Home || event->keyval == GDK_KP_Home
           || event->keyval == GDK_End || event->keyval == GDK_KP_End))
        {
          GtkTreeViewColumn *column = GTK_TREE_VIEW_COLUMN (focus_column->data);

          if (event->keyval == (rtl ? GDK_Right : GDK_Left)
              || event->keyval == (rtl ? GDK_KP_Right : GDK_KP_Left))
            {
              GtkTreeViewColumn *col;
              col = gtk_tree_view_get_drop_column (tree_view, column, DROP_LEFT);
              if (col != (GtkTreeViewColumn *)0x1)
                gtk_tree_view_move_column_after (tree_view, column, col);
              else
                gtk_widget_error_bell (widget);
            }
          else if (event->keyval == (rtl ? GDK_Left : GDK_Right)
                   || event->keyval == (rtl ? GDK_KP_Left : GDK_KP_Right))
            {
              GtkTreeViewColumn *col;
              col = gtk_tree_view_get_drop_column (tree_view, column, DROP_RIGHT);
              if (col != (GtkTreeViewColumn *)0x1)
                gtk_tree_view_move_column_after (tree_view, column, col);
              else
                gtk_widget_error_bell (widget);
            }
          else if (event->keyval == GDK_Home || event->keyval == GDK_KP_Home)
            {
              GtkTreeViewColumn *col;
              col = gtk_tree_view_get_drop_column (tree_view, column, DROP_HOME);
              if (col != (GtkTreeViewColumn *)0x1)
                gtk_tree_view_move_column_after (tree_view, column, col);
              else
                gtk_widget_error_bell (widget);
            }
          else if (event->keyval == GDK_End || event->keyval == GDK_KP_End)
            {
              GtkTreeViewColumn *col;
              col = gtk_tree_view_get_drop_column (tree_view, column, DROP_END);
              if (col != (GtkTreeViewColumn *)0x1)
                gtk_tree_view_move_column_after (tree_view, column, col);
              else
                gtk_widget_error_bell (widget);
            }

          return TRUE;
        }
    }

  /* Chain up to the parent class.  It handles the keybindings. */
  if (GTK_WIDGET_CLASS (gtk_tree_view_parent_class)->key_press_event (widget, event))
    return TRUE;

  if (tree_view->priv->search_entry_avoid_unhandled_binding)
    {
      tree_view->priv->search_entry_avoid_unhandled_binding = FALSE;
      return FALSE;
    }

  /* We pass the event to the search_entry.  If its text changes, then we start
   * the typeahead find capabilities. */
  if (gtk_widget_has_focus (GTK_WIDGET (tree_view))
      && tree_view->priv->enable_search
      && !tree_view->priv->search_custom_entry_set)
    {
      GdkEvent *new_event;
      char *old_text;
      const char *new_text;
      gboolean retval;
      GdkScreen *screen;
      gboolean text_modified;
      gulong popup_menu_id;

      gtk_tree_view_ensure_interactive_directory (tree_view);

      /* Make a copy of the current text */
      old_text = g_strdup (gtk_entry_get_text (GTK_ENTRY (tree_view->priv->search_entry)));
      new_event = gdk_event_copy ((GdkEvent *) event);
      g_object_unref (((GdkEventKey *) new_event)->window);
      ((GdkEventKey *) new_event)->window = g_object_ref (tree_view->priv->search_window->window);
      gtk_widget_realize (tree_view->priv->search_window);

      popup_menu_id = g_signal_connect (tree_view->priv->search_entry, 
					"popup-menu", G_CALLBACK (gtk_true),
                                        NULL);

      /* Move the entry off screen */
      screen = gtk_widget_get_screen (GTK_WIDGET (tree_view));
      gtk_window_move (GTK_WINDOW (tree_view->priv->search_window),
		       gdk_screen_get_width (screen) + 1,
		       gdk_screen_get_height (screen) + 1);
      gtk_widget_show (tree_view->priv->search_window);

      /* Send the event to the window.  If the preedit_changed signal is emitted
       * during this event, we will set priv->imcontext_changed  */
      tree_view->priv->imcontext_changed = FALSE;
      retval = gtk_widget_event (tree_view->priv->search_window, new_event);
      gdk_event_free (new_event);
      gtk_widget_hide (tree_view->priv->search_window);

      g_signal_handler_disconnect (tree_view->priv->search_entry, 
				   popup_menu_id);

      /* We check to make sure that the entry tried to handle the text, and that
       * the text has changed.
       */
      new_text = gtk_entry_get_text (GTK_ENTRY (tree_view->priv->search_entry));
      text_modified = strcmp (old_text, new_text) != 0;
      g_free (old_text);
      if (tree_view->priv->imcontext_changed ||    /* we're in a preedit */
	  (retval && text_modified))               /* ...or the text was modified */
	{
	  if (gtk_tree_view_real_start_interactive_search (tree_view, FALSE))
	    {
	      gtk_widget_grab_focus (GTK_WIDGET (tree_view));
	      return TRUE;
	    }
	  else
	    {
	      gtk_entry_set_text (GTK_ENTRY (tree_view->priv->search_entry), "");
	      return FALSE;
	    }
	}
    }

  return FALSE;
}

static gboolean
gtk_tree_view_key_release (GtkWidget   *widget,
			   GdkEventKey *event)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (widget);

  if (tree_view->priv->rubber_band_status)
    return TRUE;

  return GTK_WIDGET_CLASS (gtk_tree_view_parent_class)->key_release_event (widget, event);
}

/* FIXME Is this function necessary? Can I get an enter_notify event
 * w/o either an expose event or a mouse motion event?
 */
static gboolean
gtk_tree_view_enter_notify (GtkWidget        *widget,
			    GdkEventCrossing *event)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (widget);
  GtkRBTree *tree;
  GtkRBNode *node;
  gint new_y;

  /* Sanity check it */
  if (event->window != tree_view->priv->bin_window)
    return FALSE;

  if (tree_view->priv->tree == NULL)
    return FALSE;

  if (event->mode == GDK_CROSSING_GRAB ||
      event->mode == GDK_CROSSING_GTK_GRAB ||
      event->mode == GDK_CROSSING_GTK_UNGRAB ||
      event->mode == GDK_CROSSING_STATE_CHANGED)
    return TRUE;

  /* find the node internally */
  new_y = TREE_WINDOW_Y_TO_RBTREE_Y(tree_view, event->y);
  if (new_y < 0)
    new_y = 0;
  _gtk_rbtree_find_offset (tree_view->priv->tree, new_y, &tree, &node);

  tree_view->priv->event_last_x = event->x;
  tree_view->priv->event_last_y = event->y;

  if ((tree_view->priv->button_pressed_node == NULL) ||
      (tree_view->priv->button_pressed_node == node))
    prelight_or_select (tree_view, tree, node, event->x, event->y);

  return TRUE;
}

static gboolean
gtk_tree_view_leave_notify (GtkWidget        *widget,
			    GdkEventCrossing *event)
{
  GtkTreeView *tree_view;

  if (event->mode == GDK_CROSSING_GRAB)
    return TRUE;

  tree_view = GTK_TREE_VIEW (widget);

  if (tree_view->priv->prelight_node)
    _gtk_tree_view_queue_draw_node (tree_view,
                                   tree_view->priv->prelight_tree,
                                   tree_view->priv->prelight_node,
                                   NULL);

  tree_view->priv->event_last_x = -10000;
  tree_view->priv->event_last_y = -10000;

  prelight_or_select (tree_view,
		      NULL, NULL,
		      -1000, -1000); /* coords not possibly over an arrow */

  return TRUE;
}


static gint
gtk_tree_view_focus_out (GtkWidget     *widget,
			 GdkEventFocus *event)
{
  GtkTreeView *tree_view;

  tree_view = GTK_TREE_VIEW (widget);

  gtk_widget_queue_draw (widget);

  /* destroy interactive search dialog */
  if (tree_view->priv->search_window)
    gtk_tree_view_search_dialog_hide (tree_view->priv->search_window, tree_view);

  return FALSE;
}


/* Incremental Reflow
 */

static void
gtk_tree_view_node_queue_redraw (GtkTreeView *tree_view,
				 GtkRBTree   *tree,
				 GtkRBNode   *node)
{
  gint y;

  y = _gtk_rbtree_node_find_offset (tree, node)
    - tree_view->priv->vadjustment->value
    + TREE_VIEW_HEADER_HEIGHT (tree_view);

  gtk_widget_queue_draw_area (GTK_WIDGET (tree_view),
			      0, y,
			      GTK_WIDGET (tree_view)->allocation.width,
			      GTK_RBNODE_GET_HEIGHT (node));
}

static gboolean
node_is_visible (GtkTreeView *tree_view,
		 GtkRBTree   *tree,
		 GtkRBNode   *node)
{
  int y;
  int height;

  y = _gtk_rbtree_node_find_offset (tree, node);
  height = ROW_HEIGHT (tree_view, GTK_RBNODE_GET_HEIGHT (node));

  if (y >= tree_view->priv->vadjustment->value &&
      y + height <= (tree_view->priv->vadjustment->value
	             + tree_view->priv->vadjustment->page_size))
    return TRUE;

  return FALSE;
}

/* Returns TRUE if it updated the size
 */
static gboolean
validate_row (GtkTreeView *tree_view,
	      GtkRBTree   *tree,
	      GtkRBNode   *node,
	      GtkTreeIter *iter,
	      GtkTreePath *path)
{
  GtkTreeViewColumn *column;
  GList *list, *first_column, *last_column;
  gint height = 0;
  gint horizontal_separator;
  gint vertical_separator;
  gint focus_line_width;
  gint depth = gtk_tree_path_get_depth (path);
  gboolean retval = FALSE;
  gboolean is_separator = FALSE;
  gboolean draw_vgrid_lines, draw_hgrid_lines;
  gint focus_pad;
  gint grid_line_width;
  gboolean wide_separators;
  gint separator_height;

  /* double check the row needs validating */
  if (! GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_INVALID) &&
      ! GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_COLUMN_INVALID))
    return FALSE;

  is_separator = row_is_separator (tree_view, iter, NULL);

  gtk_widget_style_get (GTK_WIDGET (tree_view),
			"focus-padding", &focus_pad,
			"focus-line-width", &focus_line_width,
			"horizontal-separator", &horizontal_separator,
			"vertical-separator", &vertical_separator,
			"grid-line-width", &grid_line_width,
                        "wide-separators",  &wide_separators,
                        "separator-height", &separator_height,
			NULL);
  
  draw_vgrid_lines =
    tree_view->priv->grid_lines == GTK_TREE_VIEW_GRID_LINES_VERTICAL
    || tree_view->priv->grid_lines == GTK_TREE_VIEW_GRID_LINES_BOTH;
  draw_hgrid_lines =
    tree_view->priv->grid_lines == GTK_TREE_VIEW_GRID_LINES_HORIZONTAL
    || tree_view->priv->grid_lines == GTK_TREE_VIEW_GRID_LINES_BOTH;

  for (last_column = g_list_last (tree_view->priv->columns);
       last_column && !(GTK_TREE_VIEW_COLUMN (last_column->data)->visible);
       last_column = last_column->prev)
    ;

  for (first_column = g_list_first (tree_view->priv->columns);
       first_column && !(GTK_TREE_VIEW_COLUMN (first_column->data)->visible);
       first_column = first_column->next)
    ;

  for (list = tree_view->priv->columns; list; list = list->next)
    {
      gint tmp_width;
      gint tmp_height;

      column = list->data;

      if (! column->visible)
	continue;

      if (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_COLUMN_INVALID) && !column->dirty)
	continue;

      gtk_tree_view_column_cell_set_cell_data (column, tree_view->priv->model, iter,
					       GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_PARENT),
					       node->children?TRUE:FALSE);
      gtk_tree_view_column_cell_get_size (column,
					  NULL, NULL, NULL,
					  &tmp_width, &tmp_height);

      if (!is_separator)
	{
          tmp_height += vertical_separator;
	  height = MAX (height, tmp_height);
	  height = MAX (height, tree_view->priv->expander_size);
	}
      else
        {
          if (wide_separators)
            height = separator_height + 2 * focus_pad;
          else
            height = 2 + 2 * focus_pad;
        }

      if (gtk_tree_view_is_expander_column (tree_view, column))
        {
	  tmp_width = tmp_width + horizontal_separator + (depth - 1) * tree_view->priv->level_indentation;

	  if (TREE_VIEW_DRAW_EXPANDERS (tree_view))
	    tmp_width += depth * tree_view->priv->expander_size;
	}
      else
	tmp_width = tmp_width + horizontal_separator;

      if (draw_vgrid_lines)
        {
	  if (list->data == first_column || list->data == last_column)
	    tmp_width += grid_line_width / 2.0;
	  else
	    tmp_width += grid_line_width;
	}

      if (tmp_width > column->requested_width)
	{
	  retval = TRUE;
	  column->requested_width = tmp_width;
	}
    }

  if (draw_hgrid_lines)
    height += grid_line_width;

  if (height != GTK_RBNODE_GET_HEIGHT (node))
    {
      retval = TRUE;
      _gtk_rbtree_node_set_height (tree, node, height);
    }
  _gtk_rbtree_node_mark_valid (tree, node);
  tree_view->priv->post_validation_flag = TRUE;

  return retval;
}


static void
validate_visible_area (GtkTreeView *tree_view)
{
  GtkTreePath *path = NULL;
  GtkTreePath *above_path = NULL;
  GtkTreeIter iter;
  GtkRBTree *tree = NULL;
  GtkRBNode *node = NULL;
  gboolean need_redraw = FALSE;
  gboolean size_changed = FALSE;
  gint total_height;
  gint area_above = 0;
  gint area_below = 0;

  if (tree_view->priv->tree == NULL)
    return;

  if (! GTK_RBNODE_FLAG_SET (tree_view->priv->tree->root, GTK_RBNODE_DESCENDANTS_INVALID) &&
      tree_view->priv->scroll_to_path == NULL)
    return;

  total_height = GTK_WIDGET (tree_view)->allocation.height - TREE_VIEW_HEADER_HEIGHT (tree_view);

  if (total_height == 0)
    return;

  /* First, we check to see if we need to scroll anywhere
   */
  if (tree_view->priv->scroll_to_path)
    {
      path = gtk_tree_row_reference_get_path (tree_view->priv->scroll_to_path);
      if (path && !_gtk_tree_view_find_node (tree_view, path, &tree, &node))
	{
          /* we are going to scroll, and will update dy */
	  gtk_tree_model_get_iter (tree_view->priv->model, &iter, path);
	  if (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_INVALID) ||
	      GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_COLUMN_INVALID))
	    {
	      _gtk_tree_view_queue_draw_node (tree_view, tree, node, NULL);
	      if (validate_row (tree_view, tree, node, &iter, path))
		size_changed = TRUE;
	    }

	  if (tree_view->priv->scroll_to_use_align)
	    {
	      gint height = ROW_HEIGHT (tree_view, GTK_RBNODE_GET_HEIGHT (node));
	      area_above = (total_height - height) *
		tree_view->priv->scroll_to_row_align;
	      area_below = total_height - area_above - height;
	      area_above = MAX (area_above, 0);
	      area_below = MAX (area_below, 0);
	    }
	  else
	    {
	      /* two cases:
	       * 1) row not visible
	       * 2) row visible
	       */
	      gint dy;
	      gint height = ROW_HEIGHT (tree_view, GTK_RBNODE_GET_HEIGHT (node));

	      dy = _gtk_rbtree_node_find_offset (tree, node);

	      if (dy >= tree_view->priv->vadjustment->value &&
		  dy + height <= (tree_view->priv->vadjustment->value
		                  + tree_view->priv->vadjustment->page_size))
	        {
		  /* row visible: keep the row at the same position */
		  area_above = dy - tree_view->priv->vadjustment->value;
		  area_below = (tree_view->priv->vadjustment->value +
		                tree_view->priv->vadjustment->page_size)
		               - dy - height;
		}
	      else
	        {
		  /* row not visible */
		  if (dy >= 0
		      && dy + height <= tree_view->priv->vadjustment->page_size)
		    {
		      /* row at the beginning -- fixed */
		      area_above = dy;
		      area_below = tree_view->priv->vadjustment->page_size
				   - area_above - height;
		    }
		  else if (dy >= (tree_view->priv->vadjustment->upper -
			          tree_view->priv->vadjustment->page_size))
		    {
		      /* row at the end -- fixed */
		      area_above = dy - (tree_view->priv->vadjustment->upper -
			           tree_view->priv->vadjustment->page_size);
                      area_below = tree_view->priv->vadjustment->page_size -
                                   area_above - height;

                      if (area_below < 0)
                        {
			  area_above = tree_view->priv->vadjustment->page_size - height;
                          area_below = 0;
                        }
		    }
		  else
		    {
		      /* row somewhere in the middle, bring it to the top
		       * of the view
		       */
		      area_above = 0;
		      area_below = total_height - height;
		    }
		}
	    }
	}
      else
	/* the scroll to isn't valid; ignore it.
	 */
	{
	  if (tree_view->priv->scroll_to_path && !path)
	    {
	      gtk_tree_row_reference_free (tree_view->priv->scroll_to_path);
	      tree_view->priv->scroll_to_path = NULL;
	    }
	  if (path)
	    gtk_tree_path_free (path);
	  path = NULL;
	}      
    }

  /* We didn't have a scroll_to set, so we just handle things normally
   */
  if (path == NULL)
    {
      gint offset;

      offset = _gtk_rbtree_find_offset (tree_view->priv->tree,
					TREE_WINDOW_Y_TO_RBTREE_Y (tree_view, 0),
					&tree, &node);
      if (node == NULL)
	{
	  /* In this case, nothing has been validated */
	  path = gtk_tree_path_new_first ();
	  _gtk_tree_view_find_node (tree_view, path, &tree, &node);
	}
      else
	{
	  path = _gtk_tree_view_find_path (tree_view, tree, node);
	  total_height += offset;
	}

      gtk_tree_model_get_iter (tree_view->priv->model, &iter, path);

      if (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_INVALID) ||
	  GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_COLUMN_INVALID))
	{
	  _gtk_tree_view_queue_draw_node (tree_view, tree, node, NULL);
	  if (validate_row (tree_view, tree, node, &iter, path))
	    size_changed = TRUE;
	}
      area_above = 0;
      area_below = total_height - ROW_HEIGHT (tree_view, GTK_RBNODE_GET_HEIGHT (node));
    }

  above_path = gtk_tree_path_copy (path);

  /* if we do not validate any row above the new top_row, we will make sure
   * that the row immediately above top_row has been validated. (if we do not
   * do this, _gtk_rbtree_find_offset will find the row above top_row, because
   * when invalidated that row's height will be zero. and this will mess up
   * scrolling).
   */
  if (area_above == 0)
    {
      GtkRBTree *tmptree;
      GtkRBNode *tmpnode;

      _gtk_tree_view_find_node (tree_view, above_path, &tmptree, &tmpnode);
      _gtk_rbtree_prev_full (tmptree, tmpnode, &tmptree, &tmpnode);

      if (tmpnode)
        {
	  GtkTreePath *tmppath;
	  GtkTreeIter tmpiter;

	  tmppath = _gtk_tree_view_find_path (tree_view, tmptree, tmpnode);
	  gtk_tree_model_get_iter (tree_view->priv->model, &tmpiter, tmppath);

	  if (GTK_RBNODE_FLAG_SET (tmpnode, GTK_RBNODE_INVALID) ||
	      GTK_RBNODE_FLAG_SET (tmpnode, GTK_RBNODE_COLUMN_INVALID))
	    {
	      _gtk_tree_view_queue_draw_node (tree_view, tmptree, tmpnode, NULL);
	      if (validate_row (tree_view, tmptree, tmpnode, &tmpiter, tmppath))
		size_changed = TRUE;
	    }

	  gtk_tree_path_free (tmppath);
	}
    }

  /* Now, we walk forwards and backwards, measuring rows. Unfortunately,
   * backwards is much slower then forward, as there is no iter_prev function.
   * We go forwards first in case we run out of tree.  Then we go backwards to
   * fill out the top.
   */
  while (node && area_below > 0)
    {
      if (node->children)
	{
	  GtkTreeIter parent = iter;
	  gboolean has_child;

	  tree = node->children;
	  node = tree->root;

          g_assert (node != tree->nil);

	  while (node->left != tree->nil)
	    node = node->left;
	  has_child = gtk_tree_model_iter_children (tree_view->priv->model,
						    &iter,
						    &parent);
	  TREE_VIEW_INTERNAL_ASSERT_VOID (has_child);
	  gtk_tree_path_down (path);
	}
      else
	{
	  gboolean done = FALSE;
	  do
	    {
	      node = _gtk_rbtree_next (tree, node);
	      if (node != NULL)
		{
		  gboolean has_next = gtk_tree_model_iter_next (tree_view->priv->model, &iter);
		  done = TRUE;
		  gtk_tree_path_next (path);

		  /* Sanity Check! */
		  TREE_VIEW_INTERNAL_ASSERT_VOID (has_next);
		}
	      else
		{
		  GtkTreeIter parent_iter = iter;
		  gboolean has_parent;

		  node = tree->parent_node;
		  tree = tree->parent_tree;
		  if (tree == NULL)
		    break;
		  has_parent = gtk_tree_model_iter_parent (tree_view->priv->model,
							   &iter,
							   &parent_iter);
		  gtk_tree_path_up (path);

		  /* Sanity check */
		  TREE_VIEW_INTERNAL_ASSERT_VOID (has_parent);
		}
	    }
	  while (!done);
	}

      if (!node)
        break;

      if (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_INVALID) ||
	  GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_COLUMN_INVALID))
	{
	  _gtk_tree_view_queue_draw_node (tree_view, tree, node, NULL);
	  if (validate_row (tree_view, tree, node, &iter, path))
	      size_changed = TRUE;
	}

      area_below -= ROW_HEIGHT (tree_view, GTK_RBNODE_GET_HEIGHT (node));
    }
  gtk_tree_path_free (path);

  /* If we ran out of tree, and have extra area_below left, we need to add it
   * to area_above */
  if (area_below > 0)
    area_above += area_below;

  _gtk_tree_view_find_node (tree_view, above_path, &tree, &node);

  /* We walk backwards */
  while (area_above > 0)
    {
      _gtk_rbtree_prev_full (tree, node, &tree, &node);

      /* Always find the new path in the tree.  We cannot just assume
       * a gtk_tree_path_prev() is enough here, as there might be children
       * in between this node and the previous sibling node.  If this
       * appears to be a performance hotspot in profiles, we can look into
       * intrigate logic for keeping path, node and iter in sync like
       * we do for forward walks.  (Which will be hard because of the lacking
       * iter_prev).
       */

      if (node == NULL)
	break;

      gtk_tree_path_free (above_path);
      above_path = _gtk_tree_view_find_path (tree_view, tree, node);

      gtk_tree_model_get_iter (tree_view->priv->model, &iter, above_path);

      if (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_INVALID) ||
	  GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_COLUMN_INVALID))
	{
	  _gtk_tree_view_queue_draw_node (tree_view, tree, node, NULL);
	  if (validate_row (tree_view, tree, node, &iter, above_path))
	    size_changed = TRUE;
	}
      area_above -= ROW_HEIGHT (tree_view, GTK_RBNODE_GET_HEIGHT (node));
    }

  /* if we scrolled to a path, we need to set the dy here,
   * and sync the top row accordingly
   */
  if (tree_view->priv->scroll_to_path)
    {
      gtk_tree_view_set_top_row (tree_view, above_path, -area_above);
      gtk_tree_view_top_row_to_dy (tree_view);

      need_redraw = TRUE;
    }
  else if (tree_view->priv->height <= tree_view->priv->vadjustment->page_size)
    {
      /* when we are not scrolling, we should never set dy to something
       * else than zero. we update top_row to be in sync with dy = 0.
       */
      gtk_adjustment_set_value (GTK_ADJUSTMENT (tree_view->priv->vadjustment), 0);
      gtk_tree_view_dy_to_top_row (tree_view);
    }
  else if (tree_view->priv->vadjustment->value + tree_view->priv->vadjustment->page_size > tree_view->priv->height)
    {
      gtk_adjustment_set_value (GTK_ADJUSTMENT (tree_view->priv->vadjustment), tree_view->priv->height - tree_view->priv->vadjustment->page_size);
      gtk_tree_view_dy_to_top_row (tree_view);
    }
  else
    gtk_tree_view_top_row_to_dy (tree_view);

  /* update width/height and queue a resize */
  if (size_changed)
    {
      GtkRequisition requisition;

      /* We temporarily guess a size, under the assumption that it will be the
       * same when we get our next size_allocate.  If we don't do this, we'll be
       * in an inconsistent state if we call top_row_to_dy. */

      gtk_widget_size_request (GTK_WIDGET (tree_view), &requisition);
      tree_view->priv->hadjustment->upper = MAX (tree_view->priv->hadjustment->upper, (gfloat)requisition.width);
      tree_view->priv->vadjustment->upper = MAX (tree_view->priv->vadjustment->upper, (gfloat)requisition.height);
      gtk_adjustment_changed (tree_view->priv->hadjustment);
      gtk_adjustment_changed (tree_view->priv->vadjustment);
      gtk_widget_queue_resize (GTK_WIDGET (tree_view));
    }

  if (tree_view->priv->scroll_to_path)
    {
      gtk_tree_row_reference_free (tree_view->priv->scroll_to_path);
      tree_view->priv->scroll_to_path = NULL;
    }

  if (above_path)
    gtk_tree_path_free (above_path);

  if (tree_view->priv->scroll_to_column)
    {
      tree_view->priv->scroll_to_column = NULL;
    }
  if (need_redraw)
    gtk_widget_queue_draw (GTK_WIDGET (tree_view));
}

static void
initialize_fixed_height_mode (GtkTreeView *tree_view)
{
  if (!tree_view->priv->tree)
    return;

  if (tree_view->priv->fixed_height < 0)
    {
      GtkTreeIter iter;
      GtkTreePath *path;

      GtkRBTree *tree = NULL;
      GtkRBNode *node = NULL;

      tree = tree_view->priv->tree;
      node = tree->root;

      path = _gtk_tree_view_find_path (tree_view, tree, node);
      gtk_tree_model_get_iter (tree_view->priv->model, &iter, path);

      validate_row (tree_view, tree, node, &iter, path);

      gtk_tree_path_free (path);

      tree_view->priv->fixed_height = ROW_HEIGHT (tree_view, GTK_RBNODE_GET_HEIGHT (node));
    }

   _gtk_rbtree_set_fixed_height (tree_view->priv->tree,
                                 tree_view->priv->fixed_height, TRUE);
}

/* Our strategy for finding nodes to validate is a little convoluted.  We find
 * the left-most uninvalidated node.  We then try walking right, validating
 * nodes.  Once we find a valid node, we repeat the previous process of finding
 * the first invalid node.
 */

static gboolean
do_validate_rows (GtkTreeView *tree_view, gboolean queue_resize)
{
  GtkRBTree *tree = NULL;
  GtkRBNode *node = NULL;
  gboolean validated_area = FALSE;
  gint retval = TRUE;
  GtkTreePath *path = NULL;
  GtkTreeIter iter;
  GTimer *timer;
  gint i = 0;

  gint prev_height = -1;
  gboolean fixed_height = TRUE;

  g_assert (tree_view);

  if (tree_view->priv->tree == NULL)
      return FALSE;

  if (tree_view->priv->fixed_height_mode)
    {
      if (tree_view->priv->fixed_height < 0)
        initialize_fixed_height_mode (tree_view);

      return FALSE;
    }

  timer = g_timer_new ();
  g_timer_start (timer);

  do
    {
      if (! GTK_RBNODE_FLAG_SET (tree_view->priv->tree->root, GTK_RBNODE_DESCENDANTS_INVALID))
	{
	  retval = FALSE;
	  goto done;
	}

      if (path != NULL)
	{
	  node = _gtk_rbtree_next (tree, node);
	  if (node != NULL)
	    {
	      TREE_VIEW_INTERNAL_ASSERT (gtk_tree_model_iter_next (tree_view->priv->model, &iter), FALSE);
	      gtk_tree_path_next (path);
	    }
	  else
	    {
	      gtk_tree_path_free (path);
	      path = NULL;
	    }
	}

      if (path == NULL)
	{
	  tree = tree_view->priv->tree;
	  node = tree_view->priv->tree->root;

	  g_assert (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_DESCENDANTS_INVALID));

	  do
	    {
	      if (node->left != tree->nil &&
		  GTK_RBNODE_FLAG_SET (node->left, GTK_RBNODE_DESCENDANTS_INVALID))
		{
		  node = node->left;
		}
	      else if (node->right != tree->nil &&
		       GTK_RBNODE_FLAG_SET (node->right, GTK_RBNODE_DESCENDANTS_INVALID))
		{
		  node = node->right;
		}
	      else if (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_INVALID) ||
		       GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_COLUMN_INVALID))
		{
		  break;
		}
	      else if (node->children != NULL)
		{
		  tree = node->children;
		  node = tree->root;
		}
	      else
		/* RBTree corruption!  All bad */
		g_assert_not_reached ();
	    }
	  while (TRUE);
	  path = _gtk_tree_view_find_path (tree_view, tree, node);
	  gtk_tree_model_get_iter (tree_view->priv->model, &iter, path);
	}

      validated_area = validate_row (tree_view, tree, node, &iter, path) ||
                       validated_area;

      if (!tree_view->priv->fixed_height_check)
        {
	  gint height;

	  height = ROW_HEIGHT (tree_view, GTK_RBNODE_GET_HEIGHT (node));
	  if (prev_height < 0)
	    prev_height = height;
	  else if (prev_height != height)
	    fixed_height = FALSE;
	}

      i++;
    }
  while (g_timer_elapsed (timer, NULL) < GTK_TREE_VIEW_TIME_MS_PER_IDLE / 1000.);

  if (!tree_view->priv->fixed_height_check)
   {
     if (fixed_height)
       _gtk_rbtree_set_fixed_height (tree_view->priv->tree, prev_height, FALSE);

     tree_view->priv->fixed_height_check = 1;
   }
  
 done:
  if (validated_area)
    {
      GtkRequisition requisition;
      /* We temporarily guess a size, under the assumption that it will be the
       * same when we get our next size_allocate.  If we don't do this, we'll be
       * in an inconsistent state when we call top_row_to_dy. */

      gtk_widget_size_request (GTK_WIDGET (tree_view), &requisition);
      tree_view->priv->hadjustment->upper = MAX (tree_view->priv->hadjustment->upper, (gfloat)requisition.width);
      tree_view->priv->vadjustment->upper = MAX (tree_view->priv->vadjustment->upper, (gfloat)requisition.height);
      gtk_adjustment_changed (tree_view->priv->hadjustment);
      gtk_adjustment_changed (tree_view->priv->vadjustment);

      if (queue_resize)
        gtk_widget_queue_resize (GTK_WIDGET (tree_view));
    }

  if (path) gtk_tree_path_free (path);
  g_timer_destroy (timer);

  return retval;
}

static gboolean
validate_rows (GtkTreeView *tree_view)
{
  gboolean retval;
  
  retval = do_validate_rows (tree_view, TRUE);
  
  if (! retval && tree_view->priv->validate_rows_timer)
    {
      g_source_remove (tree_view->priv->validate_rows_timer);
      tree_view->priv->validate_rows_timer = 0;
    }

  return retval;
}

static gboolean
validate_rows_handler (GtkTreeView *tree_view)
{
  gboolean retval;

  retval = do_validate_rows (tree_view, TRUE);
  if (! retval && tree_view->priv->validate_rows_timer)
    {
      g_source_remove (tree_view->priv->validate_rows_timer);
      tree_view->priv->validate_rows_timer = 0;
    }

  return retval;
}

static gboolean
do_presize_handler (GtkTreeView *tree_view)
{
  if (tree_view->priv->mark_rows_col_dirty)
    {
      if (tree_view->priv->tree)
	_gtk_rbtree_column_invalid (tree_view->priv->tree);
      tree_view->priv->mark_rows_col_dirty = FALSE;
    }
  validate_visible_area (tree_view);
  tree_view->priv->presize_handler_timer = 0;

  if (tree_view->priv->fixed_height_mode)
    {
      GtkRequisition requisition;

      gtk_widget_size_request (GTK_WIDGET (tree_view), &requisition);

      tree_view->priv->hadjustment->upper = MAX (tree_view->priv->hadjustment->upper, (gfloat)requisition.width);
      tree_view->priv->vadjustment->upper = MAX (tree_view->priv->vadjustment->upper, (gfloat)requisition.height);
      gtk_adjustment_changed (tree_view->priv->hadjustment);
      gtk_adjustment_changed (tree_view->priv->vadjustment);
      gtk_widget_queue_resize (GTK_WIDGET (tree_view));
    }
		   
  return FALSE;
}

static gboolean
presize_handler_callback (gpointer data)
{
  do_presize_handler (GTK_TREE_VIEW (data));
		   
  return FALSE;
}

static void
install_presize_handler (GtkTreeView *tree_view)
{
  if (! gtk_widget_get_realized (GTK_WIDGET (tree_view)))
    return;

  if (! tree_view->priv->presize_handler_timer)
    {
      tree_view->priv->presize_handler_timer =
	gdk_threads_add_idle_full (GTK_PRIORITY_RESIZE - 2, presize_handler_callback, tree_view, NULL);
    }
  if (! tree_view->priv->validate_rows_timer)
    {
      tree_view->priv->validate_rows_timer =
	gdk_threads_add_idle_full (GTK_TREE_VIEW_PRIORITY_VALIDATE, (GSourceFunc) validate_rows_handler, tree_view, NULL);
    }
}

static gboolean
scroll_sync_handler (GtkTreeView *tree_view)
{
  if (tree_view->priv->height <= tree_view->priv->vadjustment->page_size)
    gtk_adjustment_set_value (GTK_ADJUSTMENT (tree_view->priv->vadjustment), 0);
  else if (gtk_tree_row_reference_valid (tree_view->priv->top_row))
    gtk_tree_view_top_row_to_dy (tree_view);
  else
    gtk_tree_view_dy_to_top_row (tree_view);

  tree_view->priv->scroll_sync_timer = 0;

  return FALSE;
}

static void
install_scroll_sync_handler (GtkTreeView *tree_view)
{
  if (!gtk_widget_get_realized (GTK_WIDGET (tree_view)))
    return;

  if (!tree_view->priv->scroll_sync_timer)
    {
      tree_view->priv->scroll_sync_timer =
	gdk_threads_add_idle_full (GTK_TREE_VIEW_PRIORITY_SCROLL_SYNC, (GSourceFunc) scroll_sync_handler, tree_view, NULL);
    }
}

static void
gtk_tree_view_set_top_row (GtkTreeView *tree_view,
			   GtkTreePath *path,
			   gint         offset)
{
  gtk_tree_row_reference_free (tree_view->priv->top_row);

  if (!path)
    {
      tree_view->priv->top_row = NULL;
      tree_view->priv->top_row_dy = 0;
    }
  else
    {
      tree_view->priv->top_row = gtk_tree_row_reference_new_proxy (G_OBJECT (tree_view), tree_view->priv->model, path);
      tree_view->priv->top_row_dy = offset;
    }
}

/* Always call this iff dy is in the visible range.  If the tree is empty, then
 * it's set to be NULL, and top_row_dy is 0;
 */
static void
gtk_tree_view_dy_to_top_row (GtkTreeView *tree_view)
{
  gint offset;
  GtkTreePath *path;
  GtkRBTree *tree;
  GtkRBNode *node;

  if (tree_view->priv->tree == NULL)
    {
      gtk_tree_view_set_top_row (tree_view, NULL, 0);
    }
  else
    {
      offset = _gtk_rbtree_find_offset (tree_view->priv->tree,
					tree_view->priv->dy,
					&tree, &node);

      if (tree == NULL)
        {
	  gtk_tree_view_set_top_row (tree_view, NULL, 0);
	}
      else
        {
	  path = _gtk_tree_view_find_path (tree_view, tree, node);
	  gtk_tree_view_set_top_row (tree_view, path, offset);
	  gtk_tree_path_free (path);
	}
    }
}

static void
gtk_tree_view_top_row_to_dy (GtkTreeView *tree_view)
{
  GtkTreePath *path;
  GtkRBTree *tree;
  GtkRBNode *node;
  int new_dy;

  /* Avoid recursive calls */
  if (tree_view->priv->in_top_row_to_dy)
    return;

  if (tree_view->priv->top_row)
    path = gtk_tree_row_reference_get_path (tree_view->priv->top_row);
  else
    path = NULL;

  if (!path)
    tree = NULL;
  else
    _gtk_tree_view_find_node (tree_view, path, &tree, &node);

  if (path)
    gtk_tree_path_free (path);

  if (tree == NULL)
    {
      /* keep dy and set new toprow */
      gtk_tree_row_reference_free (tree_view->priv->top_row);
      tree_view->priv->top_row = NULL;
      tree_view->priv->top_row_dy = 0;
      /* DO NOT install the idle handler */
      gtk_tree_view_dy_to_top_row (tree_view);
      return;
    }

  if (ROW_HEIGHT (tree_view, BACKGROUND_HEIGHT (node))
      < tree_view->priv->top_row_dy)
    {
      /* new top row -- do NOT install the idle handler */
      gtk_tree_view_dy_to_top_row (tree_view);
      return;
    }

  new_dy = _gtk_rbtree_node_find_offset (tree, node);
  new_dy += tree_view->priv->top_row_dy;

  if (new_dy + tree_view->priv->vadjustment->page_size > tree_view->priv->height)
    new_dy = tree_view->priv->height - tree_view->priv->vadjustment->page_size;

  new_dy = MAX (0, new_dy);

  tree_view->priv->in_top_row_to_dy = TRUE;
  gtk_adjustment_set_value (tree_view->priv->vadjustment, (gdouble)new_dy);
  tree_view->priv->in_top_row_to_dy = FALSE;
}


void
_gtk_tree_view_install_mark_rows_col_dirty (GtkTreeView *tree_view)
{
  tree_view->priv->mark_rows_col_dirty = TRUE;

  install_presize_handler (tree_view);
}

/*
 * This function works synchronously (due to the while (validate_rows...)
 * loop).
 *
 * There was a check for column_type != GTK_TREE_VIEW_COLUMN_AUTOSIZE
 * here. You now need to check that yourself.
 */
void
_gtk_tree_view_column_autosize (GtkTreeView *tree_view,
			        GtkTreeViewColumn *column)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (column));

  _gtk_tree_view_column_cell_set_dirty (column, FALSE);

  do_presize_handler (tree_view);
  while (validate_rows (tree_view));

  gtk_widget_queue_resize (GTK_WIDGET (tree_view));
}

/* Drag-and-drop */

static void
set_source_row (GdkDragContext *context,
                GtkTreeModel   *model,
                GtkTreePath    *source_row)
{
  g_object_set_data_full (G_OBJECT (context),
                          I_("gtk-tree-view-source-row"),
                          source_row ? gtk_tree_row_reference_new (model, source_row) : NULL,
                          (GDestroyNotify) (source_row ? gtk_tree_row_reference_free : NULL));
}

static GtkTreePath*
get_source_row (GdkDragContext *context)
{
  GtkTreeRowReference *ref =
    g_object_get_data (G_OBJECT (context), "gtk-tree-view-source-row");

  if (ref)
    return gtk_tree_row_reference_get_path (ref);
  else
    return NULL;
}

typedef struct
{
  GtkTreeRowReference *dest_row;
  guint                path_down_mode   : 1;
  guint                empty_view_drop  : 1;
  guint                drop_append_mode : 1;
}
DestRow;

static void
dest_row_free (gpointer data)
{
  DestRow *dr = (DestRow *)data;

  gtk_tree_row_reference_free (dr->dest_row);
  g_slice_free (DestRow, dr);
}

static void
set_dest_row (GdkDragContext *context,
              GtkTreeModel   *model,
              GtkTreePath    *dest_row,
              gboolean        path_down_mode,
              gboolean        empty_view_drop,
              gboolean        drop_append_mode)
{
  DestRow *dr;

  if (!dest_row)
    {
      g_object_set_data_full (G_OBJECT (context), I_("gtk-tree-view-dest-row"),
                              NULL, NULL);
      return;
    }

  dr = g_slice_new (DestRow);

  dr->dest_row = gtk_tree_row_reference_new (model, dest_row);
  dr->path_down_mode = path_down_mode != FALSE;
  dr->empty_view_drop = empty_view_drop != FALSE;
  dr->drop_append_mode = drop_append_mode != FALSE;

  g_object_set_data_full (G_OBJECT (context), I_("gtk-tree-view-dest-row"),
                          dr, (GDestroyNotify) dest_row_free);
}

static GtkTreePath*
get_dest_row (GdkDragContext *context,
              gboolean       *path_down_mode)
{
  DestRow *dr =
    g_object_get_data (G_OBJECT (context), "gtk-tree-view-dest-row");

  if (dr)
    {
      GtkTreePath *path = NULL;

      if (path_down_mode)
        *path_down_mode = dr->path_down_mode;

      if (dr->dest_row)
        path = gtk_tree_row_reference_get_path (dr->dest_row);
      else if (dr->empty_view_drop)
        path = gtk_tree_path_new_from_indices (0, -1);
      else
        path = NULL;

      if (path && dr->drop_append_mode)
        gtk_tree_path_next (path);

      return path;
    }
  else
    return NULL;
}

/* Get/set whether drag_motion requested the drag data and
 * drag_data_received should thus not actually insert the data,
 * since the data doesn't result from a drop.
 */
static void
set_status_pending (GdkDragContext *context,
                    GdkDragAction   suggested_action)
{
  g_object_set_data (G_OBJECT (context),
                     I_("gtk-tree-view-status-pending"),
                     GINT_TO_POINTER (suggested_action));
}

static GdkDragAction
get_status_pending (GdkDragContext *context)
{
  return GPOINTER_TO_INT (g_object_get_data (G_OBJECT (context),
                                             "gtk-tree-view-status-pending"));
}

static TreeViewDragInfo*
get_info (GtkTreeView *tree_view)
{
  return g_object_get_data (G_OBJECT (tree_view), "gtk-tree-view-drag-info");
}

static void
destroy_info (TreeViewDragInfo *di)
{
  g_slice_free (TreeViewDragInfo, di);
}

static TreeViewDragInfo*
ensure_info (GtkTreeView *tree_view)
{
  TreeViewDragInfo *di;

  di = get_info (tree_view);

  if (di == NULL)
    {
      di = g_slice_new0 (TreeViewDragInfo);

      g_object_set_data_full (G_OBJECT (tree_view),
                              I_("gtk-tree-view-drag-info"),
                              di,
                              (GDestroyNotify) destroy_info);
    }

  return di;
}

static void
remove_info (GtkTreeView *tree_view)
{
  g_object_set_data (G_OBJECT (tree_view), I_("gtk-tree-view-drag-info"), NULL);
}

#if 0
static gint
drag_scan_timeout (gpointer data)
{
  GtkTreeView *tree_view;
  gint x, y;
  GdkModifierType state;
  GtkTreePath *path = NULL;
  GtkTreeViewColumn *column = NULL;
  GdkRectangle visible_rect;

  GDK_THREADS_ENTER ();

  tree_view = GTK_TREE_VIEW (data);

  gdk_window_get_pointer (tree_view->priv->bin_window,
                          &x, &y, &state);

  gtk_tree_view_get_visible_rect (tree_view, &visible_rect);

  /* See if we are near the edge. */
  if ((x - visible_rect.x) < SCROLL_EDGE_SIZE ||
      (visible_rect.x + visible_rect.width - x) < SCROLL_EDGE_SIZE ||
      (y - visible_rect.y) < SCROLL_EDGE_SIZE ||
      (visible_rect.y + visible_rect.height - y) < SCROLL_EDGE_SIZE)
    {
      gtk_tree_view_get_path_at_pos (tree_view,
                                     tree_view->priv->bin_window,
                                     x, y,
                                     &path,
                                     &column,
                                     NULL,
                                     NULL);

      if (path != NULL)
        {
          gtk_tree_view_scroll_to_cell (tree_view,
                                        path,
                                        column,
					TRUE,
                                        0.5, 0.5);

          gtk_tree_path_free (path);
        }
    }

  GDK_THREADS_LEAVE ();

  return TRUE;
}
#endif /* 0 */

static void
add_scroll_timeout (GtkTreeView *tree_view)
{
  if (tree_view->priv->scroll_timeout == 0)
    {
      tree_view->priv->scroll_timeout =
	gdk_threads_add_timeout (150, scroll_row_timeout, tree_view);
    }
}

static void
remove_scroll_timeout (GtkTreeView *tree_view)
{
  if (tree_view->priv->scroll_timeout != 0)
    {
      g_source_remove (tree_view->priv->scroll_timeout);
      tree_view->priv->scroll_timeout = 0;
    }
}

static gboolean
check_model_dnd (GtkTreeModel *model,
                 GType         required_iface,
                 const gchar  *signal)
{
  if (model == NULL || !G_TYPE_CHECK_INSTANCE_TYPE ((model), required_iface))
    {
      g_warning ("You must override the default '%s' handler "
                 "on GtkTreeView when using models that don't support "
                 "the %s interface and enabling drag-and-drop. The simplest way to do this "
                 "is to connect to '%s' and call "
                 "g_signal_stop_emission_by_name() in your signal handler to prevent "
                 "the default handler from running. Look at the source code "
                 "for the default handler in gtktreeview.c to get an idea what "
                 "your handler should do. (gtktreeview.c is in the GTK source "
                 "code.) If you're using GTK from a language other than C, "
                 "there may be a more natural way to override default handlers, e.g. via derivation.",
                 signal, g_type_name (required_iface), signal);
      return FALSE;
    }
  else
    return TRUE;
}

static void
remove_open_timeout (GtkTreeView *tree_view)
{
  if (tree_view->priv->open_dest_timeout != 0)
    {
      g_source_remove (tree_view->priv->open_dest_timeout);
      tree_view->priv->open_dest_timeout = 0;
    }
}


static gint
open_row_timeout (gpointer data)
{
  GtkTreeView *tree_view = data;
  GtkTreePath *dest_path = NULL;
  GtkTreeViewDropPosition pos;
  gboolean result = FALSE;

  gtk_tree_view_get_drag_dest_row (tree_view,
                                   &dest_path,
                                   &pos);

  if (dest_path &&
      (pos == GTK_TREE_VIEW_DROP_INTO_OR_AFTER ||
       pos == GTK_TREE_VIEW_DROP_INTO_OR_BEFORE))
    {
      gtk_tree_view_expand_row (tree_view, dest_path, FALSE);
      tree_view->priv->open_dest_timeout = 0;

      gtk_tree_path_free (dest_path);
    }
  else
    {
      if (dest_path)
        gtk_tree_path_free (dest_path);

      result = TRUE;
    }

  return result;
}

static gboolean
scroll_row_timeout (gpointer data)
{
  GtkTreeView *tree_view = data;

  gtk_tree_view_vertical_autoscroll (tree_view);

  if (tree_view->priv->rubber_band_status == RUBBER_BAND_ACTIVE)
    gtk_tree_view_update_rubber_band (tree_view);

  return TRUE;
}

/* Returns TRUE if event should not be propagated to parent widgets */
static gboolean
set_destination_row (GtkTreeView    *tree_view,
                     GdkDragContext *context,
                     /* coordinates relative to the widget */
                     gint            x,
                     gint            y,
                     GdkDragAction  *suggested_action,
                     GdkAtom        *target)
{
  GtkTreePath *path = NULL;
  GtkTreeViewDropPosition pos;
  GtkTreeViewDropPosition old_pos;
  TreeViewDragInfo *di;
  GtkWidget *widget;
  GtkTreePath *old_dest_path = NULL;
  gboolean can_drop = FALSE;

  *suggested_action = 0;
  *target = GDK_NONE;

  widget = GTK_WIDGET (tree_view);

  di = get_info (tree_view);

  if (di == NULL || y - TREE_VIEW_HEADER_HEIGHT (tree_view) < 0)
    {
      /* someone unset us as a drag dest, note that if
       * we return FALSE drag_leave isn't called
       */

      gtk_tree_view_set_drag_dest_row (tree_view,
                                       NULL,
                                       GTK_TREE_VIEW_DROP_BEFORE);

      remove_scroll_timeout (GTK_TREE_VIEW (widget));
      remove_open_timeout (GTK_TREE_VIEW (widget));

      return FALSE; /* no longer a drop site */
    }

  *target = gtk_drag_dest_find_target (widget, context,
                                       gtk_drag_dest_get_target_list (widget));
  if (*target == GDK_NONE)
    {
      return FALSE;
    }

  if (!gtk_tree_view_get_dest_row_at_pos (tree_view,
                                          x, y,
                                          &path,
                                          &pos))
    {
      gint n_children;
      GtkTreeModel *model;

      remove_open_timeout (tree_view);

      /* the row got dropped on empty space, let's setup a special case
       */

      if (path)
	gtk_tree_path_free (path);

      model = gtk_tree_view_get_model (tree_view);

      n_children = gtk_tree_model_iter_n_children (model, NULL);
      if (n_children)
        {
          pos = GTK_TREE_VIEW_DROP_AFTER;
          path = gtk_tree_path_new_from_indices (n_children - 1, -1);
        }
      else
        {
          pos = GTK_TREE_VIEW_DROP_BEFORE;
          path = gtk_tree_path_new_from_indices (0, -1);
        }

      can_drop = TRUE;

      goto out;
    }

  g_assert (path);

  /* If we left the current row's "open" zone, unset the timeout for
   * opening the row
   */
  gtk_tree_view_get_drag_dest_row (tree_view,
                                   &old_dest_path,
                                   &old_pos);

  if (old_dest_path &&
      (gtk_tree_path_compare (path, old_dest_path) != 0 ||
       !(pos == GTK_TREE_VIEW_DROP_INTO_OR_AFTER ||
         pos == GTK_TREE_VIEW_DROP_INTO_OR_BEFORE)))
    remove_open_timeout (tree_view);

  if (old_dest_path)
    gtk_tree_path_free (old_dest_path);

  if (TRUE /* FIXME if the location droppable predicate */)
    {
      can_drop = TRUE;
    }

out:
  if (can_drop)
    {
      GtkWidget *source_widget;

      *suggested_action = gdk_drag_context_get_suggested_action (context);
      source_widget = gtk_drag_get_source_widget (context);

      if (source_widget == widget)
        {
          /* Default to MOVE, unless the user has
           * pressed ctrl or shift to affect available actions
           */
          if ((gdk_drag_context_get_actions (context) & GDK_ACTION_MOVE) != 0)
            *suggested_action = GDK_ACTION_MOVE;
        }

      gtk_tree_view_set_drag_dest_row (GTK_TREE_VIEW (widget),
                                       path, pos);
    }
  else
    {
      /* can't drop here */
      remove_open_timeout (tree_view);

      gtk_tree_view_set_drag_dest_row (GTK_TREE_VIEW (widget),
                                       NULL,
                                       GTK_TREE_VIEW_DROP_BEFORE);
    }

  if (path)
    gtk_tree_path_free (path);

  return TRUE;
}

static GtkTreePath*
get_logical_dest_row (GtkTreeView *tree_view,
                      gboolean    *path_down_mode,
                      gboolean    *drop_append_mode)
{
  /* adjust path to point to the row the drop goes in front of */
  GtkTreePath *path = NULL;
  GtkTreeViewDropPosition pos;

  g_return_val_if_fail (path_down_mode != NULL, NULL);
  g_return_val_if_fail (drop_append_mode != NULL, NULL);

  *path_down_mode = FALSE;
  *drop_append_mode = 0;

  gtk_tree_view_get_drag_dest_row (tree_view, &path, &pos);

  if (path == NULL)
    return NULL;

  if (pos == GTK_TREE_VIEW_DROP_BEFORE)
    ; /* do nothing */
  else if (pos == GTK_TREE_VIEW_DROP_INTO_OR_BEFORE ||
           pos == GTK_TREE_VIEW_DROP_INTO_OR_AFTER)
    *path_down_mode = TRUE;
  else
    {
      GtkTreeIter iter;
      GtkTreeModel *model = gtk_tree_view_get_model (tree_view);

      g_assert (pos == GTK_TREE_VIEW_DROP_AFTER);

      if (!gtk_tree_model_get_iter (model, &iter, path) ||
          !gtk_tree_model_iter_next (model, &iter))
        *drop_append_mode = 1;
      else
        {
          *drop_append_mode = 0;
          gtk_tree_path_next (path);
        }
    }

  return path;
}

static gboolean
gtk_tree_view_maybe_begin_dragging_row (GtkTreeView      *tree_view,
                                        GdkEventMotion   *event)
{
  GtkWidget *widget = GTK_WIDGET (tree_view);
  GdkDragContext *context;
  TreeViewDragInfo *di;
  GtkTreePath *path = NULL;
  gint button;
  gint cell_x, cell_y;
  GtkTreeModel *model;
  gboolean retval = FALSE;

  di = get_info (tree_view);

  if (di == NULL || !di->source_set)
    goto out;

  if (tree_view->priv->pressed_button < 0)
    goto out;

  if (!gtk_drag_check_threshold (widget,
                                 tree_view->priv->press_start_x,
                                 tree_view->priv->press_start_y,
                                 event->x, event->y))
    goto out;

  model = gtk_tree_view_get_model (tree_view);

  if (model == NULL)
    goto out;

  button = tree_view->priv->pressed_button;
  tree_view->priv->pressed_button = -1;

  gtk_tree_view_get_path_at_pos (tree_view,
                                 tree_view->priv->press_start_x,
                                 tree_view->priv->press_start_y,
                                 &path,
                                 NULL,
                                 &cell_x,
                                 &cell_y);

  if (path == NULL)
    goto out;

  if (!GTK_IS_TREE_DRAG_SOURCE (model) ||
      !gtk_tree_drag_source_row_draggable (GTK_TREE_DRAG_SOURCE (model),
					   path))
    goto out;

  if (!(GDK_BUTTON1_MASK << (button - 1) & di->start_button_mask))
    goto out;

  /* Now we can begin the drag */

  retval = TRUE;

  context = gtk_drag_begin (widget,
                            gtk_drag_source_get_target_list (widget),
                            di->source_actions,
                            button,
                            (GdkEvent*)event);

  set_source_row (context, model, path);

 out:
  if (path)
    gtk_tree_path_free (path);

  return retval;
}


static void
gtk_tree_view_drag_begin (GtkWidget      *widget,
                          GdkDragContext *context)
{
  GtkTreeView *tree_view;
  GtkTreePath *path = NULL;
  gint cell_x, cell_y;
  GdkPixmap *row_pix;
  TreeViewDragInfo *di;

  tree_view = GTK_TREE_VIEW (widget);

  /* if the user uses a custom DND source impl, we don't set the icon here */
  di = get_info (tree_view);

  if (di == NULL || !di->source_set)
    return;

  gtk_tree_view_get_path_at_pos (tree_view,
                                 tree_view->priv->press_start_x,
                                 tree_view->priv->press_start_y,
                                 &path,
                                 NULL,
                                 &cell_x,
                                 &cell_y);

  g_return_if_fail (path != NULL);

  row_pix = gtk_tree_view_create_row_drag_icon (tree_view,
                                                path);

  gtk_drag_set_icon_pixmap (context,
                            gdk_drawable_get_colormap (row_pix),
                            row_pix,
                            NULL,
                            /* the + 1 is for the black border in the icon */
                            tree_view->priv->press_start_x + 1,
                            cell_y + 1);

  g_object_unref (row_pix);
  gtk_tree_path_free (path);
}

static void
gtk_tree_view_drag_end (GtkWidget      *widget,
                        GdkDragContext *context)
{
  /* do nothing */
}

/* Default signal implementations for the drag signals */
static void
gtk_tree_view_drag_data_get (GtkWidget        *widget,
                             GdkDragContext   *context,
                             GtkSelectionData *selection_data,
                             guint             info,
                             guint             time)
{
  GtkTreeView *tree_view;
  GtkTreeModel *model;
  TreeViewDragInfo *di;
  GtkTreePath *source_row;

  tree_view = GTK_TREE_VIEW (widget);

  model = gtk_tree_view_get_model (tree_view);

  if (model == NULL)
    return;

  di = get_info (GTK_TREE_VIEW (widget));

  if (di == NULL)
    return;

  source_row = get_source_row (context);

  if (source_row == NULL)
    return;

  /* We can implement the GTK_TREE_MODEL_ROW target generically for
   * any model; for DragSource models there are some other targets
   * we also support.
   */

  if (GTK_IS_TREE_DRAG_SOURCE (model) &&
      gtk_tree_drag_source_drag_data_get (GTK_TREE_DRAG_SOURCE (model),
                                          source_row,
                                          selection_data))
    goto done;

  /* If drag_data_get does nothing, try providing row data. */
  if (selection_data->target == gdk_atom_intern_static_string ("GTK_TREE_MODEL_ROW"))
    {
      gtk_tree_set_row_drag_data (selection_data,
				  model,
				  source_row);
    }

 done:
  gtk_tree_path_free (source_row);
}


static void
gtk_tree_view_drag_data_delete (GtkWidget      *widget,
                                GdkDragContext *context)
{
  TreeViewDragInfo *di;
  GtkTreeModel *model;
  GtkTreeView *tree_view;
  GtkTreePath *source_row;

  tree_view = GTK_TREE_VIEW (widget);
  model = gtk_tree_view_get_model (tree_view);

  if (!check_model_dnd (model, GTK_TYPE_TREE_DRAG_SOURCE, "drag_data_delete"))
    return;

  di = get_info (tree_view);

  if (di == NULL)
    return;

  source_row = get_source_row (context);

  if (source_row == NULL)
    return;

  gtk_tree_drag_source_drag_data_delete (GTK_TREE_DRAG_SOURCE (model),
                                         source_row);

  gtk_tree_path_free (source_row);

  set_source_row (context, NULL, NULL);
}

static void
gtk_tree_view_drag_leave (GtkWidget      *widget,
                          GdkDragContext *context,
                          guint             time)
{
  /* unset any highlight row */
  gtk_tree_view_set_drag_dest_row (GTK_TREE_VIEW (widget),
                                   NULL,
                                   GTK_TREE_VIEW_DROP_BEFORE);

  remove_scroll_timeout (GTK_TREE_VIEW (widget));
  remove_open_timeout (GTK_TREE_VIEW (widget));
}


static gboolean
gtk_tree_view_drag_motion (GtkWidget        *widget,
                           GdkDragContext   *context,
			   /* coordinates relative to the widget */
                           gint              x,
                           gint              y,
                           guint             time)
{
  gboolean empty;
  GtkTreePath *path = NULL;
  GtkTreeViewDropPosition pos;
  GtkTreeView *tree_view;
  GdkDragAction suggested_action = 0;
  GdkAtom target;

  tree_view = GTK_TREE_VIEW (widget);

  if (!set_destination_row (tree_view, context, x, y, &suggested_action, &target))
    return FALSE;

  gtk_tree_view_get_drag_dest_row (tree_view, &path, &pos);

  /* we only know this *after* set_desination_row */
  empty = tree_view->priv->empty_view_drop;

  if (path == NULL && !empty)
    {
      /* Can't drop here. */
      gdk_drag_status (context, 0, time);
    }
  else
    {
      if (tree_view->priv->open_dest_timeout == 0 &&
          (pos == GTK_TREE_VIEW_DROP_INTO_OR_AFTER ||
           pos == GTK_TREE_VIEW_DROP_INTO_OR_BEFORE))
        {
          tree_view->priv->open_dest_timeout =
            gdk_threads_add_timeout (AUTO_EXPAND_TIMEOUT, open_row_timeout, tree_view);
        }
      else
        {
	  add_scroll_timeout (tree_view);
	}

      if (target == gdk_atom_intern_static_string ("GTK_TREE_MODEL_ROW"))
        {
          /* Request data so we can use the source row when
           * determining whether to accept the drop
           */
          set_status_pending (context, suggested_action);
          gtk_drag_get_data (widget, context, target, time);
        }
      else
        {
          set_status_pending (context, 0);
          gdk_drag_status (context, suggested_action, time);
        }
    }

  if (path)
    gtk_tree_path_free (path);

  return TRUE;
}


static gboolean
gtk_tree_view_drag_drop (GtkWidget        *widget,
                         GdkDragContext   *context,
			 /* coordinates relative to the widget */
                         gint              x,
                         gint              y,
                         guint             time)
{
  GtkTreeView *tree_view;
  GtkTreePath *path;
  GdkDragAction suggested_action = 0;
  GdkAtom target = GDK_NONE;
  TreeViewDragInfo *di;
  GtkTreeModel *model;
  gboolean path_down_mode;
  gboolean drop_append_mode;

  tree_view = GTK_TREE_VIEW (widget);

  model = gtk_tree_view_get_model (tree_view);

  remove_scroll_timeout (GTK_TREE_VIEW (widget));
  remove_open_timeout (GTK_TREE_VIEW (widget));

  di = get_info (tree_view);

  if (di == NULL)
    return FALSE;

  if (!check_model_dnd (model, GTK_TYPE_TREE_DRAG_DEST, "drag_drop"))
    return FALSE;

  if (!set_destination_row (tree_view, context, x, y, &suggested_action, &target))
    return FALSE;

  path = get_logical_dest_row (tree_view, &path_down_mode, &drop_append_mode);

  if (target != GDK_NONE && path != NULL)
    {
      /* in case a motion had requested drag data, change things so we
       * treat drag data receives as a drop.
       */
      set_status_pending (context, 0);
      set_dest_row (context, model, path,
                    path_down_mode, tree_view->priv->empty_view_drop,
                    drop_append_mode);
    }

  if (path)
    gtk_tree_path_free (path);

  /* Unset this thing */
  gtk_tree_view_set_drag_dest_row (GTK_TREE_VIEW (widget),
                                   NULL,
                                   GTK_TREE_VIEW_DROP_BEFORE);

  if (target != GDK_NONE)
    {
      gtk_drag_get_data (widget, context, target, time);
      return TRUE;
    }
  else
    return FALSE;
}

static void
gtk_tree_view_drag_data_received (GtkWidget        *widget,
                                  GdkDragContext   *context,
				  /* coordinates relative to the widget */
                                  gint              x,
                                  gint              y,
                                  GtkSelectionData *selection_data,
                                  guint             info,
                                  guint             time)
{
  GtkTreePath *path;
  TreeViewDragInfo *di;
  gboolean accepted = FALSE;
  GtkTreeModel *model;
  GtkTreeView *tree_view;
  GtkTreePath *dest_row;
  GdkDragAction suggested_action;
  gboolean path_down_mode;
  gboolean drop_append_mode;

  tree_view = GTK_TREE_VIEW (widget);

  model = gtk_tree_view_get_model (tree_view);

  if (!check_model_dnd (model, GTK_TYPE_TREE_DRAG_DEST, "drag_data_received"))
    return;

  di = get_info (tree_view);

  if (di == NULL)
    return;

  suggested_action = get_status_pending (context);

  if (suggested_action)
    {
      /* We are getting this data due to a request in drag_motion,
       * rather than due to a request in drag_drop, so we are just
       * supposed to call drag_status, not actually paste in the
       * data.
       */
      path = get_logical_dest_row (tree_view, &path_down_mode,
                                   &drop_append_mode);

      if (path == NULL)
        suggested_action = 0;
      else if (path_down_mode)
        gtk_tree_path_down (path);

      if (suggested_action)
        {
	  if (!gtk_tree_drag_dest_row_drop_possible (GTK_TREE_DRAG_DEST (model),
						     path,
						     selection_data))
            {
              if (path_down_mode)
                {
                  path_down_mode = FALSE;
                  gtk_tree_path_up (path);

                  if (!gtk_tree_drag_dest_row_drop_possible (GTK_TREE_DRAG_DEST (model),
                                                             path,
                                                             selection_data))
                    suggested_action = 0;
                }
              else
	        suggested_action = 0;
            }
        }

      gdk_drag_status (context, suggested_action, time);

      if (path)
        gtk_tree_path_free (path);

      /* If you can't drop, remove user drop indicator until the next motion */
      if (suggested_action == 0)
        gtk_tree_view_set_drag_dest_row (GTK_TREE_VIEW (widget),
                                         NULL,
                                         GTK_TREE_VIEW_DROP_BEFORE);

      return;
    }

  dest_row = get_dest_row (context, &path_down_mode);

  if (dest_row == NULL)
    return;

  if (selection_data->length >= 0)
    {
      if (path_down_mode)
        {
          gtk_tree_path_down (dest_row);
          if (!gtk_tree_drag_dest_row_drop_possible (GTK_TREE_DRAG_DEST (model),
                                                     dest_row, selection_data))
            gtk_tree_path_up (dest_row);
        }
    }

  if (selection_data->length >= 0)
    {
      if (gtk_tree_drag_dest_drag_data_received (GTK_TREE_DRAG_DEST (model),
                                                 dest_row,
                                                 selection_data))
        accepted = TRUE;
    }

  gtk_drag_finish (context,
                   accepted,
                   (gdk_drag_context_get_selected_action (context) == GDK_ACTION_MOVE),
                   time);

  if (gtk_tree_path_get_depth (dest_row) == 1
      && gtk_tree_path_get_indices (dest_row)[0] == 0)
    {
      /* special special case drag to "0", scroll to first item */
      if (!tree_view->priv->scroll_to_path)
        gtk_tree_view_scroll_to_cell (tree_view, dest_row, NULL, FALSE, 0.0, 0.0);
    }

  gtk_tree_path_free (dest_row);

  /* drop dest_row */
  set_dest_row (context, NULL, NULL, FALSE, FALSE, FALSE);
}



/* GtkContainer Methods
 */


static void
gtk_tree_view_remove (GtkContainer *container,
		      GtkWidget    *widget)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (container);
  GtkTreeViewChild *child = NULL;
  GList *tmp_list;

  tmp_list = tree_view->priv->children;
  while (tmp_list)
    {
      child = tmp_list->data;
      if (child->widget == widget)
	{
	  gtk_widget_unparent (widget);

	  tree_view->priv->children = g_list_remove_link (tree_view->priv->children, tmp_list);
	  g_list_free_1 (tmp_list);
	  g_slice_free (GtkTreeViewChild, child);
	  return;
	}

      tmp_list = tmp_list->next;
    }

  tmp_list = tree_view->priv->columns;

  while (tmp_list)
    {
      GtkTreeViewColumn *column;
      column = tmp_list->data;
      if (column->button == widget)
	{
	  gtk_widget_unparent (widget);
	  return;
	}
      tmp_list = tmp_list->next;
    }
}

static void
gtk_tree_view_forall (GtkContainer *container,
		      gboolean      include_internals,
		      GtkCallback   callback,
		      gpointer      callback_data)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (container);
  GtkTreeViewChild *child = NULL;
  GtkTreeViewColumn *column;
  GList *tmp_list;

  tmp_list = tree_view->priv->children;
  while (tmp_list)
    {
      child = tmp_list->data;
      tmp_list = tmp_list->next;

      (* callback) (child->widget, callback_data);
    }
  if (include_internals == FALSE)
    return;

  for (tmp_list = tree_view->priv->columns; tmp_list; tmp_list = tmp_list->next)
    {
      column = tmp_list->data;

      if (column->button)
	(* callback) (column->button, callback_data);
    }
}

/* Returns TRUE if the treeview contains no "special" (editable or activatable)
 * cells. If so we draw one big row-spanning focus rectangle.
 */
static gboolean
gtk_tree_view_has_special_cell (GtkTreeView *tree_view)
{
  GList *list;

  for (list = tree_view->priv->columns; list; list = list->next)
    {
      if (!((GtkTreeViewColumn *)list->data)->visible)
	continue;
      if (_gtk_tree_view_column_count_special_cells (list->data))
	return TRUE;
    }

  return FALSE;
}

static void
column_sizing_notify (GObject    *object,
                      GParamSpec *pspec,
                      gpointer    data)
{
  GtkTreeViewColumn *c = GTK_TREE_VIEW_COLUMN (object);

  if (gtk_tree_view_column_get_sizing (c) != GTK_TREE_VIEW_COLUMN_FIXED)
    /* disable fixed height mode */
    g_object_set (data, "fixed-height-mode", FALSE, NULL);
}

/**
 * gtk_tree_view_set_fixed_height_mode:
 * @tree_view: a #GtkTreeView 
 * @enable: %TRUE to enable fixed height mode
 * 
 * Enables or disables the fixed height mode of @tree_view. 
 * Fixed height mode speeds up #GtkTreeView by assuming that all 
 * rows have the same height. 
 * Only enable this option if all rows are the same height and all
 * columns are of type %GTK_TREE_VIEW_COLUMN_FIXED.
 *
 * Since: 2.6 
 **/
void
gtk_tree_view_set_fixed_height_mode (GtkTreeView *tree_view,
                                     gboolean     enable)
{
  GList *l;
  
  enable = enable != FALSE;

  if (enable == tree_view->priv->fixed_height_mode)
    return;

  if (!enable)
    {
      tree_view->priv->fixed_height_mode = 0;
      tree_view->priv->fixed_height = -1;

      /* force a revalidation */
      install_presize_handler (tree_view);
    }
  else 
    {
      /* make sure all columns are of type FIXED */
      for (l = tree_view->priv->columns; l; l = l->next)
	{
	  GtkTreeViewColumn *c = l->data;
	  
	  g_return_if_fail (gtk_tree_view_column_get_sizing (c) == GTK_TREE_VIEW_COLUMN_FIXED);
	}
      
      /* yes, we really have to do this is in a separate loop */
      for (l = tree_view->priv->columns; l; l = l->next)
	g_signal_connect (l->data, "notify::sizing",
			  G_CALLBACK (column_sizing_notify), tree_view);
      
      tree_view->priv->fixed_height_mode = 1;
      tree_view->priv->fixed_height = -1;
      
      if (tree_view->priv->tree)
	initialize_fixed_height_mode (tree_view);
    }

  g_object_notify (G_OBJECT (tree_view), "fixed-height-mode");
}

/**
 * gtk_tree_view_get_fixed_height_mode:
 * @tree_view: a #GtkTreeView
 * 
 * Returns whether fixed height mode is turned on for @tree_view.
 * 
 * Return value: %TRUE if @tree_view is in fixed height mode
 * 
 * Since: 2.6
 **/
gboolean
gtk_tree_view_get_fixed_height_mode (GtkTreeView *tree_view)
{
  return tree_view->priv->fixed_height_mode;
}

/* Returns TRUE if the focus is within the headers, after the focus operation is
 * done
 */
static gboolean
gtk_tree_view_header_focus (GtkTreeView      *tree_view,
			    GtkDirectionType  dir,
			    gboolean          clamp_column_visible)
{
  GtkWidget *focus_child;

  GList *last_column, *first_column;
  GList *tmp_list;
  gboolean rtl;

  if (! GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_HEADERS_VISIBLE))
    return FALSE;

  focus_child = GTK_CONTAINER (tree_view)->focus_child;

  first_column = tree_view->priv->columns;
  while (first_column)
    {
      if (gtk_widget_get_can_focus (GTK_TREE_VIEW_COLUMN (first_column->data)->button) &&
	  GTK_TREE_VIEW_COLUMN (first_column->data)->visible &&
	  (GTK_TREE_VIEW_COLUMN (first_column->data)->clickable ||
	   GTK_TREE_VIEW_COLUMN (first_column->data)->reorderable))
	break;
      first_column = first_column->next;
    }

  /* No headers are visible, or are focusable.  We can't focus in or out.
   */
  if (first_column == NULL)
    return FALSE;

  last_column = g_list_last (tree_view->priv->columns);
  while (last_column)
    {
      if (gtk_widget_get_can_focus (GTK_TREE_VIEW_COLUMN (last_column->data)->button) &&
	  GTK_TREE_VIEW_COLUMN (last_column->data)->visible &&
	  (GTK_TREE_VIEW_COLUMN (last_column->data)->clickable ||
	   GTK_TREE_VIEW_COLUMN (last_column->data)->reorderable))
	break;
      last_column = last_column->prev;
    }


  rtl = (gtk_widget_get_direction (GTK_WIDGET (tree_view)) == GTK_TEXT_DIR_RTL);

  switch (dir)
    {
    case GTK_DIR_TAB_BACKWARD:
    case GTK_DIR_TAB_FORWARD:
    case GTK_DIR_UP:
    case GTK_DIR_DOWN:
      if (focus_child == NULL)
	{
	  if (tree_view->priv->focus_column != NULL &&
              gtk_widget_get_can_focus (tree_view->priv->focus_column->button))
	    focus_child = tree_view->priv->focus_column->button;
	  else
	    focus_child = GTK_TREE_VIEW_COLUMN (first_column->data)->button;
	  gtk_widget_grab_focus (focus_child);
	  break;
	}
      return FALSE;

    case GTK_DIR_LEFT:
    case GTK_DIR_RIGHT:
      if (focus_child == NULL)
	{
	  if (tree_view->priv->focus_column != NULL)
	    focus_child = tree_view->priv->focus_column->button;
	  else if (dir == GTK_DIR_LEFT)
	    focus_child = GTK_TREE_VIEW_COLUMN (last_column->data)->button;
	  else
	    focus_child = GTK_TREE_VIEW_COLUMN (first_column->data)->button;
	  gtk_widget_grab_focus (focus_child);
	  break;
	}

      if (gtk_widget_child_focus (focus_child, dir))
	{
	  /* The focus moves inside the button. */
	  /* This is probably a great example of bad UI */
	  break;
	}

      /* We need to move the focus among the row of buttons. */
      for (tmp_list = tree_view->priv->columns; tmp_list; tmp_list = tmp_list->next)
	if (GTK_TREE_VIEW_COLUMN (tmp_list->data)->button == focus_child)
	  break;

      if ((tmp_list == first_column && dir == (rtl ? GTK_DIR_RIGHT : GTK_DIR_LEFT))
	  || (tmp_list == last_column && dir == (rtl ? GTK_DIR_LEFT : GTK_DIR_RIGHT)))
        {
	  gtk_widget_error_bell (GTK_WIDGET (tree_view));
	  break;
	}

      while (tmp_list)
	{
	  GtkTreeViewColumn *column;

	  if (dir == (rtl ? GTK_DIR_LEFT : GTK_DIR_RIGHT))
	    tmp_list = tmp_list->next;
	  else
	    tmp_list = tmp_list->prev;

	  if (tmp_list == NULL)
	    {
	      g_warning ("Internal button not found");
	      break;
	    }
	  column = tmp_list->data;
	  if (column->button &&
	      column->visible &&
	      gtk_widget_get_can_focus (column->button))
	    {
	      focus_child = column->button;
	      gtk_widget_grab_focus (column->button);
	      break;
	    }
	}
      break;
    default:
      g_assert_not_reached ();
      break;
    }

  /* if focus child is non-null, we assume it's been set to the current focus child
   */
  if (focus_child)
    {
      for (tmp_list = tree_view->priv->columns; tmp_list; tmp_list = tmp_list->next)
	if (GTK_TREE_VIEW_COLUMN (tmp_list->data)->button == focus_child)
	  {
	    tree_view->priv->focus_column = GTK_TREE_VIEW_COLUMN (tmp_list->data);
	    break;
	  }

      if (clamp_column_visible)
        {
	  gtk_tree_view_clamp_column_visible (tree_view,
					      tree_view->priv->focus_column,
					      FALSE);
	}
    }

  return (focus_child != NULL);
}

/* This function returns in 'path' the first focusable path, if the given path
 * is already focusable, it's the returned one.
 */
static gboolean
search_first_focusable_path (GtkTreeView  *tree_view,
			     GtkTreePath **path,
			     gboolean      search_forward,
			     GtkRBTree   **new_tree,
			     GtkRBNode   **new_node)
{
  GtkRBTree *tree = NULL;
  GtkRBNode *node = NULL;

  if (!path || !*path)
    return FALSE;

  _gtk_tree_view_find_node (tree_view, *path, &tree, &node);

  if (!tree || !node)
    return FALSE;

  while (node && row_is_separator (tree_view, NULL, *path))
    {
      if (search_forward)
	_gtk_rbtree_next_full (tree, node, &tree, &node);
      else
	_gtk_rbtree_prev_full (tree, node, &tree, &node);

      if (*path)
	gtk_tree_path_free (*path);

      if (node)
	*path = _gtk_tree_view_find_path (tree_view, tree, node);
      else
	*path = NULL;
    }

  if (new_tree)
    *new_tree = tree;

  if (new_node)
    *new_node = node;

  return (*path != NULL);
}

static gint
gtk_tree_view_focus (GtkWidget        *widget,
		     GtkDirectionType  direction)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (widget);
  GtkContainer *container = GTK_CONTAINER (widget);
  GtkWidget *focus_child;

  if (!gtk_widget_is_sensitive (widget) || !gtk_widget_get_can_focus (widget))
    return FALSE;

  focus_child = container->focus_child;

  gtk_tree_view_stop_editing (GTK_TREE_VIEW (widget), FALSE);
  /* Case 1.  Headers currently have focus. */
  if (focus_child)
    {
      switch (direction)
	{
	case GTK_DIR_LEFT:
	case GTK_DIR_RIGHT:
	  gtk_tree_view_header_focus (tree_view, direction, TRUE);
	  return TRUE;
	case GTK_DIR_TAB_BACKWARD:
	case GTK_DIR_UP:
	  return FALSE;
	case GTK_DIR_TAB_FORWARD:
	case GTK_DIR_DOWN:
	  gtk_widget_grab_focus (widget);
	  return TRUE;
	default:
	  g_assert_not_reached ();
	  return FALSE;
	}
    }

  /* Case 2. We don't have focus at all. */
  if (!gtk_widget_has_focus (widget))
    {
      gtk_widget_grab_focus (widget);
      return TRUE;
    }

  /* Case 3. We have focus already. */
  if (direction == GTK_DIR_TAB_BACKWARD)
    return (gtk_tree_view_header_focus (tree_view, direction, FALSE));
  else if (direction == GTK_DIR_TAB_FORWARD)
    return FALSE;

  /* Other directions caught by the keybindings */
  gtk_widget_grab_focus (widget);
  return TRUE;
}

static void
gtk_tree_view_grab_focus (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (gtk_tree_view_parent_class)->grab_focus (widget);

  gtk_tree_view_focus_to_cursor (GTK_TREE_VIEW (widget));
}

static void
gtk_tree_view_style_set (GtkWidget *widget,
			 GtkStyle *previous_style)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (widget);
  GList *list;
  GtkTreeViewColumn *column;

  if (gtk_widget_get_realized (widget))
    {
      gdk_window_set_back_pixmap (widget->window, NULL, FALSE);
      gdk_window_set_background (tree_view->priv->bin_window, &widget->style->base[widget->state]);
      gtk_style_set_background (widget->style, tree_view->priv->header_window, GTK_STATE_NORMAL);

      gtk_tree_view_set_grid_lines (tree_view, tree_view->priv->grid_lines);
      gtk_tree_view_set_enable_tree_lines (tree_view, tree_view->priv->tree_lines_enabled);
    }

  gtk_widget_style_get (widget,
			"expander-size", &tree_view->priv->expander_size,
			NULL);
  tree_view->priv->expander_size += EXPANDER_EXTRA_PADDING;

  for (list = tree_view->priv->columns; list; list = list->next)
    {
      column = list->data;
      _gtk_tree_view_column_cell_set_dirty (column, TRUE);
    }

  tree_view->priv->fixed_height = -1;
  _gtk_rbtree_mark_invalid (tree_view->priv->tree);

  gtk_widget_queue_resize (widget);
}


static void
gtk_tree_view_set_focus_child (GtkContainer *container,
			       GtkWidget    *child)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (container);
  GList *list;

  for (list = tree_view->priv->columns; list; list = list->next)
    {
      if (GTK_TREE_VIEW_COLUMN (list->data)->button == child)
	{
	  tree_view->priv->focus_column = GTK_TREE_VIEW_COLUMN (list->data);
	  break;
	}
    }

  GTK_CONTAINER_CLASS (gtk_tree_view_parent_class)->set_focus_child (container, child);
}

static void
gtk_tree_view_set_adjustments (GtkTreeView   *tree_view,
			       GtkAdjustment *hadj,
			       GtkAdjustment *vadj)
{
  gboolean need_adjust = FALSE;

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  if (hadj)
    g_return_if_fail (GTK_IS_ADJUSTMENT (hadj));
  else
    hadj = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
  if (vadj)
    g_return_if_fail (GTK_IS_ADJUSTMENT (vadj));
  else
    vadj = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0));

  if (tree_view->priv->hadjustment && (tree_view->priv->hadjustment != hadj))
    {
      g_signal_handlers_disconnect_by_func (tree_view->priv->hadjustment,
					    gtk_tree_view_adjustment_changed,
					    tree_view);
      g_object_unref (tree_view->priv->hadjustment);
    }

  if (tree_view->priv->vadjustment && (tree_view->priv->vadjustment != vadj))
    {
      g_signal_handlers_disconnect_by_func (tree_view->priv->vadjustment,
					    gtk_tree_view_adjustment_changed,
					    tree_view);
      g_object_unref (tree_view->priv->vadjustment);
    }

  if (tree_view->priv->hadjustment != hadj)
    {
      tree_view->priv->hadjustment = hadj;
      g_object_ref_sink (tree_view->priv->hadjustment);

      g_signal_connect (tree_view->priv->hadjustment, "value-changed",
			G_CALLBACK (gtk_tree_view_adjustment_changed),
			tree_view);
      need_adjust = TRUE;
    }

  if (tree_view->priv->vadjustment != vadj)
    {
      tree_view->priv->vadjustment = vadj;
      g_object_ref_sink (tree_view->priv->vadjustment);

      g_signal_connect (tree_view->priv->vadjustment, "value-changed",
			G_CALLBACK (gtk_tree_view_adjustment_changed),
			tree_view);
      need_adjust = TRUE;
    }

  if (need_adjust)
    gtk_tree_view_adjustment_changed (NULL, tree_view);
}


static gboolean
gtk_tree_view_real_move_cursor (GtkTreeView       *tree_view,
				GtkMovementStep    step,
				gint               count)
{
  GdkModifierType state;

  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), FALSE);
  g_return_val_if_fail (step == GTK_MOVEMENT_LOGICAL_POSITIONS ||
			step == GTK_MOVEMENT_VISUAL_POSITIONS ||
			step == GTK_MOVEMENT_DISPLAY_LINES ||
			step == GTK_MOVEMENT_PAGES ||
			step == GTK_MOVEMENT_BUFFER_ENDS, FALSE);

  if (tree_view->priv->tree == NULL)
    return FALSE;
  if (!gtk_widget_has_focus (GTK_WIDGET (tree_view)))
    return FALSE;

  gtk_tree_view_stop_editing (tree_view, FALSE);
  GTK_TREE_VIEW_SET_FLAG (tree_view, GTK_TREE_VIEW_DRAW_KEYFOCUS);
  gtk_widget_grab_focus (GTK_WIDGET (tree_view));

  if (gtk_get_current_event_state (&state))
    {
      if ((state & GTK_MODIFY_SELECTION_MOD_MASK) == GTK_MODIFY_SELECTION_MOD_MASK)
        tree_view->priv->modify_selection_pressed = TRUE;
      if ((state & GTK_EXTEND_SELECTION_MOD_MASK) == GTK_EXTEND_SELECTION_MOD_MASK)
        tree_view->priv->extend_selection_pressed = TRUE;
    }
  /* else we assume not pressed */

  switch (step)
    {
      /* currently we make no distinction.  When we go bi-di, we need to */
    case GTK_MOVEMENT_LOGICAL_POSITIONS:
    case GTK_MOVEMENT_VISUAL_POSITIONS:
      gtk_tree_view_move_cursor_left_right (tree_view, count);
      break;
    case GTK_MOVEMENT_DISPLAY_LINES:
      gtk_tree_view_move_cursor_up_down (tree_view, count);
      break;
    case GTK_MOVEMENT_PAGES:
      gtk_tree_view_move_cursor_page_up_down (tree_view, count);
      break;
    case GTK_MOVEMENT_BUFFER_ENDS:
      gtk_tree_view_move_cursor_start_end (tree_view, count);
      break;
    default:
      g_assert_not_reached ();
    }

  tree_view->priv->modify_selection_pressed = FALSE;
  tree_view->priv->extend_selection_pressed = FALSE;

  return TRUE;
}

static void
gtk_tree_view_put (GtkTreeView *tree_view,
		   GtkWidget   *child_widget,
		   /* in bin_window coordinates */
		   gint         x,
		   gint         y,
		   gint         width,
		   gint         height)
{
  GtkTreeViewChild *child;
  
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (GTK_IS_WIDGET (child_widget));

  child = g_slice_new (GtkTreeViewChild);

  child->widget = child_widget;
  child->x = x;
  child->y = y;
  child->width = width;
  child->height = height;

  tree_view->priv->children = g_list_append (tree_view->priv->children, child);

  if (gtk_widget_get_realized (GTK_WIDGET (tree_view)))
    gtk_widget_set_parent_window (child->widget, tree_view->priv->bin_window);
  
  gtk_widget_set_parent (child_widget, GTK_WIDGET (tree_view));
}

void
_gtk_tree_view_child_move_resize (GtkTreeView *tree_view,
				  GtkWidget   *widget,
				  /* in tree coordinates */
				  gint         x,
				  gint         y,
				  gint         width,
				  gint         height)
{
  GtkTreeViewChild *child = NULL;
  GList *list;
  GdkRectangle allocation;

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  for (list = tree_view->priv->children; list; list = list->next)
    {
      if (((GtkTreeViewChild *)list->data)->widget == widget)
	{
	  child = list->data;
	  break;
	}
    }
  if (child == NULL)
    return;

  allocation.x = child->x = x;
  allocation.y = child->y = y;
  allocation.width = child->width = width;
  allocation.height = child->height = height;

  if (gtk_widget_get_realized (widget))
    gtk_widget_size_allocate (widget, &allocation);
}


/* TreeModel Callbacks
 */

static void
gtk_tree_view_row_changed (GtkTreeModel *model,
			   GtkTreePath  *path,
			   GtkTreeIter  *iter,
			   gpointer      data)
{
  GtkTreeView *tree_view = (GtkTreeView *)data;
  GtkRBTree *tree;
  GtkRBNode *node;
  gboolean free_path = FALSE;
  GList *list;
  GtkTreePath *cursor_path;

  g_return_if_fail (path != NULL || iter != NULL);

  if (tree_view->priv->cursor != NULL)
    cursor_path = gtk_tree_row_reference_get_path (tree_view->priv->cursor);
  else
    cursor_path = NULL;

  if (tree_view->priv->edited_column &&
      (cursor_path == NULL || gtk_tree_path_compare (cursor_path, path) == 0))
    gtk_tree_view_stop_editing (tree_view, TRUE);

  if (cursor_path != NULL)
    gtk_tree_path_free (cursor_path);

  if (path == NULL)
    {
      path = gtk_tree_model_get_path (model, iter);
      free_path = TRUE;
    }
  else if (iter == NULL)
    gtk_tree_model_get_iter (model, iter, path);

  if (_gtk_tree_view_find_node (tree_view,
				path,
				&tree,
				&node))
    /* We aren't actually showing the node */
    goto done;

  if (tree == NULL)
    goto done;

  if (tree_view->priv->fixed_height_mode
      && tree_view->priv->fixed_height >= 0)
    {
      _gtk_rbtree_node_set_height (tree, node, tree_view->priv->fixed_height);
      if (gtk_widget_get_realized (GTK_WIDGET (tree_view)))
	gtk_tree_view_node_queue_redraw (tree_view, tree, node);
    }
  else
    {
      _gtk_rbtree_node_mark_invalid (tree, node);
      for (list = tree_view->priv->columns; list; list = list->next)
        {
          GtkTreeViewColumn *column;

          column = list->data;
          if (! column->visible)
            continue;

          if (column->column_type == GTK_TREE_VIEW_COLUMN_AUTOSIZE)
            {
              _gtk_tree_view_column_cell_set_dirty (column, TRUE);
            }
        }
    }

 done:
  if (!tree_view->priv->fixed_height_mode &&
      gtk_widget_get_realized (GTK_WIDGET (tree_view)))
    install_presize_handler (tree_view);
  if (free_path)
    gtk_tree_path_free (path);
}

static void
gtk_tree_view_row_inserted (GtkTreeModel *model,
			    GtkTreePath  *path,
			    GtkTreeIter  *iter,
			    gpointer      data)
{
  GtkTreeView *tree_view = (GtkTreeView *) data;
  gint *indices;
  GtkRBTree *tmptree, *tree;
  GtkRBNode *tmpnode = NULL;
  gint depth;
  gint i = 0;
  gint height;
  gboolean free_path = FALSE;
  gboolean node_visible = TRUE;

  g_return_if_fail (path != NULL || iter != NULL);

  if (tree_view->priv->fixed_height_mode
      && tree_view->priv->fixed_height >= 0)
    height = tree_view->priv->fixed_height;
  else
    height = 0;

  if (path == NULL)
    {
      path = gtk_tree_model_get_path (model, iter);
      free_path = TRUE;
    }
  else if (iter == NULL)
    gtk_tree_model_get_iter (model, iter, path);

  if (tree_view->priv->tree == NULL)
    tree_view->priv->tree = _gtk_rbtree_new ();

  tmptree = tree = tree_view->priv->tree;

  /* Update all row-references */
  gtk_tree_row_reference_inserted (G_OBJECT (data), path);
  depth = gtk_tree_path_get_depth (path);
  indices = gtk_tree_path_get_indices (path);

  /* First, find the parent tree */
  while (i < depth - 1)
    {
      if (tmptree == NULL)
	{
	  /* We aren't showing the node */
	  node_visible = FALSE;
          goto done;
	}

      tmpnode = _gtk_rbtree_find_count (tmptree, indices[i] + 1);
      if (tmpnode == NULL)
	{
	  g_warning ("A node was inserted with a parent that's not in the tree.\n" \
		     "This possibly means that a GtkTreeModel inserted a child node\n" \
		     "before the parent was inserted.");
          goto done;
	}
      else if (!GTK_RBNODE_FLAG_SET (tmpnode, GTK_RBNODE_IS_PARENT))
	{
          /* FIXME enforce correct behavior on model, probably */
	  /* In theory, the model should have emitted has_child_toggled here.  We
	   * try to catch it anyway, just to be safe, in case the model hasn't.
	   */
	  GtkTreePath *tmppath = _gtk_tree_view_find_path (tree_view,
							   tree,
							   tmpnode);
	  gtk_tree_view_row_has_child_toggled (model, tmppath, NULL, data);
	  gtk_tree_path_free (tmppath);
          goto done;
	}

      tmptree = tmpnode->children;
      tree = tmptree;
      i++;
    }

  if (tree == NULL)
    {
      node_visible = FALSE;
      goto done;
    }

  /* ref the node */
  gtk_tree_model_ref_node (tree_view->priv->model, iter);
  if (indices[depth - 1] == 0)
    {
      tmpnode = _gtk_rbtree_find_count (tree, 1);
      tmpnode = _gtk_rbtree_insert_before (tree, tmpnode, height, FALSE);
    }
  else
    {
      tmpnode = _gtk_rbtree_find_count (tree, indices[depth - 1]);
      tmpnode = _gtk_rbtree_insert_after (tree, tmpnode, height, FALSE);
    }

 done:
  if (height > 0)
    {
      if (tree)
        _gtk_rbtree_node_mark_valid (tree, tmpnode);

      if (node_visible && node_is_visible (tree_view, tree, tmpnode))
	gtk_widget_queue_resize (GTK_WIDGET (tree_view));
      else
	gtk_widget_queue_resize_no_redraw (GTK_WIDGET (tree_view));
    }
  else
    install_presize_handler (tree_view);
  if (free_path)
    gtk_tree_path_free (path);
}

static void
gtk_tree_view_row_has_child_toggled (GtkTreeModel *model,
				     GtkTreePath  *path,
				     GtkTreeIter  *iter,
				     gpointer      data)
{
  GtkTreeView *tree_view = (GtkTreeView *)data;
  GtkTreeIter real_iter;
  gboolean has_child;
  GtkRBTree *tree;
  GtkRBNode *node;
  gboolean free_path = FALSE;

  g_return_if_fail (path != NULL || iter != NULL);

  if (iter)
    real_iter = *iter;

  if (path == NULL)
    {
      path = gtk_tree_model_get_path (model, iter);
      free_path = TRUE;
    }
  else if (iter == NULL)
    gtk_tree_model_get_iter (model, &real_iter, path);

  if (_gtk_tree_view_find_node (tree_view,
				path,
				&tree,
				&node))
    /* We aren't actually showing the node */
    goto done;

  if (tree == NULL)
    goto done;

  has_child = gtk_tree_model_iter_has_child (model, &real_iter);
  /* Sanity check.
   */
  if (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_PARENT) == has_child)
    goto done;

  if (has_child)
    GTK_RBNODE_SET_FLAG (node, GTK_RBNODE_IS_PARENT);
  else
    GTK_RBNODE_UNSET_FLAG (node, GTK_RBNODE_IS_PARENT);

  if (has_child && GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_IS_LIST))
    {
      GTK_TREE_VIEW_UNSET_FLAG (tree_view, GTK_TREE_VIEW_IS_LIST);
      if (GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_SHOW_EXPANDERS))
	{
	  GList *list;

	  for (list = tree_view->priv->columns; list; list = list->next)
	    if (GTK_TREE_VIEW_COLUMN (list->data)->visible)
	      {
		GTK_TREE_VIEW_COLUMN (list->data)->dirty = TRUE;
		_gtk_tree_view_column_cell_set_dirty (GTK_TREE_VIEW_COLUMN (list->data), TRUE);
		break;
	      }
	}
      gtk_widget_queue_resize (GTK_WIDGET (tree_view));
    }
  else
    {
      _gtk_tree_view_queue_draw_node (tree_view, tree, node, NULL);
    }

 done:
  if (free_path)
    gtk_tree_path_free (path);
}

static void
count_children_helper (GtkRBTree *tree,
		       GtkRBNode *node,
		       gpointer   data)
{
  if (node->children)
    _gtk_rbtree_traverse (node->children, node->children->root, G_POST_ORDER, count_children_helper, data);
  (*((gint *)data))++;
}

static void
check_selection_helper (GtkRBTree *tree,
                        GtkRBNode *node,
                        gpointer   data)
{
  gint *value = (gint *)data;

  *value = GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_SELECTED);

  if (node->children && !*value)
    _gtk_rbtree_traverse (node->children, node->children->root, G_POST_ORDER, check_selection_helper, data);
}

static void
gtk_tree_view_row_deleted (GtkTreeModel *model,
			   GtkTreePath  *path,
			   gpointer      data)
{
  GtkTreeView *tree_view = (GtkTreeView *)data;
  GtkRBTree *tree;
  GtkRBNode *node;
  GList *list;
  gint selection_changed = FALSE;

  g_return_if_fail (path != NULL);

  gtk_tree_row_reference_deleted (G_OBJECT (data), path);

  if (_gtk_tree_view_find_node (tree_view, path, &tree, &node))
    return;

  if (tree == NULL)
    return;

  /* check if the selection has been changed */
  _gtk_rbtree_traverse (tree, node, G_POST_ORDER,
                        check_selection_helper, &selection_changed);

  for (list = tree_view->priv->columns; list; list = list->next)
    if (((GtkTreeViewColumn *)list->data)->visible &&
	((GtkTreeViewColumn *)list->data)->column_type == GTK_TREE_VIEW_COLUMN_AUTOSIZE)
      _gtk_tree_view_column_cell_set_dirty ((GtkTreeViewColumn *)list->data, TRUE);

  /* Ensure we don't have a dangling pointer to a dead node */
  ensure_unprelighted (tree_view);

  /* Cancel editting if we've started */
  gtk_tree_view_stop_editing (tree_view, TRUE);

  /* If we have a node expanded/collapsed timeout, remove it */
  remove_expand_collapse_timeout (tree_view);

  if (tree_view->priv->destroy_count_func)
    {
      gint child_count = 0;
      if (node->children)
	_gtk_rbtree_traverse (node->children, node->children->root, G_POST_ORDER, count_children_helper, &child_count);
      tree_view->priv->destroy_count_func (tree_view, path, child_count, tree_view->priv->destroy_count_data);
    }

  if (tree->root->count == 1)
    {
      if (tree_view->priv->tree == tree)
	tree_view->priv->tree = NULL;

      _gtk_rbtree_remove (tree);
    }
  else
    {
      _gtk_rbtree_remove_node (tree, node);
    }

  if (! gtk_tree_row_reference_valid (tree_view->priv->top_row))
    {
      gtk_tree_row_reference_free (tree_view->priv->top_row);
      tree_view->priv->top_row = NULL;
    }

  install_scroll_sync_handler (tree_view);

  gtk_widget_queue_resize (GTK_WIDGET (tree_view));

  if (selection_changed)
    g_signal_emit_by_name (tree_view->priv->selection, "changed");
}

static void
gtk_tree_view_rows_reordered (GtkTreeModel *model,
			      GtkTreePath  *parent,
			      GtkTreeIter  *iter,
			      gint         *new_order,
			      gpointer      data)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (data);
  GtkRBTree *tree;
  GtkRBNode *node;
  gint len;

  len = gtk_tree_model_iter_n_children (model, iter);

  if (len < 2)
    return;

  gtk_tree_row_reference_reordered (G_OBJECT (data),
				    parent,
				    iter,
				    new_order);

  if (_gtk_tree_view_find_node (tree_view,
				parent,
				&tree,
				&node))
    return;

  /* We need to special case the parent path */
  if (tree == NULL)
    tree = tree_view->priv->tree;
  else
    tree = node->children;

  if (tree == NULL)
    return;

  if (tree_view->priv->edited_column)
    gtk_tree_view_stop_editing (tree_view, TRUE);

  /* we need to be unprelighted */
  ensure_unprelighted (tree_view);

  /* clear the timeout */
  cancel_arrow_animation (tree_view);
  
  _gtk_rbtree_reorder (tree, new_order, len);

  gtk_widget_queue_draw (GTK_WIDGET (tree_view));

  gtk_tree_view_dy_to_top_row (tree_view);
}


/* Internal tree functions
 */


static void
gtk_tree_view_get_background_xrange (GtkTreeView       *tree_view,
                                     GtkRBTree         *tree,
                                     GtkTreeViewColumn *column,
                                     gint              *x1,
                                     gint              *x2)
{
  GtkTreeViewColumn *tmp_column = NULL;
  gint total_width;
  GList *list;
  gboolean rtl;

  if (x1)
    *x1 = 0;

  if (x2)
    *x2 = 0;

  rtl = (gtk_widget_get_direction (GTK_WIDGET (tree_view)) == GTK_TEXT_DIR_RTL);

  total_width = 0;
  for (list = (rtl ? g_list_last (tree_view->priv->columns) : g_list_first (tree_view->priv->columns));
       list;
       list = (rtl ? list->prev : list->next))
    {
      tmp_column = list->data;

      if (tmp_column == column)
        break;

      if (tmp_column->visible)
        total_width += tmp_column->width;
    }

  if (tmp_column != column)
    {
      g_warning (G_STRLOC": passed-in column isn't in the tree");
      return;
    }

  if (x1)
    *x1 = total_width;

  if (x2)
    {
      if (column->visible)
        *x2 = total_width + column->width;
      else
        *x2 = total_width; /* width of 0 */
    }
}
static void
gtk_tree_view_get_arrow_xrange (GtkTreeView *tree_view,
				GtkRBTree   *tree,
                                gint        *x1,
                                gint        *x2)
{
  gint x_offset = 0;
  GList *list;
  GtkTreeViewColumn *tmp_column = NULL;
  gint total_width;
  gboolean indent_expanders;
  gboolean rtl;

  rtl = (gtk_widget_get_direction (GTK_WIDGET (tree_view)) == GTK_TEXT_DIR_RTL);

  total_width = 0;
  for (list = (rtl ? g_list_last (tree_view->priv->columns) : g_list_first (tree_view->priv->columns));
       list;
       list = (rtl ? list->prev : list->next))
    {
      tmp_column = list->data;

      if (gtk_tree_view_is_expander_column (tree_view, tmp_column))
        {
	  if (rtl)
	    x_offset = total_width + tmp_column->width - tree_view->priv->expander_size;
	  else
	    x_offset = total_width;
          break;
        }

      if (tmp_column->visible)
        total_width += tmp_column->width;
    }

  gtk_widget_style_get (GTK_WIDGET (tree_view),
			"indent-expanders", &indent_expanders,
			NULL);

  if (indent_expanders)
    {
      if (rtl)
	x_offset -= tree_view->priv->expander_size * _gtk_rbtree_get_depth (tree);
      else
	x_offset += tree_view->priv->expander_size * _gtk_rbtree_get_depth (tree);
    }

  *x1 = x_offset;
  
  if (tmp_column && tmp_column->visible)
    /* +1 because x2 isn't included in the range. */
    *x2 = *x1 + tree_view->priv->expander_size + 1;
  else
    *x2 = *x1;
}

static void
gtk_tree_view_build_tree (GtkTreeView *tree_view,
			  GtkRBTree   *tree,
			  GtkTreeIter *iter,
			  gint         depth,
			  gboolean     recurse)
{
  GtkRBNode *temp = NULL;
  GtkTreePath *path = NULL;
  gboolean is_list = GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_IS_LIST);

  do
    {
      gtk_tree_model_ref_node (tree_view->priv->model, iter);
      temp = _gtk_rbtree_insert_after (tree, temp, 0, FALSE);

      if (tree_view->priv->fixed_height > 0)
        {
          if (GTK_RBNODE_FLAG_SET (temp, GTK_RBNODE_INVALID))
	    {
              _gtk_rbtree_node_set_height (tree, temp, tree_view->priv->fixed_height);
	      _gtk_rbtree_node_mark_valid (tree, temp);
	    }
        }

      if (is_list)
        continue;

      if (recurse)
	{
	  GtkTreeIter child;

	  if (!path)
	    path = gtk_tree_model_get_path (tree_view->priv->model, iter);
	  else
	    gtk_tree_path_next (path);

	  if (gtk_tree_model_iter_children (tree_view->priv->model, &child, iter))
	    {
	      gboolean expand;

	      g_signal_emit (tree_view, tree_view_signals[TEST_EXPAND_ROW], 0, iter, path, &expand);

	      if (gtk_tree_model_iter_has_child (tree_view->priv->model, iter)
		  && !expand)
	        {
	          temp->children = _gtk_rbtree_new ();
	          temp->children->parent_tree = tree;
	          temp->children->parent_node = temp;
	          gtk_tree_view_build_tree (tree_view, temp->children, &child, depth + 1, recurse);
		}
	    }
	}

      if (gtk_tree_model_iter_has_child (tree_view->priv->model, iter))
	{
	  if ((temp->flags&GTK_RBNODE_IS_PARENT) != GTK_RBNODE_IS_PARENT)
	    temp->flags ^= GTK_RBNODE_IS_PARENT;
	}
    }
  while (gtk_tree_model_iter_next (tree_view->priv->model, iter));

  if (path)
    gtk_tree_path_free (path);
}

/* Make sure the node is visible vertically */
static void
gtk_tree_view_clamp_node_visible (GtkTreeView *tree_view,
				  GtkRBTree   *tree,
				  GtkRBNode   *node)
{
  gint node_dy, height;
  GtkTreePath *path = NULL;

  if (!gtk_widget_get_realized (GTK_WIDGET (tree_view)))
    return;

  /* just return if the node is visible, avoiding a costly expose */
  node_dy = _gtk_rbtree_node_find_offset (tree, node);
  height = ROW_HEIGHT (tree_view, GTK_RBNODE_GET_HEIGHT (node));
  if (! GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_INVALID)
      && node_dy >= tree_view->priv->vadjustment->value
      && node_dy + height <= (tree_view->priv->vadjustment->value
                              + tree_view->priv->vadjustment->page_size))
    return;

  path = _gtk_tree_view_find_path (tree_view, tree, node);
  if (path)
    {
      /* We process updates because we want to clear old selected items when we scroll.
       * if this is removed, we get a "selection streak" at the bottom. */
      gdk_window_process_updates (tree_view->priv->bin_window, TRUE);
      gtk_tree_view_scroll_to_cell (tree_view, path, NULL, FALSE, 0.0, 0.0);
      gtk_tree_path_free (path);
    }
}

static void
gtk_tree_view_clamp_column_visible (GtkTreeView       *tree_view,
				    GtkTreeViewColumn *column,
				    gboolean           focus_to_cell)
{
  gint x, width;

  if (column == NULL)
    return;

  x = column->button->allocation.x;
  width = column->button->allocation.width;

  if (width > tree_view->priv->hadjustment->page_size)
    {
      /* The column is larger than the horizontal page size.  If the
       * column has cells which can be focussed individually, then we make
       * sure the cell which gets focus is fully visible (if even the
       * focus cell is bigger than the page size, we make sure the
       * left-hand side of the cell is visible).
       *
       * If the column does not have those so-called special cells, we
       * make sure the left-hand side of the column is visible.
       */

      if (focus_to_cell && gtk_tree_view_has_special_cell (tree_view))
        {
	  GtkTreePath *cursor_path;
	  GdkRectangle background_area, cell_area, focus_area;

	  cursor_path = gtk_tree_row_reference_get_path (tree_view->priv->cursor);

	  gtk_tree_view_get_cell_area (tree_view,
				       cursor_path, column, &cell_area);
	  gtk_tree_view_get_background_area (tree_view,
					     cursor_path, column,
					     &background_area);

	  gtk_tree_path_free (cursor_path);

	  _gtk_tree_view_column_get_focus_area (column,
						&background_area,
						&cell_area,
						&focus_area);

	  x = focus_area.x;
	  width = focus_area.width;

	  if (width < tree_view->priv->hadjustment->page_size)
	    {
	      if ((tree_view->priv->hadjustment->value + tree_view->priv->hadjustment->page_size) < (x + width))
		gtk_adjustment_set_value (tree_view->priv->hadjustment,
					  x + width - tree_view->priv->hadjustment->page_size);
	      else if (tree_view->priv->hadjustment->value > x)
		gtk_adjustment_set_value (tree_view->priv->hadjustment, x);
	    }
	}

      gtk_adjustment_set_value (tree_view->priv->hadjustment,
				CLAMP (x,
				       tree_view->priv->hadjustment->lower,
				       tree_view->priv->hadjustment->upper
				       - tree_view->priv->hadjustment->page_size));
    }
  else
    {
      if ((tree_view->priv->hadjustment->value + tree_view->priv->hadjustment->page_size) < (x + width))
	  gtk_adjustment_set_value (tree_view->priv->hadjustment,
				    x + width - tree_view->priv->hadjustment->page_size);
      else if (tree_view->priv->hadjustment->value > x)
	gtk_adjustment_set_value (tree_view->priv->hadjustment, x);
  }
}

/* This function could be more efficient.  I'll optimize it if profiling seems
 * to imply that it is important */
GtkTreePath *
_gtk_tree_view_find_path (GtkTreeView *tree_view,
			  GtkRBTree   *tree,
			  GtkRBNode   *node)
{
  GtkTreePath *path;
  GtkRBTree *tmp_tree;
  GtkRBNode *tmp_node, *last;
  gint count;

  path = gtk_tree_path_new ();

  g_return_val_if_fail (node != NULL, path);
  g_return_val_if_fail (node != tree->nil, path);

  count = 1 + node->left->count;

  last = node;
  tmp_node = node->parent;
  tmp_tree = tree;
  while (tmp_tree)
    {
      while (tmp_node != tmp_tree->nil)
	{
	  if (tmp_node->right == last)
	    count += 1 + tmp_node->left->count;
	  last = tmp_node;
	  tmp_node = tmp_node->parent;
	}
      gtk_tree_path_prepend_index (path, count - 1);
      last = tmp_tree->parent_node;
      tmp_tree = tmp_tree->parent_tree;
      if (last)
	{
	  count = 1 + last->left->count;
	  tmp_node = last->parent;
	}
    }
  return path;
}

/* Returns TRUE if we ran out of tree before finding the path.  If the path is
 * invalid (ie. points to a node that's not in the tree), *tree and *node are
 * both set to NULL.
 */
gboolean
_gtk_tree_view_find_node (GtkTreeView  *tree_view,
			  GtkTreePath  *path,
			  GtkRBTree   **tree,
			  GtkRBNode   **node)
{
  GtkRBNode *tmpnode = NULL;
  GtkRBTree *tmptree = tree_view->priv->tree;
  gint *indices = gtk_tree_path_get_indices (path);
  gint depth = gtk_tree_path_get_depth (path);
  gint i = 0;

  *node = NULL;
  *tree = NULL;

  if (depth == 0 || tmptree == NULL)
    return FALSE;
  do
    {
      tmpnode = _gtk_rbtree_find_count (tmptree, indices[i] + 1);
      ++i;
      if (tmpnode == NULL)
	{
	  *tree = NULL;
	  *node = NULL;
	  return FALSE;
	}
      if (i >= depth)
	{
	  *tree = tmptree;
	  *node = tmpnode;
	  return FALSE;
	}
      *tree = tmptree;
      *node = tmpnode;
      tmptree = tmpnode->children;
      if (tmptree == NULL)
	return TRUE;
    }
  while (1);
}

static gboolean
gtk_tree_view_is_expander_column (GtkTreeView       *tree_view,
				  GtkTreeViewColumn *column)
{
  GList *list;

  if (GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_IS_LIST))
    return FALSE;

  if (tree_view->priv->expander_column != NULL)
    {
      if (tree_view->priv->expander_column == column)
	return TRUE;
      return FALSE;
    }
  else
    {
      for (list = tree_view->priv->columns;
	   list;
	   list = list->next)
	if (((GtkTreeViewColumn *)list->data)->visible)
	  break;
      if (list && list->data == column)
	return TRUE;
    }
  return FALSE;
}

static void
gtk_tree_view_add_move_binding (GtkBindingSet  *binding_set,
				guint           keyval,
				guint           modmask,
				gboolean        add_shifted_binding,
				GtkMovementStep step,
				gint            count)
{
  
  gtk_binding_entry_add_signal (binding_set, keyval, modmask,
                                "move-cursor", 2,
                                G_TYPE_ENUM, step,
                                G_TYPE_INT, count);

  if (add_shifted_binding)
    gtk_binding_entry_add_signal (binding_set, keyval, GDK_SHIFT_MASK,
				  "move-cursor", 2,
				  G_TYPE_ENUM, step,
				  G_TYPE_INT, count);

  if ((modmask & GDK_CONTROL_MASK) == GDK_CONTROL_MASK)
   return;

  gtk_binding_entry_add_signal (binding_set, keyval, GDK_CONTROL_MASK | GDK_SHIFT_MASK,
                                "move-cursor", 2,
                                G_TYPE_ENUM, step,
                                G_TYPE_INT, count);

  gtk_binding_entry_add_signal (binding_set, keyval, GDK_CONTROL_MASK,
                                "move-cursor", 2,
                                G_TYPE_ENUM, step,
                                G_TYPE_INT, count);
}

static gint
gtk_tree_view_unref_tree_helper (GtkTreeModel *model,
				 GtkTreeIter  *iter,
				 GtkRBTree    *tree,
				 GtkRBNode    *node)
{
  gint retval = FALSE;
  do
    {
      g_return_val_if_fail (node != NULL, FALSE);

      if (node->children)
	{
	  GtkTreeIter child;
	  GtkRBTree *new_tree;
	  GtkRBNode *new_node;

	  new_tree = node->children;
	  new_node = new_tree->root;

	  while (new_node && new_node->left != new_tree->nil)
	    new_node = new_node->left;

	  if (!gtk_tree_model_iter_children (model, &child, iter))
	    return FALSE;

	  retval = retval || gtk_tree_view_unref_tree_helper (model, &child, new_tree, new_node);
	}

      if (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_SELECTED))
	retval = TRUE;
      gtk_tree_model_unref_node (model, iter);
      node = _gtk_rbtree_next (tree, node);
    }
  while (gtk_tree_model_iter_next (model, iter));

  return retval;
}

static gint
gtk_tree_view_unref_and_check_selection_tree (GtkTreeView *tree_view,
					      GtkRBTree   *tree)
{
  GtkTreeIter iter;
  GtkTreePath *path;
  GtkRBNode *node;
  gint retval;

  if (!tree)
    return FALSE;

  node = tree->root;
  while (node && node->left != tree->nil)
    node = node->left;

  g_return_val_if_fail (node != NULL, FALSE);
  path = _gtk_tree_view_find_path (tree_view, tree, node);
  gtk_tree_model_get_iter (GTK_TREE_MODEL (tree_view->priv->model),
			   &iter, path);
  retval = gtk_tree_view_unref_tree_helper (GTK_TREE_MODEL (tree_view->priv->model), &iter, tree, node);
  gtk_tree_path_free (path);

  return retval;
}

static void
gtk_tree_view_set_column_drag_info (GtkTreeView       *tree_view,
				    GtkTreeViewColumn *column)
{
  GtkTreeViewColumn *left_column;
  GtkTreeViewColumn *cur_column = NULL;
  GtkTreeViewColumnReorder *reorder;
  gboolean rtl;
  GList *tmp_list;
  gint left;

  /* We want to precalculate the motion list such that we know what column slots
   * are available.
   */
  left_column = NULL;
  rtl = (gtk_widget_get_direction (GTK_WIDGET (tree_view)) == GTK_TEXT_DIR_RTL);

  /* First, identify all possible drop spots */
  if (rtl)
    tmp_list = g_list_last (tree_view->priv->columns);
  else
    tmp_list = g_list_first (tree_view->priv->columns);

  while (tmp_list)
    {
      cur_column = GTK_TREE_VIEW_COLUMN (tmp_list->data);
      tmp_list = rtl?g_list_previous (tmp_list):g_list_next (tmp_list);

      if (cur_column->visible == FALSE)
	continue;

      /* If it's not the column moving and func tells us to skip over the column, we continue. */
      if (left_column != column && cur_column != column &&
	  tree_view->priv->column_drop_func &&
	  ! tree_view->priv->column_drop_func (tree_view, column, left_column, cur_column, tree_view->priv->column_drop_func_data))
	{
	  left_column = cur_column;
	  continue;
	}
      reorder = g_slice_new0 (GtkTreeViewColumnReorder);
      reorder->left_column = left_column;
      left_column = reorder->right_column = cur_column;

      tree_view->priv->column_drag_info = g_list_append (tree_view->priv->column_drag_info, reorder);
    }

  /* Add the last one */
  if (tree_view->priv->column_drop_func == NULL ||
      ((left_column != column) &&
       tree_view->priv->column_drop_func (tree_view, column, left_column, NULL, tree_view->priv->column_drop_func_data)))
    {
      reorder = g_slice_new0 (GtkTreeViewColumnReorder);
      reorder->left_column = left_column;
      reorder->right_column = NULL;
      tree_view->priv->column_drag_info = g_list_append (tree_view->priv->column_drag_info, reorder);
    }

  /* We quickly check to see if it even makes sense to reorder columns. */
  /* If there is nothing that can be moved, then we return */

  if (tree_view->priv->column_drag_info == NULL)
    return;

  /* We know there are always 2 slots possbile, as you can always return column. */
  /* If that's all there is, return */
  if (tree_view->priv->column_drag_info->next == NULL || 
      (tree_view->priv->column_drag_info->next->next == NULL &&
       ((GtkTreeViewColumnReorder *)tree_view->priv->column_drag_info->data)->right_column == column &&
       ((GtkTreeViewColumnReorder *)tree_view->priv->column_drag_info->next->data)->left_column == column))
    {
      for (tmp_list = tree_view->priv->column_drag_info; tmp_list; tmp_list = tmp_list->next)
	g_slice_free (GtkTreeViewColumnReorder, tmp_list->data);
      g_list_free (tree_view->priv->column_drag_info);
      tree_view->priv->column_drag_info = NULL;
      return;
    }
  /* We fill in the ranges for the columns, now that we've isolated them */
  left = - TREE_VIEW_COLUMN_DRAG_DEAD_MULTIPLIER (tree_view);

  for (tmp_list = tree_view->priv->column_drag_info; tmp_list; tmp_list = tmp_list->next)
    {
      reorder = (GtkTreeViewColumnReorder *) tmp_list->data;

      reorder->left_align = left;
      if (tmp_list->next != NULL)
	{
	  g_assert (tmp_list->next->data);
	  left = reorder->right_align = (reorder->right_column->button->allocation.x +
					 reorder->right_column->button->allocation.width +
					 ((GtkTreeViewColumnReorder *)tmp_list->next->data)->left_column->button->allocation.x)/2;
	}
      else
	{
	  gint width;

          width = gdk_window_get_width (tree_view->priv->header_window);
	  reorder->right_align = width + TREE_VIEW_COLUMN_DRAG_DEAD_MULTIPLIER (tree_view);
	}
    }
}

void
_gtk_tree_view_column_start_drag (GtkTreeView       *tree_view,
				  GtkTreeViewColumn *column)
{
  GdkEvent *send_event;
  GtkAllocation allocation;
  gint x, y, width, height;
  GdkScreen *screen = gtk_widget_get_screen (GTK_WIDGET (tree_view));
  GdkDisplay *display = gdk_screen_get_display (screen);

  g_return_if_fail (tree_view->priv->column_drag_info == NULL);
  g_return_if_fail (tree_view->priv->cur_reorder == NULL);

  gtk_tree_view_set_column_drag_info (tree_view, column);

  if (tree_view->priv->column_drag_info == NULL)
    return;

  if (tree_view->priv->drag_window == NULL)
    {
      GdkWindowAttr attributes;
      guint attributes_mask;

      attributes.window_type = GDK_WINDOW_CHILD;
      attributes.wclass = GDK_INPUT_OUTPUT;
      attributes.x = column->button->allocation.x;
      attributes.y = 0;
      attributes.width = column->button->allocation.width;
      attributes.height = column->button->allocation.height;
      attributes.visual = gtk_widget_get_visual (GTK_WIDGET (tree_view));
      attributes.colormap = gtk_widget_get_colormap (GTK_WIDGET (tree_view));
      attributes.event_mask = GDK_VISIBILITY_NOTIFY_MASK | GDK_EXPOSURE_MASK | GDK_POINTER_MOTION_MASK;
      attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

      tree_view->priv->drag_window = gdk_window_new (tree_view->priv->bin_window,
						     &attributes,
						     attributes_mask);
      gdk_window_set_user_data (tree_view->priv->drag_window, GTK_WIDGET (tree_view));
    }

  gdk_display_pointer_ungrab (display, GDK_CURRENT_TIME);
  gdk_display_keyboard_ungrab (display, GDK_CURRENT_TIME);

  gtk_grab_remove (column->button);

  send_event = gdk_event_new (GDK_LEAVE_NOTIFY);
  send_event->crossing.send_event = TRUE;
  send_event->crossing.window = g_object_ref (GTK_BUTTON (column->button)->event_window);
  send_event->crossing.subwindow = NULL;
  send_event->crossing.detail = GDK_NOTIFY_ANCESTOR;
  send_event->crossing.time = GDK_CURRENT_TIME;

  gtk_propagate_event (column->button, send_event);
  gdk_event_free (send_event);

  send_event = gdk_event_new (GDK_BUTTON_RELEASE);
  send_event->button.window = g_object_ref (gdk_screen_get_root_window (screen));
  send_event->button.send_event = TRUE;
  send_event->button.time = GDK_CURRENT_TIME;
  send_event->button.x = -1;
  send_event->button.y = -1;
  send_event->button.axes = NULL;
  send_event->button.state = 0;
  send_event->button.button = 1;
  send_event->button.device = gdk_display_get_core_pointer (display);
  send_event->button.x_root = 0;
  send_event->button.y_root = 0;

  gtk_propagate_event (column->button, send_event);
  gdk_event_free (send_event);

  /* Kids, don't try this at home */
  g_object_ref (column->button);
  gtk_container_remove (GTK_CONTAINER (tree_view), column->button);
  gtk_widget_set_parent_window (column->button, tree_view->priv->drag_window);
  gtk_widget_set_parent (column->button, GTK_WIDGET (tree_view));
  g_object_unref (column->button);

  tree_view->priv->drag_column_x = column->button->allocation.x;
  allocation = column->button->allocation;
  allocation.x = 0;
  gtk_widget_size_allocate (column->button, &allocation);
  gtk_widget_set_parent_window (column->button, tree_view->priv->drag_window);

  tree_view->priv->drag_column = column;
  gdk_window_show (tree_view->priv->drag_window);

  gdk_window_get_origin (tree_view->priv->header_window, &x, &y);
  width = gdk_window_get_width (tree_view->priv->header_window);
  height = gdk_window_get_height (tree_view->priv->header_window);

  gtk_widget_grab_focus (GTK_WIDGET (tree_view));
  while (gtk_events_pending ())
    gtk_main_iteration ();

  GTK_TREE_VIEW_SET_FLAG (tree_view, GTK_TREE_VIEW_IN_COLUMN_DRAG);
  gdk_pointer_grab (tree_view->priv->drag_window,
		    FALSE,
		    GDK_POINTER_MOTION_MASK|GDK_BUTTON_RELEASE_MASK,
		    NULL, NULL, GDK_CURRENT_TIME);
  gdk_keyboard_grab (tree_view->priv->drag_window,
		     FALSE,
		     GDK_CURRENT_TIME);
}

static void
gtk_tree_view_queue_draw_arrow (GtkTreeView        *tree_view,
				GtkRBTree          *tree,
				GtkRBNode          *node,
				const GdkRectangle *clip_rect)
{
  GdkRectangle rect;

  if (!gtk_widget_get_realized (GTK_WIDGET (tree_view)))
    return;

  rect.x = 0;
  rect.width = MAX (tree_view->priv->expander_size, MAX (tree_view->priv->width, GTK_WIDGET (tree_view)->allocation.width));

  rect.y = BACKGROUND_FIRST_PIXEL (tree_view, tree, node);
  rect.height = ROW_HEIGHT (tree_view, BACKGROUND_HEIGHT (node));

  if (clip_rect)
    {
      GdkRectangle new_rect;

      gdk_rectangle_intersect (clip_rect, &rect, &new_rect);

      gdk_window_invalidate_rect (tree_view->priv->bin_window, &new_rect, TRUE);
    }
  else
    {
      gdk_window_invalidate_rect (tree_view->priv->bin_window, &rect, TRUE);
    }
}

void
_gtk_tree_view_queue_draw_node (GtkTreeView        *tree_view,
				GtkRBTree          *tree,
				GtkRBNode          *node,
				const GdkRectangle *clip_rect)
{
  GdkRectangle rect;

  if (!gtk_widget_get_realized (GTK_WIDGET (tree_view)))
    return;

  rect.x = 0;
  rect.width = MAX (tree_view->priv->width, GTK_WIDGET (tree_view)->allocation.width);

  rect.y = BACKGROUND_FIRST_PIXEL (tree_view, tree, node);
  rect.height = ROW_HEIGHT (tree_view, BACKGROUND_HEIGHT (node));

  if (clip_rect)
    {
      GdkRectangle new_rect;

      gdk_rectangle_intersect (clip_rect, &rect, &new_rect);

      gdk_window_invalidate_rect (tree_view->priv->bin_window, &new_rect, TRUE);
    }
  else
    {
      gdk_window_invalidate_rect (tree_view->priv->bin_window, &rect, TRUE);
    }
}

static void
gtk_tree_view_queue_draw_path (GtkTreeView        *tree_view,
                               GtkTreePath        *path,
                               const GdkRectangle *clip_rect)
{
  GtkRBTree *tree = NULL;
  GtkRBNode *node = NULL;

  _gtk_tree_view_find_node (tree_view, path, &tree, &node);

  if (tree)
    _gtk_tree_view_queue_draw_node (tree_view, tree, node, clip_rect);
}

/* x and y are the mouse position
 */
static void
gtk_tree_view_draw_arrow (GtkTreeView *tree_view,
                          GtkRBTree   *tree,
			  GtkRBNode   *node,
			  /* in bin_window coordinates */
			  gint         x,
			  gint         y)
{
  GdkRectangle area;
  GtkStateType state;
  GtkWidget *widget;
  gint x_offset = 0;
  gint x2;
  gint vertical_separator;
  gint expander_size;
  GtkExpanderStyle expander_style;

  widget = GTK_WIDGET (tree_view);

  gtk_widget_style_get (widget,
			"vertical-separator", &vertical_separator,
			NULL);
  expander_size = tree_view->priv->expander_size - EXPANDER_EXTRA_PADDING;

  if (! GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_PARENT))
    return;

  gtk_tree_view_get_arrow_xrange (tree_view, tree, &x_offset, &x2);

  area.x = x_offset;
  area.y = CELL_FIRST_PIXEL (tree_view, tree, node, vertical_separator);
  area.width = expander_size + 2;
  area.height = MAX (CELL_HEIGHT (node, vertical_separator), (expander_size - vertical_separator));

  if (gtk_widget_get_state (widget) == GTK_STATE_INSENSITIVE)
    {
      state = GTK_STATE_INSENSITIVE;
    }
  else if (node == tree_view->priv->button_pressed_node)
    {
      if (x >= area.x && x <= (area.x + area.width) &&
	  y >= area.y && y <= (area.y + area.height))
        state = GTK_STATE_ACTIVE;
      else
        state = GTK_STATE_NORMAL;
    }
  else
    {
      if (node == tree_view->priv->prelight_node &&
	  GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_ARROW_PRELIT))
	state = GTK_STATE_PRELIGHT;
      else
	state = GTK_STATE_NORMAL;
    }

  if (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_SEMI_EXPANDED))
    expander_style = GTK_EXPANDER_SEMI_EXPANDED;
  else if (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_SEMI_COLLAPSED))
    expander_style = GTK_EXPANDER_SEMI_COLLAPSED;
  else if (node->children != NULL)
    expander_style = GTK_EXPANDER_EXPANDED;
  else
    expander_style = GTK_EXPANDER_COLLAPSED;

  gtk_paint_expander (widget->style,
                      tree_view->priv->bin_window,
                      state,
                      &area,
                      widget,
                      "treeview",
		      area.x + area.width / 2,
		      area.y + area.height / 2,
		      expander_style);
}

static void
gtk_tree_view_focus_to_cursor (GtkTreeView *tree_view)

{
  GtkTreePath *cursor_path;

  if ((tree_view->priv->tree == NULL) ||
      (! gtk_widget_get_realized (GTK_WIDGET (tree_view))))
    return;

  cursor_path = NULL;
  if (tree_view->priv->cursor)
    cursor_path = gtk_tree_row_reference_get_path (tree_view->priv->cursor);

  if (cursor_path == NULL)
    {
      /* Consult the selection before defaulting to the
       * first focusable element
       */
      GList *selected_rows;
      GtkTreeModel *model;
      GtkTreeSelection *selection;

      selection = gtk_tree_view_get_selection (tree_view);
      selected_rows = gtk_tree_selection_get_selected_rows (selection, &model);

      if (selected_rows)
	{
          cursor_path = gtk_tree_path_copy((const GtkTreePath *)(selected_rows->data));
	  g_list_foreach (selected_rows, (GFunc)gtk_tree_path_free, NULL);
	  g_list_free (selected_rows);
        }
      else
	{
	  cursor_path = gtk_tree_path_new_first ();
	  search_first_focusable_path (tree_view, &cursor_path,
				       TRUE, NULL, NULL);
	}

      gtk_tree_row_reference_free (tree_view->priv->cursor);
      tree_view->priv->cursor = NULL;

      if (cursor_path)
	{
	  if (tree_view->priv->selection->type == GTK_SELECTION_MULTIPLE ||
	      tree_view->priv->selection->type == GTK_SELECTION_SINGLE)
	    gtk_tree_view_real_set_cursor (tree_view, cursor_path, FALSE, FALSE);
	  else
	    gtk_tree_view_real_set_cursor (tree_view, cursor_path, TRUE, FALSE);
	}
    }

  if (cursor_path)
    {
      GTK_TREE_VIEW_SET_FLAG (tree_view, GTK_TREE_VIEW_DRAW_KEYFOCUS);

      gtk_tree_view_queue_draw_path (tree_view, cursor_path, NULL);
      gtk_tree_path_free (cursor_path);

      if (tree_view->priv->focus_column == NULL)
	{
	  GList *list;
	  for (list = tree_view->priv->columns; list; list = list->next)
	    {
	      if (GTK_TREE_VIEW_COLUMN (list->data)->visible)
		{
		  tree_view->priv->focus_column = GTK_TREE_VIEW_COLUMN (list->data);
		  break;
		}
	    }
	}
    }
}

static void
gtk_tree_view_move_cursor_up_down (GtkTreeView *tree_view,
				   gint         count)
{
  gint selection_count;
  GtkRBTree *cursor_tree = NULL;
  GtkRBNode *cursor_node = NULL;
  GtkRBTree *new_cursor_tree = NULL;
  GtkRBNode *new_cursor_node = NULL;
  GtkTreePath *cursor_path = NULL;
  gboolean grab_focus = TRUE;
  gboolean selectable;

  if (! gtk_widget_has_focus (GTK_WIDGET (tree_view)))
    return;

  cursor_path = NULL;
  if (!gtk_tree_row_reference_valid (tree_view->priv->cursor))
    /* FIXME: we lost the cursor; should we get the first? */
    return;

  cursor_path = gtk_tree_row_reference_get_path (tree_view->priv->cursor);
  _gtk_tree_view_find_node (tree_view, cursor_path,
			    &cursor_tree, &cursor_node);

  if (cursor_tree == NULL)
    /* FIXME: we lost the cursor; should we get the first? */
    return;

  selection_count = gtk_tree_selection_count_selected_rows (tree_view->priv->selection);
  selectable = _gtk_tree_selection_row_is_selectable (tree_view->priv->selection,
						      cursor_node,
						      cursor_path);

  if (selection_count == 0
      && tree_view->priv->selection->type != GTK_SELECTION_NONE
      && !tree_view->priv->modify_selection_pressed
      && selectable)
    {
      /* Don't move the cursor, but just select the current node */
      new_cursor_tree = cursor_tree;
      new_cursor_node = cursor_node;
    }
  else
    {
      if (count == -1)
	_gtk_rbtree_prev_full (cursor_tree, cursor_node,
			       &new_cursor_tree, &new_cursor_node);
      else
	_gtk_rbtree_next_full (cursor_tree, cursor_node,
			       &new_cursor_tree, &new_cursor_node);
    }

  gtk_tree_path_free (cursor_path);

  if (new_cursor_node)
    {
      cursor_path = _gtk_tree_view_find_path (tree_view,
					      new_cursor_tree, new_cursor_node);

      search_first_focusable_path (tree_view, &cursor_path,
				   (count != -1),
				   &new_cursor_tree,
				   &new_cursor_node);

      if (cursor_path)
	gtk_tree_path_free (cursor_path);
    }

  /*
   * If the list has only one item and multi-selection is set then select
   * the row (if not yet selected).
   */
  if (tree_view->priv->selection->type == GTK_SELECTION_MULTIPLE &&
      new_cursor_node == NULL)
    {
      if (count == -1)
        _gtk_rbtree_next_full (cursor_tree, cursor_node,
    			       &new_cursor_tree, &new_cursor_node);
      else
        _gtk_rbtree_prev_full (cursor_tree, cursor_node,
			       &new_cursor_tree, &new_cursor_node);

      if (new_cursor_node == NULL
	  && !GTK_RBNODE_FLAG_SET (cursor_node, GTK_RBNODE_IS_SELECTED))
        {
          new_cursor_node = cursor_node;
          new_cursor_tree = cursor_tree;
        }
      else
        {
          new_cursor_node = NULL;
        }
    }

  if (new_cursor_node)
    {
      cursor_path = _gtk_tree_view_find_path (tree_view, new_cursor_tree, new_cursor_node);
      gtk_tree_view_real_set_cursor (tree_view, cursor_path, TRUE, TRUE);
      gtk_tree_path_free (cursor_path);
    }
  else
    {
      gtk_tree_view_clamp_node_visible (tree_view, cursor_tree, cursor_node);

      if (!tree_view->priv->extend_selection_pressed)
        {
          if (! gtk_widget_keynav_failed (GTK_WIDGET (tree_view),
                                          count < 0 ?
                                          GTK_DIR_UP : GTK_DIR_DOWN))
            {
              GtkWidget *toplevel = gtk_widget_get_toplevel (GTK_WIDGET (tree_view));

              if (toplevel)
                gtk_widget_child_focus (toplevel,
                                        count < 0 ?
                                        GTK_DIR_TAB_BACKWARD :
                                        GTK_DIR_TAB_FORWARD);

              grab_focus = FALSE;
            }
        }
      else
        {
          gtk_widget_error_bell (GTK_WIDGET (tree_view));
        }
    }

  if (grab_focus)
    gtk_widget_grab_focus (GTK_WIDGET (tree_view));
}

static void
gtk_tree_view_move_cursor_page_up_down (GtkTreeView *tree_view,
					gint         count)
{
  GtkRBTree *cursor_tree = NULL;
  GtkRBNode *cursor_node = NULL;
  GtkTreePath *old_cursor_path = NULL;
  GtkTreePath *cursor_path = NULL;
  GtkRBTree *start_cursor_tree = NULL;
  GtkRBNode *start_cursor_node = NULL;
  gint y;
  gint window_y;
  gint vertical_separator;

  if (!gtk_widget_has_focus (GTK_WIDGET (tree_view)))
    return;

  if (gtk_tree_row_reference_valid (tree_view->priv->cursor))
    old_cursor_path = gtk_tree_row_reference_get_path (tree_view->priv->cursor);
  else
    /* This is sorta weird.  Focus in should give us a cursor */
    return;

  gtk_widget_style_get (GTK_WIDGET (tree_view), "vertical-separator", &vertical_separator, NULL);
  _gtk_tree_view_find_node (tree_view, old_cursor_path,
			    &cursor_tree, &cursor_node);

  if (cursor_tree == NULL)
    {
      /* FIXME: we lost the cursor.  Should we try to get one? */
      gtk_tree_path_free (old_cursor_path);
      return;
    }
  g_return_if_fail (cursor_node != NULL);

  y = _gtk_rbtree_node_find_offset (cursor_tree, cursor_node);
  window_y = RBTREE_Y_TO_TREE_WINDOW_Y (tree_view, y);
  y += tree_view->priv->cursor_offset;
  y += count * (int)tree_view->priv->vadjustment->page_increment;
  y = CLAMP (y, (gint)tree_view->priv->vadjustment->lower,  (gint)tree_view->priv->vadjustment->upper - vertical_separator);

  if (y >= tree_view->priv->height)
    y = tree_view->priv->height - 1;

  tree_view->priv->cursor_offset =
    _gtk_rbtree_find_offset (tree_view->priv->tree, y,
			     &cursor_tree, &cursor_node);

  if (cursor_tree == NULL)
    {
      /* FIXME: we lost the cursor.  Should we try to get one? */
      gtk_tree_path_free (old_cursor_path);
      return;
    }

  if (tree_view->priv->cursor_offset > BACKGROUND_HEIGHT (cursor_node))
    {
      _gtk_rbtree_next_full (cursor_tree, cursor_node,
			     &cursor_tree, &cursor_node);
      tree_view->priv->cursor_offset -= BACKGROUND_HEIGHT (cursor_node);
    }

  y -= tree_view->priv->cursor_offset;
  cursor_path = _gtk_tree_view_find_path (tree_view, cursor_tree, cursor_node);

  start_cursor_tree = cursor_tree;
  start_cursor_node = cursor_node;

  if (! search_first_focusable_path (tree_view, &cursor_path,
				     (count != -1),
				     &cursor_tree, &cursor_node))
    {
      /* It looks like we reached the end of the view without finding
       * a focusable row.  We will step backwards to find the last
       * focusable row.
       */
      cursor_tree = start_cursor_tree;
      cursor_node = start_cursor_node;
      cursor_path = _gtk_tree_view_find_path (tree_view, cursor_tree, cursor_node);

      search_first_focusable_path (tree_view, &cursor_path,
				   (count == -1),
				   &cursor_tree, &cursor_node);
    }

  if (!cursor_path)
    goto cleanup;

  /* update y */
  y = _gtk_rbtree_node_find_offset (cursor_tree, cursor_node);

  gtk_tree_view_real_set_cursor (tree_view, cursor_path, TRUE, FALSE);

  y -= window_y;
  gtk_tree_view_scroll_to_point (tree_view, -1, y);
  gtk_tree_view_clamp_node_visible (tree_view, cursor_tree, cursor_node);
  _gtk_tree_view_queue_draw_node (tree_view, cursor_tree, cursor_node, NULL);

  if (!gtk_tree_path_compare (old_cursor_path, cursor_path))
    gtk_widget_error_bell (GTK_WIDGET (tree_view));

  gtk_widget_grab_focus (GTK_WIDGET (tree_view));

cleanup:
  gtk_tree_path_free (old_cursor_path);
  gtk_tree_path_free (cursor_path);
}

static void
gtk_tree_view_move_cursor_left_right (GtkTreeView *tree_view,
				      gint         count)
{
  GtkRBTree *cursor_tree = NULL;
  GtkRBNode *cursor_node = NULL;
  GtkTreePath *cursor_path = NULL;
  GtkTreeViewColumn *column;
  GtkTreeIter iter;
  GList *list;
  gboolean found_column = FALSE;
  gboolean rtl;

  rtl = (gtk_widget_get_direction (GTK_WIDGET (tree_view)) == GTK_TEXT_DIR_RTL);

  if (!gtk_widget_has_focus (GTK_WIDGET (tree_view)))
    return;

  if (gtk_tree_row_reference_valid (tree_view->priv->cursor))
    cursor_path = gtk_tree_row_reference_get_path (tree_view->priv->cursor);
  else
    return;

  _gtk_tree_view_find_node (tree_view, cursor_path, &cursor_tree, &cursor_node);
  if (cursor_tree == NULL)
    return;
  if (gtk_tree_model_get_iter (tree_view->priv->model, &iter, cursor_path) == FALSE)
    {
      gtk_tree_path_free (cursor_path);
      return;
    }
  gtk_tree_path_free (cursor_path);

  list = rtl ? g_list_last (tree_view->priv->columns) : g_list_first (tree_view->priv->columns);
  if (tree_view->priv->focus_column)
    {
      for (; list; list = (rtl ? list->prev : list->next))
	{
	  if (list->data == tree_view->priv->focus_column)
	    break;
	}
    }

  while (list)
    {
      gboolean left, right;

      column = list->data;
      if (column->visible == FALSE)
	goto loop_end;

      gtk_tree_view_column_cell_set_cell_data (column,
					       tree_view->priv->model,
					       &iter,
					       GTK_RBNODE_FLAG_SET (cursor_node, GTK_RBNODE_IS_PARENT),
					       cursor_node->children?TRUE:FALSE);

      if (rtl)
        {
	  right = list->prev ? TRUE : FALSE;
	  left = list->next ? TRUE : FALSE;
	}
      else
        {
	  left = list->prev ? TRUE : FALSE;
	  right = list->next ? TRUE : FALSE;
        }

      if (_gtk_tree_view_column_cell_focus (column, count, left, right))
	{
	  tree_view->priv->focus_column = column;
	  found_column = TRUE;
	  break;
	}
    loop_end:
      if (count == 1)
	list = rtl ? list->prev : list->next;
      else
	list = rtl ? list->next : list->prev;
    }

  if (found_column)
    {
      if (!gtk_tree_view_has_special_cell (tree_view))
	_gtk_tree_view_queue_draw_node (tree_view,
				        cursor_tree,
				        cursor_node,
				        NULL);
      g_signal_emit (tree_view, tree_view_signals[CURSOR_CHANGED], 0);
      gtk_widget_grab_focus (GTK_WIDGET (tree_view));
    }
  else
    {
      gtk_widget_error_bell (GTK_WIDGET (tree_view));
    }

  gtk_tree_view_clamp_column_visible (tree_view,
				      tree_view->priv->focus_column, TRUE);
}

static void
gtk_tree_view_move_cursor_start_end (GtkTreeView *tree_view,
				     gint         count)
{
  GtkRBTree *cursor_tree;
  GtkRBNode *cursor_node;
  GtkTreePath *path;
  GtkTreePath *old_path;

  if (!gtk_widget_has_focus (GTK_WIDGET (tree_view)))
    return;

  g_return_if_fail (tree_view->priv->tree != NULL);

  gtk_tree_view_get_cursor (tree_view, &old_path, NULL);

  cursor_tree = tree_view->priv->tree;
  cursor_node = cursor_tree->root;

  if (count == -1)
    {
      while (cursor_node && cursor_node->left != cursor_tree->nil)
	cursor_node = cursor_node->left;

      /* Now go forward to find the first focusable row. */
      path = _gtk_tree_view_find_path (tree_view, cursor_tree, cursor_node);
      search_first_focusable_path (tree_view, &path,
				   TRUE, &cursor_tree, &cursor_node);
    }
  else
    {
      do
	{
	  while (cursor_node && cursor_node->right != cursor_tree->nil)
	    cursor_node = cursor_node->right;
	  if (cursor_node->children == NULL)
	    break;

	  cursor_tree = cursor_node->children;
	  cursor_node = cursor_tree->root;
	}
      while (1);

      /* Now go backwards to find last focusable row. */
      path = _gtk_tree_view_find_path (tree_view, cursor_tree, cursor_node);
      search_first_focusable_path (tree_view, &path,
				   FALSE, &cursor_tree, &cursor_node);
    }

  if (!path)
    goto cleanup;

  if (gtk_tree_path_compare (old_path, path))
    {
      gtk_tree_view_real_set_cursor (tree_view, path, TRUE, TRUE);
      gtk_widget_grab_focus (GTK_WIDGET (tree_view));
    }
  else
    {
      gtk_widget_error_bell (GTK_WIDGET (tree_view));
    }

cleanup:
  gtk_tree_path_free (old_path);
  gtk_tree_path_free (path);
}

static gboolean
gtk_tree_view_real_select_all (GtkTreeView *tree_view)
{
  if (!gtk_widget_has_focus (GTK_WIDGET (tree_view)))
    return FALSE;

  if (tree_view->priv->selection->type != GTK_SELECTION_MULTIPLE)
    return FALSE;

  gtk_tree_selection_select_all (tree_view->priv->selection);

  return TRUE;
}

static gboolean
gtk_tree_view_real_unselect_all (GtkTreeView *tree_view)
{
  if (!gtk_widget_has_focus (GTK_WIDGET (tree_view)))
    return FALSE;

  if (tree_view->priv->selection->type != GTK_SELECTION_MULTIPLE)
    return FALSE;

  gtk_tree_selection_unselect_all (tree_view->priv->selection);

  return TRUE;
}

static gboolean
gtk_tree_view_real_select_cursor_row (GtkTreeView *tree_view,
				      gboolean     start_editing)
{
  GtkRBTree *new_tree = NULL;
  GtkRBNode *new_node = NULL;
  GtkRBTree *cursor_tree = NULL;
  GtkRBNode *cursor_node = NULL;
  GtkTreePath *cursor_path = NULL;
  GtkTreeSelectMode mode = 0;

  if (!gtk_widget_has_focus (GTK_WIDGET (tree_view)))
    return FALSE;

  if (tree_view->priv->cursor)
    cursor_path = gtk_tree_row_reference_get_path (tree_view->priv->cursor);

  if (cursor_path == NULL)
    return FALSE;

  _gtk_tree_view_find_node (tree_view, cursor_path,
			    &cursor_tree, &cursor_node);

  if (cursor_tree == NULL)
    {
      gtk_tree_path_free (cursor_path);
      return FALSE;
    }

  if (!tree_view->priv->extend_selection_pressed && start_editing &&
      tree_view->priv->focus_column)
    {
      if (gtk_tree_view_start_editing (tree_view, cursor_path))
	{
	  gtk_tree_path_free (cursor_path);
	  return TRUE;
	}
    }

  if (tree_view->priv->modify_selection_pressed)
    mode |= GTK_TREE_SELECT_MODE_TOGGLE;
  if (tree_view->priv->extend_selection_pressed)
    mode |= GTK_TREE_SELECT_MODE_EXTEND;

  _gtk_tree_selection_internal_select_node (tree_view->priv->selection,
					    cursor_node,
					    cursor_tree,
					    cursor_path,
                                            mode,
					    FALSE);

  /* We bail out if the original (tree, node) don't exist anymore after
   * handling the selection-changed callback.  We do return TRUE because
   * the key press has been handled at this point.
   */
  _gtk_tree_view_find_node (tree_view, cursor_path, &new_tree, &new_node);

  if (cursor_tree != new_tree || cursor_node != new_node)
    return FALSE;

  gtk_tree_view_clamp_node_visible (tree_view, cursor_tree, cursor_node);

  gtk_widget_grab_focus (GTK_WIDGET (tree_view));
  _gtk_tree_view_queue_draw_node (tree_view, cursor_tree, cursor_node, NULL);

  if (!tree_view->priv->extend_selection_pressed)
    gtk_tree_view_row_activated (tree_view, cursor_path,
                                 tree_view->priv->focus_column);
    
  gtk_tree_path_free (cursor_path);

  return TRUE;
}

static gboolean
gtk_tree_view_real_toggle_cursor_row (GtkTreeView *tree_view)
{
  GtkRBTree *new_tree = NULL;
  GtkRBNode *new_node = NULL;
  GtkRBTree *cursor_tree = NULL;
  GtkRBNode *cursor_node = NULL;
  GtkTreePath *cursor_path = NULL;

  if (!gtk_widget_has_focus (GTK_WIDGET (tree_view)))
    return FALSE;

  cursor_path = NULL;
  if (tree_view->priv->cursor)
    cursor_path = gtk_tree_row_reference_get_path (tree_view->priv->cursor);

  if (cursor_path == NULL)
    return FALSE;

  _gtk_tree_view_find_node (tree_view, cursor_path,
			    &cursor_tree, &cursor_node);
  if (cursor_tree == NULL)
    {
      gtk_tree_path_free (cursor_path);
      return FALSE;
    }

  _gtk_tree_selection_internal_select_node (tree_view->priv->selection,
					    cursor_node,
					    cursor_tree,
					    cursor_path,
                                            GTK_TREE_SELECT_MODE_TOGGLE,
					    FALSE);

  /* We bail out if the original (tree, node) don't exist anymore after
   * handling the selection-changed callback.  We do return TRUE because
   * the key press has been handled at this point.
   */
  _gtk_tree_view_find_node (tree_view, cursor_path, &new_tree, &new_node);

  if (cursor_tree != new_tree || cursor_node != new_node)
    return FALSE;

  gtk_tree_view_clamp_node_visible (tree_view, cursor_tree, cursor_node);

  gtk_widget_grab_focus (GTK_WIDGET (tree_view));
  gtk_tree_view_queue_draw_path (tree_view, cursor_path, NULL);
  gtk_tree_path_free (cursor_path);

  return TRUE;
}

static gboolean
gtk_tree_view_real_expand_collapse_cursor_row (GtkTreeView *tree_view,
					       gboolean     logical,
					       gboolean     expand,
					       gboolean     open_all)
{
  GtkTreePath *cursor_path = NULL;
  GtkRBTree *tree;
  GtkRBNode *node;

  if (!gtk_widget_has_focus (GTK_WIDGET (tree_view)))
    return FALSE;

  cursor_path = NULL;
  if (tree_view->priv->cursor)
    cursor_path = gtk_tree_row_reference_get_path (tree_view->priv->cursor);

  if (cursor_path == NULL)
    return FALSE;

  if (_gtk_tree_view_find_node (tree_view, cursor_path, &tree, &node))
    return FALSE;

  /* Don't handle the event if we aren't an expander */
  if (!((node->flags & GTK_RBNODE_IS_PARENT) == GTK_RBNODE_IS_PARENT))
    return FALSE;

  if (!logical
      && gtk_widget_get_direction (GTK_WIDGET (tree_view)) == GTK_TEXT_DIR_RTL)
    expand = !expand;

  if (expand)
    gtk_tree_view_real_expand_row (tree_view, cursor_path, tree, node, open_all, TRUE);
  else
    gtk_tree_view_real_collapse_row (tree_view, cursor_path, tree, node, TRUE);

  gtk_tree_path_free (cursor_path);

  return TRUE;
}

static gboolean
gtk_tree_view_real_select_cursor_parent (GtkTreeView *tree_view)
{
  GtkRBTree *cursor_tree = NULL;
  GtkRBNode *cursor_node = NULL;
  GtkTreePath *cursor_path = NULL;
  GdkModifierType state;

  if (!gtk_widget_has_focus (GTK_WIDGET (tree_view)))
    goto out;

  cursor_path = NULL;
  if (tree_view->priv->cursor)
    cursor_path = gtk_tree_row_reference_get_path (tree_view->priv->cursor);

  if (cursor_path == NULL)
    goto out;

  _gtk_tree_view_find_node (tree_view, cursor_path,
			    &cursor_tree, &cursor_node);
  if (cursor_tree == NULL)
    {
      gtk_tree_path_free (cursor_path);
      goto out;
    }

  if (cursor_tree->parent_node)
    {
      gtk_tree_view_queue_draw_path (tree_view, cursor_path, NULL);
      cursor_node = cursor_tree->parent_node;
      cursor_tree = cursor_tree->parent_tree;

      gtk_tree_path_up (cursor_path);

      if (gtk_get_current_event_state (&state))
	{
	  if ((state & GTK_MODIFY_SELECTION_MOD_MASK) == GTK_MODIFY_SELECTION_MOD_MASK)
	    tree_view->priv->modify_selection_pressed = TRUE;
	}

      gtk_tree_view_real_set_cursor (tree_view, cursor_path, TRUE, FALSE);
      gtk_tree_view_clamp_node_visible (tree_view, cursor_tree, cursor_node);

      gtk_widget_grab_focus (GTK_WIDGET (tree_view));
      gtk_tree_view_queue_draw_path (tree_view, cursor_path, NULL);
      gtk_tree_path_free (cursor_path);

      tree_view->priv->modify_selection_pressed = FALSE;

      return TRUE;
    }

 out:

  tree_view->priv->search_entry_avoid_unhandled_binding = TRUE;
  return FALSE;
}

static gboolean
gtk_tree_view_search_entry_flush_timeout (GtkTreeView *tree_view)
{
  gtk_tree_view_search_dialog_hide (tree_view->priv->search_window, tree_view);
  tree_view->priv->typeselect_flush_timeout = 0;

  return FALSE;
}

/* Cut and paste from gtkwindow.c */
static void
send_focus_change (GtkWidget *widget,
		   gboolean   in)
{
  GdkEvent *fevent = gdk_event_new (GDK_FOCUS_CHANGE);

  fevent->focus_change.type = GDK_FOCUS_CHANGE;
  fevent->focus_change.window = g_object_ref (gtk_widget_get_window (widget));
  fevent->focus_change.in = in;

  gtk_widget_send_focus_change (widget, fevent);

  gdk_event_free (fevent);
}

static void
gtk_tree_view_ensure_interactive_directory (GtkTreeView *tree_view)
{
  GtkWidget *frame, *vbox, *toplevel;
  GdkScreen *screen;

  if (tree_view->priv->search_custom_entry_set)
    return;

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (tree_view));
  screen = gtk_widget_get_screen (GTK_WIDGET (tree_view));

   if (tree_view->priv->search_window != NULL)
     {
       if (GTK_WINDOW (toplevel)->group)
	 gtk_window_group_add_window (GTK_WINDOW (toplevel)->group,
				      GTK_WINDOW (tree_view->priv->search_window));
       else if (GTK_WINDOW (tree_view->priv->search_window)->group)
	 gtk_window_group_remove_window (GTK_WINDOW (tree_view->priv->search_window)->group,
					 GTK_WINDOW (tree_view->priv->search_window));
       gtk_window_set_screen (GTK_WINDOW (tree_view->priv->search_window), screen);
       return;
     }
   
  tree_view->priv->search_window = gtk_window_new (GTK_WINDOW_POPUP);
  gtk_window_set_screen (GTK_WINDOW (tree_view->priv->search_window), screen);

  if (GTK_WINDOW (toplevel)->group)
    gtk_window_group_add_window (GTK_WINDOW (toplevel)->group,
				 GTK_WINDOW (tree_view->priv->search_window));

  gtk_window_set_type_hint (GTK_WINDOW (tree_view->priv->search_window),
			    GDK_WINDOW_TYPE_HINT_UTILITY);
  gtk_window_set_modal (GTK_WINDOW (tree_view->priv->search_window), TRUE);
  g_signal_connect (tree_view->priv->search_window, "delete-event",
		    G_CALLBACK (gtk_tree_view_search_delete_event),
		    tree_view);
  g_signal_connect (tree_view->priv->search_window, "key-press-event",
		    G_CALLBACK (gtk_tree_view_search_key_press_event),
		    tree_view);
  g_signal_connect (tree_view->priv->search_window, "button-press-event",
		    G_CALLBACK (gtk_tree_view_search_button_press_event),
		    tree_view);
  g_signal_connect (tree_view->priv->search_window, "scroll-event",
		    G_CALLBACK (gtk_tree_view_search_scroll_event),
		    tree_view);

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_ETCHED_IN);
  gtk_widget_show (frame);
  gtk_container_add (GTK_CONTAINER (tree_view->priv->search_window), frame);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox);
  gtk_container_add (GTK_CONTAINER (frame), vbox);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 3);

  /* add entry */
  tree_view->priv->search_entry = gtk_entry_new ();
  gtk_widget_show (tree_view->priv->search_entry);
  g_signal_connect (tree_view->priv->search_entry, "populate-popup",
		    G_CALLBACK (gtk_tree_view_search_disable_popdown),
		    tree_view);
  g_signal_connect (tree_view->priv->search_entry,
		    "activate", G_CALLBACK (gtk_tree_view_search_activate),
		    tree_view);
  g_signal_connect (GTK_ENTRY (tree_view->priv->search_entry)->im_context,
		    "preedit-changed",
		    G_CALLBACK (gtk_tree_view_search_preedit_changed),
		    tree_view);
  gtk_container_add (GTK_CONTAINER (vbox),
		     tree_view->priv->search_entry);

  gtk_widget_realize (tree_view->priv->search_entry);
}

/* Pops up the interactive search entry.  If keybinding is TRUE then the user
 * started this by typing the start_interactive_search keybinding.  Otherwise, it came from 
 */
static gboolean
gtk_tree_view_real_start_interactive_search (GtkTreeView *tree_view,
					     gboolean     keybinding)
{
  /* We only start interactive search if we have focus or the columns
   * have focus.  If one of our children have focus, we don't want to
   * start the search.
   */
  GList *list;
  gboolean found_focus = FALSE;
  GtkWidgetClass *entry_parent_class;
  
  if (!tree_view->priv->enable_search && !keybinding)
    return FALSE;

  if (tree_view->priv->search_custom_entry_set)
    return FALSE;

  if (tree_view->priv->search_window != NULL &&
      gtk_widget_get_visible (tree_view->priv->search_window))
    return TRUE;

  for (list = tree_view->priv->columns; list; list = list->next)
    {
      GtkTreeViewColumn *column;

      column = list->data;
      if (! column->visible)
	continue;

      if (gtk_widget_has_focus (column->button))
	{
	  found_focus = TRUE;
	  break;
	}
    }
  
  if (gtk_widget_has_focus (GTK_WIDGET (tree_view)))
    found_focus = TRUE;

  if (!found_focus)
    return FALSE;

  if (tree_view->priv->search_column < 0)
    return FALSE;

  gtk_tree_view_ensure_interactive_directory (tree_view);

  if (keybinding)
    gtk_entry_set_text (GTK_ENTRY (tree_view->priv->search_entry), "");

  /* done, show it */
  tree_view->priv->search_position_func (tree_view, tree_view->priv->search_window, tree_view->priv->search_position_user_data);
  gtk_widget_show (tree_view->priv->search_window);
  if (tree_view->priv->search_entry_changed_id == 0)
    {
      tree_view->priv->search_entry_changed_id =
	g_signal_connect (tree_view->priv->search_entry, "changed",
			  G_CALLBACK (gtk_tree_view_search_init),
			  tree_view);
    }

  tree_view->priv->typeselect_flush_timeout =
    gdk_threads_add_timeout (GTK_TREE_VIEW_SEARCH_DIALOG_TIMEOUT,
		   (GSourceFunc) gtk_tree_view_search_entry_flush_timeout,
		   tree_view);

  /* Grab focus will select all the text.  We don't want that to happen, so we
   * call the parent instance and bypass the selection change.  This is probably
   * really non-kosher. */
  entry_parent_class = g_type_class_peek_parent (GTK_ENTRY_GET_CLASS (tree_view->priv->search_entry));
  (entry_parent_class->grab_focus) (tree_view->priv->search_entry);

  /* send focus-in event */
  send_focus_change (tree_view->priv->search_entry, TRUE);

  /* search first matching iter */
  gtk_tree_view_search_init (tree_view->priv->search_entry, tree_view);

  return TRUE;
}

static gboolean
gtk_tree_view_start_interactive_search (GtkTreeView *tree_view)
{
  return gtk_tree_view_real_start_interactive_search (tree_view, TRUE);
}

/* this function returns the new width of the column being resized given
 * the column and x position of the cursor; the x cursor position is passed
 * in as a pointer and automagicly corrected if it's beyond min/max limits
 */
static gint
gtk_tree_view_new_column_width (GtkTreeView *tree_view,
				gint       i,
				gint      *x)
{
  GtkTreeViewColumn *column;
  gint width;
  gboolean rtl;

  /* first translate the x position from widget->window
   * to clist->clist_window
   */
  rtl = (gtk_widget_get_direction (GTK_WIDGET (tree_view)) == GTK_TEXT_DIR_RTL);
  column = g_list_nth (tree_view->priv->columns, i)->data;
  width = rtl ? (column->button->allocation.x + column->button->allocation.width - *x) : (*x - column->button->allocation.x);
 
  /* Clamp down the value */
  if (column->min_width == -1)
    width = MAX (column->button->requisition.width,
		 width);
  else
    width = MAX (column->min_width,
		 width);
  if (column->max_width != -1)
    width = MIN (width, column->max_width);

  *x = rtl ? (column->button->allocation.x + column->button->allocation.width - width) : (column->button->allocation.x + width);
 
  return width;
}


/* FIXME this adjust_allocation is a big cut-and-paste from
 * GtkCList, needs to be some "official" way to do this
 * factored out.
 */
typedef struct
{
  GdkWindow *window;
  int dx;
  int dy;
} ScrollData;

/* The window to which widget->window is relative */
#define ALLOCATION_WINDOW(widget)		\
   (!gtk_widget_get_has_window (widget) ?		\
    (widget)->window :                          \
     gdk_window_get_parent ((widget)->window))

static void
adjust_allocation_recurse (GtkWidget *widget,
			   gpointer   data)
{
  ScrollData *scroll_data = data;

  /* Need to really size allocate instead of just poking
   * into widget->allocation if the widget is not realized.
   * FIXME someone figure out why this was.
   */
  if (!gtk_widget_get_realized (widget))
    {
      if (gtk_widget_get_visible (widget))
	{
	  GdkRectangle tmp_rectangle = widget->allocation;
	  tmp_rectangle.x += scroll_data->dx;
          tmp_rectangle.y += scroll_data->dy;
          
	  gtk_widget_size_allocate (widget, &tmp_rectangle);
	}
    }
  else
    {
      if (ALLOCATION_WINDOW (widget) == scroll_data->window)
	{
	  widget->allocation.x += scroll_data->dx;
          widget->allocation.y += scroll_data->dy;
          
	  if (GTK_IS_CONTAINER (widget))
	    gtk_container_forall (GTK_CONTAINER (widget),
				  adjust_allocation_recurse,
				  data);
	}
    }
}

static void
adjust_allocation (GtkWidget *widget,
		   int        dx,
                   int        dy)
{
  ScrollData scroll_data;

  if (gtk_widget_get_realized (widget))
    scroll_data.window = ALLOCATION_WINDOW (widget);
  else
    scroll_data.window = NULL;
    
  scroll_data.dx = dx;
  scroll_data.dy = dy;
  
  adjust_allocation_recurse (widget, &scroll_data);
}

/* Callbacks */
static void
gtk_tree_view_adjustment_changed (GtkAdjustment *adjustment,
				  GtkTreeView   *tree_view)
{
  if (gtk_widget_get_realized (GTK_WIDGET (tree_view)))
    {
      gint dy;
	
      gdk_window_move (tree_view->priv->bin_window,
		       - tree_view->priv->hadjustment->value,
		       TREE_VIEW_HEADER_HEIGHT (tree_view));
      gdk_window_move (tree_view->priv->header_window,
		       - tree_view->priv->hadjustment->value,
		       0);
      dy = tree_view->priv->dy - (int) tree_view->priv->vadjustment->value;
      if (dy)
	{
          update_prelight (tree_view,
                           tree_view->priv->event_last_x,
                           tree_view->priv->event_last_y - dy);

	  if (tree_view->priv->edited_column &&
              GTK_IS_WIDGET (tree_view->priv->edited_column->editable_widget))
	    {
	      GList *list;
	      GtkWidget *widget;
	      GtkTreeViewChild *child = NULL;

	      widget = GTK_WIDGET (tree_view->priv->edited_column->editable_widget);
	      adjust_allocation (widget, 0, dy); 
	      
	      for (list = tree_view->priv->children; list; list = list->next)
		{
		  child = (GtkTreeViewChild *)list->data;
		  if (child->widget == widget)
		    {
		      child->y += dy;
		      break;
		    }
		}
	    }
	}
      gdk_window_scroll (tree_view->priv->bin_window, 0, dy);

      if (tree_view->priv->dy != (int) tree_view->priv->vadjustment->value)
        {
          /* update our dy and top_row */
          tree_view->priv->dy = (int) tree_view->priv->vadjustment->value;

          if (!tree_view->priv->in_top_row_to_dy)
            gtk_tree_view_dy_to_top_row (tree_view);
	}

      gdk_window_process_updates (tree_view->priv->header_window, TRUE);
      gdk_window_process_updates (tree_view->priv->bin_window, TRUE);
    }
}



/* Public methods
 */

/**
 * gtk_tree_view_new:
 *
 * Creates a new #GtkTreeView widget.
 *
 * Return value: A newly created #GtkTreeView widget.
 **/
GtkWidget *
gtk_tree_view_new (void)
{
  return g_object_new (GTK_TYPE_TREE_VIEW, NULL);
}

/**
 * gtk_tree_view_new_with_model:
 * @model: the model.
 *
 * Creates a new #GtkTreeView widget with the model initialized to @model.
 *
 * Return value: A newly created #GtkTreeView widget.
 **/
GtkWidget *
gtk_tree_view_new_with_model (GtkTreeModel *model)
{
  return g_object_new (GTK_TYPE_TREE_VIEW, "model", model, NULL);
}

/* Public Accessors
 */

/**
 * gtk_tree_view_get_model:
 * @tree_view: a #GtkTreeView
 *
 * Returns the model the #GtkTreeView is based on.  Returns %NULL if the
 * model is unset.
 *
 * Return value: (transfer none): A #GtkTreeModel, or %NULL if none is currently being used.
 **/
GtkTreeModel *
gtk_tree_view_get_model (GtkTreeView *tree_view)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), NULL);

  return tree_view->priv->model;
}

/**
 * gtk_tree_view_set_model:
 * @tree_view: A #GtkTreeNode.
 * @model: (allow-none): The model.
 *
 * Sets the model for a #GtkTreeView.  If the @tree_view already has a model
 * set, it will remove it before setting the new model.  If @model is %NULL,
 * then it will unset the old model.
 **/
void
gtk_tree_view_set_model (GtkTreeView  *tree_view,
			 GtkTreeModel *model)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (model == NULL || GTK_IS_TREE_MODEL (model));

  if (model == tree_view->priv->model)
    return;

  if (tree_view->priv->scroll_to_path)
    {
      gtk_tree_row_reference_free (tree_view->priv->scroll_to_path);
      tree_view->priv->scroll_to_path = NULL;
    }

  if (tree_view->priv->rubber_band_status)
    gtk_tree_view_stop_rubber_band (tree_view);

  if (tree_view->priv->model)
    {
      GList *tmplist = tree_view->priv->columns;

      gtk_tree_view_unref_and_check_selection_tree (tree_view, tree_view->priv->tree);
      gtk_tree_view_stop_editing (tree_view, TRUE);

      remove_expand_collapse_timeout (tree_view);

      g_signal_handlers_disconnect_by_func (tree_view->priv->model,
					    gtk_tree_view_row_changed,
					    tree_view);
      g_signal_handlers_disconnect_by_func (tree_view->priv->model,
					    gtk_tree_view_row_inserted,
					    tree_view);
      g_signal_handlers_disconnect_by_func (tree_view->priv->model,
					    gtk_tree_view_row_has_child_toggled,
					    tree_view);
      g_signal_handlers_disconnect_by_func (tree_view->priv->model,
					    gtk_tree_view_row_deleted,
					    tree_view);
      g_signal_handlers_disconnect_by_func (tree_view->priv->model,
					    gtk_tree_view_rows_reordered,
					    tree_view);

      for (; tmplist; tmplist = tmplist->next)
	_gtk_tree_view_column_unset_model (tmplist->data,
					   tree_view->priv->model);

      if (tree_view->priv->tree)
	gtk_tree_view_free_rbtree (tree_view);

      gtk_tree_row_reference_free (tree_view->priv->drag_dest_row);
      tree_view->priv->drag_dest_row = NULL;
      gtk_tree_row_reference_free (tree_view->priv->cursor);
      tree_view->priv->cursor = NULL;
      gtk_tree_row_reference_free (tree_view->priv->anchor);
      tree_view->priv->anchor = NULL;
      gtk_tree_row_reference_free (tree_view->priv->top_row);
      tree_view->priv->top_row = NULL;
      gtk_tree_row_reference_free (tree_view->priv->scroll_to_path);
      tree_view->priv->scroll_to_path = NULL;

      tree_view->priv->scroll_to_column = NULL;

      g_object_unref (tree_view->priv->model);

      tree_view->priv->search_column = -1;
      tree_view->priv->fixed_height_check = 0;
      tree_view->priv->fixed_height = -1;
      tree_view->priv->dy = tree_view->priv->top_row_dy = 0;
      tree_view->priv->last_button_x = -1;
      tree_view->priv->last_button_y = -1;
    }

  tree_view->priv->model = model;

  if (tree_view->priv->model)
    {
      gint i;
      GtkTreePath *path;
      GtkTreeIter iter;
      GtkTreeModelFlags flags;

      if (tree_view->priv->search_column == -1)
	{
	  for (i = 0; i < gtk_tree_model_get_n_columns (model); i++)
	    {
	      GType type = gtk_tree_model_get_column_type (model, i);

	      if (g_value_type_transformable (type, G_TYPE_STRING))
		{
		  tree_view->priv->search_column = i;
		  break;
		}
	    }
	}

      g_object_ref (tree_view->priv->model);
      g_signal_connect (tree_view->priv->model,
			"row-changed",
			G_CALLBACK (gtk_tree_view_row_changed),
			tree_view);
      g_signal_connect (tree_view->priv->model,
			"row-inserted",
			G_CALLBACK (gtk_tree_view_row_inserted),
			tree_view);
      g_signal_connect (tree_view->priv->model,
			"row-has-child-toggled",
			G_CALLBACK (gtk_tree_view_row_has_child_toggled),
			tree_view);
      g_signal_connect (tree_view->priv->model,
			"row-deleted",
			G_CALLBACK (gtk_tree_view_row_deleted),
			tree_view);
      g_signal_connect (tree_view->priv->model,
			"rows-reordered",
			G_CALLBACK (gtk_tree_view_rows_reordered),
			tree_view);

      flags = gtk_tree_model_get_flags (tree_view->priv->model);
      if ((flags & GTK_TREE_MODEL_LIST_ONLY) == GTK_TREE_MODEL_LIST_ONLY)
        GTK_TREE_VIEW_SET_FLAG (tree_view, GTK_TREE_VIEW_IS_LIST);
      else
        GTK_TREE_VIEW_UNSET_FLAG (tree_view, GTK_TREE_VIEW_IS_LIST);

      path = gtk_tree_path_new_first ();
      if (gtk_tree_model_get_iter (tree_view->priv->model, &iter, path))
	{
	  tree_view->priv->tree = _gtk_rbtree_new ();
	  gtk_tree_view_build_tree (tree_view, tree_view->priv->tree, &iter, 1, FALSE);
	}
      gtk_tree_path_free (path);

      /*  FIXME: do I need to do this? gtk_tree_view_create_buttons (tree_view); */
      install_presize_handler (tree_view);
    }

  g_object_notify (G_OBJECT (tree_view), "model");

  if (tree_view->priv->selection)
  _gtk_tree_selection_emit_changed (tree_view->priv->selection);

  if (gtk_widget_get_realized (GTK_WIDGET (tree_view)))
    gtk_widget_queue_resize (GTK_WIDGET (tree_view));
}

/**
 * gtk_tree_view_get_selection:
 * @tree_view: A #GtkTreeView.
 *
 * Gets the #GtkTreeSelection associated with @tree_view.
 *
 * Return value: (transfer none): A #GtkTreeSelection object.
 **/
GtkTreeSelection *
gtk_tree_view_get_selection (GtkTreeView *tree_view)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), NULL);

  return tree_view->priv->selection;
}

/**
 * gtk_tree_view_get_hadjustment:
 * @tree_view: A #GtkTreeView
 *
 * Gets the #GtkAdjustment currently being used for the horizontal aspect.
 *
 * Return value: (transfer none): A #GtkAdjustment object, or %NULL
 *     if none is currently being used.
 **/
GtkAdjustment *
gtk_tree_view_get_hadjustment (GtkTreeView *tree_view)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), NULL);

  if (tree_view->priv->hadjustment == NULL)
    gtk_tree_view_set_hadjustment (tree_view, NULL);

  return tree_view->priv->hadjustment;
}

/**
 * gtk_tree_view_set_hadjustment:
 * @tree_view: A #GtkTreeView
 * @adjustment: (allow-none): The #GtkAdjustment to set, or %NULL
 *
 * Sets the #GtkAdjustment for the current horizontal aspect.
 **/
void
gtk_tree_view_set_hadjustment (GtkTreeView   *tree_view,
			       GtkAdjustment *adjustment)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  gtk_tree_view_set_adjustments (tree_view,
				 adjustment,
				 tree_view->priv->vadjustment);

  g_object_notify (G_OBJECT (tree_view), "hadjustment");
}

/**
 * gtk_tree_view_get_vadjustment:
 * @tree_view: A #GtkTreeView
 *
 * Gets the #GtkAdjustment currently being used for the vertical aspect.
 *
 * Return value: (transfer none): A #GtkAdjustment object, or %NULL
 *     if none is currently being used.
 **/
GtkAdjustment *
gtk_tree_view_get_vadjustment (GtkTreeView *tree_view)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), NULL);

  if (tree_view->priv->vadjustment == NULL)
    gtk_tree_view_set_vadjustment (tree_view, NULL);

  return tree_view->priv->vadjustment;
}

/**
 * gtk_tree_view_set_vadjustment:
 * @tree_view: A #GtkTreeView
 * @adjustment: (allow-none): The #GtkAdjustment to set, or %NULL
 *
 * Sets the #GtkAdjustment for the current vertical aspect.
 **/
void
gtk_tree_view_set_vadjustment (GtkTreeView   *tree_view,
			       GtkAdjustment *adjustment)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  gtk_tree_view_set_adjustments (tree_view,
				 tree_view->priv->hadjustment,
				 adjustment);

  g_object_notify (G_OBJECT (tree_view), "vadjustment");
}

/* Column and header operations */

/**
 * gtk_tree_view_get_headers_visible:
 * @tree_view: A #GtkTreeView.
 *
 * Returns %TRUE if the headers on the @tree_view are visible.
 *
 * Return value: Whether the headers are visible or not.
 **/
gboolean
gtk_tree_view_get_headers_visible (GtkTreeView *tree_view)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), FALSE);

  return GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_HEADERS_VISIBLE);
}

/**
 * gtk_tree_view_set_headers_visible:
 * @tree_view: A #GtkTreeView.
 * @headers_visible: %TRUE if the headers are visible
 *
 * Sets the visibility state of the headers.
 **/
void
gtk_tree_view_set_headers_visible (GtkTreeView *tree_view,
				   gboolean     headers_visible)
{
  gint x, y;
  GList *list;
  GtkTreeViewColumn *column;

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  headers_visible = !! headers_visible;

  if (GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_HEADERS_VISIBLE) == headers_visible)
    return;

  if (headers_visible)
    GTK_TREE_VIEW_SET_FLAG (tree_view, GTK_TREE_VIEW_HEADERS_VISIBLE);
  else
    GTK_TREE_VIEW_UNSET_FLAG (tree_view, GTK_TREE_VIEW_HEADERS_VISIBLE);

  if (gtk_widget_get_realized (GTK_WIDGET (tree_view)))
    {
      gdk_window_get_position (tree_view->priv->bin_window, &x, &y);
      if (headers_visible)
	{
	  gdk_window_move_resize (tree_view->priv->bin_window, x, y  + TREE_VIEW_HEADER_HEIGHT (tree_view), tree_view->priv->width, GTK_WIDGET (tree_view)->allocation.height -  + TREE_VIEW_HEADER_HEIGHT (tree_view));

          if (gtk_widget_get_mapped (GTK_WIDGET (tree_view)))
            gtk_tree_view_map_buttons (tree_view);
 	}
      else
	{
	  gdk_window_move_resize (tree_view->priv->bin_window, x, y, tree_view->priv->width, tree_view->priv->height);

	  for (list = tree_view->priv->columns; list; list = list->next)
	    {
	      column = list->data;
	      gtk_widget_unmap (column->button);
	    }
	  gdk_window_hide (tree_view->priv->header_window);
	}
    }

  tree_view->priv->vadjustment->page_size = GTK_WIDGET (tree_view)->allocation.height - TREE_VIEW_HEADER_HEIGHT (tree_view);
  tree_view->priv->vadjustment->page_increment = (GTK_WIDGET (tree_view)->allocation.height - TREE_VIEW_HEADER_HEIGHT (tree_view)) / 2;
  tree_view->priv->vadjustment->lower = 0;
  tree_view->priv->vadjustment->upper = tree_view->priv->height;
  gtk_adjustment_changed (tree_view->priv->vadjustment);

  gtk_widget_queue_resize (GTK_WIDGET (tree_view));

  g_object_notify (G_OBJECT (tree_view), "headers-visible");
}

/**
 * gtk_tree_view_columns_autosize:
 * @tree_view: A #GtkTreeView.
 *
 * Resizes all columns to their optimal width. Only works after the
 * treeview has been realized.
 **/
void
gtk_tree_view_columns_autosize (GtkTreeView *tree_view)
{
  gboolean dirty = FALSE;
  GList *list;
  GtkTreeViewColumn *column;

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  for (list = tree_view->priv->columns; list; list = list->next)
    {
      column = list->data;
      if (column->column_type == GTK_TREE_VIEW_COLUMN_AUTOSIZE)
	continue;
      _gtk_tree_view_column_cell_set_dirty (column, TRUE);
      dirty = TRUE;
    }

  if (dirty)
    gtk_widget_queue_resize (GTK_WIDGET (tree_view));
}

/**
 * gtk_tree_view_set_headers_clickable:
 * @tree_view: A #GtkTreeView.
 * @setting: %TRUE if the columns are clickable.
 *
 * Allow the column title buttons to be clicked.
 **/
void
gtk_tree_view_set_headers_clickable (GtkTreeView *tree_view,
				     gboolean   setting)
{
  GList *list;

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  for (list = tree_view->priv->columns; list; list = list->next)
    gtk_tree_view_column_set_clickable (GTK_TREE_VIEW_COLUMN (list->data), setting);

  g_object_notify (G_OBJECT (tree_view), "headers-clickable");
}


/**
 * gtk_tree_view_get_headers_clickable:
 * @tree_view: A #GtkTreeView.
 *
 * Returns whether all header columns are clickable.
 *
 * Return value: %TRUE if all header columns are clickable, otherwise %FALSE
 *
 * Since: 2.10
 **/
gboolean 
gtk_tree_view_get_headers_clickable (GtkTreeView *tree_view)
{
  GList *list;
  
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), FALSE);

  for (list = tree_view->priv->columns; list; list = list->next)
    if (!GTK_TREE_VIEW_COLUMN (list->data)->clickable)
      return FALSE;

  return TRUE;
}

/**
 * gtk_tree_view_set_rules_hint
 * @tree_view: a #GtkTreeView
 * @setting: %TRUE if the tree requires reading across rows
 *
 * This function tells GTK+ that the user interface for your
 * application requires users to read across tree rows and associate
 * cells with one another. By default, GTK+ will then render the tree
 * with alternating row colors. Do <emphasis>not</emphasis> use it
 * just because you prefer the appearance of the ruled tree; that's a
 * question for the theme. Some themes will draw tree rows in
 * alternating colors even when rules are turned off, and users who
 * prefer that appearance all the time can choose those themes. You
 * should call this function only as a <emphasis>semantic</emphasis>
 * hint to the theme engine that your tree makes alternating colors
 * useful from a functional standpoint (since it has lots of columns,
 * generally).
 *
 **/
void
gtk_tree_view_set_rules_hint (GtkTreeView  *tree_view,
                              gboolean      setting)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  setting = setting != FALSE;

  if (tree_view->priv->has_rules != setting)
    {
      tree_view->priv->has_rules = setting;
      gtk_widget_queue_draw (GTK_WIDGET (tree_view));
    }

  g_object_notify (G_OBJECT (tree_view), "rules-hint");
}

/**
 * gtk_tree_view_get_rules_hint
 * @tree_view: a #GtkTreeView
 *
 * Gets the setting set by gtk_tree_view_set_rules_hint().
 *
 * Return value: %TRUE if rules are useful for the user of this tree
 **/
gboolean
gtk_tree_view_get_rules_hint (GtkTreeView  *tree_view)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), FALSE);

  return tree_view->priv->has_rules;
}

/* Public Column functions
 */

/**
 * gtk_tree_view_append_column:
 * @tree_view: A #GtkTreeView.
 * @column: The #GtkTreeViewColumn to add.
 *
 * Appends @column to the list of columns. If @tree_view has "fixed_height"
 * mode enabled, then @column must have its "sizing" property set to be
 * GTK_TREE_VIEW_COLUMN_FIXED.
 *
 * Return value: The number of columns in @tree_view after appending.
 **/
gint
gtk_tree_view_append_column (GtkTreeView       *tree_view,
			     GtkTreeViewColumn *column)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), -1);
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (column), -1);
  g_return_val_if_fail (column->tree_view == NULL, -1);

  return gtk_tree_view_insert_column (tree_view, column, -1);
}


/**
 * gtk_tree_view_remove_column:
 * @tree_view: A #GtkTreeView.
 * @column: The #GtkTreeViewColumn to remove.
 *
 * Removes @column from @tree_view.
 *
 * Return value: The number of columns in @tree_view after removing.
 **/
gint
gtk_tree_view_remove_column (GtkTreeView       *tree_view,
                             GtkTreeViewColumn *column)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), -1);
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (column), -1);
  g_return_val_if_fail (column->tree_view == GTK_WIDGET (tree_view), -1);

  if (tree_view->priv->focus_column == column)
    tree_view->priv->focus_column = NULL;

  if (tree_view->priv->edited_column == column)
    {
      gtk_tree_view_stop_editing (tree_view, TRUE);

      /* no need to, but just to be sure ... */
      tree_view->priv->edited_column = NULL;
    }

  if (tree_view->priv->expander_column == column)
    tree_view->priv->expander_column = NULL;

  g_signal_handlers_disconnect_by_func (column,
                                        G_CALLBACK (column_sizing_notify),
                                        tree_view);

  _gtk_tree_view_column_unset_tree_view (column);

  tree_view->priv->columns = g_list_remove (tree_view->priv->columns, column);
  tree_view->priv->n_columns--;

  if (gtk_widget_get_realized (GTK_WIDGET (tree_view)))
    {
      GList *list;

      _gtk_tree_view_column_unrealize_button (column);
      for (list = tree_view->priv->columns; list; list = list->next)
	{
	  GtkTreeViewColumn *tmp_column;

	  tmp_column = GTK_TREE_VIEW_COLUMN (list->data);
	  if (tmp_column->visible)
	    _gtk_tree_view_column_cell_set_dirty (tmp_column, TRUE);
	}

      if (tree_view->priv->n_columns == 0 &&
	  gtk_tree_view_get_headers_visible (tree_view))
	gdk_window_hide (tree_view->priv->header_window);

      gtk_widget_queue_resize (GTK_WIDGET (tree_view));
    }

  g_object_unref (column);
  g_signal_emit (tree_view, tree_view_signals[COLUMNS_CHANGED], 0);

  return tree_view->priv->n_columns;
}

/**
 * gtk_tree_view_insert_column:
 * @tree_view: A #GtkTreeView.
 * @column: The #GtkTreeViewColumn to be inserted.
 * @position: The position to insert @column in.
 *
 * This inserts the @column into the @tree_view at @position.  If @position is
 * -1, then the column is inserted at the end. If @tree_view has
 * "fixed_height" mode enabled, then @column must have its "sizing" property
 * set to be GTK_TREE_VIEW_COLUMN_FIXED.
 *
 * Return value: The number of columns in @tree_view after insertion.
 **/
gint
gtk_tree_view_insert_column (GtkTreeView       *tree_view,
                             GtkTreeViewColumn *column,
                             gint               position)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), -1);
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (column), -1);
  g_return_val_if_fail (column->tree_view == NULL, -1);

  if (tree_view->priv->fixed_height_mode)
    g_return_val_if_fail (gtk_tree_view_column_get_sizing (column)
                          == GTK_TREE_VIEW_COLUMN_FIXED, -1);

  g_object_ref_sink (column);

  if (tree_view->priv->n_columns == 0 &&
      gtk_widget_get_realized (GTK_WIDGET (tree_view)) &&
      gtk_tree_view_get_headers_visible (tree_view))
    {
      gdk_window_show (tree_view->priv->header_window);
    }

  g_signal_connect (column, "notify::sizing",
                    G_CALLBACK (column_sizing_notify), tree_view);

  tree_view->priv->columns = g_list_insert (tree_view->priv->columns,
					    column, position);
  tree_view->priv->n_columns++;

  _gtk_tree_view_column_set_tree_view (column, tree_view);

  if (gtk_widget_get_realized (GTK_WIDGET (tree_view)))
    {
      GList *list;

      _gtk_tree_view_column_realize_button (column);

      for (list = tree_view->priv->columns; list; list = list->next)
	{
	  column = GTK_TREE_VIEW_COLUMN (list->data);
	  if (column->visible)
	    _gtk_tree_view_column_cell_set_dirty (column, TRUE);
	}
      gtk_widget_queue_resize (GTK_WIDGET (tree_view));
    }

  g_signal_emit (tree_view, tree_view_signals[COLUMNS_CHANGED], 0);

  return tree_view->priv->n_columns;
}

/**
 * gtk_tree_view_insert_column_with_attributes:
 * @tree_view: A #GtkTreeView
 * @position: The position to insert the new column in.
 * @title: The title to set the header to.
 * @cell: The #GtkCellRenderer.
 * @Varargs: A %NULL-terminated list of attributes.
 *
 * Creates a new #GtkTreeViewColumn and inserts it into the @tree_view at
 * @position.  If @position is -1, then the newly created column is inserted at
 * the end.  The column is initialized with the attributes given. If @tree_view
 * has "fixed_height" mode enabled, then the new column will have its sizing
 * property set to be GTK_TREE_VIEW_COLUMN_FIXED.
 *
 * Return value: The number of columns in @tree_view after insertion.
 **/
gint
gtk_tree_view_insert_column_with_attributes (GtkTreeView     *tree_view,
					     gint             position,
					     const gchar     *title,
					     GtkCellRenderer *cell,
					     ...)
{
  GtkTreeViewColumn *column;
  gchar *attribute;
  va_list args;
  gint column_id;

  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), -1);

  column = gtk_tree_view_column_new ();
  if (tree_view->priv->fixed_height_mode)
    gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_FIXED);

  gtk_tree_view_column_set_title (column, title);
  gtk_tree_view_column_pack_start (column, cell, TRUE);

  va_start (args, cell);

  attribute = va_arg (args, gchar *);

  while (attribute != NULL)
    {
      column_id = va_arg (args, gint);
      gtk_tree_view_column_add_attribute (column, cell, attribute, column_id);
      attribute = va_arg (args, gchar *);
    }

  va_end (args);

  gtk_tree_view_insert_column (tree_view, column, position);

  return tree_view->priv->n_columns;
}

/**
 * gtk_tree_view_insert_column_with_data_func:
 * @tree_view: a #GtkTreeView
 * @position: Position to insert, -1 for append
 * @title: column title
 * @cell: cell renderer for column
 * @func: function to set attributes of cell renderer
 * @data: data for @func
 * @dnotify: destroy notifier for @data
 *
 * Convenience function that inserts a new column into the #GtkTreeView
 * with the given cell renderer and a #GtkCellDataFunc to set cell renderer
 * attributes (normally using data from the model). See also
 * gtk_tree_view_column_set_cell_data_func(), gtk_tree_view_column_pack_start().
 * If @tree_view has "fixed_height" mode enabled, then the new column will have its
 * "sizing" property set to be GTK_TREE_VIEW_COLUMN_FIXED.
 *
 * Return value: number of columns in the tree view post-insert
 **/
gint
gtk_tree_view_insert_column_with_data_func  (GtkTreeView               *tree_view,
                                             gint                       position,
                                             const gchar               *title,
                                             GtkCellRenderer           *cell,
                                             GtkTreeCellDataFunc        func,
                                             gpointer                   data,
                                             GDestroyNotify             dnotify)
{
  GtkTreeViewColumn *column;

  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), -1);

  column = gtk_tree_view_column_new ();
  if (tree_view->priv->fixed_height_mode)
    gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_FIXED);

  gtk_tree_view_column_set_title (column, title);
  gtk_tree_view_column_pack_start (column, cell, TRUE);
  gtk_tree_view_column_set_cell_data_func (column, cell, func, data, dnotify);

  gtk_tree_view_insert_column (tree_view, column, position);

  return tree_view->priv->n_columns;
}

/**
 * gtk_tree_view_get_column:
 * @tree_view: A #GtkTreeView.
 * @n: The position of the column, counting from 0.
 *
 * Gets the #GtkTreeViewColumn at the given position in the #tree_view.
 *
 * Return value: (transfer none): The #GtkTreeViewColumn, or %NULL if the
 *     position is outside the range of columns.
 **/
GtkTreeViewColumn *
gtk_tree_view_get_column (GtkTreeView *tree_view,
			  gint         n)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), NULL);

  if (n < 0 || n >= tree_view->priv->n_columns)
    return NULL;

  if (tree_view->priv->columns == NULL)
    return NULL;

  return GTK_TREE_VIEW_COLUMN (g_list_nth (tree_view->priv->columns, n)->data);
}

/**
 * gtk_tree_view_get_columns:
 * @tree_view: A #GtkTreeView
 *
 * Returns a #GList of all the #GtkTreeViewColumn s currently in @tree_view.
 * The returned list must be freed with g_list_free ().
 *
 * Return value: (element-type GtkTreeViewColumn) (transfer container): A list of #GtkTreeViewColumn s
 **/
GList *
gtk_tree_view_get_columns (GtkTreeView *tree_view)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), NULL);

  return g_list_copy (tree_view->priv->columns);
}

/**
 * gtk_tree_view_move_column_after:
 * @tree_view: A #GtkTreeView
 * @column: The #GtkTreeViewColumn to be moved.
 * @base_column: (allow-none): The #GtkTreeViewColumn to be moved relative to, or %NULL.
 *
 * Moves @column to be after to @base_column.  If @base_column is %NULL, then
 * @column is placed in the first position.
 **/
void
gtk_tree_view_move_column_after (GtkTreeView       *tree_view,
				 GtkTreeViewColumn *column,
				 GtkTreeViewColumn *base_column)
{
  GList *column_list_el, *base_el = NULL;

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  column_list_el = g_list_find (tree_view->priv->columns, column);
  g_return_if_fail (column_list_el != NULL);

  if (base_column)
    {
      base_el = g_list_find (tree_view->priv->columns, base_column);
      g_return_if_fail (base_el != NULL);
    }

  if (column_list_el->prev == base_el)
    return;

  tree_view->priv->columns = g_list_remove_link (tree_view->priv->columns, column_list_el);
  if (base_el == NULL)
    {
      column_list_el->prev = NULL;
      column_list_el->next = tree_view->priv->columns;
      if (column_list_el->next)
	column_list_el->next->prev = column_list_el;
      tree_view->priv->columns = column_list_el;
    }
  else
    {
      column_list_el->prev = base_el;
      column_list_el->next = base_el->next;
      if (column_list_el->next)
	column_list_el->next->prev = column_list_el;
      base_el->next = column_list_el;
    }

  if (gtk_widget_get_realized (GTK_WIDGET (tree_view)))
    {
      gtk_widget_queue_resize (GTK_WIDGET (tree_view));
      gtk_tree_view_size_allocate_columns (GTK_WIDGET (tree_view), NULL);
    }

  g_signal_emit (tree_view, tree_view_signals[COLUMNS_CHANGED], 0);
}

/**
 * gtk_tree_view_set_expander_column:
 * @tree_view: A #GtkTreeView
 * @column: %NULL, or the column to draw the expander arrow at.
 *
 * Sets the column to draw the expander arrow at. It must be in @tree_view.  
 * If @column is %NULL, then the expander arrow is always at the first 
 * visible column.
 *
 * If you do not want expander arrow to appear in your tree, set the 
 * expander column to a hidden column.
 **/
void
gtk_tree_view_set_expander_column (GtkTreeView       *tree_view,
                                   GtkTreeViewColumn *column)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (column == NULL || GTK_IS_TREE_VIEW_COLUMN (column));

  if (tree_view->priv->expander_column != column)
    {
      GList *list;

      if (column)
	{
	  /* Confirm that column is in tree_view */
	  for (list = tree_view->priv->columns; list; list = list->next)
	    if (list->data == column)
	      break;
	  g_return_if_fail (list != NULL);
	}

      tree_view->priv->expander_column = column;
      g_object_notify (G_OBJECT (tree_view), "expander-column");
    }
}

/**
 * gtk_tree_view_get_expander_column:
 * @tree_view: A #GtkTreeView
 *
 * Returns the column that is the current expander column.
 * This column has the expander arrow drawn next to it.
 *
 * Return value: (transfer none): The expander column.
 **/
GtkTreeViewColumn *
gtk_tree_view_get_expander_column (GtkTreeView *tree_view)
{
  GList *list;

  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), NULL);

  for (list = tree_view->priv->columns; list; list = list->next)
    if (gtk_tree_view_is_expander_column (tree_view, GTK_TREE_VIEW_COLUMN (list->data)))
      return (GtkTreeViewColumn *) list->data;
  return NULL;
}


/**
 * gtk_tree_view_set_column_drag_function:
 * @tree_view: A #GtkTreeView.
 * @func: (allow-none): A function to determine which columns are reorderable, or %NULL.
 * @user_data: (allow-none): User data to be passed to @func, or %NULL
 * @destroy: (allow-none): Destroy notifier for @user_data, or %NULL
 *
 * Sets a user function for determining where a column may be dropped when
 * dragged.  This function is called on every column pair in turn at the
 * beginning of a column drag to determine where a drop can take place.  The
 * arguments passed to @func are: the @tree_view, the #GtkTreeViewColumn being
 * dragged, the two #GtkTreeViewColumn s determining the drop spot, and
 * @user_data.  If either of the #GtkTreeViewColumn arguments for the drop spot
 * are %NULL, then they indicate an edge.  If @func is set to be %NULL, then
 * @tree_view reverts to the default behavior of allowing all columns to be
 * dropped everywhere.
 **/
void
gtk_tree_view_set_column_drag_function (GtkTreeView               *tree_view,
					GtkTreeViewColumnDropFunc  func,
					gpointer                   user_data,
					GDestroyNotify             destroy)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  if (tree_view->priv->column_drop_func_data_destroy)
    tree_view->priv->column_drop_func_data_destroy (tree_view->priv->column_drop_func_data);

  tree_view->priv->column_drop_func = func;
  tree_view->priv->column_drop_func_data = user_data;
  tree_view->priv->column_drop_func_data_destroy = destroy;
}

/**
 * gtk_tree_view_scroll_to_point:
 * @tree_view: a #GtkTreeView
 * @tree_x: X coordinate of new top-left pixel of visible area, or -1
 * @tree_y: Y coordinate of new top-left pixel of visible area, or -1
 *
 * Scrolls the tree view such that the top-left corner of the visible
 * area is @tree_x, @tree_y, where @tree_x and @tree_y are specified
 * in tree coordinates.  The @tree_view must be realized before
 * this function is called.  If it isn't, you probably want to be
 * using gtk_tree_view_scroll_to_cell().
 *
 * If either @tree_x or @tree_y are -1, then that direction isn't scrolled.
 **/
void
gtk_tree_view_scroll_to_point (GtkTreeView *tree_view,
                               gint         tree_x,
                               gint         tree_y)
{
  GtkAdjustment *hadj;
  GtkAdjustment *vadj;

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (gtk_widget_get_realized (GTK_WIDGET (tree_view)));

  hadj = tree_view->priv->hadjustment;
  vadj = tree_view->priv->vadjustment;

  if (tree_x != -1)
    gtk_adjustment_set_value (hadj, CLAMP (tree_x, hadj->lower, hadj->upper - hadj->page_size));
  if (tree_y != -1)
    gtk_adjustment_set_value (vadj, CLAMP (tree_y, vadj->lower, vadj->upper - vadj->page_size));
}

/**
 * gtk_tree_view_scroll_to_cell:
 * @tree_view: A #GtkTreeView.
 * @path: (allow-none): The path of the row to move to, or %NULL.
 * @column: (allow-none): The #GtkTreeViewColumn to move horizontally to, or %NULL.
 * @use_align: whether to use alignment arguments, or %FALSE.
 * @row_align: The vertical alignment of the row specified by @path.
 * @col_align: The horizontal alignment of the column specified by @column.
 *
 * Moves the alignments of @tree_view to the position specified by @column and
 * @path.  If @column is %NULL, then no horizontal scrolling occurs.  Likewise,
 * if @path is %NULL no vertical scrolling occurs.  At a minimum, one of @column
 * or @path need to be non-%NULL.  @row_align determines where the row is
 * placed, and @col_align determines where @column is placed.  Both are expected
 * to be between 0.0 and 1.0. 0.0 means left/top alignment, 1.0 means
 * right/bottom alignment, 0.5 means center.
 *
 * If @use_align is %FALSE, then the alignment arguments are ignored, and the
 * tree does the minimum amount of work to scroll the cell onto the screen.
 * This means that the cell will be scrolled to the edge closest to its current
 * position.  If the cell is currently visible on the screen, nothing is done.
 *
 * This function only works if the model is set, and @path is a valid row on the
 * model.  If the model changes before the @tree_view is realized, the centered
 * path will be modified to reflect this change.
 **/
void
gtk_tree_view_scroll_to_cell (GtkTreeView       *tree_view,
                              GtkTreePath       *path,
                              GtkTreeViewColumn *column,
			      gboolean           use_align,
                              gfloat             row_align,
                              gfloat             col_align)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (tree_view->priv->model != NULL);
  g_return_if_fail (tree_view->priv->tree != NULL);
  g_return_if_fail (row_align >= 0.0 && row_align <= 1.0);
  g_return_if_fail (col_align >= 0.0 && col_align <= 1.0);
  g_return_if_fail (path != NULL || column != NULL);

#if 0
  g_print ("gtk_tree_view_scroll_to_cell:\npath: %s\ncolumn: %s\nuse_align: %d\nrow_align: %f\ncol_align: %f\n",
	   gtk_tree_path_to_string (path), column?"non-null":"null", use_align, row_align, col_align);
#endif
  row_align = CLAMP (row_align, 0.0, 1.0);
  col_align = CLAMP (col_align, 0.0, 1.0);


  /* Note: Despite the benefits that come from having one code path for the
   * scrolling code, we short-circuit validate_visible_area's immplementation as
   * it is much slower than just going to the point.
   */
  if (!gtk_widget_get_visible (GTK_WIDGET (tree_view)) ||
      !gtk_widget_get_realized (GTK_WIDGET (tree_view)) ||
      GTK_WIDGET_ALLOC_NEEDED (tree_view) || 
      GTK_RBNODE_FLAG_SET (tree_view->priv->tree->root, GTK_RBNODE_DESCENDANTS_INVALID))
    {
      if (tree_view->priv->scroll_to_path)
	gtk_tree_row_reference_free (tree_view->priv->scroll_to_path);

      tree_view->priv->scroll_to_path = NULL;
      tree_view->priv->scroll_to_column = NULL;

      if (path)
	tree_view->priv->scroll_to_path = gtk_tree_row_reference_new_proxy (G_OBJECT (tree_view), tree_view->priv->model, path);
      if (column)
	tree_view->priv->scroll_to_column = column;
      tree_view->priv->scroll_to_use_align = use_align;
      tree_view->priv->scroll_to_row_align = row_align;
      tree_view->priv->scroll_to_col_align = col_align;

      install_presize_handler (tree_view);
    }
  else
    {
      GdkRectangle cell_rect;
      GdkRectangle vis_rect;
      gint dest_x, dest_y;

      gtk_tree_view_get_background_area (tree_view, path, column, &cell_rect);
      gtk_tree_view_get_visible_rect (tree_view, &vis_rect);

      cell_rect.y = TREE_WINDOW_Y_TO_RBTREE_Y (tree_view, cell_rect.y);

      dest_x = vis_rect.x;
      dest_y = vis_rect.y;

      if (column)
	{
	  if (use_align)
	    {
	      dest_x = cell_rect.x - ((vis_rect.width - cell_rect.width) * col_align);
	    }
	  else
	    {
	      if (cell_rect.x < vis_rect.x)
		dest_x = cell_rect.x;
	      if (cell_rect.x + cell_rect.width > vis_rect.x + vis_rect.width)
		dest_x = cell_rect.x + cell_rect.width - vis_rect.width;
	    }
	}

      if (path)
	{
	  if (use_align)
	    {
	      dest_y = cell_rect.y - ((vis_rect.height - cell_rect.height) * row_align);
	      dest_y = MAX (dest_y, 0);
	    }
	  else
	    {
	      if (cell_rect.y < vis_rect.y)
		dest_y = cell_rect.y;
	      if (cell_rect.y + cell_rect.height > vis_rect.y + vis_rect.height)
		dest_y = cell_rect.y + cell_rect.height - vis_rect.height;
	    }
	}

      gtk_tree_view_scroll_to_point (tree_view, dest_x, dest_y);
    }
}

/**
 * gtk_tree_view_row_activated:
 * @tree_view: A #GtkTreeView
 * @path: The #GtkTreePath to be activated.
 * @column: The #GtkTreeViewColumn to be activated.
 *
 * Activates the cell determined by @path and @column.
 **/
void
gtk_tree_view_row_activated (GtkTreeView       *tree_view,
			     GtkTreePath       *path,
			     GtkTreeViewColumn *column)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  g_signal_emit (tree_view, tree_view_signals[ROW_ACTIVATED], 0, path, column);
}


static void
gtk_tree_view_expand_all_emission_helper (GtkRBTree *tree,
                                          GtkRBNode *node,
                                          gpointer   data)
{
  GtkTreeView *tree_view = data;

  if ((node->flags & GTK_RBNODE_IS_PARENT) == GTK_RBNODE_IS_PARENT &&
      node->children)
    {
      GtkTreePath *path;
      GtkTreeIter iter;

      path = _gtk_tree_view_find_path (tree_view, tree, node);
      gtk_tree_model_get_iter (tree_view->priv->model, &iter, path);

      g_signal_emit (tree_view, tree_view_signals[ROW_EXPANDED], 0, &iter, path);

      gtk_tree_path_free (path);
    }

  if (node->children)
    _gtk_rbtree_traverse (node->children,
                          node->children->root,
                          G_PRE_ORDER,
                          gtk_tree_view_expand_all_emission_helper,
                          tree_view);
}

/**
 * gtk_tree_view_expand_all:
 * @tree_view: A #GtkTreeView.
 *
 * Recursively expands all nodes in the @tree_view.
 **/
void
gtk_tree_view_expand_all (GtkTreeView *tree_view)
{
  GtkTreePath *path;
  GtkRBTree *tree;
  GtkRBNode *node;

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  if (tree_view->priv->tree == NULL)
    return;

  path = gtk_tree_path_new_first ();
  _gtk_tree_view_find_node (tree_view, path, &tree, &node);

  while (node)
    {
      gtk_tree_view_real_expand_row (tree_view, path, tree, node, TRUE, FALSE);
      node = _gtk_rbtree_next (tree, node);
      gtk_tree_path_next (path);
  }

  gtk_tree_path_free (path);
}

/* Timeout to animate the expander during expands and collapses */
static gboolean
expand_collapse_timeout (gpointer data)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (data);
  gboolean retval = do_expand_collapse (data);

  if (! retval)
    remove_expand_collapse_timeout (tree_view);

  return retval;
}

static void
add_expand_collapse_timeout (GtkTreeView *tree_view,
                             GtkRBTree   *tree,
                             GtkRBNode   *node,
                             gboolean     expand)
{
  if (tree_view->priv->expand_collapse_timeout != 0)
    return;

  tree_view->priv->expand_collapse_timeout =
      gdk_threads_add_timeout (50, expand_collapse_timeout, tree_view);
  tree_view->priv->expanded_collapsed_tree = tree;
  tree_view->priv->expanded_collapsed_node = node;

  if (expand)
    GTK_RBNODE_SET_FLAG (node, GTK_RBNODE_IS_SEMI_COLLAPSED);
  else
    GTK_RBNODE_SET_FLAG (node, GTK_RBNODE_IS_SEMI_EXPANDED);
}

static void
remove_expand_collapse_timeout (GtkTreeView *tree_view)
{
  if (tree_view->priv->expand_collapse_timeout)
    {
      g_source_remove (tree_view->priv->expand_collapse_timeout);
      tree_view->priv->expand_collapse_timeout = 0;
    }

  if (tree_view->priv->expanded_collapsed_node != NULL)
    {
      GTK_RBNODE_UNSET_FLAG (tree_view->priv->expanded_collapsed_node, GTK_RBNODE_IS_SEMI_EXPANDED);
      GTK_RBNODE_UNSET_FLAG (tree_view->priv->expanded_collapsed_node, GTK_RBNODE_IS_SEMI_COLLAPSED);

      tree_view->priv->expanded_collapsed_node = NULL;
    }
}

static void
cancel_arrow_animation (GtkTreeView *tree_view)
{
  if (tree_view->priv->expand_collapse_timeout)
    {
      while (do_expand_collapse (tree_view));

      remove_expand_collapse_timeout (tree_view);
    }
}

static gboolean
do_expand_collapse (GtkTreeView *tree_view)
{
  GtkRBNode *node;
  GtkRBTree *tree;
  gboolean expanding;
  gboolean redraw;

  redraw = FALSE;
  expanding = TRUE;

  node = tree_view->priv->expanded_collapsed_node;
  tree = tree_view->priv->expanded_collapsed_tree;

  if (node->children == NULL)
    expanding = FALSE;

  if (expanding)
    {
      if (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_SEMI_COLLAPSED))
	{
	  GTK_RBNODE_UNSET_FLAG (node, GTK_RBNODE_IS_SEMI_COLLAPSED);
	  GTK_RBNODE_SET_FLAG (node, GTK_RBNODE_IS_SEMI_EXPANDED);

	  redraw = TRUE;

	}
      else if (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_SEMI_EXPANDED))
	{
	  GTK_RBNODE_UNSET_FLAG (node, GTK_RBNODE_IS_SEMI_EXPANDED);

	  redraw = TRUE;
	}
    }
  else
    {
      if (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_SEMI_EXPANDED))
	{
	  GTK_RBNODE_UNSET_FLAG (node, GTK_RBNODE_IS_SEMI_EXPANDED);
	  GTK_RBNODE_SET_FLAG (node, GTK_RBNODE_IS_SEMI_COLLAPSED);

	  redraw = TRUE;
	}
      else if (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_SEMI_COLLAPSED))
	{
	  GTK_RBNODE_UNSET_FLAG (node, GTK_RBNODE_IS_SEMI_COLLAPSED);

	  redraw = TRUE;

	}
    }

  if (redraw)
    {
      gtk_tree_view_queue_draw_arrow (tree_view, tree, node, NULL);

      return TRUE;
    }

  return FALSE;
}

/**
 * gtk_tree_view_collapse_all:
 * @tree_view: A #GtkTreeView.
 *
 * Recursively collapses all visible, expanded nodes in @tree_view.
 **/
void
gtk_tree_view_collapse_all (GtkTreeView *tree_view)
{
  GtkRBTree *tree;
  GtkRBNode *node;
  GtkTreePath *path;
  gint *indices;

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  if (tree_view->priv->tree == NULL)
    return;

  path = gtk_tree_path_new ();
  gtk_tree_path_down (path);
  indices = gtk_tree_path_get_indices (path);

  tree = tree_view->priv->tree;
  node = tree->root;
  while (node && node->left != tree->nil)
    node = node->left;

  while (node)
    {
      if (node->children)
	gtk_tree_view_real_collapse_row (tree_view, path, tree, node, FALSE);
      indices[0]++;
      node = _gtk_rbtree_next (tree, node);
    }

  gtk_tree_path_free (path);
}

/**
 * gtk_tree_view_expand_to_path:
 * @tree_view: A #GtkTreeView.
 * @path: path to a row.
 *
 * Expands the row at @path. This will also expand all parent rows of
 * @path as necessary.
 *
 * Since: 2.2
 **/
void
gtk_tree_view_expand_to_path (GtkTreeView *tree_view,
			      GtkTreePath *path)
{
  gint i, depth;
  gint *indices;
  GtkTreePath *tmp;

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (path != NULL);

  depth = gtk_tree_path_get_depth (path);
  indices = gtk_tree_path_get_indices (path);

  tmp = gtk_tree_path_new ();
  g_return_if_fail (tmp != NULL);

  for (i = 0; i < depth; i++)
    {
      gtk_tree_path_append_index (tmp, indices[i]);
      gtk_tree_view_expand_row (tree_view, tmp, FALSE);
    }

  gtk_tree_path_free (tmp);
}

/* FIXME the bool return values for expand_row and collapse_row are
 * not analagous; they should be TRUE if the row had children and
 * was not already in the requested state.
 */


static gboolean
gtk_tree_view_real_expand_row (GtkTreeView *tree_view,
			       GtkTreePath *path,
			       GtkRBTree   *tree,
			       GtkRBNode   *node,
			       gboolean     open_all,
			       gboolean     animate)
{
  GtkTreeIter iter;
  GtkTreeIter temp;
  gboolean expand;

  if (animate)
    g_object_get (gtk_widget_get_settings (GTK_WIDGET (tree_view)),
                  "gtk-enable-animations", &animate,
                  NULL);

  remove_auto_expand_timeout (tree_view);

  if (node->children && !open_all)
    return FALSE;

  if (! GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_PARENT))
    return FALSE;

  gtk_tree_model_get_iter (tree_view->priv->model, &iter, path);
  if (! gtk_tree_model_iter_has_child (tree_view->priv->model, &iter))
    return FALSE;


   if (node->children && open_all)
    {
      gboolean retval = FALSE;
      GtkTreePath *tmp_path = gtk_tree_path_copy (path);

      gtk_tree_path_append_index (tmp_path, 0);
      tree = node->children;
      node = tree->root;
      while (node->left != tree->nil)
	node = node->left;
      /* try to expand the children */
      do
        {
         gboolean t;
	 t = gtk_tree_view_real_expand_row (tree_view, tmp_path, tree, node,
					    TRUE, animate);
         if (t)
           retval = TRUE;

         gtk_tree_path_next (tmp_path);
	 node = _gtk_rbtree_next (tree, node);
       }
      while (node != NULL);

      gtk_tree_path_free (tmp_path);

      return retval;
    }

  g_signal_emit (tree_view, tree_view_signals[TEST_EXPAND_ROW], 0, &iter, path, &expand);

  if (!gtk_tree_model_iter_has_child (tree_view->priv->model, &iter))
    return FALSE;

  if (expand)
    return FALSE;

  node->children = _gtk_rbtree_new ();
  node->children->parent_tree = tree;
  node->children->parent_node = node;

  gtk_tree_model_iter_children (tree_view->priv->model, &temp, &iter);

  gtk_tree_view_build_tree (tree_view,
			    node->children,
			    &temp,
			    gtk_tree_path_get_depth (path) + 1,
			    open_all);

  remove_expand_collapse_timeout (tree_view);

  if (animate)
    add_expand_collapse_timeout (tree_view, tree, node, TRUE);

  install_presize_handler (tree_view);

  g_signal_emit (tree_view, tree_view_signals[ROW_EXPANDED], 0, &iter, path);
  if (open_all && node->children)
    {
      _gtk_rbtree_traverse (node->children,
                            node->children->root,
                            G_PRE_ORDER,
                            gtk_tree_view_expand_all_emission_helper,
                            tree_view);
    }
  return TRUE;
}


/**
 * gtk_tree_view_expand_row:
 * @tree_view: a #GtkTreeView
 * @path: path to a row
 * @open_all: whether to recursively expand, or just expand immediate children
 *
 * Opens the row so its children are visible.
 *
 * Return value: %TRUE if the row existed and had children
 **/
gboolean
gtk_tree_view_expand_row (GtkTreeView *tree_view,
			  GtkTreePath *path,
			  gboolean     open_all)
{
  GtkRBTree *tree;
  GtkRBNode *node;

  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), FALSE);
  g_return_val_if_fail (tree_view->priv->model != NULL, FALSE);
  g_return_val_if_fail (path != NULL, FALSE);

  if (_gtk_tree_view_find_node (tree_view,
				path,
				&tree,
				&node))
    return FALSE;

  if (tree != NULL)
    return gtk_tree_view_real_expand_row (tree_view, path, tree, node, open_all, FALSE);
  else
    return FALSE;
}

static gboolean
gtk_tree_view_real_collapse_row (GtkTreeView *tree_view,
				 GtkTreePath *path,
				 GtkRBTree   *tree,
				 GtkRBNode   *node,
				 gboolean     animate)
{
  GtkTreeIter iter;
  GtkTreeIter children;
  gboolean collapse;
  gint x, y;
  GList *list;
  GdkWindow *child, *parent;

  if (animate)
    g_object_get (gtk_widget_get_settings (GTK_WIDGET (tree_view)),
                  "gtk-enable-animations", &animate,
                  NULL);

  remove_auto_expand_timeout (tree_view);

  if (node->children == NULL)
    return FALSE;

  gtk_tree_model_get_iter (tree_view->priv->model, &iter, path);

  g_signal_emit (tree_view, tree_view_signals[TEST_COLLAPSE_ROW], 0, &iter, path, &collapse);

  if (collapse)
    return FALSE;

  /* if the prelighted node is a child of us, we want to unprelight it.  We have
   * a chance to prelight the correct node below */

  if (tree_view->priv->prelight_tree)
    {
      GtkRBTree *parent_tree;
      GtkRBNode *parent_node;

      parent_tree = tree_view->priv->prelight_tree->parent_tree;
      parent_node = tree_view->priv->prelight_tree->parent_node;
      while (parent_tree)
	{
	  if (parent_tree == tree && parent_node == node)
	    {
	      ensure_unprelighted (tree_view);
	      break;
	    }
	  parent_node = parent_tree->parent_node;
	  parent_tree = parent_tree->parent_tree;
	}
    }

  TREE_VIEW_INTERNAL_ASSERT (gtk_tree_model_iter_children (tree_view->priv->model, &children, &iter), FALSE);

  for (list = tree_view->priv->columns; list; list = list->next)
    {
      GtkTreeViewColumn *column = list->data;

      if (column->visible == FALSE)
	continue;
      if (gtk_tree_view_column_get_sizing (column) == GTK_TREE_VIEW_COLUMN_AUTOSIZE)
	_gtk_tree_view_column_cell_set_dirty (column, TRUE);
    }

  if (tree_view->priv->destroy_count_func)
    {
      GtkTreePath *child_path;
      gint child_count = 0;
      child_path = gtk_tree_path_copy (path);
      gtk_tree_path_down (child_path);
      if (node->children)
	_gtk_rbtree_traverse (node->children, node->children->root, G_POST_ORDER, count_children_helper, &child_count);
      tree_view->priv->destroy_count_func (tree_view, child_path, child_count, tree_view->priv->destroy_count_data);
      gtk_tree_path_free (child_path);
    }

  if (gtk_tree_row_reference_valid (tree_view->priv->cursor))
    {
      GtkTreePath *cursor_path = gtk_tree_row_reference_get_path (tree_view->priv->cursor);

      if (gtk_tree_path_is_ancestor (path, cursor_path))
	{
	  gtk_tree_row_reference_free (tree_view->priv->cursor);
	  tree_view->priv->cursor = gtk_tree_row_reference_new_proxy (G_OBJECT (tree_view),
								      tree_view->priv->model,
								      path);
	}
      gtk_tree_path_free (cursor_path);
    }

  if (gtk_tree_row_reference_valid (tree_view->priv->anchor))
    {
      GtkTreePath *anchor_path = gtk_tree_row_reference_get_path (tree_view->priv->anchor);
      if (gtk_tree_path_is_ancestor (path, anchor_path))
	{
	  gtk_tree_row_reference_free (tree_view->priv->anchor);
	  tree_view->priv->anchor = NULL;
	}
      gtk_tree_path_free (anchor_path);
    }

  /* Stop a pending double click */
  tree_view->priv->last_button_x = -1;
  tree_view->priv->last_button_y = -1;

  remove_expand_collapse_timeout (tree_view);

  if (gtk_tree_view_unref_and_check_selection_tree (tree_view, node->children))
    {
      _gtk_rbtree_remove (node->children);
      g_signal_emit_by_name (tree_view->priv->selection, "changed");
    }
  else
    _gtk_rbtree_remove (node->children);
  
  if (animate)
    add_expand_collapse_timeout (tree_view, tree, node, FALSE);
  
  if (gtk_widget_get_mapped (GTK_WIDGET (tree_view)))
    {
      gtk_widget_queue_resize (GTK_WIDGET (tree_view));
    }

  g_signal_emit (tree_view, tree_view_signals[ROW_COLLAPSED], 0, &iter, path);

  if (gtk_widget_get_mapped (GTK_WIDGET (tree_view)))
    {
      /* now that we've collapsed all rows, we want to try to set the prelight
       * again. To do this, we fake a motion event and send it to ourselves. */

      child = tree_view->priv->bin_window;
      parent = gdk_window_get_parent (child);

      if (gdk_window_get_pointer (parent, &x, &y, NULL) == child)
	{
	  GdkEventMotion event;
	  gint child_x, child_y;

	  gdk_window_get_position (child, &child_x, &child_y);

	  event.window = tree_view->priv->bin_window;
	  event.x = x - child_x;
	  event.y = y - child_y;

	  /* despite the fact this isn't a real event, I'm almost positive it will
	   * never trigger a drag event.  maybe_drag is the only function that uses
	   * more than just event.x and event.y. */
	  gtk_tree_view_motion_bin_window (GTK_WIDGET (tree_view), &event);
	}
    }

  return TRUE;
}

/**
 * gtk_tree_view_collapse_row:
 * @tree_view: a #GtkTreeView
 * @path: path to a row in the @tree_view
 *
 * Collapses a row (hides its child rows, if they exist).
 *
 * Return value: %TRUE if the row was collapsed.
 **/
gboolean
gtk_tree_view_collapse_row (GtkTreeView *tree_view,
			    GtkTreePath *path)
{
  GtkRBTree *tree;
  GtkRBNode *node;

  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), FALSE);
  g_return_val_if_fail (tree_view->priv->tree != NULL, FALSE);
  g_return_val_if_fail (path != NULL, FALSE);

  if (_gtk_tree_view_find_node (tree_view,
				path,
				&tree,
				&node))
    return FALSE;

  if (tree == NULL || node->children == NULL)
    return FALSE;

  return gtk_tree_view_real_collapse_row (tree_view, path, tree, node, FALSE);
}

static void
gtk_tree_view_map_expanded_rows_helper (GtkTreeView            *tree_view,
					GtkRBTree              *tree,
					GtkTreePath            *path,
					GtkTreeViewMappingFunc  func,
					gpointer                user_data)
{
  GtkRBNode *node;

  if (tree == NULL || tree->root == NULL)
    return;

  node = tree->root;

  while (node && node->left != tree->nil)
    node = node->left;

  while (node)
    {
      if (node->children)
	{
	  (* func) (tree_view, path, user_data);
	  gtk_tree_path_down (path);
	  gtk_tree_view_map_expanded_rows_helper (tree_view, node->children, path, func, user_data);
	  gtk_tree_path_up (path);
	}
      gtk_tree_path_next (path);
      node = _gtk_rbtree_next (tree, node);
    }
}

/**
 * gtk_tree_view_map_expanded_rows:
 * @tree_view: A #GtkTreeView
 * @func: (scope call): A function to be called
 * @data: User data to be passed to the function.
 *
 * Calls @func on all expanded rows.
 **/
void
gtk_tree_view_map_expanded_rows (GtkTreeView            *tree_view,
				 GtkTreeViewMappingFunc  func,
				 gpointer                user_data)
{
  GtkTreePath *path;

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (func != NULL);

  path = gtk_tree_path_new_first ();

  gtk_tree_view_map_expanded_rows_helper (tree_view,
					  tree_view->priv->tree,
					  path, func, user_data);

  gtk_tree_path_free (path);
}

/**
 * gtk_tree_view_row_expanded:
 * @tree_view: A #GtkTreeView.
 * @path: A #GtkTreePath to test expansion state.
 *
 * Returns %TRUE if the node pointed to by @path is expanded in @tree_view.
 *
 * Return value: %TRUE if #path is expanded.
 **/
gboolean
gtk_tree_view_row_expanded (GtkTreeView *tree_view,
			    GtkTreePath *path)
{
  GtkRBTree *tree;
  GtkRBNode *node;

  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), FALSE);
  g_return_val_if_fail (path != NULL, FALSE);

  _gtk_tree_view_find_node (tree_view, path, &tree, &node);

  if (node == NULL)
    return FALSE;

  return (node->children != NULL);
}

/**
 * gtk_tree_view_get_reorderable:
 * @tree_view: a #GtkTreeView
 *
 * Retrieves whether the user can reorder the tree via drag-and-drop. See
 * gtk_tree_view_set_reorderable().
 *
 * Return value: %TRUE if the tree can be reordered.
 **/
gboolean
gtk_tree_view_get_reorderable (GtkTreeView *tree_view)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), FALSE);

  return tree_view->priv->reorderable;
}

/**
 * gtk_tree_view_set_reorderable:
 * @tree_view: A #GtkTreeView.
 * @reorderable: %TRUE, if the tree can be reordered.
 *
 * This function is a convenience function to allow you to reorder
 * models that support the #GtkDragSourceIface and the
 * #GtkDragDestIface.  Both #GtkTreeStore and #GtkListStore support
 * these.  If @reorderable is %TRUE, then the user can reorder the
 * model by dragging and dropping rows. The developer can listen to
 * these changes by connecting to the model's row_inserted and
 * row_deleted signals. The reordering is implemented by setting up
 * the tree view as a drag source and destination. Therefore, drag and
 * drop can not be used in a reorderable view for any other purpose.
 *
 * This function does not give you any degree of control over the order -- any
 * reordering is allowed.  If more control is needed, you should probably
 * handle drag and drop manually.
 **/
void
gtk_tree_view_set_reorderable (GtkTreeView *tree_view,
			       gboolean     reorderable)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  reorderable = reorderable != FALSE;

  if (tree_view->priv->reorderable == reorderable)
    return;

  if (reorderable)
    {
      const GtkTargetEntry row_targets[] = {
        { "GTK_TREE_MODEL_ROW", GTK_TARGET_SAME_WIDGET, 0 }
      };

      gtk_tree_view_enable_model_drag_source (tree_view,
					      GDK_BUTTON1_MASK,
					      row_targets,
					      G_N_ELEMENTS (row_targets),
					      GDK_ACTION_MOVE);
      gtk_tree_view_enable_model_drag_dest (tree_view,
					    row_targets,
					    G_N_ELEMENTS (row_targets),
					    GDK_ACTION_MOVE);
    }
  else
    {
      gtk_tree_view_unset_rows_drag_source (tree_view);
      gtk_tree_view_unset_rows_drag_dest (tree_view);
    }

  tree_view->priv->reorderable = reorderable;

  g_object_notify (G_OBJECT (tree_view), "reorderable");
}

static void
gtk_tree_view_real_set_cursor (GtkTreeView     *tree_view,
			       GtkTreePath     *path,
			       gboolean         clear_and_select,
			       gboolean         clamp_node)
{
  GtkRBTree *tree = NULL;
  GtkRBNode *node = NULL;

  if (gtk_tree_row_reference_valid (tree_view->priv->cursor))
    {
      GtkTreePath *cursor_path;
      cursor_path = gtk_tree_row_reference_get_path (tree_view->priv->cursor);
      gtk_tree_view_queue_draw_path (tree_view, cursor_path, NULL);
      gtk_tree_path_free (cursor_path);
    }

  gtk_tree_row_reference_free (tree_view->priv->cursor);
  tree_view->priv->cursor = NULL;

  /* One cannot set the cursor on a separator.   Also, if
   * _gtk_tree_view_find_node returns TRUE, it ran out of tree
   * before finding the tree and node belonging to path.  The
   * path maps to a non-existing path and we will silently bail out.
   * We unset tree and node to avoid further processing.
   */
  if (!row_is_separator (tree_view, NULL, path)
      && _gtk_tree_view_find_node (tree_view, path, &tree, &node) == FALSE)
    {
      tree_view->priv->cursor =
          gtk_tree_row_reference_new_proxy (G_OBJECT (tree_view),
                                            tree_view->priv->model,
                                            path);
    }
  else
    {
      tree = NULL;
      node = NULL;
    }

  if (tree != NULL)
    {
      GtkRBTree *new_tree = NULL;
      GtkRBNode *new_node = NULL;

      if (clear_and_select && !tree_view->priv->modify_selection_pressed)
        {
          GtkTreeSelectMode mode = 0;

          if (tree_view->priv->modify_selection_pressed)
            mode |= GTK_TREE_SELECT_MODE_TOGGLE;
          if (tree_view->priv->extend_selection_pressed)
            mode |= GTK_TREE_SELECT_MODE_EXTEND;

          _gtk_tree_selection_internal_select_node (tree_view->priv->selection,
                                                    node, tree, path, mode,
                                                    FALSE);
        }

      /* We have to re-find tree and node here again, somebody might have
       * cleared the node or the whole tree in the GtkTreeSelection::changed
       * callback. If the nodes differ we bail out here.
       */
      _gtk_tree_view_find_node (tree_view, path, &new_tree, &new_node);

      if (tree != new_tree || node != new_node)
        return;

      if (clamp_node)
        {
	  gtk_tree_view_clamp_node_visible (tree_view, tree, node);
	  _gtk_tree_view_queue_draw_node (tree_view, tree, node, NULL);
	}
    }

  g_signal_emit (tree_view, tree_view_signals[CURSOR_CHANGED], 0);
}

/**
 * gtk_tree_view_get_cursor:
 * @tree_view: A #GtkTreeView
 * @path: (out) (transfer full) (allow-none): A pointer to be filled with the current cursor path, or %NULL
 * @focus_column: (out) (transfer none) (allow-none): A pointer to be filled with the current focus column, or %NULL
 *
 * Fills in @path and @focus_column with the current path and focus column.  If
 * the cursor isn't currently set, then *@path will be %NULL.  If no column
 * currently has focus, then *@focus_column will be %NULL.
 *
 * The returned #GtkTreePath must be freed with gtk_tree_path_free() when
 * you are done with it.
 **/
void
gtk_tree_view_get_cursor (GtkTreeView        *tree_view,
			  GtkTreePath       **path,
			  GtkTreeViewColumn **focus_column)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  if (path)
    {
      if (gtk_tree_row_reference_valid (tree_view->priv->cursor))
	*path = gtk_tree_row_reference_get_path (tree_view->priv->cursor);
      else
	*path = NULL;
    }

  if (focus_column)
    {
      *focus_column = tree_view->priv->focus_column;
    }
}

/**
 * gtk_tree_view_set_cursor:
 * @tree_view: A #GtkTreeView
 * @path: A #GtkTreePath
 * @focus_column: (allow-none): A #GtkTreeViewColumn, or %NULL
 * @start_editing: %TRUE if the specified cell should start being edited.
 *
 * Sets the current keyboard focus to be at @path, and selects it.  This is
 * useful when you want to focus the user's attention on a particular row.  If
 * @focus_column is not %NULL, then focus is given to the column specified by 
 * it. Additionally, if @focus_column is specified, and @start_editing is 
 * %TRUE, then editing should be started in the specified cell.  
 * This function is often followed by @gtk_widget_grab_focus (@tree_view) 
 * in order to give keyboard focus to the widget.  Please note that editing 
 * can only happen when the widget is realized.
 *
 * If @path is invalid for @model, the current cursor (if any) will be unset
 * and the function will return without failing.
 **/
void
gtk_tree_view_set_cursor (GtkTreeView       *tree_view,
			  GtkTreePath       *path,
			  GtkTreeViewColumn *focus_column,
			  gboolean           start_editing)
{
  gtk_tree_view_set_cursor_on_cell (tree_view, path, focus_column,
				    NULL, start_editing);
}

/**
 * gtk_tree_view_set_cursor_on_cell:
 * @tree_view: A #GtkTreeView
 * @path: A #GtkTreePath
 * @focus_column: (allow-none): A #GtkTreeViewColumn, or %NULL
 * @focus_cell: (allow-none): A #GtkCellRenderer, or %NULL
 * @start_editing: %TRUE if the specified cell should start being edited.
 *
 * Sets the current keyboard focus to be at @path, and selects it.  This is
 * useful when you want to focus the user's attention on a particular row.  If
 * @focus_column is not %NULL, then focus is given to the column specified by
 * it. If @focus_column and @focus_cell are not %NULL, and @focus_column
 * contains 2 or more editable or activatable cells, then focus is given to
 * the cell specified by @focus_cell. Additionally, if @focus_column is
 * specified, and @start_editing is %TRUE, then editing should be started in
 * the specified cell.  This function is often followed by
 * @gtk_widget_grab_focus (@tree_view) in order to give keyboard focus to the
 * widget.  Please note that editing can only happen when the widget is
 * realized.
 *
 * If @path is invalid for @model, the current cursor (if any) will be unset
 * and the function will return without failing.
 *
 * Since: 2.2
 **/
void
gtk_tree_view_set_cursor_on_cell (GtkTreeView       *tree_view,
				  GtkTreePath       *path,
				  GtkTreeViewColumn *focus_column,
				  GtkCellRenderer   *focus_cell,
				  gboolean           start_editing)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (path != NULL);
  g_return_if_fail (focus_column == NULL || GTK_IS_TREE_VIEW_COLUMN (focus_column));

  if (!tree_view->priv->model)
    return;

  if (focus_cell)
    {
      g_return_if_fail (focus_column);
      g_return_if_fail (GTK_IS_CELL_RENDERER (focus_cell));
    }

  /* cancel the current editing, if it exists */
  if (tree_view->priv->edited_column &&
      tree_view->priv->edited_column->editable_widget)
    gtk_tree_view_stop_editing (tree_view, TRUE);

  gtk_tree_view_real_set_cursor (tree_view, path, TRUE, TRUE);

  if (focus_column && focus_column->visible)
    {
      GList *list;
      gboolean column_in_tree = FALSE;

      for (list = tree_view->priv->columns; list; list = list->next)
	if (list->data == focus_column)
	  {
	    column_in_tree = TRUE;
	    break;
	  }
      g_return_if_fail (column_in_tree);
      tree_view->priv->focus_column = focus_column;
      if (focus_cell)
	gtk_tree_view_column_focus_cell (focus_column, focus_cell);
      if (start_editing)
	gtk_tree_view_start_editing (tree_view, path);
    }
}

/**
 * gtk_tree_view_get_bin_window:
 * @tree_view: A #GtkTreeView
 *
 * Returns the window that @tree_view renders to.
 * This is used primarily to compare to <literal>event->window</literal>
 * to confirm that the event on @tree_view is on the right window.
 *
 * Return value: (transfer none): A #GdkWindow, or %NULL when @tree_view
 *     hasn't been realized yet
 **/
GdkWindow *
gtk_tree_view_get_bin_window (GtkTreeView *tree_view)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), NULL);

  return tree_view->priv->bin_window;
}

/**
 * gtk_tree_view_get_path_at_pos:
 * @tree_view: A #GtkTreeView.
 * @x: The x position to be identified (relative to bin_window).
 * @y: The y position to be identified (relative to bin_window).
 * @path: (out) (allow-none): A pointer to a #GtkTreePath pointer to be filled in, or %NULL
 * @column: (out) (transfer none) (allow-none): A pointer to a #GtkTreeViewColumn pointer to be filled in, or %NULL
 * @cell_x: (out) (allow-none): A pointer where the X coordinate relative to the cell can be placed, or %NULL
 * @cell_y: (out) (allow-none): A pointer where the Y coordinate relative to the cell can be placed, or %NULL
 *
 * Finds the path at the point (@x, @y), relative to bin_window coordinates
 * (please see gtk_tree_view_get_bin_window()).
 * That is, @x and @y are relative to an events coordinates. @x and @y must
 * come from an event on the @tree_view only where <literal>event->window ==
 * gtk_tree_view_get_bin_window (<!-- -->)</literal>. It is primarily for
 * things like popup menus. If @path is non-%NULL, then it will be filled
 * with the #GtkTreePath at that point.  This path should be freed with
 * gtk_tree_path_free().  If @column is non-%NULL, then it will be filled
 * with the column at that point.  @cell_x and @cell_y return the coordinates
 * relative to the cell background (i.e. the @background_area passed to
 * gtk_cell_renderer_render()).  This function is only meaningful if
 * @tree_view is realized.  Therefore this function will always return %FALSE
 * if @tree_view is not realized or does not have a model.
 *
 * For converting widget coordinates (eg. the ones you get from
 * GtkWidget::query-tooltip), please see
 * gtk_tree_view_convert_widget_to_bin_window_coords().
 *
 * Return value: %TRUE if a row exists at that coordinate.
 **/
gboolean
gtk_tree_view_get_path_at_pos (GtkTreeView        *tree_view,
			       gint                x,
			       gint                y,
			       GtkTreePath       **path,
			       GtkTreeViewColumn **column,
                               gint               *cell_x,
                               gint               *cell_y)
{
  GtkRBTree *tree;
  GtkRBNode *node;
  gint y_offset;

  g_return_val_if_fail (tree_view != NULL, FALSE);

  if (path)
    *path = NULL;
  if (column)
    *column = NULL;

  if (tree_view->priv->bin_window == NULL)
    return FALSE;

  if (tree_view->priv->tree == NULL)
    return FALSE;

  if (x > tree_view->priv->hadjustment->upper)
    return FALSE;

  if (x < 0 || y < 0)
    return FALSE;

  if (column || cell_x)
    {
      GtkTreeViewColumn *tmp_column;
      GtkTreeViewColumn *last_column = NULL;
      GList *list;
      gint remaining_x = x;
      gboolean found = FALSE;
      gboolean rtl;

      rtl = (gtk_widget_get_direction (GTK_WIDGET (tree_view)) == GTK_TEXT_DIR_RTL);
      for (list = (rtl ? g_list_last (tree_view->priv->columns) : g_list_first (tree_view->priv->columns));
	   list;
	   list = (rtl ? list->prev : list->next))
	{
	  tmp_column = list->data;

	  if (tmp_column->visible == FALSE)
	    continue;

	  last_column = tmp_column;
	  if (remaining_x <= tmp_column->width)
	    {
              found = TRUE;

              if (column)
                *column = tmp_column;

              if (cell_x)
                *cell_x = remaining_x;

	      break;
	    }
	  remaining_x -= tmp_column->width;
	}

      /* If found is FALSE and there is a last_column, then it the remainder
       * space is in that area
       */
      if (!found)
        {
	  if (last_column)
	    {
	      if (column)
		*column = last_column;
	      
	      if (cell_x)
		*cell_x = last_column->width + remaining_x;
	    }
	  else
	    {
	      return FALSE;
	    }
	}
    }

  y_offset = _gtk_rbtree_find_offset (tree_view->priv->tree,
				      TREE_WINDOW_Y_TO_RBTREE_Y (tree_view, y),
				      &tree, &node);

  if (tree == NULL)
    return FALSE;

  if (cell_y)
    *cell_y = y_offset;

  if (path)
    *path = _gtk_tree_view_find_path (tree_view, tree, node);

  return TRUE;
}


/**
 * gtk_tree_view_get_cell_area:
 * @tree_view: a #GtkTreeView
 * @path: (allow-none): a #GtkTreePath for the row, or %NULL to get only horizontal coordinates
 * @column: (allow-none): a #GtkTreeViewColumn for the column, or %NULL to get only vertical coordinates
 * @rect: (out): rectangle to fill with cell rect
 *
 * Fills the bounding rectangle in bin_window coordinates for the cell at the
 * row specified by @path and the column specified by @column.  If @path is
 * %NULL, or points to a path not currently displayed, the @y and @height fields
 * of the rectangle will be filled with 0. If @column is %NULL, the @x and @width
 * fields will be filled with 0.  The sum of all cell rects does not cover the
 * entire tree; there are extra pixels in between rows, for example. The
 * returned rectangle is equivalent to the @cell_area passed to
 * gtk_cell_renderer_render().  This function is only valid if @tree_view is
 * realized.
 **/
void
gtk_tree_view_get_cell_area (GtkTreeView        *tree_view,
                             GtkTreePath        *path,
                             GtkTreeViewColumn  *column,
                             GdkRectangle       *rect)
{
  GtkRBTree *tree = NULL;
  GtkRBNode *node = NULL;
  gint vertical_separator;
  gint horizontal_separator;

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (column == NULL || GTK_IS_TREE_VIEW_COLUMN (column));
  g_return_if_fail (rect != NULL);
  g_return_if_fail (!column || column->tree_view == (GtkWidget *) tree_view);
  g_return_if_fail (gtk_widget_get_realized (GTK_WIDGET (tree_view)));

  gtk_widget_style_get (GTK_WIDGET (tree_view),
			"vertical-separator", &vertical_separator,
			"horizontal-separator", &horizontal_separator,
			NULL);

  rect->x = 0;
  rect->y = 0;
  rect->width = 0;
  rect->height = 0;

  if (column)
    {
      rect->x = column->button->allocation.x + horizontal_separator/2;
      rect->width = column->button->allocation.width - horizontal_separator;
    }

  if (path)
    {
      gboolean ret = _gtk_tree_view_find_node (tree_view, path, &tree, &node);

      /* Get vertical coords */
      if ((!ret && tree == NULL) || ret)
	return;

      rect->y = CELL_FIRST_PIXEL (tree_view, tree, node, vertical_separator);
      rect->height = MAX (CELL_HEIGHT (node, vertical_separator), tree_view->priv->expander_size - vertical_separator);

      if (column &&
	  gtk_tree_view_is_expander_column (tree_view, column))
	{
	  gint depth = gtk_tree_path_get_depth (path);
	  gboolean rtl;

	  rtl = gtk_widget_get_direction (GTK_WIDGET (tree_view)) == GTK_TEXT_DIR_RTL;

	  if (!rtl)
	    rect->x += (depth - 1) * tree_view->priv->level_indentation;
	  rect->width -= (depth - 1) * tree_view->priv->level_indentation;

	  if (TREE_VIEW_DRAW_EXPANDERS (tree_view))
	    {
	      if (!rtl)
		rect->x += depth * tree_view->priv->expander_size;
	      rect->width -= depth * tree_view->priv->expander_size;
	    }

	  rect->width = MAX (rect->width, 0);
	}
    }
}

/**
 * gtk_tree_view_get_background_area:
 * @tree_view: a #GtkTreeView
 * @path: (allow-none): a #GtkTreePath for the row, or %NULL to get only horizontal coordinates
 * @column: (allow-none): a #GtkTreeViewColumn for the column, or %NULL to get only vertical coordiantes
 * @rect: (out): rectangle to fill with cell background rect
 *
 * Fills the bounding rectangle in bin_window coordinates for the cell at the
 * row specified by @path and the column specified by @column.  If @path is
 * %NULL, or points to a node not found in the tree, the @y and @height fields of
 * the rectangle will be filled with 0. If @column is %NULL, the @x and @width
 * fields will be filled with 0.  The returned rectangle is equivalent to the
 * @background_area passed to gtk_cell_renderer_render().  These background
 * areas tile to cover the entire bin window.  Contrast with the @cell_area,
 * returned by gtk_tree_view_get_cell_area(), which returns only the cell
 * itself, excluding surrounding borders and the tree expander area.
 *
 **/
void
gtk_tree_view_get_background_area (GtkTreeView        *tree_view,
                                   GtkTreePath        *path,
                                   GtkTreeViewColumn  *column,
                                   GdkRectangle       *rect)
{
  GtkRBTree *tree = NULL;
  GtkRBNode *node = NULL;

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (column == NULL || GTK_IS_TREE_VIEW_COLUMN (column));
  g_return_if_fail (rect != NULL);

  rect->x = 0;
  rect->y = 0;
  rect->width = 0;
  rect->height = 0;

  if (path)
    {
      /* Get vertical coords */

      if (!_gtk_tree_view_find_node (tree_view, path, &tree, &node) &&
	  tree == NULL)
	return;

      rect->y = BACKGROUND_FIRST_PIXEL (tree_view, tree, node);

      rect->height = ROW_HEIGHT (tree_view, BACKGROUND_HEIGHT (node));
    }

  if (column)
    {
      gint x2 = 0;

      gtk_tree_view_get_background_xrange (tree_view, tree, column, &rect->x, &x2);
      rect->width = x2 - rect->x;
    }
}

/**
 * gtk_tree_view_get_visible_rect:
 * @tree_view: a #GtkTreeView
 * @visible_rect: (out): rectangle to fill
 *
 * Fills @visible_rect with the currently-visible region of the
 * buffer, in tree coordinates. Convert to bin_window coordinates with
 * gtk_tree_view_convert_tree_to_bin_window_coords().
 * Tree coordinates start at 0,0 for row 0 of the tree, and cover the entire
 * scrollable area of the tree.
 **/
void
gtk_tree_view_get_visible_rect (GtkTreeView  *tree_view,
                                GdkRectangle *visible_rect)
{
  GtkWidget *widget;

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  widget = GTK_WIDGET (tree_view);

  if (visible_rect)
    {
      visible_rect->x = tree_view->priv->hadjustment->value;
      visible_rect->y = tree_view->priv->vadjustment->value;
      visible_rect->width = widget->allocation.width;
      visible_rect->height = widget->allocation.height - TREE_VIEW_HEADER_HEIGHT (tree_view);
    }
}

/**
 * gtk_tree_view_widget_to_tree_coords:
 * @tree_view: a #GtkTreeView
 * @wx: X coordinate relative to bin_window
 * @wy: Y coordinate relative to bin_window
 * @tx: return location for tree X coordinate
 * @ty: return location for tree Y coordinate
 *
 * Converts bin_window coordinates to coordinates for the
 * tree (the full scrollable area of the tree).
 *
 * Deprecated: 2.12: Due to historial reasons the name of this function is
 * incorrect.  For converting coordinates relative to the widget to
 * bin_window coordinates, please see
 * gtk_tree_view_convert_widget_to_bin_window_coords().
 *
 **/
void
gtk_tree_view_widget_to_tree_coords (GtkTreeView *tree_view,
				      gint         wx,
				      gint         wy,
				      gint        *tx,
				      gint        *ty)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  if (tx)
    *tx = wx + tree_view->priv->hadjustment->value;
  if (ty)
    *ty = wy + tree_view->priv->dy;
}

/**
 * gtk_tree_view_tree_to_widget_coords:
 * @tree_view: a #GtkTreeView
 * @tx: tree X coordinate
 * @ty: tree Y coordinate
 * @wx: return location for X coordinate relative to bin_window
 * @wy: return location for Y coordinate relative to bin_window
 *
 * Converts tree coordinates (coordinates in full scrollable area of the tree)
 * to bin_window coordinates.
 *
 * Deprecated: 2.12: Due to historial reasons the name of this function is
 * incorrect.  For converting bin_window coordinates to coordinates relative
 * to bin_window, please see
 * gtk_tree_view_convert_bin_window_to_widget_coords().
 *
 **/
void
gtk_tree_view_tree_to_widget_coords (GtkTreeView *tree_view,
                                     gint         tx,
                                     gint         ty,
                                     gint        *wx,
                                     gint        *wy)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  if (wx)
    *wx = tx - tree_view->priv->hadjustment->value;
  if (wy)
    *wy = ty - tree_view->priv->dy;
}


/**
 * gtk_tree_view_convert_widget_to_tree_coords:
 * @tree_view: a #GtkTreeView
 * @wx: X coordinate relative to the widget
 * @wy: Y coordinate relative to the widget
 * @tx: (out): return location for tree X coordinate
 * @ty: (out): return location for tree Y coordinate
 *
 * Converts widget coordinates to coordinates for the
 * tree (the full scrollable area of the tree).
 *
 * Since: 2.12
 **/
void
gtk_tree_view_convert_widget_to_tree_coords (GtkTreeView *tree_view,
                                             gint         wx,
                                             gint         wy,
                                             gint        *tx,
                                             gint        *ty)
{
  gint x, y;

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  gtk_tree_view_convert_widget_to_bin_window_coords (tree_view,
						     wx, wy,
						     &x, &y);
  gtk_tree_view_convert_bin_window_to_tree_coords (tree_view,
						   x, y,
						   tx, ty);
}

/**
 * gtk_tree_view_convert_tree_to_widget_coords:
 * @tree_view: a #GtkTreeView
 * @tx: X coordinate relative to the tree
 * @ty: Y coordinate relative to the tree
 * @wx: (out): return location for widget X coordinate
 * @wy: (out): return location for widget Y coordinate
 *
 * Converts tree coordinates (coordinates in full scrollable area of the tree)
 * to widget coordinates.
 *
 * Since: 2.12
 **/
void
gtk_tree_view_convert_tree_to_widget_coords (GtkTreeView *tree_view,
                                             gint         tx,
                                             gint         ty,
                                             gint        *wx,
                                             gint        *wy)
{
  gint x, y;

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  gtk_tree_view_convert_tree_to_bin_window_coords (tree_view,
						   tx, ty,
						   &x, &y);
  gtk_tree_view_convert_bin_window_to_widget_coords (tree_view,
						     x, y,
						     wx, wy);
}

/**
 * gtk_tree_view_convert_widget_to_bin_window_coords:
 * @tree_view: a #GtkTreeView
 * @wx: X coordinate relative to the widget
 * @wy: Y coordinate relative to the widget
 * @bx: (out): return location for bin_window X coordinate
 * @by: (out): return location for bin_window Y coordinate
 *
 * Converts widget coordinates to coordinates for the bin_window
 * (see gtk_tree_view_get_bin_window()).
 *
 * Since: 2.12
 **/
void
gtk_tree_view_convert_widget_to_bin_window_coords (GtkTreeView *tree_view,
                                                   gint         wx,
                                                   gint         wy,
                                                   gint        *bx,
                                                   gint        *by)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  if (bx)
    *bx = wx + tree_view->priv->hadjustment->value;
  if (by)
    *by = wy - TREE_VIEW_HEADER_HEIGHT (tree_view);
}

/**
 * gtk_tree_view_convert_bin_window_to_widget_coords:
 * @tree_view: a #GtkTreeView
 * @bx: bin_window X coordinate
 * @by: bin_window Y coordinate
 * @wx: (out): return location for widget X coordinate
 * @wy: (out): return location for widget Y coordinate
 *
 * Converts bin_window coordinates (see gtk_tree_view_get_bin_window())
 * to widget relative coordinates.
 *
 * Since: 2.12
 **/
void
gtk_tree_view_convert_bin_window_to_widget_coords (GtkTreeView *tree_view,
                                                   gint         bx,
                                                   gint         by,
                                                   gint        *wx,
                                                   gint        *wy)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  if (wx)
    *wx = bx - tree_view->priv->hadjustment->value;
  if (wy)
    *wy = by + TREE_VIEW_HEADER_HEIGHT (tree_view);
}

/**
 * gtk_tree_view_convert_tree_to_bin_window_coords:
 * @tree_view: a #GtkTreeView
 * @tx: tree X coordinate
 * @ty: tree Y coordinate
 * @bx: (out): return location for X coordinate relative to bin_window
 * @by: (out): return location for Y coordinate relative to bin_window
 *
 * Converts tree coordinates (coordinates in full scrollable area of the tree)
 * to bin_window coordinates.
 *
 * Since: 2.12
 **/
void
gtk_tree_view_convert_tree_to_bin_window_coords (GtkTreeView *tree_view,
                                                 gint         tx,
                                                 gint         ty,
                                                 gint        *bx,
                                                 gint        *by)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  if (bx)
    *bx = tx;
  if (by)
    *by = ty - tree_view->priv->dy;
}

/**
 * gtk_tree_view_convert_bin_window_to_tree_coords:
 * @tree_view: a #GtkTreeView
 * @bx: X coordinate relative to bin_window
 * @by: Y coordinate relative to bin_window
 * @tx: (out): return location for tree X coordinate
 * @ty: (out): return location for tree Y coordinate
 *
 * Converts bin_window coordinates to coordinates for the
 * tree (the full scrollable area of the tree).
 *
 * Since: 2.12
 **/
void
gtk_tree_view_convert_bin_window_to_tree_coords (GtkTreeView *tree_view,
                                                 gint         bx,
                                                 gint         by,
                                                 gint        *tx,
                                                 gint        *ty)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  if (tx)
    *tx = bx;
  if (ty)
    *ty = by + tree_view->priv->dy;
}



/**
 * gtk_tree_view_get_visible_range:
 * @tree_view: A #GtkTreeView
 * @start_path: (out) (allow-none): Return location for start of region,
 *              or %NULL.
 * @end_path: (out) (allow-none): Return location for end of region, or %NULL.
 *
 * Sets @start_path and @end_path to be the first and last visible path.
 * Note that there may be invisible paths in between.
 *
 * The paths should be freed with gtk_tree_path_free() after use.
 *
 * Returns: %TRUE, if valid paths were placed in @start_path and @end_path.
 *
 * Since: 2.8
 **/
gboolean
gtk_tree_view_get_visible_range (GtkTreeView  *tree_view,
                                 GtkTreePath **start_path,
                                 GtkTreePath **end_path)
{
  GtkRBTree *tree;
  GtkRBNode *node;
  gboolean retval;
  
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), FALSE);

  if (!tree_view->priv->tree)
    return FALSE;

  retval = TRUE;

  if (start_path)
    {
      _gtk_rbtree_find_offset (tree_view->priv->tree,
                               TREE_WINDOW_Y_TO_RBTREE_Y (tree_view, 0),
                               &tree, &node);
      if (node)
        *start_path = _gtk_tree_view_find_path (tree_view, tree, node);
      else
        retval = FALSE;
    }

  if (end_path)
    {
      gint y;

      if (tree_view->priv->height < tree_view->priv->vadjustment->page_size)
        y = tree_view->priv->height - 1;
      else
        y = TREE_WINDOW_Y_TO_RBTREE_Y (tree_view, tree_view->priv->vadjustment->page_size) - 1;

      _gtk_rbtree_find_offset (tree_view->priv->tree, y, &tree, &node);
      if (node)
        *end_path = _gtk_tree_view_find_path (tree_view, tree, node);
      else
        retval = FALSE;
    }

  return retval;
}

static void
unset_reorderable (GtkTreeView *tree_view)
{
  if (tree_view->priv->reorderable)
    {
      tree_view->priv->reorderable = FALSE;
      g_object_notify (G_OBJECT (tree_view), "reorderable");
    }
}

/**
 * gtk_tree_view_enable_model_drag_source:
 * @tree_view: a #GtkTreeView
 * @start_button_mask: Mask of allowed buttons to start drag
 * @targets: (array length=n_targets): the table of targets that the drag will support
 * @n_targets: the number of items in @targets
 * @actions: the bitmask of possible actions for a drag from this
 *    widget
 *
 * Turns @tree_view into a drag source for automatic DND. Calling this
 * method sets #GtkTreeView:reorderable to %FALSE.
 **/
void
gtk_tree_view_enable_model_drag_source (GtkTreeView              *tree_view,
					GdkModifierType           start_button_mask,
					const GtkTargetEntry     *targets,
					gint                      n_targets,
					GdkDragAction             actions)
{
  TreeViewDragInfo *di;

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  gtk_drag_source_set (GTK_WIDGET (tree_view),
		       0,
		       targets,
		       n_targets,
		       actions);

  di = ensure_info (tree_view);

  di->start_button_mask = start_button_mask;
  di->source_actions = actions;
  di->source_set = TRUE;

  unset_reorderable (tree_view);
}

/**
 * gtk_tree_view_enable_model_drag_dest:
 * @tree_view: a #GtkTreeView
 * @targets: (array length=n_targets): the table of targets that the drag will support
 * @n_targets: the number of items in @targets
 * @actions: the bitmask of possible actions for a drag from this
 *    widget
 * 
 * Turns @tree_view into a drop destination for automatic DND. Calling
 * this method sets #GtkTreeView:reorderable to %FALSE.
 **/
void
gtk_tree_view_enable_model_drag_dest (GtkTreeView              *tree_view,
				      const GtkTargetEntry     *targets,
				      gint                      n_targets,
				      GdkDragAction             actions)
{
  TreeViewDragInfo *di;

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  gtk_drag_dest_set (GTK_WIDGET (tree_view),
                     0,
                     targets,
                     n_targets,
                     actions);

  di = ensure_info (tree_view);
  di->dest_set = TRUE;

  unset_reorderable (tree_view);
}

/**
 * gtk_tree_view_unset_rows_drag_source:
 * @tree_view: a #GtkTreeView
 *
 * Undoes the effect of
 * gtk_tree_view_enable_model_drag_source(). Calling this method sets
 * #GtkTreeView:reorderable to %FALSE.
 **/
void
gtk_tree_view_unset_rows_drag_source (GtkTreeView *tree_view)
{
  TreeViewDragInfo *di;

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  di = get_info (tree_view);

  if (di)
    {
      if (di->source_set)
        {
          gtk_drag_source_unset (GTK_WIDGET (tree_view));
          di->source_set = FALSE;
        }

      if (!di->dest_set && !di->source_set)
        remove_info (tree_view);
    }
  
  unset_reorderable (tree_view);
}

/**
 * gtk_tree_view_unset_rows_drag_dest:
 * @tree_view: a #GtkTreeView
 *
 * Undoes the effect of
 * gtk_tree_view_enable_model_drag_dest(). Calling this method sets
 * #GtkTreeView:reorderable to %FALSE.
 **/
void
gtk_tree_view_unset_rows_drag_dest (GtkTreeView *tree_view)
{
  TreeViewDragInfo *di;

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  di = get_info (tree_view);

  if (di)
    {
      if (di->dest_set)
        {
          gtk_drag_dest_unset (GTK_WIDGET (tree_view));
          di->dest_set = FALSE;
        }

      if (!di->dest_set && !di->source_set)
        remove_info (tree_view);
    }

  unset_reorderable (tree_view);
}

/**
 * gtk_tree_view_set_drag_dest_row:
 * @tree_view: a #GtkTreeView
 * @path: (allow-none): The path of the row to highlight, or %NULL.
 * @pos: Specifies whether to drop before, after or into the row
 * 
 * Sets the row that is highlighted for feedback.
 **/
void
gtk_tree_view_set_drag_dest_row (GtkTreeView            *tree_view,
                                 GtkTreePath            *path,
                                 GtkTreeViewDropPosition pos)
{
  GtkTreePath *current_dest;

  /* Note; this function is exported to allow a custom DND
   * implementation, so it can't touch TreeViewDragInfo
   */

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  current_dest = NULL;

  if (tree_view->priv->drag_dest_row)
    {
      current_dest = gtk_tree_row_reference_get_path (tree_view->priv->drag_dest_row);
      gtk_tree_row_reference_free (tree_view->priv->drag_dest_row);
    }

  /* special case a drop on an empty model */
  tree_view->priv->empty_view_drop = 0;

  if (pos == GTK_TREE_VIEW_DROP_BEFORE && path
      && gtk_tree_path_get_depth (path) == 1
      && gtk_tree_path_get_indices (path)[0] == 0)
    {
      gint n_children;

      n_children = gtk_tree_model_iter_n_children (tree_view->priv->model,
                                                   NULL);

      if (!n_children)
        tree_view->priv->empty_view_drop = 1;
    }

  tree_view->priv->drag_dest_pos = pos;

  if (path)
    {
      tree_view->priv->drag_dest_row =
        gtk_tree_row_reference_new_proxy (G_OBJECT (tree_view), tree_view->priv->model, path);
      gtk_tree_view_queue_draw_path (tree_view, path, NULL);
    }
  else
    tree_view->priv->drag_dest_row = NULL;

  if (current_dest)
    {
      GtkRBTree *tree, *new_tree;
      GtkRBNode *node, *new_node;

      _gtk_tree_view_find_node (tree_view, current_dest, &tree, &node);
      _gtk_tree_view_queue_draw_node (tree_view, tree, node, NULL);

      if (tree && node)
	{
	  _gtk_rbtree_next_full (tree, node, &new_tree, &new_node);
	  if (new_tree && new_node)
	    _gtk_tree_view_queue_draw_node (tree_view, new_tree, new_node, NULL);

	  _gtk_rbtree_prev_full (tree, node, &new_tree, &new_node);
	  if (new_tree && new_node)
	    _gtk_tree_view_queue_draw_node (tree_view, new_tree, new_node, NULL);
	}
      gtk_tree_path_free (current_dest);
    }
}

/**
 * gtk_tree_view_get_drag_dest_row:
 * @tree_view: a #GtkTreeView
 * @path: (out) (allow-none): Return location for the path of the highlighted row, or %NULL.
 * @pos: (out) (allow-none): Return location for the drop position, or %NULL
 * 
 * Gets information about the row that is highlighted for feedback.
 **/
void
gtk_tree_view_get_drag_dest_row (GtkTreeView              *tree_view,
                                 GtkTreePath             **path,
                                 GtkTreeViewDropPosition  *pos)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  if (path)
    {
      if (tree_view->priv->drag_dest_row)
        *path = gtk_tree_row_reference_get_path (tree_view->priv->drag_dest_row);
      else
        {
          if (tree_view->priv->empty_view_drop)
            *path = gtk_tree_path_new_from_indices (0, -1);
          else
            *path = NULL;
        }
    }

  if (pos)
    *pos = tree_view->priv->drag_dest_pos;
}

/**
 * gtk_tree_view_get_dest_row_at_pos:
 * @tree_view: a #GtkTreeView
 * @drag_x: the position to determine the destination row for
 * @drag_y: the position to determine the destination row for
 * @path: (out) (allow-none): Return location for the path of the highlighted row, or %NULL.
 * @pos: (out) (allow-none): Return location for the drop position, or %NULL
 * 
 * Determines the destination row for a given position.  @drag_x and
 * @drag_y are expected to be in widget coordinates.  This function is only
 * meaningful if @tree_view is realized.  Therefore this function will always
 * return %FALSE if @tree_view is not realized or does not have a model.
 * 
 * Return value: whether there is a row at the given position, %TRUE if this
 * is indeed the case.
 **/
gboolean
gtk_tree_view_get_dest_row_at_pos (GtkTreeView             *tree_view,
                                   gint                     drag_x,
                                   gint                     drag_y,
                                   GtkTreePath            **path,
                                   GtkTreeViewDropPosition *pos)
{
  gint cell_y;
  gint bin_x, bin_y;
  gdouble offset_into_row;
  gdouble third;
  GdkRectangle cell;
  GtkTreeViewColumn *column = NULL;
  GtkTreePath *tmp_path = NULL;

  /* Note; this function is exported to allow a custom DND
   * implementation, so it can't touch TreeViewDragInfo
   */

  g_return_val_if_fail (tree_view != NULL, FALSE);
  g_return_val_if_fail (drag_x >= 0, FALSE);
  g_return_val_if_fail (drag_y >= 0, FALSE);

  if (path)
    *path = NULL;

  if (tree_view->priv->bin_window == NULL)
    return FALSE;

  if (tree_view->priv->tree == NULL)
    return FALSE;

  /* If in the top third of a row, we drop before that row; if
   * in the bottom third, drop after that row; if in the middle,
   * and the row has children, drop into the row.
   */
  gtk_tree_view_convert_widget_to_bin_window_coords (tree_view, drag_x, drag_y,
						     &bin_x, &bin_y);

  if (!gtk_tree_view_get_path_at_pos (tree_view,
				      bin_x,
				      bin_y,
                                      &tmp_path,
                                      &column,
                                      NULL,
                                      &cell_y))
    return FALSE;

  gtk_tree_view_get_background_area (tree_view, tmp_path, column,
                                     &cell);

  offset_into_row = cell_y;

  if (path)
    *path = tmp_path;
  else
    gtk_tree_path_free (tmp_path);

  tmp_path = NULL;

  third = cell.height / 3.0;

  if (pos)
    {
      if (offset_into_row < third)
        {
          *pos = GTK_TREE_VIEW_DROP_BEFORE;
        }
      else if (offset_into_row < (cell.height / 2.0))
        {
          *pos = GTK_TREE_VIEW_DROP_INTO_OR_BEFORE;
        }
      else if (offset_into_row < third * 2.0)
        {
          *pos = GTK_TREE_VIEW_DROP_INTO_OR_AFTER;
        }
      else
        {
          *pos = GTK_TREE_VIEW_DROP_AFTER;
        }
    }

  return TRUE;
}



/* KEEP IN SYNC WITH GTK_TREE_VIEW_BIN_EXPOSE */
/**
 * gtk_tree_view_create_row_drag_icon:
 * @tree_view: a #GtkTreeView
 * @path: a #GtkTreePath in @tree_view
 *
 * Creates a #GdkPixmap representation of the row at @path.
 * This image is used for a drag icon.
 *
 * Return value: (transfer none): a newly-allocated pixmap of the drag icon.
 **/
GdkPixmap *
gtk_tree_view_create_row_drag_icon (GtkTreeView  *tree_view,
                                    GtkTreePath  *path)
{
  GtkTreeIter   iter;
  GtkRBTree    *tree;
  GtkRBNode    *node;
  gint cell_offset;
  GList *list;
  GdkRectangle background_area;
  GdkRectangle expose_area;
  GtkWidget *widget;
  gint depth;
  /* start drawing inside the black outline */
  gint x = 1, y = 1;
  GdkDrawable *drawable;
  gint bin_window_width;
  gboolean is_separator = FALSE;
  gboolean rtl;
  cairo_t *cr;

  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), NULL);
  g_return_val_if_fail (path != NULL, NULL);

  widget = GTK_WIDGET (tree_view);

  if (!gtk_widget_get_realized (widget))
    return NULL;

  depth = gtk_tree_path_get_depth (path);

  _gtk_tree_view_find_node (tree_view,
                            path,
                            &tree,
                            &node);

  if (tree == NULL)
    return NULL;

  if (!gtk_tree_model_get_iter (tree_view->priv->model,
                                &iter,
                                path))
    return NULL;
  
  is_separator = row_is_separator (tree_view, &iter, NULL);

  cell_offset = x;

  background_area.y = y;
  background_area.height = ROW_HEIGHT (tree_view, BACKGROUND_HEIGHT (node));

  bin_window_width = gdk_window_get_width (tree_view->priv->bin_window);

  drawable = gdk_pixmap_new (tree_view->priv->bin_window,
                             bin_window_width + 2,
                             background_area.height + 2,
                             -1);

  expose_area.x = 0;
  expose_area.y = 0;
  expose_area.width = bin_window_width + 2;
  expose_area.height = background_area.height + 2;

  cr = gdk_cairo_create (drawable);
  gdk_cairo_set_source_color (cr, &widget->style->base [gtk_widget_get_state (widget)]);
  cairo_paint (cr);

  rtl = gtk_widget_get_direction (GTK_WIDGET (tree_view)) == GTK_TEXT_DIR_RTL;

  for (list = (rtl ? g_list_last (tree_view->priv->columns) : g_list_first (tree_view->priv->columns));
      list;
      list = (rtl ? list->prev : list->next))
    {
      GtkTreeViewColumn *column = list->data;
      GdkRectangle cell_area;
      gint vertical_separator;

      if (!column->visible)
        continue;

      gtk_tree_view_column_cell_set_cell_data (column, tree_view->priv->model, &iter,
					       GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_PARENT),
					       node->children?TRUE:FALSE);

      background_area.x = cell_offset;
      background_area.width = column->width;

      gtk_widget_style_get (widget,
			    "vertical-separator", &vertical_separator,
			    NULL);

      cell_area = background_area;

      cell_area.y += vertical_separator / 2;
      cell_area.height -= vertical_separator;

      if (gtk_tree_view_is_expander_column (tree_view, column))
        {
	  if (!rtl)
	    cell_area.x += (depth - 1) * tree_view->priv->level_indentation;
	  cell_area.width -= (depth - 1) * tree_view->priv->level_indentation;

          if (TREE_VIEW_DRAW_EXPANDERS(tree_view))
	    {
	      if (!rtl)
		cell_area.x += depth * tree_view->priv->expander_size;
	      cell_area.width -= depth * tree_view->priv->expander_size;
	    }
        }

      if (gtk_tree_view_column_cell_is_visible (column))
	{
	  if (is_separator)
	    gtk_paint_hline (widget->style,
			     drawable,
			     GTK_STATE_NORMAL,
			     &cell_area,
			     widget,
			     NULL,
			     cell_area.x,
			     cell_area.x + cell_area.width,
			     cell_area.y + cell_area.height / 2);
	  else
	    _gtk_tree_view_column_cell_render (column,
					       drawable,
					       &background_area,
					       &cell_area,
					       &expose_area,
					       0);
	}
      cell_offset += column->width;
    }

  cairo_set_source_rgb (cr, 0, 0, 0);
  cairo_rectangle (cr, 
                   0.5, 0.5, 
                   bin_window_width + 1,
                   background_area.height + 1);
  cairo_set_line_width (cr, 1.0);
  cairo_stroke (cr);

  cairo_destroy (cr);

  return drawable;
}


/**
 * gtk_tree_view_set_destroy_count_func:
 * @tree_view: A #GtkTreeView
 * @func: (allow-none): Function to be called when a view row is destroyed, or %NULL
 * @data: (allow-none): User data to be passed to @func, or %NULL
 * @destroy: (allow-none): Destroy notifier for @data, or %NULL
 *
 * This function should almost never be used.  It is meant for private use by
 * ATK for determining the number of visible children that are removed when the
 * user collapses a row, or a row is deleted.
 **/
void
gtk_tree_view_set_destroy_count_func (GtkTreeView             *tree_view,
				      GtkTreeDestroyCountFunc  func,
				      gpointer                 data,
				      GDestroyNotify           destroy)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  if (tree_view->priv->destroy_count_destroy)
    tree_view->priv->destroy_count_destroy (tree_view->priv->destroy_count_data);

  tree_view->priv->destroy_count_func = func;
  tree_view->priv->destroy_count_data = data;
  tree_view->priv->destroy_count_destroy = destroy;
}


/*
 * Interactive search
 */

/**
 * gtk_tree_view_set_enable_search:
 * @tree_view: A #GtkTreeView
 * @enable_search: %TRUE, if the user can search interactively
 *
 * If @enable_search is set, then the user can type in text to search through
 * the tree interactively (this is sometimes called "typeahead find").
 * 
 * Note that even if this is %FALSE, the user can still initiate a search 
 * using the "start-interactive-search" key binding.
 */
void
gtk_tree_view_set_enable_search (GtkTreeView *tree_view,
				 gboolean     enable_search)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  enable_search = !!enable_search;
  
  if (tree_view->priv->enable_search != enable_search)
    {
       tree_view->priv->enable_search = enable_search;
       g_object_notify (G_OBJECT (tree_view), "enable-search");
    }
}

/**
 * gtk_tree_view_get_enable_search:
 * @tree_view: A #GtkTreeView
 *
 * Returns whether or not the tree allows to start interactive searching 
 * by typing in text.
 *
 * Return value: whether or not to let the user search interactively
 */
gboolean
gtk_tree_view_get_enable_search (GtkTreeView *tree_view)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), FALSE);

  return tree_view->priv->enable_search;
}


/**
 * gtk_tree_view_get_search_column:
 * @tree_view: A #GtkTreeView
 *
 * Gets the column searched on by the interactive search code.
 *
 * Return value: the column the interactive search code searches in.
 */
gint
gtk_tree_view_get_search_column (GtkTreeView *tree_view)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), -1);

  return (tree_view->priv->search_column);
}

/**
 * gtk_tree_view_set_search_column:
 * @tree_view: A #GtkTreeView
 * @column: the column of the model to search in, or -1 to disable searching
 *
 * Sets @column as the column where the interactive search code should
 * search in for the current model. 
 * 
 * If the search column is set, users can use the "start-interactive-search"
 * key binding to bring up search popup. The enable-search property controls
 * whether simply typing text will also start an interactive search.
 *
 * Note that @column refers to a column of the current model. The search 
 * column is reset to -1 when the model is changed.
 */
void
gtk_tree_view_set_search_column (GtkTreeView *tree_view,
				 gint         column)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (column >= -1);

  if (tree_view->priv->search_column == column)
    return;

  tree_view->priv->search_column = column;
  g_object_notify (G_OBJECT (tree_view), "search-column");
}

/**
 * gtk_tree_view_get_search_equal_func:
 * @tree_view: A #GtkTreeView
 *
 * Returns the compare function currently in use.
 *
 * Return value: the currently used compare function for the search code.
 */

GtkTreeViewSearchEqualFunc
gtk_tree_view_get_search_equal_func (GtkTreeView *tree_view)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), NULL);

  return tree_view->priv->search_equal_func;
}

/**
 * gtk_tree_view_set_search_equal_func:
 * @tree_view: A #GtkTreeView
 * @search_equal_func: the compare function to use during the search
 * @search_user_data: (allow-none): user data to pass to @search_equal_func, or %NULL
 * @search_destroy: (allow-none): Destroy notifier for @search_user_data, or %NULL
 *
 * Sets the compare function for the interactive search capabilities; note
 * that somewhat like strcmp() returning 0 for equality
 * #GtkTreeViewSearchEqualFunc returns %FALSE on matches.
 **/
void
gtk_tree_view_set_search_equal_func (GtkTreeView                *tree_view,
				     GtkTreeViewSearchEqualFunc  search_equal_func,
				     gpointer                    search_user_data,
				     GDestroyNotify              search_destroy)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (search_equal_func != NULL);

  if (tree_view->priv->search_destroy)
    tree_view->priv->search_destroy (tree_view->priv->search_user_data);

  tree_view->priv->search_equal_func = search_equal_func;
  tree_view->priv->search_user_data = search_user_data;
  tree_view->priv->search_destroy = search_destroy;
  if (tree_view->priv->search_equal_func == NULL)
    tree_view->priv->search_equal_func = gtk_tree_view_search_equal_func;
}

/**
 * gtk_tree_view_get_search_entry:
 * @tree_view: A #GtkTreeView
 *
 * Returns the #GtkEntry which is currently in use as interactive search
 * entry for @tree_view.  In case the built-in entry is being used, %NULL
 * will be returned.
 *
 * Return value: (transfer none): the entry currently in use as search entry.
 *
 * Since: 2.10
 */
GtkEntry *
gtk_tree_view_get_search_entry (GtkTreeView *tree_view)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), NULL);

  if (tree_view->priv->search_custom_entry_set)
    return GTK_ENTRY (tree_view->priv->search_entry);

  return NULL;
}

/**
 * gtk_tree_view_set_search_entry:
 * @tree_view: A #GtkTreeView
 * @entry: (allow-none): the entry the interactive search code of @tree_view should use or %NULL
 *
 * Sets the entry which the interactive search code will use for this
 * @tree_view.  This is useful when you want to provide a search entry
 * in our interface at all time at a fixed position.  Passing %NULL for
 * @entry will make the interactive search code use the built-in popup
 * entry again.
 *
 * Since: 2.10
 */
void
gtk_tree_view_set_search_entry (GtkTreeView *tree_view,
				GtkEntry    *entry)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (entry == NULL || GTK_IS_ENTRY (entry));

  if (tree_view->priv->search_custom_entry_set)
    {
      if (tree_view->priv->search_entry_changed_id)
        {
	  g_signal_handler_disconnect (tree_view->priv->search_entry,
				       tree_view->priv->search_entry_changed_id);
	  tree_view->priv->search_entry_changed_id = 0;
	}
      g_signal_handlers_disconnect_by_func (tree_view->priv->search_entry,
					    G_CALLBACK (gtk_tree_view_search_key_press_event),
					    tree_view);

      g_object_unref (tree_view->priv->search_entry);
    }
  else if (tree_view->priv->search_window)
    {
      gtk_widget_destroy (tree_view->priv->search_window);

      tree_view->priv->search_window = NULL;
    }

  if (entry)
    {
      tree_view->priv->search_entry = GTK_WIDGET (g_object_ref (entry));
      tree_view->priv->search_custom_entry_set = TRUE;

      if (tree_view->priv->search_entry_changed_id == 0)
        {
          tree_view->priv->search_entry_changed_id =
	    g_signal_connect (tree_view->priv->search_entry, "changed",
			      G_CALLBACK (gtk_tree_view_search_init),
			      tree_view);
	}
      
        g_signal_connect (tree_view->priv->search_entry, "key-press-event",
		          G_CALLBACK (gtk_tree_view_search_key_press_event),
		          tree_view);

	gtk_tree_view_search_init (tree_view->priv->search_entry, tree_view);
    }
  else
    {
      tree_view->priv->search_entry = NULL;
      tree_view->priv->search_custom_entry_set = FALSE;
    }
}

/**
 * gtk_tree_view_set_search_position_func:
 * @tree_view: A #GtkTreeView
 * @func: (allow-none): the function to use to position the search dialog, or %NULL
 *    to use the default search position function
 * @data: (allow-none): user data to pass to @func, or %NULL
 * @destroy: (allow-none): Destroy notifier for @data, or %NULL
 *
 * Sets the function to use when positioning the search dialog.
 *
 * Since: 2.10
 **/
void
gtk_tree_view_set_search_position_func (GtkTreeView                   *tree_view,
				        GtkTreeViewSearchPositionFunc  func,
				        gpointer                       user_data,
				        GDestroyNotify                 destroy)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  if (tree_view->priv->search_position_destroy)
    tree_view->priv->search_position_destroy (tree_view->priv->search_position_user_data);

  tree_view->priv->search_position_func = func;
  tree_view->priv->search_position_user_data = user_data;
  tree_view->priv->search_position_destroy = destroy;
  if (tree_view->priv->search_position_func == NULL)
    tree_view->priv->search_position_func = gtk_tree_view_search_position_func;
}

/**
 * gtk_tree_view_get_search_position_func:
 * @tree_view: A #GtkTreeView
 *
 * Returns the positioning function currently in use.
 *
 * Return value: the currently used function for positioning the search dialog.
 *
 * Since: 2.10
 */
GtkTreeViewSearchPositionFunc
gtk_tree_view_get_search_position_func (GtkTreeView *tree_view)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), NULL);

  return tree_view->priv->search_position_func;
}


static void
gtk_tree_view_search_dialog_hide (GtkWidget   *search_dialog,
				  GtkTreeView *tree_view)
{
  if (tree_view->priv->disable_popdown)
    return;

  if (tree_view->priv->search_entry_changed_id)
    {
      g_signal_handler_disconnect (tree_view->priv->search_entry,
				   tree_view->priv->search_entry_changed_id);
      tree_view->priv->search_entry_changed_id = 0;
    }
  if (tree_view->priv->typeselect_flush_timeout)
    {
      g_source_remove (tree_view->priv->typeselect_flush_timeout);
      tree_view->priv->typeselect_flush_timeout = 0;
    }
	
  if (gtk_widget_get_visible (search_dialog))
    {
      /* send focus-in event */
      send_focus_change (GTK_WIDGET (tree_view->priv->search_entry), FALSE);
      gtk_widget_hide (search_dialog);
      gtk_entry_set_text (GTK_ENTRY (tree_view->priv->search_entry), "");
      send_focus_change (GTK_WIDGET (tree_view), TRUE);
    }
}

static void
gtk_tree_view_search_position_func (GtkTreeView *tree_view,
				    GtkWidget   *search_dialog,
				    gpointer     user_data)
{
  gint x, y;
  gint tree_x, tree_y;
  gint tree_width, tree_height;
  GdkWindow *tree_window = GTK_WIDGET (tree_view)->window;
  GdkScreen *screen = gdk_window_get_screen (tree_window);
  GtkRequisition requisition;
  gint monitor_num;
  GdkRectangle monitor;

  monitor_num = gdk_screen_get_monitor_at_window (screen, tree_window);
  gdk_screen_get_monitor_geometry (screen, monitor_num, &monitor);

  gtk_widget_realize (search_dialog);

  gdk_window_get_origin (tree_window, &tree_x, &tree_y);
  tree_width = gdk_window_get_width (tree_window);
  tree_height = gdk_window_get_height (tree_window);
  gtk_widget_size_request (search_dialog, &requisition);

  if (tree_x + tree_width > gdk_screen_get_width (screen))
    x = gdk_screen_get_width (screen) - requisition.width;
  else if (tree_x + tree_width - requisition.width < 0)
    x = 0;
  else
    x = tree_x + tree_width - requisition.width;

  if (tree_y + tree_height + requisition.height > gdk_screen_get_height (screen))
    y = gdk_screen_get_height (screen) - requisition.height;
  else if (tree_y + tree_height < 0) /* isn't really possible ... */
    y = 0;
  else
    y = tree_y + tree_height;

  gtk_window_move (GTK_WINDOW (search_dialog), x, y);
}

static void
gtk_tree_view_search_disable_popdown (GtkEntry *entry,
				      GtkMenu  *menu,
				      gpointer  data)
{
  GtkTreeView *tree_view = (GtkTreeView *)data;

  tree_view->priv->disable_popdown = 1;
  g_signal_connect (menu, "hide",
		    G_CALLBACK (gtk_tree_view_search_enable_popdown), data);
}

/* Because we're visible but offscreen, we just set a flag in the preedit
 * callback.
 */
static void
gtk_tree_view_search_preedit_changed (GtkIMContext *im_context,
				      GtkTreeView  *tree_view)
{
  tree_view->priv->imcontext_changed = 1;
  if (tree_view->priv->typeselect_flush_timeout)
    {
      g_source_remove (tree_view->priv->typeselect_flush_timeout);
      tree_view->priv->typeselect_flush_timeout =
	gdk_threads_add_timeout (GTK_TREE_VIEW_SEARCH_DIALOG_TIMEOUT,
		       (GSourceFunc) gtk_tree_view_search_entry_flush_timeout,
		       tree_view);
    }

}

static void
gtk_tree_view_search_activate (GtkEntry    *entry,
			       GtkTreeView *tree_view)
{
  GtkTreePath *path;
  GtkRBNode *node;
  GtkRBTree *tree;

  gtk_tree_view_search_dialog_hide (tree_view->priv->search_window,
				    tree_view);

  /* If we have a row selected and it's the cursor row, we activate
   * the row XXX */
  if (gtk_tree_row_reference_valid (tree_view->priv->cursor))
    {
      path = gtk_tree_row_reference_get_path (tree_view->priv->cursor);
      
      _gtk_tree_view_find_node (tree_view, path, &tree, &node);
      
      if (node && GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_SELECTED))
	gtk_tree_view_row_activated (tree_view, path, tree_view->priv->focus_column);
      
      gtk_tree_path_free (path);
    }
}

static gboolean
gtk_tree_view_real_search_enable_popdown (gpointer data)
{
  GtkTreeView *tree_view = (GtkTreeView *)data;

  tree_view->priv->disable_popdown = 0;

  return FALSE;
}

static void
gtk_tree_view_search_enable_popdown (GtkWidget *widget,
				     gpointer   data)
{
  gdk_threads_add_timeout_full (G_PRIORITY_HIGH, 200, gtk_tree_view_real_search_enable_popdown, g_object_ref (data), g_object_unref);
}

static gboolean
gtk_tree_view_search_delete_event (GtkWidget *widget,
				   GdkEventAny *event,
				   GtkTreeView *tree_view)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  gtk_tree_view_search_dialog_hide (widget, tree_view);

  return TRUE;
}

static gboolean
gtk_tree_view_search_button_press_event (GtkWidget *widget,
					 GdkEventButton *event,
					 GtkTreeView *tree_view)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  gtk_tree_view_search_dialog_hide (widget, tree_view);

  if (event->window == tree_view->priv->bin_window)
    gtk_tree_view_button_press (GTK_WIDGET (tree_view), event);

  return TRUE;
}

static gboolean
gtk_tree_view_search_scroll_event (GtkWidget *widget,
				   GdkEventScroll *event,
				   GtkTreeView *tree_view)
{
  gboolean retval = FALSE;

  if (event->direction == GDK_SCROLL_UP)
    {
      gtk_tree_view_search_move (widget, tree_view, TRUE);
      retval = TRUE;
    }
  else if (event->direction == GDK_SCROLL_DOWN)
    {
      gtk_tree_view_search_move (widget, tree_view, FALSE);
      retval = TRUE;
    }

  /* renew the flush timeout */
  if (retval && tree_view->priv->typeselect_flush_timeout
      && !tree_view->priv->search_custom_entry_set)
    {
      g_source_remove (tree_view->priv->typeselect_flush_timeout);
      tree_view->priv->typeselect_flush_timeout =
	gdk_threads_add_timeout (GTK_TREE_VIEW_SEARCH_DIALOG_TIMEOUT,
		       (GSourceFunc) gtk_tree_view_search_entry_flush_timeout,
		       tree_view);
    }

  return retval;
}

static gboolean
gtk_tree_view_search_key_press_event (GtkWidget *widget,
				      GdkEventKey *event,
				      GtkTreeView *tree_view)
{
  gboolean retval = FALSE;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), FALSE);

  /* close window and cancel the search */
  if (!tree_view->priv->search_custom_entry_set
      && (event->keyval == GDK_Escape ||
          event->keyval == GDK_Tab ||
	    event->keyval == GDK_KP_Tab ||
	    event->keyval == GDK_ISO_Left_Tab))
    {
      gtk_tree_view_search_dialog_hide (widget, tree_view);
      return TRUE;
    }

  /* select previous matching iter */
  if (event->keyval == GDK_Up || event->keyval == GDK_KP_Up)
    {
      if (!gtk_tree_view_search_move (widget, tree_view, TRUE))
        gtk_widget_error_bell (widget);

      retval = TRUE;
    }

  if (((event->state & (GTK_DEFAULT_ACCEL_MOD_MASK | GDK_SHIFT_MASK)) == (GTK_DEFAULT_ACCEL_MOD_MASK | GDK_SHIFT_MASK))
      && (event->keyval == GDK_g || event->keyval == GDK_G))
    {
      if (!gtk_tree_view_search_move (widget, tree_view, TRUE))
        gtk_widget_error_bell (widget);

      retval = TRUE;
    }

  /* select next matching iter */
  if (event->keyval == GDK_Down || event->keyval == GDK_KP_Down)
    {
      if (!gtk_tree_view_search_move (widget, tree_view, FALSE))
        gtk_widget_error_bell (widget);

      retval = TRUE;
    }

  if (((event->state & (GTK_DEFAULT_ACCEL_MOD_MASK | GDK_SHIFT_MASK)) == GTK_DEFAULT_ACCEL_MOD_MASK)
      && (event->keyval == GDK_g || event->keyval == GDK_G))
    {
      if (!gtk_tree_view_search_move (widget, tree_view, FALSE))
        gtk_widget_error_bell (widget);

      retval = TRUE;
    }

  /* renew the flush timeout */
  if (retval && tree_view->priv->typeselect_flush_timeout
      && !tree_view->priv->search_custom_entry_set)
    {
      g_source_remove (tree_view->priv->typeselect_flush_timeout);
      tree_view->priv->typeselect_flush_timeout =
	gdk_threads_add_timeout (GTK_TREE_VIEW_SEARCH_DIALOG_TIMEOUT,
		       (GSourceFunc) gtk_tree_view_search_entry_flush_timeout,
		       tree_view);
    }

  return retval;
}

/*  this function returns FALSE if there is a search string but
 *  nothing was found, and TRUE otherwise.
 */
static gboolean
gtk_tree_view_search_move (GtkWidget   *window,
			   GtkTreeView *tree_view,
			   gboolean     up)
{
  gboolean ret;
  gint len;
  gint count = 0;
  const gchar *text;
  GtkTreeIter iter;
  GtkTreeModel *model;
  GtkTreeSelection *selection;

  text = gtk_entry_get_text (GTK_ENTRY (tree_view->priv->search_entry));

  g_return_val_if_fail (text != NULL, FALSE);

  len = strlen (text);

  if (up && tree_view->priv->selected_iter == 1)
    return strlen (text) < 1;

  len = strlen (text);

  if (len < 1)
    return TRUE;

  model = gtk_tree_view_get_model (tree_view);
  selection = gtk_tree_view_get_selection (tree_view);

  /* search */
  gtk_tree_selection_unselect_all (selection);
  if (!gtk_tree_model_get_iter_first (model, &iter))
    return TRUE;

  ret = gtk_tree_view_search_iter (model, selection, &iter, text,
				   &count, up?((tree_view->priv->selected_iter) - 1):((tree_view->priv->selected_iter + 1)));

  if (ret)
    {
      /* found */
      tree_view->priv->selected_iter += up?(-1):(1);
      return TRUE;
    }
  else
    {
      /* return to old iter */
      count = 0;
      gtk_tree_model_get_iter_first (model, &iter);
      gtk_tree_view_search_iter (model, selection,
				 &iter, text,
				 &count, tree_view->priv->selected_iter);
      return FALSE;
    }
}

static gboolean
gtk_tree_view_search_equal_func (GtkTreeModel *model,
				 gint          column,
				 const gchar  *key,
				 GtkTreeIter  *iter,
				 gpointer      search_data)
{
  gboolean retval = TRUE;
  const gchar *str;
  gchar *normalized_string;
  gchar *normalized_key;
  gchar *case_normalized_string = NULL;
  gchar *case_normalized_key = NULL;
  GValue value = {0,};
  GValue transformed = {0,};

  gtk_tree_model_get_value (model, iter, column, &value);

  g_value_init (&transformed, G_TYPE_STRING);

  if (!g_value_transform (&value, &transformed))
    {
      g_value_unset (&value);
      return TRUE;
    }

  g_value_unset (&value);

  str = g_value_get_string (&transformed);
  if (!str)
    {
      g_value_unset (&transformed);
      return TRUE;
    }

  normalized_string = g_utf8_normalize (str, -1, G_NORMALIZE_ALL);
  normalized_key = g_utf8_normalize (key, -1, G_NORMALIZE_ALL);

  if (normalized_string && normalized_key)
    {
      case_normalized_string = g_utf8_casefold (normalized_string, -1);
      case_normalized_key = g_utf8_casefold (normalized_key, -1);

      if (strncmp (case_normalized_key, case_normalized_string, strlen (case_normalized_key)) == 0)
        retval = FALSE;
    }

  g_value_unset (&transformed);
  g_free (normalized_key);
  g_free (normalized_string);
  g_free (case_normalized_key);
  g_free (case_normalized_string);

  return retval;
}

static gboolean
gtk_tree_view_search_iter (GtkTreeModel     *model,
			   GtkTreeSelection *selection,
			   GtkTreeIter      *iter,
			   const gchar      *text,
			   gint             *count,
			   gint              n)
{
  GtkRBTree *tree = NULL;
  GtkRBNode *node = NULL;
  GtkTreePath *path;

  GtkTreeView *tree_view = gtk_tree_selection_get_tree_view (selection);

  path = gtk_tree_model_get_path (model, iter);
  _gtk_tree_view_find_node (tree_view, path, &tree, &node);

  do
    {
      if (! tree_view->priv->search_equal_func (model, tree_view->priv->search_column, text, iter, tree_view->priv->search_user_data))
        {
          (*count)++;
          if (*count == n)
            {
              gtk_tree_view_scroll_to_cell (tree_view, path, NULL,
					    TRUE, 0.5, 0.0);
              gtk_tree_selection_select_iter (selection, iter);
              gtk_tree_view_real_set_cursor (tree_view, path, FALSE, TRUE);

	      if (path)
		gtk_tree_path_free (path);

              return TRUE;
            }
        }

      if (node->children)
	{
	  gboolean has_child;
	  GtkTreeIter tmp;

	  tree = node->children;
	  node = tree->root;

	  while (node->left != tree->nil)
	    node = node->left;

	  tmp = *iter;
	  has_child = gtk_tree_model_iter_children (model, iter, &tmp);
	  gtk_tree_path_down (path);

	  /* sanity check */
	  TREE_VIEW_INTERNAL_ASSERT (has_child, FALSE);
	}
      else
	{
	  gboolean done = FALSE;

	  do
	    {
	      node = _gtk_rbtree_next (tree, node);

	      if (node)
		{
		  gboolean has_next;

		  has_next = gtk_tree_model_iter_next (model, iter);

		  done = TRUE;
		  gtk_tree_path_next (path);

		  /* sanity check */
		  TREE_VIEW_INTERNAL_ASSERT (has_next, FALSE);
		}
	      else
		{
		  gboolean has_parent;
		  GtkTreeIter tmp_iter = *iter;

		  node = tree->parent_node;
		  tree = tree->parent_tree;

		  if (!tree)
		    {
		      if (path)
			gtk_tree_path_free (path);

		      /* we've run out of tree, done with this func */
		      return FALSE;
		    }

		  has_parent = gtk_tree_model_iter_parent (model,
							   iter,
							   &tmp_iter);
		  gtk_tree_path_up (path);

		  /* sanity check */
		  TREE_VIEW_INTERNAL_ASSERT (has_parent, FALSE);
		}
	    }
	  while (!done);
	}
    }
  while (1);

  return FALSE;
}

static void
gtk_tree_view_search_init (GtkWidget   *entry,
			   GtkTreeView *tree_view)
{
  gint ret;
  gint count = 0;
  const gchar *text;
  GtkTreeIter iter;
  GtkTreeModel *model;
  GtkTreeSelection *selection;

  g_return_if_fail (GTK_IS_ENTRY (entry));
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  text = gtk_entry_get_text (GTK_ENTRY (entry));

  model = gtk_tree_view_get_model (tree_view);
  selection = gtk_tree_view_get_selection (tree_view);

  /* search */
  gtk_tree_selection_unselect_all (selection);
  if (tree_view->priv->typeselect_flush_timeout
      && !tree_view->priv->search_custom_entry_set)
    {
      g_source_remove (tree_view->priv->typeselect_flush_timeout);
      tree_view->priv->typeselect_flush_timeout =
	gdk_threads_add_timeout (GTK_TREE_VIEW_SEARCH_DIALOG_TIMEOUT,
		       (GSourceFunc) gtk_tree_view_search_entry_flush_timeout,
		       tree_view);
    }

  if (*text == '\0')
    return;

  if (!gtk_tree_model_get_iter_first (model, &iter))
    return;

  ret = gtk_tree_view_search_iter (model, selection,
				   &iter, text,
				   &count, 1);

  if (ret)
    tree_view->priv->selected_iter = 1;
}

static void
gtk_tree_view_remove_widget (GtkCellEditable *cell_editable,
			     GtkTreeView     *tree_view)
{
  if (tree_view->priv->edited_column == NULL)
    return;

  _gtk_tree_view_column_stop_editing (tree_view->priv->edited_column);
  tree_view->priv->edited_column = NULL;

  if (gtk_widget_has_focus (GTK_WIDGET (cell_editable)))
    gtk_widget_grab_focus (GTK_WIDGET (tree_view));

  g_signal_handlers_disconnect_by_func (cell_editable,
					gtk_tree_view_remove_widget,
					tree_view);

  gtk_container_remove (GTK_CONTAINER (tree_view),
			GTK_WIDGET (cell_editable));  

  /* FIXME should only redraw a single node */
  gtk_widget_queue_draw (GTK_WIDGET (tree_view));
}

static gboolean
gtk_tree_view_start_editing (GtkTreeView *tree_view,
			     GtkTreePath *cursor_path)
{
  GtkTreeIter iter;
  GdkRectangle background_area;
  GdkRectangle cell_area;
  GtkCellEditable *editable_widget = NULL;
  gchar *path_string;
  guint flags = 0; /* can be 0, as the flags are primarily for rendering */
  gint retval = FALSE;
  GtkRBTree *cursor_tree;
  GtkRBNode *cursor_node;

  g_assert (tree_view->priv->focus_column);

  if (!gtk_widget_get_realized (GTK_WIDGET (tree_view)))
    return FALSE;

  if (_gtk_tree_view_find_node (tree_view, cursor_path, &cursor_tree, &cursor_node) ||
      cursor_node == NULL)
    return FALSE;

  path_string = gtk_tree_path_to_string (cursor_path);
  gtk_tree_model_get_iter (tree_view->priv->model, &iter, cursor_path);

  validate_row (tree_view, cursor_tree, cursor_node, &iter, cursor_path);

  gtk_tree_view_column_cell_set_cell_data (tree_view->priv->focus_column,
					   tree_view->priv->model,
					   &iter,
					   GTK_RBNODE_FLAG_SET (cursor_node, GTK_RBNODE_IS_PARENT),
					   cursor_node->children?TRUE:FALSE);
  gtk_tree_view_get_background_area (tree_view,
				     cursor_path,
				     tree_view->priv->focus_column,
				     &background_area);
  gtk_tree_view_get_cell_area (tree_view,
			       cursor_path,
			       tree_view->priv->focus_column,
			       &cell_area);

  if (_gtk_tree_view_column_cell_event (tree_view->priv->focus_column,
					&editable_widget,
					NULL,
					path_string,
					&background_area,
					&cell_area,
					flags))
    {
      retval = TRUE;
      if (editable_widget != NULL)
	{
	  gint left, right;
	  GdkRectangle area;
	  GtkCellRenderer *cell;

	  area = cell_area;
	  cell = _gtk_tree_view_column_get_edited_cell (tree_view->priv->focus_column);

	  _gtk_tree_view_column_get_neighbor_sizes (tree_view->priv->focus_column, cell, &left, &right);

	  area.x += left;
	  area.width -= right + left;

	  gtk_tree_view_real_start_editing (tree_view,
					    tree_view->priv->focus_column,
					    cursor_path,
					    editable_widget,
					    &area,
					    NULL,
					    flags);
	}

    }
  g_free (path_string);
  return retval;
}

static void
gtk_tree_view_real_start_editing (GtkTreeView       *tree_view,
				  GtkTreeViewColumn *column,
				  GtkTreePath       *path,
				  GtkCellEditable   *cell_editable,
				  GdkRectangle      *cell_area,
				  GdkEvent          *event,
				  guint              flags)
{
  gint pre_val = tree_view->priv->vadjustment->value;
  GtkRequisition requisition;

  tree_view->priv->edited_column = column;
  _gtk_tree_view_column_start_editing (column, GTK_CELL_EDITABLE (cell_editable));

  gtk_tree_view_real_set_cursor (tree_view, path, FALSE, TRUE);
  cell_area->y += pre_val - (int)tree_view->priv->vadjustment->value;

  gtk_widget_size_request (GTK_WIDGET (cell_editable), &requisition);

  GTK_TREE_VIEW_SET_FLAG (tree_view, GTK_TREE_VIEW_DRAW_KEYFOCUS);

  if (requisition.height < cell_area->height)
    {
      gint diff = cell_area->height - requisition.height;
      gtk_tree_view_put (tree_view,
			 GTK_WIDGET (cell_editable),
			 cell_area->x, cell_area->y + diff/2,
			 cell_area->width, requisition.height);
    }
  else
    {
      gtk_tree_view_put (tree_view,
			 GTK_WIDGET (cell_editable),
			 cell_area->x, cell_area->y,
			 cell_area->width, cell_area->height);
    }

  gtk_cell_editable_start_editing (GTK_CELL_EDITABLE (cell_editable),
				   (GdkEvent *)event);

  gtk_widget_grab_focus (GTK_WIDGET (cell_editable));
  g_signal_connect (cell_editable, "remove-widget",
		    G_CALLBACK (gtk_tree_view_remove_widget), tree_view);
}

static void
gtk_tree_view_stop_editing (GtkTreeView *tree_view,
			    gboolean     cancel_editing)
{
  GtkTreeViewColumn *column;
  GtkCellRenderer *cell;

  if (tree_view->priv->edited_column == NULL)
    return;

  /*
   * This is very evil. We need to do this, because
   * gtk_cell_editable_editing_done may trigger gtk_tree_view_row_changed
   * later on. If gtk_tree_view_row_changed notices
   * tree_view->priv->edited_column != NULL, it'll call
   * gtk_tree_view_stop_editing again. Bad things will happen then.
   *
   * Please read that again if you intend to modify anything here.
   */

  column = tree_view->priv->edited_column;
  tree_view->priv->edited_column = NULL;

  cell = _gtk_tree_view_column_get_edited_cell (column);
  gtk_cell_renderer_stop_editing (cell, cancel_editing);

  if (!cancel_editing)
    gtk_cell_editable_editing_done (column->editable_widget);

  tree_view->priv->edited_column = column;

  gtk_cell_editable_remove_widget (column->editable_widget);
}


/**
 * gtk_tree_view_set_hover_selection:
 * @tree_view: a #GtkTreeView
 * @hover: %TRUE to enable hover selection mode
 *
 * Enables of disables the hover selection mode of @tree_view.
 * Hover selection makes the selected row follow the pointer.
 * Currently, this works only for the selection modes 
 * %GTK_SELECTION_SINGLE and %GTK_SELECTION_BROWSE.
 * 
 * Since: 2.6
 **/
void     
gtk_tree_view_set_hover_selection (GtkTreeView *tree_view,
				   gboolean     hover)
{
  hover = hover != FALSE;

  if (hover != tree_view->priv->hover_selection)
    {
      tree_view->priv->hover_selection = hover;

      g_object_notify (G_OBJECT (tree_view), "hover-selection");
    }
}

/**
 * gtk_tree_view_get_hover_selection:
 * @tree_view: a #GtkTreeView
 * 
 * Returns whether hover selection mode is turned on for @tree_view.
 * 
 * Return value: %TRUE if @tree_view is in hover selection mode
 *
 * Since: 2.6 
 **/
gboolean 
gtk_tree_view_get_hover_selection (GtkTreeView *tree_view)
{
  return tree_view->priv->hover_selection;
}

/**
 * gtk_tree_view_set_hover_expand:
 * @tree_view: a #GtkTreeView
 * @expand: %TRUE to enable hover selection mode
 *
 * Enables of disables the hover expansion mode of @tree_view.
 * Hover expansion makes rows expand or collapse if the pointer 
 * moves over them.
 * 
 * Since: 2.6
 **/
void     
gtk_tree_view_set_hover_expand (GtkTreeView *tree_view,
				gboolean     expand)
{
  expand = expand != FALSE;

  if (expand != tree_view->priv->hover_expand)
    {
      tree_view->priv->hover_expand = expand;

      g_object_notify (G_OBJECT (tree_view), "hover-expand");
    }
}

/**
 * gtk_tree_view_get_hover_expand:
 * @tree_view: a #GtkTreeView
 * 
 * Returns whether hover expansion mode is turned on for @tree_view.
 * 
 * Return value: %TRUE if @tree_view is in hover expansion mode
 *
 * Since: 2.6 
 **/
gboolean 
gtk_tree_view_get_hover_expand (GtkTreeView *tree_view)
{
  return tree_view->priv->hover_expand;
}

/**
 * gtk_tree_view_set_rubber_banding:
 * @tree_view: a #GtkTreeView
 * @enable: %TRUE to enable rubber banding
 *
 * Enables or disables rubber banding in @tree_view.  If the selection mode
 * is #GTK_SELECTION_MULTIPLE, rubber banding will allow the user to select
 * multiple rows by dragging the mouse.
 * 
 * Since: 2.10
 **/
void
gtk_tree_view_set_rubber_banding (GtkTreeView *tree_view,
				  gboolean     enable)
{
  enable = enable != FALSE;

  if (enable != tree_view->priv->rubber_banding_enable)
    {
      tree_view->priv->rubber_banding_enable = enable;

      g_object_notify (G_OBJECT (tree_view), "rubber-banding");
    }
}

/**
 * gtk_tree_view_get_rubber_banding:
 * @tree_view: a #GtkTreeView
 * 
 * Returns whether rubber banding is turned on for @tree_view.  If the
 * selection mode is #GTK_SELECTION_MULTIPLE, rubber banding will allow the
 * user to select multiple rows by dragging the mouse.
 * 
 * Return value: %TRUE if rubber banding in @tree_view is enabled.
 *
 * Since: 2.10
 **/
gboolean
gtk_tree_view_get_rubber_banding (GtkTreeView *tree_view)
{
  return tree_view->priv->rubber_banding_enable;
}

/**
 * gtk_tree_view_is_rubber_banding_active:
 * @tree_view: a #GtkTreeView
 * 
 * Returns whether a rubber banding operation is currently being done
 * in @tree_view.
 *
 * Return value: %TRUE if a rubber banding operation is currently being
 * done in @tree_view.
 *
 * Since: 2.12
 **/
gboolean
gtk_tree_view_is_rubber_banding_active (GtkTreeView *tree_view)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), FALSE);

  if (tree_view->priv->rubber_banding_enable
      && tree_view->priv->rubber_band_status == RUBBER_BAND_ACTIVE)
    return TRUE;

  return FALSE;
}

/**
 * gtk_tree_view_get_row_separator_func:
 * @tree_view: a #GtkTreeView
 * 
 * Returns the current row separator function.
 * 
 * Return value: the current row separator function.
 *
 * Since: 2.6
 **/
GtkTreeViewRowSeparatorFunc 
gtk_tree_view_get_row_separator_func (GtkTreeView *tree_view)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), NULL);

  return tree_view->priv->row_separator_func;
}

/**
 * gtk_tree_view_set_row_separator_func:
 * @tree_view: a #GtkTreeView
 * @func: a #GtkTreeViewRowSeparatorFunc
 * @data: (allow-none): user data to pass to @func, or %NULL
 * @destroy: (allow-none): destroy notifier for @data, or %NULL
 * 
 * Sets the row separator function, which is used to determine
 * whether a row should be drawn as a separator. If the row separator
 * function is %NULL, no separators are drawn. This is the default value.
 *
 * Since: 2.6
 **/
void
gtk_tree_view_set_row_separator_func (GtkTreeView                 *tree_view,
				      GtkTreeViewRowSeparatorFunc  func,
				      gpointer                     data,
				      GDestroyNotify               destroy)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  if (tree_view->priv->row_separator_destroy)
    tree_view->priv->row_separator_destroy (tree_view->priv->row_separator_data);

  tree_view->priv->row_separator_func = func;
  tree_view->priv->row_separator_data = data;
  tree_view->priv->row_separator_destroy = destroy;

  /* Have the tree recalculate heights */
  _gtk_rbtree_mark_invalid (tree_view->priv->tree);
  gtk_widget_queue_resize (GTK_WIDGET (tree_view));
}

  
static void
gtk_tree_view_grab_notify (GtkWidget *widget,
			   gboolean   was_grabbed)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (widget);

  tree_view->priv->in_grab = !was_grabbed;

  if (!was_grabbed)
    {
      tree_view->priv->pressed_button = -1;

      if (tree_view->priv->rubber_band_status)
	gtk_tree_view_stop_rubber_band (tree_view);
    }
}

static void
gtk_tree_view_state_changed (GtkWidget      *widget,
		 	     GtkStateType    previous_state)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (widget);

  if (gtk_widget_get_realized (widget))
    {
      gdk_window_set_back_pixmap (widget->window, NULL, FALSE);
      gdk_window_set_background (tree_view->priv->bin_window, &widget->style->base[widget->state]);
    }

  gtk_widget_queue_draw (widget);
}

/**
 * gtk_tree_view_get_grid_lines:
 * @tree_view: a #GtkTreeView
 *
 * Returns which grid lines are enabled in @tree_view.
 *
 * Return value: a #GtkTreeViewGridLines value indicating which grid lines
 * are enabled.
 *
 * Since: 2.10
 */
GtkTreeViewGridLines
gtk_tree_view_get_grid_lines (GtkTreeView *tree_view)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), 0);

  return tree_view->priv->grid_lines;
}

/**
 * gtk_tree_view_set_grid_lines:
 * @tree_view: a #GtkTreeView
 * @grid_lines: a #GtkTreeViewGridLines value indicating which grid lines to
 * enable.
 *
 * Sets which grid lines to draw in @tree_view.
 *
 * Since: 2.10
 */
void
gtk_tree_view_set_grid_lines (GtkTreeView           *tree_view,
			      GtkTreeViewGridLines   grid_lines)
{
  GtkTreeViewPrivate *priv;
  GtkWidget *widget;
  GtkTreeViewGridLines old_grid_lines;

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  priv = tree_view->priv;
  widget = GTK_WIDGET (tree_view);

  old_grid_lines = priv->grid_lines;
  priv->grid_lines = grid_lines;
  
  if (gtk_widget_get_realized (widget))
    {
      if (grid_lines == GTK_TREE_VIEW_GRID_LINES_NONE &&
	  priv->grid_line_width)
	{
	  priv->grid_line_width = 0;
	}
      
      if (grid_lines != GTK_TREE_VIEW_GRID_LINES_NONE && 
	  !priv->grid_line_width)
	{
	  gint8 *dash_list;

	  gtk_widget_style_get (widget,
				"grid-line-width", &priv->grid_line_width,
				"grid-line-pattern", (gchar *)&dash_list,
				NULL);
      
          if (dash_list)
            {
              priv->grid_line_dashes[0] = dash_list[0];
              if (dash_list[0])
                priv->grid_line_dashes[1] = dash_list[1];
	      
              g_free (dash_list);
            }
          else
            {
              priv->grid_line_dashes[0] = 1;
              priv->grid_line_dashes[1] = 1;
            }
	}      
    }

  if (old_grid_lines != grid_lines)
    {
      gtk_widget_queue_draw (GTK_WIDGET (tree_view));
      
      g_object_notify (G_OBJECT (tree_view), "enable-grid-lines");
    }
}

/**
 * gtk_tree_view_get_enable_tree_lines:
 * @tree_view: a #GtkTreeView.
 *
 * Returns whether or not tree lines are drawn in @tree_view.
 *
 * Return value: %TRUE if tree lines are drawn in @tree_view, %FALSE
 * otherwise.
 *
 * Since: 2.10
 */
gboolean
gtk_tree_view_get_enable_tree_lines (GtkTreeView *tree_view)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), FALSE);

  return tree_view->priv->tree_lines_enabled;
}

/**
 * gtk_tree_view_set_enable_tree_lines:
 * @tree_view: a #GtkTreeView
 * @enabled: %TRUE to enable tree line drawing, %FALSE otherwise.
 *
 * Sets whether to draw lines interconnecting the expanders in @tree_view.
 * This does not have any visible effects for lists.
 *
 * Since: 2.10
 */
void
gtk_tree_view_set_enable_tree_lines (GtkTreeView *tree_view,
				     gboolean     enabled)
{
  GtkTreeViewPrivate *priv;
  GtkWidget *widget;
  gboolean was_enabled;

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  enabled = enabled != FALSE;

  priv = tree_view->priv;
  widget = GTK_WIDGET (tree_view);

  was_enabled = priv->tree_lines_enabled;

  priv->tree_lines_enabled = enabled;

  if (gtk_widget_get_realized (widget))
    {
      if (!enabled && priv->tree_line_width)
	{
          priv->tree_line_width = 0;
	}
      
      if (enabled && !priv->tree_line_width)
	{
	  gint8 *dash_list;
	  gtk_widget_style_get (widget,
				"tree-line-width", &priv->tree_line_width,
				"tree-line-pattern", (gchar *)&dash_list,
				NULL);
	  
          if (dash_list)
            {
              priv->tree_line_dashes[0] = dash_list[0];
              if (dash_list[0])
                priv->tree_line_dashes[1] = dash_list[1];
	      
              g_free (dash_list);
            }
          else
            {
              priv->tree_line_dashes[0] = 1;
              priv->tree_line_dashes[1] = 1;
            }
	}
    }

  if (was_enabled != enabled)
    {
      gtk_widget_queue_draw (GTK_WIDGET (tree_view));

      g_object_notify (G_OBJECT (tree_view), "enable-tree-lines");
    }
}


/**
 * gtk_tree_view_set_show_expanders:
 * @tree_view: a #GtkTreeView
 * @enabled: %TRUE to enable expander drawing, %FALSE otherwise.
 *
 * Sets whether to draw and enable expanders and indent child rows in
 * @tree_view.  When disabled there will be no expanders visible in trees
 * and there will be no way to expand and collapse rows by default.  Also
 * note that hiding the expanders will disable the default indentation.  You
 * can set a custom indentation in this case using
 * gtk_tree_view_set_level_indentation().
 * This does not have any visible effects for lists.
 *
 * Since: 2.12
 */
void
gtk_tree_view_set_show_expanders (GtkTreeView *tree_view,
				  gboolean     enabled)
{
  gboolean was_enabled;

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  enabled = enabled != FALSE;
  was_enabled = GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_SHOW_EXPANDERS);

  if (enabled)
    GTK_TREE_VIEW_SET_FLAG (tree_view, GTK_TREE_VIEW_SHOW_EXPANDERS);
  else
    GTK_TREE_VIEW_UNSET_FLAG (tree_view, GTK_TREE_VIEW_SHOW_EXPANDERS);

  if (enabled != was_enabled)
    gtk_widget_queue_draw (GTK_WIDGET (tree_view));
}

/**
 * gtk_tree_view_get_show_expanders:
 * @tree_view: a #GtkTreeView.
 *
 * Returns whether or not expanders are drawn in @tree_view.
 *
 * Return value: %TRUE if expanders are drawn in @tree_view, %FALSE
 * otherwise.
 *
 * Since: 2.12
 */
gboolean
gtk_tree_view_get_show_expanders (GtkTreeView *tree_view)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), FALSE);

  return GTK_TREE_VIEW_FLAG_SET (tree_view, GTK_TREE_VIEW_SHOW_EXPANDERS);
}

/**
 * gtk_tree_view_set_level_indentation:
 * @tree_view: a #GtkTreeView
 * @indentation: the amount, in pixels, of extra indentation in @tree_view.
 *
 * Sets the amount of extra indentation for child levels to use in @tree_view
 * in addition to the default indentation.  The value should be specified in
 * pixels, a value of 0 disables this feature and in this case only the default
 * indentation will be used.
 * This does not have any visible effects for lists.
 *
 * Since: 2.12
 */
void
gtk_tree_view_set_level_indentation (GtkTreeView *tree_view,
				     gint         indentation)
{
  tree_view->priv->level_indentation = indentation;

  gtk_widget_queue_draw (GTK_WIDGET (tree_view));
}

/**
 * gtk_tree_view_get_level_indentation:
 * @tree_view: a #GtkTreeView.
 *
 * Returns the amount, in pixels, of extra indentation for child levels
 * in @tree_view.
 *
 * Return value: the amount of extra indentation for child levels in
 * @tree_view.  A return value of 0 means that this feature is disabled.
 *
 * Since: 2.12
 */
gint
gtk_tree_view_get_level_indentation (GtkTreeView *tree_view)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), 0);

  return tree_view->priv->level_indentation;
}

/**
 * gtk_tree_view_set_tooltip_row:
 * @tree_view: a #GtkTreeView
 * @tooltip: a #GtkTooltip
 * @path: a #GtkTreePath
 *
 * Sets the tip area of @tooltip to be the area covered by the row at @path.
 * See also gtk_tree_view_set_tooltip_column() for a simpler alternative.
 * See also gtk_tooltip_set_tip_area().
 *
 * Since: 2.12
 */
void
gtk_tree_view_set_tooltip_row (GtkTreeView *tree_view,
			       GtkTooltip  *tooltip,
			       GtkTreePath *path)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (GTK_IS_TOOLTIP (tooltip));

  gtk_tree_view_set_tooltip_cell (tree_view, tooltip, path, NULL, NULL);
}

/**
 * gtk_tree_view_set_tooltip_cell:
 * @tree_view: a #GtkTreeView
 * @tooltip: a #GtkTooltip
 * @path: (allow-none): a #GtkTreePath or %NULL
 * @column: (allow-none): a #GtkTreeViewColumn or %NULL
 * @cell: (allow-none): a #GtkCellRenderer or %NULL
 *
 * Sets the tip area of @tooltip to the area @path, @column and @cell have
 * in common.  For example if @path is %NULL and @column is set, the tip
 * area will be set to the full area covered by @column.  See also
 * gtk_tooltip_set_tip_area().
 *
 * Note that if @path is not specified and @cell is set and part of a column
 * containing the expander, the tooltip might not show and hide at the correct
 * position.  In such cases @path must be set to the current node under the
 * mouse cursor for this function to operate correctly.
 *
 * See also gtk_tree_view_set_tooltip_column() for a simpler alternative.
 *
 * Since: 2.12
 */
void
gtk_tree_view_set_tooltip_cell (GtkTreeView       *tree_view,
				GtkTooltip        *tooltip,
				GtkTreePath       *path,
				GtkTreeViewColumn *column,
				GtkCellRenderer   *cell)
{
  GdkRectangle rect;

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (GTK_IS_TOOLTIP (tooltip));
  g_return_if_fail (column == NULL || GTK_IS_TREE_VIEW_COLUMN (column));
  g_return_if_fail (cell == NULL || GTK_IS_CELL_RENDERER (cell));

  /* Determine x values. */
  if (column && cell)
    {
      GdkRectangle tmp;
      gint start, width;

      /* We always pass in path here, whether it is NULL or not.
       * For cells in expander columns path must be specified so that
       * we can correctly account for the indentation.  This also means
       * that the tooltip is constrained vertically by the "Determine y
       * values" code below; this is not a real problem since cells actually
       * don't stretch vertically in constrast to columns.
       */
      gtk_tree_view_get_cell_area (tree_view, path, column, &tmp);
      gtk_tree_view_column_cell_get_position (column, cell, &start, &width);

      gtk_tree_view_convert_bin_window_to_widget_coords (tree_view,
							 tmp.x + start, 0,
							 &rect.x, NULL);
      rect.width = width;
    }
  else if (column)
    {
      GdkRectangle tmp;

      gtk_tree_view_get_background_area (tree_view, NULL, column, &tmp);
      gtk_tree_view_convert_bin_window_to_widget_coords (tree_view,
							 tmp.x, 0,
							 &rect.x, NULL);
      rect.width = tmp.width;
    }
  else
    {
      rect.x = 0;
      rect.width = GTK_WIDGET (tree_view)->allocation.width;
    }

  /* Determine y values. */
  if (path)
    {
      GdkRectangle tmp;

      gtk_tree_view_get_background_area (tree_view, path, NULL, &tmp);
      gtk_tree_view_convert_bin_window_to_widget_coords (tree_view,
							 0, tmp.y,
							 NULL, &rect.y);
      rect.height = tmp.height;
    }
  else
    {
      rect.y = 0;
      rect.height = tree_view->priv->vadjustment->page_size;
    }

  gtk_tooltip_set_tip_area (tooltip, &rect);
}

/**
 * gtk_tree_view_get_tooltip_context:
 * @tree_view: a #GtkTreeView
 * @x: (inout): the x coordinate (relative to widget coordinates)
 * @y: (inout): the y coordinate (relative to widget coordinates)
 * @keyboard_tip: whether this is a keyboard tooltip or not
 * @model: (out) (allow-none): a pointer to receive a #GtkTreeModel or %NULL
 * @path: (out) (allow-none): a pointer to receive a #GtkTreePath or %NULL
 * @iter: (out) (allow-none): a pointer to receive a #GtkTreeIter or %NULL
 *
 * This function is supposed to be used in a #GtkWidget::query-tooltip
 * signal handler for #GtkTreeView.  The @x, @y and @keyboard_tip values
 * which are received in the signal handler, should be passed to this
 * function without modification.
 *
 * The return value indicates whether there is a tree view row at the given
 * coordinates (%TRUE) or not (%FALSE) for mouse tooltips.  For keyboard
 * tooltips the row returned will be the cursor row.  When %TRUE, then any of
 * @model, @path and @iter which have been provided will be set to point to
 * that row and the corresponding model.  @x and @y will always be converted
 * to be relative to @tree_view's bin_window if @keyboard_tooltip is %FALSE.
 *
 * Return value: whether or not the given tooltip context points to a row.
 *
 * Since: 2.12
 */
gboolean
gtk_tree_view_get_tooltip_context (GtkTreeView   *tree_view,
				   gint          *x,
				   gint          *y,
				   gboolean       keyboard_tip,
				   GtkTreeModel **model,
				   GtkTreePath  **path,
				   GtkTreeIter   *iter)
{
  GtkTreePath *tmppath = NULL;

  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), FALSE);
  g_return_val_if_fail (x != NULL, FALSE);
  g_return_val_if_fail (y != NULL, FALSE);

  if (keyboard_tip)
    {
      gtk_tree_view_get_cursor (tree_view, &tmppath, NULL);

      if (!tmppath)
	return FALSE;
    }
  else
    {
      gtk_tree_view_convert_widget_to_bin_window_coords (tree_view, *x, *y,
							 x, y);

      if (!gtk_tree_view_get_path_at_pos (tree_view, *x, *y,
					  &tmppath, NULL, NULL, NULL))
	return FALSE;
    }

  if (model)
    *model = gtk_tree_view_get_model (tree_view);

  if (iter)
    gtk_tree_model_get_iter (gtk_tree_view_get_model (tree_view),
			     iter, tmppath);

  if (path)
    *path = tmppath;
  else
    gtk_tree_path_free (tmppath);

  return TRUE;
}

static gboolean
gtk_tree_view_set_tooltip_query_cb (GtkWidget  *widget,
				    gint        x,
				    gint        y,
				    gboolean    keyboard_tip,
				    GtkTooltip *tooltip,
				    gpointer    data)
{
  GValue value = { 0, };
  GValue transformed = { 0, };
  GtkTreeIter iter;
  GtkTreePath *path;
  GtkTreeModel *model;
  GtkTreeView *tree_view = GTK_TREE_VIEW (widget);

  if (!gtk_tree_view_get_tooltip_context (GTK_TREE_VIEW (widget),
					  &x, &y,
					  keyboard_tip,
					  &model, &path, &iter))
    return FALSE;

  gtk_tree_model_get_value (model, &iter,
                            tree_view->priv->tooltip_column, &value);

  g_value_init (&transformed, G_TYPE_STRING);

  if (!g_value_transform (&value, &transformed))
    {
      g_value_unset (&value);
      gtk_tree_path_free (path);

      return FALSE;
    }

  g_value_unset (&value);

  if (!g_value_get_string (&transformed))
    {
      g_value_unset (&transformed);
      gtk_tree_path_free (path);

      return FALSE;
    }

  gtk_tooltip_set_markup (tooltip, g_value_get_string (&transformed));
  gtk_tree_view_set_tooltip_row (tree_view, tooltip, path);

  gtk_tree_path_free (path);
  g_value_unset (&transformed);

  return TRUE;
}

/**
 * gtk_tree_view_set_tooltip_column:
 * @tree_view: a #GtkTreeView
 * @column: an integer, which is a valid column number for @tree_view's model
 *
 * If you only plan to have simple (text-only) tooltips on full rows, you
 * can use this function to have #GtkTreeView handle these automatically
 * for you. @column should be set to the column in @tree_view's model
 * containing the tooltip texts, or -1 to disable this feature.
 *
 * When enabled, #GtkWidget::has-tooltip will be set to %TRUE and
 * @tree_view will connect a #GtkWidget::query-tooltip signal handler.
 *
 * Note that the signal handler sets the text with gtk_tooltip_set_markup(),
 * so &amp;, &lt;, etc have to be escaped in the text.
 *
 * Since: 2.12
 */
void
gtk_tree_view_set_tooltip_column (GtkTreeView *tree_view,
			          gint         column)
{
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  if (column == tree_view->priv->tooltip_column)
    return;

  if (column == -1)
    {
      g_signal_handlers_disconnect_by_func (tree_view,
	  				    gtk_tree_view_set_tooltip_query_cb,
					    NULL);
      gtk_widget_set_has_tooltip (GTK_WIDGET (tree_view), FALSE);
    }
  else
    {
      if (tree_view->priv->tooltip_column == -1)
        {
          g_signal_connect (tree_view, "query-tooltip",
		            G_CALLBACK (gtk_tree_view_set_tooltip_query_cb), NULL);
          gtk_widget_set_has_tooltip (GTK_WIDGET (tree_view), TRUE);
        }
    }

  tree_view->priv->tooltip_column = column;
  g_object_notify (G_OBJECT (tree_view), "tooltip-column");
}

/**
 * gtk_tree_view_get_tooltip_column:
 * @tree_view: a #GtkTreeView
 *
 * Returns the column of @tree_view's model which is being used for
 * displaying tooltips on @tree_view's rows.
 *
 * Return value: the index of the tooltip column that is currently being
 * used, or -1 if this is disabled.
 *
 * Since: 2.12
 */
gint
gtk_tree_view_get_tooltip_column (GtkTreeView *tree_view)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), 0);

  return tree_view->priv->tooltip_column;
}

#define __GTK_TREE_VIEW_C__
#include "gtkaliasdef.c"
