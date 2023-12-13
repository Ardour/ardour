/* -*- Mode: C; c-file-style: "gnu"; tab-width: 8 -*- */
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

#include <stdio.h>
#include <string.h>

#include <gdk/gdkkeysyms.h>

#undef GTK_DISABLE_DEPRECATED

#include "gtknotebook.h"
#include "gtkmain.h"
#include "gtkmenu.h"
#include "gtkmenuitem.h"
#include "gtklabel.h"
#include "gtkintl.h"
#include "gtkmarshalers.h"
#include "gtkbindings.h"
#include "gtkprivate.h"
#include "gtkdnd.h"
#include "gtkbuildable.h"

#include "gtkalias.h"

#define SCROLL_DELAY_FACTOR   5
#define SCROLL_THRESHOLD      12
#define DND_THRESHOLD_MULTIPLIER 4
#define FRAMES_PER_SECOND     45
#define MSECS_BETWEEN_UPDATES (1000 / FRAMES_PER_SECOND)

enum {
  SWITCH_PAGE,
  FOCUS_TAB,
  SELECT_PAGE,
  CHANGE_CURRENT_PAGE,
  MOVE_FOCUS_OUT,
  REORDER_TAB,
  PAGE_REORDERED,
  PAGE_REMOVED,
  PAGE_ADDED,
  CREATE_WINDOW,
  LAST_SIGNAL
};

enum {
  STEP_PREV,
  STEP_NEXT
};

typedef enum
{
  ARROW_NONE,
  ARROW_LEFT_BEFORE,
  ARROW_RIGHT_BEFORE,
  ARROW_LEFT_AFTER,
  ARROW_RIGHT_AFTER
} GtkNotebookArrow;

typedef enum
{
  POINTER_BEFORE,
  POINTER_AFTER,
  POINTER_BETWEEN
} GtkNotebookPointerPosition;

typedef enum
{
  DRAG_OPERATION_NONE,
  DRAG_OPERATION_REORDER,
  DRAG_OPERATION_DETACH
} GtkNotebookDragOperation;

#define ARROW_IS_LEFT(arrow)  ((arrow) == ARROW_LEFT_BEFORE || (arrow) == ARROW_LEFT_AFTER)
#define ARROW_IS_BEFORE(arrow) ((arrow) == ARROW_LEFT_BEFORE || (arrow) == ARROW_RIGHT_BEFORE)

enum {
  PROP_0,
  PROP_TAB_POS,
  PROP_SHOW_TABS,
  PROP_SHOW_BORDER,
  PROP_SCROLLABLE,
  PROP_TAB_BORDER,
  PROP_TAB_HBORDER,
  PROP_TAB_VBORDER,
  PROP_PAGE,
  PROP_ENABLE_POPUP,
  PROP_GROUP_ID,
  PROP_GROUP,
  PROP_GROUP_NAME,
  PROP_HOMOGENEOUS
};

enum {
  CHILD_PROP_0,
  CHILD_PROP_TAB_LABEL,
  CHILD_PROP_MENU_LABEL,
  CHILD_PROP_POSITION,
  CHILD_PROP_TAB_EXPAND,
  CHILD_PROP_TAB_FILL,
  CHILD_PROP_TAB_PACK,
  CHILD_PROP_REORDERABLE,
  CHILD_PROP_DETACHABLE
};

enum {
  ACTION_WIDGET_START,
  ACTION_WIDGET_END,
  N_ACTION_WIDGETS
};

#define GTK_NOTEBOOK_PAGE(_glist_)         ((GtkNotebookPage *)((GList *)(_glist_))->data)

/* some useful defines for calculating coords */
#define PAGE_LEFT_X(_page_)   (((GtkNotebookPage *) (_page_))->allocation.x)
#define PAGE_RIGHT_X(_page_)  (((GtkNotebookPage *) (_page_))->allocation.x + ((GtkNotebookPage *) (_page_))->allocation.width)
#define PAGE_MIDDLE_X(_page_) (((GtkNotebookPage *) (_page_))->allocation.x + ((GtkNotebookPage *) (_page_))->allocation.width / 2)
#define PAGE_TOP_Y(_page_)    (((GtkNotebookPage *) (_page_))->allocation.y)
#define PAGE_BOTTOM_Y(_page_) (((GtkNotebookPage *) (_page_))->allocation.y + ((GtkNotebookPage *) (_page_))->allocation.height)
#define PAGE_MIDDLE_Y(_page_) (((GtkNotebookPage *) (_page_))->allocation.y + ((GtkNotebookPage *) (_page_))->allocation.height / 2)
#define NOTEBOOK_IS_TAB_LABEL_PARENT(_notebook_,_page_) (((GtkNotebookPage *) (_page_))->tab_label->parent == ((GtkWidget *) (_notebook_)))

struct _GtkNotebookPage
{
  GtkWidget *child;
  GtkWidget *tab_label;
  GtkWidget *menu_label;
  GtkWidget *last_focus_child;	/* Last descendant of the page that had focus */

  guint default_menu : 1;	/* If true, we create the menu label ourself */
  guint default_tab  : 1;	/* If true, we create the tab label ourself */
  guint expand       : 1;
  guint fill         : 1;
  guint pack         : 1;
  guint reorderable  : 1;
  guint detachable   : 1;

  /* if true, the tab label was visible on last allocation; we track this so
   * that we know to redraw the tab area if a tab label was hidden then shown
   * without changing position */
  guint tab_allocated_visible : 1;

  GtkRequisition requisition;
  GtkAllocation allocation;

  gulong mnemonic_activate_signal;
  gulong notify_visible_handler;
};

#define GTK_NOTEBOOK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GTK_TYPE_NOTEBOOK, GtkNotebookPrivate))

typedef struct _GtkNotebookPrivate GtkNotebookPrivate;

struct _GtkNotebookPrivate
{
  gpointer group;
  gint  mouse_x;
  gint  mouse_y;
  gint  pressed_button;
  guint dnd_timer;
  guint switch_tab_timer;

  gint  drag_begin_x;
  gint  drag_begin_y;

  gint  drag_offset_x;
  gint  drag_offset_y;

  GtkWidget *dnd_window;
  GtkTargetList *source_targets;
  GtkNotebookDragOperation operation;
  GdkWindow *drag_window;
  gint drag_window_x;
  gint drag_window_y;
  GtkNotebookPage *detached_tab;

  guint32 timestamp;

  GtkWidget *action_widget[N_ACTION_WIDGETS];

  guint during_reorder : 1;
  guint during_detach  : 1;
  guint has_scrolled   : 1;
};

static const GtkTargetEntry notebook_targets [] = {
  { "GTK_NOTEBOOK_TAB", GTK_TARGET_SAME_APP, 0 },
};

#ifdef G_DISABLE_CHECKS
#define CHECK_FIND_CHILD(notebook, child)                           \
 gtk_notebook_find_child (notebook, child, G_STRLOC)
#else
#define CHECK_FIND_CHILD(notebook, child)                           \
 gtk_notebook_find_child (notebook, child, NULL)
#endif
 
/*** GtkNotebook Methods ***/
static gboolean gtk_notebook_select_page         (GtkNotebook      *notebook,
						  gboolean          move_focus);
static gboolean gtk_notebook_focus_tab           (GtkNotebook      *notebook,
						  GtkNotebookTab    type);
static gboolean gtk_notebook_change_current_page (GtkNotebook      *notebook,
						  gint              offset);
static void     gtk_notebook_move_focus_out      (GtkNotebook      *notebook,
						  GtkDirectionType  direction_type);
static gboolean gtk_notebook_reorder_tab         (GtkNotebook      *notebook,
						  GtkDirectionType  direction_type,
						  gboolean          move_to_last);
static void     gtk_notebook_remove_tab_label    (GtkNotebook      *notebook,
						  GtkNotebookPage  *page);

/*** GtkObject Methods ***/
static void gtk_notebook_destroy             (GtkObject        *object);
static void gtk_notebook_set_property	     (GObject         *object,
					      guint            prop_id,
					      const GValue    *value,
					      GParamSpec      *pspec);
static void gtk_notebook_get_property	     (GObject         *object,
					      guint            prop_id,
					      GValue          *value,
					      GParamSpec      *pspec);

/*** GtkWidget Methods ***/
static void gtk_notebook_map                 (GtkWidget        *widget);
static void gtk_notebook_unmap               (GtkWidget        *widget);
static void gtk_notebook_realize             (GtkWidget        *widget);
static void gtk_notebook_unrealize           (GtkWidget        *widget);
static void gtk_notebook_size_request        (GtkWidget        *widget,
					      GtkRequisition   *requisition);
static void gtk_notebook_size_allocate       (GtkWidget        *widget,
					      GtkAllocation    *allocation);
static gint gtk_notebook_expose              (GtkWidget        *widget,
					      GdkEventExpose   *event);
static gboolean gtk_notebook_scroll          (GtkWidget        *widget,
                                              GdkEventScroll   *event);
static gint gtk_notebook_button_press        (GtkWidget        *widget,
					      GdkEventButton   *event);
static gint gtk_notebook_button_release      (GtkWidget        *widget,
					      GdkEventButton   *event);
static gboolean gtk_notebook_popup_menu      (GtkWidget        *widget);
static gint gtk_notebook_leave_notify        (GtkWidget        *widget,
					      GdkEventCrossing *event);
static gint gtk_notebook_motion_notify       (GtkWidget        *widget,
					      GdkEventMotion   *event);
static gint gtk_notebook_focus_in            (GtkWidget        *widget,
					      GdkEventFocus    *event);
static gint gtk_notebook_focus_out           (GtkWidget        *widget,
					      GdkEventFocus    *event);
static void gtk_notebook_grab_notify         (GtkWidget          *widget,
					      gboolean            was_grabbed);
static void gtk_notebook_state_changed       (GtkWidget          *widget,
					      GtkStateType        previous_state);
static void gtk_notebook_draw_focus          (GtkWidget        *widget,
					      GdkEventExpose   *event);
static gint gtk_notebook_focus               (GtkWidget        *widget,
					      GtkDirectionType  direction);
static void gtk_notebook_style_set           (GtkWidget        *widget,
					      GtkStyle         *previous);

/*** Drag and drop Methods ***/
static void gtk_notebook_drag_begin          (GtkWidget        *widget,
					      GdkDragContext   *context);
static void gtk_notebook_drag_end            (GtkWidget        *widget,
					      GdkDragContext   *context);
static gboolean gtk_notebook_drag_failed     (GtkWidget        *widget,
					      GdkDragContext   *context,
					      GtkDragResult     result,
					      gpointer          data);
static gboolean gtk_notebook_drag_motion     (GtkWidget        *widget,
					      GdkDragContext   *context,
					      gint              x,
					      gint              y,
					      guint             time);
static void gtk_notebook_drag_leave          (GtkWidget        *widget,
					      GdkDragContext   *context,
					      guint             time);
static gboolean gtk_notebook_drag_drop       (GtkWidget        *widget,
					      GdkDragContext   *context,
					      gint              x,
					      gint              y,
					      guint             time);
static void gtk_notebook_drag_data_get       (GtkWidget        *widget,
					      GdkDragContext   *context,
					      GtkSelectionData *data,
					      guint             info,
					      guint             time);
static void gtk_notebook_drag_data_received  (GtkWidget        *widget,
					      GdkDragContext   *context,
					      gint              x,
					      gint              y,
					      GtkSelectionData *data,
					      guint             info,
					      guint             time);

/*** GtkContainer Methods ***/
static void gtk_notebook_set_child_property  (GtkContainer     *container,
					      GtkWidget        *child,
					      guint             property_id,
					      const GValue     *value,
					      GParamSpec       *pspec);
static void gtk_notebook_get_child_property  (GtkContainer     *container,
					      GtkWidget        *child,
					      guint             property_id,
					      GValue           *value,
					      GParamSpec       *pspec);
static void gtk_notebook_add                 (GtkContainer     *container,
					      GtkWidget        *widget);
static void gtk_notebook_remove              (GtkContainer     *container,
					      GtkWidget        *widget);
static void gtk_notebook_set_focus_child     (GtkContainer     *container,
					      GtkWidget        *child);
static GType gtk_notebook_child_type       (GtkContainer     *container);
static void gtk_notebook_forall              (GtkContainer     *container,
					      gboolean		include_internals,
					      GtkCallback       callback,
					      gpointer          callback_data);

/*** GtkNotebook Methods ***/
static gint gtk_notebook_real_insert_page    (GtkNotebook      *notebook,
					      GtkWidget        *child,
					      GtkWidget        *tab_label,
					      GtkWidget        *menu_label,
					      gint              position);

static GtkNotebook *gtk_notebook_create_window (GtkNotebook    *notebook,
                                                GtkWidget      *page,
                                                gint            x,
                                                gint            y);

/*** GtkNotebook Private Functions ***/
static void gtk_notebook_redraw_tabs         (GtkNotebook      *notebook);
static void gtk_notebook_redraw_arrows       (GtkNotebook      *notebook);
static void gtk_notebook_real_remove         (GtkNotebook      *notebook,
					      GList            *list);
static void gtk_notebook_update_labels       (GtkNotebook      *notebook);
static gint gtk_notebook_timer               (GtkNotebook      *notebook);
static void gtk_notebook_set_scroll_timer    (GtkNotebook *notebook);
static gint gtk_notebook_page_compare        (gconstpointer     a,
					      gconstpointer     b);
static GList* gtk_notebook_find_child        (GtkNotebook      *notebook,
					      GtkWidget        *child,
					      const gchar      *function);
static gint  gtk_notebook_real_page_position (GtkNotebook      *notebook,
					      GList            *list);
static GList * gtk_notebook_search_page      (GtkNotebook      *notebook,
					      GList            *list,
					      gint              direction,
					      gboolean          find_visible);
static void  gtk_notebook_child_reordered    (GtkNotebook      *notebook,
			                      GtkNotebookPage  *page);

/*** GtkNotebook Drawing Functions ***/
static void gtk_notebook_paint               (GtkWidget        *widget,
					      GdkRectangle     *area);
static void gtk_notebook_draw_tab            (GtkNotebook      *notebook,
					      GtkNotebookPage  *page,
					      GdkRectangle     *area);
static void gtk_notebook_draw_arrow          (GtkNotebook      *notebook,
					      GtkNotebookArrow  arrow);

/*** GtkNotebook Size Allocate Functions ***/
static void gtk_notebook_pages_allocate      (GtkNotebook      *notebook);
static gboolean gtk_notebook_page_allocate   (GtkNotebook      *notebook,
					      GtkNotebookPage  *page);
static void gtk_notebook_calc_tabs           (GtkNotebook      *notebook,
			                      GList            *start,
					      GList           **end,
					      gint             *tab_space,
					      guint             direction);

/*** GtkNotebook Page Switch Methods ***/
static void gtk_notebook_real_switch_page    (GtkNotebook      *notebook,
					      GtkNotebookPage  *child,
					      guint             page_num);

/*** GtkNotebook Page Switch Functions ***/
static void gtk_notebook_switch_page         (GtkNotebook      *notebook,
					      GtkNotebookPage  *page);
static gint gtk_notebook_page_select         (GtkNotebook      *notebook,
					      gboolean          move_focus);
static void gtk_notebook_switch_focus_tab    (GtkNotebook      *notebook,
                                              GList            *new_child);
static void gtk_notebook_menu_switch_page    (GtkWidget        *widget,
					      GtkNotebookPage  *page);

/*** GtkNotebook Menu Functions ***/
static void gtk_notebook_menu_item_create    (GtkNotebook      *notebook,
					      GList            *list);
static void gtk_notebook_menu_label_unparent (GtkWidget        *widget,
					      gpointer          data);
static void gtk_notebook_menu_detacher       (GtkWidget        *widget,
					      GtkMenu          *menu);

/*** GtkNotebook Private Setters ***/
static void gtk_notebook_set_homogeneous_tabs_internal (GtkNotebook *notebook,
							gboolean     homogeneous);
static void gtk_notebook_set_tab_border_internal       (GtkNotebook *notebook,
							guint        border_width);
static void gtk_notebook_set_tab_hborder_internal      (GtkNotebook *notebook,
							guint        tab_hborder);
static void gtk_notebook_set_tab_vborder_internal      (GtkNotebook *notebook,
							guint        tab_vborder);

static void gtk_notebook_update_tab_states             (GtkNotebook *notebook);
static gboolean gtk_notebook_mnemonic_activate_switch_page (GtkWidget *child,
							    gboolean overload,
							    gpointer data);

static gboolean focus_tabs_in  (GtkNotebook      *notebook);
static gboolean focus_child_in (GtkNotebook      *notebook,
				GtkDirectionType  direction);

static void stop_scrolling (GtkNotebook *notebook);
static void do_detach_tab  (GtkNotebook *from,
			    GtkNotebook *to,
			    GtkWidget   *child,
			    gint         x,
			    gint         y);

/* GtkBuildable */
static void gtk_notebook_buildable_init           (GtkBuildableIface *iface);
static void gtk_notebook_buildable_add_child      (GtkBuildable *buildable,
						   GtkBuilder   *builder,
						   GObject      *child,
						   const gchar  *type);

static GtkNotebookWindowCreationFunc window_creation_hook = NULL;
static gpointer window_creation_hook_data;
static GDestroyNotify window_creation_hook_destroy = NULL;

static guint notebook_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_CODE (GtkNotebook, gtk_notebook, GTK_TYPE_CONTAINER,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
						gtk_notebook_buildable_init))

static void
add_tab_bindings (GtkBindingSet    *binding_set,
		  GdkModifierType   modifiers,
		  GtkDirectionType  direction)
{
  gtk_binding_entry_add_signal (binding_set, GDK_Tab, modifiers,
                                "move_focus_out", 1,
                                GTK_TYPE_DIRECTION_TYPE, direction);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Tab, modifiers,
                                "move_focus_out", 1,
                                GTK_TYPE_DIRECTION_TYPE, direction);
}

static void
add_arrow_bindings (GtkBindingSet    *binding_set,
		    guint             keysym,
		    GtkDirectionType  direction)
{
  guint keypad_keysym = keysym - GDK_Left + GDK_KP_Left;
  
  gtk_binding_entry_add_signal (binding_set, keysym, GDK_CONTROL_MASK,
                                "move_focus_out", 1,
                                GTK_TYPE_DIRECTION_TYPE, direction);
  gtk_binding_entry_add_signal (binding_set, keypad_keysym, GDK_CONTROL_MASK,
                                "move_focus_out", 1,
                                GTK_TYPE_DIRECTION_TYPE, direction);
}

static void
add_reorder_bindings (GtkBindingSet    *binding_set,
		      guint             keysym,
		      GtkDirectionType  direction,
		      gboolean          move_to_last)
{
  guint keypad_keysym = keysym - GDK_Left + GDK_KP_Left;

  gtk_binding_entry_add_signal (binding_set, keysym, GDK_MOD1_MASK,
				"reorder_tab", 2,
				GTK_TYPE_DIRECTION_TYPE, direction,
				G_TYPE_BOOLEAN, move_to_last);
  gtk_binding_entry_add_signal (binding_set, keypad_keysym, GDK_MOD1_MASK,
				"reorder_tab", 2,
				GTK_TYPE_DIRECTION_TYPE, direction,
				G_TYPE_BOOLEAN, move_to_last);
}

static gboolean
gtk_object_handled_accumulator (GSignalInvocationHint *ihint,
                                GValue                *return_accu,
                                const GValue          *handler_return,
                                gpointer               dummy)
{
  gboolean continue_emission;
  GObject *object;

  object = g_value_get_object (handler_return);
  g_value_set_object (return_accu, object);
  continue_emission = !object;

  return continue_emission;
}

static void
gtk_notebook_class_init (GtkNotebookClass *class)
{
  GObjectClass   *gobject_class = G_OBJECT_CLASS (class);
  GtkObjectClass *object_class = GTK_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (class);
  GtkBindingSet *binding_set;
  
  gobject_class->set_property = gtk_notebook_set_property;
  gobject_class->get_property = gtk_notebook_get_property;
  object_class->destroy = gtk_notebook_destroy;

  widget_class->map = gtk_notebook_map;
  widget_class->unmap = gtk_notebook_unmap;
  widget_class->realize = gtk_notebook_realize;
  widget_class->unrealize = gtk_notebook_unrealize;
  widget_class->size_request = gtk_notebook_size_request;
  widget_class->size_allocate = gtk_notebook_size_allocate;
  widget_class->expose_event = gtk_notebook_expose;
  widget_class->scroll_event = gtk_notebook_scroll;
  widget_class->button_press_event = gtk_notebook_button_press;
  widget_class->button_release_event = gtk_notebook_button_release;
  widget_class->popup_menu = gtk_notebook_popup_menu;
  widget_class->leave_notify_event = gtk_notebook_leave_notify;
  widget_class->motion_notify_event = gtk_notebook_motion_notify;
  widget_class->grab_notify = gtk_notebook_grab_notify;
  widget_class->state_changed = gtk_notebook_state_changed;
  widget_class->focus_in_event = gtk_notebook_focus_in;
  widget_class->focus_out_event = gtk_notebook_focus_out;
  widget_class->focus = gtk_notebook_focus;
  widget_class->style_set = gtk_notebook_style_set;
  widget_class->drag_begin = gtk_notebook_drag_begin;
  widget_class->drag_end = gtk_notebook_drag_end;
  widget_class->drag_motion = gtk_notebook_drag_motion;
  widget_class->drag_leave = gtk_notebook_drag_leave;
  widget_class->drag_drop = gtk_notebook_drag_drop;
  widget_class->drag_data_get = gtk_notebook_drag_data_get;
  widget_class->drag_data_received = gtk_notebook_drag_data_received;

  container_class->add = gtk_notebook_add;
  container_class->remove = gtk_notebook_remove;
  container_class->forall = gtk_notebook_forall;
  container_class->set_focus_child = gtk_notebook_set_focus_child;
  container_class->get_child_property = gtk_notebook_get_child_property;
  container_class->set_child_property = gtk_notebook_set_child_property;
  container_class->child_type = gtk_notebook_child_type;

  class->switch_page = gtk_notebook_real_switch_page;
  class->insert_page = gtk_notebook_real_insert_page;

  class->focus_tab = gtk_notebook_focus_tab;
  class->select_page = gtk_notebook_select_page;
  class->change_current_page = gtk_notebook_change_current_page;
  class->move_focus_out = gtk_notebook_move_focus_out;
  class->reorder_tab = gtk_notebook_reorder_tab;
  class->create_window = gtk_notebook_create_window;
  
  g_object_class_install_property (gobject_class,
				   PROP_PAGE,
				   g_param_spec_int ("page",
 						     P_("Page"),
 						     P_("The index of the current page"),
 						     -1,
 						     G_MAXINT,
 						     -1,
 						     GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_TAB_POS,
				   g_param_spec_enum ("tab-pos",
 						      P_("Tab Position"),
 						      P_("Which side of the notebook holds the tabs"),
 						      GTK_TYPE_POSITION_TYPE,
 						      GTK_POS_TOP,
 						      GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_TAB_BORDER,
				   g_param_spec_uint ("tab-border",
 						      P_("Tab Border"),
 						      P_("Width of the border around the tab labels"),
 						      0,
 						      G_MAXUINT,
 						      2,
 						      GTK_PARAM_WRITABLE));
  g_object_class_install_property (gobject_class,
				   PROP_TAB_HBORDER,
				   g_param_spec_uint ("tab-hborder",
 						      P_("Horizontal Tab Border"),
 						      P_("Width of the horizontal border of tab labels"),
 						      0,
 						      G_MAXUINT,
 						      2,
 						      GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_TAB_VBORDER,
				   g_param_spec_uint ("tab-vborder",
 						      P_("Vertical Tab Border"),
 						      P_("Width of the vertical border of tab labels"),
 						      0,
 						      G_MAXUINT,
 						      2,
 						      GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_SHOW_TABS,
				   g_param_spec_boolean ("show-tabs",
 							 P_("Show Tabs"),
 							 P_("Whether tabs should be shown or not"),
 							 TRUE,
 							 GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_SHOW_BORDER,
				   g_param_spec_boolean ("show-border",
 							 P_("Show Border"),
 							 P_("Whether the border should be shown or not"),
 							 TRUE,
 							 GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_SCROLLABLE,
				   g_param_spec_boolean ("scrollable",
 							 P_("Scrollable"),
 							 P_("If TRUE, scroll arrows are added if there are too many tabs to fit"),
 							 FALSE,
 							 GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_ENABLE_POPUP,
				   g_param_spec_boolean ("enable-popup",
 							 P_("Enable Popup"),
 							 P_("If TRUE, pressing the right mouse button on the notebook pops up a menu that you can use to go to a page"),
 							 FALSE,
 							 GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_HOMOGENEOUS,
				   g_param_spec_boolean ("homogeneous",
 							 P_("Homogeneous"),
 							 P_("Whether tabs should have homogeneous sizes"),
 							 FALSE,
							 GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_GROUP_ID,
				   g_param_spec_int ("group-id",
						     P_("Group ID"),
						     P_("Group ID for tabs drag and drop"),
						     -1,
						     G_MAXINT,
						     -1,
						     GTK_PARAM_READWRITE|G_PARAM_DEPRECATED));

  /**
   * GtkNotebook:group:
   *  
   * Group for tabs drag and drop.
   *
   * Since: 2.12
   *
   * Deprecated: 2.24: Use #GtkNotebook:group-name instead
   */    
  g_object_class_install_property (gobject_class,
				   PROP_GROUP,
				   g_param_spec_pointer ("group",
							 P_("Group"),
							 P_("Group for tabs drag and drop"),
							 GTK_PARAM_READWRITE|G_PARAM_DEPRECATED));

  /**
   * GtkNotebook:group-name:
   *
   * Group name for tabs drag and drop.
   *
   * Since: 2.24
   */
  g_object_class_install_property (gobject_class,
                                   PROP_GROUP_NAME,
                                   g_param_spec_string ("group-name",
                                   P_("Group Name"),
                                   P_("Group name for tabs drag and drop"),
                                   NULL,
                                   GTK_PARAM_READWRITE));

  gtk_container_class_install_child_property (container_class,
					      CHILD_PROP_TAB_LABEL,
					      g_param_spec_string ("tab-label", 
								   P_("Tab label"),
								   P_("The string displayed on the child's tab label"),
								   NULL,
								   GTK_PARAM_READWRITE));
  gtk_container_class_install_child_property (container_class,
					      CHILD_PROP_MENU_LABEL,
					      g_param_spec_string ("menu-label", 
								   P_("Menu label"), 
								   P_("The string displayed in the child's menu entry"),
								   NULL,
								   GTK_PARAM_READWRITE));
  gtk_container_class_install_child_property (container_class,
					      CHILD_PROP_POSITION,
					      g_param_spec_int ("position", 
								P_("Position"), 
								P_("The index of the child in the parent"),
								-1, G_MAXINT, 0,
								GTK_PARAM_READWRITE));
  gtk_container_class_install_child_property (container_class,
					      CHILD_PROP_TAB_EXPAND,
					      g_param_spec_boolean ("tab-expand", 
								    P_("Tab expand"), 
								    P_("Whether to expand the child's tab or not"),
								    FALSE,
								    GTK_PARAM_READWRITE));
  gtk_container_class_install_child_property (container_class,
					      CHILD_PROP_TAB_FILL,
					      g_param_spec_boolean ("tab-fill", 
								    P_("Tab fill"), 
								    P_("Whether the child's tab should fill the allocated area or not"),
								    TRUE,
								    GTK_PARAM_READWRITE));
  gtk_container_class_install_child_property (container_class,
					      CHILD_PROP_TAB_PACK,
					      g_param_spec_enum ("tab-pack", 
								 P_("Tab pack type"),
								 P_("A GtkPackType indicating whether the child is packed with reference to the start or end of the parent"),
								 GTK_TYPE_PACK_TYPE, GTK_PACK_START,
								 GTK_PARAM_READWRITE));
  gtk_container_class_install_child_property (container_class,
					      CHILD_PROP_REORDERABLE,
					      g_param_spec_boolean ("reorderable",
								    P_("Tab reorderable"),
								    P_("Whether the tab is reorderable by user action or not"),
								    FALSE,
								    GTK_PARAM_READWRITE));
  gtk_container_class_install_child_property (container_class,
					      CHILD_PROP_DETACHABLE,
					      g_param_spec_boolean ("detachable",
								    P_("Tab detachable"),
								    P_("Whether the tab is detachable"),
								    FALSE,
								    GTK_PARAM_READWRITE));

/**
 * GtkNotebook:has-secondary-backward-stepper:
 *
 * The "has-secondary-backward-stepper" property determines whether 
 * a second backward arrow button is displayed on the opposite end 
 * of the tab area.
 *
 * Since: 2.4
 */  
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_boolean ("has-secondary-backward-stepper",
								 P_("Secondary backward stepper"),
								 P_("Display a second backward arrow button on the opposite end of the tab area"),
								 FALSE,
								 GTK_PARAM_READABLE));

/**
 * GtkNotebook:has-secondary-forward-stepper:
 *
 * The "has-secondary-forward-stepper" property determines whether 
 * a second forward arrow button is displayed on the opposite end 
 * of the tab area.
 *
 * Since: 2.4
 */  
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_boolean ("has-secondary-forward-stepper",
								 P_("Secondary forward stepper"),
								 P_("Display a second forward arrow button on the opposite end of the tab area"),
								 FALSE,
								 GTK_PARAM_READABLE));

/**
 * GtkNotebook:has-backward-stepper:
 *
 * The "has-backward-stepper" property determines whether 
 * the standard backward arrow button is displayed.
 *
 * Since: 2.4
 */  
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_boolean ("has-backward-stepper",
								 P_("Backward stepper"),
								 P_("Display the standard backward arrow button"),
								 TRUE,
								 GTK_PARAM_READABLE));

/**
 * GtkNotebook:has-forward-stepper:
 *
 * The "has-forward-stepper" property determines whether 
 * the standard forward arrow button is displayed.
 *
 * Since: 2.4
 */  
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_boolean ("has-forward-stepper",
								 P_("Forward stepper"),
								 P_("Display the standard forward arrow button"),
								 TRUE,
								 GTK_PARAM_READABLE));
  
/**
 * GtkNotebook:tab-overlap:
 *
 * The "tab-overlap" property defines size of tab overlap
 * area.
 *
 * Since: 2.10
 */  
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("tab-overlap",
							     P_("Tab overlap"),
							     P_("Size of tab overlap area"),
							     G_MININT,
							     G_MAXINT,
							     2,
							     GTK_PARAM_READABLE));

/**
 * GtkNotebook:tab-curvature:
 *
 * The "tab-curvature" property defines size of tab curvature.
 *
 * Since: 2.10
 */  
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("tab-curvature",
							     P_("Tab curvature"),
							     P_("Size of tab curvature"),
							     0,
							     G_MAXINT,
							     1,
							     GTK_PARAM_READABLE));

  /**
   * GtkNotebook:arrow-spacing:
   *
   * The "arrow-spacing" property defines the spacing between the scroll
   * arrows and the tabs.
   *
   * Since: 2.10
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("arrow-spacing",
                                                             P_("Arrow spacing"),
                                                             P_("Scroll arrow spacing"),
                                                             0,
                                                             G_MAXINT,
                                                             0,
                                                             GTK_PARAM_READABLE));

  notebook_signals[SWITCH_PAGE] =
    g_signal_new (I_("switch-page"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkNotebookClass, switch_page),
		  NULL, NULL,
		  _gtk_marshal_VOID__POINTER_UINT,
		  G_TYPE_NONE, 2,
		  G_TYPE_POINTER,
		  G_TYPE_UINT);
  notebook_signals[FOCUS_TAB] = 
    g_signal_new (I_("focus-tab"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkNotebookClass, focus_tab),
                  NULL, NULL,
                  _gtk_marshal_BOOLEAN__ENUM,
                  G_TYPE_BOOLEAN, 1,
                  GTK_TYPE_NOTEBOOK_TAB);
  notebook_signals[SELECT_PAGE] = 
    g_signal_new (I_("select-page"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkNotebookClass, select_page),
                  NULL, NULL,
                  _gtk_marshal_BOOLEAN__BOOLEAN,
                  G_TYPE_BOOLEAN, 1,
                  G_TYPE_BOOLEAN);
  notebook_signals[CHANGE_CURRENT_PAGE] = 
    g_signal_new (I_("change-current-page"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkNotebookClass, change_current_page),
                  NULL, NULL,
                  _gtk_marshal_BOOLEAN__INT,
                  G_TYPE_BOOLEAN, 1,
                  G_TYPE_INT);
  notebook_signals[MOVE_FOCUS_OUT] =
    g_signal_new (I_("move-focus-out"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkNotebookClass, move_focus_out),
                  NULL, NULL,
                  _gtk_marshal_VOID__ENUM,
                  G_TYPE_NONE, 1,
                  GTK_TYPE_DIRECTION_TYPE);
  notebook_signals[REORDER_TAB] =
    g_signal_new (I_("reorder-tab"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkNotebookClass, reorder_tab),
                  NULL, NULL,
                  _gtk_marshal_BOOLEAN__ENUM_BOOLEAN,
                  G_TYPE_BOOLEAN, 2,
                  GTK_TYPE_DIRECTION_TYPE,
		  G_TYPE_BOOLEAN);
  /**
   * GtkNotebook::page-reordered:
   * @notebook: the #GtkNotebook
   * @child: the child #GtkWidget affected
   * @page_num: the new page number for @child
   *
   * the ::page-reordered signal is emitted in the notebook
   * right after a page has been reordered.
   *
   * Since: 2.10
   **/
  notebook_signals[PAGE_REORDERED] =
    g_signal_new (I_("page-reordered"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
		  _gtk_marshal_VOID__OBJECT_UINT,
                  G_TYPE_NONE, 2,
		  GTK_TYPE_WIDGET,
		  G_TYPE_UINT);
  /**
   * GtkNotebook::page-removed:
   * @notebook: the #GtkNotebook
   * @child: the child #GtkWidget affected
   * @page_num: the @child page number
   *
   * the ::page-removed signal is emitted in the notebook
   * right after a page is removed from the notebook.
   *
   * Since: 2.10
   **/
  notebook_signals[PAGE_REMOVED] =
    g_signal_new (I_("page-removed"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
		  _gtk_marshal_VOID__OBJECT_UINT,
                  G_TYPE_NONE, 2,
		  GTK_TYPE_WIDGET,
		  G_TYPE_UINT);
  /**
   * GtkNotebook::page-added:
   * @notebook: the #GtkNotebook
   * @child: the child #GtkWidget affected
   * @page_num: the new page number for @child
   *
   * the ::page-added signal is emitted in the notebook
   * right after a page is added to the notebook.
   *
   * Since: 2.10
   **/
  notebook_signals[PAGE_ADDED] =
    g_signal_new (I_("page-added"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
		  _gtk_marshal_VOID__OBJECT_UINT,
                  G_TYPE_NONE, 2,
		  GTK_TYPE_WIDGET,
		  G_TYPE_UINT);

  /**
   * GtkNotebook::create-window:
   * @notebook: the #GtkNotebook emitting the signal
   * @page: the tab of @notebook that is being detached
   * @x: the X coordinate where the drop happens
   * @y: the Y coordinate where the drop happens
   *
   * The ::create-window signal is emitted when a detachable
   * tab is dropped on the root window. 
   *
   * A handler for this signal can create a window containing 
   * a notebook where the tab will be attached. It is also 
   * responsible for moving/resizing the window and adding the 
   * necessary properties to the notebook (e.g. the 
   * #GtkNotebook:group ).
   *
   * The default handler uses the global window creation hook,
   * if one has been set with gtk_notebook_set_window_creation_hook().
   *
   * Returns: (transfer none): a #GtkNotebook that @page should be
   *     added to, or %NULL.
   *
   * Since: 2.12
   */
  notebook_signals[CREATE_WINDOW] = 
    g_signal_new (I_("create-window"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkNotebookClass, create_window),
                  gtk_object_handled_accumulator, NULL,
                  _gtk_marshal_OBJECT__OBJECT_INT_INT,
                  GTK_TYPE_NOTEBOOK, 3,
                  GTK_TYPE_WIDGET, G_TYPE_INT, G_TYPE_INT);
 
  binding_set = gtk_binding_set_by_class (class);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_space, 0,
                                "select-page", 1, 
                                G_TYPE_BOOLEAN, FALSE);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_KP_Space, 0,
                                "select-page", 1, 
                                G_TYPE_BOOLEAN, FALSE);
  
  gtk_binding_entry_add_signal (binding_set,
                                GDK_Home, 0,
                                "focus-tab", 1, 
                                GTK_TYPE_NOTEBOOK_TAB, GTK_NOTEBOOK_TAB_FIRST);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_KP_Home, 0,
                                "focus-tab", 1, 
                                GTK_TYPE_NOTEBOOK_TAB, GTK_NOTEBOOK_TAB_FIRST);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_End, 0,
                                "focus-tab", 1, 
                                GTK_TYPE_NOTEBOOK_TAB, GTK_NOTEBOOK_TAB_LAST);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_KP_End, 0,
                                "focus-tab", 1, 
                                GTK_TYPE_NOTEBOOK_TAB, GTK_NOTEBOOK_TAB_LAST);

  gtk_binding_entry_add_signal (binding_set,
                                GDK_Page_Up, GDK_CONTROL_MASK,
                                "change-current-page", 1,
                                G_TYPE_INT, -1);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_Page_Down, GDK_CONTROL_MASK,
                                "change-current-page", 1,
                                G_TYPE_INT, 1);

  gtk_binding_entry_add_signal (binding_set,
                                GDK_Page_Up, GDK_CONTROL_MASK | GDK_MOD1_MASK,
                                "change-current-page", 1,
                                G_TYPE_INT, -1);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_Page_Down, GDK_CONTROL_MASK | GDK_MOD1_MASK,
                                "change-current-page", 1,
                                G_TYPE_INT, 1);

  add_arrow_bindings (binding_set, GDK_Up, GTK_DIR_UP);
  add_arrow_bindings (binding_set, GDK_Down, GTK_DIR_DOWN);
  add_arrow_bindings (binding_set, GDK_Left, GTK_DIR_LEFT);
  add_arrow_bindings (binding_set, GDK_Right, GTK_DIR_RIGHT);

  add_reorder_bindings (binding_set, GDK_Up, GTK_DIR_UP, FALSE);
  add_reorder_bindings (binding_set, GDK_Down, GTK_DIR_DOWN, FALSE);
  add_reorder_bindings (binding_set, GDK_Left, GTK_DIR_LEFT, FALSE);
  add_reorder_bindings (binding_set, GDK_Right, GTK_DIR_RIGHT, FALSE);
  add_reorder_bindings (binding_set, GDK_Home, GTK_DIR_LEFT, TRUE);
  add_reorder_bindings (binding_set, GDK_Home, GTK_DIR_UP, TRUE);
  add_reorder_bindings (binding_set, GDK_End, GTK_DIR_RIGHT, TRUE);
  add_reorder_bindings (binding_set, GDK_End, GTK_DIR_DOWN, TRUE);

  add_tab_bindings (binding_set, GDK_CONTROL_MASK, GTK_DIR_TAB_FORWARD);
  add_tab_bindings (binding_set, GDK_CONTROL_MASK | GDK_SHIFT_MASK, GTK_DIR_TAB_BACKWARD);

  g_type_class_add_private (class, sizeof (GtkNotebookPrivate));
}

