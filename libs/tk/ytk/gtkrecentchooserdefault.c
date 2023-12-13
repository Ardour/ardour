/* GTK - The GIMP Toolkit
 * gtkrecentchooserdefault.c
 * Copyright (C) 2005-2006, Emmanuele Bassi
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

#include <string.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "gtkstock.h"
#include "gtkicontheme.h"
#include "gtkiconfactory.h"
#include "gtksettings.h"
#include "gtktreeview.h"
#include "gtkliststore.h"
#include "gtkbutton.h"
#include "gtkcelllayout.h"
#include "gtkcellrendererpixbuf.h"
#include "gtkcellrenderertext.h"
#include "gtkcheckmenuitem.h"
#include "gtkclipboard.h"
#include "gtkcomboboxtext.h"
#include "gtkentry.h"
#include "gtkeventbox.h"
#include "gtkexpander.h"
#include "gtkframe.h"
#include "gtkhbox.h"
#include "gtkhpaned.h"
#include "gtkimage.h"
#include "gtkimagemenuitem.h"
#include "gtkintl.h"
#include "gtklabel.h"
#include "gtkmenuitem.h"
#include "gtkmessagedialog.h"
#include "gtkscrolledwindow.h"
#include "gtkseparatormenuitem.h"
#include "gtksizegroup.h"
#include "gtktable.h"
#include "gtktreemodelsort.h"
#include "gtktreemodelfilter.h"
#include "gtktreeselection.h"
#include "gtktreestore.h"
#include "gtktooltip.h"
#include "gtktypebuiltins.h"
#include "gtkvbox.h"
#include "gtkactivatable.h"

#include "gtkrecentmanager.h"
#include "gtkrecentfilter.h"
#include "gtkrecentchooser.h"
#include "gtkrecentchooserprivate.h"
#include "gtkrecentchooserutils.h"
#include "gtkrecentchooserdefault.h"

#include "gtkprivate.h"
#include "gtkalias.h"

enum 
{
  PROP_0,

  /* activatable properties */
  PROP_ACTIVATABLE_RELATED_ACTION,
  PROP_ACTIVATABLE_USE_ACTION_APPEARANCE
};


struct _GtkRecentChooserDefault
{
  GtkVBox parent_instance;
  
  GtkRecentManager *manager;
  gulong manager_changed_id;
  guint local_manager : 1;
  
  gint icon_size;

  /* RecentChooser properties */
  gint limit;  
  GtkRecentSortType sort_type;
  guint show_private : 1;
  guint show_not_found : 1;
  guint select_multiple : 1;
  guint show_tips : 1;
  guint show_icons : 1;
  guint local_only : 1;

  guint limit_set : 1;
  
  GSList *filters;
  GtkRecentFilter *current_filter;
  GtkWidget *filter_combo_hbox;
  GtkWidget *filter_combo;
  
  GtkRecentSortFunc sort_func;
  gpointer sort_data;
  GDestroyNotify sort_data_destroy;

  GtkIconTheme *icon_theme;
  
  GtkWidget *recent_view;
  GtkListStore *recent_store;
  GtkTreeViewColumn *icon_column;
  GtkTreeViewColumn *meta_column;
  GtkCellRenderer *meta_renderer;
  GtkTreeSelection *selection;
  
  GtkWidget *recent_popup_menu;
  GtkWidget *recent_popup_menu_copy_item;
  GtkWidget *recent_popup_menu_remove_item;
  GtkWidget *recent_popup_menu_clear_item;
  GtkWidget *recent_popup_menu_show_private_item;
 
  guint load_id;
  GList *recent_items;
  gint n_recent_items;
  gint loaded_items;
  guint load_state;
};

typedef struct _GtkRecentChooserDefaultClass
{
  GtkVBoxClass parent_class;
} GtkRecentChooserDefaultClass;

enum {
  RECENT_URI_COLUMN,
  RECENT_DISPLAY_NAME_COLUMN,
  RECENT_INFO_COLUMN,
    
  N_RECENT_COLUMNS
};

enum {
  LOAD_EMPTY,    /* initial state: the model is empty */
  LOAD_PRELOAD,  /* the model is loading and not inserted in the tree yet */
  LOAD_LOADING,  /* the model is fully loaded but not inserted */
  LOAD_FINISHED  /* the model is fully loaded and inserted */
};

/* Icon size for if we can't get it from the theme */
#define FALLBACK_ICON_SIZE  48
#define FALLBACK_ITEM_LIMIT 20

#define NUM_CHARS 40
#define NUM_LINES 9



/* GObject */
static void     _gtk_recent_chooser_default_class_init  (GtkRecentChooserDefaultClass *klass);
static void     _gtk_recent_chooser_default_init        (GtkRecentChooserDefault      *impl);
static GObject *gtk_recent_chooser_default_constructor  (GType                         type,
						         guint                         n_construct_prop,
						         GObjectConstructParam        *construct_params);
static void     gtk_recent_chooser_default_finalize     (GObject                      *object);
static void     gtk_recent_chooser_default_dispose      (GObject                      *object);
static void     gtk_recent_chooser_default_set_property (GObject                      *object,
						         guint                         prop_id,
						         const GValue                 *value,
						         GParamSpec                   *pspec);
static void     gtk_recent_chooser_default_get_property (GObject                      *object,
						         guint                         prop_id,
						         GValue                       *value,
						         GParamSpec                   *pspec);

/* GtkRecentChooserIface */
static void              gtk_recent_chooser_iface_init                 (GtkRecentChooserIface  *iface);
static gboolean          gtk_recent_chooser_default_set_current_uri    (GtkRecentChooser       *chooser,
								        const gchar            *uri,
								        GError                **error);
static gchar *           gtk_recent_chooser_default_get_current_uri    (GtkRecentChooser       *chooser);
static gboolean          gtk_recent_chooser_default_select_uri         (GtkRecentChooser       *chooser,
								        const gchar            *uri,
								        GError                **error);
static void              gtk_recent_chooser_default_unselect_uri       (GtkRecentChooser       *chooser,
								        const gchar            *uri);
static void              gtk_recent_chooser_default_select_all         (GtkRecentChooser       *chooser);
static void              gtk_recent_chooser_default_unselect_all       (GtkRecentChooser       *chooser);
static GList *           gtk_recent_chooser_default_get_items          (GtkRecentChooser       *chooser);
static GtkRecentManager *gtk_recent_chooser_default_get_recent_manager (GtkRecentChooser       *chooser);
static void              gtk_recent_chooser_default_set_sort_func      (GtkRecentChooser       *chooser,
									GtkRecentSortFunc       sort_func,
									gpointer                sort_data,
									GDestroyNotify          data_destroy);
static void              gtk_recent_chooser_default_add_filter         (GtkRecentChooser       *chooser,
								        GtkRecentFilter        *filter);
static void              gtk_recent_chooser_default_remove_filter      (GtkRecentChooser       *chooser,
								        GtkRecentFilter        *filter);
static GSList *          gtk_recent_chooser_default_list_filters       (GtkRecentChooser       *chooser);


static void gtk_recent_chooser_default_map      (GtkWidget *widget);
static void gtk_recent_chooser_default_show_all (GtkWidget *widget);

static void set_current_filter        (GtkRecentChooserDefault *impl,
				       GtkRecentFilter         *filter);

static GtkIconTheme *get_icon_theme_for_widget (GtkWidget   *widget);
static gint          get_icon_size_for_widget  (GtkWidget   *widget,
						GtkIconSize  icon_size);
static gint          get_recent_files_limit    (GtkWidget   *widget);

static void reload_recent_items (GtkRecentChooserDefault *impl);
static void chooser_set_model   (GtkRecentChooserDefault *impl);

static void set_recent_manager (GtkRecentChooserDefault *impl,
				GtkRecentManager        *manager);

static void chooser_set_sort_type (GtkRecentChooserDefault *impl,
				   GtkRecentSortType        sort_type);

static void recent_manager_changed_cb (GtkRecentManager  *manager,
			               gpointer           user_data);
static void recent_icon_data_func     (GtkTreeViewColumn *tree_column,
				       GtkCellRenderer   *cell,
				       GtkTreeModel      *model,
				       GtkTreeIter       *iter,
				       gpointer           user_data);
