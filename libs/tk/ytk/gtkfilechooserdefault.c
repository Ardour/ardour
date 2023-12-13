/* -*- Mode: C; c-file-style: "gnu"; tab-width: 8 -*- */
/* GTK - The GIMP Toolkit
 * gtkfilechooserdefault.c: Default implementation of GtkFileChooser
 * Copyright (C) 2003, Red Hat, Inc.
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

#include "gdk/gdkkeysyms.h"
#include "gtkalignment.h"
#include "gtkbindings.h"
#include "gtkcelllayout.h"
#include "gtkcellrendererpixbuf.h"
#include "gtkcellrenderertext.h"
#include "gtkcheckmenuitem.h"
#include "gtkclipboard.h"
#include "gtkcomboboxtext.h"
#include "gtkentry.h"
#include "gtkfilechooserprivate.h"
#include "gtkfilechooserdefault.h"
#include "gtkfilechooserdialog.h"
#include "gtkfilechooserembed.h"
#include "gtkfilechooserentry.h"
#include "gtkfilechoosersettings.h"
#include "gtkfilechooserutils.h"
#include "gtkfilechooser.h"
#include "gtkfilesystem.h"
#include "gtkfilesystemmodel.h"
#include "gtkframe.h"
#include "gtkhbox.h"
#include "gtkhpaned.h"
#include "gtkiconfactory.h"
#include "gtkicontheme.h"
#include "gtkimage.h"
#include "gtkimagemenuitem.h"
#include "gtkinfobar.h"
#include "gtklabel.h"
#include "gtkmarshalers.h"
#include "gtkmessagedialog.h"
#include "gtkmountoperation.h"
#include "gtkpathbar.h"
#include "gtkprivate.h"
#include "gtkradiobutton.h"
#include "gtkrecentfilter.h"
#include "gtkrecentmanager.h"
#include "gtkscrolledwindow.h"
#include "gtkseparatormenuitem.h"
#include "gtksizegroup.h"
#include "gtkstock.h"
#include "gtktable.h"
#include "gtktoolbar.h"
#include "gtktoolbutton.h"
#include "gtktooltip.h"
#include "gtktreednd.h"
#include "gtktreeprivate.h"
#include "gtktreeselection.h"
#include "gtkvbox.h"
#include "gtkintl.h"

#include "gtkalias.h"

#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <locale.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef G_OS_WIN32
#include <io.h>
#endif

/* Profiling stuff */
#undef PROFILE_FILE_CHOOSER
#ifdef PROFILE_FILE_CHOOSER


#ifndef F_OK 
#define F_OK 0
#endif

#define PROFILE_INDENT 4
static int profile_indent;

static void
profile_add_indent (int indent)
{
  profile_indent += indent;
  if (profile_indent < 0)
    g_error ("You screwed up your indentation");
}

void
_gtk_file_chooser_profile_log (const char *func, int indent, const char *msg1, const char *msg2)
{
  char *str;

  if (indent < 0)
    profile_add_indent (indent);

  if (profile_indent == 0)
    str = g_strdup_printf ("MARK: %s %s %s", func ? func : "", msg1 ? msg1 : "", msg2 ? msg2 : "");
  else
    str = g_strdup_printf ("MARK: %*c %s %s %s", profile_indent - 1, ' ', func ? func : "", msg1 ? msg1 : "", msg2 ? msg2 : "");

  access (str, F_OK);
  g_free (str);

  if (indent > 0)
    profile_add_indent (indent);
}

#define profile_start(x, y) _gtk_file_chooser_profile_log (G_STRFUNC, PROFILE_INDENT, x, y)
#define profile_end(x, y) _gtk_file_chooser_profile_log (G_STRFUNC, -PROFILE_INDENT, x, y)
#define profile_msg(x, y) _gtk_file_chooser_profile_log (NULL, 0, x, y)
#else
#define profile_start(x, y)
#define profile_end(x, y)
#define profile_msg(x, y)
#endif



typedef struct _GtkFileChooserDefaultClass GtkFileChooserDefaultClass;

#define GTK_FILE_CHOOSER_DEFAULT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_FILE_CHOOSER_DEFAULT, GtkFileChooserDefaultClass))
#define GTK_IS_FILE_CHOOSER_DEFAULT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_FILE_CHOOSER_DEFAULT))
#define GTK_FILE_CHOOSER_DEFAULT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_FILE_CHOOSER_DEFAULT, GtkFileChooserDefaultClass))

#define MAX_LOADING_TIME 500

#define DEFAULT_NEW_FOLDER_NAME _("Type name of new folder")

struct _GtkFileChooserDefaultClass
{
  GtkVBoxClass parent_class;
};

/* Signal IDs */
enum {
  LOCATION_POPUP,
  LOCATION_POPUP_ON_PASTE,
  UP_FOLDER,
  DOWN_FOLDER,
  HOME_FOLDER,
  DESKTOP_FOLDER,
  QUICK_BOOKMARK,
  LOCATION_TOGGLE_POPUP,
  SHOW_HIDDEN,
  SEARCH_SHORTCUT,
  RECENT_SHORTCUT,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

/* Column numbers for the shortcuts tree.  Keep these in sync with shortcuts_model_create() */
enum {
  SHORTCUTS_COL_PIXBUF,
  SHORTCUTS_COL_NAME,
  SHORTCUTS_COL_DATA,
  SHORTCUTS_COL_TYPE,
  SHORTCUTS_COL_REMOVABLE,
  SHORTCUTS_COL_PIXBUF_VISIBLE,
  SHORTCUTS_COL_CANCELLABLE,
  SHORTCUTS_COL_NUM_COLUMNS
};

typedef enum {
  SHORTCUT_TYPE_FILE,
  SHORTCUT_TYPE_VOLUME,
  SHORTCUT_TYPE_SEPARATOR,
  SHORTCUT_TYPE_SEARCH,
  SHORTCUT_TYPE_RECENT
} ShortcutType;

#define MODEL_ATTRIBUTES "standard::name,standard::type,standard::display-name," \
                         "standard::is-hidden,standard::is-backup,standard::size," \
                         "standard::content-type,time::modified"
enum {
  /* the first 3 must be these due to settings caching sort column */
  MODEL_COL_NAME,
  MODEL_COL_SIZE,
  MODEL_COL_MTIME,
  MODEL_COL_FILE,
  MODEL_COL_NAME_COLLATED,
  MODEL_COL_IS_FOLDER,
  MODEL_COL_IS_SENSITIVE,
  MODEL_COL_PIXBUF,
  MODEL_COL_SIZE_TEXT,
  MODEL_COL_MTIME_TEXT,
  MODEL_COL_ELLIPSIZE,
  MODEL_COL_NUM_COLUMNS
};

/* This list of types is passed to _gtk_file_system_model_new*() */
#define MODEL_COLUMN_TYPES					\
	MODEL_COL_NUM_COLUMNS,					\
	G_TYPE_STRING,		  /* MODEL_COL_NAME */		\
	G_TYPE_INT64,		  /* MODEL_COL_SIZE */		\
	G_TYPE_LONG,		  /* MODEL_COL_MTIME */		\
	G_TYPE_FILE,		  /* MODEL_COL_FILE */		\
	G_TYPE_STRING,		  /* MODEL_COL_NAME_COLLATED */	\
	G_TYPE_BOOLEAN,		  /* MODEL_COL_IS_FOLDER */	\
	G_TYPE_BOOLEAN,		  /* MODEL_COL_IS_SENSITIVE */	\
	GDK_TYPE_PIXBUF,	  /* MODEL_COL_PIXBUF */	\
	G_TYPE_STRING,		  /* MODEL_COL_SIZE_TEXT */	\
	G_TYPE_STRING,		  /* MODEL_COL_MTIME_TEXT */	\
	PANGO_TYPE_ELLIPSIZE_MODE /* MODEL_COL_ELLIPSIZE */

/* Identifiers for target types */
enum {
  GTK_TREE_MODEL_ROW,
};

/* Interesting places in the shortcuts bar */
typedef enum {
  SHORTCUTS_SEARCH,
  SHORTCUTS_RECENT,
  SHORTCUTS_CWD,
  SHORTCUTS_RECENT_SEPARATOR,
  SHORTCUTS_HOME,
  SHORTCUTS_DESKTOP,
  SHORTCUTS_VOLUMES,
  SHORTCUTS_SHORTCUTS,
  SHORTCUTS_BOOKMARKS_SEPARATOR,
  SHORTCUTS_BOOKMARKS,
  SHORTCUTS_CURRENT_FOLDER_SEPARATOR,
  SHORTCUTS_CURRENT_FOLDER
} ShortcutsIndex;

/* Icon size for if we can't get it from the theme */
#define FALLBACK_ICON_SIZE 16

#define PREVIEW_HBOX_SPACING 12
#define NUM_LINES 45
#define NUM_CHARS 60

static void gtk_file_chooser_default_iface_init       (GtkFileChooserIface        *iface);
static void gtk_file_chooser_embed_default_iface_init (GtkFileChooserEmbedIface   *iface);

static GObject* gtk_file_chooser_default_constructor  (GType                  type,
						       guint                  n_construct_properties,
						       GObjectConstructParam *construct_params);
static void     gtk_file_chooser_default_finalize     (GObject               *object);
static void     gtk_file_chooser_default_set_property (GObject               *object,
						       guint                  prop_id,
						       const GValue          *value,
						       GParamSpec            *pspec);
static void     gtk_file_chooser_default_get_property (GObject               *object,
						       guint                  prop_id,
						       GValue                *value,
						       GParamSpec            *pspec);
static void     gtk_file_chooser_default_dispose      (GObject               *object);
static void     gtk_file_chooser_default_show_all       (GtkWidget             *widget);
static void     gtk_file_chooser_default_realize        (GtkWidget             *widget);
static void     gtk_file_chooser_default_map            (GtkWidget             *widget);
static void     gtk_file_chooser_default_hierarchy_changed (GtkWidget          *widget,
							    GtkWidget          *previous_toplevel);
static void     gtk_file_chooser_default_style_set      (GtkWidget             *widget,
							 GtkStyle              *previous_style);
static void     gtk_file_chooser_default_screen_changed (GtkWidget             *widget,
							 GdkScreen             *previous_screen);

static gboolean       gtk_file_chooser_default_set_current_folder 	   (GtkFileChooser    *chooser,
									    GFile             *folder,
									    GError           **error);
static gboolean       gtk_file_chooser_default_update_current_folder 	   (GtkFileChooser    *chooser,
									    GFile             *folder,
									    gboolean           keep_trail,
									    gboolean           clear_entry,
									    GError           **error);
static GFile *        gtk_file_chooser_default_get_current_folder 	   (GtkFileChooser    *chooser);
static void           gtk_file_chooser_default_set_current_name   	   (GtkFileChooser    *chooser,
									    const gchar       *name);
static gboolean       gtk_file_chooser_default_select_file        	   (GtkFileChooser    *chooser,
									    GFile             *file,
									    GError           **error);
static void           gtk_file_chooser_default_unselect_file      	   (GtkFileChooser    *chooser,
									    GFile             *file);
static void           gtk_file_chooser_default_select_all         	   (GtkFileChooser    *chooser);
static void           gtk_file_chooser_default_unselect_all       	   (GtkFileChooser    *chooser);
static GSList *       gtk_file_chooser_default_get_files          	   (GtkFileChooser    *chooser);
static GFile *        gtk_file_chooser_default_get_preview_file   	   (GtkFileChooser    *chooser);
static GtkFileSystem *gtk_file_chooser_default_get_file_system    	   (GtkFileChooser    *chooser);
static void           gtk_file_chooser_default_add_filter         	   (GtkFileChooser    *chooser,
									    GtkFileFilter     *filter);
static void           gtk_file_chooser_default_remove_filter      	   (GtkFileChooser    *chooser,
									    GtkFileFilter     *filter);
static GSList *       gtk_file_chooser_default_list_filters       	   (GtkFileChooser    *chooser);
static gboolean       gtk_file_chooser_default_add_shortcut_folder    (GtkFileChooser    *chooser,
								       GFile             *file,
								       GError           **error);
static gboolean       gtk_file_chooser_default_remove_shortcut_folder (GtkFileChooser    *chooser,
								       GFile             *file,
								       GError           **error);
static GSList *       gtk_file_chooser_default_list_shortcut_folders  (GtkFileChooser    *chooser);

static void           gtk_file_chooser_default_get_default_size       (GtkFileChooserEmbed *chooser_embed,
								       gint                *default_width,
								       gint                *default_height);
static gboolean       gtk_file_chooser_default_should_respond         (GtkFileChooserEmbed *chooser_embed);
static void           gtk_file_chooser_default_initial_focus          (GtkFileChooserEmbed *chooser_embed);

static void add_selection_to_recent_list (GtkFileChooserDefault *impl);

static void location_popup_handler  (GtkFileChooserDefault *impl,
				     const gchar           *path);
static void location_popup_on_paste_handler (GtkFileChooserDefault *impl);
static void location_toggle_popup_handler   (GtkFileChooserDefault *impl);
static void up_folder_handler       (GtkFileChooserDefault *impl);
static void down_folder_handler     (GtkFileChooserDefault *impl);
static void home_folder_handler     (GtkFileChooserDefault *impl);
static void desktop_folder_handler  (GtkFileChooserDefault *impl);
static void quick_bookmark_handler  (GtkFileChooserDefault *impl,
				     gint                   bookmark_index);
static void show_hidden_handler     (GtkFileChooserDefault *impl);
static void search_shortcut_handler (GtkFileChooserDefault *impl);
static void recent_shortcut_handler (GtkFileChooserDefault *impl);
static void update_appearance       (GtkFileChooserDefault *impl);

static void set_current_filter   (GtkFileChooserDefault *impl,
				  GtkFileFilter         *filter);
static void check_preview_change (GtkFileChooserDefault *impl);

static void filter_combo_changed       (GtkComboBox           *combo_box,
					GtkFileChooserDefault *impl);

static gboolean shortcuts_key_press_event_cb (GtkWidget             *widget,
					      GdkEventKey           *event,
					      GtkFileChooserDefault *impl);

static gboolean shortcuts_select_func   (GtkTreeSelection      *selection,
					 GtkTreeModel          *model,
					 GtkTreePath           *path,
					 gboolean               path_currently_selected,
					 gpointer               data);
static gboolean shortcuts_get_selected  (GtkFileChooserDefault *impl,
					 GtkTreeIter           *iter);
static void shortcuts_activate_iter (GtkFileChooserDefault *impl,
				     GtkTreeIter           *iter);
static int shortcuts_get_index (GtkFileChooserDefault *impl,
				ShortcutsIndex         where);
static int shortcut_find_position (GtkFileChooserDefault *impl,
				   GFile                 *file);

static void bookmarks_check_add_sensitivity (GtkFileChooserDefault *impl);

static gboolean list_select_func   (GtkTreeSelection      *selection,
				    GtkTreeModel          *model,
				    GtkTreePath           *path,
				    gboolean               path_currently_selected,
				    gpointer               data);

static void list_selection_changed     (GtkTreeSelection      *tree_selection,
					GtkFileChooserDefault *impl);
static void list_row_activated         (GtkTreeView           *tree_view,
					GtkTreePath           *path,
					GtkTreeViewColumn     *column,
					GtkFileChooserDefault *impl);

static void path_bar_clicked (GtkPathBar            *path_bar,
			      GFile                 *file,
			      GFile                 *child,
                              gboolean               child_is_hidden,
                              GtkFileChooserDefault *impl);

static void add_bookmark_button_clicked_cb    (GtkButton             *button,
					       GtkFileChooserDefault *impl);
static void remove_bookmark_button_clicked_cb (GtkButton             *button,
					       GtkFileChooserDefault *impl);

static void update_cell_renderer_attributes (GtkFileChooserDefault *impl);

static void load_remove_timer (GtkFileChooserDefault *impl, LoadState new_load_state);
static void browse_files_center_selected_row (GtkFileChooserDefault *impl);

static void location_button_toggled_cb (GtkToggleButton *toggle,
					GtkFileChooserDefault *impl);
static void location_switch_to_path_bar (GtkFileChooserDefault *impl);

static void stop_loading_and_clear_list_model (GtkFileChooserDefault *impl,
                                               gboolean remove_from_treeview);

static void     search_setup_widgets         (GtkFileChooserDefault *impl);
static void     search_stop_searching        (GtkFileChooserDefault *impl,
                                              gboolean               remove_query);
static void     search_clear_model           (GtkFileChooserDefault *impl, 
					      gboolean               remove_from_treeview);
static gboolean search_should_respond        (GtkFileChooserDefault *impl);
static GSList  *search_get_selected_files    (GtkFileChooserDefault *impl);
static void     search_entry_activate_cb     (GtkEntry              *entry, 
					      gpointer               data);
static void     settings_load                (GtkFileChooserDefault *impl);
static void     settings_save                (GtkFileChooserDefault *impl);

static void     recent_start_loading         (GtkFileChooserDefault *impl);
static void     recent_stop_loading          (GtkFileChooserDefault *impl);
static void     recent_clear_model           (GtkFileChooserDefault *impl,
                                              gboolean               remove_from_treeview);
static gboolean recent_should_respond        (GtkFileChooserDefault *impl);
static GSList * recent_get_selected_files    (GtkFileChooserDefault *impl);
static void     set_file_system_backend      (GtkFileChooserDefault *impl);
static void     unset_file_system_backend    (GtkFileChooserDefault *impl);





/* Drag and drop interface declarations */

typedef struct {
  GtkTreeModelFilter parent;

  GtkFileChooserDefault *impl;
} ShortcutsPaneModelFilter;

typedef struct {
  GtkTreeModelFilterClass parent_class;
} ShortcutsPaneModelFilterClass;

#define SHORTCUTS_PANE_MODEL_FILTER_TYPE (_shortcuts_pane_model_filter_get_type ())
#define SHORTCUTS_PANE_MODEL_FILTER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), SHORTCUTS_PANE_MODEL_FILTER_TYPE, ShortcutsPaneModelFilter))

static void shortcuts_pane_model_filter_drag_source_iface_init (GtkTreeDragSourceIface *iface);

G_DEFINE_TYPE_WITH_CODE (ShortcutsPaneModelFilter,
			 _shortcuts_pane_model_filter,
			 GTK_TYPE_TREE_MODEL_FILTER,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_SOURCE,
						shortcuts_pane_model_filter_drag_source_iface_init))

static GtkTreeModel *shortcuts_pane_model_filter_new (GtkFileChooserDefault *impl,
						      GtkTreeModel          *child_model,
						      GtkTreePath           *root);



G_DEFINE_TYPE_WITH_CODE (GtkFileChooserDefault, _gtk_file_chooser_default, GTK_TYPE_VBOX,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_FILE_CHOOSER,
						gtk_file_chooser_default_iface_init)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_FILE_CHOOSER_EMBED,
						gtk_file_chooser_embed_default_iface_init));						


static void
add_normal_and_shifted_binding (GtkBindingSet  *binding_set,
				guint           keyval,
				GdkModifierType modifiers,
				const gchar    *signal_name)
{
  gtk_binding_entry_add_signal (binding_set,
				keyval, modifiers,
				signal_name, 0);

  gtk_binding_entry_add_signal (binding_set,
				keyval, modifiers | GDK_SHIFT_MASK,
				signal_name, 0);
}

static void
_gtk_file_chooser_default_class_init (GtkFileChooserDefaultClass *class)
{
  static const guint quick_bookmark_keyvals[10] = {
    GDK_KEY_1, GDK_KEY_2, GDK_KEY_3, GDK_KEY_4, GDK_KEY_5, GDK_KEY_6, GDK_KEY_7, GDK_KEY_8, GDK_KEY_9, GDK_KEY_0
  };
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GtkBindingSet *binding_set;
  int i;

  gobject_class->finalize = gtk_file_chooser_default_finalize;
  gobject_class->constructor = gtk_file_chooser_default_constructor;
  gobject_class->set_property = gtk_file_chooser_default_set_property;
  gobject_class->get_property = gtk_file_chooser_default_get_property;
  gobject_class->dispose = gtk_file_chooser_default_dispose;

  widget_class->show_all = gtk_file_chooser_default_show_all;
  widget_class->realize = gtk_file_chooser_default_realize;
  widget_class->map = gtk_file_chooser_default_map;
  widget_class->hierarchy_changed = gtk_file_chooser_default_hierarchy_changed;
  widget_class->style_set = gtk_file_chooser_default_style_set;
  widget_class->screen_changed = gtk_file_chooser_default_screen_changed;

  signals[LOCATION_POPUP] =
    g_signal_new_class_handler (I_("location-popup"),
                                G_OBJECT_CLASS_TYPE (class),
                                G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                                G_CALLBACK (location_popup_handler),
                                NULL, NULL,
                                _gtk_marshal_VOID__STRING,
                                G_TYPE_NONE, 1, G_TYPE_STRING);

  signals[LOCATION_POPUP_ON_PASTE] =
    g_signal_new_class_handler (I_("location-popup-on-paste"),
                                G_OBJECT_CLASS_TYPE (class),
                                G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                                G_CALLBACK (location_popup_on_paste_handler),
                                NULL, NULL,
                                _gtk_marshal_VOID__VOID,
                                G_TYPE_NONE, 0);

  signals[LOCATION_TOGGLE_POPUP] =
    g_signal_new_class_handler (I_("location-toggle-popup"),
                                G_OBJECT_CLASS_TYPE (class),
                                G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                                G_CALLBACK (location_toggle_popup_handler),
                                NULL, NULL,
                                _gtk_marshal_VOID__VOID,
                                G_TYPE_NONE, 0);

  signals[UP_FOLDER] =
    g_signal_new_class_handler (I_("up-folder"),
                                G_OBJECT_CLASS_TYPE (class),
                                G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                                G_CALLBACK (up_folder_handler),
                                NULL, NULL,
                                _gtk_marshal_VOID__VOID,
                                G_TYPE_NONE, 0);

  signals[DOWN_FOLDER] =
    g_signal_new_class_handler (I_("down-folder"),
                                G_OBJECT_CLASS_TYPE (class),
                                G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                                G_CALLBACK (down_folder_handler),
                                NULL, NULL,
                                _gtk_marshal_VOID__VOID,
                                G_TYPE_NONE, 0);

  signals[HOME_FOLDER] =
    g_signal_new_class_handler (I_("home-folder"),
                                G_OBJECT_CLASS_TYPE (class),
                                G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                                G_CALLBACK (home_folder_handler),
                                NULL, NULL,
                                _gtk_marshal_VOID__VOID,
                                G_TYPE_NONE, 0);

  signals[DESKTOP_FOLDER] =
    g_signal_new_class_handler (I_("desktop-folder"),
                                G_OBJECT_CLASS_TYPE (class),
                                G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                                G_CALLBACK (desktop_folder_handler),
                                NULL, NULL,
                                _gtk_marshal_VOID__VOID,
                                G_TYPE_NONE, 0);

  signals[QUICK_BOOKMARK] =
    g_signal_new_class_handler (I_("quick-bookmark"),
                                G_OBJECT_CLASS_TYPE (class),
                                G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                                G_CALLBACK (quick_bookmark_handler),
                                NULL, NULL,
                                _gtk_marshal_VOID__INT,
                                G_TYPE_NONE, 1, G_TYPE_INT);

  signals[SHOW_HIDDEN] =
    g_signal_new_class_handler (I_("show-hidden"),
                                G_OBJECT_CLASS_TYPE (class),
                                G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                                G_CALLBACK (show_hidden_handler),
                                NULL, NULL,
                                _gtk_marshal_VOID__VOID,
                                G_TYPE_NONE, 0);

  signals[SEARCH_SHORTCUT] =
    g_signal_new_class_handler (I_("search-shortcut"),
                                G_OBJECT_CLASS_TYPE (class),
                                G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                                G_CALLBACK (search_shortcut_handler),
                                NULL, NULL,
                                _gtk_marshal_VOID__VOID,
                                G_TYPE_NONE, 0);

  signals[RECENT_SHORTCUT] =
    g_signal_new_class_handler (I_("recent-shortcut"),
                                G_OBJECT_CLASS_TYPE (class),
                                G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                                G_CALLBACK (recent_shortcut_handler),
                                NULL, NULL,
                                _gtk_marshal_VOID__VOID,
                                G_TYPE_NONE, 0);

#ifdef __APPLE__ // see also gtk/gtkentry.c
#define OS_CTRL (GDK_MOD2_MASK|GDK_META_MASK)
#else
#define OS_CTRL GDK_CONTROL_MASK
#endif
  binding_set = gtk_binding_set_by_class (class);

  gtk_binding_entry_add_signal (binding_set,
				GDK_KEY_l, OS_CTRL,
				"location-toggle-popup",
				0);

  gtk_binding_entry_add_signal (binding_set,
				GDK_KEY_slash, 0,
				"location-popup",
				1, G_TYPE_STRING, "/");
  gtk_binding_entry_add_signal (binding_set,
				GDK_KEY_KP_Divide, 0,
				"location-popup",
				1, G_TYPE_STRING, "/");

#ifdef G_OS_UNIX
  gtk_binding_entry_add_signal (binding_set,
				GDK_KEY_asciitilde, 0,
				"location-popup",
				1, G_TYPE_STRING, "~");
#endif

  gtk_binding_entry_add_signal (binding_set,
				GDK_KEY_v, OS_CTRL,
				"location-popup-on-paste",
				0);
  gtk_binding_entry_add_signal (binding_set,
		  		GDK_KEY_BackSpace, 0,
				"up-folder",
				0);

  add_normal_and_shifted_binding (binding_set,
				  GDK_KEY_Up, GDK_MOD1_MASK,
				  "up-folder");

  add_normal_and_shifted_binding (binding_set,
				  GDK_KEY_KP_Up, GDK_MOD1_MASK,
				  "up-folder");

  add_normal_and_shifted_binding (binding_set,
				  GDK_KEY_Down, GDK_MOD1_MASK,
				  "down-folder");
  add_normal_and_shifted_binding (binding_set,
				  GDK_KEY_KP_Down, GDK_MOD1_MASK,
				  "down-folder");

  gtk_binding_entry_add_signal (binding_set,
				GDK_KEY_Home, GDK_MOD1_MASK,
				"home-folder",
				0);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KEY_KP_Home, GDK_MOD1_MASK,
				"home-folder",
				0);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KEY_d, GDK_MOD1_MASK,
				"desktop-folder",
				0);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KEY_h, OS_CTRL,
                                "show-hidden",
                                0);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_s, GDK_MOD1_MASK,
                                "search-shortcut",
                                0);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_r, GDK_MOD1_MASK,
                                "recent-shortcut",
                                0);

  for (i = 0; i < 10; i++)
    gtk_binding_entry_add_signal (binding_set,
				  quick_bookmark_keyvals[i], GDK_MOD1_MASK,
				  "quick-bookmark",
				  1, G_TYPE_INT, i);

  _gtk_file_chooser_install_properties (gobject_class);
}

static void
gtk_file_chooser_default_iface_init (GtkFileChooserIface *iface)
{
  iface->select_file = gtk_file_chooser_default_select_file;
  iface->unselect_file = gtk_file_chooser_default_unselect_file;
  iface->select_all = gtk_file_chooser_default_select_all;
  iface->unselect_all = gtk_file_chooser_default_unselect_all;
  iface->get_files = gtk_file_chooser_default_get_files;
  iface->get_preview_file = gtk_file_chooser_default_get_preview_file;
  iface->get_file_system = gtk_file_chooser_default_get_file_system;
  iface->set_current_folder = gtk_file_chooser_default_set_current_folder;
  iface->get_current_folder = gtk_file_chooser_default_get_current_folder;
  iface->set_current_name = gtk_file_chooser_default_set_current_name;
  iface->add_filter = gtk_file_chooser_default_add_filter;
  iface->remove_filter = gtk_file_chooser_default_remove_filter;
  iface->list_filters = gtk_file_chooser_default_list_filters;
  iface->add_shortcut_folder = gtk_file_chooser_default_add_shortcut_folder;
  iface->remove_shortcut_folder = gtk_file_chooser_default_remove_shortcut_folder;
  iface->list_shortcut_folders = gtk_file_chooser_default_list_shortcut_folders;
}

static void
gtk_file_chooser_embed_default_iface_init (GtkFileChooserEmbedIface *iface)
{
  iface->get_default_size = gtk_file_chooser_default_get_default_size;
  iface->should_respond = gtk_file_chooser_default_should_respond;
  iface->initial_focus = gtk_file_chooser_default_initial_focus;
}

static void
_gtk_file_chooser_default_init (GtkFileChooserDefault *impl)
{
  profile_start ("start", NULL);
#ifdef PROFILE_FILE_CHOOSER
  access ("MARK: *** CREATE FILE CHOOSER", F_OK);
#endif
  impl->local_only = TRUE;
  impl->preview_widget_active = TRUE;
  impl->use_preview_label = TRUE;
  impl->select_multiple = FALSE;
  impl->show_hidden = FALSE;
  impl->show_size_column = TRUE;
  impl->icon_size = FALLBACK_ICON_SIZE;
  impl->load_state = LOAD_EMPTY;
  impl->reload_state = RELOAD_EMPTY;
  impl->pending_select_files = NULL;
  impl->location_mode = LOCATION_MODE_PATH_BAR;
  impl->operation_mode = OPERATION_MODE_BROWSE;
  impl->sort_column = MODEL_COL_NAME;
  impl->sort_order = GTK_SORT_ASCENDING;
  impl->recent_manager = gtk_recent_manager_get_default ();
  impl->create_folders = TRUE;

  gtk_box_set_spacing (GTK_BOX (impl), 12);

  set_file_system_backend (impl);

  profile_end ("end", NULL);
}

/* Frees the data columns for the specified iter in the shortcuts model*/
static void
shortcuts_free_row_data (GtkFileChooserDefault *impl,
			 GtkTreeIter           *iter)
{
  gpointer col_data;
  ShortcutType shortcut_type;
  GCancellable *cancellable;

  gtk_tree_model_get (GTK_TREE_MODEL (impl->shortcuts_model), iter,
		      SHORTCUTS_COL_DATA, &col_data,
		      SHORTCUTS_COL_TYPE, &shortcut_type,
		      SHORTCUTS_COL_CANCELLABLE, &cancellable,
		      -1);

  if (cancellable)
    g_cancellable_cancel (cancellable);

  if (!(shortcut_type == SHORTCUT_TYPE_FILE || 
	shortcut_type == SHORTCUT_TYPE_VOLUME) ||
      !col_data)
    return;

  if (shortcut_type == SHORTCUT_TYPE_VOLUME)
    {
      GtkFileSystemVolume *volume;

      volume = col_data;
      _gtk_file_system_volume_unref (volume);
    }
  if (shortcut_type == SHORTCUT_TYPE_FILE)
    {
      GFile *file;

      file = col_data;
      g_object_unref (file);
    }
}

/* Frees all the data columns in the shortcuts model */
static void
shortcuts_free (GtkFileChooserDefault *impl)
{
  GtkTreeIter iter;

  if (!impl->shortcuts_model)
    return;

  if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (impl->shortcuts_model), &iter))
    do
      {
	shortcuts_free_row_data (impl, &iter);
      }
    while (gtk_tree_model_iter_next (GTK_TREE_MODEL (impl->shortcuts_model), &iter));

  g_object_unref (impl->shortcuts_model);
  impl->shortcuts_model = NULL;
}

static void
pending_select_files_free (GtkFileChooserDefault *impl)
{
  g_slist_foreach (impl->pending_select_files, (GFunc) g_object_unref, NULL);
  g_slist_free (impl->pending_select_files);
  impl->pending_select_files = NULL;
}

static void
pending_select_files_add (GtkFileChooserDefault *impl,
			  GFile                 *file)
{
  impl->pending_select_files =
    g_slist_prepend (impl->pending_select_files, g_object_ref (file));
}

static void
gtk_file_chooser_default_finalize (GObject *object)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (object);
  GSList *l;

  unset_file_system_backend (impl);

  if (impl->shortcuts_pane_filter_model)
    g_object_unref (impl->shortcuts_pane_filter_model);

  shortcuts_free (impl);

  g_free (impl->browse_files_last_selected_name);

  for (l = impl->filters; l; l = l->next)
    {
      GtkFileFilter *filter;

      filter = GTK_FILE_FILTER (l->data);
      g_object_unref (filter);
    }
  g_slist_free (impl->filters);

  if (impl->current_filter)
    g_object_unref (impl->current_filter);

  if (impl->current_volume_file)
    g_object_unref (impl->current_volume_file);

  if (impl->current_folder)
    g_object_unref (impl->current_folder);

  if (impl->preview_file)
    g_object_unref (impl->preview_file);

  if (impl->browse_path_bar_size_group)
    g_object_unref (impl->browse_path_bar_size_group);

  /* Free all the Models we have */
  stop_loading_and_clear_list_model (impl, FALSE);
  search_clear_model (impl, FALSE);
  recent_clear_model (impl, FALSE);

  /* stopping the load above should have cleared this */
  g_assert (impl->load_timeout_id == 0);

  g_free (impl->preview_display_name);

  g_free (impl->edited_new_text);

  G_OBJECT_CLASS (_gtk_file_chooser_default_parent_class)->finalize (object);
}

/* Shows an error dialog set as transient for the specified window */
static void
error_message_with_parent (GtkWindow  *parent,
			   const char *msg,
			   const char *detail)
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

  if (parent && gtk_window_has_group (parent))
    gtk_window_group_add_window (gtk_window_get_group (parent),
                                 GTK_WINDOW (dialog));

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
error_message (GtkFileChooserDefault *impl,
	       const char            *msg,
	       const char            *detail)
{
  error_message_with_parent (get_toplevel (GTK_WIDGET (impl)), msg, detail);
}

/* Shows a simple error dialog relative to a path.  Frees the GError as well. */
static void
error_dialog (GtkFileChooserDefault *impl,
	      const char            *msg,
	      GFile                 *file,
	      GError                *error)
{
  if (error)
    {
      char *uri = NULL;
      char *text;

      if (file)
	uri = g_file_get_uri (file);
      text = g_strdup_printf (msg, uri);
      error_message (impl, text, error->message);
      g_free (text);
      g_free (uri);
      g_error_free (error);
    }
}

/* Displays an error message about not being able to get information for a file.
 * Frees the GError as well.
 */
static void
error_getting_info_dialog (GtkFileChooserDefault *impl,
			   GFile                 *file,
			   GError                *error)
{
  error_dialog (impl,
		_("Could not retrieve information about the file"),
		file, error);
}

/* Shows an error dialog about not being able to add a bookmark */
static void
error_adding_bookmark_dialog (GtkFileChooserDefault *impl,
			      GFile                 *file,
			      GError                *error)
{
  error_dialog (impl,
		_("Could not add a bookmark"),
		file, error);
}

/* Shows an error dialog about not being able to remove a bookmark */
static void
error_removing_bookmark_dialog (GtkFileChooserDefault *impl,
				GFile                 *file,
				GError                *error)
{
  error_dialog (impl,
		_("Could not remove bookmark"),
		file, error);
}

/* Shows an error dialog about not being able to create a folder */
static void
error_creating_folder_dialog (GtkFileChooserDefault *impl,
			      GFile                 *file,
			      GError                *error)
{
  error_dialog (impl, 
		_("The folder could not be created"), 
		file, error);
}

/* Shows an error about not being able to create a folder because a file with
 * the same name is already there.
 */
static void
error_creating_folder_over_existing_file_dialog (GtkFileChooserDefault *impl,
						 GFile                 *file,
						 GError                *error)
{
  error_dialog (impl,
		_("The folder could not be created, as a file with the same "
                  "name already exists.  Try using a different name for the "
                  "folder, or rename the file first."),
		file, error);
}

static void
error_with_file_under_nonfolder (GtkFileChooserDefault *impl,
				 GFile *parent_file)
{
  GError *error;

  error = NULL;
  g_set_error_literal (&error, G_IO_ERROR, G_IO_ERROR_NOT_DIRECTORY,
		       _("You need to choose a valid filename."));

  error_dialog (impl,
		_("Cannot create a file under %s as it is not a folder"),
		parent_file, error);
}

/* Shows an error about not being able to select a folder because a file with
 * the same name is already there.
 */
static void
error_selecting_folder_over_existing_file_dialog (GtkFileChooserDefault *impl,
						  GFile                 *file)
{
  error_dialog (impl,
		_("You may only select folders.  The item that you selected is not a folder; "
                  "try using a different item."),
		file, NULL);
}

/* Shows an error dialog about not being able to create a filename */
static void
error_building_filename_dialog (GtkFileChooserDefault *impl,
				GError                *error)
{
  error_dialog (impl, _("Invalid file name"), 
		NULL, error);
}

/* Shows an error dialog when we cannot switch to a folder */
static void
error_changing_folder_dialog (GtkFileChooserDefault *impl,
			      GFile                 *file,
			      GError                *error)
{
  error_dialog (impl, _("The folder contents could not be displayed"),
		file, error);
}

/* Changes folders, displaying an error dialog if this fails */
static gboolean
change_folder_and_display_error (GtkFileChooserDefault *impl,
				 GFile                 *file,
				 gboolean               clear_entry)
{
  GError *error;
  gboolean result;

  g_return_val_if_fail (G_IS_FILE (file), FALSE);

  /* We copy the path because of this case:
   *
   * list_row_activated()
   *   fetches path from model; path belongs to the model (*)
   *   calls change_folder_and_display_error()
   *     calls gtk_file_chooser_set_current_folder_file()
   *       changing folders fails, sets model to NULL, thus freeing the path in (*)
   */

  error = NULL;
  result = gtk_file_chooser_default_update_current_folder (GTK_FILE_CHOOSER (impl), file, TRUE, clear_entry, &error);

  if (!result)
    error_changing_folder_dialog (impl, file, error);

  return result;
}

static void
emit_default_size_changed (GtkFileChooserDefault *impl)
{
  profile_msg ("    emit default-size-changed start", NULL);
  g_signal_emit_by_name (impl, "default-size-changed");
  profile_msg ("    emit default-size-changed end", NULL);
}

static void
update_preview_widget_visibility (GtkFileChooserDefault *impl)
{
  if (impl->use_preview_label)
    {
      if (!impl->preview_label)
	{
	  impl->preview_label = gtk_label_new (impl->preview_display_name);
	  gtk_box_pack_start (GTK_BOX (impl->preview_box), impl->preview_label, FALSE, FALSE, 0);
	  gtk_box_reorder_child (GTK_BOX (impl->preview_box), impl->preview_label, 0);
	  gtk_label_set_ellipsize (GTK_LABEL (impl->preview_label), PANGO_ELLIPSIZE_MIDDLE);
	  gtk_widget_show (impl->preview_label);
	}
    }
  else
    {
      if (impl->preview_label)
	{
	  gtk_widget_destroy (impl->preview_label);
	  impl->preview_label = NULL;
	}
    }

  if (impl->preview_widget_active && impl->preview_widget)
    gtk_widget_show (impl->preview_box);
  else
    gtk_widget_hide (impl->preview_box);

  if (!gtk_widget_get_mapped (GTK_WIDGET (impl)))
    emit_default_size_changed (impl);
}

static void
set_preview_widget (GtkFileChooserDefault *impl,
		    GtkWidget             *preview_widget)
{
  if (preview_widget == impl->preview_widget)
    return;

  if (impl->preview_widget)
    gtk_container_remove (GTK_CONTAINER (impl->preview_box),
			  impl->preview_widget);

  impl->preview_widget = preview_widget;
  if (impl->preview_widget)
    {
      gtk_widget_show (impl->preview_widget);
      gtk_box_pack_start (GTK_BOX (impl->preview_box), impl->preview_widget, TRUE, TRUE, 0);
      gtk_box_reorder_child (GTK_BOX (impl->preview_box),
			     impl->preview_widget,
			     (impl->use_preview_label && impl->preview_label) ? 1 : 0);
    }

  update_preview_widget_visibility (impl);
}

/* Renders a "Search" icon at an appropriate size for a tree view */
static GdkPixbuf *
render_search_icon (GtkFileChooserDefault *impl)
{
  return gtk_widget_render_icon (GTK_WIDGET (impl), GTK_STOCK_FIND, GTK_ICON_SIZE_MENU, NULL);
}

static GdkPixbuf *
render_recent_icon (GtkFileChooserDefault *impl)
{
  GtkIconTheme *theme;
  GdkPixbuf *retval;

  if (gtk_widget_has_screen (GTK_WIDGET (impl)))
    theme = gtk_icon_theme_get_for_screen (gtk_widget_get_screen (GTK_WIDGET (impl)));
  else
    theme = gtk_icon_theme_get_default ();

  retval = gtk_icon_theme_load_icon (theme, "document-open-recent",
                                     impl->icon_size, 0,
                                     NULL);

  /* fallback */
  if (!retval)
    retval = gtk_widget_render_icon (GTK_WIDGET (impl), GTK_STOCK_FILE, GTK_ICON_SIZE_MENU, NULL);

  return retval;
}


/* Re-reads all the icons for the shortcuts, used when the theme changes */
struct ReloadIconsData
{
  GtkFileChooserDefault *impl;
  GtkTreeRowReference *row_ref;
};

static void
shortcuts_reload_icons_get_info_cb (GCancellable *cancellable,
				    GFileInfo    *info,
				    const GError *error,
				    gpointer      user_data)
{
  GdkPixbuf *pixbuf;
  GtkTreeIter iter;
  GtkTreePath *path;
  gboolean cancelled = g_cancellable_is_cancelled (cancellable);
  struct ReloadIconsData *data = user_data;

  if (!g_slist_find (data->impl->reload_icon_cancellables, cancellable))
    goto out;

  data->impl->reload_icon_cancellables = g_slist_remove (data->impl->reload_icon_cancellables, cancellable);

  if (cancelled || error)
    goto out;

  pixbuf = _gtk_file_info_render_icon (info, GTK_WIDGET (data->impl), data->impl->icon_size);

  path = gtk_tree_row_reference_get_path (data->row_ref);
  if (path)
    {
      gtk_tree_model_get_iter (GTK_TREE_MODEL (data->impl->shortcuts_model), &iter, path);
      gtk_list_store_set (data->impl->shortcuts_model, &iter,
			  SHORTCUTS_COL_PIXBUF, pixbuf,
			  -1);
      gtk_tree_path_free (path);
    }

  if (pixbuf)
    g_object_unref (pixbuf);

out:
  gtk_tree_row_reference_free (data->row_ref);
  g_object_unref (data->impl);
  g_free (data);

  g_object_unref (cancellable);
}

static void
shortcuts_reload_icons (GtkFileChooserDefault *impl)
{
  GSList *l;
  GtkTreeIter iter;

  profile_start ("start", NULL);

  if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (impl->shortcuts_model), &iter))
    goto out;

  for (l = impl->reload_icon_cancellables; l; l = l->next)
    {
      GCancellable *cancellable = G_CANCELLABLE (l->data);
      g_cancellable_cancel (cancellable);
    }
  g_slist_free (impl->reload_icon_cancellables);
  impl->reload_icon_cancellables = NULL;

  do
    {
      gpointer data;
      ShortcutType shortcut_type;
      gboolean pixbuf_visible;
      GdkPixbuf *pixbuf;

      gtk_tree_model_get (GTK_TREE_MODEL (impl->shortcuts_model), &iter,
			  SHORTCUTS_COL_DATA, &data,
			  SHORTCUTS_COL_TYPE, &shortcut_type,
			  SHORTCUTS_COL_PIXBUF_VISIBLE, &pixbuf_visible,
			  -1);

      pixbuf = NULL;
      if (pixbuf_visible)
        {
	  if (shortcut_type == SHORTCUT_TYPE_VOLUME)
	    {
	      GtkFileSystemVolume *volume;

	      volume = data;
	      pixbuf = _gtk_file_system_volume_render_icon (volume, GTK_WIDGET (impl),
						 	    impl->icon_size, NULL);
	    }
	  else if (shortcut_type == SHORTCUT_TYPE_FILE)
            {
	      if (g_file_is_native (G_FILE (data)))
	        {
		  GFile *file;
	          struct ReloadIconsData *info;
	          GtkTreePath *tree_path;
		  GCancellable *cancellable;

	          file = data;

	          info = g_new0 (struct ReloadIconsData, 1);
	          info->impl = g_object_ref (impl);
	          tree_path = gtk_tree_model_get_path (GTK_TREE_MODEL (impl->shortcuts_model), &iter);
	          info->row_ref = gtk_tree_row_reference_new (GTK_TREE_MODEL (impl->shortcuts_model), tree_path);
	          gtk_tree_path_free (tree_path);

	          cancellable = _gtk_file_system_get_info (impl->file_system, file,
							   "standard::icon",
							   shortcuts_reload_icons_get_info_cb,
							   info);
	          impl->reload_icon_cancellables = g_slist_append (impl->reload_icon_cancellables, cancellable);
	        }
              else
	        {
	          GtkIconTheme *icon_theme;

	          /* Don't call get_info for remote paths to avoid latency and
	           * auth dialogs.
	           * If we switch to a better bookmarks file format (XBEL), we
	           * should use mime info to get a better icon.
	           */
	          icon_theme = gtk_icon_theme_get_for_screen (gtk_widget_get_screen (GTK_WIDGET (impl)));
	          pixbuf = gtk_icon_theme_load_icon (icon_theme, "folder-remote", 
						     impl->icon_size, 0, NULL);
	        }
            }
	  else if (shortcut_type == SHORTCUT_TYPE_SEARCH)
	    {
	      pixbuf = render_search_icon (impl);
            }
	  else if (shortcut_type == SHORTCUT_TYPE_RECENT)
            {
              pixbuf = render_recent_icon (impl);
            }

          gtk_list_store_set (impl->shortcuts_model, &iter,
  	   	              SHORTCUTS_COL_PIXBUF, pixbuf,
	                      -1);

          if (pixbuf)
            g_object_unref (pixbuf);

	}
    }
  while (gtk_tree_model_iter_next (GTK_TREE_MODEL (impl->shortcuts_model),&iter));

 out:

  profile_end ("end", NULL);
}

static void 
shortcuts_find_folder (GtkFileChooserDefault *impl,
		       GFile                 *folder)
{
  GtkTreeSelection *selection;
  int pos;
  GtkTreePath *path;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->browse_shortcuts_tree_view));

  g_assert (folder != NULL);
  pos = shortcut_find_position (impl, folder);
  if (pos == -1)
    {
      gtk_tree_selection_unselect_all (selection);
      return;
    }

  path = gtk_tree_path_new_from_indices (pos, -1);
  gtk_tree_selection_select_path (selection, path);
  gtk_tree_path_free (path);
}

/* If a shortcut corresponds to the current folder, selects it */
static void
shortcuts_find_current_folder (GtkFileChooserDefault *impl)
{
  shortcuts_find_folder (impl, impl->current_folder);
}

/* Removes the specified number of rows from the shortcuts list */
static void
shortcuts_remove_rows (GtkFileChooserDefault *impl,
		       int                    start_row,
		       int                    n_rows)
{
  GtkTreePath *path;

  path = gtk_tree_path_new_from_indices (start_row, -1);

  for (; n_rows; n_rows--)
    {
      GtkTreeIter iter;

      if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (impl->shortcuts_model), &iter, path))
	g_assert_not_reached ();

      shortcuts_free_row_data (impl, &iter);
      gtk_list_store_remove (impl->shortcuts_model, &iter);
    }

  gtk_tree_path_free (path);
}

static void
shortcuts_update_count (GtkFileChooserDefault *impl,
			ShortcutsIndex         type,
			gint                   value)
{
  switch (type)
    {
      case SHORTCUTS_CWD:
        if (value < 0)
	  impl->has_cwd = FALSE;
	else
	  impl->has_cwd = TRUE;
	break;
	
      case SHORTCUTS_HOME:
	if (value < 0)
	  impl->has_home = FALSE;
	else
	  impl->has_home = TRUE;
	break;

      case SHORTCUTS_DESKTOP:
	if (value < 0)
	  impl->has_desktop = FALSE;
	else
	  impl->has_desktop = TRUE;
	break;

      case SHORTCUTS_VOLUMES:
	impl->num_volumes += value;
	break;

      case SHORTCUTS_SHORTCUTS:
	impl->num_shortcuts += value;
	break;

      case SHORTCUTS_BOOKMARKS:
	impl->num_bookmarks += value;
	break;

      case SHORTCUTS_CURRENT_FOLDER:
	if (value < 0)
	  impl->shortcuts_current_folder_active = FALSE;
	else
	  impl->shortcuts_current_folder_active = TRUE;
	break;

      default:
	/* nothing */
	break;
    }
}

struct ShortcutsInsertRequest
{
  GtkFileChooserDefault *impl;
  GFile *file;
  int pos;
  char *label_copy;
  GtkTreeRowReference *row_ref;
  ShortcutsIndex type;
  gboolean name_only;
  gboolean removable;
};

static void
get_file_info_finished (GCancellable *cancellable,
			GFileInfo    *info,
			const GError *error,
			gpointer      data)
{
  gboolean cancelled = g_cancellable_is_cancelled (cancellable);
  GdkPixbuf *pixbuf;
  GtkTreePath *path;
  GtkTreeIter iter;
  GCancellable *model_cancellable;
  struct ShortcutsInsertRequest *request = data;

  path = gtk_tree_row_reference_get_path (request->row_ref);
  if (!path)
    /* Handle doesn't exist anymore in the model */
    goto out;

  gtk_tree_model_get_iter (GTK_TREE_MODEL (request->impl->shortcuts_model),
			   &iter, path);
  gtk_tree_path_free (path);

  /* validate cancellable, else goto out */
  gtk_tree_model_get (GTK_TREE_MODEL (request->impl->shortcuts_model), &iter,
		      SHORTCUTS_COL_CANCELLABLE, &model_cancellable,
		      -1);
  if (cancellable != model_cancellable)
    goto out;

  /* set the cancellable to NULL in the model (we unref later on) */
  gtk_list_store_set (request->impl->shortcuts_model, &iter,
		      SHORTCUTS_COL_CANCELLABLE, NULL,
		      -1);

  if (cancelled)
    goto out;

  if (!info)
    {
      shortcuts_free_row_data (request->impl, &iter);
      gtk_list_store_remove (request->impl->shortcuts_model, &iter);
      shortcuts_update_count (request->impl, request->type, -1);

      if (request->type == SHORTCUTS_HOME)
        {
	  GFile *home;

	  home = g_file_new_for_path (g_get_home_dir ());
	  error_getting_info_dialog (request->impl, home, g_error_copy (error));
	  g_object_unref (home);
	}
      else if (request->type == SHORTCUTS_CURRENT_FOLDER)
        {
	  /* Remove the current folder separator */
	  gint separator_pos = shortcuts_get_index (request->impl, SHORTCUTS_CURRENT_FOLDER_SEPARATOR);
	  shortcuts_remove_rows (request->impl, separator_pos, 1);
        }

      goto out;
    }
  
  if (!request->label_copy)
    request->label_copy = g_strdup (g_file_info_get_display_name (info));
  pixbuf = _gtk_file_info_render_icon (info, GTK_WIDGET (request->impl),
				       request->impl->icon_size);

  gtk_list_store_set (request->impl->shortcuts_model, &iter,
		      SHORTCUTS_COL_PIXBUF, pixbuf,
		      SHORTCUTS_COL_PIXBUF_VISIBLE, TRUE,
		      SHORTCUTS_COL_NAME, request->label_copy,
		      SHORTCUTS_COL_TYPE, SHORTCUT_TYPE_FILE,
		      SHORTCUTS_COL_REMOVABLE, request->removable,
		      -1);

  if (request->impl->shortcuts_pane_filter_model)
    gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (request->impl->shortcuts_pane_filter_model));

  if (pixbuf)
    g_object_unref (pixbuf);

out:
  g_object_unref (request->impl);
  g_object_unref (request->file);
  gtk_tree_row_reference_free (request->row_ref);
  g_free (request->label_copy);
  g_free (request);

  g_object_unref (cancellable);
}

/* FIXME: GtkFileSystem needs a function to split a remote path
 * into hostname and path components, or maybe just have a 
 * gtk_file_system_path_get_display_name().
 *
 * This function is also used in gtkfilechooserbutton.c
 */
gchar *
_gtk_file_chooser_label_for_file (GFile *file)
{
  const gchar *path, *start, *end, *p;
  gchar *uri, *host, *label;

  uri = g_file_get_uri (file);

  start = strstr (uri, "://");
  if (start)
    {
      start += 3;
      path = strchr (start, '/');
      if (path)
        end = path;
      else
        {
          end = uri + strlen (uri);
          path = "/";
        }

      /* strip username */
      p = strchr (start, '@');
      if (p && p < end)
        start = p + 1;
  
      p = strchr (start, ':');
      if (p && p < end)
        end = p;
  
      host = g_strndup (start, end - start);
  
      /* Translators: the first string is a path and the second string 
       * is a hostname. Nautilus and the panel contain the same string 
       * to translate. 
       */
      label = g_strdup_printf (_("%1$s on %2$s"), path, host);
  
      g_free (host);
    }
  else
    {
      label = g_strdup (uri);
    }
  
  g_free (uri);

  return label;
}

/* Inserts a path in the shortcuts tree, making a copy of it; alternatively,
 * inserts a volume.  A position of -1 indicates the end of the tree.
 */
static void
shortcuts_insert_file (GtkFileChooserDefault *impl,
		       int                    pos,
		       ShortcutType           shortcut_type,
		       GtkFileSystemVolume   *volume,
		       GFile                 *file,
		       const char            *label,
		       gboolean               removable,
		       ShortcutsIndex         type)
{
  char *label_copy;
  GdkPixbuf *pixbuf = NULL;
  gpointer data = NULL;
  GtkTreeIter iter;
  GtkIconTheme *icon_theme;

  profile_start ("start shortcut", NULL);

  if (shortcut_type == SHORTCUT_TYPE_VOLUME)
    {
      data = volume;
      label_copy = _gtk_file_system_volume_get_display_name (volume);
      pixbuf = _gtk_file_system_volume_render_icon (volume, GTK_WIDGET (impl),
				 		    impl->icon_size, NULL);
    }
  else if (shortcut_type == SHORTCUT_TYPE_FILE)
    {
      if (g_file_is_native (file))
        {
          struct ShortcutsInsertRequest *request;
	  GCancellable *cancellable;
          GtkTreePath *p;

          request = g_new0 (struct ShortcutsInsertRequest, 1);
          request->impl = g_object_ref (impl);
          request->file = g_object_ref (file);
          request->name_only = TRUE;
          request->removable = removable;
          request->pos = pos;
          request->type = type;
          if (label)
	    request->label_copy = g_strdup (label);

          if (pos == -1)
	    gtk_list_store_append (impl->shortcuts_model, &iter);
          else
	    gtk_list_store_insert (impl->shortcuts_model, &iter, pos);

          p = gtk_tree_model_get_path (GTK_TREE_MODEL (impl->shortcuts_model), &iter);
          request->row_ref = gtk_tree_row_reference_new (GTK_TREE_MODEL (impl->shortcuts_model), p);
          gtk_tree_path_free (p);

          cancellable = _gtk_file_system_get_info (request->impl->file_system, request->file,
						   "standard::is-hidden,standard::is-backup,standard::display-name,standard::icon",
						   get_file_info_finished, request);

          gtk_list_store_set (impl->shortcuts_model, &iter,
			      SHORTCUTS_COL_DATA, g_object_ref (file),
			      SHORTCUTS_COL_TYPE, SHORTCUT_TYPE_FILE,
			      SHORTCUTS_COL_CANCELLABLE, cancellable,
			      -1);

          shortcuts_update_count (impl, type, 1);

          return;
        }
      else
        {
          /* Don't call get_info for remote paths to avoid latency and
           * auth dialogs.
           */
          data = g_object_ref (file);
          if (label)
	    label_copy = g_strdup (label);
          else
	    label_copy = _gtk_file_chooser_label_for_file (file);

          /* If we switch to a better bookmarks file format (XBEL), we
           * should use mime info to get a better icon.
           */
          icon_theme = gtk_icon_theme_get_for_screen (gtk_widget_get_screen (GTK_WIDGET (impl)));
          pixbuf = gtk_icon_theme_load_icon (icon_theme, "folder-remote", 
					     impl->icon_size, 0, NULL);
        }
    }
   else
    {
      g_assert_not_reached ();

      return;
    }

  if (pos == -1)
    gtk_list_store_append (impl->shortcuts_model, &iter);
  else
    gtk_list_store_insert (impl->shortcuts_model, &iter, pos);

  shortcuts_update_count (impl, type, 1);

  gtk_list_store_set (impl->shortcuts_model, &iter,
		      SHORTCUTS_COL_PIXBUF, pixbuf,
		      SHORTCUTS_COL_PIXBUF_VISIBLE, TRUE,
		      SHORTCUTS_COL_NAME, label_copy,
		      SHORTCUTS_COL_DATA, data,
		      SHORTCUTS_COL_TYPE, shortcut_type,
		      SHORTCUTS_COL_REMOVABLE, removable,
		      SHORTCUTS_COL_CANCELLABLE, NULL,
		      -1);

  if (impl->shortcuts_pane_filter_model)
    gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (impl->shortcuts_pane_filter_model));

  g_free (label_copy);

  if (pixbuf)
    g_object_unref (pixbuf);

  profile_end ("end", NULL);
}

static void
shortcuts_append_search (GtkFileChooserDefault *impl)
{
  GdkPixbuf *pixbuf;
  GtkTreeIter iter;

  pixbuf = render_search_icon (impl);

  gtk_list_store_append (impl->shortcuts_model, &iter);
  gtk_list_store_set (impl->shortcuts_model, &iter,
		      SHORTCUTS_COL_PIXBUF, pixbuf,
		      SHORTCUTS_COL_PIXBUF_VISIBLE, TRUE,
		      SHORTCUTS_COL_NAME, _("Search"),
		      SHORTCUTS_COL_DATA, NULL,
		      SHORTCUTS_COL_TYPE, SHORTCUT_TYPE_SEARCH,
		      SHORTCUTS_COL_REMOVABLE, FALSE,
		      -1);

  if (pixbuf)
    g_object_unref (pixbuf);

  impl->has_search = TRUE;
}

static void
shortcuts_append_recent (GtkFileChooserDefault *impl)
{
  GdkPixbuf *pixbuf;
  GtkTreeIter iter;

  pixbuf = render_recent_icon (impl);

  gtk_list_store_append (impl->shortcuts_model, &iter);
  gtk_list_store_set (impl->shortcuts_model, &iter,
                      SHORTCUTS_COL_PIXBUF, pixbuf,
                      SHORTCUTS_COL_PIXBUF_VISIBLE, TRUE,
                      SHORTCUTS_COL_NAME, _("Recently Used"),
                      SHORTCUTS_COL_DATA, NULL,
                      SHORTCUTS_COL_TYPE, SHORTCUT_TYPE_RECENT,
                      SHORTCUTS_COL_REMOVABLE, FALSE,
                      -1);
  
  if (pixbuf)
    g_object_unref (pixbuf);
}

/* Appends the current working directory to the shortuts panel, but only if it is not equal to $HOME.
 * This is so that the user can actually use the $CWD, for example, if running an application
 * from the shell.
 */
static void
shortcuts_append_cwd (GtkFileChooserDefault *impl)
{
  char *cwd;
  const char *home;
  GFile *cwd_file;
  GFile *home_file;

  impl->has_cwd = FALSE;

  cwd = g_get_current_dir ();
  if (cwd == NULL)
    return;

  home = g_get_home_dir ();
  if (home == NULL)
    {
      g_free (cwd);
      return;
    }

  cwd_file = g_file_new_for_path (cwd);
  home_file = g_file_new_for_path (home);

  if (!g_file_equal (cwd_file, home_file))
    {
      shortcuts_insert_file (impl, -1, SHORTCUT_TYPE_FILE, NULL, cwd_file, NULL, FALSE, SHORTCUTS_CWD);
      impl->has_cwd = TRUE;
    }

  g_object_unref (cwd_file);
  g_object_unref (home_file);
  g_free (cwd);
}

/* Appends an item for the user's home directory to the shortcuts model */
static void
shortcuts_append_home (GtkFileChooserDefault *impl)
{
  const char *home_path;
  GFile *home;

  profile_start ("start", NULL);

  home_path = g_get_home_dir ();
  if (home_path == NULL)
    {
      profile_end ("end - no home directory!?", NULL);
      return;
    }

  home = g_file_new_for_path (home_path);
  shortcuts_insert_file (impl, -1, SHORTCUT_TYPE_FILE, NULL, home, NULL, FALSE, SHORTCUTS_HOME);
  impl->has_home = TRUE;

  g_object_unref (home);

  profile_end ("end", NULL);
}

/* Appends the ~/Desktop directory to the shortcuts model */
static void
shortcuts_append_desktop (GtkFileChooserDefault *impl)
{
  const char *name;
  GFile *file;

  profile_start ("start", NULL);

  name = g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP);
  /* "To disable a directory, point it to the homedir."
   * See http://freedesktop.org/wiki/Software/xdg-user-dirs
   **/
  if (!g_strcmp0 (name, g_get_home_dir ()))
    {
      profile_end ("end", NULL);
      return;
    }

  file = g_file_new_for_path (name);
  shortcuts_insert_file (impl, -1, SHORTCUT_TYPE_FILE, NULL, file, _("Desktop"), FALSE, SHORTCUTS_DESKTOP);
  impl->has_desktop = TRUE;

  /* We do not actually pop up an error dialog if there is no desktop directory
   * because some people may really not want to have one.
   */

  g_object_unref (file);

  profile_end ("end", NULL);
}

/* Appends a list of GFile to the shortcuts model; returns how many were inserted */
static int
shortcuts_append_bookmarks (GtkFileChooserDefault *impl,
			    GSList                *bookmarks)
{
  int start_row;
  int num_inserted;
  gchar *label;

  profile_start ("start", NULL);

  start_row = shortcuts_get_index (impl, SHORTCUTS_BOOKMARKS_SEPARATOR) + 1;
  num_inserted = 0;

  for (; bookmarks; bookmarks = bookmarks->next)
    {
      GFile *file;

      file = bookmarks->data;

      if (impl->local_only && !_gtk_file_has_native_path (file))
	continue;

      if (shortcut_find_position (impl, file) != -1)
        continue;

      label = _gtk_file_system_get_bookmark_label (impl->file_system, file);

      shortcuts_insert_file (impl, start_row + num_inserted, SHORTCUT_TYPE_FILE, NULL, file, label, TRUE, SHORTCUTS_BOOKMARKS);
      g_free (label);

      num_inserted++;
    }

  profile_end ("end", NULL);

  return num_inserted;
}

/* Returns the index for the corresponding item in the shortcuts bar */
static int
shortcuts_get_index (GtkFileChooserDefault *impl,
		     ShortcutsIndex         where)
{
  int n;

  n = 0;
  
  if (where == SHORTCUTS_SEARCH)
    goto out;

  n += impl->has_search ? 1 : 0;

  if (where == SHORTCUTS_RECENT)
    goto out;

  n += 1; /* we always have the recently-used item */

  if (where == SHORTCUTS_CWD)
    goto out;

  n += impl->has_cwd ? 1 : 0;

  if (where == SHORTCUTS_RECENT_SEPARATOR)
    goto out;

  n += 1; /* we always have the separator after the recently-used item */

  if (where == SHORTCUTS_HOME)
    goto out;

  n += impl->has_home ? 1 : 0;

  if (where == SHORTCUTS_DESKTOP)
    goto out;

  n += impl->has_desktop ? 1 : 0;

  if (where == SHORTCUTS_VOLUMES)
    goto out;

  n += impl->num_volumes;

  if (where == SHORTCUTS_SHORTCUTS)
    goto out;

  n += impl->num_shortcuts;

  if (where == SHORTCUTS_BOOKMARKS_SEPARATOR)
    goto out;

  /* If there are no bookmarks there won't be a separator */
  n += (impl->num_bookmarks > 0) ? 1 : 0;

  if (where == SHORTCUTS_BOOKMARKS)
    goto out;

  n += impl->num_bookmarks;

  if (where == SHORTCUTS_CURRENT_FOLDER_SEPARATOR)
    goto out;

  n += 1;

  if (where == SHORTCUTS_CURRENT_FOLDER)
    goto out;

  g_assert_not_reached ();

 out:

  return n;
}

/* Adds all the file system volumes to the shortcuts model */
static void
shortcuts_add_volumes (GtkFileChooserDefault *impl)
{
  int start_row;
  GSList *list, *l;
  int n;
  gboolean old_changing_folders;

  profile_start ("start", NULL);

  old_changing_folders = impl->changing_folder;
  impl->changing_folder = TRUE;

  start_row = shortcuts_get_index (impl, SHORTCUTS_VOLUMES);
  shortcuts_remove_rows (impl, start_row, impl->num_volumes);
  impl->num_volumes = 0;

  list = _gtk_file_system_list_volumes (impl->file_system);

  n = 0;

  for (l = list; l; l = l->next)
    {
      GtkFileSystemVolume *volume;

      volume = l->data;

      if (impl->local_only)
	{
	  if (_gtk_file_system_volume_is_mounted (volume))
	    {
	      GFile *base_file;
              gboolean base_has_native_path = FALSE;

	      base_file = _gtk_file_system_volume_get_root (volume);
              if (base_file != NULL)
                {
                  base_has_native_path = _gtk_file_has_native_path (base_file);
                  g_object_unref (base_file);
                }

              if (!base_has_native_path)
                continue;
	    }
	}

      shortcuts_insert_file (impl,
                             start_row + n,
                             SHORTCUT_TYPE_VOLUME,
                             _gtk_file_system_volume_ref (volume),
                             NULL,
                             NULL,
                             FALSE,
                             SHORTCUTS_VOLUMES);
      n++;
    }

  impl->num_volumes = n;
  g_slist_free (list);

  if (impl->shortcuts_pane_filter_model)
    gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (impl->shortcuts_pane_filter_model));

  impl->changing_folder = old_changing_folders;

  profile_end ("end", NULL);
}

/* Inserts a separator node in the shortcuts list */
static void
shortcuts_insert_separator (GtkFileChooserDefault *impl,
			    ShortcutsIndex         where)
{
  GtkTreeIter iter;

  g_assert (where == SHORTCUTS_RECENT_SEPARATOR || 
            where == SHORTCUTS_BOOKMARKS_SEPARATOR || 
	    where == SHORTCUTS_CURRENT_FOLDER_SEPARATOR);

  gtk_list_store_insert (impl->shortcuts_model, &iter,
			 shortcuts_get_index (impl, where));
  gtk_list_store_set (impl->shortcuts_model, &iter,
		      SHORTCUTS_COL_PIXBUF, NULL,
		      SHORTCUTS_COL_PIXBUF_VISIBLE, FALSE,
		      SHORTCUTS_COL_NAME, NULL,
		      SHORTCUTS_COL_DATA, NULL,
		      SHORTCUTS_COL_TYPE, SHORTCUT_TYPE_SEPARATOR,
		      -1);
}

/* Updates the list of bookmarks */
static void
shortcuts_add_bookmarks (GtkFileChooserDefault *impl)
{
  GSList *bookmarks;
  gboolean old_changing_folders;
  GtkTreeIter iter;
  GFile *list_selected = NULL;
  ShortcutType shortcut_type;
  gpointer col_data;

  profile_start ("start", NULL);
        
  old_changing_folders = impl->changing_folder;
  impl->changing_folder = TRUE;

  if (shortcuts_get_selected (impl, &iter))
    {
      gtk_tree_model_get (GTK_TREE_MODEL (impl->shortcuts_model), 
			  &iter, 
			  SHORTCUTS_COL_DATA, &col_data,
			  SHORTCUTS_COL_TYPE, &shortcut_type,
			  -1);

      if (col_data && shortcut_type == SHORTCUT_TYPE_FILE)
	list_selected = g_object_ref (col_data);
    }

  if (impl->num_bookmarks > 0)
    shortcuts_remove_rows (impl,
			   shortcuts_get_index (impl, SHORTCUTS_BOOKMARKS_SEPARATOR),
			   impl->num_bookmarks + 1);

  impl->num_bookmarks = 0;
  shortcuts_insert_separator (impl, SHORTCUTS_BOOKMARKS_SEPARATOR);

  bookmarks = _gtk_file_system_list_bookmarks (impl->file_system);
  shortcuts_append_bookmarks (impl, bookmarks);
  g_slist_foreach (bookmarks, (GFunc) g_object_unref, NULL);
  g_slist_free (bookmarks);

  if (impl->num_bookmarks == 0)
    shortcuts_remove_rows (impl, shortcuts_get_index (impl, SHORTCUTS_BOOKMARKS_SEPARATOR), 1);

  if (impl->shortcuts_pane_filter_model)
    gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (impl->shortcuts_pane_filter_model));

  if (list_selected)
    {
      shortcuts_find_folder (impl, list_selected);
      g_object_unref (list_selected);
    }

  impl->changing_folder = old_changing_folders;

  profile_end ("end", NULL);
}

/* Appends a separator and a row to the shortcuts list for the current folder */
static void
shortcuts_add_current_folder (GtkFileChooserDefault *impl)
{
  int pos;

  g_assert (!impl->shortcuts_current_folder_active);

  g_assert (impl->current_folder != NULL);

  pos = shortcut_find_position (impl, impl->current_folder);
  if (pos == -1)
    {
      GtkFileSystemVolume *volume;
      GFile *base_file;

      /* Separator */
      shortcuts_insert_separator (impl, SHORTCUTS_CURRENT_FOLDER_SEPARATOR);

      /* Item */
      pos = shortcuts_get_index (impl, SHORTCUTS_CURRENT_FOLDER);

      volume = _gtk_file_system_get_volume_for_file (impl->file_system, impl->current_folder);
      if (volume)
        base_file = _gtk_file_system_volume_get_root (volume);
      else
        base_file = NULL;

      if (base_file && g_file_equal (base_file, impl->current_folder))
        shortcuts_insert_file (impl, pos, SHORTCUT_TYPE_VOLUME, volume, NULL, NULL, FALSE, SHORTCUTS_CURRENT_FOLDER);
      else
        shortcuts_insert_file (impl, pos, SHORTCUT_TYPE_FILE, NULL, impl->current_folder, NULL, FALSE, SHORTCUTS_CURRENT_FOLDER);

      if (base_file)
        g_object_unref (base_file);
    }
}

/* Updates the current folder row in the shortcuts model */
static void
shortcuts_update_current_folder (GtkFileChooserDefault *impl)
{
  int pos;

  pos = shortcuts_get_index (impl, SHORTCUTS_CURRENT_FOLDER_SEPARATOR);

  if (impl->shortcuts_current_folder_active)
    {
      shortcuts_remove_rows (impl, pos, 2);
      impl->shortcuts_current_folder_active = FALSE;
    }

  shortcuts_add_current_folder (impl);
}

/* Filter function used for the shortcuts filter model */
static gboolean
shortcuts_pane_filter_cb (GtkTreeModel *model,
			  GtkTreeIter  *iter,
			  gpointer      data)
{
  GtkFileChooserDefault *impl;
  GtkTreePath *path;
  int pos;

  impl = GTK_FILE_CHOOSER_DEFAULT (data);

  path = gtk_tree_model_get_path (model, iter);
  if (!path)
    return FALSE;

  pos = *gtk_tree_path_get_indices (path);
  gtk_tree_path_free (path);

  return (pos < shortcuts_get_index (impl, SHORTCUTS_CURRENT_FOLDER_SEPARATOR));
}

/* Creates the list model for shortcuts */
static void
shortcuts_model_create (GtkFileChooserDefault *impl)
{
  /* Keep this order in sync with the SHORCUTS_COL_* enum values */
  impl->shortcuts_model = gtk_list_store_new (SHORTCUTS_COL_NUM_COLUMNS,
					      GDK_TYPE_PIXBUF,	/* pixbuf */
					      G_TYPE_STRING,	/* name */
					      G_TYPE_POINTER,	/* path or volume */
					      G_TYPE_INT,       /* ShortcutType */
					      G_TYPE_BOOLEAN,   /* removable */
					      G_TYPE_BOOLEAN,   /* pixbuf cell visibility */
					      G_TYPE_POINTER);  /* GCancellable */

  shortcuts_append_search (impl);

  if (impl->recent_manager)
    {
      shortcuts_append_recent (impl);
      shortcuts_insert_separator (impl, SHORTCUTS_RECENT_SEPARATOR);
    }
  
  if (impl->file_system)
    {
      shortcuts_append_cwd (impl);
      shortcuts_append_home (impl);
      shortcuts_append_desktop (impl);
      shortcuts_add_volumes (impl);
    }

  impl->shortcuts_pane_filter_model = shortcuts_pane_model_filter_new (impl,
							               GTK_TREE_MODEL (impl->shortcuts_model),
								       NULL);

  gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (impl->shortcuts_pane_filter_model),
					  shortcuts_pane_filter_cb,
					  impl,
					  NULL);
}

/* Callback used when the "New Folder" button is clicked */
static void
new_folder_button_clicked (GtkButton             *button,
			   GtkFileChooserDefault *impl)
{
  GtkTreeIter iter;
  GtkTreePath *path;

  if (!impl->browse_files_model)
    return; /* FIXME: this sucks.  Disable the New Folder button or something. */

  /* Prevent button from being clicked twice */
  gtk_widget_set_sensitive (impl->browse_new_folder_button, FALSE);

  _gtk_file_system_model_add_editable (impl->browse_files_model, &iter);

  path = gtk_tree_model_get_path (GTK_TREE_MODEL (impl->browse_files_model), &iter);
  gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (impl->browse_files_tree_view),
				path, impl->list_name_column,
				FALSE, 0.0, 0.0);

  g_object_set (impl->list_name_renderer, "editable", TRUE, NULL);
  gtk_tree_view_set_cursor (GTK_TREE_VIEW (impl->browse_files_tree_view),
			    path,
			    impl->list_name_column,
			    TRUE);

  gtk_tree_path_free (path);
}

static GSource *
add_idle_while_impl_is_alive (GtkFileChooserDefault *impl, GCallback callback)
{
  GSource *source;

  source = g_idle_source_new ();
  g_source_set_closure (source,
			g_cclosure_new_object (callback, G_OBJECT (impl)));
  g_source_attach (source, NULL);

  return source;
}

/* Idle handler for creating a new folder after editing its name cell, or for
 * canceling the editing.
 */
static gboolean
edited_idle_cb (GtkFileChooserDefault *impl)
{
  GDK_THREADS_ENTER ();
  
  g_source_destroy (impl->edited_idle);
  impl->edited_idle = NULL;

  _gtk_file_system_model_remove_editable (impl->browse_files_model);
  g_object_set (impl->list_name_renderer, "editable", FALSE, NULL);

  gtk_widget_set_sensitive (impl->browse_new_folder_button, TRUE);

  if (impl->edited_new_text /* not cancelled? */
      && (strlen (impl->edited_new_text) != 0)
      && (strcmp (impl->edited_new_text, DEFAULT_NEW_FOLDER_NAME) != 0)) /* Don't create folder if name is empty or has not been edited */
    {
      GError *error = NULL;
      GFile *file;

      file = g_file_get_child_for_display_name (impl->current_folder,
						impl->edited_new_text,
						&error);
      if (file)
	{
	  GError *error = NULL;

	  if (g_file_make_directory (file, NULL, &error))
	    change_folder_and_display_error (impl, file, FALSE);
	  else
	    error_creating_folder_dialog (impl, file, error);

	  g_object_unref (file);
	}
      else
	error_creating_folder_dialog (impl, file, error);

      g_free (impl->edited_new_text);
      impl->edited_new_text = NULL;
    }

  GDK_THREADS_LEAVE ();

  return FALSE;
}

static void
queue_edited_idle (GtkFileChooserDefault *impl,
		   const gchar           *new_text)
{
  /* We create the folder in an idle handler so that we don't modify the tree
   * just now.
   */

  if (!impl->edited_idle)
    impl->edited_idle = add_idle_while_impl_is_alive (impl, G_CALLBACK (edited_idle_cb));

  g_free (impl->edited_new_text);
  impl->edited_new_text = g_strdup (new_text);
}

/* Callback used from the text cell renderer when the new folder is named */
static void
renderer_edited_cb (GtkCellRendererText   *cell_renderer_text,
		    const gchar           *path,
		    const gchar           *new_text,
		    GtkFileChooserDefault *impl)
{
  /* work around bug #154921 */
  g_object_set (cell_renderer_text, 
		"mode", GTK_CELL_RENDERER_MODE_INERT, NULL);
  queue_edited_idle (impl, new_text);
}

/* Callback used from the text cell renderer when the new folder edition gets
 * canceled.
 */
static void
renderer_editing_canceled_cb (GtkCellRendererText   *cell_renderer_text,
			      GtkFileChooserDefault *impl)
{
  /* work around bug #154921 */
  g_object_set (cell_renderer_text, 
		"mode", GTK_CELL_RENDERER_MODE_INERT, NULL);
  queue_edited_idle (impl, NULL);
}

/* Creates the widgets for the filter combo box */
static GtkWidget *
filter_create (GtkFileChooserDefault *impl)
{
  impl->filter_combo = gtk_combo_box_text_new ();
  gtk_combo_box_set_focus_on_click (GTK_COMBO_BOX (impl->filter_combo), FALSE);

  g_signal_connect (impl->filter_combo, "changed",
		    G_CALLBACK (filter_combo_changed), impl);

  gtk_widget_set_tooltip_text (impl->filter_combo,
                               _("Select which types of files are shown"));

  return impl->filter_combo;
}

static GtkWidget *
toolbutton_new (GtkFileChooserDefault *impl,
                GIcon                 *icon,
                gboolean               sensitive,
                gboolean               show,
                GCallback              callback)
{
  GtkToolItem *item;
  GtkWidget *image;

  item = gtk_tool_button_new (NULL, NULL);
  image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_SMALL_TOOLBAR);
  gtk_widget_show (image);
  gtk_tool_button_set_icon_widget (GTK_TOOL_BUTTON (item), image);

  gtk_widget_set_sensitive (GTK_WIDGET (item), sensitive);
  g_signal_connect (item, "clicked", callback, impl);

  if (show)
    gtk_widget_show (GTK_WIDGET (item));

  return GTK_WIDGET (item);
}

/* Looks for a path among the shortcuts; returns its index or -1 if it doesn't exist */
static int
shortcut_find_position (GtkFileChooserDefault *impl,
			GFile                 *file)
{
  GtkTreeIter iter;
  int i;
  int current_folder_separator_idx;

  if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (impl->shortcuts_model), &iter))
    return -1;

  current_folder_separator_idx = shortcuts_get_index (impl, SHORTCUTS_CURRENT_FOLDER_SEPARATOR);

#if 0
  /* FIXME: is this still needed? */
  if (current_folder_separator_idx >= impl->shortcuts_model->length)
    return -1;
#endif

  for (i = 0; i < current_folder_separator_idx; i++)
    {
      gpointer col_data;
      ShortcutType shortcut_type;

      gtk_tree_model_get (GTK_TREE_MODEL (impl->shortcuts_model), &iter,
			  SHORTCUTS_COL_DATA, &col_data,
			  SHORTCUTS_COL_TYPE, &shortcut_type,
			  -1);

      if (col_data)
	{
	  if (shortcut_type == SHORTCUT_TYPE_VOLUME)
	    {
	      GtkFileSystemVolume *volume;
	      GFile *base_file;
	      gboolean exists;

	      volume = col_data;
	      base_file = _gtk_file_system_volume_get_root (volume);

	      exists = base_file && g_file_equal (file, base_file);

	      if (base_file)
		g_object_unref (base_file);

	      if (exists)
		return i;
	    }
	  else if (shortcut_type == SHORTCUT_TYPE_FILE)
	    {
	      GFile *model_file;

	      model_file = col_data;

	      if (model_file && g_file_equal (model_file, file))
		return i;
	    }
	}

      if (i < current_folder_separator_idx - 1)
	{
	  if (!gtk_tree_model_iter_next (GTK_TREE_MODEL (impl->shortcuts_model), &iter))
	    g_assert_not_reached ();
	}
    }

  return -1;
}

/* Tries to add a bookmark from a path name */
static gboolean
shortcuts_add_bookmark_from_file (GtkFileChooserDefault *impl,
				  GFile                 *file,
				  int                    pos)
{
  GError *error;

  g_return_val_if_fail (G_IS_FILE (file), FALSE);

  if (shortcut_find_position (impl, file) != -1)
    return FALSE;

  error = NULL;
  if (!_gtk_file_system_insert_bookmark (impl->file_system, file, pos, &error))
    {
      error_adding_bookmark_dialog (impl, file, error);
      return FALSE;
    }

  return TRUE;
}

static void
add_bookmark_foreach_cb (GtkTreeModel *model,
			 GtkTreePath  *path,
			 GtkTreeIter  *iter,
			 gpointer      data)
{
  GtkFileChooserDefault *impl;
  GFile *file;

  impl = (GtkFileChooserDefault *) data;

  gtk_tree_model_get (model, iter,
                      MODEL_COL_FILE, &file,
                      -1);

  shortcuts_add_bookmark_from_file (impl, file, -1);

  g_object_unref (file);
}

/* Adds a bookmark from the currently selected item in the file list */
static void
bookmarks_add_selected_folder (GtkFileChooserDefault *impl)
{
  GtkTreeSelection *selection;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->browse_files_tree_view));

  if (gtk_tree_selection_count_selected_rows (selection) == 0)
    shortcuts_add_bookmark_from_file (impl, impl->current_folder, -1);
  else
    gtk_tree_selection_selected_foreach (selection,
					 add_bookmark_foreach_cb,
					 impl);
}

/* Callback used when the "Add bookmark" button is clicked */
static void
add_bookmark_button_clicked_cb (GtkButton *button,
				GtkFileChooserDefault *impl)
{
  bookmarks_add_selected_folder (impl);
}

/* Returns TRUE plus an iter in the shortcuts_model if a row is selected;
 * returns FALSE if no shortcut is selected.
 */
static gboolean
shortcuts_get_selected (GtkFileChooserDefault *impl,
			GtkTreeIter           *iter)
{
  GtkTreeSelection *selection;
  GtkTreeIter parent_iter;

  if (!impl->browse_shortcuts_tree_view)
    return FALSE;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->browse_shortcuts_tree_view));

  if (!gtk_tree_selection_get_selected (selection, NULL, &parent_iter))
    return FALSE;

  gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (impl->shortcuts_pane_filter_model),
						    iter,
						    &parent_iter);
  return TRUE;
}

/* Removes the selected bookmarks */
static void
remove_selected_bookmarks (GtkFileChooserDefault *impl)
{
  GtkTreeIter iter;
  gpointer col_data;
  GFile *file;
  gboolean removable;
  GError *error;

  if (!shortcuts_get_selected (impl, &iter))
    return;

  gtk_tree_model_get (GTK_TREE_MODEL (impl->shortcuts_model), &iter,
		      SHORTCUTS_COL_DATA, &col_data,
		      SHORTCUTS_COL_REMOVABLE, &removable,
		      -1);

  if (!removable)
    return;

  g_assert (col_data != NULL);

  file = col_data;

  error = NULL;
  if (!_gtk_file_system_remove_bookmark (impl->file_system, file, &error))
    error_removing_bookmark_dialog (impl, file, error);
}

/* Callback used when the "Remove bookmark" button is clicked */
static void
remove_bookmark_button_clicked_cb (GtkButton *button,
				   GtkFileChooserDefault *impl)
{
  remove_selected_bookmarks (impl);
}

struct selection_check_closure {
  GtkFileChooserDefault *impl;
  int num_selected;
  gboolean all_files;
  gboolean all_folders;
};

/* Used from gtk_tree_selection_selected_foreach() */
static void
selection_check_foreach_cb (GtkTreeModel *model,
			    GtkTreePath  *path,
			    GtkTreeIter  *iter,
			    gpointer      data)
{
  struct selection_check_closure *closure;
  gboolean is_folder;
  GFile *file;

  gtk_tree_model_get (model, iter,
                      MODEL_COL_FILE, &file,
                      MODEL_COL_IS_FOLDER, &is_folder,
                      -1);

  if (file == NULL)
    return;

  g_object_unref (file);

  closure = data;
  closure->num_selected++;

  closure->all_folders = closure->all_folders && is_folder;
  closure->all_files = closure->all_files && !is_folder;
}

/* Checks whether the selected items in the file list are all files or all folders */
static void
selection_check (GtkFileChooserDefault *impl,
		 gint                  *num_selected,
		 gboolean              *all_files,
		 gboolean              *all_folders)
{
  struct selection_check_closure closure;
  GtkTreeSelection *selection;

  closure.impl = impl;
  closure.num_selected = 0;
  closure.all_files = TRUE;
  closure.all_folders = TRUE;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->browse_files_tree_view));
  gtk_tree_selection_selected_foreach (selection,
				       selection_check_foreach_cb,
				       &closure);

  g_assert (closure.num_selected == 0 || !(closure.all_files && closure.all_folders));

  if (num_selected)
    *num_selected = closure.num_selected;

  if (all_files)
    *all_files = closure.all_files;

  if (all_folders)
    *all_folders = closure.all_folders;
}

struct get_selected_file_closure {
  GtkFileChooserDefault *impl;
  GFile *file;
};

static void
get_selected_file_foreach_cb (GtkTreeModel *model,
			      GtkTreePath  *path,
			      GtkTreeIter  *iter,
			      gpointer      data)
{
  struct get_selected_file_closure *closure = data;

  if (closure->file)
    {
      /* Just in case this function gets run more than once with a multiple selection; we only care about one file */
      g_object_unref (closure->file);
      closure->file = NULL;
    }

  gtk_tree_model_get (model, iter,
                      MODEL_COL_FILE, &closure->file, /* this will give us a reffed file */
                      -1);
}

/* Returns a selected path from the file list */
static GFile *
get_selected_file (GtkFileChooserDefault *impl)
{
  struct get_selected_file_closure closure;
  GtkTreeSelection *selection;

  closure.impl = impl;
  closure.file = NULL;

  selection =  gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->browse_files_tree_view));
  gtk_tree_selection_selected_foreach (selection,
				       get_selected_file_foreach_cb,
				       &closure);

  return closure.file;
}

typedef struct {
  GtkFileChooserDefault *impl;
  gchar *tip;
} UpdateTooltipData;

static void 
update_tooltip (GtkTreeModel      *model,
		GtkTreePath       *path,
		GtkTreeIter       *iter,
		gpointer           data)
{
  UpdateTooltipData *udata = data;

  if (udata->tip == NULL)
    {
      gchar *display_name;

      gtk_tree_model_get (model, iter,
                          MODEL_COL_NAME, &display_name,
                          -1);

      udata->tip = g_strdup_printf (_("Add the folder '%s' to the bookmarks"),
                                    display_name);
      g_free (display_name);
    }
}


/* Sensitize the "add bookmark" button if all the selected items are folders, or
 * if there are no selected items *and* the current folder is not in the
 * bookmarks list.  De-sensitize the button otherwise.
 */
static void
bookmarks_check_add_sensitivity (GtkFileChooserDefault *impl)
{
  gint num_selected;
  gboolean all_folders;
  gboolean active;
  gchar *tip;

  selection_check (impl, &num_selected, NULL, &all_folders);

  if (num_selected == 0)
    active = (impl->current_folder != NULL) && (shortcut_find_position (impl, impl->current_folder) == -1);
  else if (num_selected == 1)
    {
      GFile *file;

      file = get_selected_file (impl);
      active = file && all_folders && (shortcut_find_position (impl, file) == -1);
      if (file)
	g_object_unref (file);
    }
  else
    active = all_folders;

  gtk_widget_set_sensitive (impl->browse_shortcuts_add_button, active);

  if (impl->browse_files_popup_menu_add_shortcut_item)
    gtk_widget_set_sensitive (impl->browse_files_popup_menu_add_shortcut_item,
                              (num_selected == 0) ? FALSE : active);

  if (active)
    {
      if (num_selected == 0)
        tip = g_strdup_printf (_("Add the current folder to the bookmarks"));
      else if (num_selected > 1)
        tip = g_strdup_printf (_("Add the selected folders to the bookmarks"));
      else
        {
          GtkTreeSelection *selection;
          UpdateTooltipData data;

          selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->browse_files_tree_view));
          data.impl = impl;
          data.tip = NULL;
          gtk_tree_selection_selected_foreach (selection, update_tooltip, &data);
          tip = data.tip;
        }

      gtk_widget_set_tooltip_text (impl->browse_shortcuts_add_button, tip);
      g_free (tip);
    }
}

/* Sets the sensitivity of the "remove bookmark" button depending on whether a
 * bookmark row is selected in the shortcuts tree.
 */
static void
bookmarks_check_remove_sensitivity (GtkFileChooserDefault *impl)
{
  GtkTreeIter iter;
  gboolean removable = FALSE;
  gboolean have_name = FALSE;
  gchar *name = NULL;
  
  if (shortcuts_get_selected (impl, &iter))
    {
      gtk_tree_model_get (GTK_TREE_MODEL (impl->shortcuts_model), &iter,
                          SHORTCUTS_COL_REMOVABLE, &removable,
                          SHORTCUTS_COL_NAME, &name,
                          -1);
      gtk_widget_set_sensitive (impl->browse_shortcuts_remove_button, removable);

      have_name = name != NULL && name[0] != '\0';
    }

  if (have_name)
    {
      char *tip;

      if (removable)
        tip = g_strdup_printf (_("Remove the bookmark '%s'"), name);
      else
        tip = g_strdup_printf (_("Bookmark '%s' cannot be removed"), name);

      gtk_widget_set_tooltip_text (impl->browse_shortcuts_remove_button, tip);
      g_free (tip);
    }
  else
    gtk_widget_set_tooltip_text (impl->browse_shortcuts_remove_button,
                                 _("Remove the selected bookmark"));
  g_free (name);
}

static void
shortcuts_check_popup_sensitivity (GtkFileChooserDefault *impl)
{
  GtkTreeIter iter;
  gboolean removable = FALSE;

  if (impl->browse_shortcuts_popup_menu == NULL)
    return;

  if (shortcuts_get_selected (impl, &iter))
    gtk_tree_model_get (GTK_TREE_MODEL (impl->shortcuts_model), &iter,
			SHORTCUTS_COL_REMOVABLE, &removable,
			-1);

  gtk_widget_set_sensitive (impl->browse_shortcuts_popup_menu_remove_item, removable);
  gtk_widget_set_sensitive (impl->browse_shortcuts_popup_menu_rename_item, removable);
}

/* GtkWidget::drag-begin handler for the shortcuts list. */
static void
shortcuts_drag_begin_cb (GtkWidget             *widget,
			 GdkDragContext        *context,
			 GtkFileChooserDefault *impl)
{
#if 0
  impl->shortcuts_drag_context = g_object_ref (context);
#endif
}

#if 0
/* Removes the idle handler for outside drags */
static void
shortcuts_cancel_drag_outside_idle (GtkFileChooserDefault *impl)
{
  if (!impl->shortcuts_drag_outside_idle)
    return;

  g_source_destroy (impl->shortcuts_drag_outside_idle);
  impl->shortcuts_drag_outside_idle = NULL;
}
#endif

/* GtkWidget::drag-end handler for the shortcuts list. */
static void
shortcuts_drag_end_cb (GtkWidget             *widget,
		       GdkDragContext        *context,
		       GtkFileChooserDefault *impl)
{
#if 0
  g_object_unref (impl->shortcuts_drag_context);

  shortcuts_cancel_drag_outside_idle (impl);

  if (!impl->shortcuts_drag_outside)
    return;

  gtk_button_clicked (GTK_BUTTON (impl->browse_shortcuts_remove_button));

  impl->shortcuts_drag_outside = FALSE;
#endif
}

/* GtkWidget::drag-data-delete handler for the shortcuts list. */
static void
shortcuts_drag_data_delete_cb (GtkWidget             *widget,
			       GdkDragContext        *context,
			       GtkFileChooserDefault *impl)
{
  g_signal_stop_emission_by_name (widget, "drag-data-delete");
}

/* GtkWidget::drag-leave handler for the shortcuts list.  We unhighlight the
 * drop position.
 */
static void
shortcuts_drag_leave_cb (GtkWidget             *widget,
			 GdkDragContext        *context,
			 guint                  time_,
			 GtkFileChooserDefault *impl)
{
#if 0
  if (gtk_drag_get_source_widget (context) == widget && !impl->shortcuts_drag_outside_idle)
    {
      impl->shortcuts_drag_outside_idle = add_idle_while_impl_is_alive (impl, G_CALLBACK (shortcuts_drag_outside_idle_cb));
    }
#endif

  gtk_tree_view_set_drag_dest_row (GTK_TREE_VIEW (impl->browse_shortcuts_tree_view),
				   NULL,
				   GTK_TREE_VIEW_DROP_BEFORE);

  g_signal_stop_emission_by_name (widget, "drag-leave");
}

/* Computes the appropriate row and position for dropping */
static void
shortcuts_compute_drop_position (GtkFileChooserDefault   *impl,
				 int                      x,
				 int                      y,
				 GtkTreePath            **path,
				 GtkTreeViewDropPosition *pos)
{
  GtkTreeView *tree_view;
  GtkTreeViewColumn *column;
  int cell_y;
  GdkRectangle cell;
  int row;
  int bookmarks_index;
  int header_height = 0;

  tree_view = GTK_TREE_VIEW (impl->browse_shortcuts_tree_view);

  if (gtk_tree_view_get_headers_visible (tree_view))
    header_height = TREE_VIEW_HEADER_HEIGHT (tree_view);

  bookmarks_index = shortcuts_get_index (impl, SHORTCUTS_BOOKMARKS);

  if (!gtk_tree_view_get_path_at_pos (tree_view,
                                      x,
				      y - header_height,
                                      path,
                                      &column,
                                      NULL,
                                      &cell_y))
    {
      row = bookmarks_index + impl->num_bookmarks - 1;
      *path = gtk_tree_path_new_from_indices (row, -1);
      *pos = GTK_TREE_VIEW_DROP_AFTER;
      return;
    }

  row = *gtk_tree_path_get_indices (*path);
  gtk_tree_view_get_background_area (tree_view, *path, column, &cell);
  gtk_tree_path_free (*path);

  if (row < bookmarks_index)
    {
      row = bookmarks_index;
      *pos = GTK_TREE_VIEW_DROP_BEFORE;
    }
  else if (row > bookmarks_index + impl->num_bookmarks - 1)
    {
      row = bookmarks_index + impl->num_bookmarks - 1;
      *pos = GTK_TREE_VIEW_DROP_AFTER;
    }
  else
    {
      if (cell_y < cell.height / 2)
	*pos = GTK_TREE_VIEW_DROP_BEFORE;
      else
	*pos = GTK_TREE_VIEW_DROP_AFTER;
    }

  *path = gtk_tree_path_new_from_indices (row, -1);
}

/* GtkWidget::drag-motion handler for the shortcuts list.  We basically
 * implement the destination side of DnD by hand, due to limitations in
 * GtkTreeView's DnD API.
 */
static gboolean
shortcuts_drag_motion_cb (GtkWidget             *widget,
			  GdkDragContext        *context,
			  gint                   x,
			  gint                   y,
			  guint                  time_,
			  GtkFileChooserDefault *impl)
{
  GtkTreePath *path;
  GtkTreeViewDropPosition pos;
  GdkDragAction action;

#if 0
  if (gtk_drag_get_source_widget (context) == widget)
    {
      shortcuts_cancel_drag_outside_idle (impl);

      if (impl->shortcuts_drag_outside)
	{
	  shortcuts_drag_set_delete_cursor (impl, FALSE);
	  impl->shortcuts_drag_outside = FALSE;
	}
    }
#endif

  if (gdk_drag_context_get_suggested_action (context) == GDK_ACTION_COPY ||
      (gdk_drag_context_get_actions (context) & GDK_ACTION_COPY) != 0)
    action = GDK_ACTION_COPY;
  else if (gdk_drag_context_get_suggested_action (context) == GDK_ACTION_MOVE ||
           (gdk_drag_context_get_actions (context) & GDK_ACTION_MOVE) != 0)
    action = GDK_ACTION_MOVE;
  else
    {
      action = 0;
      goto out;
    }

  shortcuts_compute_drop_position (impl, x, y, &path, &pos);
  gtk_tree_view_set_drag_dest_row (GTK_TREE_VIEW (impl->browse_shortcuts_tree_view), path, pos);
  gtk_tree_path_free (path);

 out:

  g_signal_stop_emission_by_name (widget, "drag-motion");

  if (action != 0)
    {
      gdk_drag_status (context, action, time_);
      return TRUE;
    }
  else
    return FALSE;
}

/* GtkWidget::drag-drop handler for the shortcuts list. */
static gboolean
shortcuts_drag_drop_cb (GtkWidget             *widget,
			GdkDragContext        *context,
			gint                   x,
			gint                   y,
			guint                  time_,
			GtkFileChooserDefault *impl)
{
#if 0
  shortcuts_cancel_drag_outside_idle (impl);
#endif

  g_signal_stop_emission_by_name (widget, "drag-drop");
  return TRUE;
}

/* Parses a "text/uri-list" string and inserts its URIs as bookmarks */
static void
shortcuts_drop_uris (GtkFileChooserDefault *impl,
		     GtkSelectionData      *selection_data,
		     int                    position)
{
  gchar **uris;
  gint i;

  uris = gtk_selection_data_get_uris (selection_data);
  if (!uris)
    return;

  for (i = 0; uris[i]; i++)
    {
      char *uri;
      GFile *file;

      uri = uris[i];
      file = g_file_new_for_uri (uri);

      if (shortcuts_add_bookmark_from_file (impl, file, position))
	position++;

      g_object_unref (file);
    }

  g_strfreev (uris);
}

/* Reorders the selected bookmark to the specified position */
static void
shortcuts_reorder (GtkFileChooserDefault *impl,
                   GtkSelectionData      *selection_data,
		   int                    new_position)
{
  GtkTreeIter iter;
  GtkTreeIter filter_iter;
  gpointer col_data;
  ShortcutType shortcut_type;
  GtkTreePath *path;
  int old_position;
  int bookmarks_index;
  GFile *file;
  GError *error;
  gchar *name;
  GtkTreeModel *model;

  /* Get the selected path */

  if (!gtk_tree_get_row_drag_data (selection_data, &model, &path))
    return;

  g_assert (model == impl->shortcuts_pane_filter_model);

  gtk_tree_model_get_iter (model, &filter_iter, path);
  gtk_tree_path_free (path);

  gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (impl->shortcuts_pane_filter_model),
                                                    &iter,
						    &filter_iter);
  path = gtk_tree_model_get_path (GTK_TREE_MODEL (impl->shortcuts_model), &iter);

  old_position = *gtk_tree_path_get_indices (path);
  gtk_tree_path_free (path);

  bookmarks_index = shortcuts_get_index (impl, SHORTCUTS_BOOKMARKS);
  old_position -= bookmarks_index;
  g_assert (old_position >= 0 && old_position < impl->num_bookmarks);

  gtk_tree_model_get (GTK_TREE_MODEL (impl->shortcuts_model), &iter,
		      SHORTCUTS_COL_NAME, &name,
		      SHORTCUTS_COL_DATA, &col_data,
		      SHORTCUTS_COL_TYPE, &shortcut_type,
		      -1);
  g_assert (col_data != NULL);
  g_assert (shortcut_type == SHORTCUT_TYPE_FILE);
  
  file = col_data;
  g_object_ref (file); /* removal below will free file, so we need a new ref */

  /* Remove the path from the old position and insert it in the new one */

  if (new_position > old_position)
    new_position--;

  if (old_position == new_position)
    goto out;

  error = NULL;
  if (_gtk_file_system_remove_bookmark (impl->file_system, file, &error))
    {
      shortcuts_add_bookmark_from_file (impl, file, new_position);
      _gtk_file_system_set_bookmark_label (impl->file_system, file, name);
    }
  else
    error_adding_bookmark_dialog (impl, file, error);

 out:

  g_object_unref (file);
}

/* Callback used when we get the drag data for the bookmarks list.  We add the
 * received URIs as bookmarks if they are folders.
 */
static void
shortcuts_drag_data_received_cb (GtkWidget          *widget,
				 GdkDragContext     *context,
				 gint                x,
				 gint                y,
				 GtkSelectionData   *selection_data,
				 guint               info,
				 guint               time_,
				 gpointer            data)
{
  GtkFileChooserDefault *impl;
  GtkTreePath *tree_path;
  GtkTreeViewDropPosition tree_pos;
  GdkAtom target;
  int position;
  int bookmarks_index;

  impl = GTK_FILE_CHOOSER_DEFAULT (data);

  /* Compute position */

  bookmarks_index = shortcuts_get_index (impl, SHORTCUTS_BOOKMARKS);

  shortcuts_compute_drop_position (impl, x, y, &tree_path, &tree_pos);
  position = *gtk_tree_path_get_indices (tree_path);
  gtk_tree_path_free (tree_path);

  if (tree_pos == GTK_TREE_VIEW_DROP_AFTER)
    position++;

  g_assert (position >= bookmarks_index);
  position -= bookmarks_index;

  target = gtk_selection_data_get_target (selection_data);

  if (gtk_targets_include_uri (&target, 1))
    shortcuts_drop_uris (impl, selection_data, position);
  else if (target == gdk_atom_intern_static_string ("GTK_TREE_MODEL_ROW"))
    shortcuts_reorder (impl, selection_data, position);

  g_signal_stop_emission_by_name (widget, "drag-data-received");
}

/* Callback used to display a tooltip in the shortcuts tree */
static gboolean
shortcuts_query_tooltip_cb (GtkWidget             *widget,
			    gint                   x,
			    gint                   y,
			    gboolean               keyboard_mode,
			    GtkTooltip            *tooltip,
			    GtkFileChooserDefault *impl)
{
  GtkTreeModel *model;
  GtkTreeIter iter;

  if (gtk_tree_view_get_tooltip_context (GTK_TREE_VIEW (widget),
					 &x, &y,
					 keyboard_mode,
					 &model,
					 NULL,
					 &iter))
    {
      gpointer col_data;
      ShortcutType shortcut_type;

      gtk_tree_model_get (model, &iter,
			  SHORTCUTS_COL_DATA, &col_data,
			  SHORTCUTS_COL_TYPE, &shortcut_type,
			  -1);

      if (shortcut_type == SHORTCUT_TYPE_SEPARATOR)
	return FALSE;
      else if (shortcut_type == SHORTCUT_TYPE_VOLUME)
        return FALSE;
      else if (shortcut_type == SHORTCUT_TYPE_FILE)
	{
	  GFile *file;
	  char *parse_name;

	  file = G_FILE (col_data);
	  parse_name = g_file_get_parse_name (file);

	  gtk_tooltip_set_text (tooltip, parse_name);

	  g_free (parse_name);

	  return TRUE;
	}
      else if (shortcut_type == SHORTCUT_TYPE_SEARCH)
	{
	  return FALSE;
	}
      else if (shortcut_type == SHORTCUT_TYPE_RECENT)
	{
	  return FALSE;
	}
    }

  return FALSE;
}


/* Callback used when the selection in the shortcuts tree changes */
static void
shortcuts_selection_changed_cb (GtkTreeSelection      *selection,
				GtkFileChooserDefault *impl)
{
  GtkTreeIter iter;
  GtkTreeIter child_iter;

  bookmarks_check_remove_sensitivity (impl);
  shortcuts_check_popup_sensitivity (impl);

  if (impl->changing_folder)
    return;

  if (gtk_tree_selection_get_selected(selection, NULL, &iter))
    {
      gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (impl->shortcuts_pane_filter_model),
							&child_iter,
							&iter);
      shortcuts_activate_iter (impl, &child_iter);
    }
}

static gboolean
shortcuts_row_separator_func (GtkTreeModel *model,
			      GtkTreeIter  *iter,
			      gpointer      data)
{
  ShortcutType shortcut_type;

  gtk_tree_model_get (model, iter, SHORTCUTS_COL_TYPE, &shortcut_type, -1);
  
  return shortcut_type == SHORTCUT_TYPE_SEPARATOR;
}

static gboolean
shortcuts_key_press_event_after_cb (GtkWidget             *tree_view,
				    GdkEventKey           *event,
				    GtkFileChooserDefault *impl)
{
  GtkWidget *entry;

  /* don't screw up focus switching with Tab */
  if (event->keyval == GDK_KEY_Tab
      || event->keyval == GDK_KEY_KP_Tab
      || event->keyval == GDK_KEY_ISO_Left_Tab
      || event->length < 1)
    return FALSE;

  if (impl->location_entry)
    entry = impl->location_entry;
  else if (impl->search_entry)
    entry = impl->search_entry;
  else
    entry = NULL;

  if (entry)
    {
      gtk_widget_grab_focus (entry);
      return gtk_widget_event (entry, (GdkEvent *) event);
    }
  else
    return FALSE;
}

/* Callback used when the file list's popup menu is detached */
static void
shortcuts_popup_menu_detach_cb (GtkWidget *attach_widget,
				GtkMenu   *menu)
{
  GtkFileChooserDefault *impl;
  
  impl = g_object_get_data (G_OBJECT (attach_widget), "GtkFileChooserDefault");
  g_assert (GTK_IS_FILE_CHOOSER_DEFAULT (impl));

  impl->browse_shortcuts_popup_menu = NULL;
  impl->browse_shortcuts_popup_menu_remove_item = NULL;
  impl->browse_shortcuts_popup_menu_rename_item = NULL;
}

static void
remove_shortcut_cb (GtkMenuItem           *item,
		    GtkFileChooserDefault *impl)
{
  remove_selected_bookmarks (impl);
}

/* Rename the selected bookmark */
static void
rename_selected_bookmark (GtkFileChooserDefault *impl)
{
  GtkTreeIter iter;
  GtkTreePath *path;
  GtkTreeViewColumn *column;
  GtkCellRenderer *cell;
  GList *renderers;

  if (shortcuts_get_selected (impl, &iter))
    {
      path = gtk_tree_model_get_path (GTK_TREE_MODEL (impl->shortcuts_model), &iter);
      column = gtk_tree_view_get_column (GTK_TREE_VIEW (impl->browse_shortcuts_tree_view), 0);
      renderers = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (column));
      cell = g_list_nth_data (renderers, 1);
      g_list_free (renderers);
      g_object_set (cell, "editable", TRUE, NULL);
      gtk_tree_view_set_cursor_on_cell (GTK_TREE_VIEW (impl->browse_shortcuts_tree_view),
					path, column, cell, TRUE);
      gtk_tree_path_free (path);
    }
}

static void
rename_shortcut_cb (GtkMenuItem           *item,
		    GtkFileChooserDefault *impl)
{
  rename_selected_bookmark (impl);
}

/* Constructs the popup menu for the file list if needed */
static void
shortcuts_build_popup_menu (GtkFileChooserDefault *impl)
{
  GtkWidget *item;

  if (impl->browse_shortcuts_popup_menu)
    return;

  impl->browse_shortcuts_popup_menu = gtk_menu_new ();
  gtk_menu_attach_to_widget (GTK_MENU (impl->browse_shortcuts_popup_menu),
			     impl->browse_shortcuts_tree_view,
			     shortcuts_popup_menu_detach_cb);

  item = gtk_image_menu_item_new_with_label (_("Remove"));
  impl->browse_shortcuts_popup_menu_remove_item = item;
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
				 gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU));
  g_signal_connect (item, "activate",
		    G_CALLBACK (remove_shortcut_cb), impl);
  gtk_widget_show (item);
  gtk_menu_shell_append (GTK_MENU_SHELL (impl->browse_shortcuts_popup_menu), item);

  item = gtk_menu_item_new_with_label (_("Rename..."));
  impl->browse_shortcuts_popup_menu_rename_item = item;
  g_signal_connect (item, "activate",
		    G_CALLBACK (rename_shortcut_cb), impl);
  gtk_widget_show (item);
  gtk_menu_shell_append (GTK_MENU_SHELL (impl->browse_shortcuts_popup_menu), item);

  item = gtk_separator_menu_item_new ();
  gtk_widget_show (item);
  gtk_menu_shell_append (GTK_MENU_SHELL (impl->browse_shortcuts_popup_menu), item);
}

static void
shortcuts_update_popup_menu (GtkFileChooserDefault *impl)
{
  shortcuts_build_popup_menu (impl);  
  shortcuts_check_popup_sensitivity (impl);
}

static void
popup_position_func (GtkMenu   *menu,
                     gint      *x,
                     gint      *y,
                     gboolean  *push_in,
                     gpointer	user_data);

static void
shortcuts_popup_menu (GtkFileChooserDefault *impl,
		      GdkEventButton        *event)
{
  shortcuts_update_popup_menu (impl);
  if (event)
    gtk_menu_popup (GTK_MENU (impl->browse_shortcuts_popup_menu),
		    NULL, NULL, NULL, NULL,
		    event->button, event->time);
  else
    {
      gtk_menu_popup (GTK_MENU (impl->browse_shortcuts_popup_menu),
		      NULL, NULL,
		      popup_position_func, impl->browse_shortcuts_tree_view,
		      0, GDK_CURRENT_TIME);
      gtk_menu_shell_select_first (GTK_MENU_SHELL (impl->browse_shortcuts_popup_menu),
				   FALSE);
    }
}

/* Callback used for the GtkWidget::popup-menu signal of the shortcuts list */
static gboolean
shortcuts_popup_menu_cb (GtkWidget *widget,
			 GtkFileChooserDefault *impl)
{
  shortcuts_popup_menu (impl, NULL);
  return TRUE;
}

/* Callback used when a button is pressed on the shortcuts list.  
 * We trap button 3 to bring up a popup menu.
 */
static gboolean
shortcuts_button_press_event_cb (GtkWidget             *widget,
				 GdkEventButton        *event,
				 GtkFileChooserDefault *impl)
{
  static gboolean in_press = FALSE;
  gboolean handled;

  if (in_press)
    return FALSE;

  if (!_gtk_button_event_triggers_context_menu (event))
    return FALSE;

  in_press = TRUE;
  handled = gtk_widget_event (impl->browse_shortcuts_tree_view, (GdkEvent *) event);
  in_press = FALSE;

  if (!handled)
    return FALSE;

  shortcuts_popup_menu (impl, event);
  return TRUE;
}

static void
shortcuts_edited (GtkCellRenderer       *cell,
		  gchar                 *path_string,
		  gchar                 *new_text,
		  GtkFileChooserDefault *impl)
{
  GtkTreePath *path;
  GtkTreeIter iter;
  GFile *shortcut;

  g_object_set (cell, "editable", FALSE, NULL);

  path = gtk_tree_path_new_from_string (path_string);
  if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (impl->shortcuts_model), &iter, path))
    g_assert_not_reached ();

  gtk_tree_model_get (GTK_TREE_MODEL (impl->shortcuts_model), &iter,
		      SHORTCUTS_COL_DATA, &shortcut,
		      -1);
  gtk_tree_path_free (path);
  
  _gtk_file_system_set_bookmark_label (impl->file_system, shortcut, new_text);
}

static void
shortcuts_editing_canceled (GtkCellRenderer       *cell,
			    GtkFileChooserDefault *impl)
{
  g_object_set (cell, "editable", FALSE, NULL);
}

/* Creates the widgets for the shortcuts and bookmarks tree */
static GtkWidget *
shortcuts_list_create (GtkFileChooserDefault *impl)
{
  GtkWidget *swin;
  GtkTreeSelection *selection;
  GtkTreeViewColumn *column;
  GtkCellRenderer *renderer;

  /* Target types for dragging a row to/from the shortcuts list */
  const GtkTargetEntry tree_model_row_targets[] = {
    { "GTK_TREE_MODEL_ROW", GTK_TARGET_SAME_WIDGET, GTK_TREE_MODEL_ROW }
  };

  /* Scrolled window */

  swin = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swin),
				  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swin),
				       GTK_SHADOW_IN);
  gtk_widget_show (swin);

  /* Tree */

  impl->browse_shortcuts_tree_view = gtk_tree_view_new ();
  gtk_tree_view_set_enable_search (GTK_TREE_VIEW (impl->browse_shortcuts_tree_view), FALSE);
#ifdef PROFILE_FILE_CHOOSER
  g_object_set_data (G_OBJECT (impl->browse_shortcuts_tree_view), "fmq-name", "shortcuts");
#endif

  /* Connect "after" to key-press-event on the shortcuts pane.  We want this action to be possible:
   *
   *   1. user brings up a SAVE dialog
   *   2. user clicks on a shortcut in the shortcuts pane
   *   3. user starts typing a filename
   *
   * Normally, the user's typing would be ignored, as the shortcuts treeview doesn't
   * support interactive search.  However, we'd rather focus the location entry
   * so that the user can type *there*.
   *
   * To preserve keyboard navigation in the shortcuts pane, we don't focus the
   * filename entry if one clicks on a shortcut; rather, we focus the entry only
   * if the user starts typing while the focus is in the shortcuts pane.
   */
  g_signal_connect_after (impl->browse_shortcuts_tree_view, "key-press-event",
			  G_CALLBACK (shortcuts_key_press_event_after_cb), impl);

  g_signal_connect (impl->browse_shortcuts_tree_view, "popup-menu",
		    G_CALLBACK (shortcuts_popup_menu_cb), impl);
  g_signal_connect (impl->browse_shortcuts_tree_view, "button-press-event",
		    G_CALLBACK (shortcuts_button_press_event_cb), impl);
  /* Accessible object name for the file chooser's shortcuts pane */
  atk_object_set_name (gtk_widget_get_accessible (impl->browse_shortcuts_tree_view), _("Places"));

  gtk_tree_view_set_model (GTK_TREE_VIEW (impl->browse_shortcuts_tree_view), impl->shortcuts_pane_filter_model);

  gtk_tree_view_enable_model_drag_source (GTK_TREE_VIEW (impl->browse_shortcuts_tree_view),
					  GDK_BUTTON1_MASK,
					  tree_model_row_targets,
					  G_N_ELEMENTS (tree_model_row_targets),
					  GDK_ACTION_MOVE);

  gtk_drag_dest_set (impl->browse_shortcuts_tree_view,
		     GTK_DEST_DEFAULT_ALL,
		     tree_model_row_targets,
		     G_N_ELEMENTS (tree_model_row_targets),
		     GDK_ACTION_COPY | GDK_ACTION_MOVE);
  gtk_drag_dest_add_uri_targets (impl->browse_shortcuts_tree_view);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->browse_shortcuts_tree_view));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
  gtk_tree_selection_set_select_function (selection,
					  shortcuts_select_func,
					  impl, NULL);

  g_signal_connect (selection, "changed",
		    G_CALLBACK (shortcuts_selection_changed_cb), impl);

  g_signal_connect (impl->browse_shortcuts_tree_view, "key-press-event",
		    G_CALLBACK (shortcuts_key_press_event_cb), impl);

  g_signal_connect (impl->browse_shortcuts_tree_view, "drag-begin",
		    G_CALLBACK (shortcuts_drag_begin_cb), impl);
  g_signal_connect (impl->browse_shortcuts_tree_view, "drag-end",
		    G_CALLBACK (shortcuts_drag_end_cb), impl);
  g_signal_connect (impl->browse_shortcuts_tree_view, "drag-data-delete",
		    G_CALLBACK (shortcuts_drag_data_delete_cb), impl);

  g_signal_connect (impl->browse_shortcuts_tree_view, "drag-leave",
		    G_CALLBACK (shortcuts_drag_leave_cb), impl);
  g_signal_connect (impl->browse_shortcuts_tree_view, "drag-motion",
		    G_CALLBACK (shortcuts_drag_motion_cb), impl);
  g_signal_connect (impl->browse_shortcuts_tree_view, "drag-drop",
		    G_CALLBACK (shortcuts_drag_drop_cb), impl);
  g_signal_connect (impl->browse_shortcuts_tree_view, "drag-data-received",
		    G_CALLBACK (shortcuts_drag_data_received_cb), impl);

  /* Support tooltips */
  gtk_widget_set_has_tooltip (impl->browse_shortcuts_tree_view, TRUE);
  g_signal_connect (impl->browse_shortcuts_tree_view, "query-tooltip",
		    G_CALLBACK (shortcuts_query_tooltip_cb), impl);

  gtk_container_add (GTK_CONTAINER (swin), impl->browse_shortcuts_tree_view);
  gtk_widget_show (impl->browse_shortcuts_tree_view);

  /* Column */

  column = gtk_tree_view_column_new ();
  /* Column header for the file chooser's shortcuts pane */
  gtk_tree_view_column_set_title (column, _("_Places"));

  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, renderer,
				       "pixbuf", SHORTCUTS_COL_PIXBUF,
				       "visible", SHORTCUTS_COL_PIXBUF_VISIBLE,
				       NULL);

  renderer = gtk_cell_renderer_text_new ();
  g_object_set (renderer,
                "width-chars", 12,
                "ellipsize", PANGO_ELLIPSIZE_END,
                NULL);
  g_signal_connect (renderer, "edited",
		    G_CALLBACK (shortcuts_edited), impl);
  g_signal_connect (renderer, "editing-canceled",
		    G_CALLBACK (shortcuts_editing_canceled), impl);
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_attributes (column, renderer,
				       "text", SHORTCUTS_COL_NAME,
				       NULL);

  gtk_tree_view_set_row_separator_func (GTK_TREE_VIEW (impl->browse_shortcuts_tree_view),
					shortcuts_row_separator_func,
					NULL, NULL);

  gtk_tree_view_append_column (GTK_TREE_VIEW (impl->browse_shortcuts_tree_view), column);

  return swin;
}

/* Creates the widgets for the shortcuts/bookmarks pane */
static GtkWidget *
shortcuts_pane_create (GtkFileChooserDefault *impl,
		       GtkSizeGroup          *size_group)
{
  GtkWidget *vbox;
  GtkWidget *toolbar;
  GtkWidget *widget;
  GIcon *icon;

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox);

  /* Shortcuts tree */

  widget = shortcuts_list_create (impl);

  gtk_size_group_add_widget (size_group, widget);

  gtk_box_pack_start (GTK_BOX (vbox), widget, TRUE, TRUE, 0);

  /* Box for buttons */

  toolbar = gtk_toolbar_new ();
  gtk_toolbar_set_style (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_ICONS);

  gtk_toolbar_set_icon_size (GTK_TOOLBAR (toolbar), GTK_ICON_SIZE_MENU);

  gtk_box_pack_start (GTK_BOX (vbox), toolbar, FALSE, FALSE, 0);
  gtk_widget_show (toolbar);

  /* Add bookmark button */
  icon = g_themed_icon_new_with_default_fallbacks ("list-add-symbolic");
  impl->browse_shortcuts_add_button = toolbutton_new (impl,
                                                      icon,
                                                      FALSE,
                                                      TRUE,
                                                      G_CALLBACK (add_bookmark_button_clicked_cb));
  g_object_unref (icon);

  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (impl->browse_shortcuts_add_button), 0);
  gtk_widget_set_tooltip_text (impl->browse_shortcuts_add_button,
                               _("Add the selected folder to the Bookmarks"));

  /* Remove bookmark button */
  icon = g_themed_icon_new_with_default_fallbacks ("list-remove-symbolic");
  impl->browse_shortcuts_remove_button = toolbutton_new (impl,
                                                         icon,
                                                         FALSE,
                                                         TRUE,
                                                         G_CALLBACK (remove_bookmark_button_clicked_cb));
  g_object_unref (icon);
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (impl->browse_shortcuts_remove_button), 1);
  gtk_widget_set_tooltip_text (impl->browse_shortcuts_remove_button,
                               _("Remove the selected bookmark"));

  return vbox;
}

static gboolean
key_is_left_or_right (GdkEventKey *event)
{
  guint modifiers;

  modifiers = gtk_accelerator_get_default_mod_mask ();

  return ((event->keyval == GDK_KEY_Right
	   || event->keyval == GDK_KEY_KP_Right
	   || event->keyval == GDK_KEY_Left
	   || event->keyval == GDK_KEY_KP_Left)
	  && (event->state & modifiers) == 0);
}

/* Handles key press events on the file list, so that we can trap Enter to
 * activate the default button on our own.  Also, checks to see if '/' has been
 * pressed.
 */
static gboolean
browse_files_key_press_event_cb (GtkWidget   *widget,
				 GdkEventKey *event,
				 gpointer     data)
{
  GtkFileChooserDefault *impl;

  impl = (GtkFileChooserDefault *) data;

  if ((event->keyval == GDK_KEY_slash
       || event->keyval == GDK_KEY_KP_Divide
#ifdef G_OS_UNIX
       || event->keyval == GDK_KEY_asciitilde
#endif
       ) && !(event->state & GTK_NO_TEXT_INPUT_MOD_MASK))
    {
      location_popup_handler (impl, event->string);
      return TRUE;
    }

  if (key_is_left_or_right (event))
    {
      gtk_widget_grab_focus (impl->browse_shortcuts_tree_view);
      return TRUE;
    }

  if ((event->keyval == GDK_KEY_Return
       || event->keyval == GDK_KEY_ISO_Enter
       || event->keyval == GDK_KEY_KP_Enter
       || event->keyval == GDK_KEY_space
       || event->keyval == GDK_KEY_KP_Space)
      && !(event->state & gtk_accelerator_get_default_mod_mask ())
      && !(impl->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER ||
	   impl->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER))
    {
      GtkWindow *window;

      window = get_toplevel (widget);
      if (window)
        {
          GtkWidget *default_widget, *focus_widget;

          default_widget = gtk_window_get_default_widget (window);
          focus_widget = gtk_window_get_focus (window);

          if (widget != default_widget &&
              !(widget == focus_widget && (!default_widget || !gtk_widget_get_sensitive (default_widget))))
	    {
	      gtk_window_activate_default (window);

	      return TRUE;
	    }
        }
    }

  return FALSE;
}

/* Callback used when the file list's popup menu is detached */
static void
popup_menu_detach_cb (GtkWidget *attach_widget,
		      GtkMenu   *menu)
{
  GtkFileChooserDefault *impl;

  impl = g_object_get_data (G_OBJECT (attach_widget), "GtkFileChooserDefault");
  g_assert (GTK_IS_FILE_CHOOSER_DEFAULT (impl));

  impl->browse_files_popup_menu = NULL;
  impl->browse_files_popup_menu_add_shortcut_item = NULL;
  impl->browse_files_popup_menu_hidden_files_item = NULL;
}

/* Callback used when the "Add to Bookmarks" menu item is activated */
static void
add_to_shortcuts_cb (GtkMenuItem           *item,
		     GtkFileChooserDefault *impl)
{
  bookmarks_add_selected_folder (impl);
}

/* Callback used when the "Show Hidden Files" menu item is toggled */
static void
show_hidden_toggled_cb (GtkCheckMenuItem      *item,
			GtkFileChooserDefault *impl)
{
  g_object_set (impl,
		"show-hidden", gtk_check_menu_item_get_active (item),
		NULL);
}

/* Callback used when the "Show Size Column" menu item is toggled */
static void
show_size_column_toggled_cb (GtkCheckMenuItem *item,
                             GtkFileChooserDefault *impl)
{
  impl->show_size_column = gtk_check_menu_item_get_active (item);

  gtk_tree_view_column_set_visible (impl->list_size_column,
                                    impl->show_size_column);
}

/* Shows an error dialog about not being able to select a dragged file */
static void
error_selecting_dragged_file_dialog (GtkFileChooserDefault *impl,
				     GFile                 *file,
				     GError                *error)
{
  error_dialog (impl,
		_("Could not select file"),
		file, error);
}

static void
file_list_drag_data_select_uris (GtkFileChooserDefault  *impl,
				 gchar                 **uris)
{
  int i;
  char *uri;
  GtkFileChooser *chooser = GTK_FILE_CHOOSER (impl);

  for (i = 1; uris[i]; i++)
    {
      GFile *file;
      GError *error = NULL;

      uri = uris[i];
      file = g_file_new_for_uri (uri);

      gtk_file_chooser_default_select_file (chooser, file, &error);
      if (error)
	error_selecting_dragged_file_dialog (impl, file, error);

      g_object_unref (file);
    }
}

struct FileListDragData
{
  GtkFileChooserDefault *impl;
  gchar **uris;
  GFile *file;
};

static void
file_list_drag_data_received_get_info_cb (GCancellable *cancellable,
					  GFileInfo    *info,
					  const GError *error,
					  gpointer      user_data)
{
  gboolean cancelled = g_cancellable_is_cancelled (cancellable);
  struct FileListDragData *data = user_data;
  GtkFileChooser *chooser = GTK_FILE_CHOOSER (data->impl);

  if (cancellable != data->impl->file_list_drag_data_received_cancellable)
    goto out;

  data->impl->file_list_drag_data_received_cancellable = NULL;

  if (cancelled || error)
    goto out;

  if ((data->impl->action == GTK_FILE_CHOOSER_ACTION_OPEN ||
       data->impl->action == GTK_FILE_CHOOSER_ACTION_SAVE) &&
      data->uris[1] == 0 && !error && _gtk_file_info_consider_as_directory (info))
    change_folder_and_display_error (data->impl, data->file, FALSE);
  else
    {
      GError *error = NULL;

      gtk_file_chooser_default_unselect_all (chooser);
      gtk_file_chooser_default_select_file (chooser, data->file, &error);
      if (error)
	error_selecting_dragged_file_dialog (data->impl, data->file, error);
      else
	browse_files_center_selected_row (data->impl);
    }

  if (data->impl->select_multiple)
    file_list_drag_data_select_uris (data->impl, data->uris);

out:
  g_object_unref (data->impl);
  g_strfreev (data->uris);
  g_object_unref (data->file);
  g_free (data);

  g_object_unref (cancellable);
}

static void
file_list_drag_data_received_cb (GtkWidget        *widget,
                                 GdkDragContext   *context,
                                 gint              x,
                                 gint              y,
                                 GtkSelectionData *selection_data,
                                 guint             info,
                                 guint             time_,
                                 gpointer          data)
{
  GtkFileChooserDefault *impl;
  gchar **uris;
  char *uri;
  GFile *file;

  impl = GTK_FILE_CHOOSER_DEFAULT (data);

  /* Allow only drags from other widgets; see bug #533891. */
  if (gtk_drag_get_source_widget (context) == widget)
    {
      g_signal_stop_emission_by_name (widget, "drag-data-received");
      return;
    }

  /* Parse the text/uri-list string, navigate to the first one */
  uris = gtk_selection_data_get_uris (selection_data);
  if (uris && uris[0])
    {
      struct FileListDragData *data;

      uri = uris[0];
      file = g_file_new_for_uri (uri);

      data = g_new0 (struct FileListDragData, 1);
      data->impl = g_object_ref (impl);
      data->uris = uris;
      data->file = file;

      if (impl->file_list_drag_data_received_cancellable)
        g_cancellable_cancel (impl->file_list_drag_data_received_cancellable);

      impl->file_list_drag_data_received_cancellable =
        _gtk_file_system_get_info (impl->file_system, file,
                                   "standard::type",
                                   file_list_drag_data_received_get_info_cb,
                                   data);
    }

  g_signal_stop_emission_by_name (widget, "drag-data-received");
}

/* Don't do anything with the drag_drop signal */
static gboolean
file_list_drag_drop_cb (GtkWidget             *widget,
			GdkDragContext        *context,
			gint                   x,
			gint                   y,
			guint                  time_,
			GtkFileChooserDefault *impl)
{
  g_signal_stop_emission_by_name (widget, "drag-drop");
  return TRUE;
}

/* Disable the normal tree drag motion handler, it makes it look like you're
   dropping the dragged item onto a tree item */
static gboolean
file_list_drag_motion_cb (GtkWidget             *widget,
                          GdkDragContext        *context,
                          gint                   x,
                          gint                   y,
                          guint                  time_,
                          GtkFileChooserDefault *impl)
{
  g_signal_stop_emission_by_name (widget, "drag-motion");
  return TRUE;
}

/* Constructs the popup menu for the file list if needed */
static void
file_list_build_popup_menu (GtkFileChooserDefault *impl)
{
  GtkWidget *item;

  if (impl->browse_files_popup_menu)
    return;

  impl->browse_files_popup_menu = gtk_menu_new ();
  gtk_menu_attach_to_widget (GTK_MENU (impl->browse_files_popup_menu),
			     impl->browse_files_tree_view,
			     popup_menu_detach_cb);

  item = gtk_image_menu_item_new_with_mnemonic (_("_Add to Bookmarks"));
  impl->browse_files_popup_menu_add_shortcut_item = item;
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
				 gtk_image_new_from_stock (GTK_STOCK_ADD, GTK_ICON_SIZE_MENU));
  g_signal_connect (item, "activate",
		    G_CALLBACK (add_to_shortcuts_cb), impl);
  gtk_widget_show (item);
  gtk_menu_shell_append (GTK_MENU_SHELL (impl->browse_files_popup_menu), item);

  item = gtk_separator_menu_item_new ();
  gtk_widget_show (item);
  gtk_menu_shell_append (GTK_MENU_SHELL (impl->browse_files_popup_menu), item);

  item = gtk_check_menu_item_new_with_mnemonic (_("Show _Hidden Files"));
  impl->browse_files_popup_menu_hidden_files_item = item;
  g_signal_connect (item, "toggled",
		    G_CALLBACK (show_hidden_toggled_cb), impl);
  gtk_widget_show (item);
  gtk_menu_shell_append (GTK_MENU_SHELL (impl->browse_files_popup_menu), item);

  item = gtk_check_menu_item_new_with_mnemonic (_("Show _Size Column"));
  impl->browse_files_popup_menu_size_column_item = item;
  g_signal_connect (item, "toggled",
                    G_CALLBACK (show_size_column_toggled_cb), impl);
  gtk_widget_show (item);
  gtk_menu_shell_append (GTK_MENU_SHELL (impl->browse_files_popup_menu), item);

  bookmarks_check_add_sensitivity (impl);
}

/* Updates the popup menu for the file list, creating it if necessary */
static void
file_list_update_popup_menu (GtkFileChooserDefault *impl)
{
  file_list_build_popup_menu (impl);

  /* FIXME - handle OPERATION_MODE_SEARCH and OPERATION_MODE_RECENT */

  /* The sensitivity of the Add to Bookmarks item is set in
   * bookmarks_check_add_sensitivity()
   */

  /* 'Show Hidden Files' */
  g_signal_handlers_block_by_func (impl->browse_files_popup_menu_hidden_files_item,
				   G_CALLBACK (show_hidden_toggled_cb), impl);
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (impl->browse_files_popup_menu_hidden_files_item),
				  impl->show_hidden);
  g_signal_handlers_unblock_by_func (impl->browse_files_popup_menu_hidden_files_item,
				     G_CALLBACK (show_hidden_toggled_cb), impl);

  /* 'Show Size Column' */
  g_signal_handlers_block_by_func (impl->browse_files_popup_menu_size_column_item,
				   G_CALLBACK (show_size_column_toggled_cb), impl);
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (impl->browse_files_popup_menu_size_column_item),
				  impl->show_size_column);
  g_signal_handlers_unblock_by_func (impl->browse_files_popup_menu_size_column_item,
				     G_CALLBACK (show_size_column_toggled_cb), impl);
}

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

  g_return_if_fail (gtk_widget_get_realized (widget));

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
file_list_popup_menu (GtkFileChooserDefault *impl,
		      GdkEventButton        *event)
{
  file_list_update_popup_menu (impl);
  if (event)
    gtk_menu_popup (GTK_MENU (impl->browse_files_popup_menu),
		    NULL, NULL, NULL, NULL,
		    event->button, event->time);
  else
    {
      gtk_menu_popup (GTK_MENU (impl->browse_files_popup_menu),
		      NULL, NULL,
		      popup_position_func, impl->browse_files_tree_view,
		      0, GDK_CURRENT_TIME);
      gtk_menu_shell_select_first (GTK_MENU_SHELL (impl->browse_files_popup_menu),
				   FALSE);
    }

}

/* Callback used for the GtkWidget::popup-menu signal of the file list */
static gboolean
list_popup_menu_cb (GtkWidget *widget,
		    GtkFileChooserDefault *impl)
{
  file_list_popup_menu (impl, NULL);
  return TRUE;
}

/* Callback used when a button is pressed on the file list.  We trap button 3 to
 * bring up a popup menu.
 */
static gboolean
list_button_press_event_cb (GtkWidget             *widget,
			    GdkEventButton        *event,
			    GtkFileChooserDefault *impl)
{
  static gboolean in_press = FALSE;

  if (in_press)
    return FALSE;

  if (!_gtk_button_event_triggers_context_menu (event))
    return FALSE;

  in_press = TRUE;
  gtk_widget_event (impl->browse_files_tree_view, (GdkEvent *) event);
  in_press = FALSE;

  file_list_popup_menu (impl, event);
  return TRUE;
}

typedef struct {
  OperationMode operation_mode;
  gint general_column;
  gint model_column;
} ColumnMap;

/* Sets the sort column IDs for the file list; needs to be done whenever we
 * change the model on the treeview.
 */
static void
file_list_set_sort_column_ids (GtkFileChooserDefault *impl)
{
  gtk_tree_view_column_set_sort_column_id (impl->list_name_column, MODEL_COL_NAME);
  gtk_tree_view_column_set_sort_column_id (impl->list_mtime_column, MODEL_COL_MTIME);
  gtk_tree_view_column_set_sort_column_id (impl->list_size_column, MODEL_COL_SIZE);
}

static gboolean
file_list_query_tooltip_cb (GtkWidget  *widget,
                            gint        x,
                            gint        y,
                            gboolean    keyboard_tip,
                            GtkTooltip *tooltip,
                            gpointer    user_data)
{
  GtkFileChooserDefault *impl = user_data;
  GtkTreeModel *model;
  GtkTreePath *path;
  GtkTreeIter iter;
  GFile *file;
  char *parse_name;

  if (impl->operation_mode == OPERATION_MODE_BROWSE)
    return FALSE;


  if (!gtk_tree_view_get_tooltip_context (GTK_TREE_VIEW (impl->browse_files_tree_view),
                                          &x, &y,
                                          keyboard_tip,
                                          &model, &path, &iter))
    return FALSE;
                                       
  gtk_tree_model_get (model, &iter,
                      MODEL_COL_FILE, &file,
                      -1);

  if (file == NULL)
    {
      gtk_tree_path_free (path);
      return FALSE;
    }

  parse_name = g_file_get_parse_name (file);
  gtk_tooltip_set_text (tooltip, parse_name);
  gtk_tree_view_set_tooltip_row (GTK_TREE_VIEW (impl->browse_files_tree_view),
                                 tooltip,
                                 path);

  g_free (parse_name);
  g_object_unref (file);
  gtk_tree_path_free (path);

  return TRUE;
}

static void
set_icon_cell_renderer_fixed_size (GtkFileChooserDefault *impl, GtkCellRenderer *renderer)
{
  gint xpad, ypad;

  gtk_cell_renderer_get_padding (renderer, &xpad, &ypad);
  gtk_cell_renderer_set_fixed_size (renderer, 
                                    xpad * 2 + impl->icon_size,
                                    ypad * 2 + impl->icon_size);
}

/* Creates the widgets for the file list */
static GtkWidget *
create_file_list (GtkFileChooserDefault *impl)
{
  GtkWidget *swin;
  GtkTreeSelection *selection;
  GtkTreeViewColumn *column;
  GtkCellRenderer *renderer;

  /* Scrolled window */
  swin = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swin),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swin),
				       GTK_SHADOW_IN);

  /* Tree/list view */

  impl->browse_files_tree_view = gtk_tree_view_new ();
#ifdef PROFILE_FILE_CHOOSER
  g_object_set_data (G_OBJECT (impl->browse_files_tree_view), "fmq-name", "file_list");
#endif
  g_object_set_data (G_OBJECT (impl->browse_files_tree_view), I_("GtkFileChooserDefault"), impl);
  atk_object_set_name (gtk_widget_get_accessible (impl->browse_files_tree_view), _("Files"));

  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (impl->browse_files_tree_view), TRUE);
  gtk_container_add (GTK_CONTAINER (swin), impl->browse_files_tree_view);

  gtk_drag_dest_set (impl->browse_files_tree_view,
                     GTK_DEST_DEFAULT_ALL,
                     NULL, 0,
                     GDK_ACTION_COPY | GDK_ACTION_MOVE);
  gtk_drag_dest_add_uri_targets (impl->browse_files_tree_view);
  
  g_signal_connect (impl->browse_files_tree_view, "row-activated",
		    G_CALLBACK (list_row_activated), impl);
  g_signal_connect (impl->browse_files_tree_view, "key-press-event",
    		    G_CALLBACK (browse_files_key_press_event_cb), impl);
  g_signal_connect (impl->browse_files_tree_view, "popup-menu",
		    G_CALLBACK (list_popup_menu_cb), impl);
  g_signal_connect (impl->browse_files_tree_view, "button-press-event",
		    G_CALLBACK (list_button_press_event_cb), impl);

  g_signal_connect (impl->browse_files_tree_view, "drag-data-received",
                    G_CALLBACK (file_list_drag_data_received_cb), impl);
  g_signal_connect (impl->browse_files_tree_view, "drag-drop",
                    G_CALLBACK (file_list_drag_drop_cb), impl);
  g_signal_connect (impl->browse_files_tree_view, "drag-motion",
                    G_CALLBACK (file_list_drag_motion_cb), impl);

  g_object_set (impl->browse_files_tree_view, "has-tooltip", TRUE, NULL);
  g_signal_connect (impl->browse_files_tree_view, "query-tooltip",
                    G_CALLBACK (file_list_query_tooltip_cb), impl);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->browse_files_tree_view));
  gtk_tree_selection_set_select_function (selection,
					  list_select_func,
					  impl, NULL);
  gtk_tree_view_enable_model_drag_source (GTK_TREE_VIEW (impl->browse_files_tree_view),
					  GDK_BUTTON1_MASK,
					  NULL, 0,
					  GDK_ACTION_COPY | GDK_ACTION_MOVE);
  gtk_drag_source_add_uri_targets (impl->browse_files_tree_view);

  g_signal_connect (selection, "changed",
		    G_CALLBACK (list_selection_changed), impl);

  /* Keep the column order in sync with update_cell_renderer_attributes() */

  /* Filename column */

  impl->list_name_column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_expand (impl->list_name_column, TRUE);
  gtk_tree_view_column_set_resizable (impl->list_name_column, TRUE);
  gtk_tree_view_column_set_title (impl->list_name_column, _("Name"));

  renderer = gtk_cell_renderer_pixbuf_new ();
  /* We set a fixed size so that we get an empty slot even if no icons are loaded yet */
  set_icon_cell_renderer_fixed_size (impl, renderer);
  gtk_tree_view_column_pack_start (impl->list_name_column, renderer, FALSE);

  impl->list_name_renderer = gtk_cell_renderer_text_new ();
  g_object_set (impl->list_name_renderer,
		"ellipsize", PANGO_ELLIPSIZE_END,
		NULL);
  g_signal_connect (impl->list_name_renderer, "edited",
		    G_CALLBACK (renderer_edited_cb), impl);
  g_signal_connect (impl->list_name_renderer, "editing-canceled",
		    G_CALLBACK (renderer_editing_canceled_cb), impl);
  gtk_tree_view_column_pack_start (impl->list_name_column, impl->list_name_renderer, TRUE);

  gtk_tree_view_append_column (GTK_TREE_VIEW (impl->browse_files_tree_view), impl->list_name_column);

  /* Size column */

  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_resizable (column, TRUE);
  gtk_tree_view_column_set_title (column, _("Size"));

  renderer = gtk_cell_renderer_text_new ();
  g_object_set (renderer, 
                "alignment", PANGO_ALIGN_RIGHT,
                NULL);
  gtk_tree_view_column_pack_start (column, renderer, TRUE); /* bug: it doesn't expand */
  gtk_tree_view_append_column (GTK_TREE_VIEW (impl->browse_files_tree_view), column);
  impl->list_size_column = column;

  /* Modification time column */

  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_resizable (column, TRUE);
  gtk_tree_view_column_set_title (column, _("Modified"));

  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (impl->browse_files_tree_view), column);
  impl->list_mtime_column = column;
  
  file_list_set_sort_column_ids (impl);
  update_cell_renderer_attributes (impl);

  gtk_widget_show_all (swin);

  return swin;
}

/* Creates the widgets for the files/folders pane */
static GtkWidget *
file_pane_create (GtkFileChooserDefault *impl,
		  GtkSizeGroup          *size_group)
{
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *widget;

  vbox = gtk_vbox_new (FALSE, 6);
  gtk_widget_show (vbox);

  /* Box for lists and preview */

  hbox = gtk_hbox_new (FALSE, PREVIEW_HBOX_SPACING);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
  gtk_widget_show (hbox);

  /* File list */

  widget = create_file_list (impl);
  gtk_box_pack_start (GTK_BOX (hbox), widget, TRUE, TRUE, 0);
  gtk_size_group_add_widget (size_group, widget);

  /* Preview */

  impl->preview_box = gtk_vbox_new (FALSE, 12);
  gtk_box_pack_start (GTK_BOX (hbox), impl->preview_box, FALSE, FALSE, 0);
  /* Don't show preview box initially */

  /* Filter combo */

  impl->filter_combo_hbox = gtk_hbox_new (FALSE, 12);

  widget = filter_create (impl);

  gtk_widget_show (widget);
  gtk_box_pack_end (GTK_BOX (impl->filter_combo_hbox), widget, FALSE, FALSE, 0);

  gtk_box_pack_end (GTK_BOX (vbox), impl->filter_combo_hbox, FALSE, FALSE, 0);

  return vbox;
}

static void
location_entry_create (GtkFileChooserDefault *impl)
{
  if (!impl->location_entry)
    impl->location_entry = _gtk_file_chooser_entry_new (TRUE);

  _gtk_file_chooser_entry_set_local_only (GTK_FILE_CHOOSER_ENTRY (impl->location_entry), impl->local_only);
  _gtk_file_chooser_entry_set_action (GTK_FILE_CHOOSER_ENTRY (impl->location_entry), impl->action);
  gtk_entry_set_width_chars (GTK_ENTRY (impl->location_entry), 45);
  gtk_entry_set_activates_default (GTK_ENTRY (impl->location_entry), TRUE);
}

/* Creates the widgets specific to Save mode */
static void
save_widgets_create (GtkFileChooserDefault *impl)
{
  GtkWidget *vbox;
  GtkWidget *widget;

  if (impl->save_widgets != NULL)
    return;

  location_switch_to_path_bar (impl);

  vbox = gtk_vbox_new (FALSE, 12);

  impl->save_widgets_table = gtk_table_new (2, 2, FALSE);
  gtk_box_pack_start (GTK_BOX (vbox), impl->save_widgets_table, FALSE, FALSE, 0);
  gtk_widget_show (impl->save_widgets_table);
  gtk_table_set_row_spacings (GTK_TABLE (impl->save_widgets_table), 12);
  gtk_table_set_col_spacings (GTK_TABLE (impl->save_widgets_table), 12);

  /* Label */

  widget = gtk_label_new_with_mnemonic (_("_Name:"));
  gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (impl->save_widgets_table), widget,
		    0, 1, 0, 1,
		    GTK_FILL, GTK_FILL,
		    0, 0);
  gtk_widget_show (widget);

  /* Location entry */

  location_entry_create (impl);
  gtk_table_attach (GTK_TABLE (impl->save_widgets_table), impl->location_entry,
		    1, 2, 0, 1,
		    GTK_EXPAND | GTK_FILL, 0,
		    0, 0);
  gtk_widget_show (impl->location_entry);
  gtk_label_set_mnemonic_widget (GTK_LABEL (widget), impl->location_entry);

  /* Folder combo */
  impl->save_folder_label = gtk_label_new (NULL);
  gtk_misc_set_alignment (GTK_MISC (impl->save_folder_label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (impl->save_widgets_table), impl->save_folder_label,
		    0, 1, 1, 2,
		    GTK_FILL, GTK_FILL,
		    0, 0);
  gtk_widget_show (impl->save_folder_label);

  impl->save_widgets = vbox;
  gtk_box_pack_start (GTK_BOX (impl), impl->save_widgets, FALSE, FALSE, 0);
  gtk_box_reorder_child (GTK_BOX (impl), impl->save_widgets, 0);
  gtk_widget_show (impl->save_widgets);
}

/* Destroys the widgets specific to Save mode */
static void
save_widgets_destroy (GtkFileChooserDefault *impl)
{
  if (impl->save_widgets == NULL)
    return;

  gtk_widget_destroy (impl->save_widgets);
  impl->save_widgets = NULL;
  impl->save_widgets_table = NULL;
  impl->location_entry = NULL;
  impl->save_folder_label = NULL;
}

/* Turns on the path bar widget.  Can be called even if we are already in that
 * mode.
 */
static void
location_switch_to_path_bar (GtkFileChooserDefault *impl)
{
  if (impl->location_entry)
    {
      gtk_widget_destroy (impl->location_entry);
      impl->location_entry = NULL;
    }

  gtk_widget_hide (impl->location_entry_box);
}

/* Turns on the location entry.  Can be called even if we are already in that
 * mode.
 */
static void
location_switch_to_filename_entry (GtkFileChooserDefault *impl)
{
  /* when in search or recent files mode, we are not showing the
   * location_entry_box container, so there's no point in switching
   * to it.
   */
  if (impl->operation_mode == OPERATION_MODE_SEARCH ||
      impl->operation_mode == OPERATION_MODE_RECENT)
    return;

  if (impl->location_entry)
    {
      gtk_widget_destroy (impl->location_entry);
      impl->location_entry = NULL;
    }

  /* Box */

  gtk_widget_show (impl->location_entry_box);

  /* Entry */

  location_entry_create (impl);
  gtk_box_pack_start (GTK_BOX (impl->location_entry_box), impl->location_entry, TRUE, TRUE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (impl->location_label), impl->location_entry);

  /* Configure the entry */

  _gtk_file_chooser_entry_set_base_folder (GTK_FILE_CHOOSER_ENTRY (impl->location_entry), impl->current_folder);

  /* Done */

  gtk_widget_show (impl->location_entry);
  gtk_widget_grab_focus (impl->location_entry);
}

/* Sets a new location mode.  set_buttons determines whether the toggle button
 * for the mode will also be changed.
 */
static void
location_mode_set (GtkFileChooserDefault *impl,
		   LocationMode new_mode,
		   gboolean set_button)
{
  if (impl->action == GTK_FILE_CHOOSER_ACTION_OPEN ||
      impl->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER)
    {
      GtkWindow *toplevel;
      GtkWidget *current_focus;
      gboolean button_active;
      gboolean switch_to_file_list;

      switch (new_mode)
	{
	case LOCATION_MODE_PATH_BAR:
	  button_active = FALSE;

	  /* The location_entry will disappear when we switch to path bar mode.  So,
	   * we'll focus the file list in that case, to avoid having a window with
	   * no focused widget.
	   */
	  toplevel = get_toplevel (GTK_WIDGET (impl));
	  switch_to_file_list = FALSE;
	  if (toplevel)
	    {
	      current_focus = gtk_window_get_focus (toplevel);
	      if (!current_focus || current_focus == impl->location_entry)
		switch_to_file_list = TRUE;
	    }

	  location_switch_to_path_bar (impl);

	  if (switch_to_file_list)
	    gtk_widget_grab_focus (impl->browse_files_tree_view);

	  break;

	case LOCATION_MODE_FILENAME_ENTRY:
	  button_active = TRUE;
	  location_switch_to_filename_entry (impl);
	  break;

	default:
	  g_assert_not_reached ();
	  return;
	}

      if (set_button)
	{
	  g_signal_handlers_block_by_func (impl->location_button,
					   G_CALLBACK (location_button_toggled_cb), impl);

	  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (impl->location_button), button_active);

	  g_signal_handlers_unblock_by_func (impl->location_button,
					     G_CALLBACK (location_button_toggled_cb), impl);
	}
    }

  impl->location_mode = new_mode;
}

static void
location_toggle_popup_handler (GtkFileChooserDefault *impl)
{
  /* when in search or recent files mode, we are not showing the
   * location_entry_box container, so there's no point in switching
   * to it.
   */
  if (impl->operation_mode == OPERATION_MODE_SEARCH ||
      impl->operation_mode == OPERATION_MODE_RECENT)
    return;

  /* If the file entry is not visible, show it.
   * If it is visible, turn it off only if it is focused.  Otherwise, switch to the entry.
   */
  if (impl->location_mode == LOCATION_MODE_PATH_BAR)
    {
      location_mode_set (impl, LOCATION_MODE_FILENAME_ENTRY, TRUE);
    }
  else if (impl->location_mode == LOCATION_MODE_FILENAME_ENTRY)
    {
      if (gtk_widget_has_focus (impl->location_entry))
        {
          location_mode_set (impl, LOCATION_MODE_PATH_BAR, TRUE);
        }
      else
        {
          gtk_widget_grab_focus (impl->location_entry);
        }
    }
}

/* Callback used when one of the location mode buttons is toggled */
static void
location_button_toggled_cb (GtkToggleButton *toggle,
			    GtkFileChooserDefault *impl)
{
  gboolean is_active;
  LocationMode new_mode;

  is_active = gtk_toggle_button_get_active (toggle);

  if (is_active)
    {
      g_assert (impl->location_mode == LOCATION_MODE_PATH_BAR);
      new_mode = LOCATION_MODE_FILENAME_ENTRY;
    }
  else
    {
      g_assert (impl->location_mode == LOCATION_MODE_FILENAME_ENTRY);
      new_mode = LOCATION_MODE_PATH_BAR;
    }

  location_mode_set (impl, new_mode, FALSE);
}

/* Creates a toggle button for the location entry. */
static void
location_button_create (GtkFileChooserDefault *impl)
{
  GtkWidget *image;
  const char *str;

  image = gtk_image_new_from_stock (GTK_STOCK_EDIT, GTK_ICON_SIZE_BUTTON);
  gtk_widget_show (image);

  impl->location_button = g_object_new (GTK_TYPE_TOGGLE_BUTTON,
					"image", image,
					NULL);

  g_signal_connect (impl->location_button, "toggled",
		    G_CALLBACK (location_button_toggled_cb), impl);

  str = _("Type a file name");

  gtk_widget_set_tooltip_text (impl->location_button, str);
  atk_object_set_name (gtk_widget_get_accessible (impl->location_button), str);
}

typedef enum {
  PATH_BAR_FOLDER_PATH,
  PATH_BAR_SELECT_A_FOLDER,
  PATH_BAR_ERROR_NO_FILENAME,
  PATH_BAR_ERROR_NO_FOLDER,
  PATH_BAR_RECENTLY_USED,
  PATH_BAR_SEARCH
} PathBarMode;

/* Creates the info bar for informational messages or warnings, with its icon and label */
static void
info_bar_create (GtkFileChooserDefault *impl)
{
  GtkWidget *content_area;

  impl->browse_select_a_folder_info_bar = gtk_info_bar_new ();
  impl->browse_select_a_folder_icon = gtk_image_new_from_stock (GTK_STOCK_DIRECTORY, GTK_ICON_SIZE_MENU);
  impl->browse_select_a_folder_label = gtk_label_new (NULL);

  content_area = gtk_info_bar_get_content_area (GTK_INFO_BAR (impl->browse_select_a_folder_info_bar));

  gtk_box_pack_start (GTK_BOX (content_area), impl->browse_select_a_folder_icon, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (content_area), impl->browse_select_a_folder_label, FALSE, FALSE, 0);

  gtk_widget_show (impl->browse_select_a_folder_icon);
  gtk_widget_show (impl->browse_select_a_folder_label);
}

/* Sets the info bar to show the appropriate informational or warning message */
static void
info_bar_set (GtkFileChooserDefault *impl, PathBarMode mode)
{
  char *str;
  gboolean free_str;
  GtkMessageType message_type;

  free_str = FALSE;

  switch (mode)
    {
    case PATH_BAR_SELECT_A_FOLDER:
      str = g_strconcat ("<i>", _("Please select a folder below"), "</i>", NULL);
      free_str = TRUE;
      message_type = GTK_MESSAGE_OTHER;
      break;

    case PATH_BAR_ERROR_NO_FILENAME:
      str = _("Please type a file name");
      message_type = GTK_MESSAGE_WARNING;
      break;

    case PATH_BAR_ERROR_NO_FOLDER:
      str = _("Please select a folder below");
      message_type = GTK_MESSAGE_WARNING;
      break;

    default:
      g_assert_not_reached ();
      return;
    }

  gtk_info_bar_set_message_type (GTK_INFO_BAR (impl->browse_select_a_folder_info_bar), message_type);
  gtk_image_set_from_stock (GTK_IMAGE (impl->browse_select_a_folder_icon),
			    (message_type == GTK_MESSAGE_WARNING) ? GTK_STOCK_DIALOG_WARNING : GTK_STOCK_DIRECTORY,
			    GTK_ICON_SIZE_MENU);
  gtk_label_set_markup (GTK_LABEL (impl->browse_select_a_folder_label), str);

  if (free_str)
    g_free (str);
}

/* Creates the icon and label used to show that the file chooser is in Search or Recently-used mode */
static void
special_mode_widgets_create (GtkFileChooserDefault *impl)
{
  impl->browse_special_mode_icon = gtk_image_new ();
  gtk_size_group_add_widget (impl->browse_path_bar_size_group, impl->browse_special_mode_icon);
  gtk_box_pack_start (GTK_BOX (impl->browse_path_bar_hbox), impl->browse_special_mode_icon, FALSE, FALSE, 0);

  impl->browse_special_mode_label = gtk_label_new (NULL);
  gtk_size_group_add_widget (impl->browse_path_bar_size_group, impl->browse_special_mode_label);
  gtk_box_pack_start (GTK_BOX (impl->browse_path_bar_hbox), impl->browse_special_mode_label, FALSE, FALSE, 0);
}

/* Creates the path bar's container and eveyrthing that goes in it: location button, pathbar, info bar, and Create Folder button */
static void
path_bar_widgets_create (GtkFileChooserDefault *impl)
{
  /* Location widgets - note browse_path_bar_hbox is packed in the right place until switch_path_bar() */
  impl->browse_path_bar_hbox = gtk_hbox_new (FALSE, 12);
  gtk_widget_show (impl->browse_path_bar_hbox);

  /* Size group that allows the path bar to be the same size between modes */
  impl->browse_path_bar_size_group = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);
  gtk_size_group_set_ignore_hidden (impl->browse_path_bar_size_group, FALSE);

  /* Location button */
  location_button_create (impl);
  gtk_size_group_add_widget (impl->browse_path_bar_size_group, impl->location_button);
  gtk_box_pack_start (GTK_BOX (impl->browse_path_bar_hbox), impl->location_button, FALSE, FALSE, 0);

  /* Path bar */
  impl->browse_path_bar = g_object_new (GTK_TYPE_PATH_BAR, NULL);
  _gtk_path_bar_set_file_system (GTK_PATH_BAR (impl->browse_path_bar), impl->file_system);
  g_signal_connect (impl->browse_path_bar, "path-clicked", G_CALLBACK (path_bar_clicked), impl);

  gtk_size_group_add_widget (impl->browse_path_bar_size_group, impl->browse_path_bar);
  gtk_box_pack_start (GTK_BOX (impl->browse_path_bar_hbox), impl->browse_path_bar, TRUE, TRUE, 0);

  /* Info bar */
  info_bar_create (impl);
  gtk_size_group_add_widget (impl->browse_path_bar_size_group, impl->browse_select_a_folder_info_bar);
  gtk_box_pack_start (GTK_BOX (impl->browse_path_bar_hbox), impl->browse_select_a_folder_info_bar, TRUE, TRUE, 0);

  /* Widgets for special modes (recently-used in Open mode, Search mode) */
  special_mode_widgets_create (impl);

  /* Create Folder */
  impl->browse_new_folder_button = gtk_button_new_with_mnemonic (_("Create Fo_lder"));
  g_signal_connect (impl->browse_new_folder_button, "clicked",
		    G_CALLBACK (new_folder_button_clicked), impl);
  gtk_size_group_add_widget (impl->browse_path_bar_size_group, impl->browse_new_folder_button);
  gtk_box_pack_end (GTK_BOX (impl->browse_path_bar_hbox), impl->browse_new_folder_button, FALSE, FALSE, 0);
}

/* Sets the path bar's mode to show a label, the actual folder path, or a
 * warning message.  You may call this function with PATH_BAR_ERROR_* directly
 * if the pathbar is already showing the widgets you expect; otherwise, call
 * path_bar_update() instead to set the appropriate widgets automatically.
 */
static void
path_bar_set_mode (GtkFileChooserDefault *impl, PathBarMode mode)
{
  gboolean path_bar_visible		= FALSE;
  gboolean special_mode_widgets_visible = FALSE;
  gboolean info_bar_visible		= FALSE;
  gboolean create_folder_visible        = FALSE;

  char *tmp;

  switch (mode)
    {
    case PATH_BAR_FOLDER_PATH:
      path_bar_visible = TRUE;
      break;

    case PATH_BAR_SELECT_A_FOLDER:
    case PATH_BAR_ERROR_NO_FILENAME:
    case PATH_BAR_ERROR_NO_FOLDER:
      info_bar_set (impl, mode);
      info_bar_visible = TRUE;
      break;

    case PATH_BAR_RECENTLY_USED:
      gtk_image_set_from_icon_name (GTK_IMAGE (impl->browse_special_mode_icon), "document-open-recent", GTK_ICON_SIZE_BUTTON);

      tmp = g_strdup_printf ("<b>%s</b>", _("Recently Used"));
      gtk_label_set_markup (GTK_LABEL (impl->browse_special_mode_label), tmp);
      g_free (tmp);

      special_mode_widgets_visible = TRUE;
      break;

    case PATH_BAR_SEARCH:
      gtk_image_set_from_stock (GTK_IMAGE (impl->browse_special_mode_icon), GTK_STOCK_FIND, GTK_ICON_SIZE_BUTTON);

      tmp = g_strdup_printf ("<b>%s</b>", _("Search:"));
      gtk_label_set_markup (GTK_LABEL (impl->browse_special_mode_label), tmp);
      g_free (tmp);

      special_mode_widgets_visible = TRUE;
      break;

    default:
      g_assert_not_reached ();
    }

  gtk_widget_set_visible (impl->browse_path_bar,			path_bar_visible);
  gtk_widget_set_visible (impl->browse_special_mode_icon,		special_mode_widgets_visible);
  gtk_widget_set_visible (impl->browse_special_mode_label,		special_mode_widgets_visible);
  gtk_widget_set_visible (impl->browse_select_a_folder_info_bar,	info_bar_visible);

  if (path_bar_visible)
    {
      if (impl->create_folders
	  && impl->action != GTK_FILE_CHOOSER_ACTION_OPEN
	  && impl->operation_mode != OPERATION_MODE_RECENT)
	create_folder_visible = TRUE;
    }

  gtk_widget_set_visible (impl->browse_new_folder_button,		create_folder_visible);
}

/* Creates the main hpaned with the widgets shared by Open and Save mode */
static void
browse_widgets_create (GtkFileChooserDefault *impl)
{
  GtkWidget *hpaned;
  GtkWidget *widget;
  GtkSizeGroup *size_group;

  impl->browse_widgets_box = gtk_vbox_new (FALSE, 12);
  gtk_box_pack_start (GTK_BOX (impl), impl->browse_widgets_box, TRUE, TRUE, 0);
  gtk_widget_show (impl->browse_widgets_box);

  impl->browse_header_box = gtk_vbox_new (FALSE, 12);
  gtk_box_pack_start (GTK_BOX (impl->browse_widgets_box), impl->browse_header_box, FALSE, FALSE, 0);
  gtk_widget_show (impl->browse_header_box);

  /* Path bar, info bar, and their respective machinery - the browse_path_bar_hbox will get packed elsewhere */
  path_bar_widgets_create (impl);

  /* Box for the location label and entry */

  impl->location_entry_box = gtk_hbox_new (FALSE, 12);
  gtk_box_pack_start (GTK_BOX (impl->browse_header_box), impl->location_entry_box, FALSE, FALSE, 0);

  impl->location_label = gtk_label_new_with_mnemonic (_("_Location:"));
  gtk_widget_show (impl->location_label);
  gtk_box_pack_start (GTK_BOX (impl->location_entry_box), impl->location_label, FALSE, FALSE, 0);

  /* size group is used by the scrolled windows of the panes */
  size_group = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);

  /* Paned widget */
  hpaned = gtk_hpaned_new ();
  gtk_widget_show (hpaned);
  gtk_box_pack_start (GTK_BOX (impl->browse_widgets_box), hpaned, TRUE, TRUE, 0);

  widget = shortcuts_pane_create (impl, size_group);
  gtk_paned_pack1 (GTK_PANED (hpaned), widget, FALSE, FALSE);
  widget = file_pane_create (impl, size_group);
  gtk_paned_pack2 (GTK_PANED (hpaned), widget, TRUE, FALSE);
  gtk_paned_set_position (GTK_PANED (hpaned), 148);
  g_object_unref (size_group);
}

static GObject*
gtk_file_chooser_default_constructor (GType                  type,
				      guint                  n_construct_properties,
				      GObjectConstructParam *construct_params)
{
  GtkFileChooserDefault *impl;
  GObject *object;

  profile_start ("start", NULL);

  object = G_OBJECT_CLASS (_gtk_file_chooser_default_parent_class)->constructor (type,
										n_construct_properties,
										construct_params);
  impl = GTK_FILE_CHOOSER_DEFAULT (object);

  g_assert (impl->file_system);

  gtk_widget_push_composite_child ();

  /* Shortcuts model */
  shortcuts_model_create (impl);

  /* The browse widgets */
  browse_widgets_create (impl);

  /* Alignment to hold extra widget */
  impl->extra_align = gtk_alignment_new (0.0, 0.5, 1.0, 1.0);
  gtk_box_pack_start (GTK_BOX (impl), impl->extra_align, FALSE, FALSE, 0);

  gtk_widget_pop_composite_child ();
  update_appearance (impl);

  profile_end ("end", NULL);

  return object;
}

/* Sets the extra_widget by packing it in the appropriate place */
static void
set_extra_widget (GtkFileChooserDefault *impl,
		  GtkWidget             *extra_widget)
{
  if (extra_widget)
    {
      g_object_ref (extra_widget);
      /* FIXME: is this right ? */
      gtk_widget_show (extra_widget);
    }

  if (impl->extra_widget)
    {
      gtk_container_remove (GTK_CONTAINER (impl->extra_align), impl->extra_widget);
      g_object_unref (impl->extra_widget);
    }

  impl->extra_widget = extra_widget;
  if (impl->extra_widget)
    {
      gtk_container_add (GTK_CONTAINER (impl->extra_align), impl->extra_widget);
      gtk_widget_show (impl->extra_align);
    }
  else
    gtk_widget_hide (impl->extra_align);
}

static void
set_local_only (GtkFileChooserDefault *impl,
		gboolean               local_only)
{
  if (local_only != impl->local_only)
    {
      impl->local_only = local_only;

      if (impl->location_entry)
	_gtk_file_chooser_entry_set_local_only (GTK_FILE_CHOOSER_ENTRY (impl->location_entry), local_only);

      if (impl->shortcuts_model && impl->file_system)
	{
	  shortcuts_add_volumes (impl);
	  shortcuts_add_bookmarks (impl);
	}

      if (local_only && impl->current_folder &&
           !_gtk_file_has_native_path (impl->current_folder))
	{
	  /* If we are pointing to a non-local folder, make an effort to change
	   * back to a local folder, but it's really up to the app to not cause
	   * such a situation, so we ignore errors.
	   */
	  const gchar *home = g_get_home_dir ();
	  GFile *home_file;

	  if (home == NULL)
	    return;

	  home_file = g_file_new_for_path (home);

	  gtk_file_chooser_set_current_folder_file (GTK_FILE_CHOOSER (impl), home_file, NULL);

	  g_object_unref (home_file);
	}
    }
}

static void
volumes_bookmarks_changed_cb (GtkFileSystem         *file_system,
			      GtkFileChooserDefault *impl)
{
  shortcuts_add_volumes (impl);
  shortcuts_add_bookmarks (impl);

  bookmarks_check_add_sensitivity (impl);
  bookmarks_check_remove_sensitivity (impl);
  shortcuts_check_popup_sensitivity (impl);
}

/* Sets the file chooser to multiple selection mode */
static void
set_select_multiple (GtkFileChooserDefault *impl,
		     gboolean               select_multiple,
		     gboolean               property_notify)
{
  GtkTreeSelection *selection;
  GtkSelectionMode mode;

  if (select_multiple == impl->select_multiple)
    return;

  mode = select_multiple ? GTK_SELECTION_MULTIPLE : GTK_SELECTION_BROWSE;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->browse_files_tree_view));
  gtk_tree_selection_set_mode (selection, mode);

  gtk_tree_view_set_rubber_banding (GTK_TREE_VIEW (impl->browse_files_tree_view), select_multiple);

  impl->select_multiple = select_multiple;
  g_object_notify (G_OBJECT (impl), "select-multiple");

  check_preview_change (impl);
}

static void
set_file_system_backend (GtkFileChooserDefault *impl)
{
  profile_start ("start for backend", "default");

  impl->file_system = _gtk_file_system_new ();

  g_signal_connect (impl->file_system, "volumes-changed",
		    G_CALLBACK (volumes_bookmarks_changed_cb), impl);
  g_signal_connect (impl->file_system, "bookmarks-changed",
		    G_CALLBACK (volumes_bookmarks_changed_cb), impl);

  profile_end ("end", NULL);
}

static void
unset_file_system_backend (GtkFileChooserDefault *impl)
{
  g_signal_handlers_disconnect_by_func (impl->file_system,
					G_CALLBACK (volumes_bookmarks_changed_cb), impl);

  g_object_unref (impl->file_system);

  impl->file_system = NULL;
}

/* Saves the widgets around the pathbar so they can be reparented later
 * in the correct place.  This function must be called paired with
 * restore_path_bar().
 */
static void
save_path_bar (GtkFileChooserDefault *impl)
{
  GtkWidget *parent;

  g_object_ref (impl->browse_path_bar_hbox);

  parent = gtk_widget_get_parent (impl->browse_path_bar_hbox);
  if (parent)
    gtk_container_remove (GTK_CONTAINER (parent), impl->browse_path_bar_hbox);
}

/* Reparents the path bar and the "Create folder" button to the right place:
 * Above the file list in Open mode, or to the right of the "Save in folder:"
 * label in Save mode.  The save_path_bar() function must be called before this
 * one.
 */
static void
restore_path_bar (GtkFileChooserDefault *impl)
{
  if (impl->action == GTK_FILE_CHOOSER_ACTION_OPEN
      || impl->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER)
    {
      gtk_box_pack_start (GTK_BOX (impl->browse_header_box), impl->browse_path_bar_hbox, FALSE, FALSE, 0);
      gtk_box_reorder_child (GTK_BOX (impl->browse_header_box), impl->browse_path_bar_hbox, 0);
    }
  else if (impl->action == GTK_FILE_CHOOSER_ACTION_SAVE
	   || impl->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER)
    {
      gtk_table_attach (GTK_TABLE (impl->save_widgets_table), impl->browse_path_bar_hbox,
			1, 2, 1, 2,
			GTK_EXPAND | GTK_FILL, GTK_FILL,
			0, 0);
      gtk_label_set_mnemonic_widget (GTK_LABEL (impl->save_folder_label), impl->browse_path_bar);
    }
  else
    g_assert_not_reached ();

  g_object_unref (impl->browse_path_bar_hbox);
}

/* Takes the folder stored in a row in the recent_model, and puts it in the pathbar */
static void
put_recent_folder_in_pathbar (GtkFileChooserDefault *impl, GtkTreeIter *iter)
{
  GFile *file;

  gtk_tree_model_get (GTK_TREE_MODEL (impl->recent_model), iter,
		      MODEL_COL_FILE, &file,
		      -1);
  _gtk_path_bar_set_file (GTK_PATH_BAR (impl->browse_path_bar), file, FALSE, NULL); /* NULL-GError */
  g_object_unref (file);
}

/* Sets the pathbar in the appropriate mode according to the current operation mode and action.  This is the central function for
 * dealing with the pathbar's widgets; as long as impl->action and impl->operation_mode are set correctly, then calling this
 * function will update all the pathbar's widgets.
 */
static void
path_bar_update (GtkFileChooserDefault *impl)
{
  PathBarMode mode;

  switch (impl->operation_mode)
    {
    case OPERATION_MODE_BROWSE:
      mode = PATH_BAR_FOLDER_PATH;
      break;

    case OPERATION_MODE_RECENT:
      if (impl->action == GTK_FILE_CHOOSER_ACTION_SAVE)
	{
	  GtkTreeSelection *selection;
	  gboolean have_selected;
	  GtkTreeIter iter;

	  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->browse_files_tree_view));

	  /* Save mode means single-selection mode, so the following is valid */
	  have_selected = gtk_tree_selection_get_selected (selection, NULL, &iter);

	  if (have_selected)
	    {
	      mode = PATH_BAR_FOLDER_PATH;
	      put_recent_folder_in_pathbar (impl, &iter);
	    }
	  else
	    mode = PATH_BAR_SELECT_A_FOLDER;
	}
      else
	mode = PATH_BAR_RECENTLY_USED;

      break;

    case OPERATION_MODE_SEARCH:
      mode = PATH_BAR_SEARCH;
      break;

    default:
      g_assert_not_reached ();
      return;
    }

  path_bar_set_mode (impl, mode);
}

static void
operation_mode_discard_search_widgets (GtkFileChooserDefault *impl)
{
  if (impl->search_hbox)
    {
      gtk_widget_destroy (impl->search_hbox);

      impl->search_hbox = NULL;
      impl->search_entry = NULL;
    }
}

/* Stops running operations like populating the browse model, searches, and the recent-files model */
static void
operation_mode_stop (GtkFileChooserDefault *impl, OperationMode mode)
{
  switch (mode)
    {
    case OPERATION_MODE_BROWSE:
      stop_loading_and_clear_list_model (impl, TRUE);
      break;

    case OPERATION_MODE_SEARCH:
      search_stop_searching (impl, FALSE);
      search_clear_model (impl, TRUE);

      operation_mode_discard_search_widgets (impl);
      break;

    case OPERATION_MODE_RECENT:
      recent_stop_loading (impl);
      recent_clear_model (impl, TRUE);
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
operation_mode_set_browse (GtkFileChooserDefault *impl)
{
  path_bar_update (impl);

  if (impl->action == GTK_FILE_CHOOSER_ACTION_OPEN ||
      impl->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER)
    {
      gtk_widget_show (impl->location_button);
      location_mode_set (impl, impl->location_mode, TRUE);

      if (impl->location_mode == LOCATION_MODE_FILENAME_ENTRY)
	gtk_widget_show (impl->location_entry_box);
    }
}

static void
operation_mode_set_search (GtkFileChooserDefault *impl)
{
  g_assert (impl->search_hbox == NULL);
  g_assert (impl->search_entry == NULL);
  g_assert (impl->search_model == NULL);

  search_setup_widgets (impl);
}

static void
operation_mode_set_recent (GtkFileChooserDefault *impl)
{
  path_bar_update (impl);

  /* Hide the location widgets temporarily */
  if (impl->action == GTK_FILE_CHOOSER_ACTION_OPEN ||
      impl->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER)
    {
      gtk_widget_hide (impl->location_button);
      gtk_widget_hide (impl->location_entry_box);
    }

  recent_start_loading (impl);
}

/* Sometimes we need to frob the selection in the shortcuts list manually */
static void
shortcuts_select_item_without_activating (GtkFileChooserDefault *impl, int pos)
{
  GtkTreeSelection *selection;
  GtkTreePath *path;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->browse_shortcuts_tree_view));

  g_signal_handlers_block_by_func (selection, G_CALLBACK (shortcuts_selection_changed_cb), impl);

  path = gtk_tree_path_new_from_indices (pos, -1);
  gtk_tree_selection_select_path (selection, path);
  gtk_tree_path_free (path);

  g_signal_handlers_unblock_by_func (selection, G_CALLBACK (shortcuts_selection_changed_cb), impl);
}

static void
operation_mode_set (GtkFileChooserDefault *impl, OperationMode mode)
{
  ShortcutsIndex shortcut_to_select;

  operation_mode_stop (impl, impl->operation_mode);

  impl->operation_mode = mode;

  switch (impl->operation_mode)
    {
    case OPERATION_MODE_BROWSE:
      operation_mode_set_browse (impl);
      shortcut_to_select = SHORTCUTS_CURRENT_FOLDER;
      break;

    case OPERATION_MODE_SEARCH:
      operation_mode_set_search (impl);
      shortcut_to_select = SHORTCUTS_SEARCH;
      break;

    case OPERATION_MODE_RECENT:
      operation_mode_set_recent (impl);
      shortcut_to_select = SHORTCUTS_RECENT;
      break;

    default:
      g_assert_not_reached ();
      return;
    }

  if (shortcut_to_select != SHORTCUTS_CURRENT_FOLDER)
    shortcuts_select_item_without_activating (impl, shortcuts_get_index (impl, shortcut_to_select));
}

/* This function is basically a do_all function.
 *
 * It sets the visibility on all the widgets based on the current state, and
 * moves the custom_widget if needed.
 */
static void
update_appearance (GtkFileChooserDefault *impl)
{
  save_path_bar (impl);

  if (impl->action == GTK_FILE_CHOOSER_ACTION_SAVE ||
      impl->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER)
    {
      const char *text;

      gtk_widget_hide (impl->location_button);
      save_widgets_create (impl);

      if (impl->action == GTK_FILE_CHOOSER_ACTION_SAVE)
	text = _("Save in _folder:");
      else
	text = _("Create in _folder:");

      gtk_label_set_text_with_mnemonic (GTK_LABEL (impl->save_folder_label), text);

      if (impl->select_multiple)
	{
	  g_warning ("Save mode cannot be set in conjunction with multiple selection mode.  "
		     "Re-setting to single selection mode.");
	  set_select_multiple (impl, FALSE, TRUE);
	}
    }
  else if (impl->action == GTK_FILE_CHOOSER_ACTION_OPEN ||
	   impl->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER)
    {
      gtk_widget_show (impl->location_button);
      save_widgets_destroy (impl);
      location_mode_set (impl, impl->location_mode, TRUE);
    }

  if (impl->location_entry)
    _gtk_file_chooser_entry_set_action (GTK_FILE_CHOOSER_ENTRY (impl->location_entry), impl->action);

  restore_path_bar (impl);
  path_bar_update (impl);

  /* This *is* needed; we need to redraw the file list because the "sensitivity"
   * of files may change depending whether we are in a file or folder-only mode.
   */
  gtk_widget_queue_draw (impl->browse_files_tree_view);

  emit_default_size_changed (impl);
}

static void
gtk_file_chooser_default_set_property (GObject      *object,
				       guint         prop_id,
				       const GValue *value,
				       GParamSpec   *pspec)

{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (object);

  switch (prop_id)
    {
    case GTK_FILE_CHOOSER_PROP_ACTION:
      {
	GtkFileChooserAction action = g_value_get_enum (value);

	if (action != impl->action)
	  {
	    gtk_file_chooser_default_unselect_all (GTK_FILE_CHOOSER (impl));
	    
	    if ((action == GTK_FILE_CHOOSER_ACTION_SAVE ||
                 action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER)
		&& impl->select_multiple)
	      {
		g_warning ("Tried to change the file chooser action to SAVE or CREATE_FOLDER, but "
			   "this is not allowed in multiple selection mode.  Resetting the file chooser "
			   "to single selection mode.");
		set_select_multiple (impl, FALSE, TRUE);
	      }
	    impl->action = action;
            update_cell_renderer_attributes (impl);
	    update_appearance (impl);
	    settings_load (impl);
	  }
      }
      break;

    case GTK_FILE_CHOOSER_PROP_FILE_SYSTEM_BACKEND:
      /* Ignore property */
      break;

    case GTK_FILE_CHOOSER_PROP_FILTER:
      set_current_filter (impl, g_value_get_object (value));
      break;

    case GTK_FILE_CHOOSER_PROP_LOCAL_ONLY:
      set_local_only (impl, g_value_get_boolean (value));
      break;

    case GTK_FILE_CHOOSER_PROP_PREVIEW_WIDGET:
      set_preview_widget (impl, g_value_get_object (value));
      break;

    case GTK_FILE_CHOOSER_PROP_PREVIEW_WIDGET_ACTIVE:
      impl->preview_widget_active = g_value_get_boolean (value);
      update_preview_widget_visibility (impl);
      break;

    case GTK_FILE_CHOOSER_PROP_USE_PREVIEW_LABEL:
      impl->use_preview_label = g_value_get_boolean (value);
      update_preview_widget_visibility (impl);
      break;

    case GTK_FILE_CHOOSER_PROP_EXTRA_WIDGET:
      set_extra_widget (impl, g_value_get_object (value));
      break;

    case GTK_FILE_CHOOSER_PROP_SELECT_MULTIPLE:
      {
	gboolean select_multiple = g_value_get_boolean (value);
	if ((impl->action == GTK_FILE_CHOOSER_ACTION_SAVE ||
             impl->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER)
	    && select_multiple)
	  {
	    g_warning ("Tried to set the file chooser to multiple selection mode, but this is "
		       "not allowed in SAVE or CREATE_FOLDER modes.  Ignoring the change and "
		       "leaving the file chooser in single selection mode.");
	    return;
	  }

	set_select_multiple (impl, select_multiple, FALSE);
      }
      break;

    case GTK_FILE_CHOOSER_PROP_SHOW_HIDDEN:
      {
	gboolean show_hidden = g_value_get_boolean (value);
	if (show_hidden != impl->show_hidden)
	  {
	    impl->show_hidden = show_hidden;

	    if (impl->browse_files_model)
	      _gtk_file_system_model_set_show_hidden (impl->browse_files_model, show_hidden);
	  }
      }
      break;

    case GTK_FILE_CHOOSER_PROP_DO_OVERWRITE_CONFIRMATION:
      {
	gboolean do_overwrite_confirmation = g_value_get_boolean (value);
	impl->do_overwrite_confirmation = do_overwrite_confirmation;
      }
      break;

    case GTK_FILE_CHOOSER_PROP_CREATE_FOLDERS:
      {
        gboolean create_folders = g_value_get_boolean (value);
        impl->create_folders = create_folders;
        update_appearance (impl);
      }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_file_chooser_default_get_property (GObject    *object,
				       guint       prop_id,
				       GValue     *value,
				       GParamSpec *pspec)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (object);

  switch (prop_id)
    {
    case GTK_FILE_CHOOSER_PROP_ACTION:
      g_value_set_enum (value, impl->action);
      break;

    case GTK_FILE_CHOOSER_PROP_FILTER:
      g_value_set_object (value, impl->current_filter);
      break;

    case GTK_FILE_CHOOSER_PROP_LOCAL_ONLY:
      g_value_set_boolean (value, impl->local_only);
      break;

    case GTK_FILE_CHOOSER_PROP_PREVIEW_WIDGET:
      g_value_set_object (value, impl->preview_widget);
      break;

    case GTK_FILE_CHOOSER_PROP_PREVIEW_WIDGET_ACTIVE:
      g_value_set_boolean (value, impl->preview_widget_active);
      break;

    case GTK_FILE_CHOOSER_PROP_USE_PREVIEW_LABEL:
      g_value_set_boolean (value, impl->use_preview_label);
      break;

    case GTK_FILE_CHOOSER_PROP_EXTRA_WIDGET:
      g_value_set_object (value, impl->extra_widget);
      break;

    case GTK_FILE_CHOOSER_PROP_SELECT_MULTIPLE:
      g_value_set_boolean (value, impl->select_multiple);
      break;

    case GTK_FILE_CHOOSER_PROP_SHOW_HIDDEN:
      g_value_set_boolean (value, impl->show_hidden);
      break;

    case GTK_FILE_CHOOSER_PROP_DO_OVERWRITE_CONFIRMATION:
      g_value_set_boolean (value, impl->do_overwrite_confirmation);
      break;

    case GTK_FILE_CHOOSER_PROP_CREATE_FOLDERS:
      g_value_set_boolean (value, impl->create_folders);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/* This cancels everything that may be going on in the background. */
static void
cancel_all_operations (GtkFileChooserDefault *impl)
{
  GSList *l;

  pending_select_files_free (impl);

  if (impl->reload_icon_cancellables)
    {
      for (l = impl->reload_icon_cancellables; l; l = l->next)
        {
	  GCancellable *cancellable = G_CANCELLABLE (l->data);
	  g_cancellable_cancel (cancellable);
        }
      g_slist_free (impl->reload_icon_cancellables);
      impl->reload_icon_cancellables = NULL;
    }

  if (impl->loading_shortcuts)
    {
      for (l = impl->loading_shortcuts; l; l = l->next)
        {
	  GCancellable *cancellable = G_CANCELLABLE (l->data);
	  g_cancellable_cancel (cancellable);
        }
      g_slist_free (impl->loading_shortcuts);
      impl->loading_shortcuts = NULL;
    }

  if (impl->file_list_drag_data_received_cancellable)
    {
      g_cancellable_cancel (impl->file_list_drag_data_received_cancellable);
      impl->file_list_drag_data_received_cancellable = NULL;
    }

  if (impl->update_current_folder_cancellable)
    {
      g_cancellable_cancel (impl->update_current_folder_cancellable);
      impl->update_current_folder_cancellable = NULL;
    }

  if (impl->should_respond_get_info_cancellable)
    {
      g_cancellable_cancel (impl->should_respond_get_info_cancellable);
      impl->should_respond_get_info_cancellable = NULL;
    }

  if (impl->file_exists_get_info_cancellable)
    {
      g_cancellable_cancel (impl->file_exists_get_info_cancellable);
      impl->file_exists_get_info_cancellable = NULL;
    }

  if (impl->update_from_entry_cancellable)
    {
      g_cancellable_cancel (impl->update_from_entry_cancellable);
      impl->update_from_entry_cancellable = NULL;
    }

  if (impl->shortcuts_activate_iter_cancellable)
    {
      g_cancellable_cancel (impl->shortcuts_activate_iter_cancellable);
      impl->shortcuts_activate_iter_cancellable = NULL;
    }

  search_stop_searching (impl, TRUE);
  recent_stop_loading (impl);
}

/* Removes the settings signal handler.  It's safe to call multiple times */
static void
remove_settings_signal (GtkFileChooserDefault *impl,
			GdkScreen             *screen)
{
  if (impl->settings_signal_id)
    {
      GtkSettings *settings;

      settings = gtk_settings_get_for_screen (screen);
      g_signal_handler_disconnect (settings,
				   impl->settings_signal_id);
      impl->settings_signal_id = 0;
    }
}

static void
gtk_file_chooser_default_dispose (GObject *object)
{
  GtkFileChooserDefault *impl = (GtkFileChooserDefault *) object;

  cancel_all_operations (impl);

  if (impl->extra_widget)
    {
      g_object_unref (impl->extra_widget);
      impl->extra_widget = NULL;
    }

  remove_settings_signal (impl, gtk_widget_get_screen (GTK_WIDGET (impl)));

  G_OBJECT_CLASS (_gtk_file_chooser_default_parent_class)->dispose (object);
}

/* We override show-all since we have internal widgets that
 * shouldn't be shown when you call show_all(), like the filter
 * combo box.
 */
static void
gtk_file_chooser_default_show_all (GtkWidget *widget)
{
  GtkFileChooserDefault *impl = (GtkFileChooserDefault *) widget;

  gtk_widget_show (widget);

  if (impl->extra_widget)
    gtk_widget_show_all (impl->extra_widget);
}

/* Handler for GtkWindow::set-focus; this is where we save the last-focused
 * widget on our toplevel.  See gtk_file_chooser_default_hierarchy_changed()
 */
static void
toplevel_set_focus_cb (GtkWindow             *window,
		       GtkWidget             *focus,
		       GtkFileChooserDefault *impl)
{
  impl->toplevel_last_focus_widget = gtk_window_get_focus (window);
}

/* Callback used when the toplevel widget unmaps itself.  We don't do this in
 * our own ::unmap() handler because in GTK+2, child widgets don't get
 * unmapped.
 */
static void
toplevel_unmap_cb (GtkWidget *widget,
		   GtkFileChooserDefault *impl)
{
  settings_save (impl);

  cancel_all_operations (impl);
  impl->reload_state = RELOAD_EMPTY;
}

/* We monitor the focus widget on our toplevel to be able to know which widget
 * was last focused at the time our "should_respond" method gets called.
 */
static void
gtk_file_chooser_default_hierarchy_changed (GtkWidget *widget,
					    GtkWidget *previous_toplevel)
{
  GtkFileChooserDefault *impl;
  GtkWidget *toplevel;

  impl = GTK_FILE_CHOOSER_DEFAULT (widget);
  toplevel = gtk_widget_get_toplevel (widget);

  if (previous_toplevel)
    {
      if (impl->toplevel_set_focus_id != 0)
	{
	  g_signal_handler_disconnect (previous_toplevel, impl->toplevel_set_focus_id);
	  impl->toplevel_set_focus_id = 0;
	  impl->toplevel_last_focus_widget = NULL;
	}

      if (impl->toplevel_unmapped_id != 0)
	{
	  g_signal_handler_disconnect (previous_toplevel, impl->toplevel_unmapped_id);
	  impl->toplevel_unmapped_id = 0;
	}
    }

  if (gtk_widget_is_toplevel (toplevel))
    {
      g_assert (impl->toplevel_set_focus_id == 0);
      impl->toplevel_set_focus_id = g_signal_connect (toplevel, "set-focus",
						      G_CALLBACK (toplevel_set_focus_cb), impl);
      impl->toplevel_last_focus_widget = gtk_window_get_focus (GTK_WINDOW (toplevel));

      g_assert (impl->toplevel_unmapped_id == 0);
      impl->toplevel_unmapped_id = g_signal_connect (toplevel, "unmap",
						     G_CALLBACK (toplevel_unmap_cb), impl);
    }
}

/* Changes the icons wherever it is needed */
static void
change_icon_theme (GtkFileChooserDefault *impl)
{
  GtkSettings *settings;
  gint width, height;
  GtkCellRenderer *renderer;
  GList *cells;

  profile_start ("start", NULL);

  settings = gtk_settings_get_for_screen (gtk_widget_get_screen (GTK_WIDGET (impl)));

  if (gtk_icon_size_lookup_for_settings (settings, GTK_ICON_SIZE_MENU, &width, &height))
    impl->icon_size = MAX (width, height);
  else
    impl->icon_size = FALLBACK_ICON_SIZE;

  shortcuts_reload_icons (impl);
  /* the first cell in the first column is the icon column, and we have a fixed size there */
  cells = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (
        gtk_tree_view_get_column (GTK_TREE_VIEW (impl->browse_files_tree_view), 0)));
  renderer = GTK_CELL_RENDERER (cells->data);
  set_icon_cell_renderer_fixed_size (impl, renderer);
  g_list_free (cells);
  if (impl->browse_files_model)
    _gtk_file_system_model_clear_cache (impl->browse_files_model, MODEL_COL_PIXBUF);
  gtk_widget_queue_resize (impl->browse_files_tree_view);

  profile_end ("end", NULL);
}

/* Callback used when a GtkSettings value changes */
static void
settings_notify_cb (GObject               *object,
		    GParamSpec            *pspec,
		    GtkFileChooserDefault *impl)
{
  const char *name;

  profile_start ("start", NULL);

  name = g_param_spec_get_name (pspec);

  if (strcmp (name, "gtk-icon-theme-name") == 0 ||
      strcmp (name, "gtk-icon-sizes") == 0)
    change_icon_theme (impl);

  profile_end ("end", NULL);
}

/* Installs a signal handler for GtkSettings so that we can monitor changes in
 * the icon theme.
 */
static void
check_icon_theme (GtkFileChooserDefault *impl)
{
  GtkSettings *settings;

  profile_start ("start", NULL);

  if (impl->settings_signal_id)
    {
      profile_end ("end", NULL);
      return;
    }

  if (gtk_widget_has_screen (GTK_WIDGET (impl)))
    {
      settings = gtk_settings_get_for_screen (gtk_widget_get_screen (GTK_WIDGET (impl)));
      impl->settings_signal_id = g_signal_connect (settings, "notify",
						   G_CALLBACK (settings_notify_cb), impl);

      change_icon_theme (impl);
    }

  profile_end ("end", NULL);
}

static void
gtk_file_chooser_default_style_set (GtkWidget *widget,
				    GtkStyle  *previous_style)
{
  GtkFileChooserDefault *impl;

  profile_start ("start", NULL);

  impl = GTK_FILE_CHOOSER_DEFAULT (widget);

  profile_msg ("    parent class style_set start", NULL);
  GTK_WIDGET_CLASS (_gtk_file_chooser_default_parent_class)->style_set (widget, previous_style);
  profile_msg ("    parent class style_set end", NULL);

  if (gtk_widget_has_screen (GTK_WIDGET (impl)))
    change_icon_theme (impl);

  emit_default_size_changed (impl);

  profile_end ("end", NULL);
}

static void
gtk_file_chooser_default_screen_changed (GtkWidget *widget,
					 GdkScreen *previous_screen)
{
  GtkFileChooserDefault *impl;

  profile_start ("start", NULL);

  impl = GTK_FILE_CHOOSER_DEFAULT (widget);

  if (GTK_WIDGET_CLASS (_gtk_file_chooser_default_parent_class)->screen_changed)
    GTK_WIDGET_CLASS (_gtk_file_chooser_default_parent_class)->screen_changed (widget, previous_screen);

  remove_settings_signal (impl, previous_screen);
  check_icon_theme (impl);

  emit_default_size_changed (impl);

  profile_end ("end", NULL);
}

static void
set_sort_column (GtkFileChooserDefault *impl)
{
  GtkTreeSortable *sortable;

  sortable = GTK_TREE_SORTABLE (gtk_tree_view_get_model (GTK_TREE_VIEW (impl->browse_files_tree_view)));
  /* can happen when we're still populating the model */
  if (sortable == NULL)
    return;

  gtk_tree_sortable_set_sort_column_id (sortable,
                                        impl->sort_column,
                                        impl->sort_order);
}

static void
settings_load (GtkFileChooserDefault *impl)
{
  GtkFileChooserSettings *settings;
  LocationMode location_mode;
  gboolean show_hidden;
  gboolean show_size_column;
  gint sort_column;
  GtkSortType sort_order;
  StartupMode startup_mode;

  settings = _gtk_file_chooser_settings_new ();

  location_mode = _gtk_file_chooser_settings_get_location_mode (settings);
  show_hidden = _gtk_file_chooser_settings_get_show_hidden (settings);
  show_size_column = _gtk_file_chooser_settings_get_show_size_column (settings);
  sort_column = _gtk_file_chooser_settings_get_sort_column (settings);
  sort_order = _gtk_file_chooser_settings_get_sort_order (settings);
  startup_mode = _gtk_file_chooser_settings_get_startup_mode (settings);

  g_object_unref (settings);

  location_mode_set (impl, location_mode, TRUE);

  gtk_file_chooser_set_show_hidden (GTK_FILE_CHOOSER (impl), show_hidden);

  impl->show_size_column = show_size_column;
  gtk_tree_view_column_set_visible (impl->list_size_column, show_size_column);

  impl->sort_column = sort_column;
  impl->sort_order = sort_order;
  /* We don't call set_sort_column() here as the models may not have been
   * created yet.  The individual functions that create and set the models will
   * call set_sort_column() themselves.
   */

  impl->startup_mode = startup_mode;
}

static void
save_dialog_geometry (GtkFileChooserDefault *impl, GtkFileChooserSettings *settings)
{
  GtkWindow *toplevel;
  int x, y, width, height;

  toplevel = get_toplevel (GTK_WIDGET (impl));

  if (!(toplevel && GTK_IS_FILE_CHOOSER_DIALOG (toplevel)))
    return;

  gtk_window_get_position (toplevel, &x, &y);
  gtk_window_get_size (toplevel, &width, &height);

  _gtk_file_chooser_settings_set_geometry (settings, x, y, width, height);
}

static void
settings_save (GtkFileChooserDefault *impl)
{
  GtkFileChooserSettings *settings;

  settings = _gtk_file_chooser_settings_new ();

  /* All the other state */

  _gtk_file_chooser_settings_set_location_mode (settings, impl->location_mode);
  _gtk_file_chooser_settings_set_show_hidden (settings, gtk_file_chooser_get_show_hidden (GTK_FILE_CHOOSER (impl)));
  _gtk_file_chooser_settings_set_show_size_column (settings, impl->show_size_column);
  _gtk_file_chooser_settings_set_sort_column (settings, impl->sort_column);
  _gtk_file_chooser_settings_set_sort_order (settings, impl->sort_order);
  _gtk_file_chooser_settings_set_startup_mode (settings, impl->startup_mode);

  save_dialog_geometry (impl, settings);

  /* NULL GError */
  _gtk_file_chooser_settings_save (settings, NULL);

  g_object_unref (settings);
}

/* GtkWidget::realize method */
static void
gtk_file_chooser_default_realize (GtkWidget *widget)
{
  GtkFileChooserDefault *impl;

  impl = GTK_FILE_CHOOSER_DEFAULT (widget);

  GTK_WIDGET_CLASS (_gtk_file_chooser_default_parent_class)->realize (widget);

  emit_default_size_changed (impl);
}

/* Changes the current folder to $CWD */
static void
switch_to_cwd (GtkFileChooserDefault *impl)
{
  char *current_working_dir;

  current_working_dir = g_get_current_dir ();
  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (impl), current_working_dir);
  g_free (current_working_dir);
}

/* Sets the file chooser to showing Recent Files or $CWD, depending on the
 * user's settings.
 */
static void
set_startup_mode (GtkFileChooserDefault *impl)
{
  switch (impl->startup_mode)
    {
    case STARTUP_MODE_RECENT:
      recent_shortcut_handler (impl);
      break;

    case STARTUP_MODE_CWD:
      switch_to_cwd (impl);
      break;

    default:
      g_assert_not_reached ();
    }
}

/* GtkWidget::map method */
static void
gtk_file_chooser_default_map (GtkWidget *widget)
{
  GtkFileChooserDefault *impl;

  profile_start ("start", NULL);

  impl = GTK_FILE_CHOOSER_DEFAULT (widget);

  GTK_WIDGET_CLASS (_gtk_file_chooser_default_parent_class)->map (widget);

  settings_load (impl);

  if (impl->operation_mode == OPERATION_MODE_BROWSE)
    {
      switch (impl->reload_state)
        {
        case RELOAD_EMPTY:
	  set_startup_mode (impl);
          break;
        
        case RELOAD_HAS_FOLDER:
          /* Nothing; we are already loading or loaded, so we
           * don't need to reload
           */
          break;

        default:
          g_assert_not_reached ();
      }
    }

  volumes_bookmarks_changed_cb (impl->file_system, impl);

  profile_end ("end", NULL);
}

static void
install_list_model_filter (GtkFileChooserDefault *impl)
{
  _gtk_file_system_model_set_filter (impl->browse_files_model,
                                     impl->current_filter);
}

#define COMPARE_DIRECTORIES										       \
  GtkFileChooserDefault *impl = user_data;								       \
  GtkFileSystemModel *fs_model = GTK_FILE_SYSTEM_MODEL (model);                                                \
  gboolean dir_a, dir_b;										       \
													       \
  dir_a = g_value_get_boolean (_gtk_file_system_model_get_value (fs_model, a, MODEL_COL_IS_FOLDER));           \
  dir_b = g_value_get_boolean (_gtk_file_system_model_get_value (fs_model, b, MODEL_COL_IS_FOLDER));           \
													       \
  if (dir_a != dir_b)											       \
    return impl->list_sort_ascending ? (dir_a ? -1 : 1) : (dir_a ? 1 : -1) /* Directories *always* go first */

/* Sort callback for the filename column */
static gint
name_sort_func (GtkTreeModel *model,
		GtkTreeIter  *a,
		GtkTreeIter  *b,
		gpointer      user_data)
{
  COMPARE_DIRECTORIES;
  else
    {
      const char *key_a, *key_b;
      gint result;

      key_a = g_value_get_string (_gtk_file_system_model_get_value (fs_model, a, MODEL_COL_NAME_COLLATED));
      key_b = g_value_get_string (_gtk_file_system_model_get_value (fs_model, b, MODEL_COL_NAME_COLLATED));

      if (key_a && key_b)
        result = strcmp (key_a, key_b);
      else if (key_a)
        result = 1;
      else if (key_b)
        result = -1;
      else
        result = 0;

      return result;
    }
}

/* Sort callback for the size column */
static gint
size_sort_func (GtkTreeModel *model,
		GtkTreeIter  *a,
		GtkTreeIter  *b,
		gpointer      user_data)
{
  COMPARE_DIRECTORIES;
  else
    {
      gint64 size_a, size_b;

      size_a = g_value_get_int64 (_gtk_file_system_model_get_value (fs_model, a, MODEL_COL_SIZE));
      size_b = g_value_get_int64 (_gtk_file_system_model_get_value (fs_model, b, MODEL_COL_SIZE));

      return size_a < size_b ? -1 : (size_a == size_b ? 0 : 1);
    }
}

/* Sort callback for the mtime column */
static gint
mtime_sort_func (GtkTreeModel *model,
		 GtkTreeIter  *a,
		 GtkTreeIter  *b,
		 gpointer      user_data)
{
  COMPARE_DIRECTORIES;
  else
    {
      glong ta, tb;

      ta = g_value_get_long (_gtk_file_system_model_get_value (fs_model, a, MODEL_COL_MTIME));
      tb = g_value_get_long (_gtk_file_system_model_get_value (fs_model, b, MODEL_COL_MTIME));

      return ta < tb ? -1 : (ta == tb ? 0 : 1);
    }
}

/* Callback used when the sort column changes.  We cache the sort order for use
 * in name_sort_func().
 */
static void
list_sort_column_changed_cb (GtkTreeSortable       *sortable,
			     GtkFileChooserDefault *impl)
{
  gint sort_column_id;
  GtkSortType sort_type;

  if (gtk_tree_sortable_get_sort_column_id (sortable, &sort_column_id, &sort_type))
    {
      impl->list_sort_ascending = (sort_type == GTK_SORT_ASCENDING);
      impl->sort_column = sort_column_id;
      impl->sort_order = sort_type;
    }
}

static void
set_busy_cursor (GtkFileChooserDefault *impl,
		 gboolean               busy)
{
  GtkWidget *widget;
  GtkWindow *toplevel;
  GdkDisplay *display;
  GdkCursor *cursor;

  toplevel = get_toplevel (GTK_WIDGET (impl));
  widget = GTK_WIDGET (toplevel);
  if (!toplevel || !gtk_widget_get_realized (widget))
    return;

  display = gtk_widget_get_display (widget);

  if (busy)
    cursor = gdk_cursor_new_for_display (display, GDK_WATCH);
  else
    cursor = NULL;

  gdk_window_set_cursor (gtk_widget_get_window (widget), cursor);
  gdk_display_flush (display);

  if (cursor)
    gdk_cursor_unref (cursor);
}

/* Creates a sort model to wrap the file system model and sets it on the tree view */
static void
load_set_model (GtkFileChooserDefault *impl)
{
  profile_start ("start", NULL);

  g_assert (impl->browse_files_model != NULL);

  profile_msg ("    gtk_tree_view_set_model start", NULL);
  gtk_tree_view_set_model (GTK_TREE_VIEW (impl->browse_files_tree_view),
			   GTK_TREE_MODEL (impl->browse_files_model));
  gtk_tree_view_columns_autosize (GTK_TREE_VIEW (impl->browse_files_tree_view));
  gtk_tree_view_set_search_column (GTK_TREE_VIEW (impl->browse_files_tree_view),
				   MODEL_COL_NAME);
  file_list_set_sort_column_ids (impl);
  set_sort_column (impl);
  profile_msg ("    gtk_tree_view_set_model end", NULL);
  impl->list_sort_ascending = TRUE;

  profile_end ("end", NULL);
}

/* Timeout callback used when the loading timer expires */
static gboolean
load_timeout_cb (gpointer data)
{
  GtkFileChooserDefault *impl;

  profile_start ("start", NULL);

  impl = GTK_FILE_CHOOSER_DEFAULT (data);
  g_assert (impl->load_state == LOAD_PRELOAD);
  g_assert (impl->load_timeout_id != 0);
  g_assert (impl->browse_files_model != NULL);

  impl->load_timeout_id = 0;
  impl->load_state = LOAD_LOADING;

  load_set_model (impl);

  profile_end ("end", NULL);

  return FALSE;
}

/* Sets up a new load timer for the model and switches to the LOAD_PRELOAD state */
static void
load_setup_timer (GtkFileChooserDefault *impl)
{
  g_assert (impl->load_timeout_id == 0);
  g_assert (impl->load_state != LOAD_PRELOAD);

  impl->load_timeout_id = gdk_threads_add_timeout (MAX_LOADING_TIME, load_timeout_cb, impl);
  impl->load_state = LOAD_PRELOAD;
}

/* Removes the load timeout; changes the impl->load_state to the specified value. */
static void
load_remove_timer (GtkFileChooserDefault *impl, LoadState new_load_state)
{
  if (impl->load_timeout_id != 0)
    {
      g_assert (impl->load_state == LOAD_PRELOAD);

      g_source_remove (impl->load_timeout_id);
      impl->load_timeout_id = 0;
    }
  else
    g_assert (impl->load_state == LOAD_EMPTY ||
	      impl->load_state == LOAD_LOADING ||
	      impl->load_state == LOAD_FINISHED);

  g_assert (new_load_state == LOAD_EMPTY ||
	    new_load_state == LOAD_LOADING ||
	    new_load_state == LOAD_FINISHED);
  impl->load_state = new_load_state;
}

/* Selects the first row in the file list */
static void
browse_files_select_first_row (GtkFileChooserDefault *impl)
{
  GtkTreePath *path;
  GtkTreeIter dummy_iter;
  GtkTreeModel *tree_model;

  tree_model = gtk_tree_view_get_model (GTK_TREE_VIEW (impl->browse_files_tree_view));

  if (!tree_model)
    return;

  path = gtk_tree_path_new_from_indices (0, -1);

  /* If the list is empty, do nothing. */
  if (gtk_tree_model_get_iter (tree_model, &dummy_iter, path))
      gtk_tree_view_set_cursor (GTK_TREE_VIEW (impl->browse_files_tree_view), path, NULL, FALSE);

  gtk_tree_path_free (path);
}

struct center_selected_row_closure {
  GtkFileChooserDefault *impl;
  gboolean already_centered;
};

/* Callback used from gtk_tree_selection_selected_foreach(); centers the
 * selected row in the tree view.
 */
static void
center_selected_row_foreach_cb (GtkTreeModel      *model,
				GtkTreePath       *path,
				GtkTreeIter       *iter,
				gpointer           data)
{
  struct center_selected_row_closure *closure;

  closure = data;
  if (closure->already_centered)
    return;

  gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (closure->impl->browse_files_tree_view), path, NULL, TRUE, 0.5, 0.0);
  closure->already_centered = TRUE;
}

/* Centers the selected row in the tree view */
static void
browse_files_center_selected_row (GtkFileChooserDefault *impl)
{
  struct center_selected_row_closure closure;
  GtkTreeSelection *selection;

  closure.impl = impl;
  closure.already_centered = FALSE;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->browse_files_tree_view));
  gtk_tree_selection_selected_foreach (selection, center_selected_row_foreach_cb, &closure);
}

static gboolean
show_and_select_files (GtkFileChooserDefault *impl,
		       GSList                *files)
{
  GtkTreeSelection *selection;
  GtkFileSystemModel *fsmodel;
  gboolean enabled_hidden, removed_filters;
  gboolean selected_a_file;
  GSList *walk;

  g_assert (impl->load_state == LOAD_FINISHED);
  g_assert (impl->browse_files_model != NULL);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->browse_files_tree_view));
  fsmodel = GTK_FILE_SYSTEM_MODEL (gtk_tree_view_get_model (GTK_TREE_VIEW (impl->browse_files_tree_view)));

  g_assert (fsmodel == impl->browse_files_model);

  enabled_hidden = impl->show_hidden;
  removed_filters = (impl->current_filter == NULL);

  selected_a_file = FALSE;

  for (walk = files; walk; walk = walk->next)
    {
      GFile *file = walk->data;
      GtkTreeIter iter;

      /* Is it a hidden file? */

      if (!_gtk_file_system_model_get_iter_for_file (fsmodel, &iter, file))
        continue;

      if (!_gtk_file_system_model_iter_is_visible (fsmodel, &iter))
        {
          GFileInfo *info = _gtk_file_system_model_get_info (fsmodel, &iter);

          if (!enabled_hidden &&
              (g_file_info_get_is_hidden (info) ||
               g_file_info_get_is_backup (info)))
            {
              g_object_set (impl, "show-hidden", TRUE, NULL);
              enabled_hidden = TRUE;
            }
        }

      /* Is it a filtered file? */

      if (!_gtk_file_system_model_get_iter_for_file (fsmodel, &iter, file))
        continue; /* re-get the iter as it may change when the model refilters */

      if (!_gtk_file_system_model_iter_is_visible (fsmodel, &iter))
        {
	  /* Maybe we should have a way to ask the fsmodel if it had filtered a file */
	  if (!removed_filters)
	    {
	      set_current_filter (impl, NULL);
	      removed_filters = TRUE;
	    }
	}

      /* Okay, can we select the file now? */
          
      if (!_gtk_file_system_model_get_iter_for_file (fsmodel, &iter, file))
        continue;

      if (_gtk_file_system_model_iter_is_visible (fsmodel, &iter))
        {
          GtkTreePath *path;

          gtk_tree_selection_select_iter (selection, &iter);

          path = gtk_tree_model_get_path (GTK_TREE_MODEL (fsmodel), &iter);
          gtk_tree_view_set_cursor (GTK_TREE_VIEW (impl->browse_files_tree_view),
                                    path, NULL, FALSE);
          gtk_tree_path_free (path);

          selected_a_file = TRUE;
        }
    }

  browse_files_center_selected_row (impl);

  return selected_a_file;
}

/* Processes the pending operation when a folder is finished loading */
static void
pending_select_files_process (GtkFileChooserDefault *impl)
{
  g_assert (impl->load_state == LOAD_FINISHED);
  g_assert (impl->browse_files_model != NULL);

  if (impl->pending_select_files)
    {
      show_and_select_files (impl, impl->pending_select_files);
      pending_select_files_free (impl);
      browse_files_center_selected_row (impl);
    }
  else
    {
      /* We only select the first row if the chooser is actually mapped ---
       * selecting the first row is to help the user when he is interacting with
       * the chooser, but sometimes a chooser works not on behalf of the user,
       * but rather on behalf of something else like GtkFileChooserButton.  In
       * that case, the chooser's selection should be what the caller expects,
       * as the user can't see that something else got selected.  See bug #165264.
       */
      if (impl->action == GTK_FILE_CHOOSER_ACTION_OPEN &&
          gtk_widget_get_mapped (GTK_WIDGET (impl)))
	browse_files_select_first_row (impl);
    }

  g_assert (impl->pending_select_files == NULL);
}

static void
show_error_on_reading_current_folder (GtkFileChooserDefault *impl, GError *error)
{
  GFileInfo *info;
  char *msg;

  info = g_file_query_info (impl->current_folder,
			    G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
			    G_FILE_QUERY_INFO_NONE,
			    NULL,
			    NULL);
  if (info)
    {
      msg = g_strdup_printf (_("Could not read the contents of %s"), g_file_info_get_display_name (info));
      g_object_unref (info);
    }
  else
    msg = g_strdup (_("Could not read the contents of the folder"));

  error_message (impl, msg, error->message);
  g_free (msg);
}

/* Callback used when the file system model finishes loading */
static void
browse_files_model_finished_loading_cb (GtkFileSystemModel    *model,
                                        GError                *error,
					GtkFileChooserDefault *impl)
{
  profile_start ("start", NULL);

  if (error)
    show_error_on_reading_current_folder (impl, error);

  if (impl->load_state == LOAD_PRELOAD)
    {
      load_remove_timer (impl, LOAD_FINISHED);
      load_set_model (impl);
    }
  else if (impl->load_state == LOAD_LOADING)
    {
      /* Nothing */
    }
  else
    {
      /* We can't g_assert_not_reached(), as something other than us may have
       *  initiated a folder reload.  See #165556.
       */
      profile_end ("end", NULL);
      return;
    }

  g_assert (impl->load_timeout_id == 0);

  impl->load_state = LOAD_FINISHED;

  pending_select_files_process (impl);
  set_busy_cursor (impl, FALSE);
#ifdef PROFILE_FILE_CHOOSER
  access ("MARK: *** FINISHED LOADING", F_OK);
#endif

  profile_end ("end", NULL);
}

static void
stop_loading_and_clear_list_model (GtkFileChooserDefault *impl,
                                   gboolean remove_from_treeview)
{
  load_remove_timer (impl, LOAD_EMPTY);
  
  if (impl->browse_files_model)
    {
      g_object_unref (impl->browse_files_model);
      impl->browse_files_model = NULL;
    }

  if (remove_from_treeview)
    gtk_tree_view_set_model (GTK_TREE_VIEW (impl->browse_files_tree_view), NULL);
}

static char *
my_g_format_time_for_display (glong secs)
{
  GDate mtime, now;
  gint days_diff;
  struct tm tm_mtime;
  time_t time_mtime, time_now;
  const gchar *format;
  gchar *locale_format = NULL;
  gchar buf[256];
  char *date_str = NULL;
#ifdef G_OS_WIN32
  const char *locale, *dot = NULL;
  gint64 codepage = -1;
  char charset[20];
#endif

  time_mtime = secs;

#ifdef HAVE_LOCALTIME_R
  localtime_r ((time_t *) &time_mtime, &tm_mtime);
#else
  {
    struct tm *ptm = localtime ((time_t *) &time_mtime);

    if (!ptm)
      {
        g_warning ("ptm != NULL failed");
        
        return g_strdup (_("Unknown"));
      }
    else
      memcpy ((void *) &tm_mtime, (void *) ptm, sizeof (struct tm));
  }
#endif /* HAVE_LOCALTIME_R */

  g_date_set_time_t (&mtime, time_mtime);
  time_now = time (NULL);
  g_date_set_time_t (&now, time_now);

  days_diff = g_date_get_julian (&now) - g_date_get_julian (&mtime);

  /* Translators: %H means "hours" and %M means "minutes" */
  if (days_diff == 0)
    format = _("%H:%M");
  else if (days_diff == 1)
    format = _("Yesterday at %H:%M");
  else
    {
      if (days_diff > 1 && days_diff < 7)
        format = "%A"; /* Days from last week */
      else
        format = "%x"; /* Any other date */
    }

#ifdef G_OS_WIN32
  /* g_locale_from_utf8() returns a string in the system
   * code-page, which is not always the same as that used by the C
   * library. For instance when running a GTK+ program with
   * LANG=ko on an English version of Windows, the system
   * code-page is 1252, but the code-page used by the C library is
   * 949. (It's GTK+ itself that sets the C library locale when it
   * notices the LANG environment variable. See gtkmain.c The
   * Microsoft C library doesn't look at any locale environment
   * variables.) We need to pass strftime() a string in the C
   * library's code-page. See bug #509885.
   */
  locale = setlocale (LC_ALL, NULL);
  if (locale != NULL)
    dot = strchr (locale, '.');
  if (dot != NULL)
    {
      codepage = g_ascii_strtoll (dot+1, NULL, 10);
      
      /* All codepages should fit in 16 bits AFAIK */
      if (codepage > 0 && codepage < 65536)
        {
          sprintf (charset, "CP%u", (guint) codepage);
          locale_format = g_convert (format, -1, charset, "UTF-8", NULL, NULL, NULL);
        }
    }
#else
  locale_format = g_locale_from_utf8 (format, -1, NULL, NULL, NULL);
#endif
  if (locale_format != NULL &&
      strftime (buf, sizeof (buf), locale_format, &tm_mtime) != 0)
    {
#ifdef G_OS_WIN32
      /* As above but in opposite direction... */
      if (codepage > 0 && codepage < 65536)
        date_str = g_convert (buf, -1, "UTF-8", charset, NULL, NULL, NULL);
#else
      date_str = g_locale_to_utf8 (buf, -1, NULL, NULL, NULL);
#endif
    }

  if (date_str == NULL)
    date_str = g_strdup (_("Unknown"));

  g_free (locale_format);
  return date_str;
}

static void
copy_attribute (GFileInfo *to, GFileInfo *from, const char *attribute)
{
  GFileAttributeType type;
  gpointer value;

  if (g_file_info_get_attribute_data (from, attribute, &type, &value, NULL))
    g_file_info_set_attribute (to, attribute, type, value);
}

static void
file_system_model_got_thumbnail (GObject *object, GAsyncResult *res, gpointer data)
{
  GtkFileSystemModel *model = data; /* might be unreffed if operation was cancelled */
  GFile *file = G_FILE (object);
  GFileInfo *queried, *info;
  GtkTreeIter iter;

  queried = g_file_query_info_finish (file, res, NULL);
  if (queried == NULL)
    return;

  GDK_THREADS_ENTER ();

  /* now we know model is valid */

  /* file was deleted */
  if (!_gtk_file_system_model_get_iter_for_file (model, &iter, file))
    {
      g_object_unref (queried);
      GDK_THREADS_LEAVE ();
      return;
    }

  info = g_file_info_dup (_gtk_file_system_model_get_info (model, &iter));

  copy_attribute (info, queried, G_FILE_ATTRIBUTE_THUMBNAIL_PATH);
  copy_attribute (info, queried, G_FILE_ATTRIBUTE_THUMBNAILING_FAILED);
  copy_attribute (info, queried, G_FILE_ATTRIBUTE_STANDARD_ICON);

  _gtk_file_system_model_update_file (model, file, info);

  g_object_unref (info);
  g_object_unref (queried);

  GDK_THREADS_LEAVE ();
}

static gboolean
file_system_model_set (GtkFileSystemModel *model,
                       GFile              *file,
                       GFileInfo          *info,
                       int                 column,
                       GValue             *value,
                       gpointer            data)
{
  GtkFileChooserDefault *impl = data;
 
  switch (column)
    {
    case MODEL_COL_FILE:
      g_value_set_object (value, file);
      break;
    case MODEL_COL_NAME:
      if (info == NULL)
        g_value_set_string (value, DEFAULT_NEW_FOLDER_NAME);
      else 
        g_value_set_string (value, g_file_info_get_display_name (info));
      break;
    case MODEL_COL_NAME_COLLATED:
      if (info == NULL)
        g_value_take_string (value, g_utf8_collate_key_for_filename (DEFAULT_NEW_FOLDER_NAME, -1));
      else 
        g_value_take_string (value, g_utf8_collate_key_for_filename (g_file_info_get_display_name (info), -1));
      break;
    case MODEL_COL_IS_FOLDER:
      g_value_set_boolean (value, info == NULL || _gtk_file_info_consider_as_directory (info));
      break;
    case MODEL_COL_IS_SENSITIVE:
      if (info)
        {
          gboolean sensitive = TRUE;

          if (impl->action != GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER &&
              impl->action != GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER)
            {
              sensitive = TRUE;
            }
          else if (!_gtk_file_info_consider_as_directory (info))
            {
              sensitive = FALSE;
            }
          else
            {
              GtkTreeIter iter;
              if (!_gtk_file_system_model_get_iter_for_file (model, &iter, file))
                g_assert_not_reached ();
              sensitive = !_gtk_file_system_model_iter_is_filtered_out (model, &iter);
            }

          g_value_set_boolean (value, sensitive);
        }
      else
        g_value_set_boolean (value, TRUE);
      break;
    case MODEL_COL_PIXBUF:
      if (info)
        {
          if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_STANDARD_ICON))
            {
              g_value_take_object (value, _gtk_file_info_render_icon (info, GTK_WIDGET (impl), impl->icon_size));
            }
          else
            {
              GtkTreeModel *tree_model;
              GtkTreePath *path, *start, *end;
              GtkTreeIter iter;

              if (impl->browse_files_tree_view == NULL ||
                  g_file_info_has_attribute (info, "filechooser::queried"))
                return FALSE;

              tree_model = gtk_tree_view_get_model (GTK_TREE_VIEW (impl->browse_files_tree_view));
              if (tree_model != GTK_TREE_MODEL (model))
                return FALSE;

              if (!_gtk_file_system_model_get_iter_for_file (model,
                                                             &iter,
                                                             file))
                g_assert_not_reached ();
              if (!gtk_tree_view_get_visible_range (GTK_TREE_VIEW (impl->browse_files_tree_view), &start, &end))
                return FALSE;
              path = gtk_tree_model_get_path (tree_model, &iter);
              if (gtk_tree_path_compare (start, path) != 1 &&
                  gtk_tree_path_compare (path, end) != 1)
                {
                  g_file_info_set_attribute_boolean (info, "filechooser::queried", TRUE);
                  g_file_query_info_async (file,
                                           G_FILE_ATTRIBUTE_THUMBNAIL_PATH ","
                                           G_FILE_ATTRIBUTE_THUMBNAILING_FAILED ","
                                           G_FILE_ATTRIBUTE_STANDARD_ICON,
                                           G_FILE_QUERY_INFO_NONE,
                                           G_PRIORITY_DEFAULT,
                                           _gtk_file_system_model_get_cancellable (model),
                                           file_system_model_got_thumbnail,
                                           model);
                }
              gtk_tree_path_free (path);
              gtk_tree_path_free (start);
              gtk_tree_path_free (end);
              return FALSE;
            }
        }
      else
        g_value_set_object (value, NULL);
      break;
    case MODEL_COL_SIZE:
      g_value_set_int64 (value, info ? g_file_info_get_size (info) : 0);
      break;
    case MODEL_COL_SIZE_TEXT:
      if (info == NULL || _gtk_file_info_consider_as_directory (info))
        g_value_set_string (value, NULL);
      else
#if GLIB_CHECK_VERSION(2,30,0)
        g_value_take_string (value, g_format_size (g_file_info_get_size (info)));
#else
        g_value_take_string (value, g_format_size_for_display (g_file_info_get_size (info)));
#endif
      break;
    case MODEL_COL_MTIME:
    case MODEL_COL_MTIME_TEXT:
      {
        GTimeVal tv;
        if (info == NULL)
          break;
        g_file_info_get_modification_time (info, &tv);
        if (column == MODEL_COL_MTIME)
          g_value_set_long (value, tv.tv_sec);
        else if (tv.tv_sec == 0)
          g_value_set_static_string (value, _("Unknown"));
        else
          g_value_take_string (value, my_g_format_time_for_display (tv.tv_sec));
        break;
      }
    case MODEL_COL_ELLIPSIZE:
      g_value_set_enum (value, info ? PANGO_ELLIPSIZE_END : PANGO_ELLIPSIZE_NONE);
      break;
    default:
      g_assert_not_reached ();
      break;
    }

  return TRUE;
}

/* Gets rid of the old list model and creates a new one for the current folder */
static gboolean
set_list_model (GtkFileChooserDefault *impl,
		GError               **error)
{
  g_assert (impl->current_folder != NULL);

  profile_start ("start", NULL);

  stop_loading_and_clear_list_model (impl, TRUE);

  set_busy_cursor (impl, TRUE);

  impl->browse_files_model = 
    _gtk_file_system_model_new_for_directory (impl->current_folder,
					      MODEL_ATTRIBUTES,
					      file_system_model_set,
					      impl,
					      MODEL_COLUMN_TYPES);

  _gtk_file_system_model_set_show_hidden (impl->browse_files_model, impl->show_hidden);

  profile_msg ("    set sort function", NULL);
  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (impl->browse_files_model), MODEL_COL_NAME, name_sort_func, impl, NULL);
  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (impl->browse_files_model), MODEL_COL_SIZE, size_sort_func, impl, NULL);
  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (impl->browse_files_model), MODEL_COL_MTIME, mtime_sort_func, impl, NULL);
  gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (impl->browse_files_model), NULL, NULL, NULL);
  set_sort_column (impl);
  impl->list_sort_ascending = TRUE;
  g_signal_connect (impl->browse_files_model, "sort-column-changed",
		    G_CALLBACK (list_sort_column_changed_cb), impl);

  load_setup_timer (impl); /* This changes the state to LOAD_PRELOAD */

  g_signal_connect (impl->browse_files_model, "finished-loading",
		    G_CALLBACK (browse_files_model_finished_loading_cb), impl);

  install_list_model_filter (impl);

  profile_end ("end", NULL);

  return TRUE;
}

struct update_chooser_entry_selected_foreach_closure {
  int num_selected;
  GtkTreeIter first_selected_iter;
};

static gint
compare_utf8_filenames (const gchar *a,
			const gchar *b)
{
  gchar *a_folded, *b_folded;
  gint retval;

  a_folded = g_utf8_strdown (a, -1);
  b_folded = g_utf8_strdown (b, -1);

  retval = strcmp (a_folded, b_folded);

  g_free (a_folded);
  g_free (b_folded);

  return retval;
}

static void
update_chooser_entry_selected_foreach (GtkTreeModel *model,
				       GtkTreePath *path,
				       GtkTreeIter *iter,
				       gpointer data)
{
  struct update_chooser_entry_selected_foreach_closure *closure;

  closure = data;
  closure->num_selected++;

  if (closure->num_selected == 1)
    closure->first_selected_iter = *iter;
}

static void
update_chooser_entry (GtkFileChooserDefault *impl)
{
  GtkTreeSelection *selection;
  struct update_chooser_entry_selected_foreach_closure closure;

  /* no need to update the file chooser's entry if there's no entry */
  if (impl->operation_mode == OPERATION_MODE_SEARCH ||
      !impl->location_entry)
    return;

  if (!(impl->action == GTK_FILE_CHOOSER_ACTION_SAVE
        || impl->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER
        || ((impl->action == GTK_FILE_CHOOSER_ACTION_OPEN
             || impl->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER)
            && impl->location_mode == LOCATION_MODE_FILENAME_ENTRY)))
    return;

  g_assert (impl->location_entry != NULL);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->browse_files_tree_view));
  closure.num_selected = 0;
  gtk_tree_selection_selected_foreach (selection, update_chooser_entry_selected_foreach, &closure);

  if (closure.num_selected == 0)
    {
      if (impl->operation_mode == OPERATION_MODE_RECENT)
	_gtk_file_chooser_entry_set_base_folder (GTK_FILE_CHOOSER_ENTRY (impl->location_entry), NULL);
      else
	goto maybe_clear_entry;
    }
  else if (closure.num_selected == 1)
    {
      if (impl->operation_mode == OPERATION_MODE_BROWSE)
        {
          GFileInfo *info;
          gboolean change_entry;

          info = _gtk_file_system_model_get_info (impl->browse_files_model, &closure.first_selected_iter);

          /* If the cursor moved to the row of the newly created folder,
           * retrieving info will return NULL.
           */
          if (!info)
            return;

          g_free (impl->browse_files_last_selected_name);
          impl->browse_files_last_selected_name =
            g_strdup (g_file_info_get_display_name (info));

          if (impl->action == GTK_FILE_CHOOSER_ACTION_OPEN ||
              impl->action == GTK_FILE_CHOOSER_ACTION_SAVE ||
              impl->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER)
            {
              /* Don't change the name when clicking on a folder... */
              change_entry = (! _gtk_file_info_consider_as_directory (info));
            }
          else
            change_entry = TRUE; /* ... unless we are in SELECT_FOLDER mode */

          if (change_entry)
            {
              gtk_entry_set_text (GTK_ENTRY (impl->location_entry), impl->browse_files_last_selected_name);

              if (impl->action == GTK_FILE_CHOOSER_ACTION_SAVE)
                _gtk_file_chooser_entry_select_filename (GTK_FILE_CHOOSER_ENTRY (impl->location_entry));
            }

          return;
        }
      else if (impl->operation_mode == OPERATION_MODE_RECENT
	       && impl->action == GTK_FILE_CHOOSER_ACTION_SAVE)
	{
	  GFile *folder;

	  /* Set the base folder on the name entry, so it will do completion relative to the correct recent-folder */

	  gtk_tree_model_get (GTK_TREE_MODEL (impl->recent_model), &closure.first_selected_iter,
			      MODEL_COL_FILE, &folder,
			      -1);
	  _gtk_file_chooser_entry_set_base_folder (GTK_FILE_CHOOSER_ENTRY (impl->location_entry), folder);
	  g_object_unref (folder);
	  return;
	}
    }
  else
    {
      g_assert (!(impl->action == GTK_FILE_CHOOSER_ACTION_SAVE ||
                  impl->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER));

      /* Multiple selection, so just clear the entry. */
      g_free (impl->browse_files_last_selected_name);
      impl->browse_files_last_selected_name = NULL;

      gtk_entry_set_text (GTK_ENTRY (impl->location_entry), "");
      return;
    }

 maybe_clear_entry:

  if ((impl->action == GTK_FILE_CHOOSER_ACTION_OPEN || impl->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER)
      && impl->browse_files_last_selected_name)
    {
      const char *entry_text;
      int len;
      gboolean clear_entry;

      entry_text = gtk_entry_get_text (GTK_ENTRY (impl->location_entry));
      len = strlen (entry_text);
      if (len != 0)
        {
          /* The file chooser entry may have appended a "/" to its text.
           * So take it out, and compare the result to the old selection.
           */
          if (entry_text[len - 1] == G_DIR_SEPARATOR)
            {
              gchar *tmp;

              tmp = g_strndup (entry_text, len - 1);
              clear_entry = (compare_utf8_filenames (impl->browse_files_last_selected_name, tmp) == 0);
              g_free (tmp);
            }
          else
            clear_entry = (compare_utf8_filenames (impl->browse_files_last_selected_name, entry_text) == 0);
        }
      else
        clear_entry = FALSE;

      if (clear_entry)
        gtk_entry_set_text (GTK_ENTRY (impl->location_entry), "");
    }
}

static gboolean
gtk_file_chooser_default_set_current_folder (GtkFileChooser  *chooser,
					     GFile           *file,
					     GError         **error)
{
  return gtk_file_chooser_default_update_current_folder (chooser, file, FALSE, FALSE, error);
}


struct UpdateCurrentFolderData
{
  GtkFileChooserDefault *impl;
  GFile *file;
  gboolean keep_trail;
  gboolean clear_entry;
  GFile *original_file;
  GError *original_error;
};

static void
update_current_folder_mount_enclosing_volume_cb (GCancellable        *cancellable,
                                                 GtkFileSystemVolume *volume,
                                                 const GError        *error,
                                                 gpointer             user_data)
{
  gboolean cancelled = g_cancellable_is_cancelled (cancellable);
  struct UpdateCurrentFolderData *data = user_data;
  GtkFileChooserDefault *impl = data->impl;

  if (cancellable != impl->update_current_folder_cancellable)
    goto out;

  impl->update_current_folder_cancellable = NULL;
  set_busy_cursor (impl, FALSE);

  if (cancelled)
    goto out;

  if (error)
    {
      error_changing_folder_dialog (data->impl, data->file, g_error_copy (error));
      impl->reload_state = RELOAD_EMPTY;
      goto out;
    }

  change_folder_and_display_error (impl, data->file, data->clear_entry);

out:
  g_object_unref (data->file);
  g_free (data);

  g_object_unref (cancellable);
}

static void
update_current_folder_get_info_cb (GCancellable *cancellable,
				   GFileInfo    *info,
				   const GError *error,
				   gpointer      user_data)
{
  gboolean cancelled = g_cancellable_is_cancelled (cancellable);
  struct UpdateCurrentFolderData *data = user_data;
  GtkFileChooserDefault *impl = data->impl;

  if (cancellable != impl->update_current_folder_cancellable)
    goto out;

  impl->update_current_folder_cancellable = NULL;
  impl->reload_state = RELOAD_EMPTY;

  set_busy_cursor (impl, FALSE);

  if (cancelled)
    goto out;

  if (error)
    {
      GFile *parent_file;

      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_MOUNTED))
        {
          GMountOperation *mount_operation;
          GtkWidget *toplevel;

          g_object_unref (cancellable);
          toplevel = gtk_widget_get_toplevel (GTK_WIDGET (impl));

          mount_operation = gtk_mount_operation_new (GTK_WINDOW (toplevel));

          set_busy_cursor (impl, TRUE);

          impl->update_current_folder_cancellable =
            _gtk_file_system_mount_enclosing_volume (impl->file_system, data->file,
                                                     mount_operation,
                                                     update_current_folder_mount_enclosing_volume_cb,
                                                     data);

          return;
        }

      if (!data->original_file)
        {
	  data->original_file = g_object_ref (data->file);
	  data->original_error = g_error_copy (error);
	}

      parent_file = g_file_get_parent (data->file);

      /* get parent path and try to change the folder to that */
      if (parent_file)
        {
	  g_object_unref (data->file);
	  data->file = parent_file;

	  g_object_unref (cancellable);

	  /* restart the update current folder operation */
	  impl->reload_state = RELOAD_HAS_FOLDER;

	  impl->update_current_folder_cancellable =
	    _gtk_file_system_get_info (impl->file_system, data->file,
				       "standard::type",
				       update_current_folder_get_info_cb,
				       data);

	  set_busy_cursor (impl, TRUE);

	  return;
	}
      else
        {
          /* Error and bail out, ignoring "not found" errors since they're useless:
           * they only happen when a program defaults to a folder that has been (re)moved.
           */
          if (!g_error_matches (data->original_error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
            error_changing_folder_dialog (impl, data->original_file, data->original_error);
          else
            g_error_free (data->original_error);

	  g_object_unref (data->original_file);

	  goto out;
	}
    }

  if (data->original_file)
    {
      /* Error and bail out, ignoring "not found" errors since they're useless:
       * they only happen when a program defaults to a folder that has been (re)moved.
       */
      if (!g_error_matches (data->original_error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
        error_changing_folder_dialog (impl, data->original_file, data->original_error);
      else
        g_error_free (data->original_error);

      g_object_unref (data->original_file);
    }

  if (! _gtk_file_info_consider_as_directory (info))
    goto out;

  if (!_gtk_path_bar_set_file (GTK_PATH_BAR (impl->browse_path_bar), data->file, data->keep_trail, NULL))
    goto out;

  if (impl->current_folder != data->file)
    {
      if (impl->current_folder)
	g_object_unref (impl->current_folder);

      impl->current_folder = g_object_ref (data->file);
    }

  impl->reload_state = RELOAD_HAS_FOLDER;

  /* Update the widgets that may trigger a folder change themselves.  */

  if (!impl->changing_folder)
    {
      impl->changing_folder = TRUE;

      shortcuts_update_current_folder (impl);

      impl->changing_folder = FALSE;
    }

  /* Set the folder on the save entry */

  if (impl->location_entry)
    {
      _gtk_file_chooser_entry_set_base_folder (GTK_FILE_CHOOSER_ENTRY (impl->location_entry),
					       impl->current_folder);

      if (data->clear_entry)
        gtk_entry_set_text (GTK_ENTRY (impl->location_entry), "");
    }

  /* Create a new list model.  This is slightly evil; we store the result value
   * but perform more actions rather than returning immediately even if it
   * generates an error.
   */
  set_list_model (impl, NULL);

  /* Refresh controls */

  shortcuts_find_current_folder (impl);

  g_signal_emit_by_name (impl, "current-folder-changed", 0);

  check_preview_change (impl);
  bookmarks_check_add_sensitivity (impl);

  g_signal_emit_by_name (impl, "selection-changed", 0);

out:
  g_object_unref (data->file);
  g_free (data);

  g_object_unref (cancellable);
}

static gboolean
gtk_file_chooser_default_update_current_folder (GtkFileChooser    *chooser,
						GFile             *file,
						gboolean           keep_trail,
						gboolean           clear_entry,
						GError           **error)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (chooser);
  struct UpdateCurrentFolderData *data;

  profile_start ("start", NULL);

  g_object_ref (file);

  operation_mode_set (impl, OPERATION_MODE_BROWSE);

  if (impl->local_only && !_gtk_file_has_native_path (file))
    {
      g_set_error_literal (error,
                           GTK_FILE_CHOOSER_ERROR,
                           GTK_FILE_CHOOSER_ERROR_BAD_FILENAME,
                           _("Cannot change to folder because it is not local"));

      g_object_unref (file);
      profile_end ("end - not local", NULL);
      return FALSE;
    }

  if (impl->update_current_folder_cancellable)
    g_cancellable_cancel (impl->update_current_folder_cancellable);

  /* Test validity of path here.  */
  data = g_new0 (struct UpdateCurrentFolderData, 1);
  data->impl = impl;
  data->file = g_object_ref (file);
  data->keep_trail = keep_trail;
  data->clear_entry = clear_entry;

  impl->reload_state = RELOAD_HAS_FOLDER;

  impl->update_current_folder_cancellable =
    _gtk_file_system_get_info (impl->file_system, file,
			       "standard::type",
			       update_current_folder_get_info_cb,
			       data);

  set_busy_cursor (impl, TRUE);
  g_object_unref (file);

  profile_end ("end", NULL);
  return TRUE;
}

static GFile *
gtk_file_chooser_default_get_current_folder (GtkFileChooser *chooser)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (chooser);

  if (impl->operation_mode == OPERATION_MODE_SEARCH ||
      impl->operation_mode == OPERATION_MODE_RECENT)
    return NULL;
 
  if (impl->current_folder)
    return g_object_ref (impl->current_folder);

  return NULL;
}

static void
gtk_file_chooser_default_set_current_name (GtkFileChooser *chooser,
					   const gchar    *name)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (chooser);

  g_return_if_fail (impl->action == GTK_FILE_CHOOSER_ACTION_SAVE ||
		    impl->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER);

  pending_select_files_free (impl);
  gtk_entry_set_text (GTK_ENTRY (impl->location_entry), name);
}

static gboolean
gtk_file_chooser_default_select_file (GtkFileChooser  *chooser,
				      GFile           *file,
				      GError         **error)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (chooser);
  GFile *parent_file;
  gboolean same_path;

  parent_file = g_file_get_parent (file);

  if (!parent_file)
    return gtk_file_chooser_set_current_folder_file (chooser, file, error);

  if (impl->operation_mode == OPERATION_MODE_SEARCH ||
      impl->operation_mode == OPERATION_MODE_RECENT ||
      impl->load_state == LOAD_EMPTY)
    {
      same_path = FALSE;
    }
  else
    {
      g_assert (impl->current_folder != NULL);

      same_path = g_file_equal (parent_file, impl->current_folder);
    }

  if (same_path && impl->load_state == LOAD_FINISHED)
    {
      gboolean result;
      GSList files;

      files.data = (gpointer) file;
      files.next = NULL;

      result = show_and_select_files (impl, &files);
      g_object_unref (parent_file);
      return result;
    }

  pending_select_files_add (impl, file);

  if (!same_path)
    {
      gboolean result;

      result = gtk_file_chooser_set_current_folder_file (chooser, parent_file, error);
      g_object_unref (parent_file);
      return result;
    }

  g_object_unref (parent_file);
  return TRUE;
}

static void
gtk_file_chooser_default_unselect_file (GtkFileChooser *chooser,
					GFile          *file)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (chooser);
  GtkTreeView *tree_view = GTK_TREE_VIEW (impl->browse_files_tree_view);
  GtkTreeIter iter;

  if (!impl->browse_files_model)
    return;

  if (!_gtk_file_system_model_get_iter_for_file (impl->browse_files_model,
                                                 &iter,
                                                 file))
    return;

  gtk_tree_selection_unselect_iter (gtk_tree_view_get_selection (tree_view),
                                    &iter);
}

static gboolean
maybe_select (GtkTreeModel *model, 
	      GtkTreePath  *path, 
	      GtkTreeIter  *iter, 
	      gpointer     data)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (data);
  GtkTreeSelection *selection;
  gboolean is_sensitive;
  gboolean is_folder;
  
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->browse_files_tree_view));
  
  gtk_tree_model_get (model, iter,
                      MODEL_COL_IS_FOLDER, &is_folder,
                      MODEL_COL_IS_SENSITIVE, &is_sensitive,
                      -1);

  if (is_sensitive &&
      ((is_folder && impl->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER) ||
       (!is_folder && impl->action == GTK_FILE_CHOOSER_ACTION_OPEN)))
    gtk_tree_selection_select_iter (selection, iter);
  else
    gtk_tree_selection_unselect_iter (selection, iter);
    
  return FALSE;
}

static void
gtk_file_chooser_default_select_all (GtkFileChooser *chooser)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (chooser);

  if (impl->operation_mode == OPERATION_MODE_SEARCH ||
      impl->operation_mode == OPERATION_MODE_RECENT)
    {
      GtkTreeSelection *selection;
      
      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->browse_files_tree_view));
      gtk_tree_selection_select_all (selection);
      return;
    }

  if (impl->select_multiple)
    gtk_tree_model_foreach (GTK_TREE_MODEL (impl->browse_files_model), 
			    maybe_select, impl);
}

static void
gtk_file_chooser_default_unselect_all (GtkFileChooser *chooser)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (chooser);
  GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->browse_files_tree_view));

  gtk_tree_selection_unselect_all (selection);
  pending_select_files_free (impl);
}

/* Checks whether the filename entry for the Save modes contains a well-formed filename.
 *
 * is_well_formed_ret - whether what the user typed passes gkt_file_system_make_path()
 *
 * is_empty_ret - whether the file entry is totally empty
 *
 * is_file_part_empty_ret - whether the file part is empty (will be if user types "foobar/", and
 *                          the path will be "$cwd/foobar")
 */
static void
check_save_entry (GtkFileChooserDefault *impl,
		  GFile                **file_ret,
		  gboolean              *is_well_formed_ret,
		  gboolean              *is_empty_ret,
		  gboolean              *is_file_part_empty_ret,
		  gboolean              *is_folder)
{
  GtkFileChooserEntry *chooser_entry;
  GFile *current_folder;
  const char *file_part;
  GFile *file;
  GError *error;

  g_assert (impl->action == GTK_FILE_CHOOSER_ACTION_SAVE
	    || impl->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER
	    || ((impl->action == GTK_FILE_CHOOSER_ACTION_OPEN
		 || impl->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER)
		&& impl->location_mode == LOCATION_MODE_FILENAME_ENTRY));

  chooser_entry = GTK_FILE_CHOOSER_ENTRY (impl->location_entry);

  if (strlen (gtk_entry_get_text (GTK_ENTRY (chooser_entry))) == 0)
    {
      *file_ret = NULL;
      *is_well_formed_ret = TRUE;
      *is_empty_ret = TRUE;
      *is_file_part_empty_ret = TRUE;
      *is_folder = FALSE;

      return;
    }

  *is_empty_ret = FALSE;

  current_folder = _gtk_file_chooser_entry_get_current_folder (chooser_entry);
  if (!current_folder)
    {
      *file_ret = NULL;
      *is_well_formed_ret = FALSE;
      *is_file_part_empty_ret = FALSE;
      *is_folder = FALSE;

      return;
    }

  file_part = _gtk_file_chooser_entry_get_file_part (chooser_entry);

  if (!file_part || file_part[0] == '\0')
    {
      *file_ret = current_folder;
      *is_well_formed_ret = TRUE;
      *is_file_part_empty_ret = TRUE;
      *is_folder = TRUE;

      return;
    }

  *is_file_part_empty_ret = FALSE;

  error = NULL;
  file = g_file_get_child_for_display_name (current_folder, file_part, &error);
  g_object_unref (current_folder);

  if (!file)
    {
      error_building_filename_dialog (impl, error);
      *file_ret = NULL;
      *is_well_formed_ret = FALSE;
      *is_folder = FALSE;

      return;
    }

  *file_ret = file;
  *is_well_formed_ret = TRUE;
  *is_folder = _gtk_file_chooser_entry_get_is_folder (chooser_entry, file);
}

struct get_files_closure {
  GtkFileChooserDefault *impl;
  GSList *result;
  GFile *file_from_entry;
};

static void
get_files_foreach (GtkTreeModel *model,
		   GtkTreePath  *path,
		   GtkTreeIter  *iter,
		   gpointer      data)
{
  struct get_files_closure *info;
  GFile *file;
  GtkFileSystemModel *fs_model;

  info = data;
  fs_model = info->impl->browse_files_model;

  file = _gtk_file_system_model_get_file (fs_model, iter);
  if (!file)
    return; /* We are on the editable row */

  if (!info->file_from_entry || !g_file_equal (info->file_from_entry, file))
    info->result = g_slist_prepend (info->result, g_object_ref (file));
}

static GSList *
gtk_file_chooser_default_get_files (GtkFileChooser *chooser)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (chooser);
  struct get_files_closure info;
  GtkWindow *toplevel;
  GtkWidget *current_focus;
  gboolean file_list_seen;

  info.impl = impl;
  info.result = NULL;
  info.file_from_entry = NULL;

  if (impl->operation_mode == OPERATION_MODE_SEARCH)
    return search_get_selected_files (impl);

  if (impl->operation_mode == OPERATION_MODE_RECENT)
    {
      if (impl->action == GTK_FILE_CHOOSER_ACTION_SAVE)
	{
	  file_list_seen = TRUE;
	  goto file_entry;
	}
      else
	return recent_get_selected_files (impl);
    }

  toplevel = get_toplevel (GTK_WIDGET (impl));
  if (toplevel)
    current_focus = gtk_window_get_focus (toplevel);
  else
    current_focus = NULL;

  file_list_seen = FALSE;
  if (current_focus == impl->browse_files_tree_view)
    {
      GtkTreeSelection *selection;

    file_list:

      file_list_seen = TRUE;
      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->browse_files_tree_view));
      gtk_tree_selection_selected_foreach (selection, get_files_foreach, &info);

      /* If there is no selection in the file list, we probably have this situation:
       *
       * 1. The user typed a filename in the SAVE filename entry ("foo.txt").
       * 2. He then double-clicked on a folder ("bar") in the file list
       *
       * So we want the selection to be "bar/foo.txt".  Jump to the case for the
       * filename entry to see if that is the case.
       */
      if (info.result == NULL && impl->location_entry)
	goto file_entry;
    }
  else if (impl->location_entry && current_focus == impl->location_entry)
    {
      gboolean is_well_formed, is_empty, is_file_part_empty, is_folder;

    file_entry:

      check_save_entry (impl, &info.file_from_entry, &is_well_formed, &is_empty, &is_file_part_empty, &is_folder);

      if (is_empty)
	goto out;

      if (!is_well_formed)
	return NULL;

      if (is_file_part_empty && impl->action == GTK_FILE_CHOOSER_ACTION_SAVE)
	{
	  g_object_unref (info.file_from_entry);
	  return NULL;
	}

      if (info.file_from_entry)
        info.result = g_slist_prepend (info.result, info.file_from_entry);
      else if (!file_list_seen) 
        goto file_list;
      else
        return NULL;
    }
  else if (impl->toplevel_last_focus_widget == impl->browse_files_tree_view)
    goto file_list;
  else if (impl->location_entry && impl->toplevel_last_focus_widget == impl->location_entry)
    goto file_entry;
  else
    {
      /* The focus is on a dialog's action area button or something else */
      if (impl->action == GTK_FILE_CHOOSER_ACTION_SAVE ||
          impl->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER)
	goto file_entry;
      else
	goto file_list; 
    }

 out:

  /* If there's no folder selected, and we're in SELECT_FOLDER mode, then we
   * fall back to the current directory */
  if (impl->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER &&
      info.result == NULL)
    {
      GFile *current_folder;

      current_folder = gtk_file_chooser_get_current_folder_file (chooser);

      if (current_folder)
        info.result = g_slist_prepend (info.result, current_folder);
    }

  return g_slist_reverse (info.result);
}

GFile *
gtk_file_chooser_default_get_preview_file (GtkFileChooser *chooser)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (chooser);

  if (impl->preview_file)
    return g_object_ref (impl->preview_file);
  else
    return NULL;
}

static GtkFileSystem *
gtk_file_chooser_default_get_file_system (GtkFileChooser *chooser)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (chooser);

  return impl->file_system;
}

/* Shows or hides the filter widgets */
static void
show_filters (GtkFileChooserDefault *impl,
	      gboolean               show)
{
  if (show)
    gtk_widget_show (impl->filter_combo_hbox);
  else
    gtk_widget_hide (impl->filter_combo_hbox);
}

static void
gtk_file_chooser_default_add_filter (GtkFileChooser *chooser,
				     GtkFileFilter  *filter)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (chooser);
  const gchar *name;

  if (g_slist_find (impl->filters, filter))
    {
      g_warning ("gtk_file_chooser_add_filter() called on filter already in list\n");
      return;
    }

  g_object_ref_sink (filter);
  impl->filters = g_slist_append (impl->filters, filter);

  name = gtk_file_filter_get_name (filter);
  if (!name)
    name = "Untitled filter";	/* Place-holder, doesn't need to be marked for translation */

  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (impl->filter_combo), name);

  if (!g_slist_find (impl->filters, impl->current_filter))
    set_current_filter (impl, filter);

  show_filters (impl, TRUE);
}

static void
gtk_file_chooser_default_remove_filter (GtkFileChooser *chooser,
					GtkFileFilter  *filter)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (chooser);
  GtkTreeModel *model;
  GtkTreeIter iter;
  gint filter_index;

  filter_index = g_slist_index (impl->filters, filter);

  if (filter_index < 0)
    {
      g_warning ("gtk_file_chooser_remove_filter() called on filter not in list\n");
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

  /* Remove row from the combo box */
  model = gtk_combo_box_get_model (GTK_COMBO_BOX (impl->filter_combo));
  if (!gtk_tree_model_iter_nth_child  (model, &iter, NULL, filter_index))
    g_assert_not_reached ();

  gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

  g_object_unref (filter);

  if (!impl->filters)
    show_filters (impl, FALSE);
}

static GSList *
gtk_file_chooser_default_list_filters (GtkFileChooser *chooser)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (chooser);

  return g_slist_copy (impl->filters);
}

/* Returns the position in the shortcuts tree where the nth specified shortcut would appear */
static int
shortcuts_get_pos_for_shortcut_folder (GtkFileChooserDefault *impl,
				       int                    pos)
{
  return pos + shortcuts_get_index (impl, SHORTCUTS_SHORTCUTS);
}

struct AddShortcutData
{
  GtkFileChooserDefault *impl;
  GFile *file;
};

static void
add_shortcut_get_info_cb (GCancellable *cancellable,
			  GFileInfo    *info,
			  const GError *error,
			  gpointer      user_data)
{
  int pos;
  gboolean cancelled = g_cancellable_is_cancelled (cancellable);
  struct AddShortcutData *data = user_data;

  if (!g_slist_find (data->impl->loading_shortcuts, cancellable))
    goto out;

  data->impl->loading_shortcuts = g_slist_remove (data->impl->loading_shortcuts, cancellable);

  if (cancelled || error || (! _gtk_file_info_consider_as_directory (info)))
    goto out;

  pos = shortcuts_get_pos_for_shortcut_folder (data->impl, data->impl->num_shortcuts);

  shortcuts_insert_file (data->impl, pos, SHORTCUT_TYPE_FILE, NULL, data->file, NULL, FALSE, SHORTCUTS_SHORTCUTS);

  /* need to call shortcuts_add_bookmarks to flush out any duplicates bug #577806 */
  shortcuts_add_bookmarks (data->impl);

out:
  g_object_unref (data->impl);
  g_object_unref (data->file);
  g_free (data);

  g_object_unref (cancellable);
}

static gboolean
gtk_file_chooser_default_add_shortcut_folder (GtkFileChooser  *chooser,
					      GFile           *file,
					      GError         **error)
{
  GCancellable *cancellable;
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (chooser);
  struct AddShortcutData *data;
  GSList *l;
  int pos;

  /* Avoid adding duplicates */
  pos = shortcut_find_position (impl, file);
  if (pos >= 0 && pos < shortcuts_get_index (impl, SHORTCUTS_BOOKMARKS_SEPARATOR))
    {
      gchar *uri;

      uri = g_file_get_uri (file);
      /* translators, "Shortcut" means "Bookmark" here */
      g_set_error (error,
		   GTK_FILE_CHOOSER_ERROR,
		   GTK_FILE_CHOOSER_ERROR_ALREADY_EXISTS,
		   _("Shortcut %s already exists"),
		   uri);
      g_free (uri);

      return FALSE;
    }

  for (l = impl->loading_shortcuts; l; l = l->next)
    {
      GCancellable *c = l->data;
      GFile *f;

      f = g_object_get_data (G_OBJECT (c), "add-shortcut-path-key");
      if (f && g_file_equal (file, f))
        {
	  gchar *uri;

	  uri = g_file_get_uri (file);
          g_set_error (error,
		       GTK_FILE_CHOOSER_ERROR,
		       GTK_FILE_CHOOSER_ERROR_ALREADY_EXISTS,
		       _("Shortcut %s already exists"),
		       uri);
          g_free (uri);

          return FALSE;
	}
    }

  data = g_new0 (struct AddShortcutData, 1);
  data->impl = g_object_ref (impl);
  data->file = g_object_ref (file);

  cancellable = _gtk_file_system_get_info (impl->file_system, file,
					   "standard::type",
					   add_shortcut_get_info_cb, data);

  if (!cancellable)
    return FALSE;

  impl->loading_shortcuts = g_slist_append (impl->loading_shortcuts, cancellable);
  g_object_set_data (G_OBJECT (cancellable), "add-shortcut-path-key", data->file);

  return TRUE;
}

static gboolean
gtk_file_chooser_default_remove_shortcut_folder (GtkFileChooser  *chooser,
						 GFile           *file,
						 GError         **error)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (chooser);
  int pos;
  GtkTreeIter iter;
  GSList *l;
  char *uri;
  int i;

  for (l = impl->loading_shortcuts; l; l = l->next)
    {
      GCancellable *c = l->data;
      GFile *f;

      f = g_object_get_data (G_OBJECT (c), "add-shortcut-path-key");
      if (f && g_file_equal (file, f))
        {
	  impl->loading_shortcuts = g_slist_remove (impl->loading_shortcuts, c);
	  g_cancellable_cancel (c);
          return TRUE;
	}
    }

  if (impl->num_shortcuts == 0)
    goto out;

  pos = shortcuts_get_pos_for_shortcut_folder (impl, 0);
  if (!gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (impl->shortcuts_model), &iter, NULL, pos))
    g_assert_not_reached ();

  for (i = 0; i < impl->num_shortcuts; i++)
    {
      gpointer col_data;
      ShortcutType shortcut_type;
      GFile *shortcut;

      gtk_tree_model_get (GTK_TREE_MODEL (impl->shortcuts_model), &iter,
			  SHORTCUTS_COL_DATA, &col_data,
			  SHORTCUTS_COL_TYPE, &shortcut_type,
			  -1);
      g_assert (col_data != NULL);
      g_assert (shortcut_type == SHORTCUT_TYPE_FILE);

      shortcut = col_data;
      if (g_file_equal (shortcut, file))
	{
	  shortcuts_remove_rows (impl, pos + i, 1);
	  impl->num_shortcuts--;
	  return TRUE;
	}

      if (!gtk_tree_model_iter_next (GTK_TREE_MODEL (impl->shortcuts_model), &iter))
	g_assert_not_reached ();
    }

 out:

  uri = g_file_get_uri (file);
  /* translators, "Shortcut" means "Bookmark" here */
  g_set_error (error,
	       GTK_FILE_CHOOSER_ERROR,
	       GTK_FILE_CHOOSER_ERROR_NONEXISTENT,
	       _("Shortcut %s does not exist"),
	       uri);
  g_free (uri);

  return FALSE;
}

static GSList *
gtk_file_chooser_default_list_shortcut_folders (GtkFileChooser *chooser)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (chooser);
  int pos;
  GtkTreeIter iter;
  int i;
  GSList *list;

  if (impl->num_shortcuts == 0)
    return NULL;

  pos = shortcuts_get_pos_for_shortcut_folder (impl, 0);
  if (!gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (impl->shortcuts_model), &iter, NULL, pos))
    g_assert_not_reached ();

  list = NULL;

  for (i = 0; i < impl->num_shortcuts; i++)
    {
      gpointer col_data;
      ShortcutType shortcut_type;
      GFile *shortcut;

      gtk_tree_model_get (GTK_TREE_MODEL (impl->shortcuts_model), &iter,
			  SHORTCUTS_COL_DATA, &col_data,
			  SHORTCUTS_COL_TYPE, &shortcut_type,
			  -1);
      g_assert (col_data != NULL);
      g_assert (shortcut_type == SHORTCUT_TYPE_FILE);

      shortcut = col_data;
      list = g_slist_prepend (list, g_object_ref (shortcut));

      if (i != impl->num_shortcuts - 1)
	{
	  if (!gtk_tree_model_iter_next (GTK_TREE_MODEL (impl->shortcuts_model), &iter))
	    g_assert_not_reached ();
	}
    }

  return g_slist_reverse (list);
}

/* Guesses a size based upon font sizes */
static void
find_good_size_from_style (GtkWidget *widget,
			   gint      *width,
			   gint      *height)
{
  int font_size;
  GdkScreen *screen;
  double resolution;

  g_assert (widget->style != NULL);

  screen = gtk_widget_get_screen (widget);
  if (screen)
    {
      resolution = gdk_screen_get_resolution (screen);
      if (resolution < 0.0) /* will be -1 if the resolution is not defined in the GdkScreen */
	resolution = 96.0;
    }
  else
    resolution = 96.0; /* wheeee */

  font_size = pango_font_description_get_size (widget->style->font_desc);
  font_size = PANGO_PIXELS (font_size) * resolution / 72.0;

  *width = font_size * NUM_CHARS;
  *height = font_size * NUM_LINES;
}

static void
gtk_file_chooser_default_get_default_size (GtkFileChooserEmbed *chooser_embed,
					   gint                *default_width,
					   gint                *default_height)
{
  GtkFileChooserDefault *impl;
  GtkRequisition req;
  GtkFileChooserSettings *settings;
  int x, y, width, height;

  impl = GTK_FILE_CHOOSER_DEFAULT (chooser_embed);

  settings = _gtk_file_chooser_settings_new ();
  _gtk_file_chooser_settings_get_geometry (settings, &x, &y, &width, &height);
  g_object_unref (settings);

  if (x >= 0 && y >= 0 && width > 0 && height > 0)
    {
      *default_width = width;
      *default_height = height;
      return;
    }

  find_good_size_from_style (GTK_WIDGET (chooser_embed), default_width, default_height);

  if (impl->preview_widget_active &&
      impl->preview_widget &&
      gtk_widget_get_visible (impl->preview_widget))
    {
      gtk_widget_size_request (impl->preview_box, &req);
      *default_width += PREVIEW_HBOX_SPACING + req.width;
    }

  if (impl->extra_widget &&
      gtk_widget_get_visible (impl->extra_widget))
    {
      gtk_widget_size_request (impl->extra_align, &req);
      *default_height += gtk_box_get_spacing (GTK_BOX (chooser_embed)) + req.height;
    }
}

struct switch_folder_closure {
  GtkFileChooserDefault *impl;
  GFile *file;
  int num_selected;
};

/* Used from gtk_tree_selection_selected_foreach() in switch_to_selected_folder() */
static void
switch_folder_foreach_cb (GtkTreeModel      *model,
			  GtkTreePath       *path,
			  GtkTreeIter       *iter,
			  gpointer           data)
{
  struct switch_folder_closure *closure;

  closure = data;

  closure->file = _gtk_file_system_model_get_file (closure->impl->browse_files_model, iter);
  closure->num_selected++;
}

/* Changes to the selected folder in the list view */
static void
switch_to_selected_folder (GtkFileChooserDefault *impl)
{
  GtkTreeSelection *selection;
  struct switch_folder_closure closure;

  /* We do this with foreach() rather than get_selected() as we may be in
   * multiple selection mode
   */

  closure.impl = impl;
  closure.file = NULL;
  closure.num_selected = 0;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->browse_files_tree_view));
  gtk_tree_selection_selected_foreach (selection, switch_folder_foreach_cb, &closure);

  g_assert (closure.file && closure.num_selected == 1);

  change_folder_and_display_error (impl, closure.file, FALSE);
}

/* Gets the GFileInfo for the selected row in the file list; assumes single
 * selection mode.
 */
static GFileInfo *
get_selected_file_info_from_file_list (GtkFileChooserDefault *impl,
				       gboolean              *had_selection)
{
  GtkTreeSelection *selection;
  GtkTreeIter iter;
  GFileInfo *info;

  g_assert (!impl->select_multiple);
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->browse_files_tree_view));
  if (!gtk_tree_selection_get_selected (selection, NULL, &iter))
    {
      *had_selection = FALSE;
      return NULL;
    }

  *had_selection = TRUE;

  info = _gtk_file_system_model_get_info (impl->browse_files_model, &iter);
  return info;
}

/* Gets the display name of the selected file in the file list; assumes single
 * selection mode and that something is selected.
 */
static const gchar *
get_display_name_from_file_list (GtkFileChooserDefault *impl)
{
  GFileInfo *info;
  gboolean had_selection;

  info = get_selected_file_info_from_file_list (impl, &had_selection);
  g_assert (had_selection);
  g_assert (info != NULL);

  return g_file_info_get_display_name (info);
}

static void
add_custom_button_to_dialog (GtkDialog   *dialog,
			     const gchar *mnemonic_label,
			     const gchar *stock_id,
			     gint         response_id)
{
  GtkWidget *button;

  button = gtk_button_new_with_mnemonic (mnemonic_label);
  gtk_widget_set_can_default (button, TRUE);
  gtk_button_set_image (GTK_BUTTON (button),
			gtk_image_new_from_stock (stock_id, GTK_ICON_SIZE_BUTTON));
  gtk_widget_show (button);

  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, response_id);
}

/* Presents an overwrite confirmation dialog; returns whether we should accept
 * the filename.
 */
static gboolean
confirm_dialog_should_accept_filename (GtkFileChooserDefault *impl,
				       const gchar           *file_part,
				       const gchar           *folder_display_name)
{
  GtkWindow *toplevel;
  GtkWidget *dialog;
  int response;

  toplevel = get_toplevel (GTK_WIDGET (impl));

  dialog = gtk_message_dialog_new (toplevel,
				   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				   GTK_MESSAGE_QUESTION,
				   GTK_BUTTONS_NONE,
				   _("A file named \"%s\" already exists.  Do you want to replace it?"),
				   file_part);
  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
					    _("The file already exists in \"%s\".  Replacing it will "
					      "overwrite its contents."),
					    folder_display_name);

  gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
  add_custom_button_to_dialog (GTK_DIALOG (dialog), _("_Replace"),
                               GTK_STOCK_SAVE_AS, GTK_RESPONSE_ACCEPT);
  gtk_dialog_set_alternative_button_order (GTK_DIALOG (dialog),
                                           GTK_RESPONSE_ACCEPT,
                                           GTK_RESPONSE_CANCEL,
                                           -1);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);

  if (gtk_window_has_group (toplevel))
    gtk_window_group_add_window (gtk_window_get_group (toplevel),
                                 GTK_WINDOW (dialog));

  response = gtk_dialog_run (GTK_DIALOG (dialog));

  gtk_widget_destroy (dialog);

  return (response == GTK_RESPONSE_ACCEPT);
}

struct GetDisplayNameData
{
  GtkFileChooserDefault *impl;
  gchar *file_part;
};

/* Every time we request a response explicitly, we need to save the selection to the recently-used list,
 * as requesting a response means, "the dialog is confirmed".
 */
static void
request_response_and_add_to_recent_list (GtkFileChooserDefault *impl)
{
  g_signal_emit_by_name (impl, "response-requested");
  add_selection_to_recent_list (impl);
}

static void
confirmation_confirm_get_info_cb (GCancellable *cancellable,
				  GFileInfo    *info,
				  const GError *error,
				  gpointer      user_data)
{
  gboolean cancelled = g_cancellable_is_cancelled (cancellable);
  gboolean should_respond = FALSE;
  struct GetDisplayNameData *data = user_data;

  if (cancellable != data->impl->should_respond_get_info_cancellable)
    goto out;

  data->impl->should_respond_get_info_cancellable = NULL;

  if (cancelled)
    goto out;

  if (error)
    /* Huh?  Did the folder disappear?  Let the caller deal with it */
    should_respond = TRUE;
  else
    should_respond = confirm_dialog_should_accept_filename (data->impl, data->file_part, g_file_info_get_display_name (info));

  set_busy_cursor (data->impl, FALSE);
  if (should_respond)
    request_response_and_add_to_recent_list (data->impl);

out:
  g_object_unref (data->impl);
  g_free (data->file_part);
  g_free (data);

  g_object_unref (cancellable);
}

/* Does overwrite confirmation if appropriate, and returns whether the dialog
 * should respond.  Can get the file part from the file list or the save entry.
 */
static gboolean
should_respond_after_confirm_overwrite (GtkFileChooserDefault *impl,
					const gchar           *file_part,
					GFile                 *parent_file)
{
  GtkFileChooserConfirmation conf;

  if (!impl->do_overwrite_confirmation)
    return TRUE;

  conf = GTK_FILE_CHOOSER_CONFIRMATION_CONFIRM;

  g_signal_emit_by_name (impl, "confirm-overwrite", &conf);

  switch (conf)
    {
    case GTK_FILE_CHOOSER_CONFIRMATION_CONFIRM:
      {
	struct GetDisplayNameData *data;

	g_assert (file_part != NULL);

	data = g_new0 (struct GetDisplayNameData, 1);
	data->impl = g_object_ref (impl);
	data->file_part = g_strdup (file_part);

	if (impl->should_respond_get_info_cancellable)
	  g_cancellable_cancel (impl->should_respond_get_info_cancellable);

	impl->should_respond_get_info_cancellable =
	  _gtk_file_system_get_info (impl->file_system, parent_file,
				     "standard::display-name",
				     confirmation_confirm_get_info_cb,
				     data);
	set_busy_cursor (data->impl, TRUE);
	return FALSE;
      }

    case GTK_FILE_CHOOSER_CONFIRMATION_ACCEPT_FILENAME:
      return TRUE;

    case GTK_FILE_CHOOSER_CONFIRMATION_SELECT_AGAIN:
      return FALSE;

    default:
      g_assert_not_reached ();
      return FALSE;
    }
}

struct FileExistsData
{
  GtkFileChooserDefault *impl;
  gboolean file_exists_and_is_not_folder;
  GFile *parent_file;
  GFile *file;
};

static void
name_entry_get_parent_info_cb (GCancellable *cancellable,
			       GFileInfo    *info,
			       const GError *error,
			       gpointer      user_data)
{
  gboolean parent_is_folder;
  gboolean cancelled = g_cancellable_is_cancelled (cancellable);
  struct FileExistsData *data = user_data;

  if (cancellable != data->impl->should_respond_get_info_cancellable)
    goto out;

  data->impl->should_respond_get_info_cancellable = NULL;

  set_busy_cursor (data->impl, FALSE);

  if (cancelled)
    goto out;

  if (!info)
    parent_is_folder = FALSE;
  else
    parent_is_folder = _gtk_file_info_consider_as_directory (info);

  if (parent_is_folder)
    {
      if (data->impl->action == GTK_FILE_CHOOSER_ACTION_OPEN)
	{
	  request_response_and_add_to_recent_list (data->impl); /* even if the file doesn't exist, apps can make good use of that (e.g. Emacs) */
	}
      else if (data->impl->action == GTK_FILE_CHOOSER_ACTION_SAVE)
        {
          if (data->file_exists_and_is_not_folder)
	    {
	      gboolean retval;
	      char *file_part;

              /* Dup the string because the string may be modified
               * depending on what clients do in the confirm-overwrite
               * signal and this corrupts the pointer
               */
              file_part = g_strdup (_gtk_file_chooser_entry_get_file_part (GTK_FILE_CHOOSER_ENTRY (data->impl->location_entry)));
	      retval = should_respond_after_confirm_overwrite (data->impl, file_part, data->parent_file);
              g_free (file_part);

	      if (retval)
		request_response_and_add_to_recent_list (data->impl);
	    }
	  else
	    request_response_and_add_to_recent_list (data->impl);
	}
      else if (data->impl->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER
	       || data->impl->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER)
        {
	  GError *mkdir_error = NULL;

	  /* In both cases (SELECT_FOLDER and CREATE_FOLDER), if you type
	   * "/blah/nonexistent" you *will* want a folder created.
	   */

	  set_busy_cursor (data->impl, TRUE);
	  g_file_make_directory (data->file, NULL, &mkdir_error);
	  set_busy_cursor (data->impl, FALSE);

	  if (!mkdir_error)
	    request_response_and_add_to_recent_list (data->impl);
	  else
	    error_creating_folder_dialog (data->impl, data->file, mkdir_error);
        }
      else
	g_assert_not_reached ();
    }
  else
    {
      if (info)
	{
	  /* The parent exists, but it's not a folder!  Someone probably typed existing_file.txt/subfile.txt */
	  error_with_file_under_nonfolder (data->impl, data->parent_file);
	}
      else
	{
	  GError *error_copy;

	  /* The parent folder is not readable for some reason */

	  error_copy = g_error_copy (error);
	  error_changing_folder_dialog (data->impl, data->parent_file, error_copy);
	}
    }

out:
  g_object_unref (data->impl);
  g_object_unref (data->file);
  g_object_unref (data->parent_file);
  g_free (data);

  g_object_unref (cancellable);
}

static void
file_exists_get_info_cb (GCancellable *cancellable,
			 GFileInfo    *info,
			 const GError *error,
			 gpointer      user_data)
{
  gboolean data_ownership_taken = FALSE;
  gboolean cancelled = g_cancellable_is_cancelled (cancellable);
  gboolean file_exists;
  gboolean is_folder;
  gboolean needs_parent_check = FALSE;
  struct FileExistsData *data = user_data;

  if (cancellable != data->impl->file_exists_get_info_cancellable)
    goto out;

  data->impl->file_exists_get_info_cancellable = NULL;

  set_busy_cursor (data->impl, FALSE);

  if (cancelled)
    goto out;

  file_exists = (info != NULL);
  is_folder = (file_exists && _gtk_file_info_consider_as_directory (info));

  if (data->impl->action == GTK_FILE_CHOOSER_ACTION_OPEN)
    {
      if (is_folder)
	change_folder_and_display_error (data->impl, data->file, TRUE);
      else
	{
	  if (file_exists)
	    request_response_and_add_to_recent_list (data->impl); /* user typed an existing filename; we are done */
	  else
	    needs_parent_check = TRUE; /* file doesn't exist; see if its parent exists */
	}
    }
  else if (data->impl->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER)
    {
      if (file_exists && !is_folder)
        {
          /* Oops, the user typed the name of an existing path which is not
           * a folder
           */
          error_creating_folder_over_existing_file_dialog (data->impl, data->file,
						           g_error_copy (error));
        }
      else
        {
          needs_parent_check = TRUE;
        }
    }
  else if (data->impl->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER)
    {
      if (!file_exists)
        {
	  needs_parent_check = TRUE;
        }
      else
	{
	  if (is_folder)
	    {
	      /* User typed a folder; we are done */
	      request_response_and_add_to_recent_list (data->impl);
	    }
	  else
	    error_selecting_folder_over_existing_file_dialog (data->impl, data->file);
	}
    }
  else if (data->impl->action == GTK_FILE_CHOOSER_ACTION_SAVE)
    {
      if (is_folder)
	change_folder_and_display_error (data->impl, data->file, TRUE);
      else
	needs_parent_check = TRUE;
    }
  else
    {
      g_assert_not_reached();
    }

  if (needs_parent_check)
    {
      /* check that everything up to the last path component exists (i.e. the parent) */

      data->file_exists_and_is_not_folder = file_exists && !is_folder;
      data_ownership_taken = TRUE;

      if (data->impl->should_respond_get_info_cancellable)
        g_cancellable_cancel (data->impl->should_respond_get_info_cancellable);

      data->impl->should_respond_get_info_cancellable =
	_gtk_file_system_get_info (data->impl->file_system,
				   data->parent_file,
				   "standard::type",
				   name_entry_get_parent_info_cb,
				   data);
      set_busy_cursor (data->impl, TRUE);
    }

out:
  if (!data_ownership_taken)
    {
      g_object_unref (data->impl);
      g_object_unref (data->file);
      g_object_unref (data->parent_file);
      g_free (data);
    }

  g_object_unref (cancellable);
}

static void
paste_text_received (GtkClipboard          *clipboard,
		     const gchar           *text,
		     GtkFileChooserDefault *impl)
{
  GFile *file;

  if (!text)
    return;

  file = g_file_new_for_uri (text);

  if (!gtk_file_chooser_default_select_file (GTK_FILE_CHOOSER (impl), file, NULL))
    location_popup_handler (impl, text);

  g_object_unref (file);
}

/* Handler for the "location-popup-on-paste" keybinding signal */
static void
location_popup_on_paste_handler (GtkFileChooserDefault *impl)
{
  GtkClipboard *clipboard = gtk_widget_get_clipboard (GTK_WIDGET (impl),
		  				      GDK_SELECTION_CLIPBOARD);
  gtk_clipboard_request_text (clipboard,
		  	      (GtkClipboardTextReceivedFunc) paste_text_received,
			      impl);
}

/* Implementation for GtkFileChooserEmbed::should_respond() */
static void
add_selection_to_recent_list (GtkFileChooserDefault *impl)
{
  GSList *files;
  GSList *l;

  files = gtk_file_chooser_default_get_files (GTK_FILE_CHOOSER (impl));

  for (l = files; l; l = l->next)
    {
      GFile *file = l->data;
      char *uri;

      uri = g_file_get_uri (file);
      if (uri)
	{
	  gtk_recent_manager_add_item (impl->recent_manager, uri);
	  g_free (uri);
	}
    }

  g_slist_foreach (files, (GFunc) g_object_unref, NULL);
  g_slist_free (files);
}

static gboolean
gtk_file_chooser_default_should_respond (GtkFileChooserEmbed *chooser_embed)
{
  GtkFileChooserDefault *impl;
  GtkWidget *toplevel;
  GtkWidget *current_focus;
  gboolean retval;

  impl = GTK_FILE_CHOOSER_DEFAULT (chooser_embed);

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (impl));
  g_assert (GTK_IS_WINDOW (toplevel));

  retval = FALSE;

  current_focus = gtk_window_get_focus (GTK_WINDOW (toplevel));

  if (current_focus == impl->browse_files_tree_view)
    {
      /* The following array encodes what we do based on the impl->action and the
       * number of files selected.
       */
      typedef enum {
	NOOP,			/* Do nothing (don't respond) */
	RESPOND,		/* Respond immediately */
	RESPOND_OR_SWITCH,      /* Respond immediately if the selected item is a file; switch to it if it is a folder */
	ALL_FILES,		/* Respond only if everything selected is a file */
	ALL_FOLDERS,		/* Respond only if everything selected is a folder */
	SAVE_ENTRY,		/* Go to the code for handling the save entry */
	NOT_REACHED		/* Sanity check */
      } ActionToTake;
      static const ActionToTake what_to_do[4][3] = {
	/*				  0 selected		1 selected		many selected */
	/* ACTION_OPEN */		{ NOOP,			RESPOND_OR_SWITCH,	ALL_FILES   },
	/* ACTION_SAVE */		{ SAVE_ENTRY,		RESPOND_OR_SWITCH,	NOT_REACHED },
	/* ACTION_SELECT_FOLDER */	{ RESPOND,		ALL_FOLDERS,		ALL_FOLDERS },
	/* ACTION_CREATE_FOLDER */	{ SAVE_ENTRY,		ALL_FOLDERS,		NOT_REACHED }
      };

      int num_selected;
      gboolean all_files, all_folders;
      int k;
      ActionToTake action;

    file_list:

      g_assert (impl->action >= GTK_FILE_CHOOSER_ACTION_OPEN && impl->action <= GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER);

      if (impl->operation_mode == OPERATION_MODE_SEARCH)
	{
	  retval = search_should_respond (impl);
	  goto out;
	}

      if (impl->operation_mode == OPERATION_MODE_RECENT)
	{
	  if (impl->action == GTK_FILE_CHOOSER_ACTION_SAVE)
	    goto save_entry;
	  else
	    {
	      retval = recent_should_respond (impl);
	      goto out;
	    }
	}

      selection_check (impl, &num_selected, &all_files, &all_folders);

      if (num_selected > 2)
	k = 2;
      else
	k = num_selected;

      action = what_to_do [impl->action] [k];

      switch (action)
	{
	case NOOP:
	  return FALSE;

	case RESPOND:
	  retval = TRUE;
	  goto out;

	case RESPOND_OR_SWITCH:
	  g_assert (num_selected == 1);

	  if (all_folders)
	    {
	      switch_to_selected_folder (impl);
	      return FALSE;
	    }
	  else if (impl->action == GTK_FILE_CHOOSER_ACTION_SAVE)
	    {
	      retval = should_respond_after_confirm_overwrite (impl,
							       get_display_name_from_file_list (impl),
							       impl->current_folder);
	      goto out;
	    }
	  else
	    {
	      retval = TRUE;
	      goto out;
	    }

	case ALL_FILES:
	  retval = all_files;
	  goto out;

	case ALL_FOLDERS:
	  retval = all_folders;
	  goto out;

	case SAVE_ENTRY:
	  goto save_entry;

	default:
	  g_assert_not_reached ();
	}
    }
  else if ((impl->location_entry != NULL) && (current_focus == impl->location_entry))
    {
      GFile *file;
      gboolean is_well_formed, is_empty, is_file_part_empty;
      gboolean is_folder;
      GtkFileChooserEntry *entry;
      GError *error;

    save_entry:

      g_assert (impl->action == GTK_FILE_CHOOSER_ACTION_SAVE
		|| impl->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER
		|| ((impl->action == GTK_FILE_CHOOSER_ACTION_OPEN
		     || impl->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER)
		    && impl->location_mode == LOCATION_MODE_FILENAME_ENTRY));

      entry = GTK_FILE_CHOOSER_ENTRY (impl->location_entry);
      check_save_entry (impl, &file, &is_well_formed, &is_empty, &is_file_part_empty, &is_folder);

      if (!is_well_formed)
	{
	  if (!is_empty
	      && impl->action == GTK_FILE_CHOOSER_ACTION_SAVE
	      && impl->operation_mode == OPERATION_MODE_RECENT)
	    {
	      path_bar_set_mode (impl, PATH_BAR_ERROR_NO_FOLDER);
#if 0
	      /* We'll #ifdef this out, as the fucking treeview selects its first row,
	       * thus changing our assumption that no selection is present - setting
	       * a selection causes the error message from path_bar_set_mode() to go away,
	       * but we want the user to see that message!
	       */
	      gtk_widget_grab_focus (impl->browse_files_tree_view);
#endif
	    }
	  /* FIXME: else show an "invalid filename" error as the pathbar mode? */

	  return FALSE;
	}

      if (is_empty)
        {
          if (impl->action == GTK_FILE_CHOOSER_ACTION_SAVE
	      || impl->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER)
	    {
	      path_bar_set_mode (impl, PATH_BAR_ERROR_NO_FILENAME);
	      gtk_widget_grab_focus (impl->location_entry);
	      return FALSE;
	    }

          goto file_list;
        }

      g_assert (file != NULL);

      error = NULL;
      if (is_folder)
	{
	  if (impl->action == GTK_FILE_CHOOSER_ACTION_OPEN ||
	      impl->action == GTK_FILE_CHOOSER_ACTION_SAVE)
	    {
	      change_folder_and_display_error (impl, file, TRUE);
	    }
	  else if (impl->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER ||
		   impl->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER)
	    {
	      /* The folder already exists, so we do not need to create it.
	       * Just respond to terminate the dialog.
	       */
	      retval = TRUE;
	    }
	  else
	    {
	      g_assert_not_reached ();
	    }
	}
      else
	{
	  struct FileExistsData *data;

	  /* We need to check whether file exists and whether it is a folder -
	   * the GtkFileChooserEntry *does* report is_folder==FALSE as a false
	   * negative (it doesn't know yet if your last path component is a
	   * folder).
	   */

	  data = g_new0 (struct FileExistsData, 1);
	  data->impl = g_object_ref (impl);
	  data->file = g_object_ref (file);
	  data->parent_file = _gtk_file_chooser_entry_get_current_folder (entry);

	  if (impl->file_exists_get_info_cancellable)
	    g_cancellable_cancel (impl->file_exists_get_info_cancellable);

	  impl->file_exists_get_info_cancellable =
	    _gtk_file_system_get_info (impl->file_system, file,
				       "standard::type",
				       file_exists_get_info_cb,
				       data);

	  set_busy_cursor (impl, TRUE);

	  if (error != NULL)
	    g_error_free (error);
	}

      g_object_unref (file);
    }
  else if (impl->toplevel_last_focus_widget == impl->browse_files_tree_view)
    {
      /* The focus is on a dialog's action area button, *and* the widget that
       * was focused immediately before it is the file list.  
       */
      goto file_list;
    }
  else if (impl->operation_mode == OPERATION_MODE_SEARCH && impl->toplevel_last_focus_widget == impl->search_entry)
    {
      search_entry_activate_cb (GTK_ENTRY (impl->search_entry), impl);
      return FALSE;
    }
  else if (impl->location_entry && impl->toplevel_last_focus_widget == impl->location_entry)
    {
      /* The focus is on a dialog's action area button, *and* the widget that
       * was focused immediately before it is the location entry.
       */
      goto save_entry;
    }
  else
    /* The focus is on a dialog's action area button or something else */
    if (impl->action == GTK_FILE_CHOOSER_ACTION_SAVE
	|| impl->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER)
      goto save_entry;
    else
      goto file_list; 

 out:

  if (retval)
    add_selection_to_recent_list (impl);

  return retval;
}

/* Implementation for GtkFileChooserEmbed::initial_focus() */
static void
gtk_file_chooser_default_initial_focus (GtkFileChooserEmbed *chooser_embed)
{
  GtkFileChooserDefault *impl;
  GtkWidget *widget;

  impl = GTK_FILE_CHOOSER_DEFAULT (chooser_embed);

  if (impl->action == GTK_FILE_CHOOSER_ACTION_OPEN ||
      impl->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER)
    {
      if (impl->location_mode == LOCATION_MODE_PATH_BAR
	  || impl->operation_mode == OPERATION_MODE_RECENT)
	widget = impl->browse_files_tree_view;
      else
	widget = impl->location_entry;
    }
  else if (impl->action == GTK_FILE_CHOOSER_ACTION_SAVE ||
	   impl->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER)
    widget = impl->location_entry;
  else
    {
      g_assert_not_reached ();
      widget = NULL;
    }

  g_assert (widget != NULL);
  gtk_widget_grab_focus (widget);
}

/* Callback used from gtk_tree_selection_selected_foreach(); gets the selected GFiles */
static void
search_selected_foreach_get_file_cb (GtkTreeModel *model,
				     GtkTreePath  *path,
				     GtkTreeIter  *iter,
				     gpointer      data)
{
  GSList **list;
  GFile *file;

  list = data;

  gtk_tree_model_get (model, iter, MODEL_COL_FILE, &file, -1);
  *list = g_slist_prepend (*list, g_object_ref (file));
}

/* Constructs a list of the selected paths in search mode */
static GSList *
search_get_selected_files (GtkFileChooserDefault *impl)
{
  GSList *result;
  GtkTreeSelection *selection;

  result = NULL;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->browse_files_tree_view));
  gtk_tree_selection_selected_foreach (selection, search_selected_foreach_get_file_cb, &result);
  result = g_slist_reverse (result);

  return result;
}

/* Called from ::should_respond().  We return whether there are selected files
 * in the search list.
 */
static gboolean
search_should_respond (GtkFileChooserDefault *impl)
{
  GtkTreeSelection *selection;

  g_assert (impl->operation_mode == OPERATION_MODE_SEARCH);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->browse_files_tree_view));
  return (gtk_tree_selection_count_selected_rows (selection) != 0);
}

/* Adds one hit from the search engine to the search_model */
static void
search_add_hit (GtkFileChooserDefault *impl,
		gchar                 *uri)
{
  GFile *file;

  file = g_file_new_for_uri (uri);
  if (!file)
    return;

  if (!g_file_is_native (file))
    {
      g_object_unref (file);
      return;
    }

  _gtk_file_system_model_add_and_query_file (impl->search_model,
                                             file,
                                             MODEL_ATTRIBUTES);

  g_object_unref (file);
}

/* Callback used from GtkSearchEngine when we get new hits */
static void
search_engine_hits_added_cb (GtkSearchEngine *engine,
			     GList           *hits,
			     gpointer         data)
{
  GtkFileChooserDefault *impl;
  GList *l;
  
  impl = GTK_FILE_CHOOSER_DEFAULT (data);

  for (l = hits; l; l = l->next)
    search_add_hit (impl, (gchar*)l->data);
}

/* Callback used from GtkSearchEngine when the query is done running */
static void
search_engine_finished_cb (GtkSearchEngine *engine,
			   gpointer         data)
{
  GtkFileChooserDefault *impl;
  
  impl = GTK_FILE_CHOOSER_DEFAULT (data);
  
#if 0
  /* EB: setting the model here will avoid loads of row events,
   * but it'll make the search look like blocked.
   */
  gtk_tree_view_set_model (GTK_TREE_VIEW (impl->browse_files_tree_view),
                           GTK_TREE_MODEL (impl->search_model));
  file_list_set_sort_column_ids (impl);
#endif

  /* FMQ: if search was empty, say that we got no hits */
  set_busy_cursor (impl, FALSE);
}

/* Displays a generic error when we cannot create a GtkSearchEngine.  
 * It would be better if _gtk_search_engine_new() gave us a GError 
 * with a better message, but it doesn't do that right now.
 */
static void
search_error_could_not_create_client (GtkFileChooserDefault *impl)
{
  error_message (impl,
		 _("Could not start the search process"),
		 _("The program was not able to create a connection to the indexer "
		   "daemon.  Please make sure it is running."));
}

static void
search_engine_error_cb (GtkSearchEngine *engine,
			const gchar     *message,
			gpointer         data)
{
  GtkFileChooserDefault *impl;
  
  impl = GTK_FILE_CHOOSER_DEFAULT (data);

  search_stop_searching (impl, TRUE);
  error_message (impl, _("Could not send the search request"), message);

  set_busy_cursor (impl, FALSE);
}

/* Frees the data in the search_model */
static void
search_clear_model (GtkFileChooserDefault *impl, 
		    gboolean               remove_from_treeview)
{
  if (!impl->search_model)
    return;

  g_object_unref (impl->search_model);
  impl->search_model = NULL;
  
  if (remove_from_treeview)
    gtk_tree_view_set_model (GTK_TREE_VIEW (impl->browse_files_tree_view), NULL);
}

/* Stops any ongoing searches; does not touch the search_model */
static void
search_stop_searching (GtkFileChooserDefault *impl,
                       gboolean               remove_query)
{
  if (remove_query && impl->search_query)
    {
      g_object_unref (impl->search_query);
      impl->search_query = NULL;
    }
  
  if (impl->search_engine)
    {
      _gtk_search_engine_stop (impl->search_engine);
      
      g_object_unref (impl->search_engine);
      impl->search_engine = NULL;
    }
}

/* Creates the search_model and puts it in the tree view */
static void
search_setup_model (GtkFileChooserDefault *impl)
{
  g_assert (impl->search_model == NULL);

  impl->search_model = _gtk_file_system_model_new (file_system_model_set,
                                                   impl,
						   MODEL_COLUMN_TYPES);

  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (impl->search_model),
				   MODEL_COL_NAME,
				   name_sort_func,
				   impl, NULL);
  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (impl->search_model),
				   MODEL_COL_MTIME,
				   mtime_sort_func,
				   impl, NULL);
  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (impl->search_model),
				   MODEL_COL_SIZE,
				   size_sort_func,
				   impl, NULL);
  set_sort_column (impl);

  /* EB: setting the model here will make the hits list update feel
   * more "alive" than setting the model at the end of the search
   * run
   */
  gtk_tree_view_set_model (GTK_TREE_VIEW (impl->browse_files_tree_view),
                           GTK_TREE_MODEL (impl->search_model));
  file_list_set_sort_column_ids (impl);
}

/* Creates a new query with the specified text and launches it */
static void
search_start_query (GtkFileChooserDefault *impl,
		    const gchar           *query_text)
{
  search_stop_searching (impl, FALSE);
  search_clear_model (impl, TRUE);
  search_setup_model (impl);
  set_busy_cursor (impl, TRUE);

  if (impl->search_engine == NULL)
    impl->search_engine = _gtk_search_engine_new ();

  if (!impl->search_engine)
    {
      set_busy_cursor (impl, FALSE);
      search_error_could_not_create_client (impl); /* lame; we don't get an error code or anything */
      return;
    }

  if (!impl->search_query)
    {
      impl->search_query = _gtk_query_new ();
      _gtk_query_set_text (impl->search_query, query_text);
    }
  
  _gtk_search_engine_set_query (impl->search_engine, impl->search_query);

  g_signal_connect (impl->search_engine, "hits-added",
		    G_CALLBACK (search_engine_hits_added_cb), impl);
  g_signal_connect (impl->search_engine, "finished",
		    G_CALLBACK (search_engine_finished_cb), impl);
  g_signal_connect (impl->search_engine, "error",
		    G_CALLBACK (search_engine_error_cb), impl);

  _gtk_search_engine_start (impl->search_engine);
}

/* Callback used when the user presses Enter while typing on the search
 * entry; starts the query
 */
static void
search_entry_activate_cb (GtkEntry *entry,
			  gpointer data)
{
  GtkFileChooserDefault *impl;
  const char *text;

  impl = GTK_FILE_CHOOSER_DEFAULT (data);

  text = gtk_entry_get_text (GTK_ENTRY (impl->search_entry));
  if (strlen (text) == 0)
    return;

  /* reset any existing query object */
  if (impl->search_query)
    {
      g_object_unref (impl->search_query);
      impl->search_query = NULL;
    }

  search_start_query (impl, text);
}

static gboolean
focus_entry_idle_cb (GtkFileChooserDefault *impl)
{
  GDK_THREADS_ENTER ();
  
  g_source_destroy (impl->focus_entry_idle);
  impl->focus_entry_idle = NULL;

  if (impl->search_entry)
    gtk_widget_grab_focus (impl->search_entry);

  GDK_THREADS_LEAVE ();

  return FALSE;
}

static void
focus_search_entry_in_idle (GtkFileChooserDefault *impl)
{
  /* bgo#634558 - When the user clicks on the Search entry in the shortcuts
   * pane, we get a selection-changed signal and we set up the search widgets.
   * However, gtk_tree_view_button_press() focuses the treeview *after* making
   * the change to the selection.  So, we need to re-focus the search entry
   * after the treeview has finished doing its work; we'll do that in an idle
   * handler.
   */

  if (!impl->focus_entry_idle)
    impl->focus_entry_idle = add_idle_while_impl_is_alive (impl, G_CALLBACK (focus_entry_idle_cb));
}

/* Hides the path bar and creates the search entry */
static void
search_setup_widgets (GtkFileChooserDefault *impl)
{
  impl->search_hbox = gtk_hbox_new (FALSE, 12);

  path_bar_update (impl);

  impl->search_entry = gtk_entry_new ();
  g_signal_connect (impl->search_entry, "activate",
		    G_CALLBACK (search_entry_activate_cb),
		    impl);
  gtk_box_pack_start (GTK_BOX (impl->search_hbox), impl->search_entry, TRUE, TRUE, 0);

  /* if there already is a query, restart it */
  if (impl->search_query)
    {
      gchar *query = _gtk_query_get_text (impl->search_query);

      if (query)
        {
          gtk_entry_set_text (GTK_ENTRY (impl->search_entry), query);
          search_start_query (impl, query);

          g_free (query);
        }
      else
        {
          g_object_unref (impl->search_query);
          impl->search_query = NULL;
        }
    }

  /* Box for search widgets */
  gtk_box_pack_start (GTK_BOX (impl->browse_path_bar_hbox), impl->search_hbox, TRUE, TRUE, 0);
  gtk_widget_show_all (impl->search_hbox);
  gtk_size_group_add_widget (GTK_SIZE_GROUP (impl->browse_path_bar_size_group), impl->search_hbox);

  /* Hide the location widgets temporarily */

  if (impl->action == GTK_FILE_CHOOSER_ACTION_OPEN ||
      impl->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER)
    {
      gtk_widget_hide (impl->location_button);
      gtk_widget_hide (impl->location_entry_box);
    }

  focus_search_entry_in_idle (impl);

  /* FMQ: hide the filter combo? */
}

/*
 * Recent files support
 */

/* Frees the data in the recent_model */
static void
recent_clear_model (GtkFileChooserDefault *impl,
                    gboolean               remove_from_treeview)
{
  if (!impl->recent_model)
    return;

  if (remove_from_treeview)
    gtk_tree_view_set_model (GTK_TREE_VIEW (impl->browse_files_tree_view), NULL);

  g_object_unref (impl->recent_model);
  impl->recent_model = NULL;
}

/* Stops any ongoing loading of the recent files list; does
 * not touch the recent_model
 */
static void
recent_stop_loading (GtkFileChooserDefault *impl)
{
  if (impl->load_recent_id)
    {
      g_source_remove (impl->load_recent_id);
      impl->load_recent_id = 0;
    }
}

static void
recent_setup_model (GtkFileChooserDefault *impl)
{
  g_assert (impl->recent_model == NULL);

  impl->recent_model = _gtk_file_system_model_new (file_system_model_set,
                                                   impl,
						   MODEL_COLUMN_TYPES);

  _gtk_file_system_model_set_filter (impl->recent_model,
                                     impl->current_filter);
  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (impl->recent_model),
				   MODEL_COL_NAME,
				   name_sort_func,
				   impl, NULL);
  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (impl->recent_model),
                                   MODEL_COL_SIZE,
                                   size_sort_func,
                                   impl, NULL);
  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (impl->recent_model),
                                   MODEL_COL_MTIME,
                                   mtime_sort_func,
                                   impl, NULL);
  set_sort_column (impl);
}

typedef struct
{
  GtkFileChooserDefault *impl;
  GList *items;
} RecentLoadData;

static void
recent_idle_cleanup (gpointer data)
{
  RecentLoadData *load_data = data;
  GtkFileChooserDefault *impl = load_data->impl;

  gtk_tree_view_set_model (GTK_TREE_VIEW (impl->browse_files_tree_view),
                           GTK_TREE_MODEL (impl->recent_model));
  file_list_set_sort_column_ids (impl);
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (impl->recent_model), MODEL_COL_MTIME, GTK_SORT_DESCENDING);

  set_busy_cursor (impl, FALSE);
  
  impl->load_recent_id = 0;
  
  g_free (load_data);
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

/* Populates the file system model with the GtkRecentInfo* items in the provided list; frees the items */
static void
populate_model_with_recent_items (GtkFileChooserDefault *impl, GList *items)
{
  gint limit;
  GList *l;
  int n;

  limit = get_recent_files_limit (GTK_WIDGET (impl));

  n = 0;

  for (l = items; l; l = l->next)
    {
      GtkRecentInfo *info = l->data;
      GFile *file;

      file = g_file_new_for_uri (gtk_recent_info_get_uri (info));
      _gtk_file_system_model_add_and_query_file (impl->recent_model,
                                                 file,
                                                 MODEL_ATTRIBUTES);
      g_object_unref (file);

      n++;
      if (limit != -1 && n >= limit)
	break;
    }
}

static void
populate_model_with_folders (GtkFileChooserDefault *impl, GList *items)
{
  GList *folders;
  GList *l;

  folders = _gtk_file_chooser_extract_recent_folders (items);

  for (l = folders; l; l = l->next)
    {
      GFile *folder = l->data;

      _gtk_file_system_model_add_and_query_file (impl->recent_model,
                                                 folder,
                                                 MODEL_ATTRIBUTES);
    }

  g_list_foreach (folders, (GFunc) g_object_unref, NULL);
  g_list_free (folders);
}

static gboolean
recent_idle_load (gpointer data)
{
  RecentLoadData *load_data = data;
  GtkFileChooserDefault *impl = load_data->impl;

  if (!impl->recent_manager)
    return FALSE;

  load_data->items = gtk_recent_manager_get_items (impl->recent_manager);
  if (!load_data->items)
    return FALSE;

  if (impl->action == GTK_FILE_CHOOSER_ACTION_OPEN)
    populate_model_with_recent_items (impl, load_data->items);
  else
    populate_model_with_folders (impl, load_data->items);

  g_list_foreach (load_data->items, (GFunc) gtk_recent_info_unref, NULL);
  g_list_free (load_data->items);
  load_data->items = NULL;

  return FALSE;
}

static void
recent_start_loading (GtkFileChooserDefault *impl)
{
  RecentLoadData *load_data;

  recent_stop_loading (impl);
  recent_clear_model (impl, TRUE);
  recent_setup_model (impl);
  set_busy_cursor (impl, TRUE);

  g_assert (impl->load_recent_id == 0);

  load_data = g_new (RecentLoadData, 1);
  load_data->impl = impl;
  load_data->items = NULL;

  /* begin lazy loading the recent files into the model */
  impl->load_recent_id = gdk_threads_add_idle_full (G_PRIORITY_HIGH_IDLE + 30,
                                                    recent_idle_load,
                                                    load_data,
                                                    recent_idle_cleanup);
}

static void
recent_selected_foreach_get_file_cb (GtkTreeModel *model,
				     GtkTreePath  *path,
				     GtkTreeIter  *iter,
				     gpointer      data)
{
  GSList **list;
  GFile *file;

  list = data;

  gtk_tree_model_get (model, iter, MODEL_COL_FILE, &file, -1);
  *list = g_slist_prepend (*list, file);
}

/* Constructs a list of the selected paths in recent files mode */
static GSList *
recent_get_selected_files (GtkFileChooserDefault *impl)
{
  GSList *result;
  GtkTreeSelection *selection;

  result = NULL;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->browse_files_tree_view));
  gtk_tree_selection_selected_foreach (selection, recent_selected_foreach_get_file_cb, &result);
  result = g_slist_reverse (result);

  return result;
}

/* Called from ::should_respond().  We return whether there are selected
 * files in the recent files list.
 */
static gboolean
recent_should_respond (GtkFileChooserDefault *impl)
{
  GtkTreeSelection *selection;

  g_assert (impl->operation_mode == OPERATION_MODE_RECENT);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->browse_files_tree_view));
  return (gtk_tree_selection_count_selected_rows (selection) != 0);
}

static void
set_current_filter (GtkFileChooserDefault *impl,
		    GtkFileFilter         *filter)
{
  if (impl->current_filter != filter)
    {
      int filter_index;

      /* NULL filters are allowed to reset to non-filtered status
       */
      filter_index = g_slist_index (impl->filters, filter);
      if (impl->filters && filter && filter_index < 0)
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
				  filter_index);

      if (impl->browse_files_model)
	install_list_model_filter (impl);

      if (impl->search_model)
        _gtk_file_system_model_set_filter (impl->search_model, filter);

      if (impl->recent_model)
        _gtk_file_system_model_set_filter (impl->recent_model, filter);

      g_object_notify (G_OBJECT (impl), "filter");
    }
}

static void
filter_combo_changed (GtkComboBox           *combo_box,
		      GtkFileChooserDefault *impl)
{
  gint new_index = gtk_combo_box_get_active (combo_box);
  GtkFileFilter *new_filter = g_slist_nth_data (impl->filters, new_index);

  set_current_filter (impl, new_filter);
}

static void
check_preview_change (GtkFileChooserDefault *impl)
{
  GtkTreePath *cursor_path;
  GFile *new_file;
  char *new_display_name;
  GtkTreeModel *model;

  gtk_tree_view_get_cursor (GTK_TREE_VIEW (impl->browse_files_tree_view), &cursor_path, NULL);
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (impl->browse_files_tree_view));
  if (cursor_path)
    {
      GtkTreeIter iter;

      gtk_tree_model_get_iter (model, &iter, cursor_path);
      gtk_tree_model_get (model, &iter,
                          MODEL_COL_FILE, &new_file,
                          MODEL_COL_NAME, &new_display_name,
                          -1);
      
      gtk_tree_path_free (cursor_path);
    }
  else
    {
      new_file = NULL;
      new_display_name = NULL;
    }

  if (new_file != impl->preview_file &&
      !(new_file && impl->preview_file &&
	g_file_equal (new_file, impl->preview_file)))
    {
      if (impl->preview_file)
	{
	  g_object_unref (impl->preview_file);
	  g_free (impl->preview_display_name);
	}

      if (new_file)
	{
	  impl->preview_file = new_file;
	  impl->preview_display_name = new_display_name;
	}
      else
	{
	  impl->preview_file = NULL;
	  impl->preview_display_name = NULL;
          g_free (new_display_name);
	}

      if (impl->use_preview_label && impl->preview_label)
	gtk_label_set_text (GTK_LABEL (impl->preview_label), impl->preview_display_name);

      g_signal_emit_by_name (impl, "update-preview");
    }
  else
    {
      if (new_file)
        g_object_unref (new_file);

      g_free (new_display_name);
    }
}

static void
shortcuts_activate_volume_mount_cb (GCancellable        *cancellable,
				    GtkFileSystemVolume *volume,
				    const GError        *error,
				    gpointer             data)
{
  GFile *file;
  gboolean cancelled = g_cancellable_is_cancelled (cancellable);
  GtkFileChooserDefault *impl = data;

  if (cancellable != impl->shortcuts_activate_iter_cancellable)
    goto out;

  impl->shortcuts_activate_iter_cancellable = NULL;

  set_busy_cursor (impl, FALSE);

  if (cancelled)
    goto out;

  if (error)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_FAILED_HANDLED))
        {
          char *msg, *name;

	  name = _gtk_file_system_volume_get_display_name (volume);
          msg = g_strdup_printf (_("Could not mount %s"), name);

          error_message (impl, msg, error->message);

          g_free (msg);
	  g_free (name);
        }

      goto out;
    }

  file = _gtk_file_system_volume_get_root (volume);
  if (file != NULL)
    {
      change_folder_and_display_error (impl, file, FALSE);
      g_object_unref (file);
    }

out:
  g_object_unref (impl);
  g_object_unref (cancellable);
}


/* Activates a volume by mounting it if necessary and then switching to its
 * base path.
 */
static void
shortcuts_activate_volume (GtkFileChooserDefault *impl,
			   GtkFileSystemVolume   *volume)
{
  GFile *file;

  operation_mode_set (impl, OPERATION_MODE_BROWSE);

  /* We ref the file chooser since volume_mount() may run a main loop, and the
   * user could close the file chooser window in the meantime.
   */
  g_object_ref (impl);

  if (!_gtk_file_system_volume_is_mounted (volume))
    {
      GMountOperation *mount_op;

      set_busy_cursor (impl, TRUE);
   
      mount_op = gtk_mount_operation_new (get_toplevel (GTK_WIDGET (impl)));
      impl->shortcuts_activate_iter_cancellable =
        _gtk_file_system_mount_volume (impl->file_system, volume, mount_op,
				       shortcuts_activate_volume_mount_cb,
				       g_object_ref (impl));
      g_object_unref (mount_op);
    }
  else
    {
      file = _gtk_file_system_volume_get_root (volume);
      if (file != NULL)
        {
          change_folder_and_display_error (impl, file, FALSE);
	  g_object_unref (file);
        }
    }

  g_object_unref (impl);
}

/* Opens the folder or volume at the specified iter in the shortcuts model */
struct ShortcutsActivateData
{
  GtkFileChooserDefault *impl;
  GFile *file;
};

static void
shortcuts_activate_get_info_cb (GCancellable *cancellable,
				GFileInfo    *info,
			        const GError *error,
			        gpointer      user_data)
{
  gboolean cancelled = g_cancellable_is_cancelled (cancellable);
  struct ShortcutsActivateData *data = user_data;

  if (cancellable != data->impl->shortcuts_activate_iter_cancellable)
    goto out;

  data->impl->shortcuts_activate_iter_cancellable = NULL;

  if (cancelled)
    goto out;

  if (!error && _gtk_file_info_consider_as_directory (info))
    change_folder_and_display_error (data->impl, data->file, FALSE);
  else
    gtk_file_chooser_default_select_file (GTK_FILE_CHOOSER (data->impl),
                                          data->file,
                                          NULL);

out:
  g_object_unref (data->impl);
  g_object_unref (data->file);
  g_free (data);

  g_object_unref (cancellable);
}

static void
shortcuts_activate_mount_enclosing_volume (GCancellable        *cancellable,
					   GtkFileSystemVolume *volume,
					   const GError        *error,
					   gpointer             user_data)
{
  struct ShortcutsActivateData *data = user_data;

  if (error)
    {
      error_changing_folder_dialog (data->impl, data->file, g_error_copy (error));

      g_object_unref (data->impl);
      g_object_unref (data->file);
      g_free (data);

      return;
    }

  data->impl->shortcuts_activate_iter_cancellable =
    _gtk_file_system_get_info (data->impl->file_system, data->file,
			       "standard::type",
			       shortcuts_activate_get_info_cb, data);

  if (volume)
    _gtk_file_system_volume_unref (volume);
}

static void
shortcuts_activate_iter (GtkFileChooserDefault *impl,
			 GtkTreeIter           *iter)
{
  gpointer col_data;
  ShortcutType shortcut_type;

  /* In the Save modes, we want to preserve what the uesr typed in the filename
   * entry, so that he may choose another folder without erasing his typed name.
   */
  if (impl->location_entry
      && !(impl->action == GTK_FILE_CHOOSER_ACTION_SAVE
	   || impl->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER))
    gtk_entry_set_text (GTK_ENTRY (impl->location_entry), "");

  gtk_tree_model_get (GTK_TREE_MODEL (impl->shortcuts_model), iter,
		      SHORTCUTS_COL_DATA, &col_data,
		      SHORTCUTS_COL_TYPE, &shortcut_type,
		      -1);

  if (impl->shortcuts_activate_iter_cancellable)
    {
      g_cancellable_cancel (impl->shortcuts_activate_iter_cancellable);
      impl->shortcuts_activate_iter_cancellable = NULL;
    }

  if (shortcut_type == SHORTCUT_TYPE_SEPARATOR)
    return;
  else if (shortcut_type == SHORTCUT_TYPE_VOLUME)
    {
      GtkFileSystemVolume *volume;

      volume = col_data;

      operation_mode_set (impl, OPERATION_MODE_BROWSE);

      shortcuts_activate_volume (impl, volume);
    }
  else if (shortcut_type == SHORTCUT_TYPE_FILE)
    {
      struct ShortcutsActivateData *data;
      GtkFileSystemVolume *volume;

      operation_mode_set (impl, OPERATION_MODE_BROWSE);

      volume = _gtk_file_system_get_volume_for_file (impl->file_system, col_data);

      data = g_new0 (struct ShortcutsActivateData, 1);
      data->impl = g_object_ref (impl);
      data->file = g_object_ref (col_data);

      if (!volume || !_gtk_file_system_volume_is_mounted (volume))
	{
	  GMountOperation *mount_operation;
	  GtkWidget *toplevel;

	  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (impl));

	  mount_operation = gtk_mount_operation_new (GTK_WINDOW (toplevel));

	  impl->shortcuts_activate_iter_cancellable =
	    _gtk_file_system_mount_enclosing_volume (impl->file_system, col_data,
						     mount_operation,
						     shortcuts_activate_mount_enclosing_volume,
						     data);
	}
      else
	{
	  impl->shortcuts_activate_iter_cancellable =
	    _gtk_file_system_get_info (impl->file_system, data->file,
	 			       "standard::type",
				       shortcuts_activate_get_info_cb, data);
	}
    }
  else if (shortcut_type == SHORTCUT_TYPE_SEARCH)
    {
      operation_mode_set (impl, OPERATION_MODE_SEARCH);
    }
  else if (shortcut_type == SHORTCUT_TYPE_RECENT)
    {
      operation_mode_set (impl, OPERATION_MODE_RECENT);
    }
}

/* Handler for GtkWidget::key-press-event on the shortcuts list */
static gboolean
shortcuts_key_press_event_cb (GtkWidget             *widget,
			      GdkEventKey           *event,
			      GtkFileChooserDefault *impl)
{
  guint modifiers;

  modifiers = gtk_accelerator_get_default_mod_mask ();

  if (key_is_left_or_right (event))
    {
      gtk_widget_grab_focus (impl->browse_files_tree_view);
      return TRUE;
    }

  if ((event->keyval == GDK_KEY_BackSpace
      || event->keyval == GDK_KEY_Delete
      || event->keyval == GDK_KEY_KP_Delete)
      && (event->state & modifiers) == 0)
    {
      remove_selected_bookmarks (impl);
      return TRUE;
    }

  if ((event->keyval == GDK_KEY_F2)
      && (event->state & modifiers) == 0)
    {
      rename_selected_bookmark (impl);
      return TRUE;
    }

  return FALSE;
}

static gboolean
shortcuts_select_func  (GtkTreeSelection  *selection,
			GtkTreeModel      *model,
			GtkTreePath       *path,
			gboolean           path_currently_selected,
			gpointer           data)
{
  GtkFileChooserDefault *impl = data;
  GtkTreeIter filter_iter;
  ShortcutType shortcut_type;

  if (!gtk_tree_model_get_iter (impl->shortcuts_pane_filter_model, &filter_iter, path))
    g_assert_not_reached ();

  gtk_tree_model_get (impl->shortcuts_pane_filter_model, &filter_iter, SHORTCUTS_COL_TYPE, &shortcut_type, -1);

  return shortcut_type != SHORTCUT_TYPE_SEPARATOR;
}

static gboolean
list_select_func  (GtkTreeSelection  *selection,
		   GtkTreeModel      *model,
		   GtkTreePath       *path,
		   gboolean           path_currently_selected,
		   gpointer           data)
{
  GtkFileChooserDefault *impl = data;

  if (impl->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER ||
      impl->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER)
    {
      GtkTreeIter iter;
      gboolean is_sensitive;
      gboolean is_folder;

      if (!gtk_tree_model_get_iter (model, &iter, path))
        return FALSE;
      gtk_tree_model_get (model, &iter,
                          MODEL_COL_IS_SENSITIVE, &is_sensitive,
                          MODEL_COL_IS_FOLDER, &is_folder,
                          -1);
      if (!is_sensitive || !is_folder)
        return FALSE;
    }
    
  return TRUE;
}

static void
list_selection_changed (GtkTreeSelection      *selection,
			GtkFileChooserDefault *impl)
{
  /* See if we are in the new folder editable row for Save mode */
  if (impl->operation_mode == OPERATION_MODE_BROWSE &&
      impl->action == GTK_FILE_CHOOSER_ACTION_SAVE)
    {
      GFileInfo *info;
      gboolean had_selection;

      info = get_selected_file_info_from_file_list (impl, &had_selection);
      if (!had_selection)
	goto out; /* normal processing */

      if (!info)
	return; /* We are on the editable row for New Folder */
    }

 out:

  if (impl->location_entry)
    update_chooser_entry (impl);

  path_bar_update (impl);

  check_preview_change (impl);
  bookmarks_check_add_sensitivity (impl);

  g_signal_emit_by_name (impl, "selection-changed", 0);
}

/* Callback used when a row in the file list is activated */
static void
list_row_activated (GtkTreeView           *tree_view,
		    GtkTreePath           *path,
		    GtkTreeViewColumn     *column,
		    GtkFileChooserDefault *impl)
{
  GFile *file;
  GtkTreeIter iter;
  GtkTreeModel *model;
  gboolean is_folder;
  gboolean is_sensitive;

  model = gtk_tree_view_get_model (tree_view);

  if (!gtk_tree_model_get_iter (model, &iter, path))
    return;

  gtk_tree_model_get (model, &iter,
                      MODEL_COL_FILE, &file,
                      MODEL_COL_IS_FOLDER, &is_folder,
                      MODEL_COL_IS_SENSITIVE, &is_sensitive,
                      -1);
        
  if (is_sensitive && is_folder && file)
    {
      change_folder_and_display_error (impl, file, FALSE);
      return;
    }

  if (impl->action == GTK_FILE_CHOOSER_ACTION_OPEN ||
      impl->action == GTK_FILE_CHOOSER_ACTION_SAVE)
    g_signal_emit_by_name (impl, "file-activated");

  if (file)
    g_object_unref (file);
}

static void
path_bar_clicked (GtkPathBar            *path_bar,
		  GFile                 *file,
		  GFile                 *child_file,
		  gboolean               child_is_hidden,
		  GtkFileChooserDefault *impl)
{
  if (child_file)
    pending_select_files_add (impl, child_file);

  if (!change_folder_and_display_error (impl, file, FALSE))
    return;

  /* Say we have "/foo/bar/[.baz]" and the user clicks on "bar".  We should then
   * show hidden files so that ".baz" appears in the file list, as it will still
   * be shown in the path bar: "/foo/[bar]/.baz"
   */
  if (child_is_hidden)
    g_object_set (impl, "show-hidden", TRUE, NULL);
}

static void
update_cell_renderer_attributes (GtkFileChooserDefault *impl)
{
  GtkTreeViewColumn *column;
  GtkCellRenderer *renderer;
  GList *walk, *list;

  /* Keep the following column numbers in sync with create_file_list() */

  /* name */
  column = gtk_tree_view_get_column (GTK_TREE_VIEW (impl->browse_files_tree_view), 0);
  list = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (column));
  for (walk = list; walk; walk = walk->next)
    {
      renderer = walk->data;
      if (GTK_IS_CELL_RENDERER_PIXBUF (renderer))
        {
          gtk_tree_view_column_set_attributes (column, renderer, 
                                               "pixbuf", MODEL_COL_PIXBUF,
                                               NULL);
        }
      else
        {
          gtk_tree_view_column_set_attributes (column, renderer, 
                                               "text", MODEL_COL_NAME,
                                               "ellipsize", MODEL_COL_ELLIPSIZE,
                                               NULL);
        }

      gtk_tree_view_column_add_attribute (column, renderer, "sensitive", MODEL_COL_IS_SENSITIVE);
    }
  g_list_free (list);

  /* size */
  column = gtk_tree_view_get_column (GTK_TREE_VIEW (impl->browse_files_tree_view), 1);
  list = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (column));
  renderer = list->data;
  gtk_tree_view_column_set_attributes (column, renderer, 
                                       "text", MODEL_COL_SIZE_TEXT,
                                       NULL);

  gtk_tree_view_column_add_attribute (column, renderer, "sensitive", MODEL_COL_IS_SENSITIVE);
  g_list_free (list);

  /* mtime */
  column = gtk_tree_view_get_column (GTK_TREE_VIEW (impl->browse_files_tree_view), 2);
  list = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (column));
  renderer = list->data;
  gtk_tree_view_column_set_attributes (column, renderer, 
                                       "text", MODEL_COL_MTIME_TEXT,
                                       NULL);
  gtk_tree_view_column_add_attribute (column, renderer, "sensitive", MODEL_COL_IS_SENSITIVE);
  g_list_free (list);
}

GtkWidget *
_gtk_file_chooser_default_new (void)
{
  return g_object_new (GTK_TYPE_FILE_CHOOSER_DEFAULT, NULL);
}

static void
location_set_user_text (GtkFileChooserDefault *impl,
			const gchar           *path)
{
  gtk_entry_set_text (GTK_ENTRY (impl->location_entry), path);
  gtk_editable_set_position (GTK_EDITABLE (impl->location_entry), -1);
}

static void
location_popup_handler (GtkFileChooserDefault *impl,
			const gchar           *path)
{ 
  if (impl->operation_mode != OPERATION_MODE_BROWSE)
    {
      GtkWidget *widget_to_focus;

      operation_mode_set (impl, OPERATION_MODE_BROWSE);
      
      if (impl->current_folder)
        change_folder_and_display_error (impl, impl->current_folder, FALSE);

      if (impl->location_mode == LOCATION_MODE_PATH_BAR)
        widget_to_focus = impl->browse_files_tree_view;
      else
        widget_to_focus = impl->location_entry;

      gtk_widget_grab_focus (widget_to_focus);
      return; 
    }
  
  if (impl->action == GTK_FILE_CHOOSER_ACTION_OPEN ||
      impl->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER)
    {
      if (!path)
	return;

      location_mode_set (impl, LOCATION_MODE_FILENAME_ENTRY, TRUE);
      location_set_user_text (impl, path);
    }
  else if (impl->action == GTK_FILE_CHOOSER_ACTION_SAVE ||
	   impl->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER)
    {
      gtk_widget_grab_focus (impl->location_entry);
      if (path != NULL)
	location_set_user_text (impl, path);
    }
  else
    g_assert_not_reached ();
}

/* Handler for the "up-folder" keybinding signal */
static void
up_folder_handler (GtkFileChooserDefault *impl)
{
  _gtk_path_bar_up (GTK_PATH_BAR (impl->browse_path_bar));
}

/* Handler for the "down-folder" keybinding signal */
static void
down_folder_handler (GtkFileChooserDefault *impl)
{
  _gtk_path_bar_down (GTK_PATH_BAR (impl->browse_path_bar));
}

/* Switches to the shortcut in the specified index */
static void
switch_to_shortcut (GtkFileChooserDefault *impl,
		    int pos)
{
  GtkTreeIter iter;

  if (!gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (impl->shortcuts_model), &iter, NULL, pos))
    g_assert_not_reached ();

  shortcuts_activate_iter (impl, &iter);
}

/* Handler for the "home-folder" keybinding signal */
static void
home_folder_handler (GtkFileChooserDefault *impl)
{
  if (impl->has_home)
    switch_to_shortcut (impl, shortcuts_get_index (impl, SHORTCUTS_HOME));
}

/* Handler for the "desktop-folder" keybinding signal */
static void
desktop_folder_handler (GtkFileChooserDefault *impl)
{
  if (impl->has_desktop)
    switch_to_shortcut (impl, shortcuts_get_index (impl, SHORTCUTS_DESKTOP));
}

/* Handler for the "search-shortcut" keybinding signal */
static void
search_shortcut_handler (GtkFileChooserDefault *impl)
{
  if (impl->has_search)
    {
      switch_to_shortcut (impl, shortcuts_get_index (impl, SHORTCUTS_SEARCH));

      /* we want the entry widget to grab the focus the first
       * time, not the browse_files_tree_view widget.
       */
      if (impl->search_entry)
        gtk_widget_grab_focus (impl->search_entry);
    }
}

/* Handler for the "recent-shortcut" keybinding signal */
static void
recent_shortcut_handler (GtkFileChooserDefault *impl)
{
  switch_to_shortcut (impl, shortcuts_get_index (impl, SHORTCUTS_RECENT));
}

static void
quick_bookmark_handler (GtkFileChooserDefault *impl,
			gint bookmark_index)
{
  int bookmark_pos;
  GtkTreePath *path;

  if (bookmark_index < 0 || bookmark_index >= impl->num_bookmarks)
    return;

  bookmark_pos = shortcuts_get_index (impl, SHORTCUTS_BOOKMARKS) + bookmark_index;

  path = gtk_tree_path_new_from_indices (bookmark_pos, -1);
  gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (impl->browse_shortcuts_tree_view),
				path, NULL,
				FALSE, 0.0, 0.0);
  gtk_tree_path_free (path);

  switch_to_shortcut (impl, bookmark_pos);
}

static void
show_hidden_handler (GtkFileChooserDefault *impl)
{
  g_object_set (impl,
		"show-hidden", !impl->show_hidden,
		NULL);
}


/* Drag and drop interfaces */

static void
_shortcuts_pane_model_filter_class_init (ShortcutsPaneModelFilterClass *class)
{
}

static void
_shortcuts_pane_model_filter_init (ShortcutsPaneModelFilter *model)
{
  model->impl = NULL;
}

/* GtkTreeDragSource::row_draggable implementation for the shortcuts filter model */
static gboolean
shortcuts_pane_model_filter_row_draggable (GtkTreeDragSource *drag_source,
				           GtkTreePath       *path)
{
  ShortcutsPaneModelFilter *model;
  int pos;
  int bookmarks_pos;

  model = SHORTCUTS_PANE_MODEL_FILTER (drag_source);

  pos = *gtk_tree_path_get_indices (path);
  bookmarks_pos = shortcuts_get_index (model->impl, SHORTCUTS_BOOKMARKS);

  return (pos >= bookmarks_pos && pos < bookmarks_pos + model->impl->num_bookmarks);
}

/* GtkTreeDragSource::drag_data_get implementation for the shortcuts
 * filter model
 */
static gboolean
shortcuts_pane_model_filter_drag_data_get (GtkTreeDragSource *drag_source,
                                           GtkTreePath       *path,
                                           GtkSelectionData  *selection_data)
{
  /* FIXME */

  return FALSE;
}

/* Fill the GtkTreeDragSourceIface vtable */
static void
shortcuts_pane_model_filter_drag_source_iface_init (GtkTreeDragSourceIface *iface)
{
  iface->row_draggable = shortcuts_pane_model_filter_row_draggable;
  iface->drag_data_get = shortcuts_pane_model_filter_drag_data_get;
}

#if 0
/* Fill the GtkTreeDragDestIface vtable */
static void
shortcuts_pane_model_filter_drag_dest_iface_init (GtkTreeDragDestIface *iface)
{
  iface->drag_data_received = shortcuts_pane_model_filter_drag_data_received;
  iface->row_drop_possible = shortcuts_pane_model_filter_row_drop_possible;
}
#endif

static GtkTreeModel *
shortcuts_pane_model_filter_new (GtkFileChooserDefault *impl,
			         GtkTreeModel          *child_model,
			         GtkTreePath           *root)
{
  ShortcutsPaneModelFilter *model;

  model = g_object_new (SHORTCUTS_PANE_MODEL_FILTER_TYPE,
			"child-model", child_model,
			"virtual-root", root,
			NULL);

  model->impl = impl;

  return GTK_TREE_MODEL (model);
}