static void
gtk_notebook_init (GtkNotebook *notebook)
{
  GtkNotebookPrivate *priv;

  gtk_widget_set_can_focus (GTK_WIDGET (notebook), TRUE);
  gtk_widget_set_has_window (GTK_WIDGET (notebook), FALSE);

  priv = GTK_NOTEBOOK_GET_PRIVATE (notebook);

  notebook->cur_page = NULL;
  notebook->children = NULL;
  notebook->first_tab = NULL;
  notebook->focus_tab = NULL;
  notebook->event_window = NULL;
  notebook->menu = NULL;

  notebook->tab_hborder = 2;
  notebook->tab_vborder = 2;

  notebook->show_tabs = TRUE;
  notebook->show_border = TRUE;
  notebook->tab_pos = GTK_POS_TOP;
  notebook->scrollable = FALSE;
  notebook->in_child = 0;
  notebook->click_child = 0;
  notebook->button = 0;
  notebook->need_timer = 0;
  notebook->child_has_focus = FALSE;
  notebook->have_visible_child = FALSE;
  notebook->focus_out = FALSE;

  notebook->has_before_previous = 1;
  notebook->has_before_next     = 0;
  notebook->has_after_previous  = 0;
  notebook->has_after_next      = 1;

  priv->group = NULL;
  priv->pressed_button = -1;
  priv->dnd_timer = 0;
  priv->switch_tab_timer = 0;
  priv->source_targets = gtk_target_list_new (notebook_targets,
					      G_N_ELEMENTS (notebook_targets));
  priv->operation = DRAG_OPERATION_NONE;
  priv->detached_tab = NULL;
  priv->during_detach = FALSE;
  priv->has_scrolled = FALSE;

  gtk_drag_dest_set (GTK_WIDGET (notebook), 0,
		     notebook_targets, G_N_ELEMENTS (notebook_targets),
                     GDK_ACTION_MOVE);

  g_signal_connect (G_OBJECT (notebook), "drag-failed",
		    G_CALLBACK (gtk_notebook_drag_failed), NULL);

  gtk_drag_dest_set_track_motion (GTK_WIDGET (notebook), TRUE);
}

static void
gtk_notebook_buildable_init (GtkBuildableIface *iface)
{
  iface->add_child = gtk_notebook_buildable_add_child;
}

static void
gtk_notebook_buildable_add_child (GtkBuildable  *buildable,
				  GtkBuilder    *builder,
				  GObject       *child,
				  const gchar   *type)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (buildable);

  if (type && strcmp (type, "tab") == 0)
    {
      GtkWidget * page;

      page = gtk_notebook_get_nth_page (notebook, -1);
      /* To set the tab label widget, we must have already a child
       * inside the tab container. */
      g_assert (page != NULL);
      gtk_notebook_set_tab_label (notebook, page, GTK_WIDGET (child));
    }
  else if (type && strcmp (type, "action-start") == 0)
    {
      gtk_notebook_set_action_widget (notebook, GTK_WIDGET (child), GTK_PACK_START);
    }
  else if (type && strcmp (type, "action-end") == 0)
    {
      gtk_notebook_set_action_widget (notebook, GTK_WIDGET (child), GTK_PACK_END);
    }
  else if (!type)
    gtk_notebook_append_page (notebook, GTK_WIDGET (child), NULL);
  else
    GTK_BUILDER_WARN_INVALID_CHILD_TYPE (notebook, type);
}

static gboolean
gtk_notebook_select_page (GtkNotebook *notebook,
                          gboolean     move_focus)
{
  if (gtk_widget_is_focus (GTK_WIDGET (notebook)) && notebook->show_tabs)
    {
      gtk_notebook_page_select (notebook, move_focus);
      return TRUE;
    }
  else
    return FALSE;
}

static gboolean
gtk_notebook_focus_tab (GtkNotebook       *notebook,
                        GtkNotebookTab     type)
{
  GList *list;

  if (gtk_widget_is_focus (GTK_WIDGET (notebook)) && notebook->show_tabs)
    {
      switch (type)
	{
	case GTK_NOTEBOOK_TAB_FIRST:
	  list = gtk_notebook_search_page (notebook, NULL, STEP_NEXT, TRUE);
	  if (list)
	    gtk_notebook_switch_focus_tab (notebook, list);
	  break;
	case GTK_NOTEBOOK_TAB_LAST:
	  list = gtk_notebook_search_page (notebook, NULL, STEP_PREV, TRUE);
	  if (list)
	    gtk_notebook_switch_focus_tab (notebook, list);
	  break;
	}

      return TRUE;
    }
  else
    return FALSE;
}

static gboolean
gtk_notebook_change_current_page (GtkNotebook *notebook,
				  gint         offset)
{
  GList *current = NULL;

  if (!notebook->show_tabs)
    return FALSE;

  if (notebook->cur_page)
    current = g_list_find (notebook->children, notebook->cur_page);

  while (offset != 0)
    {
      current = gtk_notebook_search_page (notebook, current,
                                          offset < 0 ? STEP_PREV : STEP_NEXT,
                                          TRUE);

      if (!current)
        {
          gboolean wrap_around;

          g_object_get (gtk_widget_get_settings (GTK_WIDGET (notebook)),
                        "gtk-keynav-wrap-around", &wrap_around,
                        NULL);

          if (wrap_around)
            current = gtk_notebook_search_page (notebook, NULL,
                                                offset < 0 ? STEP_PREV : STEP_NEXT,
                                                TRUE);
          else
            break;
        }

      offset += offset < 0 ? 1 : -1;
    }

  if (current)
    gtk_notebook_switch_page (notebook, current->data);
  else
    gtk_widget_error_bell (GTK_WIDGET (notebook));

  return TRUE;
}

static GtkDirectionType
get_effective_direction (GtkNotebook      *notebook,
			 GtkDirectionType  direction)
{
  /* Remap the directions into the effective direction it would be for a
   * GTK_POS_TOP notebook
   */

#define D(rest) GTK_DIR_##rest

  static const GtkDirectionType translate_direction[2][4][6] = {
    /* LEFT */   {{ D(TAB_FORWARD),  D(TAB_BACKWARD), D(LEFT), D(RIGHT), D(UP),   D(DOWN) },
    /* RIGHT */  { D(TAB_BACKWARD), D(TAB_FORWARD),  D(LEFT), D(RIGHT), D(DOWN), D(UP)   },
    /* TOP */    { D(TAB_FORWARD),  D(TAB_BACKWARD), D(UP),   D(DOWN),  D(LEFT), D(RIGHT) },
    /* BOTTOM */ { D(TAB_BACKWARD), D(TAB_FORWARD),  D(DOWN), D(UP),    D(LEFT), D(RIGHT) }},
    /* LEFT */  {{ D(TAB_BACKWARD), D(TAB_FORWARD),  D(LEFT), D(RIGHT), D(DOWN), D(UP)   },
    /* RIGHT */  { D(TAB_FORWARD),  D(TAB_BACKWARD), D(LEFT), D(RIGHT), D(UP),   D(DOWN) },
    /* TOP */    { D(TAB_FORWARD),  D(TAB_BACKWARD), D(UP),   D(DOWN),  D(RIGHT), D(LEFT) },
    /* BOTTOM */ { D(TAB_BACKWARD), D(TAB_FORWARD),  D(DOWN), D(UP),    D(RIGHT), D(LEFT) }},
  };

#undef D

  int text_dir = gtk_widget_get_direction (GTK_WIDGET (notebook)) == GTK_TEXT_DIR_RTL ? 1 : 0;

  return translate_direction[text_dir][notebook->tab_pos][direction];
}

static gint
get_effective_tab_pos (GtkNotebook *notebook)
{
  if (gtk_widget_get_direction (GTK_WIDGET (notebook)) == GTK_TEXT_DIR_RTL)
    {
      switch (notebook->tab_pos) 
	{
	case GTK_POS_LEFT:
	  return GTK_POS_RIGHT;
	case GTK_POS_RIGHT:
	  return GTK_POS_LEFT;
	default: ;
	}
    }

  return notebook->tab_pos;
}

static gint
get_tab_gap_pos (GtkNotebook *notebook)
{
  gint tab_pos = get_effective_tab_pos (notebook);
  gint gap_side = GTK_POS_BOTTOM;
  
  switch (tab_pos)
    {
    case GTK_POS_TOP:
      gap_side = GTK_POS_BOTTOM;
      break;
    case GTK_POS_BOTTOM:
      gap_side = GTK_POS_TOP;
      break;
    case GTK_POS_LEFT:
      gap_side = GTK_POS_RIGHT;
      break;
    case GTK_POS_RIGHT:
      gap_side = GTK_POS_LEFT;
      break;
    }

  return gap_side;
}

static void
gtk_notebook_move_focus_out (GtkNotebook      *notebook,
			     GtkDirectionType  direction_type)
{
  GtkDirectionType effective_direction = get_effective_direction (notebook, direction_type);
  GtkWidget *toplevel;
  
  if (GTK_CONTAINER (notebook)->focus_child && effective_direction == GTK_DIR_UP)
    if (focus_tabs_in (notebook))
      return;
  if (gtk_widget_is_focus (GTK_WIDGET (notebook)) && effective_direction == GTK_DIR_DOWN)
    if (focus_child_in (notebook, GTK_DIR_TAB_FORWARD))
      return;

  /* At this point, we know we should be focusing out of the notebook entirely. We
   * do this by setting a flag, then propagating the focus motion to the notebook.
   */
  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (notebook));
  if (!gtk_widget_is_toplevel (toplevel))
    return;

  g_object_ref (notebook);
  
  notebook->focus_out = TRUE;
  g_signal_emit_by_name (toplevel, "move-focus", direction_type);
  notebook->focus_out = FALSE;
  
  g_object_unref (notebook);
}

static gint
reorder_tab (GtkNotebook *notebook, GList *position, GList *tab)
{
  GList *elem;

  if (position == tab)
    return g_list_position (notebook->children, tab);

  /* check that we aren't inserting the tab in the
   * same relative position, taking packing into account */
  elem = (position) ? position->prev : g_list_last (notebook->children);

  while (elem && elem != tab && GTK_NOTEBOOK_PAGE (elem)->pack != GTK_NOTEBOOK_PAGE (tab)->pack)
    elem = elem->prev;

  if (elem == tab)
    return g_list_position (notebook->children, tab);

  /* now actually reorder the tab */
  if (notebook->first_tab == tab)
    notebook->first_tab = gtk_notebook_search_page (notebook, notebook->first_tab,
						    STEP_NEXT, TRUE);

  notebook->children = g_list_remove_link (notebook->children, tab);

  if (!position)
    elem = g_list_last (notebook->children);
  else
    {
      elem = position->prev;
      position->prev = tab;
    }

  if (elem)
    elem->next = tab;
  else
    notebook->children = tab;

  tab->prev = elem;
  tab->next = position;

  return g_list_position (notebook->children, tab);
}

static gboolean
gtk_notebook_reorder_tab (GtkNotebook      *notebook,
			  GtkDirectionType  direction_type,
			  gboolean          move_to_last)
{
  GtkDirectionType effective_direction = get_effective_direction (notebook, direction_type);
  GtkNotebookPage *page;
  GList *last, *child;
  gint page_num;

  if (!gtk_widget_is_focus (GTK_WIDGET (notebook)) || !notebook->show_tabs)
    return FALSE;

  if (!notebook->cur_page ||
      !notebook->cur_page->reorderable)
    return FALSE;

  if (effective_direction != GTK_DIR_LEFT &&
      effective_direction != GTK_DIR_RIGHT)
    return FALSE;

  if (move_to_last)
    {
      child = notebook->focus_tab;

      do
	{
	  last = child;
	  child = gtk_notebook_search_page (notebook, last, 
					    (effective_direction == GTK_DIR_RIGHT) ? STEP_NEXT : STEP_PREV,
					    TRUE);
	}
      while (child && GTK_NOTEBOOK_PAGE (last)->pack == GTK_NOTEBOOK_PAGE (child)->pack);

      child = last;
    }
  else
    child = gtk_notebook_search_page (notebook, notebook->focus_tab,
				      (effective_direction == GTK_DIR_RIGHT) ? STEP_NEXT : STEP_PREV,
				      TRUE);

  if (!child || child->data == notebook->cur_page)
    return FALSE;

  page = child->data;

  if (page->pack == notebook->cur_page->pack)
    {
      if (effective_direction == GTK_DIR_RIGHT)
	page_num = reorder_tab (notebook, (page->pack == GTK_PACK_START) ? child->next : child, notebook->focus_tab);
      else
	page_num = reorder_tab (notebook, (page->pack == GTK_PACK_START) ? child : child->next, notebook->focus_tab);

      gtk_notebook_pages_allocate (notebook);

      g_signal_emit (notebook,
		     notebook_signals[PAGE_REORDERED],
		     0,
		     ((GtkNotebookPage *) notebook->focus_tab->data)->child,
		     page_num);

      return TRUE;
    }

  return FALSE;
}

/**
 * gtk_notebook_new:
 * 
 * Creates a new #GtkNotebook widget with no pages.

 * Return value: the newly created #GtkNotebook
 **/
GtkWidget*
gtk_notebook_new (void)
{
  return g_object_new (GTK_TYPE_NOTEBOOK, NULL);
}

/* Private GtkObject Methods :
 * 
 * gtk_notebook_destroy
 * gtk_notebook_set_arg
 * gtk_notebook_get_arg
 */
static void
gtk_notebook_destroy (GtkObject *object)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (object);
  GtkNotebookPrivate *priv = GTK_NOTEBOOK_GET_PRIVATE (notebook);

  if (priv->action_widget[GTK_PACK_START])
    {
      gtk_widget_unparent (priv->action_widget[GTK_PACK_START]);
      priv->action_widget[GTK_PACK_START] = NULL;
    }

  if (priv->action_widget[GTK_PACK_END])
    {
      gtk_widget_unparent (priv->action_widget[GTK_PACK_END]);
      priv->action_widget[GTK_PACK_END] = NULL;
    }

  if (notebook->menu)
    gtk_notebook_popup_disable (notebook);

  if (priv->source_targets)
    {
      gtk_target_list_unref (priv->source_targets);
      priv->source_targets = NULL;
    }

  if (priv->switch_tab_timer)
    {
      g_source_remove (priv->switch_tab_timer);
      priv->switch_tab_timer = 0;
    }

  GTK_OBJECT_CLASS (gtk_notebook_parent_class)->destroy (object);
}