static void recent_meta_data_func     (GtkTreeViewColumn *tree_column,
				       GtkCellRenderer   *cell,
				       GtkTreeModel      *model,
				       GtkTreeIter       *iter,
				       gpointer           user_data);

static void selection_changed_cb      (GtkTreeSelection  *z,
				       gpointer           user_data);
static void row_activated_cb          (GtkTreeView       *tree_view,
				       GtkTreePath       *tree_path,
				       GtkTreeViewColumn *tree_column,
				       gpointer           user_data);
static void filter_combo_changed_cb   (GtkComboBox       *combo_box,
				       gpointer           user_data);

static void remove_all_activated_cb   (GtkMenuItem       *menu_item,
				       gpointer           user_data);
static void remove_item_activated_cb  (GtkMenuItem       *menu_item,
				       gpointer           user_data);
static void show_private_toggled_cb   (GtkCheckMenuItem  *menu_item,
				       gpointer           user_data);

static gboolean recent_view_popup_menu_cb   (GtkWidget      *widget,
					     gpointer        user_data);
static gboolean recent_view_button_press_cb (GtkWidget      *widget,
					     GdkEventButton *event,
					     gpointer        user_data);

static void     recent_view_drag_begin_cb         (GtkWidget        *widget,
						   GdkDragContext   *context,
						   gpointer          user_data);
static void     recent_view_drag_data_get_cb      (GtkWidget        *widget,
						   GdkDragContext   *context,
						   GtkSelectionData *selection_data,
						   guint             info,
						   guint32           time_,
						   gpointer          data);
static gboolean recent_view_query_tooltip_cb      (GtkWidget        *widget,
                                                   gint              x,
                                                   gint              y,
                                                   gboolean          keyboard_tip,
                                                   GtkTooltip       *tooltip,
                                                   gpointer          user_data);

static void gtk_recent_chooser_activatable_iface_init (GtkActivatableIface  *iface);
static void gtk_recent_chooser_update                 (GtkActivatable       *activatable,
						       GtkAction            *action,
						       const gchar          *property_name);
static void gtk_recent_chooser_sync_action_properties (GtkActivatable       *activatable,
						       GtkAction            *action);

G_DEFINE_TYPE_WITH_CODE (GtkRecentChooserDefault,
			 _gtk_recent_chooser_default,
			 GTK_TYPE_VBOX,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_RECENT_CHOOSER,
				 		gtk_recent_chooser_iface_init)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_ACTIVATABLE,
				 		gtk_recent_chooser_activatable_iface_init))




static void
gtk_recent_chooser_iface_init (GtkRecentChooserIface *iface)
{
  iface->set_current_uri = gtk_recent_chooser_default_set_current_uri;
  iface->get_current_uri = gtk_recent_chooser_default_get_current_uri;
  iface->select_uri = gtk_recent_chooser_default_select_uri;
  iface->unselect_uri = gtk_recent_chooser_default_unselect_uri;
  iface->select_all = gtk_recent_chooser_default_select_all;
  iface->unselect_all = gtk_recent_chooser_default_unselect_all;
  iface->get_items = gtk_recent_chooser_default_get_items;
  iface->get_recent_manager = gtk_recent_chooser_default_get_recent_manager;
  iface->set_sort_func = gtk_recent_chooser_default_set_sort_func;
  iface->add_filter = gtk_recent_chooser_default_add_filter;
  iface->remove_filter = gtk_recent_chooser_default_remove_filter;
  iface->list_filters = gtk_recent_chooser_default_list_filters;
}

static void
gtk_recent_chooser_activatable_iface_init (GtkActivatableIface *iface)

{
  iface->update = gtk_recent_chooser_update;
  iface->sync_action_properties = gtk_recent_chooser_sync_action_properties;
}

static void
_gtk_recent_chooser_default_class_init (GtkRecentChooserDefaultClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gobject_class->constructor = gtk_recent_chooser_default_constructor;
  gobject_class->set_property = gtk_recent_chooser_default_set_property;
  gobject_class->get_property = gtk_recent_chooser_default_get_property;
  gobject_class->dispose = gtk_recent_chooser_default_dispose;
  gobject_class->finalize = gtk_recent_chooser_default_finalize;
  
  widget_class->map = gtk_recent_chooser_default_map;
  widget_class->show_all = gtk_recent_chooser_default_show_all;
  
  _gtk_recent_chooser_install_properties (gobject_class);

  g_object_class_override_property (gobject_class, PROP_ACTIVATABLE_RELATED_ACTION, "related-action");
  g_object_class_override_property (gobject_class, PROP_ACTIVATABLE_USE_ACTION_APPEARANCE, "use-action-appearance");
}

static void
_gtk_recent_chooser_default_init (GtkRecentChooserDefault *impl)
{
  gtk_box_set_spacing (GTK_BOX (impl), 6);

  /* by default, we use the global manager */
  impl->local_manager = FALSE;
  
  impl->limit = FALLBACK_ITEM_LIMIT;
  impl->sort_type = GTK_RECENT_SORT_NONE;

  impl->show_icons = TRUE;
  impl->show_private = FALSE;
  impl->show_not_found = TRUE;
  impl->show_tips = FALSE;
  impl->select_multiple = FALSE;
  impl->local_only = TRUE;
  
  impl->icon_size = FALLBACK_ICON_SIZE;
  impl->icon_theme = NULL;
  
  impl->current_filter = NULL;

  impl->recent_items = NULL;
  impl->n_recent_items = 0;
  impl->loaded_items = 0;
  
  impl->load_state = LOAD_EMPTY;
}

static GObject *
gtk_recent_chooser_default_constructor (GType                  type,
				        guint                  n_params,
				        GObjectConstructParam *params)
{
  GObjectClass *parent_class;
  GtkRecentChooserDefault *impl;
  GObject *object;
  GtkWidget *scrollw;
  GtkCellRenderer *renderer;

  parent_class = G_OBJECT_CLASS (_gtk_recent_chooser_default_parent_class);
  object = parent_class->constructor (type, n_params, params);
  impl = GTK_RECENT_CHOOSER_DEFAULT (object);
  
  g_assert (impl->manager);
  
  gtk_widget_push_composite_child ();

  impl->limit = get_recent_files_limit (GTK_WIDGET (impl));
  
  scrollw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrollw),
  				       GTK_SHADOW_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrollw),
  				  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start (GTK_BOX (impl), scrollw, TRUE, TRUE, 0);
  gtk_widget_show (scrollw);
  
  impl->recent_view = gtk_tree_view_new ();
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (impl->recent_view), FALSE);
  g_signal_connect (impl->recent_view, "row-activated",
                    G_CALLBACK (row_activated_cb), impl);
  g_signal_connect (impl->recent_view, "popup-menu",
  		    G_CALLBACK (recent_view_popup_menu_cb), impl);
  g_signal_connect (impl->recent_view, "button-press-event",
  		    G_CALLBACK (recent_view_button_press_cb), impl);
  g_signal_connect (impl->recent_view, "drag-begin",
		    G_CALLBACK (recent_view_drag_begin_cb), impl);
  g_signal_connect (impl->recent_view, "drag-data-get",
		    G_CALLBACK (recent_view_drag_data_get_cb), impl);

  g_object_set (impl->recent_view, "has-tooltip", TRUE, NULL);
  g_signal_connect (impl->recent_view, "query-tooltip",
                    G_CALLBACK (recent_view_query_tooltip_cb), impl);

  g_object_set_data (G_OBJECT (impl->recent_view),
                     "GtkRecentChooserDefault", impl);
  
  gtk_container_add (GTK_CONTAINER (scrollw), impl->recent_view);
  gtk_widget_show (impl->recent_view);
  
  impl->icon_column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_expand (impl->icon_column, FALSE);
  gtk_tree_view_column_set_resizable (impl->icon_column, FALSE);
  
  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_tree_view_column_pack_start (impl->icon_column, renderer, FALSE);
  gtk_tree_view_column_set_cell_data_func (impl->icon_column,
  					   renderer,
  					   recent_icon_data_func,
  					   impl,
  					   NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (impl->recent_view),
                               impl->icon_column);
  
  impl->meta_column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_expand (impl->meta_column, TRUE);
  gtk_tree_view_column_set_resizable (impl->meta_column, FALSE);
  
  impl->meta_renderer = gtk_cell_renderer_text_new ();
  g_object_set (G_OBJECT (impl->meta_renderer),
                "ellipsize", PANGO_ELLIPSIZE_END,
                NULL);
  gtk_tree_view_column_pack_start (impl->meta_column, impl->meta_renderer, TRUE);
  gtk_tree_view_column_set_cell_data_func (impl->meta_column,
  					   impl->meta_renderer,
  					   recent_meta_data_func,
  					   impl,
  					   NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (impl->recent_view),
                               impl->meta_column);
  
  impl->selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->recent_view));
  gtk_tree_selection_set_mode (impl->selection, GTK_SELECTION_SINGLE);
  g_signal_connect (impl->selection, "changed", G_CALLBACK (selection_changed_cb), impl);

  /* drag and drop */
  gtk_drag_source_set (impl->recent_view,
		       GDK_BUTTON1_MASK,
		       NULL, 0,
		       GDK_ACTION_COPY);
  gtk_drag_source_add_uri_targets (impl->recent_view);

  impl->filter_combo_hbox = gtk_hbox_new (FALSE, 12);
  
  impl->filter_combo = gtk_combo_box_text_new ();
  gtk_combo_box_set_focus_on_click (GTK_COMBO_BOX (impl->filter_combo), FALSE);
  g_signal_connect (impl->filter_combo, "changed",
                    G_CALLBACK (filter_combo_changed_cb), impl);
  gtk_widget_set_tooltip_text (impl->filter_combo,
		               _("Select which type of documents are shown"));
  
  gtk_box_pack_end (GTK_BOX (impl->filter_combo_hbox),
                    impl->filter_combo,
                    FALSE, FALSE, 0);
  gtk_widget_show (impl->filter_combo);
  
  gtk_box_pack_end (GTK_BOX (impl), impl->filter_combo_hbox, FALSE, FALSE, 0);
  
  gtk_widget_pop_composite_child ();
  
  impl->recent_store = gtk_list_store_new (N_RECENT_COLUMNS,
  					   G_TYPE_STRING,       /* uri */
  					   G_TYPE_STRING,       /* display_name */
  					   GTK_TYPE_RECENT_INFO /* info */);
  
  return object;
}