static void
gtk_notebook_set_property (GObject         *object,
			   guint            prop_id,
			   const GValue    *value,
			   GParamSpec      *pspec)
{
  GtkNotebook *notebook;

  notebook = GTK_NOTEBOOK (object);

  switch (prop_id)
    {
    case PROP_SHOW_TABS:
      gtk_notebook_set_show_tabs (notebook, g_value_get_boolean (value));
      break;
    case PROP_SHOW_BORDER:
      gtk_notebook_set_show_border (notebook, g_value_get_boolean (value));
      break;
    case PROP_SCROLLABLE:
      gtk_notebook_set_scrollable (notebook, g_value_get_boolean (value));
      break;
    case PROP_ENABLE_POPUP:
      if (g_value_get_boolean (value))
	gtk_notebook_popup_enable (notebook);
      else
	gtk_notebook_popup_disable (notebook);
      break;
    case PROP_HOMOGENEOUS:
      gtk_notebook_set_homogeneous_tabs_internal (notebook, g_value_get_boolean (value));
      break;  
    case PROP_PAGE:
      gtk_notebook_set_current_page (notebook, g_value_get_int (value));
      break;
    case PROP_TAB_POS:
      gtk_notebook_set_tab_pos (notebook, g_value_get_enum (value));
      break;
    case PROP_TAB_BORDER:
      gtk_notebook_set_tab_border_internal (notebook, g_value_get_uint (value));
      break;
    case PROP_TAB_HBORDER:
      gtk_notebook_set_tab_hborder_internal (notebook, g_value_get_uint (value));
      break;
    case PROP_TAB_VBORDER:
      gtk_notebook_set_tab_vborder_internal (notebook, g_value_get_uint (value));
      break;
    case PROP_GROUP_ID:
      gtk_notebook_set_group_id (notebook, g_value_get_int (value));
      break;
    case PROP_GROUP:
      gtk_notebook_set_group (notebook, g_value_get_pointer (value));
      break;
    case PROP_GROUP_NAME:
      gtk_notebook_set_group_name (notebook, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_notebook_get_property (GObject         *object,
			   guint            prop_id,
			   GValue          *value,
			   GParamSpec      *pspec)
{
  GtkNotebook *notebook;
  GtkNotebookPrivate *priv;

  notebook = GTK_NOTEBOOK (object);
  priv = GTK_NOTEBOOK_GET_PRIVATE (notebook);

  switch (prop_id)
    {
    case PROP_SHOW_TABS:
      g_value_set_boolean (value, notebook->show_tabs);
      break;
    case PROP_SHOW_BORDER:
      g_value_set_boolean (value, notebook->show_border);
      break;
    case PROP_SCROLLABLE:
      g_value_set_boolean (value, notebook->scrollable);
      break;
    case PROP_ENABLE_POPUP:
      g_value_set_boolean (value, notebook->menu != NULL);
      break;
    case PROP_HOMOGENEOUS:
      g_value_set_boolean (value, notebook->homogeneous);
      break;
    case PROP_PAGE:
      g_value_set_int (value, gtk_notebook_get_current_page (notebook));
      break;
    case PROP_TAB_POS:
      g_value_set_enum (value, notebook->tab_pos);
      break;
    case PROP_TAB_HBORDER:
      g_value_set_uint (value, notebook->tab_hborder);
      break;
    case PROP_TAB_VBORDER:
      g_value_set_uint (value, notebook->tab_vborder);
      break;
    case PROP_GROUP_ID:
      g_value_set_int (value, gtk_notebook_get_group_id (notebook));
      break;
    case PROP_GROUP:
      g_value_set_pointer (value, priv->group);
      break;
    case PROP_GROUP_NAME:
      g_value_set_string (value, gtk_notebook_get_group_name (notebook));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/* Private GtkWidget Methods :
 * 
 * gtk_notebook_map
 * gtk_notebook_unmap
 * gtk_notebook_realize
 * gtk_notebook_size_request
 * gtk_notebook_size_allocate
 * gtk_notebook_expose
 * gtk_notebook_scroll
 * gtk_notebook_button_press
 * gtk_notebook_button_release
 * gtk_notebook_popup_menu
 * gtk_notebook_leave_notify
 * gtk_notebook_motion_notify
 * gtk_notebook_focus_in
 * gtk_notebook_focus_out
 * gtk_notebook_draw_focus
 * gtk_notebook_style_set
 * gtk_notebook_drag_begin
 * gtk_notebook_drag_end
 * gtk_notebook_drag_failed
 * gtk_notebook_drag_motion
 * gtk_notebook_drag_drop
 * gtk_notebook_drag_data_get
 * gtk_notebook_drag_data_received
 */
static gboolean
gtk_notebook_get_event_window_position (GtkNotebook  *notebook,
					GdkRectangle *rectangle)
{
  GtkNotebookPrivate *priv = GTK_NOTEBOOK_GET_PRIVATE (notebook);
  GtkWidget *widget = GTK_WIDGET (notebook);
  gint border_width = GTK_CONTAINER (notebook)->border_width;
  GtkNotebookPage *visible_page = NULL;
  GList *tmp_list;
  gint tab_pos = get_effective_tab_pos (notebook);
  gboolean is_rtl;
  gint i;

  for (tmp_list = notebook->children; tmp_list; tmp_list = tmp_list->next)
    {
      GtkNotebookPage *page = tmp_list->data;
      if (gtk_widget_get_visible (page->child))
	{
	  visible_page = page;
	  break;
	}
    }

  if (notebook->show_tabs && visible_page)
    {
      if (rectangle)
	{
	  is_rtl = gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;
	  rectangle->x = widget->allocation.x + border_width;
	  rectangle->y = widget->allocation.y + border_width;

	  switch (tab_pos)
	    {
	    case GTK_POS_TOP:
	    case GTK_POS_BOTTOM:
	      rectangle->width = widget->allocation.width - 2 * border_width;
	      rectangle->height = visible_page->requisition.height;
	      if (tab_pos == GTK_POS_BOTTOM)
		rectangle->y += widget->allocation.height - 2 * border_width - rectangle->height;

              for (i = 0; i < N_ACTION_WIDGETS; i++)
                {
                  if (priv->action_widget[i] &&
                      gtk_widget_get_visible (priv->action_widget[i]))
                    {
                      rectangle->width -= priv->action_widget[i]->allocation.width;
                      if ((!is_rtl && i == ACTION_WIDGET_START) ||
                          (is_rtl && i == ACTION_WIDGET_END))
                        rectangle->x += priv->action_widget[i]->allocation.width;
                    }
                }
	      break;
	    case GTK_POS_LEFT:
	    case GTK_POS_RIGHT:
	      rectangle->width = visible_page->requisition.width;
	      rectangle->height = widget->allocation.height - 2 * border_width;
	      if (tab_pos == GTK_POS_RIGHT)
		rectangle->x += widget->allocation.width - 2 * border_width - rectangle->width;

              for (i = 0; i < N_ACTION_WIDGETS; i++)
                {
                  if (priv->action_widget[i] &&
                      gtk_widget_get_visible (priv->action_widget[i]))
                    {
                      rectangle->height -= priv->action_widget[i]->allocation.height;

                      if (i == ACTION_WIDGET_START)
                        rectangle->y += priv->action_widget[i]->allocation.height;
                    }
                }
              break;
	    }
	}

      return TRUE;
    }
  else
    {
      if (rectangle)
	{
	  rectangle->x = rectangle->y = 0;
	  rectangle->width = rectangle->height = 10;
	}
    }

  return FALSE;
}

static void
gtk_notebook_map (GtkWidget *widget)
{
  GtkNotebookPrivate *priv;
  GtkNotebook *notebook;
  GtkNotebookPage *page;
  GList *children;
  gint i;

  gtk_widget_set_mapped (widget, TRUE);

  notebook = GTK_NOTEBOOK (widget);
  priv = GTK_NOTEBOOK_GET_PRIVATE (notebook);

  if (notebook->cur_page &&
      gtk_widget_get_visible (notebook->cur_page->child) &&
      !gtk_widget_get_mapped (notebook->cur_page->child))
    gtk_widget_map (notebook->cur_page->child);

  for (i = 0; i < N_ACTION_WIDGETS; i++)
    {
      if (priv->action_widget[i] &&
          gtk_widget_get_visible (priv->action_widget[i]) &&
          GTK_WIDGET_CHILD_VISIBLE (priv->action_widget[i]) &&
          !gtk_widget_get_mapped (priv->action_widget[i]))
        gtk_widget_map (priv->action_widget[i]);
    }

  if (notebook->scrollable)
    gtk_notebook_pages_allocate (notebook);
  else
    {
      children = notebook->children;

      while (children)
	{
	  page = children->data;
	  children = children->next;

	  if (page->tab_label &&
	      gtk_widget_get_visible (page->tab_label) &&
	      !gtk_widget_get_mapped (page->tab_label))
	    gtk_widget_map (page->tab_label);
	}
    }

  if (gtk_notebook_get_event_window_position (notebook, NULL))
    gdk_window_show_unraised (notebook->event_window);
}

static void
gtk_notebook_unmap (GtkWidget *widget)
{
  stop_scrolling (GTK_NOTEBOOK (widget));
  
  gtk_widget_set_mapped (widget, FALSE);

  gdk_window_hide (GTK_NOTEBOOK (widget)->event_window);

  GTK_WIDGET_CLASS (gtk_notebook_parent_class)->unmap (widget);
}

static void
gtk_notebook_realize (GtkWidget *widget)
{
  GtkNotebook *notebook;
  GdkWindowAttr attributes;
  gint attributes_mask;
  GdkRectangle event_window_pos;

  notebook = GTK_NOTEBOOK (widget);

  gtk_widget_set_realized (widget, TRUE);

  gtk_notebook_get_event_window_position (notebook, &event_window_pos);
  
  widget->window = gtk_widget_get_parent_window (widget);
  g_object_ref (widget->window);
  
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = event_window_pos.x;
  attributes.y = event_window_pos.y;
  attributes.width = event_window_pos.width;
  attributes.height = event_window_pos.height;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_BUTTON_PRESS_MASK |
			    GDK_BUTTON_RELEASE_MASK | GDK_KEY_PRESS_MASK |
			    GDK_POINTER_MOTION_MASK | GDK_LEAVE_NOTIFY_MASK |
			    GDK_SCROLL_MASK);
  attributes_mask = GDK_WA_X | GDK_WA_Y;

  notebook->event_window = gdk_window_new (gtk_widget_get_parent_window (widget), 
					   &attributes, attributes_mask);
  gdk_window_set_user_data (notebook->event_window, notebook);

  widget->style = gtk_style_attach (widget->style, widget->window);
}

static void
gtk_notebook_unrealize (GtkWidget *widget)
{
  GtkNotebook *notebook;
  GtkNotebookPrivate *priv;

  notebook = GTK_NOTEBOOK (widget);
  priv = GTK_NOTEBOOK_GET_PRIVATE (widget);

  gdk_window_set_user_data (notebook->event_window, NULL);
  gdk_window_destroy (notebook->event_window);
  notebook->event_window = NULL;

  if (priv->drag_window)
    {
      gdk_window_set_user_data (priv->drag_window, NULL);
      gdk_window_destroy (priv->drag_window);
      priv->drag_window = NULL;
    }

  GTK_WIDGET_CLASS (gtk_notebook_parent_class)->unrealize (widget);
}

static void
gtk_notebook_size_request (GtkWidget      *widget,
			   GtkRequisition *requisition)
{
  GtkNotebookPrivate *priv = GTK_NOTEBOOK_GET_PRIVATE (widget);
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  GtkNotebookPage *page;
  GList *children;
  GtkRequisition child_requisition;
  GtkRequisition action_widget_requisition[2] = { { 0 }, { 0 } };
  gboolean switch_page = FALSE;
  gint vis_pages;
  gint focus_width;
  gint tab_overlap;
  gint tab_curvature;
  gint arrow_spacing;
  gint scroll_arrow_hlength;
  gint scroll_arrow_vlength;

  gtk_widget_style_get (widget,
                        "focus-line-width", &focus_width,
			"tab-overlap", &tab_overlap,
			"tab-curvature", &tab_curvature,
                        "arrow-spacing", &arrow_spacing,
                        "scroll-arrow-hlength", &scroll_arrow_hlength,
                        "scroll-arrow-vlength", &scroll_arrow_vlength,
			NULL);

  widget->requisition.width = 0;
  widget->requisition.height = 0;

  for (children = notebook->children, vis_pages = 0; children;
       children = children->next)
    {
      page = children->data;

      if (gtk_widget_get_visible (page->child))
	{
	  vis_pages++;
	  gtk_widget_size_request (page->child, &child_requisition);
	  
	  widget->requisition.width = MAX (widget->requisition.width,
					   child_requisition.width);
	  widget->requisition.height = MAX (widget->requisition.height,
					    child_requisition.height);

	  if (notebook->menu && page->menu_label->parent &&
	      !gtk_widget_get_visible (page->menu_label->parent))
	    gtk_widget_show (page->menu_label->parent);
	}
      else
	{
	  if (page == notebook->cur_page)
	    switch_page = TRUE;
	  if (notebook->menu && page->menu_label->parent &&
	      gtk_widget_get_visible (page->menu_label->parent))
	    gtk_widget_hide (page->menu_label->parent);
	}
    }

  if (notebook->show_border || notebook->show_tabs)
    {
      widget->requisition.width += widget->style->xthickness * 2;
      widget->requisition.height += widget->style->ythickness * 2;

      if (notebook->show_tabs)
	{
	  gint tab_width = 0;
	  gint tab_height = 0;
	  gint tab_max = 0;
	  gint padding;
          gint i;
          gint action_width = 0;
          gint action_height = 0;
	  
	  for (children = notebook->children; children;
	       children = children->next)
	    {
	      page = children->data;
	      
	      if (gtk_widget_get_visible (page->child))
		{
		  if (!gtk_widget_get_visible (page->tab_label))
		    gtk_widget_show (page->tab_label);

		  gtk_widget_size_request (page->tab_label,
					   &child_requisition);

		  page->requisition.width = 
		    child_requisition.width +
		    2 * widget->style->xthickness;
		  page->requisition.height = 
		    child_requisition.height +
		    2 * widget->style->ythickness;
		  
		  switch (notebook->tab_pos)
		    {
		    case GTK_POS_TOP:
		    case GTK_POS_BOTTOM:
		      page->requisition.height += 2 * (notebook->tab_vborder +
						       focus_width);
		      tab_height = MAX (tab_height, page->requisition.height);
		      tab_max = MAX (tab_max, page->requisition.width);
		      break;
		    case GTK_POS_LEFT:
		    case GTK_POS_RIGHT:
		      page->requisition.width += 2 * (notebook->tab_hborder +
						      focus_width);
		      tab_width = MAX (tab_width, page->requisition.width);
		      tab_max = MAX (tab_max, page->requisition.height);
		      break;
		    }
		}
	      else if (gtk_widget_get_visible (page->tab_label))
		gtk_widget_hide (page->tab_label);
	    }

	  children = notebook->children;

	  if (vis_pages)
	    {
              for (i = 0; i < N_ACTION_WIDGETS; i++)
                {
                  if (priv->action_widget[i])
                    {
                      gtk_widget_size_request (priv->action_widget[i], &action_widget_requisition[i]);
                      action_widget_requisition[i].width += widget->style->xthickness;
                      action_widget_requisition[i].height += widget->style->ythickness;
                    }
                }

	      switch (notebook->tab_pos)
		{
		case GTK_POS_TOP:
		case GTK_POS_BOTTOM:
		  if (tab_height == 0)
		    break;

		  if (notebook->scrollable && vis_pages > 1 && 
		      widget->requisition.width < tab_width)
		    tab_height = MAX (tab_height, scroll_arrow_hlength);

                  tab_height = MAX (tab_height, action_widget_requisition[ACTION_WIDGET_START].height);
                  tab_height = MAX (tab_height, action_widget_requisition[ACTION_WIDGET_END].height);

		  padding = 2 * (tab_curvature + focus_width +
				 notebook->tab_hborder) - tab_overlap;
		  tab_max += padding;
		  while (children)
		    {
		      page = children->data;
		      children = children->next;
		  
		      if (!gtk_widget_get_visible (page->child))
			continue;

		      if (notebook->homogeneous)
			page->requisition.width = tab_max;
		      else
			page->requisition.width += padding;

		      tab_width += page->requisition.width;
		      page->requisition.height = tab_height;
		    }

		  if (notebook->scrollable && vis_pages > 1 &&
		      widget->requisition.width < tab_width)
		    tab_width = tab_max + 2 * (scroll_arrow_hlength + arrow_spacing);

		  action_width += action_widget_requisition[ACTION_WIDGET_START].width;
		  action_width += action_widget_requisition[ACTION_WIDGET_END].width;
                  if (notebook->homogeneous && !notebook->scrollable)
                    widget->requisition.width = MAX (widget->requisition.width,
                                                     vis_pages * tab_max +
                                                     tab_overlap + action_width);
                  else
                    widget->requisition.width = MAX (widget->requisition.width,
                                                     tab_width + tab_overlap + action_width);

		  widget->requisition.height += tab_height;
		  break;
		case GTK_POS_LEFT:
		case GTK_POS_RIGHT:
		  if (tab_width == 0)
		    break;

		  if (notebook->scrollable && vis_pages > 1 && 
		      widget->requisition.height < tab_height)
		    tab_width = MAX (tab_width,
                                     arrow_spacing + 2 * scroll_arrow_vlength);

		  tab_width = MAX (tab_width, action_widget_requisition[ACTION_WIDGET_START].width);
		  tab_width = MAX (tab_width, action_widget_requisition[ACTION_WIDGET_END].width);

		  padding = 2 * (tab_curvature + focus_width +
				 notebook->tab_vborder) - tab_overlap;
		  tab_max += padding;

		  while (children)
		    {
		      page = children->data;
		      children = children->next;

		      if (!gtk_widget_get_visible (page->child))
			continue;

		      page->requisition.width = tab_width;

		      if (notebook->homogeneous)
			page->requisition.height = tab_max;
		      else
			page->requisition.height += padding;

		      tab_height += page->requisition.height;
		    }

		  if (notebook->scrollable && vis_pages > 1 && 
		      widget->requisition.height < tab_height)
		    tab_height = tab_max + (2 * scroll_arrow_vlength + arrow_spacing);
		  action_height += action_widget_requisition[ACTION_WIDGET_START].height;
		  action_height += action_widget_requisition[ACTION_WIDGET_END].height;

                  if (notebook->homogeneous && !notebook->scrollable)
                    widget->requisition.height =
		      MAX (widget->requisition.height,
			   vis_pages * tab_max + tab_overlap + action_height);
                  else
                    widget->requisition.height =
		      MAX (widget->requisition.height,
			   tab_height + tab_overlap + action_height);

		  if (!notebook->homogeneous || notebook->scrollable)
		    vis_pages = 1;
		  widget->requisition.height = MAX (widget->requisition.height,
						    vis_pages * tab_max +
						    tab_overlap);

		  widget->requisition.width += tab_width;
		  break;
		}
	    }
	}
      else
	{
	  for (children = notebook->children; children;
	       children = children->next)
	    {
	      page = children->data;
	      
	      if (page->tab_label && gtk_widget_get_visible (page->tab_label))
		gtk_widget_hide (page->tab_label);
	    }
	}
    }

  widget->requisition.width += GTK_CONTAINER (widget)->border_width * 2;
  widget->requisition.height += GTK_CONTAINER (widget)->border_width * 2;

  if (switch_page)
    {
      if (vis_pages)
	{
	  for (children = notebook->children; children;
	       children = children->next)
	    {
	      page = children->data;
	      if (gtk_widget_get_visible (page->child))
		{
		  gtk_notebook_switch_page (notebook, page);
		  break;
		}
	    }
	}
      else if (gtk_widget_get_visible (widget))
	{
	  widget->requisition.width = GTK_CONTAINER (widget)->border_width * 2;
	  widget->requisition.height= GTK_CONTAINER (widget)->border_width * 2;
	}
    }
  if (vis_pages && !notebook->cur_page)
    {
      children = gtk_notebook_search_page (notebook, NULL, STEP_NEXT, TRUE);
      if (children)
	{
	  notebook->first_tab = children;
	  gtk_notebook_switch_page (notebook, GTK_NOTEBOOK_PAGE (children));
	}
    }
}

static void
gtk_notebook_size_allocate (GtkWidget     *widget,
			    GtkAllocation *allocation)
{
  GtkNotebookPrivate *priv = GTK_NOTEBOOK_GET_PRIVATE (widget);
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  gint tab_pos = get_effective_tab_pos (notebook);
  gboolean is_rtl;
  gint focus_width;

  gtk_widget_style_get (widget, "focus-line-width", &focus_width, NULL);
  
  widget->allocation = *allocation;
  if (gtk_widget_get_realized (widget))
    {
      GdkRectangle position;

      if (gtk_notebook_get_event_window_position (notebook, &position))
	{
	  gdk_window_move_resize (notebook->event_window,
				  position.x, position.y,
				  position.width, position.height);
	  if (gtk_widget_get_mapped (GTK_WIDGET (notebook)))
	    gdk_window_show_unraised (notebook->event_window);
	}
      else
	gdk_window_hide (notebook->event_window);
    }

  if (notebook->children)
    {
      gint border_width = GTK_CONTAINER (widget)->border_width;
      GtkNotebookPage *page;
      GtkAllocation child_allocation;
      GList *children;
      gint i;
      
      child_allocation.x = widget->allocation.x + border_width;
      child_allocation.y = widget->allocation.y + border_width;
      child_allocation.width = MAX (1, allocation->width - border_width * 2);
      child_allocation.height = MAX (1, allocation->height - border_width * 2);

      if (notebook->show_tabs || notebook->show_border)
	{
	  child_allocation.x += widget->style->xthickness;
	  child_allocation.y += widget->style->ythickness;
	  child_allocation.width = MAX (1, child_allocation.width -
					widget->style->xthickness * 2);
	  child_allocation.height = MAX (1, child_allocation.height -
					 widget->style->ythickness * 2);

	  if (notebook->show_tabs && notebook->children && notebook->cur_page)
	    {
	      switch (tab_pos)
		{
		case GTK_POS_TOP:
		  child_allocation.y += notebook->cur_page->requisition.height;
		case GTK_POS_BOTTOM:
		  child_allocation.height =
		    MAX (1, child_allocation.height -
			 notebook->cur_page->requisition.height);
		  break;
		case GTK_POS_LEFT:
		  child_allocation.x += notebook->cur_page->requisition.width;
		case GTK_POS_RIGHT:
		  child_allocation.width =
		    MAX (1, child_allocation.width -
			 notebook->cur_page->requisition.width);
		  break;
		}

              for (i = 0; i < N_ACTION_WIDGETS; i++)
                {
                  GtkAllocation widget_allocation;

                  if (!priv->action_widget[i])
                    continue;

		  widget_allocation.x = widget->allocation.x + border_width;
		  widget_allocation.y = widget->allocation.y + border_width;
		  is_rtl = gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;

		  switch (tab_pos)
		    {
		    case GTK_POS_BOTTOM:
		      widget_allocation.y +=
			widget->allocation.height - 2 * border_width - notebook->cur_page->requisition.height;
		      /* fall through */
		    case GTK_POS_TOP:
		      widget_allocation.width = priv->action_widget[i]->requisition.width;
		      widget_allocation.height = notebook->cur_page->requisition.height - widget->style->ythickness;

		      if ((i == ACTION_WIDGET_START && is_rtl) ||
                          (i == ACTION_WIDGET_END && !is_rtl))
			widget_allocation.x +=
			  widget->allocation.width - 2 * border_width -
			  priv->action_widget[i]->requisition.width;
                      if (tab_pos == GTK_POS_TOP) /* no fall through */
                          widget_allocation.y += 2 * focus_width;
		      break;
		    case GTK_POS_RIGHT:
		      widget_allocation.x +=
			widget->allocation.width - 2 * border_width - notebook->cur_page->requisition.width;
		      /* fall through */
		    case GTK_POS_LEFT:
		      widget_allocation.height = priv->action_widget[i]->requisition.height;
		      widget_allocation.width = notebook->cur_page->requisition.width - widget->style->xthickness;

                      if (i == ACTION_WIDGET_END)
                        widget_allocation.y +=
                          widget->allocation.height - 2 * border_width -
                          priv->action_widget[i]->requisition.height;
                      if (tab_pos == GTK_POS_LEFT) /* no fall through */  
                        widget_allocation.x += 2 * focus_width;
		      break;
		    }

		  gtk_widget_size_allocate (priv->action_widget[i], &widget_allocation);
		}
	    }
	}

      children = notebook->children;
      while (children)
	{
	  page = children->data;
	  children = children->next;
	  
	  if (gtk_widget_get_visible (page->child))
	    gtk_widget_size_allocate (page->child, &child_allocation);
	}

      gtk_notebook_pages_allocate (notebook);
    }
}

static gint
gtk_notebook_expose (GtkWidget      *widget,
		     GdkEventExpose *event)
{
  GtkNotebook *notebook;
  GtkNotebookPrivate *priv;
  gint i;

  notebook = GTK_NOTEBOOK (widget);
  priv = GTK_NOTEBOOK_GET_PRIVATE (widget);

  if (event->window == priv->drag_window)
    {
      GdkRectangle area = { 0, };
      cairo_t *cr;

      /* FIXME: This is a workaround to make tabs reordering work better
       * with engines with rounded tabs. If the drag window background
       * isn't set, the rounded corners would be black.
       *
       * Ideally, these corners should be made transparent, Either by using
       * ARGB visuals or shape windows.
       */
      cr = gdk_cairo_create (priv->drag_window);
      gdk_cairo_set_source_color (cr, &widget->style->bg [GTK_STATE_NORMAL]);
      cairo_paint (cr);
      cairo_destroy (cr);

      area.width = gdk_window_get_width (priv->drag_window);
      area.height = gdk_window_get_height (priv->drag_window);
      gtk_notebook_draw_tab (notebook,
			     notebook->cur_page,
			     &area);
      gtk_notebook_draw_focus (widget, event);
      gtk_container_propagate_expose (GTK_CONTAINER (notebook),
				      notebook->cur_page->tab_label, event);
    }
  else if (gtk_widget_is_drawable (widget))
    {
      gtk_notebook_paint (widget, &event->area);
      if (notebook->show_tabs)
	{
	  GtkNotebookPage *page;
	  GList *pages;

	  gtk_notebook_draw_focus (widget, event);
	  pages = notebook->children;

	  while (pages)
	    {
	      page = GTK_NOTEBOOK_PAGE (pages);
	      pages = pages->next;

	      if (page->tab_label->window == event->window &&
		  gtk_widget_is_drawable (page->tab_label))
		gtk_container_propagate_expose (GTK_CONTAINER (notebook),
						page->tab_label, event);
	    }
	}

      if (notebook->cur_page)
	gtk_container_propagate_expose (GTK_CONTAINER (notebook),
					notebook->cur_page->child,
					event);
      if (notebook->show_tabs)
      {
        for (i = 0; i < N_ACTION_WIDGETS; i++)
        {
          if (priv->action_widget[i] &&
              gtk_widget_is_drawable (priv->action_widget[i]))
            gtk_container_propagate_expose (GTK_CONTAINER (notebook),
                                            priv->action_widget[i], event);
        }
      }
    }

  return FALSE;
}

static gboolean
gtk_notebook_show_arrows (GtkNotebook *notebook)
{
  gboolean show_arrow = FALSE;
  GList *children;
  
  if (!notebook->scrollable)
    return FALSE;

  children = notebook->children;
  while (children)
    {
      GtkNotebookPage *page = children->data;

      if (page->tab_label && !gtk_widget_get_child_visible (page->tab_label))
	show_arrow = TRUE;

      children = children->next;
    }

  return show_arrow;
}

static void
gtk_notebook_get_arrow_rect (GtkNotebook     *notebook,
			     GdkRectangle    *rectangle,
			     GtkNotebookArrow arrow)
{
  GdkRectangle event_window_pos;
  gboolean before = ARROW_IS_BEFORE (arrow);
  gboolean left = ARROW_IS_LEFT (arrow);

  if (gtk_notebook_get_event_window_position (notebook, &event_window_pos))
    {
      gint scroll_arrow_hlength;
      gint scroll_arrow_vlength;

      gtk_widget_style_get (GTK_WIDGET (notebook),
                            "scroll-arrow-hlength", &scroll_arrow_hlength,
                            "scroll-arrow-vlength", &scroll_arrow_vlength,
                            NULL);

      switch (notebook->tab_pos)
	{
	case GTK_POS_LEFT:
	case GTK_POS_RIGHT:
          rectangle->width = scroll_arrow_vlength;
          rectangle->height = scroll_arrow_vlength;

	  if ((before && (notebook->has_before_previous != notebook->has_before_next)) ||
	      (!before && (notebook->has_after_previous != notebook->has_after_next))) 
	  rectangle->x = event_window_pos.x + (event_window_pos.width - rectangle->width) / 2;
	  else if (left)
	    rectangle->x = event_window_pos.x + event_window_pos.width / 2 - rectangle->width;
	  else 
	    rectangle->x = event_window_pos.x + event_window_pos.width / 2;
	  rectangle->y = event_window_pos.y;
	  if (!before)
	    rectangle->y += event_window_pos.height - rectangle->height;
	  break;

	case GTK_POS_TOP:
	case GTK_POS_BOTTOM:
          rectangle->width = scroll_arrow_hlength;
          rectangle->height = scroll_arrow_hlength;

	  if (before)
	    {
	      if (left || !notebook->has_before_previous)
		rectangle->x = event_window_pos.x;
	      else
		rectangle->x = event_window_pos.x + rectangle->width;
	    }
	  else
	    {
	      if (!left || !notebook->has_after_next)
		rectangle->x = event_window_pos.x + event_window_pos.width - rectangle->width;
	      else
		rectangle->x = event_window_pos.x + event_window_pos.width - 2 * rectangle->width;
	    }
	  rectangle->y = event_window_pos.y + (event_window_pos.height - rectangle->height) / 2;
	  break;
	}
    }
}

static GtkNotebookArrow
gtk_notebook_get_arrow (GtkNotebook *notebook,
			gint         x,
			gint         y)
{
  GdkRectangle arrow_rect;
  GdkRectangle event_window_pos;
  gint i;
  gint x0, y0;
  GtkNotebookArrow arrow[4];

  arrow[0] = notebook->has_before_previous ? ARROW_LEFT_BEFORE : ARROW_NONE;
  arrow[1] = notebook->has_before_next ? ARROW_RIGHT_BEFORE : ARROW_NONE;
  arrow[2] = notebook->has_after_previous ? ARROW_LEFT_AFTER : ARROW_NONE;
  arrow[3] = notebook->has_after_next ? ARROW_RIGHT_AFTER : ARROW_NONE;

  if (gtk_notebook_show_arrows (notebook))
    {
      gtk_notebook_get_event_window_position (notebook, &event_window_pos);
      for (i = 0; i < 4; i++) 
	{ 
	  if (arrow[i] == ARROW_NONE)
	    continue;
      
	  gtk_notebook_get_arrow_rect (notebook, &arrow_rect, arrow[i]);
      
	  x0 = x - arrow_rect.x;
	  y0 = y - arrow_rect.y;

	  if (y0 >= 0 && y0 < arrow_rect.height &&
	      x0 >= 0 && x0 < arrow_rect.width)
	    return arrow[i];
	}
    }

  return ARROW_NONE;
}

static void
gtk_notebook_do_arrow (GtkNotebook     *notebook,
		       GtkNotebookArrow arrow)
{
  GtkWidget *widget = GTK_WIDGET (notebook);
  gboolean is_rtl, left;

  is_rtl = gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;
  left = (ARROW_IS_LEFT (arrow) && !is_rtl) || 
         (!ARROW_IS_LEFT (arrow) && is_rtl);

  if (!notebook->focus_tab ||
      gtk_notebook_search_page (notebook, notebook->focus_tab,
				left ? STEP_PREV : STEP_NEXT,
				TRUE))
    {
      gtk_notebook_change_current_page (notebook, left ? -1 : 1);
      gtk_widget_grab_focus (widget);
    }
}

static gboolean
gtk_notebook_arrow_button_press (GtkNotebook      *notebook,
				 GtkNotebookArrow  arrow,
				 gint              button)
{
  GtkWidget *widget = GTK_WIDGET (notebook);
  gboolean is_rtl = gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;
  gboolean left = (ARROW_IS_LEFT (arrow) && !is_rtl) || 
                  (!ARROW_IS_LEFT (arrow) && is_rtl);

  if (!gtk_widget_has_focus (widget))
    gtk_widget_grab_focus (widget);
  
  notebook->button = button;
  notebook->click_child = arrow;

  if (button == 1)
    {
      gtk_notebook_do_arrow (notebook, arrow);
      gtk_notebook_set_scroll_timer (notebook);
    }
  else if (button == 2)
    gtk_notebook_page_select (notebook, TRUE);
  else if (button == 3)
    gtk_notebook_switch_focus_tab (notebook,
				   gtk_notebook_search_page (notebook,
							     NULL,
							     left ? STEP_NEXT : STEP_PREV,
							     TRUE));
  gtk_notebook_redraw_arrows (notebook);

  return TRUE;
}

static gboolean
get_widget_coordinates (GtkWidget *widget,
			GdkEvent  *event,
			gint      *x,
			gint      *y)
{
  GdkWindow *window = ((GdkEventAny *)event)->window;
  gdouble tx, ty;

  if (!gdk_event_get_coords (event, &tx, &ty))
    return FALSE;

  while (window && window != widget->window)
    {
      gint window_x, window_y;

      gdk_window_get_position (window, &window_x, &window_y);
      tx += window_x;
      ty += window_y;

      window = gdk_window_get_parent (window);
    }

  if (window)
    {
      *x = tx;
      *y = ty;

      return TRUE;
    }
  else
    return FALSE;
}

static gboolean
gtk_notebook_scroll (GtkWidget      *widget,
                     GdkEventScroll *event)
{
  GtkNotebookPrivate *priv = GTK_NOTEBOOK_GET_PRIVATE (widget);
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  GtkWidget *child, *event_widget;
  gint i;

  if (!notebook->cur_page)
    return FALSE;

  child = notebook->cur_page->child;
  event_widget = gtk_get_event_widget ((GdkEvent *)event);

  /* ignore scroll events from the content of the page */
  if (!event_widget || gtk_widget_is_ancestor (event_widget, child) || event_widget == child)
    return FALSE;

  /* nor from the action area */
  for (i = 0; i < 2; i++)
    {
      if (event_widget == priv->action_widget[i] ||
          (priv->action_widget[i] &&
           gtk_widget_is_ancestor (event_widget, priv->action_widget[i])))
        return FALSE;
    }

  switch (event->direction)
    {
    case GDK_SCROLL_RIGHT:
    case GDK_SCROLL_DOWN:
      gtk_notebook_next_page (notebook);
      break;
    case GDK_SCROLL_LEFT:
    case GDK_SCROLL_UP:
      gtk_notebook_prev_page (notebook);
      break;
    }

  return TRUE;
}

static GList*
get_tab_at_pos (GtkNotebook *notebook, gint x, gint y)
{
  GtkNotebookPage *page;
  GList *children = notebook->children;

  while (children)
    {
      page = children->data;
      
      if (gtk_widget_get_visible (page->child) &&
	  page->tab_label && gtk_widget_get_mapped (page->tab_label) &&
	  (x >= page->allocation.x) &&
	  (y >= page->allocation.y) &&
	  (x <= (page->allocation.x + page->allocation.width)) &&
	  (y <= (page->allocation.y + page->allocation.height)))
	return children;

      children = children->next;
    }

  return NULL;
}

static gboolean
gtk_notebook_button_press (GtkWidget      *widget,
			   GdkEventButton *event)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  GtkNotebookPrivate *priv = GTK_NOTEBOOK_GET_PRIVATE (notebook);
  GtkNotebookPage *page;
  GList *tab;
  GtkNotebookArrow arrow;
  gint x, y;

  if (event->type != GDK_BUTTON_PRESS || !notebook->children ||
      notebook->button)
    return FALSE;

  if (!get_widget_coordinates (widget, (GdkEvent *)event, &x, &y))
    return FALSE;

  arrow = gtk_notebook_get_arrow (notebook, x, y);
  if (arrow)
    return gtk_notebook_arrow_button_press (notebook, arrow, event->button);

  if (notebook->menu && _gtk_button_event_triggers_context_menu (event))
    {
      gtk_menu_popup (GTK_MENU (notebook->menu), NULL, NULL, 
		      NULL, NULL, 3, event->time);
      return TRUE;
    }

  if (event->button != 1)
    return FALSE;

  notebook->button = event->button;

  if ((tab = get_tab_at_pos (notebook, x, y)) != NULL)
    {
      gboolean page_changed, was_focus;

      page = tab->data;
      page_changed = page != notebook->cur_page;
      was_focus = gtk_widget_is_focus (widget);

      gtk_notebook_switch_focus_tab (notebook, tab);
      gtk_widget_grab_focus (widget);

      if (page_changed && !was_focus)
	gtk_widget_child_focus (page->child, GTK_DIR_TAB_FORWARD);

      /* save press to possibly begin a drag */
      if (page->reorderable || page->detachable)
	{
	  priv->during_detach = FALSE;
	  priv->during_reorder = FALSE;
	  priv->pressed_button = event->button;

	  priv->mouse_x = x;
	  priv->mouse_y = y;

	  priv->drag_begin_x = priv->mouse_x;
	  priv->drag_begin_y = priv->mouse_y;
	  priv->drag_offset_x = priv->drag_begin_x - page->allocation.x;
	  priv->drag_offset_y = priv->drag_begin_y - page->allocation.y;
	}
    }

  return TRUE;
}

static void
popup_position_func (GtkMenu  *menu,
                     gint     *x,
                     gint     *y,
                     gboolean *push_in,
                     gpointer  data)
{
  GtkNotebook *notebook = data;
  GtkWidget *w;
  GtkRequisition requisition;

  if (notebook->focus_tab)
    {
      GtkNotebookPage *page;

      page = notebook->focus_tab->data;
      w = page->tab_label;
    }
  else
   {
     w = GTK_WIDGET (notebook);
   }

  gdk_window_get_origin (w->window, x, y);
  gtk_widget_size_request (GTK_WIDGET (menu), &requisition);

  if (gtk_widget_get_direction (w) == GTK_TEXT_DIR_RTL)
    *x += w->allocation.x + w->allocation.width - requisition.width;
  else
    *x += w->allocation.x;

  *y += w->allocation.y + w->allocation.height;

  *push_in = FALSE;
}

static gboolean
gtk_notebook_popup_menu (GtkWidget *widget)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);

  if (notebook->menu)
    {
      gtk_menu_popup (GTK_MENU (notebook->menu), NULL, NULL, 
		      popup_position_func, notebook,
		      0, gtk_get_current_event_time ());
      gtk_menu_shell_select_first (GTK_MENU_SHELL (notebook->menu), FALSE);
      return TRUE;
    }

  return FALSE;
}

static void 
stop_scrolling (GtkNotebook *notebook)
{
  if (notebook->timer)
    {
      g_source_remove (notebook->timer);
      notebook->timer = 0;
      notebook->need_timer = FALSE;
    }
  notebook->click_child = 0;
  notebook->button = 0;
  gtk_notebook_redraw_arrows (notebook);
}

static GList*
get_drop_position (GtkNotebook *notebook,
		   guint        pack)
{
  GtkNotebookPrivate *priv;
  GList *children, *last_child;
  GtkNotebookPage *page;
  gboolean is_rtl;
  gint x, y;

  priv = GTK_NOTEBOOK_GET_PRIVATE (notebook);
  x = priv->mouse_x;
  y = priv->mouse_y;

  is_rtl = gtk_widget_get_direction ((GtkWidget *) notebook) == GTK_TEXT_DIR_RTL;
  children = notebook->children;
  last_child = NULL;

  while (children)
    {
      page = children->data;

      if ((priv->operation != DRAG_OPERATION_REORDER || page != notebook->cur_page) &&
	  gtk_widget_get_visible (page->child) &&
	  page->tab_label &&
	  gtk_widget_get_mapped (page->tab_label) &&
	  page->pack == pack)
	{
	  switch (notebook->tab_pos)
	    {
	    case GTK_POS_TOP:
	    case GTK_POS_BOTTOM:
	      if (!is_rtl)
		{
		  if ((page->pack == GTK_PACK_START && PAGE_MIDDLE_X (page) > x) ||
		      (page->pack == GTK_PACK_END && PAGE_MIDDLE_X (page) < x))
		    return children;
		}
	      else
		{
		  if ((page->pack == GTK_PACK_START && PAGE_MIDDLE_X (page) < x) ||
		      (page->pack == GTK_PACK_END && PAGE_MIDDLE_X (page) > x))
		    return children;
		}

	      break;
	    case GTK_POS_LEFT:
	    case GTK_POS_RIGHT:
	      if ((page->pack == GTK_PACK_START && PAGE_MIDDLE_Y (page) > y) ||
		  (page->pack == GTK_PACK_END && PAGE_MIDDLE_Y (page) < y))
		return children;

	      break;
	    }

	  last_child = children->next;
	}

      children = children->next;
    }

  return last_child;
}

static void
show_drag_window (GtkNotebook        *notebook,
		  GtkNotebookPrivate *priv,
		  GtkNotebookPage    *page)
{
  GtkWidget *widget = GTK_WIDGET (notebook);

  if (!priv->drag_window)
    {
      GdkWindowAttr attributes;
      guint attributes_mask;

      attributes.x = page->allocation.x;
      attributes.y = page->allocation.y;
      attributes.width = page->allocation.width;
      attributes.height = page->allocation.height;
      attributes.window_type = GDK_WINDOW_CHILD;
      attributes.wclass = GDK_INPUT_OUTPUT;
      attributes.visual = gtk_widget_get_visual (widget);
      attributes.colormap = gtk_widget_get_colormap (widget);
      attributes.event_mask = GDK_VISIBILITY_NOTIFY_MASK | GDK_EXPOSURE_MASK | GDK_POINTER_MOTION_MASK;
      attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

      priv->drag_window = gdk_window_new (gtk_widget_get_parent_window (widget),
					  &attributes,
					  attributes_mask);
      gdk_window_set_user_data (priv->drag_window, widget);
    }

  g_object_ref (page->tab_label);
  gtk_widget_unparent (page->tab_label);
  gtk_widget_set_parent_window (page->tab_label, priv->drag_window);
  gtk_widget_set_parent (page->tab_label, widget);
  g_object_unref (page->tab_label);

  gdk_window_show (priv->drag_window);

  /* the grab will dissapear when the window is hidden */
  gdk_pointer_grab (priv->drag_window,
		    FALSE,
		    GDK_POINTER_MOTION_MASK | GDK_BUTTON_RELEASE_MASK,
		    NULL, NULL, GDK_CURRENT_TIME);
}

/* This function undoes the reparenting that happens both when drag_window
 * is shown for reordering and when the DnD icon is shown for detaching
 */
static void
hide_drag_window (GtkNotebook        *notebook,
		  GtkNotebookPrivate *priv,
		  GtkNotebookPage    *page)
{
  GtkWidget *widget = GTK_WIDGET (notebook);
  GtkWidget *parent = page->tab_label->parent;

  if (page->tab_label->window != widget->window ||
      !NOTEBOOK_IS_TAB_LABEL_PARENT (notebook, page))
    {
      g_object_ref (page->tab_label);

      if (GTK_IS_WINDOW (parent))
	{
	  /* parent widget is the drag window */
	  gtk_container_remove (GTK_CONTAINER (parent), page->tab_label);
	}
      else
	gtk_widget_unparent (page->tab_label);

      gtk_widget_set_parent (page->tab_label, widget);
      g_object_unref (page->tab_label);
    }

  if (priv->drag_window &&
      gdk_window_is_visible (priv->drag_window))
    gdk_window_hide (priv->drag_window);
}

static void
gtk_notebook_stop_reorder (GtkNotebook *notebook)
{
  GtkNotebookPrivate *priv;
  GtkNotebookPage *page;

  priv = GTK_NOTEBOOK_GET_PRIVATE (notebook);

  if (priv->operation == DRAG_OPERATION_DETACH)
    page = priv->detached_tab;
  else
    page = notebook->cur_page;

  if (!page || !page->tab_label)
    return;

  priv->pressed_button = -1;

  if (page->reorderable || page->detachable)
    {
      if (priv->during_reorder)
	{
	  gint old_page_num, page_num;
	  GList *element;

	  element = get_drop_position (notebook, page->pack);
	  old_page_num = g_list_position (notebook->children, notebook->focus_tab);
	  page_num = reorder_tab (notebook, element, notebook->focus_tab);
          gtk_notebook_child_reordered (notebook, page);
          
	  if (priv->has_scrolled || old_page_num != page_num)
	    g_signal_emit (notebook,
			   notebook_signals[PAGE_REORDERED], 0,
			   page->child, page_num);

	  priv->has_scrolled = FALSE;
          priv->during_reorder = FALSE; 
	}

      hide_drag_window (notebook, priv, page);

      priv->operation = DRAG_OPERATION_NONE;
      gtk_notebook_pages_allocate (notebook);

      if (priv->dnd_timer)
	{
	  g_source_remove (priv->dnd_timer);
	  priv->dnd_timer = 0;
	}
    }
}

static gint
gtk_notebook_button_release (GtkWidget      *widget,
			     GdkEventButton *event)
{
  GtkNotebook *notebook;
  GtkNotebookPrivate *priv;
  GtkNotebookPage *page;

  if (event->type != GDK_BUTTON_RELEASE)
    return FALSE;

  notebook = GTK_NOTEBOOK (widget);
  priv = GTK_NOTEBOOK_GET_PRIVATE (notebook);
  page = notebook->cur_page;

  if (!priv->during_detach &&
      page->reorderable &&
      event->button == priv->pressed_button)
    gtk_notebook_stop_reorder (notebook);

  if (event->button == notebook->button)
    {
      stop_scrolling (notebook);
      return TRUE;
    }
  else
    return FALSE;
}

static gint
gtk_notebook_leave_notify (GtkWidget        *widget,
			   GdkEventCrossing *event)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  gint x, y;

  if (!get_widget_coordinates (widget, (GdkEvent *)event, &x, &y))
    return FALSE;

  if (notebook->in_child)
    {
      notebook->in_child = 0;
      gtk_notebook_redraw_arrows (notebook);
    }

  return TRUE;
}

static GtkNotebookPointerPosition
get_pointer_position (GtkNotebook *notebook)
{
  GtkWidget *widget = (GtkWidget *) notebook;
  GtkNotebookPrivate *priv = GTK_NOTEBOOK_GET_PRIVATE (notebook);
  gint wx, wy, width, height;
  gboolean is_rtl;

  if (!notebook->scrollable)
    return POINTER_BETWEEN;

  gdk_window_get_position (notebook->event_window, &wx, &wy);
  width = gdk_window_get_width (notebook->event_window);
  height = gdk_window_get_height (notebook->event_window);

  if (notebook->tab_pos == GTK_POS_TOP ||
      notebook->tab_pos == GTK_POS_BOTTOM)
    {
      gint x;

      is_rtl = gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;
      x = priv->mouse_x - wx;

      if (x > width - SCROLL_THRESHOLD)
	return (is_rtl) ? POINTER_BEFORE : POINTER_AFTER;
      else if (x < SCROLL_THRESHOLD)
	return (is_rtl) ? POINTER_AFTER : POINTER_BEFORE;
      else
	return POINTER_BETWEEN;
    }
  else
    {
      gint y;

      y = priv->mouse_y - wy;
      if (y > height - SCROLL_THRESHOLD)
	return POINTER_AFTER;
      else if (y < SCROLL_THRESHOLD)
	return POINTER_BEFORE;
      else
	return POINTER_BETWEEN;
    }
}

static gboolean
scroll_notebook_timer (gpointer data)
{
  GtkNotebook *notebook = (GtkNotebook *) data;
  GtkNotebookPrivate *priv;
  GtkNotebookPointerPosition pointer_position;
  GList *element, *first_tab;

  priv = GTK_NOTEBOOK_GET_PRIVATE (notebook);
  pointer_position = get_pointer_position (notebook);

  element = get_drop_position (notebook, notebook->cur_page->pack);
  reorder_tab (notebook, element, notebook->focus_tab);
  first_tab = gtk_notebook_search_page (notebook, notebook->first_tab,
					(pointer_position == POINTER_BEFORE) ? STEP_PREV : STEP_NEXT,
					TRUE);
  if (first_tab)
    {
      notebook->first_tab = first_tab;
      gtk_notebook_pages_allocate (notebook);

      gdk_window_move_resize (priv->drag_window,
			      priv->drag_window_x,
			      priv->drag_window_y,
			      notebook->cur_page->allocation.width,
			      notebook->cur_page->allocation.height);
      gdk_window_raise (priv->drag_window);
    }

  return TRUE;
}

static gboolean
check_threshold (GtkNotebook *notebook,
		 gint         current_x,
		 gint         current_y)
{
  GtkWidget *widget;
  gint dnd_threshold;
  GdkRectangle rectangle = { 0, }; /* shut up gcc */
  GtkSettings *settings;
  
  widget = GTK_WIDGET (notebook);
  settings = gtk_widget_get_settings (GTK_WIDGET (notebook));
  g_object_get (G_OBJECT (settings), "gtk-dnd-drag-threshold", &dnd_threshold, NULL);

  /* we want a large threshold */
  dnd_threshold *= DND_THRESHOLD_MULTIPLIER;

  gdk_window_get_position (notebook->event_window, &rectangle.x, &rectangle.y);
  rectangle.width = gdk_window_get_width (notebook->event_window);
  rectangle.height = gdk_window_get_height (notebook->event_window);

  rectangle.x -= dnd_threshold;
  rectangle.width += 2 * dnd_threshold;
  rectangle.y -= dnd_threshold;
  rectangle.height += 2 * dnd_threshold;

  return (current_x < rectangle.x ||
	  current_x > rectangle.x + rectangle.width ||
	  current_y < rectangle.y ||
	  current_y > rectangle.y + rectangle.height);
}

static gint
gtk_notebook_motion_notify (GtkWidget      *widget,
			    GdkEventMotion *event)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  GtkNotebookPrivate *priv = GTK_NOTEBOOK_GET_PRIVATE (notebook);
  GtkNotebookPage *page;
  GtkNotebookArrow arrow;
  GtkNotebookPointerPosition pointer_position;
  GtkSettings *settings;
  guint timeout;
  gint x_win, y_win;

  page = notebook->cur_page;

  if (!page)
    return FALSE;

  if (!(event->state & GDK_BUTTON1_MASK) &&
      priv->pressed_button != -1)
    {
      gtk_notebook_stop_reorder (notebook);
      stop_scrolling (notebook);
    }

  if (event->time < priv->timestamp + MSECS_BETWEEN_UPDATES)
    return FALSE;

  priv->timestamp = event->time;

  /* While animating the move, event->x is relative to the flying tab
   * (priv->drag_window has a pointer grab), but we need coordinates relative to
   * the notebook widget.
   */
  gdk_window_get_origin (widget->window, &x_win, &y_win);
  priv->mouse_x = event->x_root - x_win;
  priv->mouse_y = event->y_root - y_win;

  arrow = gtk_notebook_get_arrow (notebook, priv->mouse_x, priv->mouse_y);
  if (arrow != notebook->in_child)
    {
      notebook->in_child = arrow;
      gtk_notebook_redraw_arrows (notebook);
    }

  if (priv->pressed_button == -1)
    return FALSE;

  if (page->detachable &&
      check_threshold (notebook, priv->mouse_x, priv->mouse_y))
    {
      priv->detached_tab = notebook->cur_page;
      priv->during_detach = TRUE;

      gtk_drag_begin (widget, priv->source_targets, GDK_ACTION_MOVE,
		      priv->pressed_button, (GdkEvent*) event);
      return TRUE;
    }

  if (page->reorderable &&
      (priv->during_reorder ||
       gtk_drag_check_threshold (widget, priv->drag_begin_x, priv->drag_begin_y, priv->mouse_x, priv->mouse_y)))
    {
      priv->during_reorder = TRUE;
      pointer_position = get_pointer_position (notebook);

      if (event->window == priv->drag_window &&
	  pointer_position != POINTER_BETWEEN &&
	  gtk_notebook_show_arrows (notebook))
	{
	  /* scroll tabs */
	  if (!priv->dnd_timer)
	    {
	      priv->has_scrolled = TRUE;
	      settings = gtk_widget_get_settings (GTK_WIDGET (notebook));
	      g_object_get (settings, "gtk-timeout-repeat", &timeout, NULL);

	      priv->dnd_timer = gdk_threads_add_timeout (timeout * SCROLL_DELAY_FACTOR,
					       scroll_notebook_timer, 
					       (gpointer) notebook);
	    }
	}
      else
	{
	  if (priv->dnd_timer)
	    {
	      g_source_remove (priv->dnd_timer);
	      priv->dnd_timer = 0;
	    }
	}

      if (event->window == priv->drag_window ||
	  priv->operation != DRAG_OPERATION_REORDER)
	{
	  /* the drag operation is beginning, create the window */
	  if (priv->operation != DRAG_OPERATION_REORDER)
	    {
	      priv->operation = DRAG_OPERATION_REORDER;
	      show_drag_window (notebook, priv, page);
	    }

	  gtk_notebook_pages_allocate (notebook);
	  gdk_window_move_resize (priv->drag_window,
				  priv->drag_window_x,
				  priv->drag_window_y,
				  page->allocation.width,
				  page->allocation.height);
	}
    }

  return TRUE;
}

static void
gtk_notebook_grab_notify (GtkWidget *widget,
			  gboolean   was_grabbed)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);

  if (!was_grabbed)
    {
      gtk_notebook_stop_reorder (notebook);
      stop_scrolling (notebook);
    }
}

static void
gtk_notebook_state_changed (GtkWidget    *widget,
			    GtkStateType  previous_state)
{
  if (!gtk_widget_is_sensitive (widget))
    stop_scrolling (GTK_NOTEBOOK (widget));
}

static gint
gtk_notebook_focus_in (GtkWidget     *widget,
		       GdkEventFocus *event)
{
  gtk_notebook_redraw_tabs (GTK_NOTEBOOK (widget));
  
  return FALSE;
}

static gint
gtk_notebook_focus_out (GtkWidget     *widget,
			GdkEventFocus *event)
{
  gtk_notebook_redraw_tabs (GTK_NOTEBOOK (widget));

  return FALSE;
}

static void
gtk_notebook_draw_focus (GtkWidget      *widget,
			 GdkEventExpose *event)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);

  if (gtk_widget_has_focus (widget) && gtk_widget_is_drawable (widget) &&
      notebook->show_tabs && notebook->cur_page &&
      notebook->cur_page->tab_label->window == event->window)
    {
      GtkNotebookPage *page;

      page = notebook->cur_page;

      if (gtk_widget_intersect (page->tab_label, &event->area, NULL))
        {
          GdkRectangle area;
          gint focus_width;

          gtk_widget_style_get (widget, "focus-line-width", &focus_width, NULL);

          area.x = page->tab_label->allocation.x - focus_width;
          area.y = page->tab_label->allocation.y - focus_width;
          area.width = page->tab_label->allocation.width + 2 * focus_width;
          area.height = page->tab_label->allocation.height + 2 * focus_width;

	  gtk_paint_focus (widget->style, event->window, 
                           gtk_widget_get_state (widget), NULL, widget, "tab",
			   area.x, area.y, area.width, area.height);
        }
    }
}

static void
gtk_notebook_style_set  (GtkWidget *widget,
			 GtkStyle  *previous)
{
  GtkNotebook *notebook;

  gboolean has_before_previous;
  gboolean has_before_next;
  gboolean has_after_previous;
  gboolean has_after_next;

  notebook = GTK_NOTEBOOK (widget);
  
  gtk_widget_style_get (widget,
                        "has-backward-stepper", &has_before_previous,
                        "has-secondary-forward-stepper", &has_before_next,
                        "has-secondary-backward-stepper", &has_after_previous,
                        "has-forward-stepper", &has_after_next,
                        NULL);
  
  notebook->has_before_previous = has_before_previous;
  notebook->has_before_next = has_before_next;
  notebook->has_after_previous = has_after_previous;
  notebook->has_after_next = has_after_next;

  GTK_WIDGET_CLASS (gtk_notebook_parent_class)->style_set (widget, previous);
}

static gboolean
on_drag_icon_expose (GtkWidget      *widget,
		     GdkEventExpose *event,
		     gpointer        data)
{
  GtkWidget *notebook, *child = GTK_WIDGET (data);
  GtkRequisition requisition;
  gint gap_pos;

  notebook = GTK_WIDGET (data);
  child = GTK_BIN (widget)->child;
  gtk_widget_size_request (widget, &requisition);
  gap_pos = get_tab_gap_pos (GTK_NOTEBOOK (notebook));

  gtk_paint_extension (notebook->style, widget->window,
		       GTK_STATE_NORMAL, GTK_SHADOW_OUT,
		       NULL, widget, "tab",
		       0, 0,
		       requisition.width, requisition.height,
		       gap_pos);
  if (child)
    gtk_container_propagate_expose (GTK_CONTAINER (widget), child, event);

  return TRUE;
}

static void
gtk_notebook_drag_begin (GtkWidget        *widget,
			 GdkDragContext   *context)
{
  GtkNotebookPrivate *priv = GTK_NOTEBOOK_GET_PRIVATE (widget);
  GtkNotebook *notebook = (GtkNotebook*) widget;
  GtkWidget *tab_label;

  if (priv->dnd_timer)
    {
      g_source_remove (priv->dnd_timer);
      priv->dnd_timer = 0;
    }

  priv->operation = DRAG_OPERATION_DETACH;
  gtk_notebook_pages_allocate (notebook);

  tab_label = priv->detached_tab->tab_label;

  hide_drag_window (notebook, priv, notebook->cur_page);
  g_object_ref (tab_label);
  gtk_widget_unparent (tab_label);

  priv->dnd_window = gtk_window_new (GTK_WINDOW_POPUP);
  gtk_window_set_screen (GTK_WINDOW (priv->dnd_window),
                         gtk_widget_get_screen (widget));
  gtk_widget_set_colormap (priv->dnd_window, gtk_widget_get_colormap (widget));
  gtk_container_add (GTK_CONTAINER (priv->dnd_window), tab_label);
  gtk_widget_set_size_request (priv->dnd_window,
			       priv->detached_tab->allocation.width,
			       priv->detached_tab->allocation.height);
  g_object_unref (tab_label);

  g_signal_connect (G_OBJECT (priv->dnd_window), "expose-event",
		    G_CALLBACK (on_drag_icon_expose), notebook);

  gtk_drag_set_icon_widget (context, priv->dnd_window, -2, -2);
}

static void
gtk_notebook_drag_end (GtkWidget      *widget,
		       GdkDragContext *context)
{
  GtkNotebookPrivate *priv = GTK_NOTEBOOK_GET_PRIVATE (widget);

  gtk_notebook_stop_reorder (GTK_NOTEBOOK (widget));

  if (priv->detached_tab)
    gtk_notebook_switch_page (GTK_NOTEBOOK (widget), priv->detached_tab);

  GTK_BIN (priv->dnd_window)->child = NULL;
  gtk_widget_destroy (priv->dnd_window);
  priv->dnd_window = NULL;

  priv->operation = DRAG_OPERATION_NONE;
}

static GtkNotebook *
gtk_notebook_create_window (GtkNotebook *notebook,
                            GtkWidget   *page,
                            gint         x,
                            gint         y)
{
  if (window_creation_hook)
    return (* window_creation_hook) (notebook, page, x, y, window_creation_hook_data);

  return NULL;
}

static gboolean
gtk_notebook_drag_failed (GtkWidget      *widget,
			  GdkDragContext *context,
			  GtkDragResult   result,
			  gpointer        data)
{
  if (result == GTK_DRAG_RESULT_NO_TARGET)
    {
      GtkNotebookPrivate *priv;
      GtkNotebook *notebook, *dest_notebook = NULL;
      GdkDisplay *display;
      gint x, y;

      notebook = GTK_NOTEBOOK (widget);
      priv = GTK_NOTEBOOK_GET_PRIVATE (notebook);

      display = gtk_widget_get_display (widget);
      gdk_display_get_pointer (display, NULL, &x, &y, NULL);

      g_signal_emit (notebook, notebook_signals[CREATE_WINDOW], 0,
                     priv->detached_tab->child, x, y, &dest_notebook);

      if (dest_notebook)
	do_detach_tab (notebook, dest_notebook, priv->detached_tab->child, 0, 0);

      return TRUE;
    }

  return FALSE;
}

static gboolean
gtk_notebook_switch_tab_timeout (gpointer data)
{
  GtkNotebook *notebook;
  GtkNotebookPrivate *priv;
  GList *tab;
  gint x, y;

  notebook = GTK_NOTEBOOK (data);
  priv = GTK_NOTEBOOK_GET_PRIVATE (notebook);

  priv->switch_tab_timer = 0;
  x = priv->mouse_x;
  y = priv->mouse_y;

  if ((tab = get_tab_at_pos (notebook, x, y)) != NULL)
    {
      /* FIXME: hack, we don't want the
       * focus to move fom the source widget
       */
      notebook->child_has_focus = FALSE;
      gtk_notebook_switch_focus_tab (notebook, tab);
    }

  return FALSE;
}

static gboolean
gtk_notebook_drag_motion (GtkWidget      *widget,
			  GdkDragContext *context,
			  gint            x,
			  gint            y,
			  guint           time)
{
  GtkNotebook *notebook;
  GtkNotebookPrivate *priv;
  GdkRectangle position;
  GtkSettings *settings;
  GtkNotebookArrow arrow;
  guint timeout;
  GdkAtom target, tab_target;

  notebook = GTK_NOTEBOOK (widget);
  arrow = gtk_notebook_get_arrow (notebook,
				  x + widget->allocation.x,
				  y + widget->allocation.y);
  if (arrow)
    {
      notebook->click_child = arrow;
      gtk_notebook_set_scroll_timer (notebook);
      gdk_drag_status (context, 0, time);
      return TRUE;
    }

  stop_scrolling (notebook);
  target = gtk_drag_dest_find_target (widget, context, NULL);
  tab_target = gdk_atom_intern_static_string ("GTK_NOTEBOOK_TAB");

  if (target == tab_target)
    {
      gpointer widget_group, source_widget_group;
      GtkWidget *source_widget;

      source_widget = gtk_drag_get_source_widget (context);
      g_assert (source_widget);

      widget_group = gtk_notebook_get_group (notebook);
      source_widget_group = gtk_notebook_get_group (GTK_NOTEBOOK (source_widget));

      if (widget_group && source_widget_group &&
	  widget_group == source_widget_group &&
	  !(widget == GTK_NOTEBOOK (source_widget)->cur_page->child || 
	    gtk_widget_is_ancestor (widget, GTK_NOTEBOOK (source_widget)->cur_page->child)))
	{
	  gdk_drag_status (context, GDK_ACTION_MOVE, time);
	  return TRUE;
	}
      else
	{
	  /* it's a tab, but doesn't share
	   * ID with this notebook */
	  gdk_drag_status (context, 0, time);
	}
    }

  priv = GTK_NOTEBOOK_GET_PRIVATE (widget);
  x += widget->allocation.x;
  y += widget->allocation.y;

  if (gtk_notebook_get_event_window_position (notebook, &position) &&
      x >= position.x && x <= position.x + position.width &&
      y >= position.y && y <= position.y + position.height)
    {
      priv->mouse_x = x;
      priv->mouse_y = y;

      if (!priv->switch_tab_timer)
	{
          settings = gtk_widget_get_settings (widget);

          g_object_get (settings, "gtk-timeout-expand", &timeout, NULL);
	  priv->switch_tab_timer = gdk_threads_add_timeout (timeout,
						  gtk_notebook_switch_tab_timeout,
						  widget);
	}
    }
  else
    {
      if (priv->switch_tab_timer)
	{
	  g_source_remove (priv->switch_tab_timer);
	  priv->switch_tab_timer = 0;
	}
    }

  return (target == tab_target) ? TRUE : FALSE;
}

static void
gtk_notebook_drag_leave (GtkWidget      *widget,
			 GdkDragContext *context,
			 guint           time)
{
  GtkNotebookPrivate *priv;

  priv = GTK_NOTEBOOK_GET_PRIVATE (widget);

  if (priv->switch_tab_timer)
    {
      g_source_remove (priv->switch_tab_timer);
      priv->switch_tab_timer = 0;
    }

  stop_scrolling (GTK_NOTEBOOK (widget));
}

static gboolean
gtk_notebook_drag_drop (GtkWidget        *widget,
			GdkDragContext   *context,
			gint              x,
			gint              y,
			guint             time)
{
  GdkAtom target, tab_target;

  target = gtk_drag_dest_find_target (widget, context, NULL);
  tab_target = gdk_atom_intern_static_string ("GTK_NOTEBOOK_TAB");

  if (target == tab_target)
    {
      gtk_drag_get_data (widget, context, target, time);
      return TRUE;
    }

  return FALSE;
}

static void
do_detach_tab (GtkNotebook     *from,
	       GtkNotebook     *to,
	       GtkWidget       *child,
	       gint             x,
	       gint             y)
{
  GtkNotebookPrivate *priv;
  GtkWidget *tab_label, *menu_label;
  gboolean tab_expand, tab_fill, reorderable, detachable;
  GList *element;
  guint tab_pack;
  gint page_num;

  menu_label = gtk_notebook_get_menu_label (from, child);

  if (menu_label)
    g_object_ref (menu_label);

  tab_label = gtk_notebook_get_tab_label (from, child);
  
  if (tab_label)
    g_object_ref (tab_label);

  g_object_ref (child);

  gtk_container_child_get (GTK_CONTAINER (from),
			   child,
			   "tab-expand", &tab_expand,
			   "tab-fill", &tab_fill,
			   "tab-pack", &tab_pack,
			   "reorderable", &reorderable,
			   "detachable", &detachable,
			   NULL);

  gtk_container_remove (GTK_CONTAINER (from), child);

  priv = GTK_NOTEBOOK_GET_PRIVATE (to);
  priv->mouse_x = x + GTK_WIDGET (to)->allocation.x;
  priv->mouse_y = y + GTK_WIDGET (to)->allocation.y;

  element = get_drop_position (to, tab_pack);
  page_num = g_list_position (to->children, element);
  gtk_notebook_insert_page_menu (to, child, tab_label, menu_label, page_num);

  gtk_container_child_set (GTK_CONTAINER (to), child,
			   "tab-pack", tab_pack,
			   "tab-expand", tab_expand,
			   "tab-fill", tab_fill,
			   "reorderable", reorderable,
			   "detachable", detachable,
			   NULL);
  if (child)
    g_object_unref (child);

  if (tab_label)
    g_object_unref (tab_label);

  if (menu_label)
    g_object_unref (menu_label);

  gtk_notebook_set_current_page (to, page_num);
}

static void
gtk_notebook_drag_data_get (GtkWidget        *widget,
			    GdkDragContext   *context,
			    GtkSelectionData *data,
			    guint             info,
			    guint             time)
{
  GtkNotebookPrivate *priv;

  if (data->target == gdk_atom_intern_static_string ("GTK_NOTEBOOK_TAB"))
    {
      priv = GTK_NOTEBOOK_GET_PRIVATE (widget);

      gtk_selection_data_set (data,
			      data->target,
			      8,
			      (void*) &priv->detached_tab->child,
			      sizeof (gpointer));
    }
}

static void
gtk_notebook_drag_data_received (GtkWidget        *widget,
				 GdkDragContext   *context,
				 gint              x,
				 gint              y,
				 GtkSelectionData *data,
				 guint             info,
				 guint             time)
{
  GtkNotebook *notebook;
  GtkWidget *source_widget;
  GtkWidget **child;

  notebook = GTK_NOTEBOOK (widget);
  source_widget = gtk_drag_get_source_widget (context);

  if (source_widget &&
      data->target == gdk_atom_intern_static_string ("GTK_NOTEBOOK_TAB"))
    {
      child = (void*) data->data;

      do_detach_tab (GTK_NOTEBOOK (source_widget), notebook, *child, x, y);
      gtk_drag_finish (context, TRUE, FALSE, time);
    }
  else
    gtk_drag_finish (context, FALSE, FALSE, time);
}

/* Private GtkContainer Methods :
 * 
 * gtk_notebook_set_child_arg
 * gtk_notebook_get_child_arg
 * gtk_notebook_add
 * gtk_notebook_remove
 * gtk_notebook_focus
 * gtk_notebook_set_focus_child
 * gtk_notebook_child_type
 * gtk_notebook_forall
 */
static void
gtk_notebook_set_child_property (GtkContainer    *container,
				 GtkWidget       *child,
				 guint            property_id,
				 const GValue    *value,
				 GParamSpec      *pspec)
{
  gboolean expand;
  gboolean fill;
  GtkPackType pack_type;

  /* not finding child's page is valid for menus or labels */
  if (!gtk_notebook_find_child (GTK_NOTEBOOK (container), child, NULL))
    return;

  switch (property_id)
    {
    case CHILD_PROP_TAB_LABEL:
      /* a NULL pointer indicates a default_tab setting, otherwise
       * we need to set the associated label
       */
      gtk_notebook_set_tab_label_text (GTK_NOTEBOOK (container), child,
				       g_value_get_string (value));
      break;
    case CHILD_PROP_MENU_LABEL:
      gtk_notebook_set_menu_label_text (GTK_NOTEBOOK (container), child,
					g_value_get_string (value));
      break;
    case CHILD_PROP_POSITION:
      gtk_notebook_reorder_child (GTK_NOTEBOOK (container), child,
				  g_value_get_int (value));
      break;
    case CHILD_PROP_TAB_EXPAND:
      gtk_notebook_query_tab_label_packing (GTK_NOTEBOOK (container), child,
					    &expand, &fill, &pack_type);
      gtk_notebook_set_tab_label_packing (GTK_NOTEBOOK (container), child,
					  g_value_get_boolean (value),
					  fill, pack_type);
      break;
    case CHILD_PROP_TAB_FILL:
      gtk_notebook_query_tab_label_packing (GTK_NOTEBOOK (container), child,
					    &expand, &fill, &pack_type);
      gtk_notebook_set_tab_label_packing (GTK_NOTEBOOK (container), child,
					  expand,
					  g_value_get_boolean (value),
					  pack_type);
      break;
    case CHILD_PROP_TAB_PACK:
      gtk_notebook_query_tab_label_packing (GTK_NOTEBOOK (container), child,
					    &expand, &fill, &pack_type);
      gtk_notebook_set_tab_label_packing (GTK_NOTEBOOK (container), child,
					  expand, fill,
					  g_value_get_enum (value));
      break;
    case CHILD_PROP_REORDERABLE:
      gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (container), child,
					g_value_get_boolean (value));
      break;
    case CHILD_PROP_DETACHABLE:
      gtk_notebook_set_tab_detachable (GTK_NOTEBOOK (container), child,
				       g_value_get_boolean (value));
      break;
    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      break;
    }
}

static void
gtk_notebook_get_child_property (GtkContainer    *container,
				 GtkWidget       *child,
				 guint            property_id,
				 GValue          *value,
				 GParamSpec      *pspec)
{
  GList *list;
  GtkNotebook *notebook;
  GtkWidget *label;
  gboolean expand;
  gboolean fill;
  GtkPackType pack_type;

  notebook = GTK_NOTEBOOK (container);

  /* not finding child's page is valid for menus or labels */
  list = gtk_notebook_find_child (notebook, child, NULL);
  if (!list)
    {
      /* nothing to set on labels or menus */
      g_param_value_set_default (pspec, value);
      return;
    }

  switch (property_id)
    {
    case CHILD_PROP_TAB_LABEL:
      label = gtk_notebook_get_tab_label (notebook, child);

      if (GTK_IS_LABEL (label))
	g_value_set_string (value, GTK_LABEL (label)->label);
      else
	g_value_set_string (value, NULL);
      break;
    case CHILD_PROP_MENU_LABEL:
      label = gtk_notebook_get_menu_label (notebook, child);

      if (GTK_IS_LABEL (label))
	g_value_set_string (value, GTK_LABEL (label)->label);
      else
	g_value_set_string (value, NULL);
      break;
    case CHILD_PROP_POSITION:
      g_value_set_int (value, g_list_position (notebook->children, list));
      break;
    case CHILD_PROP_TAB_EXPAND:
	gtk_notebook_query_tab_label_packing (GTK_NOTEBOOK (container), child,
					      &expand, NULL, NULL);
	g_value_set_boolean (value, expand);
      break;
    case CHILD_PROP_TAB_FILL:
	gtk_notebook_query_tab_label_packing (GTK_NOTEBOOK (container), child,
					      NULL, &fill, NULL);
	g_value_set_boolean (value, fill);
      break;
    case CHILD_PROP_TAB_PACK:
	gtk_notebook_query_tab_label_packing (GTK_NOTEBOOK (container), child,
					      NULL, NULL, &pack_type);
	g_value_set_enum (value, pack_type);
      break;
    case CHILD_PROP_REORDERABLE:
      g_value_set_boolean (value,
			   gtk_notebook_get_tab_reorderable (GTK_NOTEBOOK (container), child));
      break;
    case CHILD_PROP_DETACHABLE:
      g_value_set_boolean (value,
			   gtk_notebook_get_tab_detachable (GTK_NOTEBOOK (container), child));
      break;
    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      break;
    }
}