static void
gtk_recent_chooser_default_set_property (GObject      *object,
				         guint         prop_id,
					 const GValue *value,
					 GParamSpec   *pspec)
{
  GtkRecentChooserDefault *impl = GTK_RECENT_CHOOSER_DEFAULT (object);
  
  switch (prop_id)
    {
    case GTK_RECENT_CHOOSER_PROP_RECENT_MANAGER:
      set_recent_manager (impl, g_value_get_object (value));
      break;
    case GTK_RECENT_CHOOSER_PROP_SHOW_PRIVATE:
      impl->show_private = g_value_get_boolean (value);
      if (impl->recent_popup_menu_show_private_item)
	{
          GtkCheckMenuItem *item = GTK_CHECK_MENU_ITEM (impl->recent_popup_menu_show_private_item);
	  g_signal_handlers_block_by_func (item, G_CALLBACK (show_private_toggled_cb), impl);
          gtk_check_menu_item_set_active (item, impl->show_private);
	  g_signal_handlers_unblock_by_func (item, G_CALLBACK (show_private_toggled_cb), impl);
        }
      reload_recent_items (impl);
      break;
    case GTK_RECENT_CHOOSER_PROP_SHOW_NOT_FOUND:
      impl->show_not_found = g_value_get_boolean (value);
      reload_recent_items (impl);
      break;
    case GTK_RECENT_CHOOSER_PROP_SHOW_TIPS:
      impl->show_tips = g_value_get_boolean (value);
      break;
    case GTK_RECENT_CHOOSER_PROP_SHOW_ICONS:
      impl->show_icons = g_value_get_boolean (value);
      gtk_tree_view_column_set_visible (impl->icon_column, impl->show_icons);
      break;
    case GTK_RECENT_CHOOSER_PROP_SELECT_MULTIPLE:
      impl->select_multiple = g_value_get_boolean (value);
      
      if (impl->select_multiple)
        gtk_tree_selection_set_mode (impl->selection, GTK_SELECTION_MULTIPLE);
      else
        gtk_tree_selection_set_mode (impl->selection, GTK_SELECTION_SINGLE);
      break;
    case GTK_RECENT_CHOOSER_PROP_LOCAL_ONLY:
      impl->local_only = g_value_get_boolean (value);
      reload_recent_items (impl);
      break;
    case GTK_RECENT_CHOOSER_PROP_LIMIT:
      impl->limit = g_value_get_int (value);
      impl->limit_set = TRUE;
      reload_recent_items (impl);
      break;
    case GTK_RECENT_CHOOSER_PROP_SORT_TYPE:
      chooser_set_sort_type (impl, g_value_get_enum (value));
      break;
    case GTK_RECENT_CHOOSER_PROP_FILTER:
      set_current_filter (impl, g_value_get_object (value));
      break;
    case PROP_ACTIVATABLE_RELATED_ACTION:
      _gtk_recent_chooser_set_related_action (GTK_RECENT_CHOOSER (impl), g_value_get_object (value));
      break;
    case PROP_ACTIVATABLE_USE_ACTION_APPEARANCE: 
      _gtk_recent_chooser_set_use_action_appearance (GTK_RECENT_CHOOSER (impl), g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_recent_chooser_default_get_property (GObject    *object,
					 guint       prop_id,
					 GValue     *value,
					 GParamSpec *pspec)
{
  GtkRecentChooserDefault *impl = GTK_RECENT_CHOOSER_DEFAULT (object);
  
  switch (prop_id)
    {
    case GTK_RECENT_CHOOSER_PROP_LIMIT:
      g_value_set_int (value, impl->limit);
      break;
    case GTK_RECENT_CHOOSER_PROP_SORT_TYPE:
      g_value_set_enum (value, impl->sort_type);
      break;
    case GTK_RECENT_CHOOSER_PROP_SHOW_PRIVATE:
      g_value_set_boolean (value, impl->show_private);
      break;
    case GTK_RECENT_CHOOSER_PROP_SHOW_ICONS:
      g_value_set_boolean (value, impl->show_icons);
      break;
    case GTK_RECENT_CHOOSER_PROP_SHOW_NOT_FOUND:
      g_value_set_boolean (value, impl->show_not_found);
      break;
    case GTK_RECENT_CHOOSER_PROP_SHOW_TIPS:
      g_value_set_boolean (value, impl->show_tips);
      break;
    case GTK_RECENT_CHOOSER_PROP_LOCAL_ONLY:
      g_value_set_boolean (value, impl->local_only);
      break;
    case GTK_RECENT_CHOOSER_PROP_SELECT_MULTIPLE:
      g_value_set_boolean (value, impl->select_multiple);
      break;
    case GTK_RECENT_CHOOSER_PROP_FILTER:
      g_value_set_object (value, impl->current_filter);
      break;
    case PROP_ACTIVATABLE_RELATED_ACTION:
      g_value_set_object (value, _gtk_recent_chooser_get_related_action (GTK_RECENT_CHOOSER (impl)));
      break;
    case PROP_ACTIVATABLE_USE_ACTION_APPEARANCE: 
      g_value_set_boolean (value, _gtk_recent_chooser_get_use_action_appearance (GTK_RECENT_CHOOSER (impl)));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_recent_chooser_default_dispose (GObject *object)
{
  GtkRecentChooserDefault *impl = GTK_RECENT_CHOOSER_DEFAULT (object);

  if (impl->load_id)
    {
      g_source_remove (impl->load_id);
      impl->load_state = LOAD_EMPTY;
      impl->load_id = 0;
    }

  if (impl->recent_items)
    {
      g_list_foreach (impl->recent_items, (GFunc) gtk_recent_info_unref, NULL);
      g_list_free (impl->recent_items);
      impl->recent_items = NULL;
    }

  if (impl->manager && impl->manager_changed_id)
    {
      g_signal_handler_disconnect (impl->manager, impl->manager_changed_id);
      impl->manager_changed_id = 0;
    }

  if (impl->filters)
    {
      g_slist_foreach (impl->filters, (GFunc) g_object_unref, NULL);
      g_slist_free (impl->filters);
      impl->filters = NULL;
    }
  
  if (impl->current_filter)
    {
      g_object_unref (impl->current_filter);
      impl->current_filter = NULL;
    }

  if (impl->recent_store)
    {
      g_object_unref (impl->recent_store);
      impl->recent_store = NULL;
    }

  G_OBJECT_CLASS (_gtk_recent_chooser_default_parent_class)->dispose (object);
}

static void
gtk_recent_chooser_default_finalize (GObject *object)
{
  GtkRecentChooserDefault *impl = GTK_RECENT_CHOOSER_DEFAULT (object);

  impl->manager = NULL; 
  
  if (impl->sort_data_destroy)
    {
      impl->sort_data_destroy (impl->sort_data);
      impl->sort_data_destroy = NULL;
    }
  
  impl->sort_data = NULL;
  impl->sort_func = NULL;
  
  G_OBJECT_CLASS (_gtk_recent_chooser_default_parent_class)->finalize (object);
}

/* override GtkWidget::show_all since we have internal widgets we wish to keep
 * hidden unless we decide otherwise, like the filter combo box.
 */
static void
gtk_recent_chooser_default_show_all (GtkWidget *widget)
{
  gtk_widget_show (widget);
}



/* Shows an error dialog set as transient for the specified window */
static void
error_message_with_parent (GtkWindow   *parent,
			   const gchar *msg,
			   const gchar *detail)
{
  GtkWidget *dialog;

  dialog = gtk_message_dialog_new (parent,
				   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				   GTK_MESSAGE_ERROR,
				   GTK_BUTTONS_OK,
				   "%s",
				   msg);
  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
					    "%s", detail);

  if (parent->group)
    gtk_window_group_add_window (parent->group, GTK_WINDOW (dialog));

  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}

/* Returns a toplevel GtkWindow, or NULL if none */
static GtkWindow *
get_toplevel (GtkWidget *widget)
{
  GtkWidget *toplevel;

  toplevel = gtk_widget_get_toplevel (widget);
  if (!gtk_widget_is_toplevel (toplevel))
    return NULL;
  else
    return GTK_WINDOW (toplevel);
}

/* Shows an error dialog for the file chooser */
static void
error_message (GtkRecentChooserDefault *impl,
	       const gchar             *msg,
	       const gchar             *detail)
{
  error_message_with_parent (get_toplevel (GTK_WIDGET (impl)), msg, detail);
}

static void
set_busy_cursor (GtkRecentChooserDefault *impl,
		 gboolean                 show_busy_cursor)
{
  GtkWindow *toplevel;
  GdkDisplay *display;
  GdkCursor *cursor;

  toplevel = get_toplevel (GTK_WIDGET (impl));
  if (!toplevel || !gtk_widget_get_realized (GTK_WIDGET (toplevel)))
    return;
  
  display = gtk_widget_get_display (GTK_WIDGET (toplevel));
  
  cursor = NULL;
  if (show_busy_cursor)
    cursor = gdk_cursor_new_for_display (display, GDK_WATCH);

  gdk_window_set_cursor (GTK_WIDGET (toplevel)->window, cursor);
  gdk_display_flush (display);

  if (cursor)
    gdk_cursor_unref (cursor);
}

static void
chooser_set_model (GtkRecentChooserDefault *impl)
{
  g_assert (impl->recent_store != NULL);
  g_assert (impl->load_state == LOAD_LOADING);

  gtk_tree_view_set_model (GTK_TREE_VIEW (impl->recent_view),
                           GTK_TREE_MODEL (impl->recent_store));
  gtk_tree_view_columns_autosize (GTK_TREE_VIEW (impl->recent_view));
  gtk_tree_view_set_enable_search (GTK_TREE_VIEW (impl->recent_view), TRUE);
  gtk_tree_view_set_search_column (GTK_TREE_VIEW (impl->recent_view),
  				   RECENT_DISPLAY_NAME_COLUMN);

  impl->load_state = LOAD_FINISHED;
}

static gboolean
load_recent_items (gpointer user_data)
{
  GtkRecentChooserDefault *impl;
  GtkRecentInfo *info;
  GtkTreeIter iter;
  const gchar *uri, *name;
  gboolean retval;
  
  impl = GTK_RECENT_CHOOSER_DEFAULT (user_data);
  
  g_assert ((impl->load_state == LOAD_EMPTY) ||
            (impl->load_state == LOAD_PRELOAD));
  
  /* store the items for multiple runs */
  if (!impl->recent_items)
    {
      impl->recent_items = gtk_recent_chooser_get_items (GTK_RECENT_CHOOSER (impl));
      if (!impl->recent_items)
        {
	  impl->load_state = LOAD_FINISHED;
          
          return FALSE;
        }
        
      impl->n_recent_items = g_list_length (impl->recent_items);
      impl->loaded_items = 0;
      impl->load_state = LOAD_PRELOAD;
    }
  
  info = (GtkRecentInfo *) g_list_nth_data (impl->recent_items,
                                            impl->loaded_items);
  g_assert (info);

  uri = gtk_recent_info_get_uri (info);
  name = gtk_recent_info_get_display_name (info);
  
  /* at this point, everything goes inside the model; operations on the
   * visualization of items inside the model are done in the cell data
   * funcs (remember that there are two of those: one for the icon and
   * one for the text), while the filtering is done only when a filter
   * is actually loaded. */
  gtk_list_store_append (impl->recent_store, &iter);
  gtk_list_store_set (impl->recent_store, &iter,
  		      RECENT_URI_COLUMN, uri,           /* uri  */
  		      RECENT_DISPLAY_NAME_COLUMN, name, /* display_name */
  		      RECENT_INFO_COLUMN, info,         /* info */
  		      -1);
  
  impl->loaded_items += 1;

  if (impl->loaded_items == impl->n_recent_items)
    {
      /* we have finished loading, so we remove the items cache */
      impl->load_state = LOAD_LOADING;
      
      g_list_foreach (impl->recent_items,
      		      (GFunc) gtk_recent_info_unref,
      		      NULL);
      g_list_free (impl->recent_items);
      
      impl->recent_items = NULL;
      impl->n_recent_items = 0;
      impl->loaded_items = 0;

      /* load the filled up model */
      chooser_set_model (impl);

      retval = FALSE;
      impl->load_id = 0;
    }
  else
    {
      /* we did not finish, so continue loading */
      retval = TRUE;
    }
  
  return retval;
}

static void
cleanup_after_load (gpointer user_data)
{
  GtkRecentChooserDefault *impl;
  
  impl = GTK_RECENT_CHOOSER_DEFAULT (user_data);

  if (impl->load_id != 0)
    {
      g_assert ((impl->load_state == LOAD_EMPTY) ||
                (impl->load_state == LOAD_PRELOAD) ||
		(impl->load_state == LOAD_LOADING) ||
		(impl->load_state == LOAD_FINISHED));
      
      /* we have officialy finished loading all the items,
       * so we can reset the state machine
       */
      g_source_remove (impl->load_id);
      impl->load_id = 0;
      impl->load_state = LOAD_EMPTY;
    }
  else
    g_assert ((impl->load_state == LOAD_EMPTY) ||
	      (impl->load_state == LOAD_LOADING) ||
	      (impl->load_state == LOAD_FINISHED));

  set_busy_cursor (impl, FALSE);
}

/* clears the current model and reloads the recently used resources */
static void
reload_recent_items (GtkRecentChooserDefault *impl)
{
  GtkWidget *widget;

  /* reload is already in progress - do not disturb */
  if (impl->load_id)
    return;
  
  widget = GTK_WIDGET (impl);

  gtk_tree_view_set_model (GTK_TREE_VIEW (impl->recent_view), NULL);
  gtk_list_store_clear (impl->recent_store);
  
  if (!impl->icon_theme)
    impl->icon_theme = get_icon_theme_for_widget (widget);

  impl->icon_size = get_icon_size_for_widget (widget,
		  			      GTK_ICON_SIZE_BUTTON);

  if (!impl->limit_set)
    impl->limit = get_recent_files_limit (widget);

  set_busy_cursor (impl, TRUE);

  impl->load_state = LOAD_EMPTY;
  impl->load_id = gdk_threads_add_idle_full (G_PRIORITY_HIGH_IDLE + 30,
                                             load_recent_items,
                                             impl,
                                             cleanup_after_load);
}

/* taken form gtkfilechooserdialog.c */
static void
set_default_size (GtkRecentChooserDefault *impl)
{
  GtkWidget *widget;
  gint width, height;
  gint font_size;
  GdkScreen *screen;
  gint monitor_num;
  GtkRequisition req;
  GdkRectangle monitor;

  widget = GTK_WIDGET (impl);

  /* Size based on characters and the icon size */
  font_size = pango_font_description_get_size (widget->style->font_desc);
  font_size = PANGO_PIXELS (font_size);

  width = impl->icon_size + font_size * NUM_CHARS;
  height = (impl->icon_size + font_size) * NUM_LINES;

  /* Use at least the requisition size... */
  gtk_widget_size_request (widget, &req);
  width = MAX (width, req.width);
  height = MAX (height, req.height);

  /* ... but no larger than the monitor */
  screen = gtk_widget_get_screen (widget);
  monitor_num = gdk_screen_get_monitor_at_window (screen, widget->window);

  gdk_screen_get_monitor_geometry (screen, monitor_num, &monitor);

  width = MIN (width, monitor.width * 3 / 4);
  height = MIN (height, monitor.height * 3 / 4);

  /* Set size */
  gtk_widget_set_size_request (impl->recent_view, width, height);
}

static void
gtk_recent_chooser_default_map (GtkWidget *widget)
{
  GtkRecentChooserDefault *impl = GTK_RECENT_CHOOSER_DEFAULT (widget);

  GTK_WIDGET_CLASS (_gtk_recent_chooser_default_parent_class)->map (widget);

  /* reloads everything */
  reload_recent_items (impl);

  set_default_size (impl);
}

static void
recent_icon_data_func (GtkTreeViewColumn *tree_column,
		       GtkCellRenderer   *cell,
		       GtkTreeModel      *model,
		       GtkTreeIter       *iter,
		       gpointer           user_data)
{
  GtkRecentChooserDefault *impl = GTK_RECENT_CHOOSER_DEFAULT (user_data);
  GtkRecentInfo *info = NULL;
  GdkPixbuf *pixbuf;
  
  gtk_tree_model_get (model, iter,
                      RECENT_INFO_COLUMN, &info,
                      -1);
  g_assert (info != NULL);
  
  pixbuf = gtk_recent_info_get_icon (info, impl->icon_size);
  
  g_object_set (cell,
                "pixbuf", pixbuf,
                NULL);
  
  if (pixbuf)  
    g_object_unref (pixbuf);

  gtk_recent_info_unref (info);
}

static void
recent_meta_data_func (GtkTreeViewColumn *tree_column,
		       GtkCellRenderer   *cell,
		       GtkTreeModel      *model,
		       GtkTreeIter       *iter,
		       gpointer           user_data)
{
  GtkRecentInfo *info = NULL;
  gchar *name;
  
  gtk_tree_model_get (model, iter,
                      RECENT_DISPLAY_NAME_COLUMN, &name,
                      RECENT_INFO_COLUMN, &info,
                      -1);
  g_assert (info != NULL);
  
  if (!name)
    name = gtk_recent_info_get_short_name (info);

  g_object_set (cell, "text", name, NULL);
  
  g_free (name);
  gtk_recent_info_unref (info);
}


static gchar *
gtk_recent_chooser_default_get_current_uri (GtkRecentChooser *chooser)
{
  GtkRecentChooserDefault *impl = GTK_RECENT_CHOOSER_DEFAULT (chooser);
  
  g_assert (impl->selection != NULL);
  
  if (!impl->select_multiple)
    {
      GtkTreeModel *model;
      GtkTreeIter iter;
      gchar *uri = NULL;
      
      if (!gtk_tree_selection_get_selected (impl->selection, &model, &iter))
        return NULL;
      
      gtk_tree_model_get (model, &iter, RECENT_URI_COLUMN, &uri, -1);
      
      return uri;
    }
  
  return NULL;
}

typedef struct
{
  guint found : 1;
  guint do_select : 1;
  guint do_activate : 1;
  
  gchar *uri;
  
  GtkRecentChooserDefault *impl;
} SelectURIData;

static gboolean
scan_for_uri_cb (GtkTreeModel *model,
		 GtkTreePath  *path,
		 GtkTreeIter  *iter,
		 gpointer      user_data)
{
  SelectURIData *select_data = (SelectURIData *) user_data;
  gchar *uri = NULL;
  
  if (!select_data)
    return TRUE;
  
  if (select_data->found)
    return TRUE;
  
  gtk_tree_model_get (model, iter, RECENT_URI_COLUMN, &uri, -1);
  if (!uri)
    return FALSE;
  
  if (strcmp (uri, select_data->uri) == 0)
    {
      select_data->found = TRUE;
      
      if (select_data->do_activate)
        gtk_tree_view_row_activated (GTK_TREE_VIEW (select_data->impl->recent_view),
        			     path,
        			     select_data->impl->meta_column);
      
      if (select_data->do_select)
        gtk_tree_selection_select_path (select_data->impl->selection, path);
      else
        gtk_tree_selection_unselect_path (select_data->impl->selection, path);

      g_free (uri);
      
      return TRUE;
    }

  g_free (uri);
  
  return FALSE;
}

static gboolean
gtk_recent_chooser_default_set_current_uri (GtkRecentChooser  *chooser,
					    const gchar       *uri,
					    GError           **error)
{
  GtkRecentChooserDefault *impl = GTK_RECENT_CHOOSER_DEFAULT (chooser);
  SelectURIData *data;
  
  data = g_new0 (SelectURIData, 1);
  data->uri = g_strdup (uri);
  data->impl = impl;
  data->found = FALSE;
  data->do_activate = TRUE;
  data->do_select = TRUE;
  
  gtk_tree_model_foreach (GTK_TREE_MODEL (impl->recent_store),
  			  scan_for_uri_cb,
  			  data);
  
  if (!data->found)
    {
      g_free (data->uri);
      g_free (data);
      
      g_set_error (error, GTK_RECENT_CHOOSER_ERROR,
      		   GTK_RECENT_CHOOSER_ERROR_NOT_FOUND,
      		   _("No item for URI '%s' found"),
      		   uri);
      return FALSE;
    }
  
  g_free (data->uri);
  g_free (data);

  return TRUE;
}

static gboolean
gtk_recent_chooser_default_select_uri (GtkRecentChooser  *chooser,
				       const gchar       *uri,
				       GError           **error)
{
  GtkRecentChooserDefault *impl = GTK_RECENT_CHOOSER_DEFAULT (chooser);
  SelectURIData *data;
  
  data = g_new0 (SelectURIData, 1);
  data->uri = g_strdup (uri);
  data->impl = impl;
  data->found = FALSE;
  data->do_activate = FALSE;
  data->do_select = TRUE;
  
  gtk_tree_model_foreach (GTK_TREE_MODEL (impl->recent_store),
  			  scan_for_uri_cb,
  			  data);
  
  if (!data->found)
    {
      g_free (data->uri);
      g_free (data);
      
      g_set_error (error, GTK_RECENT_CHOOSER_ERROR,
      		   GTK_RECENT_CHOOSER_ERROR_NOT_FOUND,
      		   _("No item for URI '%s' found"),
      		   uri);
      return FALSE;
    }
  
  g_free (data->uri);
  g_free (data);

  return TRUE;
}

static void
gtk_recent_chooser_default_unselect_uri (GtkRecentChooser *chooser,
					 const gchar      *uri)
{
  GtkRecentChooserDefault *impl = GTK_RECENT_CHOOSER_DEFAULT (chooser);
  SelectURIData *data;
  
  data = g_new0 (SelectURIData, 1);
  data->uri = g_strdup (uri);
  data->impl = impl;
  data->found = FALSE;
  data->do_activate = FALSE;
  data->do_select = FALSE;
  
  gtk_tree_model_foreach (GTK_TREE_MODEL (impl->recent_store),
  			  scan_for_uri_cb,
  			  data);
  
  g_free (data->uri);
  g_free (data);
}

static void
gtk_recent_chooser_default_select_all (GtkRecentChooser *chooser)
{
  GtkRecentChooserDefault *impl = GTK_RECENT_CHOOSER_DEFAULT (chooser);
  
  if (!impl->select_multiple)
    return;
  
  gtk_tree_selection_select_all (impl->selection);
}

static void
gtk_recent_chooser_default_unselect_all (GtkRecentChooser *chooser)
{
  GtkRecentChooserDefault *impl = GTK_RECENT_CHOOSER_DEFAULT (chooser);
  
  gtk_tree_selection_unselect_all (impl->selection);
}

static void
gtk_recent_chooser_default_set_sort_func (GtkRecentChooser  *chooser,
					  GtkRecentSortFunc  sort_func,
					  gpointer           sort_data,
					  GDestroyNotify     data_destroy)
{
  GtkRecentChooserDefault *impl = GTK_RECENT_CHOOSER_DEFAULT (chooser);
  
  if (impl->sort_data_destroy)
    {
      impl->sort_data_destroy (impl->sort_data);
      impl->sort_data_destroy = NULL;
    }
      
  impl->sort_func = NULL;
  impl->sort_data = NULL;
  
  if (sort_func)
    {
      impl->sort_func = sort_func;
      impl->sort_data = sort_data;
      impl->sort_data_destroy = data_destroy;
    }
}

static GList *
gtk_recent_chooser_default_get_items (GtkRecentChooser *chooser)
{
  GtkRecentChooserDefault *impl;

  impl = GTK_RECENT_CHOOSER_DEFAULT (chooser);

  return _gtk_recent_chooser_get_items (chooser,
                                        impl->current_filter,
                                        impl->sort_func,
                                        impl->sort_data);
}

static GtkRecentManager *
gtk_recent_chooser_default_get_recent_manager (GtkRecentChooser *chooser)
{
  return GTK_RECENT_CHOOSER_DEFAULT (chooser)->manager;
}

static void
show_filters (GtkRecentChooserDefault *impl,
              gboolean                 show)
{
  if (show)
    gtk_widget_show (impl->filter_combo_hbox);
  else
    gtk_widget_hide (impl->filter_combo_hbox);
}

static void
gtk_recent_chooser_default_add_filter (GtkRecentChooser *chooser,
				       GtkRecentFilter  *filter)
{
  GtkRecentChooserDefault *impl;
  const gchar *name;

  impl = GTK_RECENT_CHOOSER_DEFAULT (chooser);
  
  if (g_slist_find (impl->filters, filter))
    {
      g_warning ("gtk_recent_chooser_add_filter() called on filter already in list\n");
      return;
    }
  
  g_object_ref_sink (filter);
  impl->filters = g_slist_append (impl->filters, filter);
  
  /* display new filter */
  name = gtk_recent_filter_get_name (filter);
  if (!name)
    name = _("Untitled filter");

  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (impl->filter_combo), name);

  if (!g_slist_find (impl->filters, impl->current_filter))
    set_current_filter (impl, filter);
  
  show_filters (impl, TRUE);
}

static void
gtk_recent_chooser_default_remove_filter (GtkRecentChooser *chooser,
					  GtkRecentFilter  *filter)
{
  GtkRecentChooserDefault *impl = GTK_RECENT_CHOOSER_DEFAULT (chooser);
  GtkTreeModel *model;
  GtkTreeIter iter;
  gint filter_idx;
  
  filter_idx = g_slist_index (impl->filters, filter);
  
  if (filter_idx < 0)
    {
      g_warning ("gtk_recent_chooser_remove_filter() called on filter not in list\n");
      return;  
    }
  
  impl->filters = g_slist_remove (impl->filters, filter);
  
  if (filter == impl->current_filter)
    {
      if (impl->filters)
        set_current_filter (impl, impl->filters->data);
      else
        set_current_filter (impl, NULL);
    }
  
  model = gtk_combo_box_get_model (GTK_COMBO_BOX (impl->filter_combo));
  gtk_tree_model_iter_nth_child (model, &iter, NULL, filter_idx);
  gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
  
  g_object_unref (filter);
  
  if (!impl->filters)
    show_filters (impl, FALSE);
}

static GSList *
gtk_recent_chooser_default_list_filters (GtkRecentChooser *chooser)
{
  GtkRecentChooserDefault *impl = GTK_RECENT_CHOOSER_DEFAULT (chooser);
  
  return g_slist_copy (impl->filters);
}

static void
set_current_filter (GtkRecentChooserDefault *impl,
		    GtkRecentFilter         *filter)
{
  if (impl->current_filter != filter)
    {
      gint filter_idx;
      
      filter_idx = g_slist_index (impl->filters, filter);
      if (impl->filters && filter && filter_idx < 0)
        return;
      
      if (impl->current_filter)
        g_object_unref (impl->current_filter);
      
      impl->current_filter = filter;
      
      if (impl->current_filter)     
        {
          g_object_ref_sink (impl->current_filter);
        }
      
      if (impl->filters)
        gtk_combo_box_set_active (GTK_COMBO_BOX (impl->filter_combo),
                                  filter_idx);
      
      if (impl->recent_store)
        reload_recent_items (impl);

      g_object_notify (G_OBJECT (impl), "filter");
    }
}

static void
chooser_set_sort_type (GtkRecentChooserDefault *impl,
		       GtkRecentSortType        sort_type)
{
  if (impl->sort_type != sort_type)
    {
      impl->sort_type = sort_type;
      reload_recent_items (impl);

      g_object_notify (G_OBJECT (impl), "sort-type");
    }
}


static GtkIconTheme *
get_icon_theme_for_widget (GtkWidget *widget)
{
  if (gtk_widget_has_screen (widget))
    return gtk_icon_theme_get_for_screen (gtk_widget_get_screen (widget));

  return gtk_icon_theme_get_default ();
}

static gint
get_icon_size_for_widget (GtkWidget   *widget,
			  GtkIconSize  icon_size)
{
  GtkSettings *settings;
  gint width, height;

  if (gtk_widget_has_screen (widget))
    settings = gtk_settings_get_for_screen (gtk_widget_get_screen (widget));
  else
    settings = gtk_settings_get_default ();

  if (gtk_icon_size_lookup_for_settings (settings, icon_size,
                                         &width, &height))
    return MAX (width, height);

  return FALLBACK_ICON_SIZE;
}

static gint
get_recent_files_limit (GtkWidget *widget)
{
  GtkSettings *settings;
  gint limit;

  if (gtk_widget_has_screen (widget))
    settings = gtk_settings_get_for_screen (gtk_widget_get_screen (widget));
  else
    settings = gtk_settings_get_default ();
  
  g_object_get (G_OBJECT (settings), "gtk-recent-files-limit", &limit, NULL);

  return limit;
}

static void
recent_manager_changed_cb (GtkRecentManager *manager,
			   gpointer          user_data)
{
  GtkRecentChooserDefault *impl = GTK_RECENT_CHOOSER_DEFAULT (user_data);

  reload_recent_items (impl);
}

static void
selection_changed_cb (GtkTreeSelection *selection,
		      gpointer          user_data)
{
  _gtk_recent_chooser_selection_changed (GTK_RECENT_CHOOSER (user_data));
}

static void
row_activated_cb (GtkTreeView       *tree_view,
		  GtkTreePath       *tree_path,
		  GtkTreeViewColumn *tree_column,
		  gpointer           user_data)
{
  _gtk_recent_chooser_item_activated (GTK_RECENT_CHOOSER (user_data));
}

static void
filter_combo_changed_cb (GtkComboBox *combo_box,
			 gpointer     user_data)
{
  GtkRecentChooserDefault *impl;
  gint new_index;
  GtkRecentFilter *filter;
  
  impl = GTK_RECENT_CHOOSER_DEFAULT (user_data);
  
  new_index = gtk_combo_box_get_active (combo_box);
  filter = g_slist_nth_data (impl->filters, new_index);
  
  set_current_filter (impl, filter);
}

static GdkPixbuf *
get_drag_pixbuf (GtkRecentChooserDefault *impl)
{
  GtkRecentInfo *info;
  GdkPixbuf *retval;
  gint size;
  
  g_assert (GTK_IS_RECENT_CHOOSER_DEFAULT (impl));

  info = gtk_recent_chooser_get_current_item (GTK_RECENT_CHOOSER (impl));
  if (!info)
    return NULL;

  size = get_icon_size_for_widget (GTK_WIDGET (impl), GTK_ICON_SIZE_DND);

  retval = gtk_recent_info_get_icon (info, size);
  gtk_recent_info_unref (info);

  return retval;
}

static void
recent_view_drag_begin_cb (GtkWidget      *widget,
			   GdkDragContext *context,
			   gpointer        user_data)
{
  GtkRecentChooserDefault *impl = GTK_RECENT_CHOOSER_DEFAULT (user_data);
  GdkPixbuf *pixbuf;

  pixbuf = get_drag_pixbuf (impl);
  if (pixbuf)
    {
      gtk_drag_set_icon_pixbuf (context, pixbuf, 0, 0);
      g_object_unref (pixbuf);
    }
  else
    gtk_drag_set_icon_default (context);
}

typedef struct
{
  gchar **uri_list;
  gsize next_pos;
} DragData;

static void
append_uri_to_urilist (GtkTreeModel *model,
		       GtkTreePath  *path,
		       GtkTreeIter  *iter,
		       gpointer      user_data)
{
  DragData *drag_data = (DragData *) user_data;
  GtkTreeModel *child_model;
  GtkTreeIter child_iter;
  gchar *uri = NULL;
  gsize pos;

  child_model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (model));
  gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model),
  						    &child_iter,
  						    iter);
  gtk_tree_model_get (child_model, &child_iter,
  		      RECENT_URI_COLUMN, &uri,
  		      -1);
  g_assert (uri != NULL);

  pos = drag_data->next_pos;
  drag_data->uri_list[pos] = g_strdup (uri);
  drag_data->next_pos = pos + 1;
}

static void
recent_view_drag_data_get_cb (GtkWidget        *widget,
			      GdkDragContext   *context,
			      GtkSelectionData *selection_data,
			      guint             info,
			      guint32           time_,
			      gpointer          data)
{
  GtkRecentChooserDefault *impl = GTK_RECENT_CHOOSER_DEFAULT (data);
  DragData drag_data;
  gsize n_uris;
  
  n_uris = gtk_tree_selection_count_selected_rows (impl->selection);
  if (n_uris == 0)
    return;

  drag_data.uri_list = g_new0 (gchar *, n_uris + 1);
  drag_data.next_pos = 0;
  
  gtk_tree_selection_selected_foreach (impl->selection,
      				       append_uri_to_urilist,
      				       &drag_data);
  
  gtk_selection_data_set_uris (selection_data, drag_data.uri_list);

  g_strfreev (drag_data.uri_list);
}

static gboolean
recent_view_query_tooltip_cb (GtkWidget  *widget,
                              gint        x,
                              gint        y,
                              gboolean    keyboard_tip,
                              GtkTooltip *tooltip,
                              gpointer    user_data)
{
  GtkRecentChooserDefault *impl = user_data;
  GtkTreeView *tree_view;
  GtkTreeIter iter;
  GtkTreePath *path = NULL;
  GtkRecentInfo *info = NULL;
  gchar *uri_display;

  if (!impl->show_tips)
    return FALSE;

  tree_view = GTK_TREE_VIEW (impl->recent_view);

  gtk_tree_view_get_tooltip_context (tree_view,
                                     &x, &y,
                                     keyboard_tip,
                                     NULL, &path, NULL);
  if (!path)
    return FALSE;

  if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (impl->recent_store), &iter, path))
    {
      gtk_tree_path_free (path);
      return FALSE;
    }

  gtk_tree_model_get (GTK_TREE_MODEL (impl->recent_store), &iter,
                      RECENT_INFO_COLUMN, &info,
                      -1);

  uri_display = gtk_recent_info_get_uri_display (info);
  
  gtk_tooltip_set_text (tooltip, uri_display);
  gtk_tree_view_set_tooltip_row (tree_view, tooltip, path);

  g_free (uri_display);
  gtk_tree_path_free (path);
  gtk_recent_info_unref (info);

  return TRUE;
}