static void
gtk_notebook_add (GtkContainer *container,
		  GtkWidget    *widget)
{
  gtk_notebook_insert_page_menu (GTK_NOTEBOOK (container), widget, 
				 NULL, NULL, -1);
}

static void
gtk_notebook_remove (GtkContainer *container,
		     GtkWidget    *widget)
{
  GtkNotebook *notebook;
  GtkNotebookPage *page;
  GList *children;
  gint page_num = 0;

  notebook = GTK_NOTEBOOK (container);

  children = notebook->children;
  while (children)
    {
      page = children->data;

      if (page->child == widget)
	break;

      page_num++;
      children = children->next;
    }
 
  if (children == NULL)
    return;
  
  g_object_ref (widget);

  gtk_notebook_real_remove (notebook, children);

  g_signal_emit (notebook,
		 notebook_signals[PAGE_REMOVED],
		 0,
		 widget,
		 page_num);
  
  g_object_unref (widget);
}

static gboolean
focus_tabs_in (GtkNotebook *notebook)
{
  if (notebook->show_tabs && notebook->cur_page)
    {
      gtk_widget_grab_focus (GTK_WIDGET (notebook));

      gtk_notebook_switch_focus_tab (notebook,
				     g_list_find (notebook->children,
						  notebook->cur_page));

      return TRUE;
    }
  else
    return FALSE;
}

static gboolean
focus_tabs_move (GtkNotebook     *notebook,
		 GtkDirectionType direction,
		 gint             search_direction)
{
  GList *new_page;

  new_page = gtk_notebook_search_page (notebook, notebook->focus_tab,
				       search_direction, TRUE);
  if (!new_page)
    {
      gboolean wrap_around;

      g_object_get (gtk_widget_get_settings (GTK_WIDGET (notebook)),
                    "gtk-keynav-wrap-around", &wrap_around,
                    NULL);

      if (wrap_around)
        new_page = gtk_notebook_search_page (notebook, NULL,
                                             search_direction, TRUE);
    }

  if (new_page)
    gtk_notebook_switch_focus_tab (notebook, new_page);
  else
    gtk_widget_error_bell (GTK_WIDGET (notebook));

  return TRUE;
}

static gboolean
focus_child_in (GtkNotebook      *notebook,
		GtkDirectionType  direction)
{
  if (notebook->cur_page)
    return gtk_widget_child_focus (notebook->cur_page->child, direction);
  else
    return FALSE;
}

static gboolean
focus_action_in (GtkNotebook      *notebook,
                 gint              action,
                 GtkDirectionType  direction)
{
  GtkNotebookPrivate *priv = GTK_NOTEBOOK_GET_PRIVATE (notebook);

  if (priv->action_widget[action] &&
      gtk_widget_get_visible (priv->action_widget[action]))
    return gtk_widget_child_focus (priv->action_widget[action], direction);
  else
    return FALSE;
}

/* Focus in the notebook can either be on the pages, or on
 * the tabs or on the action_widgets.
 */
static gint
gtk_notebook_focus (GtkWidget        *widget,
		    GtkDirectionType  direction)
{
  GtkNotebookPrivate *priv;
  GtkWidget *old_focus_child;
  GtkNotebook *notebook;
  GtkDirectionType effective_direction;
  gint first_action;
  gint last_action;

  gboolean widget_is_focus;
  GtkContainer *container;

  container = GTK_CONTAINER (widget);
  notebook = GTK_NOTEBOOK (container);
  priv = GTK_NOTEBOOK_GET_PRIVATE (notebook);

  if (notebook->tab_pos == GTK_POS_TOP ||
      notebook->tab_pos == GTK_POS_LEFT)
    {
      first_action = ACTION_WIDGET_START;
      last_action = ACTION_WIDGET_END;
    }
  else
    {
      first_action = ACTION_WIDGET_END;
      last_action = ACTION_WIDGET_START;
    }

  if (notebook->focus_out)
    {
      notebook->focus_out = FALSE; /* Clear this to catch the wrap-around case */
      return FALSE;
    }

  widget_is_focus = gtk_widget_is_focus (widget);
  old_focus_child = container->focus_child;

  effective_direction = get_effective_direction (notebook, direction);

  if (old_focus_child)		/* Focus on page child or action widget */
    {
      if (gtk_widget_child_focus (old_focus_child, direction))
        return TRUE;

      if (old_focus_child == priv->action_widget[ACTION_WIDGET_START])
	{
	  switch (effective_direction)
	    {
	    case GTK_DIR_DOWN:
              return focus_child_in (notebook, GTK_DIR_TAB_FORWARD);
	    case GTK_DIR_RIGHT:
	      return focus_tabs_in (notebook);
	    case GTK_DIR_LEFT:
              return FALSE;
	    case GTK_DIR_UP:
	      return FALSE;
            default:
              switch (direction)
                {
	        case GTK_DIR_TAB_FORWARD:
                  if ((notebook->tab_pos == GTK_POS_RIGHT || notebook->tab_pos == GTK_POS_BOTTOM) &&
                      focus_child_in (notebook, direction))
                    return TRUE;
	          return focus_tabs_in (notebook);
                case GTK_DIR_TAB_BACKWARD:
                  return FALSE;
                default:
                  g_assert_not_reached ();
                }
	    }
	}
      else if (old_focus_child == priv->action_widget[ACTION_WIDGET_END])
	{
	  switch (effective_direction)
	    {
	    case GTK_DIR_DOWN:
              return focus_child_in (notebook, GTK_DIR_TAB_FORWARD);
	    case GTK_DIR_RIGHT:
              return FALSE;
	    case GTK_DIR_LEFT:
	      return focus_tabs_in (notebook);
	    case GTK_DIR_UP:
	      return FALSE;
            default:
              switch (direction)
                {
	        case GTK_DIR_TAB_FORWARD:
                  return FALSE;
                case GTK_DIR_TAB_BACKWARD:
                  if ((notebook->tab_pos == GTK_POS_TOP || notebook->tab_pos == GTK_POS_LEFT) &&
                      focus_child_in (notebook, direction))
                    return TRUE;
	          return focus_tabs_in (notebook);
                default:
                  g_assert_not_reached ();
                }
	    }
	}
      else
	{
	  switch (effective_direction)
	    {
	    case GTK_DIR_TAB_BACKWARD:
	    case GTK_DIR_UP:
	      /* Focus onto the tabs */
	      return focus_tabs_in (notebook);
	    case GTK_DIR_DOWN:
	    case GTK_DIR_LEFT:
	    case GTK_DIR_RIGHT:
	      return FALSE;
	    case GTK_DIR_TAB_FORWARD:
              return focus_action_in (notebook, last_action, direction);
	    }
	}
    }
  else if (widget_is_focus)	/* Focus was on tabs */
    {
      switch (effective_direction)
	{
        case GTK_DIR_TAB_BACKWARD:
              return focus_action_in (notebook, first_action, direction);
        case GTK_DIR_UP:
	  return FALSE;
        case GTK_DIR_TAB_FORWARD:
          if (focus_child_in (notebook, GTK_DIR_TAB_FORWARD))
            return TRUE;
          return focus_action_in (notebook, last_action, direction);
	case GTK_DIR_DOWN:
	  /* We use TAB_FORWARD rather than direction so that we focus a more
	   * predictable widget for the user; users may be using arrow focusing
	   * in this situation even if they don't usually use arrow focusing.
	   */
          return focus_child_in (notebook, GTK_DIR_TAB_FORWARD);
	case GTK_DIR_LEFT:
	  return focus_tabs_move (notebook, direction, STEP_PREV);
	case GTK_DIR_RIGHT:
	  return focus_tabs_move (notebook, direction, STEP_NEXT);
	}
    }
  else /* Focus was not on widget */
    {
      switch (effective_direction)
	{
	case GTK_DIR_TAB_FORWARD:
	case GTK_DIR_DOWN:
          if (focus_action_in (notebook, first_action, direction))
            return TRUE;
	  if (focus_tabs_in (notebook))
	    return TRUE;
          if (focus_action_in (notebook, last_action, direction))
            return TRUE;
	  if (focus_child_in (notebook, direction))
	    return TRUE;
	  return FALSE;
	case GTK_DIR_TAB_BACKWARD:
          if (focus_action_in (notebook, last_action, direction))
            return TRUE;
	  if (focus_child_in (notebook, direction))
	    return TRUE;
	  if (focus_tabs_in (notebook))
	    return TRUE;
          if (focus_action_in (notebook, first_action, direction))
            return TRUE;
	case GTK_DIR_UP:
	case GTK_DIR_LEFT:
	case GTK_DIR_RIGHT:
	  return focus_child_in (notebook, direction);
	}
    }

  g_assert_not_reached ();
  return FALSE;
}

static void
gtk_notebook_set_focus_child (GtkContainer *container,
			      GtkWidget    *child)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (container);
  GtkWidget *page_child;
  GtkWidget *toplevel;

  /* If the old focus widget was within a page of the notebook,
   * (child may either be NULL or not in this case), record it
   * for future use if we switch to the page with a mnemonic.
   */

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (container));
  if (toplevel && gtk_widget_is_toplevel (toplevel))
    {
      page_child = GTK_WINDOW (toplevel)->focus_widget; 
      while (page_child)
	{
	  if (page_child->parent == GTK_WIDGET (container))
	    {
	      GList *list = gtk_notebook_find_child (notebook, page_child, NULL);
	      if (list != NULL) 
		{
		  GtkNotebookPage *page = list->data;
	      
		  if (page->last_focus_child)
		    g_object_remove_weak_pointer (G_OBJECT (page->last_focus_child), (gpointer *)&page->last_focus_child);
		  
		  page->last_focus_child = GTK_WINDOW (toplevel)->focus_widget;
		  g_object_add_weak_pointer (G_OBJECT (page->last_focus_child), (gpointer *)&page->last_focus_child);
	      
		  break;
		}
	    }

	  page_child = page_child->parent;
	}
    }
  
  if (child)
    {
      g_return_if_fail (GTK_IS_WIDGET (child));

      notebook->child_has_focus = TRUE;
      if (!notebook->focus_tab)
	{
	  GList *children;
	  GtkNotebookPage *page;

	  children = notebook->children;
	  while (children)
	    {
	      page = children->data;
	      if (page->child == child || page->tab_label == child)
		gtk_notebook_switch_focus_tab (notebook, children);
	      children = children->next;
	    }
	}
    }
  else
    notebook->child_has_focus = FALSE;

  GTK_CONTAINER_CLASS (gtk_notebook_parent_class)->set_focus_child (container, child);
}

static void
gtk_notebook_forall (GtkContainer *container,
		     gboolean      include_internals,
		     GtkCallback   callback,
		     gpointer      callback_data)
{
  GtkNotebookPrivate *priv;
  GtkNotebook *notebook;
  GList *children;
  gint i;

  notebook = GTK_NOTEBOOK (container);
  priv = GTK_NOTEBOOK_GET_PRIVATE (notebook);

  children = notebook->children;
  while (children)
    {
      GtkNotebookPage *page;
      
      page = children->data;
      children = children->next;
      (* callback) (page->child, callback_data);

      if (include_internals)
	{
	  if (page->tab_label)
	    (* callback) (page->tab_label, callback_data);
	}
    }

  if (include_internals) {
    for (i = 0; i < N_ACTION_WIDGETS; i++)
      {
        if (priv->action_widget[i])
          (* callback) (priv->action_widget[i], callback_data);
      }
  }
}

static GType
gtk_notebook_child_type (GtkContainer     *container)
{
  return GTK_TYPE_WIDGET;
}

/* Private GtkNotebook Methods:
 *
 * gtk_notebook_real_insert_page
 */
static void
page_visible_cb (GtkWidget  *page,
                 GParamSpec *arg,
                 gpointer    data)
{
  GtkNotebook *notebook = (GtkNotebook *) data;
  GList *list;
  GList *next = NULL;

  if (notebook->cur_page &&
      notebook->cur_page->child == page &&
      !gtk_widget_get_visible (page))
    {
      list = g_list_find (notebook->children, notebook->cur_page);
      if (list)
        {
          next = gtk_notebook_search_page (notebook, list, STEP_NEXT, TRUE);
          if (!next)
            next = gtk_notebook_search_page (notebook, list, STEP_PREV, TRUE);
        }

      if (next)
        gtk_notebook_switch_page (notebook, GTK_NOTEBOOK_PAGE (next));
    }
}

static gint
gtk_notebook_real_insert_page (GtkNotebook *notebook,
			       GtkWidget   *child,
			       GtkWidget   *tab_label,
			       GtkWidget   *menu_label,
			       gint         position)
{
  GtkNotebookPage *page;
  gint nchildren;

  gtk_widget_freeze_child_notify (child);

  page = g_slice_new0 (GtkNotebookPage);
  page->child = child;

  nchildren = g_list_length (notebook->children);
  if ((position < 0) || (position > nchildren))
    position = nchildren;

  notebook->children = g_list_insert (notebook->children, page, position);

  if (!tab_label)
    {
      page->default_tab = TRUE;
      if (notebook->show_tabs)
	tab_label = gtk_label_new (NULL);
    }
  page->tab_label = tab_label;
  page->menu_label = menu_label;
  page->expand = FALSE;
  page->fill = TRUE;
  page->pack = GTK_PACK_START; 

  if (!menu_label)
    page->default_menu = TRUE;
  else  
    g_object_ref_sink (page->menu_label);

  if (notebook->menu)
    gtk_notebook_menu_item_create (notebook,
				   g_list_find (notebook->children, page));

  gtk_widget_set_parent (child, GTK_WIDGET (notebook));
  if (tab_label)
    gtk_widget_set_parent (tab_label, GTK_WIDGET (notebook));

  gtk_notebook_update_labels (notebook);

  if (!notebook->first_tab)
    notebook->first_tab = notebook->children;

  /* child visible will be turned on by switch_page below */
  if (notebook->cur_page != page)
    gtk_widget_set_child_visible (child, FALSE);

  if (tab_label)
    {
      if (notebook->show_tabs && gtk_widget_get_visible (child))
	gtk_widget_show (tab_label);
      else
	gtk_widget_hide (tab_label);

    page->mnemonic_activate_signal =
      g_signal_connect (tab_label,
			"mnemonic-activate",
			G_CALLBACK (gtk_notebook_mnemonic_activate_switch_page),
			notebook);
    }

  page->notify_visible_handler = g_signal_connect (child, "notify::visible",
						   G_CALLBACK (page_visible_cb), notebook);

  g_signal_emit (notebook,
		 notebook_signals[PAGE_ADDED],
		 0,
		 child,
		 position);

  if (!notebook->cur_page)
    {
      gtk_notebook_switch_page (notebook, page);
      /* focus_tab is set in the switch_page method */
      gtk_notebook_switch_focus_tab (notebook, notebook->focus_tab);
    }

  gtk_notebook_update_tab_states (notebook);

  if (notebook->scrollable)
    gtk_notebook_redraw_arrows (notebook);

  gtk_widget_child_notify (child, "tab-expand");
  gtk_widget_child_notify (child, "tab-fill");
  gtk_widget_child_notify (child, "tab-pack");
  gtk_widget_child_notify (child, "tab-label");
  gtk_widget_child_notify (child, "menu-label");
  gtk_widget_child_notify (child, "position");
  gtk_widget_thaw_child_notify (child);

  /* The page-added handler might have reordered the pages, re-get the position */
  return gtk_notebook_page_num (notebook, child);
}

/* Private GtkNotebook Functions:
 *
 * gtk_notebook_redraw_tabs
 * gtk_notebook_real_remove
 * gtk_notebook_update_labels
 * gtk_notebook_timer
 * gtk_notebook_set_scroll_timer
 * gtk_notebook_page_compare
 * gtk_notebook_real_page_position
 * gtk_notebook_search_page
 */
static void
gtk_notebook_redraw_tabs (GtkNotebook *notebook)
{
  GtkWidget *widget;
  GtkNotebookPage *page;
  GdkRectangle redraw_rect;
  gint border;
  gint tab_pos = get_effective_tab_pos (notebook);

  widget = GTK_WIDGET (notebook);
  border = GTK_CONTAINER (notebook)->border_width;

  if (!gtk_widget_get_mapped (widget) || !notebook->first_tab)
    return;

  page = notebook->first_tab->data;

  redraw_rect.x = border;
  redraw_rect.y = border;

  switch (tab_pos)
    {
    case GTK_POS_BOTTOM:
      redraw_rect.y = widget->allocation.height - border -
	page->allocation.height - widget->style->ythickness;

      if (page != notebook->cur_page)
	redraw_rect.y -= widget->style->ythickness;
      /* fall through */
    case GTK_POS_TOP:
      redraw_rect.width = widget->allocation.width - 2 * border;
      redraw_rect.height = page->allocation.height + widget->style->ythickness;

      if (page != notebook->cur_page)
	redraw_rect.height += widget->style->ythickness;
      break;
    case GTK_POS_RIGHT:
      redraw_rect.x = widget->allocation.width - border -
	page->allocation.width - widget->style->xthickness;

      if (page != notebook->cur_page)
	redraw_rect.x -= widget->style->xthickness;
      /* fall through */
    case GTK_POS_LEFT:
      redraw_rect.width = page->allocation.width + widget->style->xthickness;
      redraw_rect.height = widget->allocation.height - 2 * border;

      if (page != notebook->cur_page)
	redraw_rect.width += widget->style->xthickness;
      break;
    }

  redraw_rect.x += widget->allocation.x;
  redraw_rect.y += widget->allocation.y;

  gdk_window_invalidate_rect (widget->window, &redraw_rect, TRUE);
}

static void
gtk_notebook_redraw_arrows (GtkNotebook *notebook)
{
  if (gtk_widget_get_mapped (GTK_WIDGET (notebook)) &&
      gtk_notebook_show_arrows (notebook))
    {
      GdkRectangle rect;
      gint i;
      GtkNotebookArrow arrow[4];

      arrow[0] = notebook->has_before_previous ? ARROW_LEFT_BEFORE : ARROW_NONE;
      arrow[1] = notebook->has_before_next ? ARROW_RIGHT_BEFORE : ARROW_NONE;
      arrow[2] = notebook->has_after_previous ? ARROW_LEFT_AFTER : ARROW_NONE;
      arrow[3] = notebook->has_after_next ? ARROW_RIGHT_AFTER : ARROW_NONE;

      for (i = 0; i < 4; i++) 
	{
	  if (arrow[i] == ARROW_NONE)
	    continue;

	  gtk_notebook_get_arrow_rect (notebook, &rect, arrow[i]);
	  gdk_window_invalidate_rect (GTK_WIDGET (notebook)->window, 
				      &rect, FALSE);
	}
    }
}

static gboolean
gtk_notebook_timer (GtkNotebook *notebook)
{
  gboolean retval = FALSE;

  if (notebook->timer)
    {
      gtk_notebook_do_arrow (notebook, notebook->click_child);

      if (notebook->need_timer)
	{
          GtkSettings *settings;
          guint        timeout;

          settings = gtk_widget_get_settings (GTK_WIDGET (notebook));
          g_object_get (settings, "gtk-timeout-repeat", &timeout, NULL);

	  notebook->need_timer = FALSE;
	  notebook->timer = gdk_threads_add_timeout (timeout * SCROLL_DELAY_FACTOR,
					   (GSourceFunc) gtk_notebook_timer,
					   (gpointer) notebook);
	}
      else
	retval = TRUE;
    }

  return retval;
}

static void
gtk_notebook_set_scroll_timer (GtkNotebook *notebook)
{
  GtkWidget *widget = GTK_WIDGET (notebook);

  if (!notebook->timer)
    {
      GtkSettings *settings = gtk_widget_get_settings (widget);
      guint timeout;

      g_object_get (settings, "gtk-timeout-initial", &timeout, NULL);

      notebook->timer = gdk_threads_add_timeout (timeout,
				       (GSourceFunc) gtk_notebook_timer,
				       (gpointer) notebook);
      notebook->need_timer = TRUE;
    }
}

static gint
gtk_notebook_page_compare (gconstpointer a,
			   gconstpointer b)
{
  return (((GtkNotebookPage *) a)->child != b);
}

static GList*
gtk_notebook_find_child (GtkNotebook *notebook,
			 GtkWidget   *child,
			 const gchar *function)
{
  GList *list = g_list_find_custom (notebook->children, child,
				    gtk_notebook_page_compare);

#ifndef G_DISABLE_CHECKS
  if (!list && function)
    g_warning ("%s: unable to find child %p in notebook %p",
	       function, child, notebook);
#endif

  return list;
}

static void
gtk_notebook_remove_tab_label (GtkNotebook     *notebook,
			       GtkNotebookPage *page)
{
  if (page->tab_label)
    {
      if (page->mnemonic_activate_signal)
	g_signal_handler_disconnect (page->tab_label,
				     page->mnemonic_activate_signal);
      page->mnemonic_activate_signal = 0;

      gtk_widget_set_state (page->tab_label, GTK_STATE_NORMAL);
      gtk_widget_unparent (page->tab_label);
      page->tab_label = NULL;
    }
}

static void
gtk_notebook_real_remove (GtkNotebook *notebook,
			  GList       *list)
{
  GtkNotebookPrivate *priv;
  GtkNotebookPage *page;
  GList * next_list;
  gint need_resize = FALSE;
  GtkWidget *tab_label;

  gboolean destroying;

  priv = GTK_NOTEBOOK_GET_PRIVATE (notebook);
  destroying = GTK_OBJECT_FLAGS (notebook) & GTK_IN_DESTRUCTION;
  
  next_list = gtk_notebook_search_page (notebook, list, STEP_NEXT, TRUE);
  if (!next_list)
    next_list = gtk_notebook_search_page (notebook, list, STEP_PREV, TRUE);

  notebook->children = g_list_remove_link (notebook->children, list);

  if (notebook->cur_page == list->data)
    { 
      notebook->cur_page = NULL;
      if (next_list && !destroying)
	gtk_notebook_switch_page (notebook, GTK_NOTEBOOK_PAGE (next_list));
    }

  if (priv->detached_tab == list->data)
    priv->detached_tab = NULL;

  if (list == notebook->first_tab)
    notebook->first_tab = next_list;
  if (list == notebook->focus_tab && !destroying)
    gtk_notebook_switch_focus_tab (notebook, next_list);

  page = list->data;
  
  g_signal_handler_disconnect (page->child, page->notify_visible_handler); 

  if (gtk_widget_get_visible (page->child) &&
      gtk_widget_get_visible (GTK_WIDGET (notebook)))
    need_resize = TRUE;

  gtk_widget_unparent (page->child);

  tab_label = page->tab_label;
  if (tab_label)
    {
      g_object_ref (tab_label);
      gtk_notebook_remove_tab_label (notebook, page);
      if (destroying)
        gtk_widget_destroy (tab_label);
      g_object_unref (tab_label);
    }

  if (notebook->menu)
    {
      GtkWidget *parent = page->menu_label->parent;

      gtk_notebook_menu_label_unparent (parent, NULL);
      gtk_container_remove (GTK_CONTAINER (notebook->menu), parent);

      gtk_widget_queue_resize (notebook->menu);
    }
  if (!page->default_menu)
    g_object_unref (page->menu_label);

  g_list_free (list);

  if (page->last_focus_child)
    {
      g_object_remove_weak_pointer (G_OBJECT (page->last_focus_child), (gpointer *)&page->last_focus_child);
      page->last_focus_child = NULL;
    }
  
  g_slice_free (GtkNotebookPage, page);

  gtk_notebook_update_labels (notebook);
  if (need_resize)
    gtk_widget_queue_resize (GTK_WIDGET (notebook));
}

static void
gtk_notebook_update_labels (GtkNotebook *notebook)
{
  GtkNotebookPage *page;
  GList *list;
  gchar string[32];
  gint page_num = 1;

  if (!notebook->show_tabs && !notebook->menu)
    return;

  for (list = gtk_notebook_search_page (notebook, NULL, STEP_NEXT, FALSE);
       list;
       list = gtk_notebook_search_page (notebook, list, STEP_NEXT, FALSE))
    {
      page = list->data;
      g_snprintf (string, sizeof(string), _("Page %u"), page_num++);
      if (notebook->show_tabs)
	{
	  if (page->default_tab)
	    {
	      if (!page->tab_label)
		{
		  page->tab_label = gtk_label_new (string);
		  gtk_widget_set_parent (page->tab_label,
					 GTK_WIDGET (notebook));
		}
	      else
		gtk_label_set_text (GTK_LABEL (page->tab_label), string);
	    }

	  if (gtk_widget_get_visible (page->child) &&
	      !gtk_widget_get_visible (page->tab_label))
	    gtk_widget_show (page->tab_label);
	  else if (!gtk_widget_get_visible (page->child) &&
		   gtk_widget_get_visible (page->tab_label))
	    gtk_widget_hide (page->tab_label);
	}
      if (notebook->menu && page->default_menu)
	{
	  if (GTK_IS_LABEL (page->tab_label))
	    gtk_label_set_text (GTK_LABEL (page->menu_label),
                                GTK_LABEL (page->tab_label)->label);
	  else
	    gtk_label_set_text (GTK_LABEL (page->menu_label), string);
	}
    }  
}

static gint
gtk_notebook_real_page_position (GtkNotebook *notebook,
				 GList       *list)
{
  GList *work;
  gint count_start;

  for (work = notebook->children, count_start = 0;
       work && work != list; work = work->next)
    if (GTK_NOTEBOOK_PAGE (work)->pack == GTK_PACK_START)
      count_start++;

  if (!work)
    return -1;

  if (GTK_NOTEBOOK_PAGE (list)->pack == GTK_PACK_START)
    return count_start;

  return (count_start + g_list_length (list) - 1);
}

static GList *
gtk_notebook_search_page (GtkNotebook *notebook,
			  GList       *list,
			  gint         direction,
			  gboolean     find_visible)
{
  GtkNotebookPage *page = NULL;
  GList *old_list = NULL;
  gint flag = 0;

  switch (direction)
    {
    case STEP_PREV:
      flag = GTK_PACK_END;
      break;

    case STEP_NEXT:
      flag = GTK_PACK_START;
      break;
    }

  if (list)
    page = list->data;

  if (!page || page->pack == flag)
    {
      if (list)
	{
	  old_list = list;
	  list = list->next;
	}
      else
	list = notebook->children;

      while (list)
	{
	  page = list->data;
	  if (page->pack == flag &&
	      (!find_visible ||
	       (gtk_widget_get_visible (page->child) &&
		(!page->tab_label || NOTEBOOK_IS_TAB_LABEL_PARENT (notebook, page)))))
	    return list;
	  old_list = list;
	  list = list->next;
	}
      list = old_list;
    }
  else
    {
      old_list = list;
      list = list->prev;
    }
  while (list)
    {
      page = list->data;
      if (page->pack != flag &&
	  (!find_visible ||
	   (gtk_widget_get_visible (page->child) &&
	    (!page->tab_label || NOTEBOOK_IS_TAB_LABEL_PARENT (notebook, page)))))
	return list;
      old_list = list;
      list = list->prev;
    }
  return NULL;
}

/* Private GtkNotebook Drawing Functions:
 *
 * gtk_notebook_paint
 * gtk_notebook_draw_tab
 * gtk_notebook_draw_arrow
 */
static void
gtk_notebook_paint (GtkWidget    *widget,
		    GdkRectangle *area)
{
  GtkNotebook *notebook;
  GtkNotebookPrivate *priv;
  GtkNotebookPage *page;
  GList *children;
  gboolean showarrow;
  gint width, height;
  gint x, y;
  gint border_width = GTK_CONTAINER (widget)->border_width;
  gint gap_x = 0, gap_width = 0, step = STEP_PREV;
  gboolean is_rtl;
  gint tab_pos;
   
  if (!gtk_widget_is_drawable (widget))
    return;

  notebook = GTK_NOTEBOOK (widget);
  priv = GTK_NOTEBOOK_GET_PRIVATE (notebook);
  is_rtl = gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;
  tab_pos = get_effective_tab_pos (notebook);

  if ((!notebook->show_tabs && !notebook->show_border) ||
      !notebook->cur_page || !gtk_widget_get_visible (notebook->cur_page->child))
    return;

  x = widget->allocation.x + border_width;
  y = widget->allocation.y + border_width;
  width = widget->allocation.width - border_width * 2;
  height = widget->allocation.height - border_width * 2;

  if (notebook->show_border && (!notebook->show_tabs || !notebook->children))
    {
      gtk_paint_box (widget->style, widget->window,
		     GTK_STATE_NORMAL, GTK_SHADOW_OUT,
		     area, widget, "notebook",
		     x, y, width, height);
      return;
    }

  if (!notebook->first_tab)
    notebook->first_tab = notebook->children;

  if (!gtk_widget_get_mapped (notebook->cur_page->tab_label))
    page = GTK_NOTEBOOK_PAGE (notebook->first_tab);
  else
    page = notebook->cur_page;

  switch (tab_pos)
    {
    case GTK_POS_TOP:
      y += page->allocation.height;
      /* fall thru */
    case GTK_POS_BOTTOM:
      height -= page->allocation.height;
      break;
    case GTK_POS_LEFT:
      x += page->allocation.width;
      /* fall thru */
    case GTK_POS_RIGHT:
      width -= page->allocation.width;
      break;
    }

  if (!NOTEBOOK_IS_TAB_LABEL_PARENT (notebook, notebook->cur_page) ||
      !gtk_widget_get_mapped (notebook->cur_page->tab_label))
    {
      gap_x = 0;
      gap_width = 0;
    }
  else
    {
      switch (tab_pos)
	{
	case GTK_POS_TOP:
	case GTK_POS_BOTTOM:
	  if (priv->operation == DRAG_OPERATION_REORDER)
	    gap_x = priv->drag_window_x - widget->allocation.x - border_width;
	  else
	    gap_x = notebook->cur_page->allocation.x - widget->allocation.x - border_width;

	  gap_width = notebook->cur_page->allocation.width;
	  step = is_rtl ? STEP_NEXT : STEP_PREV;
	  break;
	case GTK_POS_LEFT:
	case GTK_POS_RIGHT:
	  if (priv->operation == DRAG_OPERATION_REORDER)
	    gap_x = priv->drag_window_y - border_width - widget->allocation.y;
	  else
	    gap_x = notebook->cur_page->allocation.y - widget->allocation.y - border_width;

	  gap_width = notebook->cur_page->allocation.height;
	  step = STEP_PREV;
	  break;
	}
    }
  gtk_paint_box_gap (widget->style, widget->window,
		     GTK_STATE_NORMAL, GTK_SHADOW_OUT,
		     area, widget, "notebook",
		     x, y, width, height,
		     tab_pos, gap_x, gap_width);

  showarrow = FALSE;
  children = gtk_notebook_search_page (notebook, NULL, step, TRUE);
  while (children)
    {
      page = children->data;
      children = gtk_notebook_search_page (notebook, children,
					   step, TRUE);
      if (!gtk_widget_get_visible (page->child))
	continue;
      if (!gtk_widget_get_mapped (page->tab_label))
	showarrow = TRUE;
      else if (page != notebook->cur_page)
	gtk_notebook_draw_tab (notebook, page, area);
    }

  if (showarrow && notebook->scrollable) 
    {
      if (notebook->has_before_previous)
	gtk_notebook_draw_arrow (notebook, ARROW_LEFT_BEFORE);
      if (notebook->has_before_next)
	gtk_notebook_draw_arrow (notebook, ARROW_RIGHT_BEFORE);
      if (notebook->has_after_previous)
	gtk_notebook_draw_arrow (notebook, ARROW_LEFT_AFTER);
      if (notebook->has_after_next)
	gtk_notebook_draw_arrow (notebook, ARROW_RIGHT_AFTER);
    }
  gtk_notebook_draw_tab (notebook, notebook->cur_page, area);
}