static void
remove_selected_from_list (GtkRecentChooserDefault *impl)
{
  gchar *uri;
  GError *err;
  
  if (impl->select_multiple)
    return;
  
  uri = gtk_recent_chooser_get_current_uri (GTK_RECENT_CHOOSER (impl));
  if (!uri)
    return;
  
  err = NULL;
  if (!gtk_recent_manager_remove_item (impl->manager, uri, &err))
    {
      gchar *msg;
   
      msg = g_strdup (_("Could not remove item"));
      error_message (impl, msg, err->message);
      
      g_free (msg);
      g_error_free (err);
    }
  
  g_free (uri);
}

static void
copy_activated_cb (GtkMenuItem *menu_item,
		   gpointer     user_data)
{
  GtkRecentChooserDefault *impl = GTK_RECENT_CHOOSER_DEFAULT (user_data);
  GtkRecentInfo *info;
  gchar *utf8_uri;

  info = gtk_recent_chooser_get_current_item (GTK_RECENT_CHOOSER (impl));
  if (!info)
    return;

  utf8_uri = gtk_recent_info_get_uri_display (info);
  
  gtk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (impl),
			  			    GDK_SELECTION_CLIPBOARD),
                          utf8_uri, -1);

  gtk_recent_info_unref (info);
  g_free (utf8_uri);
}

static void
remove_all_activated_cb (GtkMenuItem *menu_item,
			 gpointer     user_data)
{
  GtkRecentChooserDefault *impl = GTK_RECENT_CHOOSER_DEFAULT (user_data);
  GError *err = NULL;
  
  gtk_recent_manager_purge_items (impl->manager, &err);
  if (err)
    {
       gchar *msg;

       msg = g_strdup (_("Could not clear list"));

       error_message (impl, msg, err->message);
       
       g_free (msg);
       g_error_free (err);
    }
}

static void
remove_item_activated_cb (GtkMenuItem *menu_item,
			  gpointer     user_data)
{
  GtkRecentChooserDefault *impl = GTK_RECENT_CHOOSER_DEFAULT (user_data);
  
  remove_selected_from_list (impl);
}

static void
show_private_toggled_cb (GtkCheckMenuItem *menu_item,
			 gpointer          user_data)
{
  GtkRecentChooserDefault *impl = GTK_RECENT_CHOOSER_DEFAULT (user_data);
  
  g_object_set (G_OBJECT (impl),
  		"show-private", gtk_check_menu_item_get_active (menu_item),
  		NULL);
}

static void
recent_popup_menu_detach_cb (GtkWidget *attach_widget,
			     GtkMenu   *menu)
{
  GtkRecentChooserDefault *impl;
  
  impl = g_object_get_data (G_OBJECT (attach_widget), "GtkRecentChooserDefault");
  g_assert (GTK_IS_RECENT_CHOOSER_DEFAULT (impl));
  
  impl->recent_popup_menu = NULL;
  impl->recent_popup_menu_remove_item = NULL;
  impl->recent_popup_menu_copy_item = NULL;
  impl->recent_popup_menu_clear_item = NULL;
  impl->recent_popup_menu_show_private_item = NULL;
}