static void
gtk_notebook_draw_tab (GtkNotebook     *notebook,
		       GtkNotebookPage *page,
		       GdkRectangle    *area)
{
  GtkNotebookPrivate *priv;
  GdkRectangle child_area;
  GdkRectangle page_area;
  GtkStateType state_type;
  GtkPositionType gap_side;
  GdkWindow *window;
  GtkWidget *widget;
  
  if (!NOTEBOOK_IS_TAB_LABEL_PARENT (notebook, page) ||
      !gtk_widget_get_mapped (page->tab_label) ||
      (page->allocation.width == 0) || (page->allocation.height == 0))
    return;

  widget = GTK_WIDGET (notebook);
  priv = GTK_NOTEBOOK_GET_PRIVATE (notebook);

  if (priv->operation == DRAG_OPERATION_REORDER && page == notebook->cur_page)
    window = priv->drag_window;
  else
    window = widget->window;

  page_area.x = page->allocation.x;
  page_area.y = page->allocation.y;
  page_area.width = page->allocation.width;
  page_area.height = page->allocation.height;

  if (gdk_rectangle_intersect (&page_area, area, &child_area))
    {
      gap_side = get_tab_gap_pos (notebook);

      if (notebook->cur_page == page)
	state_type = GTK_STATE_NORMAL;
      else 
	state_type = GTK_STATE_ACTIVE;

      gtk_paint_extension (widget->style, window,
			   state_type, GTK_SHADOW_OUT,
			   area, widget, "tab",
			   page_area.x, page_area.y,
			   page_area.width, page_area.height,
			   gap_side);
    }
}

static void
gtk_notebook_draw_arrow (GtkNotebook      *notebook,
			 GtkNotebookArrow  nbarrow)
{
  GtkStateType state_type;
  GtkShadowType shadow_type;
  GtkWidget *widget;
  GdkRectangle arrow_rect;
  GtkArrowType arrow;
  gboolean is_rtl, left;

  widget = GTK_WIDGET (notebook);

  if (gtk_widget_is_drawable (widget))
    {
      gint scroll_arrow_hlength;
      gint scroll_arrow_vlength;
      gint arrow_size;

      gtk_notebook_get_arrow_rect (notebook, &arrow_rect, nbarrow);

      is_rtl = gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;
      left = (ARROW_IS_LEFT (nbarrow) && !is_rtl) ||
             (!ARROW_IS_LEFT (nbarrow) && is_rtl); 

      gtk_widget_style_get (widget,
                            "scroll-arrow-hlength", &scroll_arrow_hlength,
                            "scroll-arrow-vlength", &scroll_arrow_vlength,
                            NULL);

      if (notebook->in_child == nbarrow)
        {
          if (notebook->click_child == nbarrow)
            state_type = GTK_STATE_ACTIVE;
          else
            state_type = GTK_STATE_PRELIGHT;
        }
      else
        state_type = gtk_widget_get_state (widget);

      if (notebook->click_child == nbarrow)
        shadow_type = GTK_SHADOW_IN;
      else
        shadow_type = GTK_SHADOW_OUT;

      if (notebook->focus_tab &&
	  !gtk_notebook_search_page (notebook, notebook->focus_tab,
				     left ? STEP_PREV : STEP_NEXT, TRUE))
	{
	  shadow_type = GTK_SHADOW_ETCHED_IN;
	  state_type = GTK_STATE_INSENSITIVE;
	}
      
      if (notebook->tab_pos == GTK_POS_LEFT ||
	  notebook->tab_pos == GTK_POS_RIGHT)
        {
          arrow = (ARROW_IS_LEFT (nbarrow) ? GTK_ARROW_UP : GTK_ARROW_DOWN);
          arrow_size = scroll_arrow_vlength;
        }
      else
        {
          arrow = (ARROW_IS_LEFT (nbarrow) ? GTK_ARROW_LEFT : GTK_ARROW_RIGHT);
          arrow_size = scroll_arrow_hlength;
        }
     
      gtk_paint_arrow (widget->style, widget->window, state_type, 
		       shadow_type, NULL, widget, "notebook",
		       arrow, TRUE, arrow_rect.x, arrow_rect.y, 
		       arrow_size, arrow_size);
    }
}

/* Private GtkNotebook Size Allocate Functions:
 *
 * gtk_notebook_tab_space
 * gtk_notebook_calculate_shown_tabs
 * gtk_notebook_calculate_tabs_allocation
 * gtk_notebook_pages_allocate
 * gtk_notebook_page_allocate
 * gtk_notebook_calc_tabs
 */
static void
gtk_notebook_tab_space (GtkNotebook *notebook,
			gboolean    *show_arrows,
			gint        *min,
			gint        *max,
			gint        *tab_space)
{
  GtkNotebookPrivate *priv;
  GtkWidget *widget;
  GList *children;
  gint tab_pos = get_effective_tab_pos (notebook);
  gint tab_overlap;
  gint arrow_spacing;
  gint scroll_arrow_hlength;
  gint scroll_arrow_vlength;
  gboolean is_rtl;
  gint i;

  widget = GTK_WIDGET (notebook);
  priv = GTK_NOTEBOOK_GET_PRIVATE (notebook);
  children = notebook->children;
  is_rtl = gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;

  gtk_widget_style_get (GTK_WIDGET (notebook),
                        "arrow-spacing", &arrow_spacing,
                        "scroll-arrow-hlength", &scroll_arrow_hlength,
                        "scroll-arrow-vlength", &scroll_arrow_vlength,
                        NULL);

  switch (tab_pos)
    {
    case GTK_POS_TOP:
    case GTK_POS_BOTTOM:
      *min = widget->allocation.x + GTK_CONTAINER (notebook)->border_width;
      *max = widget->allocation.x + widget->allocation.width - GTK_CONTAINER (notebook)->border_width;

      for (i = 0; i < N_ACTION_WIDGETS; i++)
        {
          if (priv->action_widget[i])
            {
              if ((i == ACTION_WIDGET_START && !is_rtl) ||
                  (i == ACTION_WIDGET_END && is_rtl))
                *min += priv->action_widget[i]->allocation.width + widget->style->xthickness;
              else
                *max -= priv->action_widget[i]->allocation.width + widget->style->xthickness;
            }
        }

      while (children)
	{
          GtkNotebookPage *page;

	  page = children->data;
	  children = children->next;

	  if (NOTEBOOK_IS_TAB_LABEL_PARENT (notebook, page) &&
	      gtk_widget_get_visible (page->child))
	    *tab_space += page->requisition.width;
	}
      break;
    case GTK_POS_RIGHT:
    case GTK_POS_LEFT:
      *min = widget->allocation.y + GTK_CONTAINER (notebook)->border_width;
      *max = widget->allocation.y + widget->allocation.height - GTK_CONTAINER (notebook)->border_width;

      for (i = 0; i < N_ACTION_WIDGETS; i++)
        {
          if (priv->action_widget[i])
            {
              if (i == ACTION_WIDGET_START)
                *min += priv->action_widget[i]->allocation.height + widget->style->ythickness;
              else
                *max -= priv->action_widget[i]->allocation.height + widget->style->ythickness;
            }
        }

      while (children)
	{
          GtkNotebookPage *page;

	  page = children->data;
	  children = children->next;

	  if (NOTEBOOK_IS_TAB_LABEL_PARENT (notebook, page) &&
	      gtk_widget_get_visible (page->child))
	    *tab_space += page->requisition.height;
	}
      break;
    }

  if (!notebook->scrollable)
    *show_arrows = FALSE;
  else
    {
      gtk_widget_style_get (widget, "tab-overlap", &tab_overlap, NULL);

      switch (tab_pos)
	{
	case GTK_POS_TOP:
	case GTK_POS_BOTTOM:
	  if (*tab_space > *max - *min - tab_overlap)
	    {
	      *show_arrows = TRUE;

	      /* take arrows into account */
	      *tab_space = *max - *min - tab_overlap;

	      if (notebook->has_after_previous)
		{
		  *tab_space -= arrow_spacing + scroll_arrow_hlength;
		  *max -= arrow_spacing + scroll_arrow_hlength;
		}

	      if (notebook->has_after_next)
		{
		  *tab_space -= arrow_spacing + scroll_arrow_hlength;
		  *max -= arrow_spacing + scroll_arrow_hlength;
		}

	      if (notebook->has_before_previous)
		{
		  *tab_space -= arrow_spacing + scroll_arrow_hlength;
		  *min += arrow_spacing + scroll_arrow_hlength;
		}

	      if (notebook->has_before_next)
		{
		  *tab_space -= arrow_spacing + scroll_arrow_hlength;
		  *min += arrow_spacing + scroll_arrow_hlength;
		}
	    }
	  break;
	case GTK_POS_LEFT:
	case GTK_POS_RIGHT:
	  if (*tab_space > *max - *min - tab_overlap)
	    {
	      *show_arrows = TRUE;

	      /* take arrows into account */
	      *tab_space = *max - *min - tab_overlap;

	      if (notebook->has_after_previous || notebook->has_after_next)
		{
		  *tab_space -= arrow_spacing + scroll_arrow_vlength;
		  *max -= arrow_spacing + scroll_arrow_vlength;
		}

	      if (notebook->has_before_previous || notebook->has_before_next)
		{
		  *tab_space -= arrow_spacing + scroll_arrow_vlength;
		  *min += arrow_spacing + scroll_arrow_vlength;
		}
	    }
	  break;
	}
    }
}

static void
gtk_notebook_calculate_shown_tabs (GtkNotebook *notebook,
				   gboolean     show_arrows,
				   gint         min,
				   gint         max,
				   gint         tab_space,
				   GList      **last_child,
				   gint        *n,
				   gint        *remaining_space)
{
  GtkWidget *widget;
  GtkContainer *container;
  GList *children;
  GtkNotebookPage *page;
  gint tab_pos, tab_overlap;
  
  widget = GTK_WIDGET (notebook);
  container = GTK_CONTAINER (notebook);
  gtk_widget_style_get (widget, "tab-overlap", &tab_overlap, NULL);
  tab_pos = get_effective_tab_pos (notebook);

  if (show_arrows) /* first_tab <- focus_tab */
    {
      *remaining_space = tab_space;

      if (NOTEBOOK_IS_TAB_LABEL_PARENT (notebook, notebook->cur_page) &&
	  gtk_widget_get_visible (notebook->cur_page->child))
	{
	  gtk_notebook_calc_tabs (notebook,
				  notebook->focus_tab,
				  &(notebook->focus_tab),
				  remaining_space, STEP_NEXT);
	}

      if (tab_space <= 0 || *remaining_space <= 0)
	{
	  /* show 1 tab */
	  notebook->first_tab = notebook->focus_tab;
	  *last_child = gtk_notebook_search_page (notebook, notebook->focus_tab,
						  STEP_NEXT, TRUE);
          page = notebook->first_tab->data;
          *remaining_space = tab_space - page->requisition.width;
          *n = 1;
	}
      else
	{
	  children = NULL;

	  if (notebook->first_tab && notebook->first_tab != notebook->focus_tab)
	    {
	      /* Is first_tab really predecessor of focus_tab? */
	      page = notebook->first_tab->data;
	      if (NOTEBOOK_IS_TAB_LABEL_PARENT (notebook, page) &&
		  gtk_widget_get_visible (page->child))
		for (children = notebook->focus_tab;
		     children && children != notebook->first_tab;
		     children = gtk_notebook_search_page (notebook,
							  children,
							  STEP_PREV,
							  TRUE));
	    }

	  if (!children)
	    {
	      if (NOTEBOOK_IS_TAB_LABEL_PARENT (notebook, notebook->cur_page))
		notebook->first_tab = notebook->focus_tab;
	      else
		notebook->first_tab = gtk_notebook_search_page (notebook, notebook->focus_tab,
								STEP_NEXT, TRUE);
	    }
	  else
	    /* calculate shown tabs counting backwards from the focus tab */
	    gtk_notebook_calc_tabs (notebook,
				    gtk_notebook_search_page (notebook,
							      notebook->focus_tab,
							      STEP_PREV,
							      TRUE),
				    &(notebook->first_tab), remaining_space,
				    STEP_PREV);

	  if (*remaining_space < 0)
	    {
	      notebook->first_tab =
		gtk_notebook_search_page (notebook, notebook->first_tab,
					  STEP_NEXT, TRUE);
	      if (!notebook->first_tab)
		notebook->first_tab = notebook->focus_tab;

	      *last_child = gtk_notebook_search_page (notebook, notebook->focus_tab,
						      STEP_NEXT, TRUE); 
	    }
	  else /* focus_tab -> end */   
	    {
	      if (!notebook->first_tab)
		notebook->first_tab = gtk_notebook_search_page (notebook,
								NULL,
								STEP_NEXT,
								TRUE);
	      children = NULL;
	      gtk_notebook_calc_tabs (notebook,
				      gtk_notebook_search_page (notebook,
								notebook->focus_tab,
								STEP_NEXT,
								TRUE),
				      &children, remaining_space, STEP_NEXT);

	      if (*remaining_space <= 0) 
		*last_child = children;
	      else /* start <- first_tab */
		{
		  *last_child = NULL;
		  children = NULL;

		  gtk_notebook_calc_tabs (notebook,
					  gtk_notebook_search_page (notebook,
								    notebook->first_tab,
								    STEP_PREV,
								    TRUE),
					  &children, remaining_space, STEP_PREV);

		  if (*remaining_space == 0)
		    notebook->first_tab = children;
		  else
		    notebook->first_tab = gtk_notebook_search_page(notebook,
								   children,
								   STEP_NEXT,
								   TRUE);
		}
	    }

          if (*remaining_space < 0)
            {
              /* calculate number of tabs */
              *remaining_space = - (*remaining_space);
              *n = 0;

              for (children = notebook->first_tab;
                   children && children != *last_child;
                   children = gtk_notebook_search_page (notebook, children,
                                                        STEP_NEXT, TRUE))
                (*n)++;
	    }
          else
	    *remaining_space = 0;
        }

      /* unmap all non-visible tabs */
      for (children = gtk_notebook_search_page (notebook, NULL,
						STEP_NEXT, TRUE);
	   children && children != notebook->first_tab;
	   children = gtk_notebook_search_page (notebook, children,
						STEP_NEXT, TRUE))
	{
	  page = children->data;

	  if (page->tab_label &&
	      NOTEBOOK_IS_TAB_LABEL_PARENT (notebook, page))
	    gtk_widget_set_child_visible (page->tab_label, FALSE);
	}

      for (children = *last_child; children;
	   children = gtk_notebook_search_page (notebook, children,
						STEP_NEXT, TRUE))
	{
	  page = children->data;

	  if (page->tab_label &&
	      NOTEBOOK_IS_TAB_LABEL_PARENT (notebook, page))
	    gtk_widget_set_child_visible (page->tab_label, FALSE);
	}
    }
  else /* !show_arrows */
    {
      gint c = 0;
      *n = 0;

      *remaining_space = max - min - tab_overlap - tab_space;
      children = notebook->children;
      notebook->first_tab = gtk_notebook_search_page (notebook, NULL,
						      STEP_NEXT, TRUE);
      while (children)
	{
	  page = children->data;
	  children = children->next;

	  if (!NOTEBOOK_IS_TAB_LABEL_PARENT (notebook, page) ||
	      !gtk_widget_get_visible (page->child))
	    continue;

	  c++;

	  if (page->expand)
	    (*n)++;
	}

      /* if notebook is homogeneous, all tabs are expanded */
      if (notebook->homogeneous && *n)
	*n = c;
    }
}

static gboolean
get_allocate_at_bottom (GtkWidget *widget,
			gint       search_direction)
{
  gboolean is_rtl = (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL);
  gboolean tab_pos = get_effective_tab_pos (GTK_NOTEBOOK (widget));

  switch (tab_pos)
    {
    case GTK_POS_TOP:
    case GTK_POS_BOTTOM:
      if (!is_rtl)
	return (search_direction == STEP_PREV);
      else
	return (search_direction == STEP_NEXT);

      break;
    case GTK_POS_RIGHT:
    case GTK_POS_LEFT:
      return (search_direction == STEP_PREV);
      break;
    }

  return FALSE;
}

static void
gtk_notebook_calculate_tabs_allocation (GtkNotebook  *notebook,
					GList       **children,
					GList        *last_child,
					gboolean      showarrow,
					gint          direction,
					gint         *remaining_space,
					gint         *expanded_tabs,
					gint          min,
					gint          max)
{
  GtkWidget *widget;
  GtkContainer *container;
  GtkNotebookPrivate *priv;
  GtkNotebookPage *page;
  gboolean allocate_at_bottom;
  gint tab_overlap, tab_pos, tab_extra_space;
  gint left_x, right_x, top_y, bottom_y, anchor;
  gint xthickness, ythickness;
  gboolean gap_left, packing_changed;
  GtkAllocation child_allocation = { 0, };

  widget = GTK_WIDGET (notebook);
  container = GTK_CONTAINER (notebook);
  priv = GTK_NOTEBOOK_GET_PRIVATE (notebook);
  gtk_widget_style_get (widget, "tab-overlap", &tab_overlap, NULL);
  tab_pos = get_effective_tab_pos (notebook);
  allocate_at_bottom = get_allocate_at_bottom (widget, direction);
  anchor = 0;

  child_allocation.x = widget->allocation.x + container->border_width;
  child_allocation.y = widget->allocation.y + container->border_width;

  xthickness = widget->style->xthickness;
  ythickness = widget->style->ythickness;

  switch (tab_pos)
    {
    case GTK_POS_BOTTOM:
      child_allocation.y = widget->allocation.y + widget->allocation.height -
	notebook->cur_page->requisition.height - container->border_width;
      /* fall through */
    case GTK_POS_TOP:
      child_allocation.x = (allocate_at_bottom) ? max : min;
      child_allocation.height = notebook->cur_page->requisition.height;
      anchor = child_allocation.x;
      break;
      
    case GTK_POS_RIGHT:
      child_allocation.x = widget->allocation.x + widget->allocation.width -
	notebook->cur_page->requisition.width - container->border_width;
      /* fall through */
    case GTK_POS_LEFT:
      child_allocation.y = (allocate_at_bottom) ? max : min;
      child_allocation.width = notebook->cur_page->requisition.width;
      anchor = child_allocation.y;
      break;
    }

  left_x   = CLAMP (priv->mouse_x - priv->drag_offset_x,
		    min, max - notebook->cur_page->allocation.width);
  top_y    = CLAMP (priv->mouse_y - priv->drag_offset_y,
		    min, max - notebook->cur_page->allocation.height);
  right_x  = left_x + notebook->cur_page->allocation.width;
  bottom_y = top_y + notebook->cur_page->allocation.height;
  gap_left = packing_changed = FALSE;

  while (*children && *children != last_child)
    {
      page = (*children)->data;

      if (direction == STEP_NEXT && page->pack != GTK_PACK_START)
	{
	  if (!showarrow)
	    break;
	  else if (priv->operation == DRAG_OPERATION_REORDER)
	    packing_changed = TRUE;
	}

      if (direction == STEP_NEXT)
	*children = gtk_notebook_search_page (notebook, *children, direction, TRUE);
      else
	{
	  *children = (*children)->next;

          if (page->pack != GTK_PACK_END || !gtk_widget_get_visible (page->child))
	    continue;
	}

      if (!NOTEBOOK_IS_TAB_LABEL_PARENT (notebook, page))
	continue;

      tab_extra_space = 0;
      if (*expanded_tabs && (showarrow || page->expand || notebook->homogeneous))
	{
	  tab_extra_space = *remaining_space / *expanded_tabs;
	  *remaining_space -= tab_extra_space;
	  (*expanded_tabs)--;
	}

      switch (tab_pos)
	{
	case GTK_POS_TOP:
	case GTK_POS_BOTTOM:
	  child_allocation.width = page->requisition.width + tab_overlap + tab_extra_space;

	  /* make sure that the reordered tab doesn't go past the last position */
	  if (priv->operation == DRAG_OPERATION_REORDER &&
	      !gap_left && packing_changed)
	    {
	      if (!allocate_at_bottom)
		{
		  if ((notebook->cur_page->pack == GTK_PACK_START && left_x >= anchor) ||
		      (notebook->cur_page->pack == GTK_PACK_END && left_x < anchor))
		    {
		      left_x = priv->drag_window_x = anchor;
		      anchor += notebook->cur_page->allocation.width - tab_overlap;
		    }
		}
	      else
		{
		  if ((notebook->cur_page->pack == GTK_PACK_START && right_x <= anchor) ||
		      (notebook->cur_page->pack == GTK_PACK_END && right_x > anchor))
		    {
		      anchor -= notebook->cur_page->allocation.width;
		      left_x = priv->drag_window_x = anchor;
		      anchor += tab_overlap;
		    }
		}

	      gap_left = TRUE;
	    }

	  if (priv->operation == DRAG_OPERATION_REORDER && page == notebook->cur_page)
	    {
	      priv->drag_window_x = left_x;
	      priv->drag_window_y = child_allocation.y;
	    }
 	  else
 	    {
 	      if (allocate_at_bottom)
		anchor -= child_allocation.width;
 
 	      if (priv->operation == DRAG_OPERATION_REORDER && page->pack == notebook->cur_page->pack)
 		{
 		  if (!allocate_at_bottom &&
 		      left_x >= anchor &&
 		      left_x <= anchor + child_allocation.width / 2)
 		    anchor += notebook->cur_page->allocation.width - tab_overlap;
 		  else if (allocate_at_bottom &&
 			   right_x >= anchor + child_allocation.width / 2 &&
 			   right_x <= anchor + child_allocation.width)
 		    anchor -= notebook->cur_page->allocation.width - tab_overlap;
 		}

	      child_allocation.x = anchor;
 	    }

	  break;
	case GTK_POS_LEFT:
	case GTK_POS_RIGHT:
	  child_allocation.height = page->requisition.height + tab_overlap + tab_extra_space;

	  /* make sure that the reordered tab doesn't go past the last position */
	  if (priv->operation == DRAG_OPERATION_REORDER &&
	      !gap_left && packing_changed)
	    {
	      if (!allocate_at_bottom &&
		  ((notebook->cur_page->pack == GTK_PACK_START && top_y >= anchor) ||
		   (notebook->cur_page->pack == GTK_PACK_END && top_y < anchor)))
		{
		  top_y = priv->drag_window_y = anchor;
		  anchor += notebook->cur_page->allocation.height - tab_overlap;
		}
 
	      gap_left = TRUE;
	    }

	  if (priv->operation == DRAG_OPERATION_REORDER && page == notebook->cur_page)
	    {
	      priv->drag_window_x = child_allocation.x;
	      priv->drag_window_y = top_y;
	    }
 	  else
 	    {
	      if (allocate_at_bottom)
		anchor -= child_allocation.height;

 	      if (priv->operation == DRAG_OPERATION_REORDER && page->pack == notebook->cur_page->pack)
		{
		  if (!allocate_at_bottom &&
		      top_y >= anchor &&
		      top_y <= anchor + child_allocation.height / 2)
		    anchor += notebook->cur_page->allocation.height - tab_overlap;
		  else if (allocate_at_bottom &&
			   bottom_y >= anchor + child_allocation.height / 2 &&
			   bottom_y <= anchor + child_allocation.height)
		    anchor -= notebook->cur_page->allocation.height - tab_overlap;
		}

	      child_allocation.y = anchor;
 	    }

	  break;
	}

      page->allocation = child_allocation;

      if ((page == priv->detached_tab && priv->operation == DRAG_OPERATION_DETACH) ||
	  (page == notebook->cur_page && priv->operation == DRAG_OPERATION_REORDER))
	{
	  /* needs to be allocated at 0,0
	   * to be shown in the drag window */
	  page->allocation.x = 0;
	  page->allocation.y = 0;
	}
      
      if (page != notebook->cur_page)
	{
	  switch (tab_pos)
	    {
	    case GTK_POS_TOP:
	      page->allocation.y += ythickness;
	      /* fall through */
	    case GTK_POS_BOTTOM:
	      page->allocation.height = MAX (1, page->allocation.height - ythickness);
	      break;
	    case GTK_POS_LEFT:
	      page->allocation.x += xthickness;
	      /* fall through */
	    case GTK_POS_RIGHT:
	      page->allocation.width = MAX (1, page->allocation.width - xthickness);
	      break;
	    }
	}

      /* calculate whether to leave a gap based on reorder operation or not */
      switch (tab_pos)
	{
	case GTK_POS_TOP:
	case GTK_POS_BOTTOM:
 	  if (priv->operation != DRAG_OPERATION_REORDER ||
	      (priv->operation == DRAG_OPERATION_REORDER && page != notebook->cur_page))
 	    {
 	      if (priv->operation == DRAG_OPERATION_REORDER)
 		{
 		  if (page->pack == notebook->cur_page->pack &&
 		      !allocate_at_bottom &&
 		      left_x >  anchor + child_allocation.width / 2 &&
 		      left_x <= anchor + child_allocation.width)
 		    anchor += notebook->cur_page->allocation.width - tab_overlap;
 		  else if (page->pack == notebook->cur_page->pack &&
 			   allocate_at_bottom &&
 			   right_x >= anchor &&
 			   right_x <= anchor + child_allocation.width / 2)
 		    anchor -= notebook->cur_page->allocation.width - tab_overlap;
 		}
 
 	      if (!allocate_at_bottom)
 		anchor += child_allocation.width - tab_overlap;
 	      else
 		anchor += tab_overlap;
 	    }

	  break;
	case GTK_POS_LEFT:
	case GTK_POS_RIGHT:
 	  if (priv->operation != DRAG_OPERATION_REORDER  ||
	      (priv->operation == DRAG_OPERATION_REORDER && page != notebook->cur_page))
 	    {
 	      if (priv->operation == DRAG_OPERATION_REORDER)
		{
		  if (page->pack == notebook->cur_page->pack &&
		      !allocate_at_bottom &&
		      top_y >= anchor + child_allocation.height / 2 &&
		      top_y <= anchor + child_allocation.height)
		    anchor += notebook->cur_page->allocation.height - tab_overlap;
		  else if (page->pack == notebook->cur_page->pack &&
			   allocate_at_bottom &&
			   bottom_y >= anchor &&
			   bottom_y <= anchor + child_allocation.height / 2)
		    anchor -= notebook->cur_page->allocation.height - tab_overlap;
		}

	      if (!allocate_at_bottom)
		anchor += child_allocation.height - tab_overlap;
	      else
		anchor += tab_overlap;
 	    }

	  break;
	}

      /* set child visible */
      if (page->tab_label)
	gtk_widget_set_child_visible (page->tab_label, TRUE);
    }

  /* Don't move the current tab past the last position during tabs reordering */
  if (children &&
      priv->operation == DRAG_OPERATION_REORDER &&
      ((direction == STEP_NEXT && notebook->cur_page->pack == GTK_PACK_START) ||
       ((direction == STEP_PREV || packing_changed) && notebook->cur_page->pack == GTK_PACK_END)))
    {
      switch (tab_pos)
	{
	case GTK_POS_TOP:
	case GTK_POS_BOTTOM:
	  if (allocate_at_bottom)
	    anchor -= notebook->cur_page->allocation.width;

	  if ((!allocate_at_bottom && priv->drag_window_x > anchor) ||
	      (allocate_at_bottom && priv->drag_window_x < anchor))
	    priv->drag_window_x = anchor;
	  break;
	case GTK_POS_LEFT:
	case GTK_POS_RIGHT:
	  if (allocate_at_bottom)
	    anchor -= notebook->cur_page->allocation.height;

	  if ((!allocate_at_bottom && priv->drag_window_y > anchor) ||
	      (allocate_at_bottom && priv->drag_window_y < anchor))
	    priv->drag_window_y = anchor;
	  break;
	}
    }
}

static void
gtk_notebook_pages_allocate (GtkNotebook *notebook)
{
  GList *children = NULL;
  GList *last_child = NULL;
  gboolean showarrow = FALSE;
  gint tab_space, min, max, remaining_space;
  gint expanded_tabs, operation;
  gboolean tab_allocations_changed = FALSE;

  if (!notebook->show_tabs || !notebook->children || !notebook->cur_page)
    return;

  min = max = tab_space = remaining_space = 0;
  expanded_tabs = 1;

  gtk_notebook_tab_space (notebook, &showarrow,
			  &min, &max, &tab_space);

  gtk_notebook_calculate_shown_tabs (notebook, showarrow,
				     min, max, tab_space, &last_child,
				     &expanded_tabs, &remaining_space);

  children = notebook->first_tab;
  gtk_notebook_calculate_tabs_allocation (notebook, &children, last_child,
					  showarrow, STEP_NEXT,
					  &remaining_space, &expanded_tabs, min, max);
  if (children && children != last_child)
    {
      children = notebook->children;
      gtk_notebook_calculate_tabs_allocation (notebook, &children, last_child,
					      showarrow, STEP_PREV,
					      &remaining_space, &expanded_tabs, min, max);
    }

  children = notebook->children;

  while (children)
    {
      if (gtk_notebook_page_allocate (notebook, GTK_NOTEBOOK_PAGE (children)))
	tab_allocations_changed = TRUE;
      children = children->next;
    }

  operation = GTK_NOTEBOOK_GET_PRIVATE (notebook)->operation;

  if (!notebook->first_tab)
    notebook->first_tab = notebook->children;

  if (tab_allocations_changed)
    gtk_notebook_redraw_tabs (notebook);
}