static void
recent_view_menu_ensure_state (GtkRecentChooserDefault *impl)
{
  gint count;
  
  g_assert (GTK_IS_RECENT_CHOOSER_DEFAULT (impl));
  g_assert (impl->recent_popup_menu != NULL);

  if (!impl->manager)
    count = 0;
  else
    g_object_get (G_OBJECT (impl->manager), "size", &count, NULL);

  if (count == 0)
    {
      gtk_widget_set_sensitive (impl->recent_popup_menu_remove_item, FALSE);
      gtk_widget_set_sensitive (impl->recent_popup_menu_copy_item, FALSE);
      gtk_widget_set_sensitive (impl->recent_popup_menu_clear_item, FALSE);
      gtk_widget_set_sensitive (impl->recent_popup_menu_show_private_item, FALSE);
    }
}

static void
recent_view_menu_build (GtkRecentChooserDefault *impl)
{
  GtkWidget *item;
  
  if (impl->recent_popup_menu)
    {
      recent_view_menu_ensure_state (impl);
      
      return;
    }
  
  impl->recent_popup_menu = gtk_menu_new ();
  gtk_menu_attach_to_widget (GTK_MENU (impl->recent_popup_menu),
  			     impl->recent_view,
  			     recent_popup_menu_detach_cb);
  
  item = gtk_image_menu_item_new_with_mnemonic (_("Copy _Location"));
  impl->recent_popup_menu_copy_item = item;
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
		  		 gtk_image_new_from_stock (GTK_STOCK_COPY, GTK_ICON_SIZE_MENU));
  g_signal_connect (item, "activate",
		    G_CALLBACK (copy_activated_cb), impl);
  gtk_widget_show (item);
  gtk_menu_shell_append (GTK_MENU_SHELL (impl->recent_popup_menu), item);

  item = gtk_separator_menu_item_new ();
  gtk_widget_show (item);
  gtk_menu_shell_append (GTK_MENU_SHELL (impl->recent_popup_menu), item);
  
  item = gtk_image_menu_item_new_with_mnemonic (_("_Remove From List"));
  impl->recent_popup_menu_remove_item = item;
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
  				 gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU));
  g_signal_connect (item, "activate",
  		    G_CALLBACK (remove_item_activated_cb), impl);
  gtk_widget_show (item);
  gtk_menu_shell_append (GTK_MENU_SHELL (impl->recent_popup_menu), item);

  item = gtk_image_menu_item_new_with_mnemonic (_("_Clear List"));
  impl->recent_popup_menu_clear_item = item;
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
  				 gtk_image_new_from_stock (GTK_STOCK_CLEAR, GTK_ICON_SIZE_MENU));
  g_signal_connect (item, "activate",
		    G_CALLBACK (remove_all_activated_cb), impl);
  
  gtk_widget_show (item);
  gtk_menu_shell_append (GTK_MENU_SHELL (impl->recent_popup_menu), item);
  
  item = gtk_separator_menu_item_new ();
  gtk_widget_show (item);
  gtk_menu_shell_append (GTK_MENU_SHELL (impl->recent_popup_menu), item);
  
  item = gtk_check_menu_item_new_with_mnemonic (_("Show _Private Resources"));
  impl->recent_popup_menu_show_private_item = item;
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), impl->show_private);
  g_signal_connect (item, "toggled",
  		    G_CALLBACK (show_private_toggled_cb), impl);
  gtk_widget_show (item);
  gtk_menu_shell_append (GTK_MENU_SHELL (impl->recent_popup_menu), item);
  
  recent_view_menu_ensure_state (impl);
}

/* taken from gtkfilechooserdefault.c */
static void
popup_position_func (GtkMenu   *menu,
                     gint      *x,
                     gint      *y,
                     gboolean  *push_in,
                     gpointer	user_data)
{
  GtkWidget *widget = GTK_WIDGET (user_data);
  GdkScreen *screen = gtk_widget_get_screen (widget);
  GtkRequisition req;
  gint monitor_num;
  GdkRectangle monitor;

  if (G_UNLIKELY (!gtk_widget_get_realized (widget)))
    return;

  gdk_window_get_origin (widget->window, x, y);

  gtk_widget_size_request (GTK_WIDGET (menu), &req);

  *x += (widget->allocation.width - req.width) / 2;
  *y += (widget->allocation.height - req.height) / 2;

  monitor_num = gdk_screen_get_monitor_at_point (screen, *x, *y);
  gtk_menu_set_monitor (menu, monitor_num);
  gdk_screen_get_monitor_geometry (screen, monitor_num, &monitor);

  *x = CLAMP (*x, monitor.x, monitor.x + MAX (0, monitor.width - req.width));
  *y = CLAMP (*y, monitor.y, monitor.y + MAX (0, monitor.height - req.height));

  *push_in = FALSE;
}


static void
recent_view_menu_popup (GtkRecentChooserDefault *impl,
			GdkEventButton          *event)
{
  recent_view_menu_build (impl);
  
  if (event)
    gtk_menu_popup (GTK_MENU (impl->recent_popup_menu),
    		    NULL, NULL, NULL, NULL,
    		    event->button, event->time);
  else
    {
      gtk_menu_popup (GTK_MENU (impl->recent_popup_menu),
      		      NULL, NULL,
      		      popup_position_func, impl->recent_view,
      		      0, GDK_CURRENT_TIME);
      gtk_menu_shell_select_first (GTK_MENU_SHELL (impl->recent_popup_menu),
      				   FALSE);
    }
}

static gboolean
recent_view_popup_menu_cb (GtkWidget *widget,
			   gpointer   user_data)
{
  recent_view_menu_popup (GTK_RECENT_CHOOSER_DEFAULT (user_data), NULL);
  return TRUE;
}

static gboolean
recent_view_button_press_cb (GtkWidget      *widget,
			     GdkEventButton *event,
			     gpointer        user_data)
{
  GtkRecentChooserDefault *impl = GTK_RECENT_CHOOSER_DEFAULT (user_data);

  if (_gtk_button_event_triggers_context_menu (event))
    {
      GtkTreePath *path;
      gboolean res;

      if (event->window != gtk_tree_view_get_bin_window (GTK_TREE_VIEW (impl->recent_view)))
        return FALSE;

      res = gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (impl->recent_view),
		      			   event->x, event->y,
					   &path,
					   NULL, NULL, NULL);
      if (!res)
        return FALSE;

      /* select the path before creating the popup menu */
      gtk_tree_selection_select_path (impl->selection, path);
      gtk_tree_path_free (path);
      
      recent_view_menu_popup (impl, event);

      return TRUE;
    }
  
  return FALSE;
}

static void
set_recent_manager (GtkRecentChooserDefault *impl,
		    GtkRecentManager        *manager)
{
  if (impl->manager)
    {
      if (impl->manager_changed_id)
        {
          g_signal_handler_disconnect (impl, impl->manager_changed_id);
          impl->manager_changed_id = 0;
        }

      impl->manager = NULL;
    }
  
  if (manager)
    impl->manager = manager;
  else
    impl->manager = gtk_recent_manager_get_default ();
  
  if (impl->manager)
    {
      impl->manager_changed_id = g_signal_connect (impl->manager, "changed",
                                                   G_CALLBACK (recent_manager_changed_cb),
                                                   impl);
    }
}

static void
gtk_recent_chooser_update (GtkActivatable *activatable,
			   GtkAction      *action,
			   const gchar    *property_name)
{
  if (strcmp (property_name, "visible") == 0)
    {
      if (gtk_action_is_visible (action))
	gtk_widget_show (GTK_WIDGET (activatable));
      else
	gtk_widget_hide (GTK_WIDGET (activatable));
    }

  if (strcmp (property_name, "sensitive") == 0)
    gtk_widget_set_sensitive (GTK_WIDGET (activatable), gtk_action_is_sensitive (action));

  _gtk_recent_chooser_update (activatable, action, property_name);
}


static void 
gtk_recent_chooser_sync_action_properties (GtkActivatable *activatable,
				           GtkAction      *action)
{
  if (action)
    {
      if (gtk_action_is_visible (action))
	gtk_widget_show (GTK_WIDGET (activatable));
      else
	gtk_widget_hide (GTK_WIDGET (activatable));
      
      gtk_widget_set_sensitive (GTK_WIDGET (activatable), gtk_action_is_sensitive (action));
    }

  _gtk_recent_chooser_sync_action_properties (activatable, action);
}


GtkWidget *
_gtk_recent_chooser_default_new (GtkRecentManager *manager)
{
  return g_object_new (GTK_TYPE_RECENT_CHOOSER_DEFAULT,
  		       "recent-manager", manager,
  		       NULL);
}