static gboolean
gtk_notebook_page_allocate (GtkNotebook     *notebook,
			    GtkNotebookPage *page)
{
  GtkWidget *widget = GTK_WIDGET (notebook);
  GtkAllocation child_allocation;
  GtkRequisition tab_requisition;
  gint xthickness;
  gint ythickness;
  gint padding;
  gint focus_width;
  gint tab_curvature;
  gint tab_pos = get_effective_tab_pos (notebook);
  gboolean tab_allocation_changed;
  gboolean was_visible = page->tab_allocated_visible;

  if (!page->tab_label ||
      !gtk_widget_get_visible (page->tab_label) ||
      !gtk_widget_get_child_visible (page->tab_label))
    {
      page->tab_allocated_visible = FALSE;
      return was_visible;
    }

  xthickness = widget->style->xthickness;
  ythickness = widget->style->ythickness;

  gtk_widget_get_child_requisition (page->tab_label, &tab_requisition);
  gtk_widget_style_get (widget,
			"focus-line-width", &focus_width,
			"tab-curvature", &tab_curvature,
			NULL);
  switch (tab_pos)
    {
    case GTK_POS_TOP:
    case GTK_POS_BOTTOM:
      padding = tab_curvature + focus_width + notebook->tab_hborder;
      if (page->fill)
	{
	  child_allocation.x = xthickness + focus_width + notebook->tab_hborder;
	  child_allocation.width = MAX (1, page->allocation.width - 2 * child_allocation.x);
	  child_allocation.x += page->allocation.x;
	}
      else
	{
	  child_allocation.x = page->allocation.x +
	    (page->allocation.width - tab_requisition.width) / 2;

	  child_allocation.width = tab_requisition.width;
	}

      child_allocation.y = notebook->tab_vborder + focus_width + page->allocation.y;

      if (tab_pos == GTK_POS_TOP)
	child_allocation.y += ythickness;

      child_allocation.height = MAX (1, (page->allocation.height - ythickness -
					 2 * (notebook->tab_vborder + focus_width)));
      break;
    case GTK_POS_LEFT:
    case GTK_POS_RIGHT:
      padding = tab_curvature + focus_width + notebook->tab_vborder;
      if (page->fill)
	{
	  child_allocation.y = ythickness + padding;
	  child_allocation.height = MAX (1, (page->allocation.height -
					     2 * child_allocation.y));
	  child_allocation.y += page->allocation.y;
	}
      else
	{
	  child_allocation.y = page->allocation.y +
	    (page->allocation.height - tab_requisition.height) / 2;

	  child_allocation.height = tab_requisition.height;
	}

      child_allocation.x = notebook->tab_hborder + focus_width + page->allocation.x;

      if (tab_pos == GTK_POS_LEFT)
	child_allocation.x += xthickness;

      child_allocation.width = MAX (1, (page->allocation.width - xthickness -
					2 * (notebook->tab_hborder + focus_width)));
      break;
    }

  tab_allocation_changed = (child_allocation.x != page->tab_label->allocation.x ||
			    child_allocation.y != page->tab_label->allocation.y ||
			    child_allocation.width != page->tab_label->allocation.width ||
			    child_allocation.height != page->tab_label->allocation.height);

  gtk_widget_size_allocate (page->tab_label, &child_allocation);

  if (!was_visible)
    {
      page->tab_allocated_visible = TRUE;
      tab_allocation_changed = TRUE;
    }

  return tab_allocation_changed;
}

static void 
gtk_notebook_calc_tabs (GtkNotebook  *notebook,
			GList        *start,
                        GList       **end,
			gint         *tab_space,
                        guint         direction)
{
  GtkNotebookPage *page = NULL;
  GList *children;
  GList *last_list = NULL;
  GList *last_calculated_child = NULL;
  gboolean pack;
  gint tab_pos = get_effective_tab_pos (notebook);
  guint real_direction;

  if (!start)
    return;

  children = start;
  pack = GTK_NOTEBOOK_PAGE (start)->pack;
  if (pack == GTK_PACK_END)
    real_direction = (direction == STEP_PREV) ? STEP_NEXT : STEP_PREV;
  else
    real_direction = direction;

  while (1)
    {
      switch (tab_pos)
	{
	case GTK_POS_TOP:
	case GTK_POS_BOTTOM:
	  while (children)
	    {
	      page = children->data;
	      if (NOTEBOOK_IS_TAB_LABEL_PARENT (notebook, page) &&
		  gtk_widget_get_visible (page->child))
		{
		  if (page->pack == pack)
		    {
		      *tab_space -= page->requisition.width;
		      if (*tab_space < 0 || children == *end)
			{
			  if (*tab_space < 0) 
			    {
			      *tab_space = - (*tab_space +
					      page->requisition.width);

			      if (*tab_space == 0 && direction == STEP_PREV)
				children = last_calculated_child;

			      *end = children;
			    }
			  return;
			}

		      last_calculated_child = children;
		    }
		  last_list = children;
		}
	      if (real_direction == STEP_NEXT)
		children = children->next;
	      else
		children = children->prev;
	    }
	  break;
	case GTK_POS_LEFT:
	case GTK_POS_RIGHT:
	  while (children)
	    {
	      page = children->data;
	      if (NOTEBOOK_IS_TAB_LABEL_PARENT (notebook, page) &&
		  gtk_widget_get_visible (page->child))
		{
		  if (page->pack == pack)
		    {
		      *tab_space -= page->requisition.height;
		      if (*tab_space < 0 || children == *end)
			{
			  if (*tab_space < 0)
			    {
			      *tab_space = - (*tab_space +
					      page->requisition.height);

			      if (*tab_space == 0 && direction == STEP_PREV)
				children = last_calculated_child;

			      *end = children;
			    }
			  return;
			}

		      last_calculated_child = children;
		    }
		  last_list = children;
		}
	      if (real_direction == STEP_NEXT)
		children = children->next;
	      else
		children = children->prev;
	    }
	  break;
	}
      if (real_direction == STEP_PREV)
	return;
      pack = (pack == GTK_PACK_END) ? GTK_PACK_START : GTK_PACK_END;
      real_direction = STEP_PREV;
      children = last_list;
    }
}

static void
gtk_notebook_update_tab_states (GtkNotebook *notebook)
{
  GList *list;

  for (list = notebook->children; list != NULL; list = list->next)
    {
      GtkNotebookPage *page = list->data;
      
      if (page->tab_label)
	{
	  if (page == notebook->cur_page)
	    gtk_widget_set_state (page->tab_label, GTK_STATE_NORMAL);
	  else
	    gtk_widget_set_state (page->tab_label, GTK_STATE_ACTIVE);
	}
    }
}

/* Private GtkNotebook Page Switch Methods:
 *
 * gtk_notebook_real_switch_page
 */
static void
gtk_notebook_real_switch_page (GtkNotebook     *notebook,
			       GtkNotebookPage* child,
			       guint            page_num)
{
  GList *list = gtk_notebook_find_child (notebook, GTK_WIDGET (child), NULL);
  GtkNotebookPage *page = GTK_NOTEBOOK_PAGE (list);
  gboolean child_has_focus;

  if (notebook->cur_page == page || !gtk_widget_get_visible (GTK_WIDGET (child)))
    return;

  /* save the value here, changing visibility changes focus */
  child_has_focus = notebook->child_has_focus;

  if (notebook->cur_page)
    gtk_widget_set_child_visible (notebook->cur_page->child, FALSE);

  notebook->cur_page = page;

  if (!notebook->focus_tab ||
      notebook->focus_tab->data != (gpointer) notebook->cur_page)
    notebook->focus_tab = 
      g_list_find (notebook->children, notebook->cur_page);

  gtk_widget_set_child_visible (notebook->cur_page->child, TRUE);

  /* If the focus was on the previous page, move it to the first
   * element on the new page, if possible, or if not, to the
   * notebook itself.
   */
  if (child_has_focus)
    {
      if (notebook->cur_page->last_focus_child &&
	  gtk_widget_is_ancestor (notebook->cur_page->last_focus_child, notebook->cur_page->child))
	gtk_widget_grab_focus (notebook->cur_page->last_focus_child);
      else
	if (!gtk_widget_child_focus (notebook->cur_page->child, GTK_DIR_TAB_FORWARD))
	  gtk_widget_grab_focus (GTK_WIDGET (notebook));
    }
  
  gtk_notebook_update_tab_states (notebook);
  gtk_widget_queue_resize (GTK_WIDGET (notebook));
  g_object_notify (G_OBJECT (notebook), "page");
}

/* Private GtkNotebook Page Switch Functions:
 *
 * gtk_notebook_switch_page
 * gtk_notebook_page_select
 * gtk_notebook_switch_focus_tab
 * gtk_notebook_menu_switch_page
 */
static void
gtk_notebook_switch_page (GtkNotebook     *notebook,
			  GtkNotebookPage *page)
{ 
  guint page_num;

  if (notebook->cur_page == page)
    return;

  page_num = g_list_index (notebook->children, page);

  g_signal_emit (notebook,
		 notebook_signals[SWITCH_PAGE],
		 0,
		 page->child,
		 page_num);
}

static gint
gtk_notebook_page_select (GtkNotebook *notebook,
			  gboolean     move_focus)
{
  GtkNotebookPage *page;
  GtkDirectionType dir = GTK_DIR_DOWN; /* Quiet GCC */
  gint tab_pos = get_effective_tab_pos (notebook);

  if (!notebook->focus_tab)
    return FALSE;

  page = notebook->focus_tab->data;
  gtk_notebook_switch_page (notebook, page);

  if (move_focus)
    {
      switch (tab_pos)
	{
	case GTK_POS_TOP:
	  dir = GTK_DIR_DOWN;
	  break;
	case GTK_POS_BOTTOM:
	  dir = GTK_DIR_UP;
	  break;
	case GTK_POS_LEFT:
	  dir = GTK_DIR_RIGHT;
	  break;
	case GTK_POS_RIGHT:
	  dir = GTK_DIR_LEFT;
	  break;
	}

      if (gtk_widget_child_focus (page->child, dir))
        return TRUE;
    }
  return FALSE;
}

static void
gtk_notebook_switch_focus_tab (GtkNotebook *notebook, 
			       GList       *new_child)
{
  GList *old_child;
  GtkNotebookPage *page;

  if (notebook->focus_tab == new_child)
    return;

  old_child = notebook->focus_tab;
  notebook->focus_tab = new_child;

  if (notebook->scrollable)
    gtk_notebook_redraw_arrows (notebook);

  if (!notebook->show_tabs || !notebook->focus_tab)
    return;

  page = notebook->focus_tab->data;
  if (gtk_widget_get_mapped (page->tab_label))
    gtk_notebook_redraw_tabs (notebook);
  else
    gtk_notebook_pages_allocate (notebook);

  gtk_notebook_switch_page (notebook, page);
}

static void
gtk_notebook_menu_switch_page (GtkWidget       *widget,
			       GtkNotebookPage *page)
{
  GtkNotebook *notebook;
  GList *children;
  guint page_num;

  notebook = GTK_NOTEBOOK (gtk_menu_get_attach_widget
			   (GTK_MENU (widget->parent)));

  if (notebook->cur_page == page)
    return;

  page_num = 0;
  children = notebook->children;
  while (children && children->data != page)
    {
      children = children->next;
      page_num++;
    }

  g_signal_emit (notebook,
		 notebook_signals[SWITCH_PAGE],
		 0,
		 page->child,
		 page_num);
}

/* Private GtkNotebook Menu Functions:
 *
 * gtk_notebook_menu_item_create
 * gtk_notebook_menu_label_unparent
 * gtk_notebook_menu_detacher
 */
static void
gtk_notebook_menu_item_create (GtkNotebook *notebook, 
			       GList       *list)
{	
  GtkNotebookPage *page;
  GtkWidget *menu_item;

  page = list->data;
  if (page->default_menu)
    {
      if (GTK_IS_LABEL (page->tab_label))
	page->menu_label = gtk_label_new (GTK_LABEL (page->tab_label)->label);
      else
	page->menu_label = gtk_label_new ("");
      gtk_misc_set_alignment (GTK_MISC (page->menu_label), 0.0, 0.5);
    }

  gtk_widget_show (page->menu_label);
  menu_item = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menu_item), page->menu_label);
  gtk_menu_shell_insert (GTK_MENU_SHELL (notebook->menu), menu_item,
			 gtk_notebook_real_page_position (notebook, list));
  g_signal_connect (menu_item, "activate",
		    G_CALLBACK (gtk_notebook_menu_switch_page), page);
  if (gtk_widget_get_visible (page->child))
    gtk_widget_show (menu_item);
}

static void
gtk_notebook_menu_label_unparent (GtkWidget *widget, 
				  gpointer  data)
{
  gtk_widget_unparent (GTK_BIN (widget)->child);
  GTK_BIN (widget)->child = NULL;
}

static void
gtk_notebook_menu_detacher (GtkWidget *widget,
			    GtkMenu   *menu)
{
  GtkNotebook *notebook;

  notebook = GTK_NOTEBOOK (widget);
  g_return_if_fail (notebook->menu == (GtkWidget*) menu);

  notebook->menu = NULL;
}

/* Private GtkNotebook Setter Functions:
 *
 * gtk_notebook_set_homogeneous_tabs_internal
 * gtk_notebook_set_tab_border_internal
 * gtk_notebook_set_tab_hborder_internal
 * gtk_notebook_set_tab_vborder_internal
 */
static void
gtk_notebook_set_homogeneous_tabs_internal (GtkNotebook *notebook,
				            gboolean     homogeneous)
{
  if (homogeneous == notebook->homogeneous)
    return;

  notebook->homogeneous = homogeneous;
  gtk_widget_queue_resize (GTK_WIDGET (notebook));

  g_object_notify (G_OBJECT (notebook), "homogeneous");
}

static void
gtk_notebook_set_tab_border_internal (GtkNotebook *notebook,
				      guint        border_width)
{
  notebook->tab_hborder = border_width;
  notebook->tab_vborder = border_width;

  if (notebook->show_tabs &&
      gtk_widget_get_visible (GTK_WIDGET (notebook)))
    gtk_widget_queue_resize (GTK_WIDGET (notebook));

  g_object_freeze_notify (G_OBJECT (notebook));
  g_object_notify (G_OBJECT (notebook), "tab-hborder");
  g_object_notify (G_OBJECT (notebook), "tab-vborder");
  g_object_thaw_notify (G_OBJECT (notebook));
}

static void
gtk_notebook_set_tab_hborder_internal (GtkNotebook *notebook,
				       guint        tab_hborder)
{
  if (notebook->tab_hborder == tab_hborder)
    return;

  notebook->tab_hborder = tab_hborder;

  if (notebook->show_tabs &&
      gtk_widget_get_visible (GTK_WIDGET (notebook)))
    gtk_widget_queue_resize (GTK_WIDGET (notebook));

  g_object_notify (G_OBJECT (notebook), "tab-hborder");
}

static void
gtk_notebook_set_tab_vborder_internal (GtkNotebook *notebook,
				       guint        tab_vborder)
{
  if (notebook->tab_vborder == tab_vborder)
    return;

  notebook->tab_vborder = tab_vborder;

  if (notebook->show_tabs &&
      gtk_widget_get_visible (GTK_WIDGET (notebook)))
    gtk_widget_queue_resize (GTK_WIDGET (notebook));

  g_object_notify (G_OBJECT (notebook), "tab-vborder");
}

/* Public GtkNotebook Page Insert/Remove Methods :
 *
 * gtk_notebook_append_page
 * gtk_notebook_append_page_menu
 * gtk_notebook_prepend_page
 * gtk_notebook_prepend_page_menu
 * gtk_notebook_insert_page
 * gtk_notebook_insert_page_menu
 * gtk_notebook_remove_page
 */
/**
 * gtk_notebook_append_page:
 * @notebook: a #GtkNotebook
 * @child: the #GtkWidget to use as the contents of the page.
 * @tab_label: (allow-none): the #GtkWidget to be used as the label for the page,
 *             or %NULL to use the default label, 'page N'.
 *
 * Appends a page to @notebook.
 *
 * Return value: the index (starting from 0) of the appended
 * page in the notebook, or -1 if function fails
 **/
gint
gtk_notebook_append_page (GtkNotebook *notebook,
			  GtkWidget   *child,
			  GtkWidget   *tab_label)
{
  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), -1);
  g_return_val_if_fail (GTK_IS_WIDGET (child), -1);
  g_return_val_if_fail (tab_label == NULL || GTK_IS_WIDGET (tab_label), -1);
  
  return gtk_notebook_insert_page_menu (notebook, child, tab_label, NULL, -1);
}

/**
 * gtk_notebook_append_page_menu:
 * @notebook: a #GtkNotebook
 * @child: the #GtkWidget to use as the contents of the page.
 * @tab_label: (allow-none): the #GtkWidget to be used as the label for the page,
 *             or %NULL to use the default label, 'page N'.
 * @menu_label: (allow-none): the widget to use as a label for the page-switch
 *              menu, if that is enabled. If %NULL, and @tab_label
 *              is a #GtkLabel or %NULL, then the menu label will be
 *              a newly created label with the same text as @tab_label;
 *              If @tab_label is not a #GtkLabel, @menu_label must be
 *              specified if the page-switch menu is to be used.
 * 
 * Appends a page to @notebook, specifying the widget to use as the
 * label in the popup menu.
 *
 * Return value: the index (starting from 0) of the appended
 * page in the notebook, or -1 if function fails
 **/
gint
gtk_notebook_append_page_menu (GtkNotebook *notebook,
			       GtkWidget   *child,
			       GtkWidget   *tab_label,
			       GtkWidget   *menu_label)
{
  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), -1);
  g_return_val_if_fail (GTK_IS_WIDGET (child), -1);
  g_return_val_if_fail (tab_label == NULL || GTK_IS_WIDGET (tab_label), -1);
  g_return_val_if_fail (menu_label == NULL || GTK_IS_WIDGET (menu_label), -1);
  
  return gtk_notebook_insert_page_menu (notebook, child, tab_label, menu_label, -1);
}

/**
 * gtk_notebook_prepend_page:
 * @notebook: a #GtkNotebook
 * @child: the #GtkWidget to use as the contents of the page.
 * @tab_label: (allow-none): the #GtkWidget to be used as the label for the page,
 *             or %NULL to use the default label, 'page N'.
 *
 * Prepends a page to @notebook.
 *
 * Return value: the index (starting from 0) of the prepended
 * page in the notebook, or -1 if function fails
 **/
gint
gtk_notebook_prepend_page (GtkNotebook *notebook,
			   GtkWidget   *child,
			   GtkWidget   *tab_label)
{
  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), -1);
  g_return_val_if_fail (GTK_IS_WIDGET (child), -1);
  g_return_val_if_fail (tab_label == NULL || GTK_IS_WIDGET (tab_label), -1);
  
  return gtk_notebook_insert_page_menu (notebook, child, tab_label, NULL, 0);
}

/**
 * gtk_notebook_prepend_page_menu:
 * @notebook: a #GtkNotebook
 * @child: the #GtkWidget to use as the contents of the page.
 * @tab_label: (allow-none): the #GtkWidget to be used as the label for the page,
 *             or %NULL to use the default label, 'page N'.
 * @menu_label: (allow-none): the widget to use as a label for the page-switch
 *              menu, if that is enabled. If %NULL, and @tab_label
 *              is a #GtkLabel or %NULL, then the menu label will be
 *              a newly created label with the same text as @tab_label;
 *              If @tab_label is not a #GtkLabel, @menu_label must be
 *              specified if the page-switch menu is to be used.
 * 
 * Prepends a page to @notebook, specifying the widget to use as the
 * label in the popup menu.
 *
 * Return value: the index (starting from 0) of the prepended
 * page in the notebook, or -1 if function fails
 **/
gint
gtk_notebook_prepend_page_menu (GtkNotebook *notebook,
				GtkWidget   *child,
				GtkWidget   *tab_label,
				GtkWidget   *menu_label)
{
  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), -1);
  g_return_val_if_fail (GTK_IS_WIDGET (child), -1);
  g_return_val_if_fail (tab_label == NULL || GTK_IS_WIDGET (tab_label), -1);
  g_return_val_if_fail (menu_label == NULL || GTK_IS_WIDGET (menu_label), -1);
  
  return gtk_notebook_insert_page_menu (notebook, child, tab_label, menu_label, 0);
}

/**
 * gtk_notebook_insert_page:
 * @notebook: a #GtkNotebook
 * @child: the #GtkWidget to use as the contents of the page.
 * @tab_label: (allow-none): the #GtkWidget to be used as the label for the page,
 *             or %NULL to use the default label, 'page N'.
 * @position: the index (starting at 0) at which to insert the page,
 *            or -1 to append the page after all other pages.
 *
 * Insert a page into @notebook at the given position.
 *
 * Return value: the index (starting from 0) of the inserted
 * page in the notebook, or -1 if function fails
 **/
gint
gtk_notebook_insert_page (GtkNotebook *notebook,
			  GtkWidget   *child,
			  GtkWidget   *tab_label,
			  gint         position)
{
  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), -1);
  g_return_val_if_fail (GTK_IS_WIDGET (child), -1);
  g_return_val_if_fail (tab_label == NULL || GTK_IS_WIDGET (tab_label), -1);
  
  return gtk_notebook_insert_page_menu (notebook, child, tab_label, NULL, position);
}


static gint
gtk_notebook_page_compare_tab (gconstpointer a,
			       gconstpointer b)
{
  return (((GtkNotebookPage *) a)->tab_label != b);
}

static gboolean
gtk_notebook_mnemonic_activate_switch_page (GtkWidget *child,
					    gboolean overload,
					    gpointer data)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (data);
  GList *list;
  
  list = g_list_find_custom (notebook->children, child,
			     gtk_notebook_page_compare_tab);
  if (list)
    {
      GtkNotebookPage *page = list->data;

      gtk_widget_grab_focus (GTK_WIDGET (notebook));	/* Do this first to avoid focusing new page */
      gtk_notebook_switch_page (notebook, page);
      focus_tabs_in (notebook);
    }

  return TRUE;
}

/**
 * gtk_notebook_insert_page_menu:
 * @notebook: a #GtkNotebook
 * @child: the #GtkWidget to use as the contents of the page.
 * @tab_label: (allow-none): the #GtkWidget to be used as the label for the page,
 *             or %NULL to use the default label, 'page N'.
 * @menu_label: (allow-none): the widget to use as a label for the page-switch
 *              menu, if that is enabled. If %NULL, and @tab_label
 *              is a #GtkLabel or %NULL, then the menu label will be
 *              a newly created label with the same text as @tab_label;
 *              If @tab_label is not a #GtkLabel, @menu_label must be
 *              specified if the page-switch menu is to be used.
 * @position: the index (starting at 0) at which to insert the page,
 *            or -1 to append the page after all other pages.
 * 
 * Insert a page into @notebook at the given position, specifying
 * the widget to use as the label in the popup menu.
 *
 * Return value: the index (starting from 0) of the inserted
 * page in the notebook
 **/
gint
gtk_notebook_insert_page_menu (GtkNotebook *notebook,
			       GtkWidget   *child,
			       GtkWidget   *tab_label,
			       GtkWidget   *menu_label,
			       gint         position)
{
  GtkNotebookClass *class;

  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), -1);
  g_return_val_if_fail (GTK_IS_WIDGET (child), -1);
  g_return_val_if_fail (tab_label == NULL || GTK_IS_WIDGET (tab_label), -1);
  g_return_val_if_fail (menu_label == NULL || GTK_IS_WIDGET (menu_label), -1);

  class = GTK_NOTEBOOK_GET_CLASS (notebook);

  return (class->insert_page) (notebook, child, tab_label, menu_label, position);
}

/**
 * gtk_notebook_remove_page:
 * @notebook: a #GtkNotebook.
 * @page_num: the index of a notebook page, starting
 *            from 0. If -1, the last page will
 *            be removed.
 * 
 * Removes a page from the notebook given its index
 * in the notebook.
 **/
void
gtk_notebook_remove_page (GtkNotebook *notebook,
			  gint         page_num)
{
  GList *list = NULL;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  if (page_num >= 0)
    list = g_list_nth (notebook->children, page_num);
  else
    list = g_list_last (notebook->children);

  if (list)
    gtk_container_remove (GTK_CONTAINER (notebook),
			  ((GtkNotebookPage *) list->data)->child);
}

/* Public GtkNotebook Page Switch Methods :
 * gtk_notebook_get_current_page
 * gtk_notebook_page_num
 * gtk_notebook_set_current_page
 * gtk_notebook_next_page
 * gtk_notebook_prev_page
 */
/**
 * gtk_notebook_get_current_page:
 * @notebook: a #GtkNotebook
 * 
 * Returns the page number of the current page.
 * 
 * Return value: the index (starting from 0) of the current
 * page in the notebook. If the notebook has no pages, then
 * -1 will be returned.
 **/
gint
gtk_notebook_get_current_page (GtkNotebook *notebook)
{
  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), -1);

  if (!notebook->cur_page)
    return -1;

  return g_list_index (notebook->children, notebook->cur_page);
}

/**
 * gtk_notebook_get_nth_page:
 * @notebook: a #GtkNotebook
 * @page_num: the index of a page in the notebook, or -1
 *            to get the last page.
 * 
 * Returns the child widget contained in page number @page_num.
 *
 * Return value: (transfer none): the child widget, or %NULL if @page_num is
 * out of bounds.
 **/
GtkWidget*
gtk_notebook_get_nth_page (GtkNotebook *notebook,
			   gint         page_num)
{
  GtkNotebookPage *page;
  GList *list;

  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), NULL);

  if (page_num >= 0)
    list = g_list_nth (notebook->children, page_num);
  else
    list = g_list_last (notebook->children);

  if (list)
    {
      page = list->data;
      return page->child;
    }

  return NULL;
}

/**
 * gtk_notebook_get_n_pages:
 * @notebook: a #GtkNotebook
 * 
 * Gets the number of pages in a notebook.
 * 
 * Return value: the number of pages in the notebook.
 *
 * Since: 2.2
 **/
gint
gtk_notebook_get_n_pages (GtkNotebook *notebook)
{
  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), 0);

  return g_list_length (notebook->children);
}

/**
 * gtk_notebook_page_num:
 * @notebook: a #GtkNotebook
 * @child: a #GtkWidget
 * 
 * Finds the index of the page which contains the given child
 * widget.
 * 
 * Return value: the index of the page containing @child, or
 *   -1 if @child is not in the notebook.
 **/
gint
gtk_notebook_page_num (GtkNotebook      *notebook,
		       GtkWidget        *child)
{
  GList *children;
  gint num;

  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), -1);

  num = 0;
  children = notebook->children;
  while (children)
    {
      GtkNotebookPage *page =  children->data;
      
      if (page->child == child)
	return num;

      children = children->next;
      num++;
    }

  return -1;
}

/**
 * gtk_notebook_set_current_page:
 * @notebook: a #GtkNotebook
 * @page_num: index of the page to switch to, starting from 0.
 *            If negative, the last page will be used. If greater
 *            than the number of pages in the notebook, nothing
 *            will be done.
 *                
 * Switches to the page number @page_num. 
 *
 * Note that due to historical reasons, GtkNotebook refuses
 * to switch to a page unless the child widget is visible. 
 * Therefore, it is recommended to show child widgets before
 * adding them to a notebook. 
 */
void
gtk_notebook_set_current_page (GtkNotebook *notebook,
			       gint         page_num)
{
  GList *list;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  if (page_num < 0)
    page_num = g_list_length (notebook->children) - 1;

  list = g_list_nth (notebook->children, page_num);
  if (list)
    gtk_notebook_switch_page (notebook, GTK_NOTEBOOK_PAGE (list));
}

/**
 * gtk_notebook_next_page:
 * @notebook: a #GtkNotebook
 * 
 * Switches to the next page. Nothing happens if the current page is
 * the last page.
 **/
void
gtk_notebook_next_page (GtkNotebook *notebook)
{
  GList *list;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  list = g_list_find (notebook->children, notebook->cur_page);
  if (!list)
    return;

  list = gtk_notebook_search_page (notebook, list, STEP_NEXT, TRUE);
  if (!list)
    return;

  gtk_notebook_switch_page (notebook, GTK_NOTEBOOK_PAGE (list));
}

/**
 * gtk_notebook_prev_page:
 * @notebook: a #GtkNotebook
 * 
 * Switches to the previous page. Nothing happens if the current page
 * is the first page.
 **/
void
gtk_notebook_prev_page (GtkNotebook *notebook)
{
  GList *list;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  list = g_list_find (notebook->children, notebook->cur_page);
  if (!list)
    return;

  list = gtk_notebook_search_page (notebook, list, STEP_PREV, TRUE);
  if (!list)
    return;

  gtk_notebook_switch_page (notebook, GTK_NOTEBOOK_PAGE (list));
}

/* Public GtkNotebook/Tab Style Functions
 *
 * gtk_notebook_set_show_border
 * gtk_notebook_get_show_border
 * gtk_notebook_set_show_tabs
 * gtk_notebook_get_show_tabs
 * gtk_notebook_set_tab_pos
 * gtk_notebook_get_tab_pos
 * gtk_notebook_set_scrollable
 * gtk_notebook_get_scrollable
 * gtk_notebook_get_tab_hborder
 * gtk_notebook_get_tab_vborder
 */
/**
 * gtk_notebook_set_show_border:
 * @notebook: a #GtkNotebook
 * @show_border: %TRUE if a bevel should be drawn around the notebook.
 * 
 * Sets whether a bevel will be drawn around the notebook pages.
 * This only has a visual effect when the tabs are not shown.
 * See gtk_notebook_set_show_tabs().
 **/
void
gtk_notebook_set_show_border (GtkNotebook *notebook,
			      gboolean     show_border)
{
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  if (notebook->show_border != show_border)
    {
      notebook->show_border = show_border;

      if (gtk_widget_get_visible (GTK_WIDGET (notebook)))
	gtk_widget_queue_resize (GTK_WIDGET (notebook));
      
      g_object_notify (G_OBJECT (notebook), "show-border");
    }
}

/**
 * gtk_notebook_get_show_border:
 * @notebook: a #GtkNotebook
 *
 * Returns whether a bevel will be drawn around the notebook pages. See
 * gtk_notebook_set_show_border().
 *
 * Return value: %TRUE if the bevel is drawn
 **/
gboolean
gtk_notebook_get_show_border (GtkNotebook *notebook)
{
  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), FALSE);

  return notebook->show_border;
}

/**
 * gtk_notebook_set_show_tabs:
 * @notebook: a #GtkNotebook
 * @show_tabs: %TRUE if the tabs should be shown.
 * 
 * Sets whether to show the tabs for the notebook or not.
 **/
void
gtk_notebook_set_show_tabs (GtkNotebook *notebook,
			    gboolean     show_tabs)
{
  GtkNotebookPrivate *priv;
  GtkNotebookPage *page;
  GList *children;
  gint i;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  priv = GTK_NOTEBOOK_GET_PRIVATE (notebook);

  show_tabs = show_tabs != FALSE;

  if (notebook->show_tabs == show_tabs)
    return;

  notebook->show_tabs = show_tabs;
  children = notebook->children;

  if (!show_tabs)
    {
      gtk_widget_set_can_focus (GTK_WIDGET (notebook), FALSE);

      while (children)
	{
	  page = children->data;
	  children = children->next;
	  if (page->default_tab)
	    {
	      gtk_widget_destroy (page->tab_label);
	      page->tab_label = NULL;
	    }
	  else
	    gtk_widget_hide (page->tab_label);
	}
    }
  else
    {
      gtk_widget_set_can_focus (GTK_WIDGET (notebook), TRUE);
      gtk_notebook_update_labels (notebook);
    }

  for (i = 0; i < N_ACTION_WIDGETS; i++)
    {
      if (priv->action_widget[i])
        gtk_widget_set_child_visible (priv->action_widget[i], show_tabs);
    }

  gtk_widget_queue_resize (GTK_WIDGET (notebook));

  g_object_notify (G_OBJECT (notebook), "show-tabs");
}

/**
 * gtk_notebook_get_show_tabs:
 * @notebook: a #GtkNotebook
 *
 * Returns whether the tabs of the notebook are shown. See
 * gtk_notebook_set_show_tabs().
 *
 * Return value: %TRUE if the tabs are shown
 **/
gboolean
gtk_notebook_get_show_tabs (GtkNotebook *notebook)
{
  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), FALSE);

  return notebook->show_tabs;
}

/**
 * gtk_notebook_set_tab_pos:
 * @notebook: a #GtkNotebook.
 * @pos: the edge to draw the tabs at.
 * 
 * Sets the edge at which the tabs for switching pages in the
 * notebook are drawn.
 **/
void
gtk_notebook_set_tab_pos (GtkNotebook     *notebook,
			  GtkPositionType  pos)
{
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  if (notebook->tab_pos != pos)
    {
      notebook->tab_pos = pos;
      if (gtk_widget_get_visible (GTK_WIDGET (notebook)))
	gtk_widget_queue_resize (GTK_WIDGET (notebook));
    }

  g_object_notify (G_OBJECT (notebook), "tab-pos");
}

/**
 * gtk_notebook_get_tab_pos:
 * @notebook: a #GtkNotebook
 *
 * Gets the edge at which the tabs for switching pages in the
 * notebook are drawn.
 *
 * Return value: the edge at which the tabs are drawn
 **/
GtkPositionType
gtk_notebook_get_tab_pos (GtkNotebook *notebook)
{
  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), GTK_POS_TOP);

  return notebook->tab_pos;
}

/**
 * gtk_notebook_set_homogeneous_tabs:
 * @notebook: a #GtkNotebook
 * @homogeneous: %TRUE if all tabs should be the same size.
 * 
 * Sets whether the tabs must have all the same size or not.
 **/
void
gtk_notebook_set_homogeneous_tabs (GtkNotebook *notebook,
				   gboolean     homogeneous)
{
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  gtk_notebook_set_homogeneous_tabs_internal (notebook, homogeneous);
}

/**
 * gtk_notebook_set_tab_border:
 * @notebook: a #GtkNotebook
 * @border_width: width of the border around the tab labels.
 * 
 * Sets the width the border around the tab labels
 * in a notebook. This is equivalent to calling
 * gtk_notebook_set_tab_hborder (@notebook, @border_width) followed
 * by gtk_notebook_set_tab_vborder (@notebook, @border_width).
 **/
void
gtk_notebook_set_tab_border (GtkNotebook *notebook,
			     guint        border_width)
{
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  gtk_notebook_set_tab_border_internal (notebook, border_width);
}

/**
 * gtk_notebook_set_tab_hborder:
 * @notebook: a #GtkNotebook
 * @tab_hborder: width of the horizontal border of tab labels.
 * 
 * Sets the width of the horizontal border of tab labels.
 **/
void
gtk_notebook_set_tab_hborder (GtkNotebook *notebook,
			      guint        tab_hborder)
{
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  gtk_notebook_set_tab_hborder_internal (notebook, tab_hborder);
}

/**
 * gtk_notebook_set_tab_vborder:
 * @notebook: a #GtkNotebook
 * @tab_vborder: width of the vertical border of tab labels.
 * 
 * Sets the width of the vertical border of tab labels.
 **/
void
gtk_notebook_set_tab_vborder (GtkNotebook *notebook,
			      guint        tab_vborder)
{
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  gtk_notebook_set_tab_vborder_internal (notebook, tab_vborder);
}

/**
 * gtk_notebook_set_scrollable:
 * @notebook: a #GtkNotebook
 * @scrollable: %TRUE if scroll arrows should be added
 * 
 * Sets whether the tab label area will have arrows for scrolling if
 * there are too many tabs to fit in the area.
 **/
void
gtk_notebook_set_scrollable (GtkNotebook *notebook,
			     gboolean     scrollable)
{
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  scrollable = (scrollable != FALSE);

  if (scrollable != notebook->scrollable)
    {
      notebook->scrollable = scrollable;

      if (gtk_widget_get_visible (GTK_WIDGET (notebook)))
	gtk_widget_queue_resize (GTK_WIDGET (notebook));

      g_object_notify (G_OBJECT (notebook), "scrollable");
    }
}

/**
 * gtk_notebook_get_scrollable:
 * @notebook: a #GtkNotebook
 *
 * Returns whether the tab label area has arrows for scrolling. See
 * gtk_notebook_set_scrollable().
 *
 * Return value: %TRUE if arrows for scrolling are present
 **/
gboolean
gtk_notebook_get_scrollable (GtkNotebook *notebook)
{
  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), FALSE);

  return notebook->scrollable;
}

/**
 * gtk_notebook_get_tab_hborder:
 * @notebook: a #GtkNotebook
 *
 * Returns the horizontal width of a tab border.
 *
 * Return value: horizontal width of a tab border
 *
 * Since: 2.22
 */
guint16
gtk_notebook_get_tab_hborder (GtkNotebook *notebook)
{
  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), FALSE);

  return notebook->tab_hborder;
}

/**
 * gtk_notebook_get_tab_vborder:
 * @notebook: a #GtkNotebook
 *
 * Returns the vertical width of a tab border.
 *
 * Return value: vertical width of a tab border
 *
 * Since: 2.22
 */
guint16
gtk_notebook_get_tab_vborder (GtkNotebook *notebook)
{
  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), FALSE);

  return notebook->tab_vborder;
}


/* Public GtkNotebook Popup Menu Methods:
 *
 * gtk_notebook_popup_enable
 * gtk_notebook_popup_disable
 */


/**
 * gtk_notebook_popup_enable:
 * @notebook: a #GtkNotebook
 * 
 * Enables the popup menu: if the user clicks with the right mouse button on
 * the tab labels, a menu with all the pages will be popped up.
 **/
void
gtk_notebook_popup_enable (GtkNotebook *notebook)
{
  GList *list;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  if (notebook->menu)
    return;

  notebook->menu = gtk_menu_new ();
  for (list = gtk_notebook_search_page (notebook, NULL, STEP_NEXT, FALSE);
       list;
       list = gtk_notebook_search_page (notebook, list, STEP_NEXT, FALSE))
    gtk_notebook_menu_item_create (notebook, list);

  gtk_notebook_update_labels (notebook);
  gtk_menu_attach_to_widget (GTK_MENU (notebook->menu),
			     GTK_WIDGET (notebook),
			     gtk_notebook_menu_detacher);

  g_object_notify (G_OBJECT (notebook), "enable-popup");
}

/**
 * gtk_notebook_popup_disable:
 * @notebook: a #GtkNotebook
 * 
 * Disables the popup menu.
 **/
void       
gtk_notebook_popup_disable  (GtkNotebook *notebook)
{
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  if (!notebook->menu)
    return;

  gtk_container_foreach (GTK_CONTAINER (notebook->menu),
			 (GtkCallback) gtk_notebook_menu_label_unparent, NULL);
  gtk_widget_destroy (notebook->menu);

  g_object_notify (G_OBJECT (notebook), "enable-popup");
}

/* Public GtkNotebook Page Properties Functions:
 *
 * gtk_notebook_get_tab_label
 * gtk_notebook_set_tab_label
 * gtk_notebook_set_tab_label_text
 * gtk_notebook_get_menu_label
 * gtk_notebook_set_menu_label
 * gtk_notebook_set_menu_label_text
 * gtk_notebook_set_tab_label_packing
 * gtk_notebook_query_tab_label_packing
 * gtk_notebook_get_tab_reorderable
 * gtk_notebook_set_tab_reorderable
 * gtk_notebook_get_tab_detachable
 * gtk_notebook_set_tab_detachable
 */

/**
 * gtk_notebook_get_tab_label:
 * @notebook: a #GtkNotebook
 * @child: the page
 * 
 * Returns the tab label widget for the page @child. %NULL is returned
 * if @child is not in @notebook or if no tab label has specifically
 * been set for @child.
 *
 * Return value: (transfer none): the tab label
 **/
GtkWidget *
gtk_notebook_get_tab_label (GtkNotebook *notebook,
			    GtkWidget   *child)
{
  GList *list;

  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (child), NULL);

  list = CHECK_FIND_CHILD (notebook, child);
  if (!list)  
    return NULL;

  if (GTK_NOTEBOOK_PAGE (list)->default_tab)
    return NULL;

  return GTK_NOTEBOOK_PAGE (list)->tab_label;
}  

/**
 * gtk_notebook_set_tab_label:
 * @notebook: a #GtkNotebook
 * @child: the page
 * @tab_label: (allow-none): the tab label widget to use, or %NULL for default tab
 *             label.
 *
 * Changes the tab label for @child. If %NULL is specified
 * for @tab_label, then the page will have the label 'page N'.
 **/
void
gtk_notebook_set_tab_label (GtkNotebook *notebook,
			    GtkWidget   *child,
			    GtkWidget   *tab_label)
{
  GtkNotebookPage *page;
  GList *list;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  g_return_if_fail (GTK_IS_WIDGET (child));

  list = CHECK_FIND_CHILD (notebook, child);
  if (!list)  
    return;

  /* a NULL pointer indicates a default_tab setting, otherwise
   * we need to set the associated label
   */
  page = list->data;
  
  if (page->tab_label == tab_label)
    return;
  

  gtk_notebook_remove_tab_label (notebook, page);
  
  if (tab_label)
    {
      page->default_tab = FALSE;
      page->tab_label = tab_label;
      gtk_widget_set_parent (page->tab_label, GTK_WIDGET (notebook));
    }
  else
    {
      page->default_tab = TRUE;
      page->tab_label = NULL;

      if (notebook->show_tabs)
	{
	  gchar string[32];

	  g_snprintf (string, sizeof(string), _("Page %u"), 
		      gtk_notebook_real_page_position (notebook, list));
	  page->tab_label = gtk_label_new (string);
	  gtk_widget_set_parent (page->tab_label, GTK_WIDGET (notebook));
	}
    }

  if (page->tab_label)
    page->mnemonic_activate_signal =
      g_signal_connect (page->tab_label,
			"mnemonic-activate",
			G_CALLBACK (gtk_notebook_mnemonic_activate_switch_page),
			notebook);

  if (notebook->show_tabs && gtk_widget_get_visible (child))
    {
      gtk_widget_show (page->tab_label);
      gtk_widget_queue_resize (GTK_WIDGET (notebook));
    }

  gtk_notebook_update_tab_states (notebook);
  gtk_widget_child_notify (child, "tab-label");
}

/**
 * gtk_notebook_set_tab_label_text:
 * @notebook: a #GtkNotebook
 * @child: the page
 * @tab_text: the label text
 * 
 * Creates a new label and sets it as the tab label for the page
 * containing @child.
 **/
void
gtk_notebook_set_tab_label_text (GtkNotebook *notebook,
				 GtkWidget   *child,
				 const gchar *tab_text)
{
  GtkWidget *tab_label = NULL;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  if (tab_text)
    tab_label = gtk_label_new (tab_text);
  gtk_notebook_set_tab_label (notebook, child, tab_label);
  gtk_widget_child_notify (child, "tab-label");
}

/**
 * gtk_notebook_get_tab_label_text:
 * @notebook: a #GtkNotebook
 * @child: a widget contained in a page of @notebook
 *
 * Retrieves the text of the tab label for the page containing
 *    @child.
 *
 * Return value: the text of the tab label, or %NULL if the
 *               tab label widget is not a #GtkLabel. The
 *               string is owned by the widget and must not
 *               be freed.
 **/
const gchar *
gtk_notebook_get_tab_label_text (GtkNotebook *notebook,
				 GtkWidget   *child)
{
  GtkWidget *tab_label;

  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (child), NULL);

  tab_label = gtk_notebook_get_tab_label (notebook, child);

  if (GTK_IS_LABEL (tab_label))
    return gtk_label_get_text (GTK_LABEL (tab_label));
  else
    return NULL;
}

/**
 * gtk_notebook_get_menu_label:
 * @notebook: a #GtkNotebook
 * @child: a widget contained in a page of @notebook
 *
 * Retrieves the menu label widget of the page containing @child.
 *
 * Return value: (transfer none): the menu label, or %NULL if the
 *     notebook page does not have a menu label other than the
 *     default (the tab label).
 **/
GtkWidget*
gtk_notebook_get_menu_label (GtkNotebook *notebook,
			     GtkWidget   *child)
{
  GList *list;

  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (child), NULL);

  list = CHECK_FIND_CHILD (notebook, child);
  if (!list)
    return NULL;

  if (GTK_NOTEBOOK_PAGE (list)->default_menu)
    return NULL;

  return GTK_NOTEBOOK_PAGE (list)->menu_label;
}

/**
 * gtk_notebook_set_menu_label:
 * @notebook: a #GtkNotebook
 * @child: the child widget
 * @menu_label: (allow-none): the menu label, or NULL for default
 *
 * Changes the menu label for the page containing @child.
 **/
void
gtk_notebook_set_menu_label (GtkNotebook *notebook,
			     GtkWidget   *child,
			     GtkWidget   *menu_label)
{
  GtkNotebookPage *page;
  GList *list;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  g_return_if_fail (GTK_IS_WIDGET (child));

  list = CHECK_FIND_CHILD (notebook, child);
  if (!list)  
    return;

  page = list->data;
  if (page->menu_label)
    {
      if (notebook->menu)
	gtk_container_remove (GTK_CONTAINER (notebook->menu), 
			      page->menu_label->parent);

      if (!page->default_menu)
	g_object_unref (page->menu_label);
    }

  if (menu_label)
    {
      page->menu_label = menu_label;
      g_object_ref_sink (page->menu_label);
      page->default_menu = FALSE;
    }
  else
    page->default_menu = TRUE;

  if (notebook->menu)
    gtk_notebook_menu_item_create (notebook, list);
  gtk_widget_child_notify (child, "menu-label");
}

/**
 * gtk_notebook_set_menu_label_text:
 * @notebook: a #GtkNotebook
 * @child: the child widget
 * @menu_text: the label text
 * 
 * Creates a new label and sets it as the menu label of @child.
 **/
void
gtk_notebook_set_menu_label_text (GtkNotebook *notebook,
				  GtkWidget   *child,
				  const gchar *menu_text)
{
  GtkWidget *menu_label = NULL;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  if (menu_text)
    {
      menu_label = gtk_label_new (menu_text);
      gtk_misc_set_alignment (GTK_MISC (menu_label), 0.0, 0.5);
    }
  gtk_notebook_set_menu_label (notebook, child, menu_label);
  gtk_widget_child_notify (child, "menu-label");
}

/**
 * gtk_notebook_get_menu_label_text:
 * @notebook: a #GtkNotebook
 * @child: the child widget of a page of the notebook.
 *
 * Retrieves the text of the menu label for the page containing
 *    @child.
 *
 * Return value: the text of the tab label, or %NULL if the
 *               widget does not have a menu label other than
 *               the default menu label, or the menu label widget
 *               is not a #GtkLabel. The string is owned by
 *               the widget and must not be freed.
 **/
const gchar *
gtk_notebook_get_menu_label_text (GtkNotebook *notebook,
				  GtkWidget *child)
{
  GtkWidget *menu_label;

  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (child), NULL);
 
  menu_label = gtk_notebook_get_menu_label (notebook, child);

  if (GTK_IS_LABEL (menu_label))
    return gtk_label_get_text (GTK_LABEL (menu_label));
  else
    return NULL;
}
  
/* Helper function called when pages are reordered
 */
static void
gtk_notebook_child_reordered (GtkNotebook     *notebook,
			      GtkNotebookPage *page)
{
  if (notebook->menu)
    {
      GtkWidget *menu_item;
      
      menu_item = page->menu_label->parent;
      gtk_container_remove (GTK_CONTAINER (menu_item), page->menu_label);
      gtk_container_remove (GTK_CONTAINER (notebook->menu), menu_item);
      gtk_notebook_menu_item_create (notebook, g_list_find (notebook->children, page));
    }

  gtk_notebook_update_tab_states (notebook);
  gtk_notebook_update_labels (notebook);
}

/**
 * gtk_notebook_set_tab_label_packing:
 * @notebook: a #GtkNotebook
 * @child: the child widget
 * @expand: whether to expand the tab label or not
 * @fill: whether the tab label should fill the allocated area or not
 * @pack_type: the position of the tab label
 *
 * Sets the packing parameters for the tab label of the page
 * containing @child. See gtk_box_pack_start() for the exact meaning
 * of the parameters.
 *
 * Deprecated: 2.20: Modify the #GtkNotebook:tab-expand and
 *   #GtkNotebook:tab-fill child properties instead.
 *   Modifying the packing of the tab label is a deprecated feature and
 *   shouldn't be done anymore.
 **/
void
gtk_notebook_set_tab_label_packing (GtkNotebook *notebook,
				    GtkWidget   *child,
				    gboolean     expand,
				    gboolean     fill,
				    GtkPackType  pack_type)
{
  GtkNotebookPage *page;
  GList *list;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  g_return_if_fail (GTK_IS_WIDGET (child));

  list = CHECK_FIND_CHILD (notebook, child);
  if (!list)  
    return;

  page = list->data;
  expand = expand != FALSE;
  fill = fill != FALSE;
  if (page->pack == pack_type && page->expand == expand && page->fill == fill)
    return;

  gtk_widget_freeze_child_notify (child);
  page->expand = expand;
  gtk_widget_child_notify (child, "tab-expand");
  page->fill = fill;
  gtk_widget_child_notify (child, "tab-fill");
  if (page->pack != pack_type)
    {
      page->pack = pack_type;
      gtk_notebook_child_reordered (notebook, page);
    }
  gtk_widget_child_notify (child, "tab-pack");
  gtk_widget_child_notify (child, "position");
  if (notebook->show_tabs)
    gtk_notebook_pages_allocate (notebook);
  gtk_widget_thaw_child_notify (child);
}  

/**
 * gtk_notebook_query_tab_label_packing:
 * @notebook: a #GtkNotebook
 * @child: the page
 * @expand: location to store the expand value (or NULL)
 * @fill: location to store the fill value (or NULL)
 * @pack_type: location to store the pack_type (or NULL)
 * 
 * Query the packing attributes for the tab label of the page
 * containing @child.
 *
 * Deprecated: 2.20: Modify the #GtkNotebook:tab-expand and
 *   #GtkNotebook:tab-fill child properties instead.
 **/
void
gtk_notebook_query_tab_label_packing (GtkNotebook *notebook,
				      GtkWidget   *child,
				      gboolean    *expand,
				      gboolean    *fill,
				      GtkPackType *pack_type)
{
  GList *list;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  g_return_if_fail (GTK_IS_WIDGET (child));

  list = CHECK_FIND_CHILD (notebook, child);
  if (!list)
    return;

  if (expand)
    *expand = GTK_NOTEBOOK_PAGE (list)->expand;
  if (fill)
    *fill = GTK_NOTEBOOK_PAGE (list)->fill;
  if (pack_type)
    *pack_type = GTK_NOTEBOOK_PAGE (list)->pack;
}

/**
 * gtk_notebook_reorder_child:
 * @notebook: a #GtkNotebook
 * @child: the child to move
 * @position: the new position, or -1 to move to the end
 * 
 * Reorders the page containing @child, so that it appears in position
 * @position. If @position is greater than or equal to the number of
 * children in the list or negative, @child will be moved to the end
 * of the list.
 **/
void
gtk_notebook_reorder_child (GtkNotebook *notebook,
			    GtkWidget   *child,
			    gint         position)
{
  GList *list, *new_list;
  GtkNotebookPage *page;
  gint old_pos;
  gint max_pos;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  g_return_if_fail (GTK_IS_WIDGET (child));

  list = CHECK_FIND_CHILD (notebook, child);
  if (!list)
    return;

  max_pos = g_list_length (notebook->children) - 1;
  if (position < 0 || position > max_pos)
    position = max_pos;

  old_pos = g_list_position (notebook->children, list);

  if (old_pos == position)
    return;

  page = list->data;
  notebook->children = g_list_delete_link (notebook->children, list);

  notebook->children = g_list_insert (notebook->children, page, position);
  new_list = g_list_nth (notebook->children, position);

  /* Fix up GList references in GtkNotebook structure */
  if (notebook->first_tab == list)
    notebook->first_tab = new_list;
  if (notebook->focus_tab == list)
    notebook->focus_tab = new_list;

  gtk_widget_freeze_child_notify (child);

  /* Move around the menu items if necessary */
  gtk_notebook_child_reordered (notebook, page);
  gtk_widget_child_notify (child, "tab-pack");
  gtk_widget_child_notify (child, "position");

  if (notebook->show_tabs)
    gtk_notebook_pages_allocate (notebook);

  gtk_widget_thaw_child_notify (child);

  g_signal_emit (notebook,
		 notebook_signals[PAGE_REORDERED],
		 0,
		 child,
		 position);
}

/**
 * gtk_notebook_set_window_creation_hook:
 * @func: (allow-none): the #GtkNotebookWindowCreationFunc, or %NULL
 * @data: user data for @func
 * @destroy: (allow-none): Destroy notifier for @data, or %NULL
 *
 * Installs a global function used to create a window
 * when a detached tab is dropped in an empty area.
 *
 * Since: 2.10
 *
 * Deprecated: 2.24: Use the #GtkNotebook::create-window signal instead
**/
void
gtk_notebook_set_window_creation_hook (GtkNotebookWindowCreationFunc  func,
				       gpointer                       data,
                                       GDestroyNotify                 destroy)
{
  if (window_creation_hook_destroy)
    window_creation_hook_destroy (window_creation_hook_data);

  window_creation_hook = func;
  window_creation_hook_data = data;
  window_creation_hook_destroy = destroy;
}

/**
 * gtk_notebook_set_group_id:
 * @notebook: a #GtkNotebook
 * @group_id: a group identificator, or -1 to unset it
 *
 * Sets an group identificator for @notebook, notebooks sharing
 * the same group identificator will be able to exchange tabs
 * via drag and drop. A notebook with group identificator -1 will
 * not be able to exchange tabs with any other notebook.
 * 
 * Since: 2.10
 * Deprecated: 2.12: use gtk_notebook_set_group_name() instead.
 */
void
gtk_notebook_set_group_id (GtkNotebook *notebook,
			   gint         group_id)
{
  gpointer group;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  /* add 1 to get rid of the -1/NULL difference */
  group = GINT_TO_POINTER (group_id + 1);
  gtk_notebook_set_group (notebook, group);
}

/**
 * gtk_notebook_set_group:
 * @notebook: a #GtkNotebook
 * @group: (allow-none): a pointer to identify the notebook group, or %NULL to unset it
 *
 * Sets a group identificator pointer for @notebook, notebooks sharing
 * the same group identificator pointer will be able to exchange tabs
 * via drag and drop. A notebook with a %NULL group identificator will
 * not be able to exchange tabs with any other notebook.
 *
 * Since: 2.12
 *
 * Deprecated: 2.24: Use gtk_notebook_set_group_name() instead
 */
void
gtk_notebook_set_group (GtkNotebook *notebook,
			gpointer     group)
{
  GtkNotebookPrivate *priv;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  priv = GTK_NOTEBOOK_GET_PRIVATE (notebook);

  if (priv->group != group)
    {
      priv->group = group;
      g_object_notify (G_OBJECT (notebook), "group");
    }
}

/**
 * gtk_notebook_set_group_name:
 * @notebook: a #GtkNotebook
 * @name: (allow-none): the name of the notebook group, or %NULL to unset it
 *
 * Sets a group name for @notebook.
 *
 * Notebooks with the same name will be able to exchange tabs
 * via drag and drop. A notebook with a %NULL group name will
 * not be able to exchange tabs with any other notebook.
 *
 * Since: 2.24
 */
void
gtk_notebook_set_group_name (GtkNotebook *notebook,
                             const gchar *group_name)
{
  gpointer group;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  group = (gpointer)g_intern_string (group_name);
  gtk_notebook_set_group (notebook, group);
  g_object_notify (G_OBJECT (notebook), "group-name");
}

/**
 * gtk_notebook_get_group_id:
 * @notebook: a #GtkNotebook
 * 
 * Gets the current group identificator for @notebook.
 * 
 * Return Value: the group identificator, or -1 if none is set.
 *
 * Since: 2.10
 * Deprecated: 2.12: use gtk_notebook_get_group_name() instead.
 */
gint
gtk_notebook_get_group_id (GtkNotebook *notebook)
{
  GtkNotebookPrivate *priv;

  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), -1);

  priv = GTK_NOTEBOOK_GET_PRIVATE (notebook);

  /* substract 1 to get rid of the -1/NULL difference */
  return GPOINTER_TO_INT (priv->group) - 1;
}


/**
 * gtk_notebook_get_group:
 * @notebook: a #GtkNotebook
 *
 * Gets the current group identificator pointer for @notebook.
 *
 * Return Value: (transfer none): the group identificator,
 *     or %NULL if none is set.
 *
 * Since: 2.12
 *
 * Deprecated: 2.24: Use gtk_notebook_get_group_name() instead
 **/
gpointer
gtk_notebook_get_group (GtkNotebook *notebook)
{
  GtkNotebookPrivate *priv;

  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), NULL);

  priv = GTK_NOTEBOOK_GET_PRIVATE (notebook);
  return priv->group;
}

/**
 * gtk_notebook_get_group_name:
 * @notebook: a #GtkNotebook
 *
 * Gets the current group name for @notebook.
 *
 * Note that this funtion can emphasis not be used
 * together with gtk_notebook_set_group() or
 * gtk_notebook_set_group_id().
 *
 Return Value: (transfer none): the group name,
 *     or %NULL if none is set.
 *
 * Since: 2.24
 */
const gchar *
gtk_notebook_get_group_name (GtkNotebook *notebook)
{
  GtkNotebookPrivate *priv;
  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), NULL);

  priv = GTK_NOTEBOOK_GET_PRIVATE (notebook);
  return (const gchar *)priv->group;
}

/**
 * gtk_notebook_get_tab_reorderable:
 * @notebook: a #GtkNotebook
 * @child: a child #GtkWidget
 * 
 * Gets whether the tab can be reordered via drag and drop or not.
 * 
 * Return Value: %TRUE if the tab is reorderable.
 * 
 * Since: 2.10
 **/
gboolean
gtk_notebook_get_tab_reorderable (GtkNotebook *notebook,
				  GtkWidget   *child)
{
  GList *list;

  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), FALSE);
  g_return_val_if_fail (GTK_IS_WIDGET (child), FALSE);

  list = CHECK_FIND_CHILD (notebook, child);
  if (!list)  
    return FALSE;

  return GTK_NOTEBOOK_PAGE (list)->reorderable;
}

/**
 * gtk_notebook_set_tab_reorderable:
 * @notebook: a #GtkNotebook
 * @child: a child #GtkWidget
 * @reorderable: whether the tab is reorderable or not.
 *
 * Sets whether the notebook tab can be reordered
 * via drag and drop or not.
 * 
 * Since: 2.10
 **/
void
gtk_notebook_set_tab_reorderable (GtkNotebook *notebook,
				  GtkWidget   *child,
				  gboolean     reorderable)
{
  GList *list;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  g_return_if_fail (GTK_IS_WIDGET (child));

  list = CHECK_FIND_CHILD (notebook, child);
  if (!list)  
    return;

  if (GTK_NOTEBOOK_PAGE (list)->reorderable != reorderable)
    {
      GTK_NOTEBOOK_PAGE (list)->reorderable = (reorderable == TRUE);
      gtk_widget_child_notify (child, "reorderable");
    }
}

/**
 * gtk_notebook_get_tab_detachable:
 * @notebook: a #GtkNotebook
 * @child: a child #GtkWidget
 * 
 * Returns whether the tab contents can be detached from @notebook.
 * 
 * Return Value: TRUE if the tab is detachable.
 *
 * Since: 2.10
 **/
gboolean
gtk_notebook_get_tab_detachable (GtkNotebook *notebook,
				 GtkWidget   *child)
{
  GList *list;

  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), FALSE);
  g_return_val_if_fail (GTK_IS_WIDGET (child), FALSE);

  list = CHECK_FIND_CHILD (notebook, child);
  if (!list)  
    return FALSE;

  return GTK_NOTEBOOK_PAGE (list)->detachable;
}

/**
 * gtk_notebook_set_tab_detachable:
 * @notebook: a #GtkNotebook
 * @child: a child #GtkWidget
 * @detachable: whether the tab is detachable or not
 *
 * Sets whether the tab can be detached from @notebook to another
 * notebook or widget.
 *
 * Note that 2 notebooks must share a common group identificator
 * (see gtk_notebook_set_group_id ()) to allow automatic tabs
 * interchange between them.
 *
 * If you want a widget to interact with a notebook through DnD
 * (i.e.: accept dragged tabs from it) it must be set as a drop
 * destination and accept the target "GTK_NOTEBOOK_TAB". The notebook
 * will fill the selection with a GtkWidget** pointing to the child
 * widget that corresponds to the dropped tab.
 * |[
 *  static void
 *  on_drop_zone_drag_data_received (GtkWidget        *widget,
 *                                   GdkDragContext   *context,
 *                                   gint              x,
 *                                   gint              y,
 *                                   GtkSelectionData *selection_data,
 *                                   guint             info,
 *                                   guint             time,
 *                                   gpointer          user_data)
 *  {
 *    GtkWidget *notebook;
 *    GtkWidget **child;
 *    
 *    notebook = gtk_drag_get_source_widget (context);
 *    child = (void*) selection_data->data;
 *    
 *    process_widget (*child);
 *    gtk_container_remove (GTK_CONTAINER (notebook), *child);
 *  }
 * ]|
 *
 * If you want a notebook to accept drags from other widgets,
 * you will have to set your own DnD code to do it.
 *
 * Since: 2.10
 **/
void
gtk_notebook_set_tab_detachable (GtkNotebook *notebook,
				 GtkWidget  *child,
				 gboolean    detachable)
{
  GList *list;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  g_return_if_fail (GTK_IS_WIDGET (child));

  list = CHECK_FIND_CHILD (notebook, child);
  if (!list)  
    return;

  if (GTK_NOTEBOOK_PAGE (list)->detachable != detachable)
    {
      GTK_NOTEBOOK_PAGE (list)->detachable = (detachable == TRUE);
      gtk_widget_child_notify (child, "detachable");
    }
}

/**
 * gtk_notebook_get_action_widget:
 * @notebook: a #GtkNotebook
 * @pack_type: pack type of the action widget to receive
 *
 * Gets one of the action widgets. See gtk_notebook_set_action_widget().
 *
 * Returns: (transfer none): The action widget with the given @pack_type
 *     or %NULL when this action widget has not been set
 *
 * Since: 2.20
 */
GtkWidget*
gtk_notebook_get_action_widget (GtkNotebook *notebook,
                                GtkPackType  pack_type)
{
  GtkNotebookPrivate *priv;

  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), NULL);

  priv = GTK_NOTEBOOK_GET_PRIVATE (notebook);
  return priv->action_widget[pack_type];
}

/**
 * gtk_notebook_set_action_widget:
 * @notebook: a #GtkNotebook
 * @widget: a #GtkWidget
 * @pack_type: pack type of the action widget
 *
 * Sets @widget as one of the action widgets. Depending on the pack type
 * the widget will be placed before or after the tabs. You can use
 * a #GtkBox if you need to pack more than one widget on the same side.
 *
 * Note that action widgets are "internal" children of the notebook and thus
 * not included in the list returned from gtk_container_foreach().
 *
 * Since: 2.20
 */
void
gtk_notebook_set_action_widget (GtkNotebook *notebook,
				GtkWidget   *widget,
                                GtkPackType  pack_type)
{
  GtkNotebookPrivate *priv;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  g_return_if_fail (!widget || GTK_IS_WIDGET (widget));
  g_return_if_fail (!widget || widget->parent == NULL);

  priv = GTK_NOTEBOOK_GET_PRIVATE (notebook);

  if (priv->action_widget[pack_type])
    gtk_widget_unparent (priv->action_widget[pack_type]);

  priv->action_widget[pack_type] = widget;

  if (widget)
    {
      gtk_widget_set_child_visible (widget, notebook->show_tabs);
      gtk_widget_set_parent (widget, GTK_WIDGET (notebook));
    }

  gtk_widget_queue_resize (GTK_WIDGET (notebook));
}

#define __GTK_NOTEBOOK_C__
#include "gtkaliasdef.c"
