/* GtkIconTheme - a loader for icon themes
 * gtk-icon-theme.c Copyright (C) 2002, 2003 Red Hat, Inc.
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

#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <glib/gstdio.h>

#ifdef G_OS_WIN32
#ifndef S_ISDIR
#define S_ISDIR(mode) ((mode)&_S_IFDIR)
#endif
#define WIN32_MEAN_AND_LEAN
#include <windows.h>
#include "gdk/gdkwin32.h"
#endif /* G_OS_WIN32 */

#include "gtkicontheme.h"
#include "gtkiconfactory.h"
#include "gtkiconcache.h"
#include "gtkbuiltincache.h"
#include "gtkintl.h"
#include "gtkmain.h"
#include "gtksettings.h"
#include "gtkprivate.h"
#include "gtkalias.h"

#define DEFAULT_THEME_NAME "hicolor"

typedef enum
{
  ICON_THEME_DIR_FIXED,  
  ICON_THEME_DIR_SCALABLE,  
  ICON_THEME_DIR_THRESHOLD,
  ICON_THEME_DIR_UNTHEMED
} IconThemeDirType;

/* In reverse search order: */
typedef enum
{
  ICON_SUFFIX_NONE = 0,
  ICON_SUFFIX_XPM = 1 << 0,
  ICON_SUFFIX_SVG = 1 << 1,
  ICON_SUFFIX_PNG = 1 << 2,
  HAS_ICON_FILE = 1 << 3
} IconSuffix;


struct _GtkIconThemePrivate
{
  guint custom_theme        : 1;
  guint is_screen_singleton : 1;
  guint pixbuf_supports_svg : 1;
  guint themes_valid        : 1;
  guint check_reload        : 1;
  guint loading_themes      : 1;
  
  char *current_theme;
  char *fallback_theme;
  char **search_path;
  int search_path_len;

  /* A list of all the themes needed to look up icons.
   * In search order, without duplicates
   */
  GList *themes;
  GHashTable *unthemed_icons;
  
  /* Note: The keys of this hashtable are owned by the
   * themedir and unthemed hashtables.
   */
  GHashTable *all_icons;

  /* GdkScreen for the icon theme (may be NULL)
   */
  GdkScreen *screen;
  
  /* time when we last stat:ed for theme changes */
  long last_stat_time;
  GList *dir_mtimes;

  gulong reset_styles_idle;
};

struct _GtkIconInfo
{
  /* Information about the source
   */
  gchar *filename;
#if defined (G_OS_WIN32) && !defined (_WIN64)
  /* System codepage version of filename, for DLL ABI backward
   * compatibility functions.
   */
  gchar *cp_filename;
#endif
  GLoadableIcon *loadable;
  GSList *emblem_infos;

  /* Cache pixbuf (if there is any) */
  GdkPixbuf *cache_pixbuf;

  GtkIconData *data;
  
  /* Information about the directory where
   * the source was found
   */
  IconThemeDirType dir_type;
  gint dir_size;
  gint threshold;

  /* Parameters influencing the scaled icon
   */
  gint desired_size;
  guint raw_coordinates : 1;
  guint forced_size     : 1;

  /* Cached information if we go ahead and try to load
   * the icon.
   */
  GdkPixbuf *pixbuf;
  GError *load_error;
  gdouble scale;
  gboolean emblems_applied;

  guint ref_count;
};

typedef struct
{
  char *name;
  char *display_name;
  char *comment;
  char *example;

  /* In search order */
  GList *dirs;
} IconTheme;

typedef struct
{
  IconThemeDirType type;
  GQuark context;

  int size;
  int min_size;
  int max_size;
  int threshold;

  char *dir;
  char *subdir;
  int subdir_index;
  
  GtkIconCache *cache;
  
  GHashTable *icons;
  GHashTable *icon_data;
} IconThemeDir;

typedef struct
{
  char *svg_filename;
  char *no_svg_filename;
} UnthemedIcon;

typedef struct
{
  gint size;
  GdkPixbuf *pixbuf;
} BuiltinIcon;

typedef struct 
{
  char *dir;
  time_t mtime; /* 0 == not existing or not a dir */
  gboolean exists;

  GtkIconCache *cache;
} IconThemeDirMtime;

static void  gtk_icon_theme_finalize   (GObject              *object);
static void  theme_dir_destroy         (IconThemeDir         *dir);

static void         theme_destroy     (IconTheme        *theme);
static GtkIconInfo *theme_lookup_icon (IconTheme        *theme,
				       const char       *icon_name,
				       int               size,
				       gboolean          allow_svg,
				       gboolean          use_default_icons);
static void         theme_list_icons  (IconTheme        *theme,
				       GHashTable       *icons,
				       GQuark            context);
static void         theme_list_contexts  (IconTheme        *theme,
					  GHashTable       *contexts);
static void         theme_subdir_load (GtkIconTheme     *icon_theme,
				       IconTheme        *theme,
				       GKeyFile         *theme_file,
				       char             *subdir);
static void         do_theme_change   (GtkIconTheme     *icon_theme);

static void     blow_themes               (GtkIconTheme    *icon_themes);
static gboolean rescan_themes             (GtkIconTheme    *icon_themes);

static void  icon_data_free            (GtkIconData     *icon_data);
static void load_icon_data             (IconThemeDir    *dir,
			                const char      *path,
			                const char      *name);

static IconSuffix theme_dir_get_icon_suffix (IconThemeDir *dir,
					     const gchar  *icon_name,
					     gboolean     *has_icon_file);


static GtkIconInfo *icon_info_new             (void);
static GtkIconInfo *icon_info_new_builtin     (BuiltinIcon *icon);

static IconSuffix suffix_from_name (const char *name);

static BuiltinIcon *find_builtin_icon (const gchar *icon_name,
				       gint        size,
				       gint        *min_difference_p,
				       gboolean    *has_larger_p);

static guint signal_changed = 0;

static GHashTable *icon_theme_builtin_icons;

/* also used in gtkiconfactory.c */
GtkIconCache *_builtin_cache = NULL;
static GList *builtin_dirs = NULL;

G_DEFINE_TYPE (GtkIconTheme, gtk_icon_theme, G_TYPE_OBJECT)

/**
 * gtk_icon_theme_new:
 * 
 * Creates a new icon theme object. Icon theme objects are used
 * to lookup up an icon by name in a particular icon theme.
 * Usually, you'll want to use gtk_icon_theme_get_default()
 * or gtk_icon_theme_get_for_screen() rather than creating
 * a new icon theme object for scratch.
 * 
 * Return value: the newly created #GtkIconTheme object.
 *
 * Since: 2.4
 **/
GtkIconTheme *
gtk_icon_theme_new (void)
{
  return g_object_new (GTK_TYPE_ICON_THEME, NULL);
}

/**
 * gtk_icon_theme_get_default:
 * 
 * Gets the icon theme for the default screen. See
 * gtk_icon_theme_get_for_screen().
 *
 * Return value: (transfer none): A unique #GtkIconTheme associated with
 *  the default screen. This icon theme is associated with
 *  the screen and can be used as long as the screen
 *  is open. Do not ref or unref it.
 *
 * Since: 2.4
 **/
GtkIconTheme *
gtk_icon_theme_get_default (void)
{
  return gtk_icon_theme_get_for_screen (gdk_screen_get_default ());
}

/**
 * gtk_icon_theme_get_for_screen:
 * @screen: a #GdkScreen
 * 
 * Gets the icon theme object associated with @screen; if this
 * function has not previously been called for the given
 * screen, a new icon theme object will be created and
 * associated with the screen. Icon theme objects are
 * fairly expensive to create, so using this function
 * is usually a better choice than calling than gtk_icon_theme_new()
 * and setting the screen yourself; by using this function
 * a single icon theme object will be shared between users.
 *
 * Return value: (transfer none): A unique #GtkIconTheme associated with
 *  the given screen. This icon theme is associated with
 *  the screen and can be used as long as the screen
 *  is open. Do not ref or unref it.
 *
 * Since: 2.4
 **/
GtkIconTheme *
gtk_icon_theme_get_for_screen (GdkScreen *screen)
{
  GtkIconTheme *icon_theme;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);
  g_return_val_if_fail (!screen->closed, NULL);

  icon_theme = g_object_get_data (G_OBJECT (screen), "gtk-icon-theme");
  if (!icon_theme)
    {
      GtkIconThemePrivate *priv;

      icon_theme = gtk_icon_theme_new ();
      gtk_icon_theme_set_screen (icon_theme, screen);

      priv = icon_theme->priv;
      priv->is_screen_singleton = TRUE;

      g_object_set_data (G_OBJECT (screen), I_("gtk-icon-theme"), icon_theme);
    }

  return icon_theme;
}

static void
gtk_icon_theme_class_init (GtkIconThemeClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = gtk_icon_theme_finalize;

/**
 * GtkIconTheme::changed
 * @icon_theme: the icon theme
 * 
 * Emitted when the current icon theme is switched or GTK+ detects
 * that a change has occurred in the contents of the current
 * icon theme.
 **/
  signal_changed = g_signal_new (I_("changed"),
				 G_TYPE_FROM_CLASS (klass),
				 G_SIGNAL_RUN_LAST,
				 G_STRUCT_OFFSET (GtkIconThemeClass, changed),
				 NULL, NULL,
				 g_cclosure_marshal_VOID__VOID,
				 G_TYPE_NONE, 0);

  g_type_class_add_private (klass, sizeof (GtkIconThemePrivate));
}


/* Callback when the display that the icon theme is attached to
 * is closed; unset the screen, and if it's the unique theme
 * for the screen, drop the reference
 */
static void
display_closed (GdkDisplay   *display,
		gboolean      is_error,
		GtkIconTheme *icon_theme)
{
  GtkIconThemePrivate *priv = icon_theme->priv;
  GdkScreen *screen = priv->screen;
  gboolean was_screen_singleton = priv->is_screen_singleton;

  if (was_screen_singleton)
    {
      g_object_set_data (G_OBJECT (screen), I_("gtk-icon-theme"), NULL);
      priv->is_screen_singleton = FALSE;
    }

  gtk_icon_theme_set_screen (icon_theme, NULL);

  if (was_screen_singleton)
    {
      g_object_unref (icon_theme);
    }
}

static void
update_current_theme (GtkIconTheme *icon_theme)
{
#define theme_changed(_old, _new) \
  ((_old && !_new) || (!_old && _new) || \
   (_old && _new && strcmp (_old, _new) != 0))
  GtkIconThemePrivate *priv = icon_theme->priv;

  if (!priv->custom_theme)
    {
      gchar *theme = NULL;
      gchar *fallback_theme = NULL;
      gboolean changed = FALSE;

      if (priv->screen)
	{
	  GtkSettings *settings = gtk_settings_get_for_screen (priv->screen);
	  g_object_get (settings, 
			"gtk-icon-theme-name", &theme, 
			"gtk-fallback-icon-theme", &fallback_theme, NULL);
	}

      /* ensure that the current theme (even when just the default)
       * is searched before any fallback theme
       */
      if (!theme && fallback_theme)
	theme = g_strdup (DEFAULT_THEME_NAME);

      if (theme_changed (priv->current_theme, theme))
	{
	  g_free (priv->current_theme);
	  priv->current_theme = theme;
	  changed = TRUE;
	}
      else
	g_free (theme);

      if (theme_changed (priv->fallback_theme, fallback_theme))
	{
	  g_free (priv->fallback_theme);
	  priv->fallback_theme = fallback_theme;
	  changed = TRUE;
	}
      else
	g_free (fallback_theme);

      if (changed)
	do_theme_change (icon_theme);
    }
#undef theme_changed
}

/* Callback when the icon theme GtkSetting changes
 */
static void
theme_changed (GtkSettings  *settings,
	       GParamSpec   *pspec,
	       GtkIconTheme *icon_theme)
{
  update_current_theme (icon_theme);
}

static void
unset_screen (GtkIconTheme *icon_theme)
{
  GtkIconThemePrivate *priv = icon_theme->priv;
  GtkSettings *settings;
  GdkDisplay *display;
  
  if (priv->screen)
    {
      settings = gtk_settings_get_for_screen (priv->screen);
      display = gdk_screen_get_display (priv->screen);
      
      g_signal_handlers_disconnect_by_func (display,
					    (gpointer) display_closed,
					    icon_theme);
      g_signal_handlers_disconnect_by_func (settings,
					    (gpointer) theme_changed,
					    icon_theme);

      priv->screen = NULL;
    }
}

/**
 * gtk_icon_theme_set_screen:
 * @icon_theme: a #GtkIconTheme
 * @screen: a #GdkScreen
 * 
 * Sets the screen for an icon theme; the screen is used
 * to track the user's currently configured icon theme,
 * which might be different for different screens.
 *
 * Since: 2.4
 **/
void
gtk_icon_theme_set_screen (GtkIconTheme *icon_theme,
			   GdkScreen    *screen)
{
  GtkIconThemePrivate *priv;
  GtkSettings *settings;
  GdkDisplay *display;

  g_return_if_fail (GTK_ICON_THEME (icon_theme));
  g_return_if_fail (screen == NULL || GDK_IS_SCREEN (screen));

  priv = icon_theme->priv;

  unset_screen (icon_theme);
  
  if (screen)
    {
      display = gdk_screen_get_display (screen);
      settings = gtk_settings_get_for_screen (screen);
      
      priv->screen = screen;
      
      g_signal_connect (display, "closed",
			G_CALLBACK (display_closed), icon_theme);
      g_signal_connect (settings, "notify::gtk-icon-theme-name",
			G_CALLBACK (theme_changed), icon_theme);
      g_signal_connect (settings, "notify::gtk-fallback-icon-theme-name",
			G_CALLBACK (theme_changed), icon_theme);
    }

  update_current_theme (icon_theme);
}

/* Checks whether a loader for SVG files has been registered
 * with GdkPixbuf.
 */
static gboolean
pixbuf_supports_svg (void)
{
  GSList *formats;
  GSList *tmp_list;
  static gint found_svg = -1;

  if (found_svg != -1)
    return found_svg;

  formats = gdk_pixbuf_get_formats ();

  found_svg = FALSE; 
  for (tmp_list = formats; tmp_list && !found_svg; tmp_list = tmp_list->next)
    {
      gchar **mime_types = gdk_pixbuf_format_get_mime_types (tmp_list->data);
      gchar **mime_type;
      
      for (mime_type = mime_types; *mime_type && !found_svg; mime_type++)
	{
	  if (strcmp (*mime_type, "image/svg") == 0)
	    found_svg = TRUE;
	}

      g_strfreev (mime_types);
    }

  g_slist_free (formats);
  
  return found_svg;
}

static void
gtk_icon_theme_init (GtkIconTheme *icon_theme)
{
  GtkIconThemePrivate *priv;
  const gchar * const *xdg_data_dirs;
  int i, j;
  
  priv = g_type_instance_get_private ((GTypeInstance *)icon_theme,
				      GTK_TYPE_ICON_THEME);
  icon_theme->priv = priv;

  priv->custom_theme = FALSE;

  xdg_data_dirs = g_get_system_data_dirs ();
  for (i = 0; xdg_data_dirs[i]; i++) ;

  priv->search_path_len = 2 * i + 2;
  
  priv->search_path = g_new (char *, priv->search_path_len);
  
  i = 0;
  priv->search_path[i++] = g_build_filename (g_get_home_dir (), ".icons", NULL);
  priv->search_path[i++] = g_build_filename (g_get_user_data_dir (), "icons", NULL);
  
  for (j = 0; xdg_data_dirs[j]; j++) 
    priv->search_path[i++] = g_build_filename (xdg_data_dirs[j], "icons", NULL);

  for (j = 0; xdg_data_dirs[j]; j++) 
    priv->search_path[i++] = g_build_filename (xdg_data_dirs[j], "pixmaps", NULL);

  priv->themes_valid = FALSE;
  priv->themes = NULL;
  priv->unthemed_icons = NULL;
  
  priv->pixbuf_supports_svg = pixbuf_supports_svg ();
}

static void
free_dir_mtime (IconThemeDirMtime *dir_mtime)
{
  if (dir_mtime->cache)
    _gtk_icon_cache_unref (dir_mtime->cache);

  g_free (dir_mtime->dir);
  g_slice_free (IconThemeDirMtime, dir_mtime);

}

static gboolean
reset_styles_idle (gpointer user_data)
{
  GtkIconTheme *icon_theme;
  GtkIconThemePrivate *priv;

  icon_theme = GTK_ICON_THEME (user_data);
  priv = icon_theme->priv;

  if (priv->screen && priv->is_screen_singleton)
    {
      GtkSettings *settings = gtk_settings_get_for_screen (priv->screen);
      gtk_rc_reset_styles (settings);
    }

  priv->reset_styles_idle = 0;

  return FALSE;
}

static void
do_theme_change (GtkIconTheme *icon_theme)
{
  GtkIconThemePrivate *priv = icon_theme->priv;

  if (!priv->themes_valid)
    return;
  
  GTK_NOTE (ICONTHEME, 
	    g_print ("change to icon theme \"%s\"\n", priv->current_theme));
  blow_themes (icon_theme);
  g_signal_emit (icon_theme, signal_changed, 0);

  if (!priv->reset_styles_idle)
    priv->reset_styles_idle = 
      gdk_threads_add_idle_full (GTK_PRIORITY_RESIZE - 2, 
		       reset_styles_idle, icon_theme, NULL);
}

static void
blow_themes (GtkIconTheme *icon_theme)
{
  GtkIconThemePrivate *priv = icon_theme->priv;
  
  if (priv->themes_valid)
    {
      g_hash_table_destroy (priv->all_icons);
      g_list_foreach (priv->themes, (GFunc)theme_destroy, NULL);
      g_list_free (priv->themes);
      g_list_foreach (priv->dir_mtimes, (GFunc)free_dir_mtime, NULL);
      g_list_free (priv->dir_mtimes);
      g_hash_table_destroy (priv->unthemed_icons);
    }
  priv->themes = NULL;
  priv->unthemed_icons = NULL;
  priv->dir_mtimes = NULL;
  priv->all_icons = NULL;
  priv->themes_valid = FALSE;
}

static void
gtk_icon_theme_finalize (GObject *object)
{
  GtkIconTheme *icon_theme;
  GtkIconThemePrivate *priv;
  int i;

  icon_theme = GTK_ICON_THEME (object);
  priv = icon_theme->priv;

  if (priv->reset_styles_idle)
    {
      g_source_remove (priv->reset_styles_idle);
      priv->reset_styles_idle = 0;
    }

  unset_screen (icon_theme);

  g_free (priv->current_theme);
  priv->current_theme = NULL;

  for (i = 0; i < priv->search_path_len; i++)
    g_free (priv->search_path[i]);

  g_free (priv->search_path);
  priv->search_path = NULL;

  blow_themes (icon_theme);

  G_OBJECT_CLASS (gtk_icon_theme_parent_class)->finalize (object);  
}

/**
 * gtk_icon_theme_set_search_path:
 * @icon_theme: a #GtkIconTheme
 * @path: (array length=n_elements) (element-type filename): array of
 *     directories that are searched for icon themes
 * @n_elements: number of elements in @path.
 * 
 * Sets the search path for the icon theme object. When looking
 * for an icon theme, GTK+ will search for a subdirectory of
 * one or more of the directories in @path with the same name
 * as the icon theme. (Themes from multiple of the path elements
 * are combined to allow themes to be extended by adding icons
 * in the user's home directory.)
 *
 * In addition if an icon found isn't found either in the current
 * icon theme or the default icon theme, and an image file with
 * the right name is found directly in one of the elements of
 * @path, then that image will be used for the icon name.
 * (This is legacy feature, and new icons should be put
 * into the default icon theme, which is called DEFAULT_THEME_NAME,
 * rather than directly on the icon path.)
 *
 * Since: 2.4
 **/
void
gtk_icon_theme_set_search_path (GtkIconTheme *icon_theme,
				const gchar  *path[],
				gint          n_elements)
{
  GtkIconThemePrivate *priv;
  gint i;

  g_return_if_fail (GTK_IS_ICON_THEME (icon_theme));

  priv = icon_theme->priv;
  for (i = 0; i < priv->search_path_len; i++)
    g_free (priv->search_path[i]);

  g_free (priv->search_path);

  priv->search_path = g_new (gchar *, n_elements);
  priv->search_path_len = n_elements;

  for (i = 0; i < priv->search_path_len; i++)
    priv->search_path[i] = g_strdup (path[i]);

  do_theme_change (icon_theme);
}


/**
 * gtk_icon_theme_get_search_path:
 * @icon_theme: a #GtkIconTheme
 * @path: (allow-none) (array length=n_elements) (out): location to store a list of icon theme path directories or %NULL
 *        The stored value should be freed with g_strfreev().
 * @n_elements: location to store number of elements
 *              in @path, or %NULL
 * 
 * Gets the current search path. See gtk_icon_theme_set_search_path().
 *
 * Since: 2.4
 **/
void
gtk_icon_theme_get_search_path (GtkIconTheme      *icon_theme,
				gchar            **path[],
				gint              *n_elements)
{
  GtkIconThemePrivate *priv;
  int i;

  g_return_if_fail (GTK_IS_ICON_THEME (icon_theme));

  priv = icon_theme->priv;

  if (n_elements)
    *n_elements = priv->search_path_len;
  
  if (path)
    {
      *path = g_new (gchar *, priv->search_path_len + 1);
      for (i = 0; i < priv->search_path_len; i++)
	(*path)[i] = g_strdup (priv->search_path[i]);
      (*path)[i] = NULL;
    }
}

/**
 * gtk_icon_theme_append_search_path:
 * @icon_theme: a #GtkIconTheme
 * @path: directory name to append to the icon path
 * 
 * Appends a directory to the search path. 
 * See gtk_icon_theme_set_search_path(). 
 *
 * Since: 2.4
 **/
void
gtk_icon_theme_append_search_path (GtkIconTheme *icon_theme,
				   const gchar  *path)
{
  GtkIconThemePrivate *priv;

  g_return_if_fail (GTK_IS_ICON_THEME (icon_theme));
  g_return_if_fail (path != NULL);

  priv = icon_theme->priv;
  
  priv->search_path_len++;

  priv->search_path = g_renew (gchar *, priv->search_path, priv->search_path_len);
  priv->search_path[priv->search_path_len-1] = g_strdup (path);

  do_theme_change (icon_theme);
}

/**
 * gtk_icon_theme_prepend_search_path:
 * @icon_theme: a #GtkIconTheme
 * @path: directory name to prepend to the icon path
 * 
 * Prepends a directory to the search path. 
 * See gtk_icon_theme_set_search_path().
 *
 * Since: 2.4
 **/
void
gtk_icon_theme_prepend_search_path (GtkIconTheme *icon_theme,
				    const gchar  *path)
{
  GtkIconThemePrivate *priv;
  int i;

  g_return_if_fail (GTK_IS_ICON_THEME (icon_theme));
  g_return_if_fail (path != NULL);

  priv = icon_theme->priv;
  
  priv->search_path_len++;
  priv->search_path = g_renew (gchar *, priv->search_path, priv->search_path_len);

  for (i = priv->search_path_len - 1; i > 0; i--)
    priv->search_path[i] = priv->search_path[i - 1];
  
  priv->search_path[0] = g_strdup (path);

  do_theme_change (icon_theme);
}

/**
 * gtk_icon_theme_set_custom_theme:
 * @icon_theme: a #GtkIconTheme
 * @theme_name: name of icon theme to use instead of configured theme,
 *   or %NULL to unset a previously set custom theme
 * 
 * Sets the name of the icon theme that the #GtkIconTheme object uses
 * overriding system configuration. This function cannot be called
 * on the icon theme objects returned from gtk_icon_theme_get_default()
 * and gtk_icon_theme_get_for_screen().
 *
 * Since: 2.4
 **/
void
gtk_icon_theme_set_custom_theme (GtkIconTheme *icon_theme,
				 const gchar  *theme_name)
{
  GtkIconThemePrivate *priv;

  g_return_if_fail (GTK_IS_ICON_THEME (icon_theme));

  priv = icon_theme->priv;

  g_return_if_fail (!priv->is_screen_singleton);
  
  if (theme_name != NULL)
    {
      priv->custom_theme = TRUE;
      if (!priv->current_theme || strcmp (theme_name, priv->current_theme) != 0)
	{
	  g_free (priv->current_theme);
	  priv->current_theme = g_strdup (theme_name);

	  do_theme_change (icon_theme);
	}
    }
  else
    {
      if (priv->custom_theme)
	{
	  priv->custom_theme = FALSE;

	  update_current_theme (icon_theme);
	}
    }
}

static void
insert_theme (GtkIconTheme *icon_theme, const char *theme_name)
{
  int i;
  GList *l;
  char **dirs;
  char **themes;
  GtkIconThemePrivate *priv;
  IconTheme *theme = NULL;
  char *path;
  GKeyFile *theme_file;
  GError *error = NULL;
  IconThemeDirMtime *dir_mtime;
  GStatBuf stat_buf;
  
  priv = icon_theme->priv;

  for (l = priv->themes; l != NULL; l = l->next)
    {
      theme = l->data;
      if (strcmp (theme->name, theme_name) == 0)
	return;
    }
  
  for (i = 0; i < priv->search_path_len; i++)
    {
      path = g_build_filename (priv->search_path[i],
			       theme_name,
			       NULL);
      dir_mtime = g_slice_new (IconThemeDirMtime);
      dir_mtime->cache = NULL;
      dir_mtime->dir = path;
      if (g_stat (path, &stat_buf) == 0 && S_ISDIR (stat_buf.st_mode))
	{
	  dir_mtime->mtime = stat_buf.st_mtime;
	  dir_mtime->exists = TRUE;
	}
      else
	{
	  dir_mtime->mtime = 0;
	  dir_mtime->exists = FALSE;
	}

      priv->dir_mtimes = g_list_prepend (priv->dir_mtimes, dir_mtime);
    }
  priv->dir_mtimes = g_list_reverse (priv->dir_mtimes);

  theme_file = NULL;
  for (i = 0; i < priv->search_path_len && !theme_file; i++)
    {
      path = g_build_filename (priv->search_path[i],
			       theme_name,
			       "index.theme",
			       NULL);
      if (g_file_test (path, G_FILE_TEST_IS_REGULAR)) 
	{
	  theme_file = g_key_file_new ();
	  g_key_file_set_list_separator (theme_file, ',');
	  g_key_file_load_from_file (theme_file, path, 0, &error);
	  if (error)
	    {
	      g_key_file_free (theme_file);
	      theme_file = NULL;
	      g_error_free (error);
	      error = NULL;
	    }
	}
      g_free (path);
    }

  if (theme_file || strcmp (theme_name, DEFAULT_THEME_NAME) == 0)
    {
      theme = g_new0 (IconTheme, 1);
      theme->name = g_strdup (theme_name);
      priv->themes = g_list_prepend (priv->themes, theme);
    }

  if (theme_file == NULL)
    return;

  theme->display_name = 
    g_key_file_get_locale_string (theme_file, "Icon Theme", "Name", NULL, NULL);
  if (!theme->display_name)
    g_warning ("Theme file for %s has no name\n", theme_name);

  dirs = g_key_file_get_string_list (theme_file, "Icon Theme", "Directories", NULL, NULL);
  if (!dirs)
    {
      g_warning ("Theme file for %s has no directories\n", theme_name);
      priv->themes = g_list_remove (priv->themes, theme);
      g_free (theme->name);
      g_free (theme->display_name);
      g_free (theme);
      g_key_file_free (theme_file);
      return;
    }
  
  theme->comment = 
    g_key_file_get_locale_string (theme_file, 
				  "Icon Theme", "Comment",
				  NULL, NULL);
  theme->example = 
    g_key_file_get_string (theme_file, 
			   "Icon Theme", "Example",
			   NULL);

  theme->dirs = NULL;
  for (i = 0; dirs[i] != NULL; i++)
    theme_subdir_load (icon_theme, theme, theme_file, dirs[i]);

  g_strfreev (dirs);

  theme->dirs = g_list_reverse (theme->dirs);

  themes = g_key_file_get_string_list (theme_file,
				       "Icon Theme",
				       "Inherits",
				       NULL,
				       NULL);
  if (themes)
    {
      for (i = 0; themes[i] != NULL; i++)
	insert_theme (icon_theme, themes[i]);
      
      g_strfreev (themes);
    }

  g_key_file_free (theme_file);
}

static void
free_unthemed_icon (UnthemedIcon *unthemed_icon)
{
  g_free (unthemed_icon->svg_filename);
  g_free (unthemed_icon->no_svg_filename);
  g_slice_free (UnthemedIcon, unthemed_icon);
}

static char *
strip_suffix (const char *filename)
{
  const char *dot;

  dot = strrchr (filename, '.');

  if (dot == NULL)
    return g_strdup (filename);

  return g_strndup (filename, dot - filename);
}

static void
load_themes (GtkIconTheme *icon_theme)
{
  GtkIconThemePrivate *priv;
  GDir *gdir;
  int base;
  char *dir;
  const char *file;
  UnthemedIcon *unthemed_icon;
  IconSuffix old_suffix, new_suffix;
  GTimeVal tv;
  IconThemeDirMtime *dir_mtime;
  GStatBuf stat_buf;
  
  priv = icon_theme->priv;

  priv->all_icons = g_hash_table_new (g_str_hash, g_str_equal);
  
  if (priv->current_theme)
    insert_theme (icon_theme, priv->current_theme);

  /* Always look in the "default" icon theme, and in a fallback theme */
  if (priv->fallback_theme)
    insert_theme (icon_theme, priv->fallback_theme);
  insert_theme (icon_theme, DEFAULT_THEME_NAME);
  priv->themes = g_list_reverse (priv->themes);


  priv->unthemed_icons = g_hash_table_new_full (g_str_hash, g_str_equal,
						g_free, (GDestroyNotify)free_unthemed_icon);

  for (base = 0; base < icon_theme->priv->search_path_len; base++)
    {
      dir = icon_theme->priv->search_path[base];

      dir_mtime = g_slice_new (IconThemeDirMtime);
      priv->dir_mtimes = g_list_append (priv->dir_mtimes, dir_mtime);
      
      dir_mtime->dir = g_strdup (dir);
      dir_mtime->mtime = 0;
      dir_mtime->exists = FALSE;
      dir_mtime->cache = NULL;

      if (g_stat (dir, &stat_buf) != 0 || !S_ISDIR (stat_buf.st_mode))
	continue;
      dir_mtime->mtime = stat_buf.st_mtime;
      dir_mtime->exists = TRUE;

      dir_mtime->cache = _gtk_icon_cache_new_for_path (dir);
      if (dir_mtime->cache != NULL)
	continue;

      gdir = g_dir_open (dir, 0, NULL);
      if (gdir == NULL)
	continue;

      while ((file = g_dir_read_name (gdir)))
	{
	  new_suffix = suffix_from_name (file);
	  
	  if (new_suffix != ICON_SUFFIX_NONE)
	    {
	      char *abs_file;
	      char *base_name;

	      abs_file = g_build_filename (dir, file, NULL);
	      base_name = strip_suffix (file);

	      if ((unthemed_icon = g_hash_table_lookup (priv->unthemed_icons,
							base_name)))
		{
		  if (new_suffix == ICON_SUFFIX_SVG)
		    {
		      if (unthemed_icon->svg_filename)
			g_free (abs_file);
		      else
			unthemed_icon->svg_filename = abs_file;
		    }
		  else
		    {
		      if (unthemed_icon->no_svg_filename)
			{
			  old_suffix = suffix_from_name (unthemed_icon->no_svg_filename);
			  if (new_suffix > old_suffix)
			    {
			      g_free (unthemed_icon->no_svg_filename);
			      unthemed_icon->no_svg_filename = abs_file;			      
			    }
			  else
			    g_free (abs_file);
			}
		      else
			unthemed_icon->no_svg_filename = abs_file;			      
		    }

		  g_free (base_name);
		}
	      else
		{
		  unthemed_icon = g_slice_new0 (UnthemedIcon);
		  
		  if (new_suffix == ICON_SUFFIX_SVG)
		    unthemed_icon->svg_filename = abs_file;
		  else
		    unthemed_icon->no_svg_filename = abs_file;

		  g_hash_table_insert (priv->unthemed_icons,
				       base_name,
				       unthemed_icon);
		  g_hash_table_insert (priv->all_icons,
				       base_name, NULL);
		}
	    }
	}
      g_dir_close (gdir);
    }

  priv->themes_valid = TRUE;
  
  g_get_current_time(&tv);
  priv->last_stat_time = tv.tv_sec;
}

void
_gtk_icon_theme_ensure_builtin_cache (void)
{
  static gboolean initialized = FALSE;
  IconThemeDir *dir;
  static IconThemeDir dirs[5] = 
    {
      { ICON_THEME_DIR_THRESHOLD, 0, 16, 16, 16, 2, NULL, "16", -1, NULL, NULL, NULL },
      { ICON_THEME_DIR_THRESHOLD, 0, 20, 20, 20, 2, NULL, "20", -1,  NULL, NULL, NULL },
      { ICON_THEME_DIR_THRESHOLD, 0, 24, 24, 24, 2, NULL, "24", -1, NULL, NULL, NULL },
      { ICON_THEME_DIR_THRESHOLD, 0, 32, 32, 32, 2, NULL, "32", -1, NULL, NULL, NULL },
      { ICON_THEME_DIR_THRESHOLD, 0, 48, 48, 48, 2, NULL, "48", -1, NULL, NULL, NULL }
    };
  gint i;

  if (!initialized)
    {
      initialized = TRUE;

      _builtin_cache = _gtk_icon_cache_new ((gchar *)builtin_icons);

      for (i = 0; i < G_N_ELEMENTS (dirs); i++)
	{
	  dir = &(dirs[i]);
	  dir->cache = _gtk_icon_cache_ref (_builtin_cache);
          dir->subdir_index = _gtk_icon_cache_get_directory_index (dir->cache, dir->subdir);

	  builtin_dirs = g_list_append (builtin_dirs, dir);
	}
    }
}

static void
ensure_valid_themes (GtkIconTheme *icon_theme)
{
  GtkIconThemePrivate *priv = icon_theme->priv;
  GTimeVal tv;
  gboolean was_valid = priv->themes_valid;

  if (priv->loading_themes)
    return;
  priv->loading_themes = TRUE;

  _gtk_icon_theme_ensure_builtin_cache ();

  if (priv->themes_valid)
    {
      g_get_current_time (&tv);

      if (ABS (tv.tv_sec - priv->last_stat_time) > 5 &&
	  rescan_themes (icon_theme))
	blow_themes (icon_theme);
    }
  
  if (!priv->themes_valid)
    {
      load_themes (icon_theme);

      if (was_valid)
	{
	  g_signal_emit (icon_theme, signal_changed, 0);

	  if (!priv->check_reload && priv->screen)
	    {	  
	      static GdkAtom atom_iconthemes = GDK_NONE;
	      GdkEvent *event = gdk_event_new (GDK_CLIENT_EVENT);
	      int i;

	      if (!atom_iconthemes)
		atom_iconthemes = gdk_atom_intern_static_string ("_GTK_LOAD_ICONTHEMES");

	      for (i = 0; i < 5; i++)
		event->client.data.l[i] = 0;
	      event->client.data_format = 32;
	      event->client.message_type = atom_iconthemes;

	      gdk_screen_broadcast_client_message (priv->screen, event);
	    }
	}
    }

  priv->loading_themes = FALSE;
}

static GtkIconInfo *
choose_icon (GtkIconTheme       *icon_theme,
	     const gchar        *icon_names[],
	     gint                size,
	     GtkIconLookupFlags  flags)
{
  GtkIconThemePrivate *priv;
  GList *l;
  GtkIconInfo *icon_info = NULL;
  UnthemedIcon *unthemed_icon = NULL;
  gboolean allow_svg;
  gboolean use_builtin;
  gint i;

  priv = icon_theme->priv;

  if (flags & GTK_ICON_LOOKUP_NO_SVG)
    allow_svg = FALSE;
  else if (flags & GTK_ICON_LOOKUP_FORCE_SVG)
    allow_svg = TRUE;
  else
    allow_svg = priv->pixbuf_supports_svg;

  use_builtin = flags & GTK_ICON_LOOKUP_USE_BUILTIN;
  
  ensure_valid_themes (icon_theme);

  for (l = priv->themes; l; l = l->next)
    {
      IconTheme *theme = l->data;
      
      for (i = 0; icon_names[i]; i++)
        {
          icon_info = theme_lookup_icon (theme, icon_names[i], size, allow_svg, use_builtin);
          if (icon_info)
            goto out;
        }
    }

  for (i = 0; icon_names[i]; i++)
    {
      unthemed_icon = g_hash_table_lookup (priv->unthemed_icons, icon_names[i]);
      if (unthemed_icon)
        break;
    }
#ifdef G_OS_WIN32
  /* Still not found an icon, check if reference to a Win32 resource */
  if (!unthemed_icon)
    {
      gchar **resources;
      HICON hIcon = NULL;
      
      resources = g_strsplit (icon_names[0], ",", 0);
      if (resources[0])
	{
	  wchar_t *wfile = g_utf8_to_utf16 (resources[0], -1, NULL, NULL, NULL);
	  ExtractIconExW (wfile, resources[1] ? atoi (resources[1]) : 0, &hIcon, NULL, 1);
	  g_free (wfile);
	}
      
      if (hIcon)
	{
	  icon_info = icon_info_new ();
	  icon_info->cache_pixbuf = gdk_win32_icon_to_pixbuf_libgtk_only (hIcon);
	  DestroyIcon (hIcon);
          icon_info->dir_type = ICON_THEME_DIR_UNTHEMED;
          icon_info->dir_size = size;
	}
      g_strfreev (resources);
    }
#endif

  if (unthemed_icon)
    {
      icon_info = icon_info_new ();

      /* A SVG icon, when allowed, beats out a XPM icon, but not
       * a PNG icon
       */
      if (allow_svg &&
	  unthemed_icon->svg_filename &&
	  (!unthemed_icon->no_svg_filename ||
	   suffix_from_name (unthemed_icon->no_svg_filename) != ICON_SUFFIX_PNG))
	icon_info->filename = g_strdup (unthemed_icon->svg_filename);
      else if (unthemed_icon->no_svg_filename)
	icon_info->filename = g_strdup (unthemed_icon->no_svg_filename);
#if defined (G_OS_WIN32) && !defined (_WIN64)
      icon_info->cp_filename = g_locale_from_utf8 (icon_info->filename,
						   -1, NULL, NULL, NULL);
#endif

      icon_info->dir_type = ICON_THEME_DIR_UNTHEMED;
      icon_info->dir_size = size;
    }

 out:
  if (icon_info) 
    {
      icon_info->desired_size = size;
      icon_info->forced_size = (flags & GTK_ICON_LOOKUP_FORCE_SIZE) != 0;
    }
  else
    {
      static gboolean check_for_default_theme = TRUE;
      char *default_theme_path;
      gboolean found = FALSE;
      unsigned i;

      if (check_for_default_theme)
	{
	  check_for_default_theme = FALSE;

	  for (i = 0; !found && i < priv->search_path_len; i++)
	    {
	      default_theme_path = g_build_filename (priv->search_path[i],
						     DEFAULT_THEME_NAME,
						     "index.theme",
						     NULL);
	      found = g_file_test (default_theme_path, G_FILE_TEST_IS_REGULAR);
	      g_free (default_theme_path);
	    }

	  if (!found)
	    {
	      g_warning (_("Could not find the icon '%s'. The '%s' theme\n"
			   "was not found either, perhaps you need to install it.\n"
			   "You can get a copy from:\n"
			   "\t%s"),
			 icon_names[0], DEFAULT_THEME_NAME, "http://icon-theme.freedesktop.org/releases");
	    }
	}
    }

  return icon_info;
}


/**
 * gtk_icon_theme_lookup_icon:
 * @icon_theme: a #GtkIconTheme
 * @icon_name: the name of the icon to lookup
 * @size: desired icon size
 * @flags: flags modifying the behavior of the icon lookup
 * 
 * Looks up a named icon and returns a structure containing
 * information such as the filename of the icon. The icon
 * can then be rendered into a pixbuf using
 * gtk_icon_info_load_icon(). (gtk_icon_theme_load_icon()
 * combines these two steps if all you need is the pixbuf.)
 * 
 * Return value: a #GtkIconInfo structure containing information
 * about the icon, or %NULL if the icon wasn't found. Free with
 * gtk_icon_info_free()
 *
 * Since: 2.4
 */
GtkIconInfo *
gtk_icon_theme_lookup_icon (GtkIconTheme       *icon_theme,
			    const gchar        *icon_name,
			    gint                size,
			    GtkIconLookupFlags  flags)
{
  GtkIconInfo *info;

  g_return_val_if_fail (GTK_IS_ICON_THEME (icon_theme), NULL);
  g_return_val_if_fail (icon_name != NULL, NULL);
  g_return_val_if_fail ((flags & GTK_ICON_LOOKUP_NO_SVG) == 0 ||
			(flags & GTK_ICON_LOOKUP_FORCE_SVG) == 0, NULL);

  GTK_NOTE (ICONTHEME, 
	    g_print ("gtk_icon_theme_lookup_icon %s\n", icon_name));

  if (flags & GTK_ICON_LOOKUP_GENERIC_FALLBACK)
    {
      gchar **names;
      gint dashes, i;
      gchar *p;
 
      dashes = 0;
      for (p = (gchar *) icon_name; *p; p++)
        if (*p == '-')
          dashes++;

      names = g_new (gchar *, dashes + 2);
      names[0] = g_strdup (icon_name);
      for (i = 1; i <= dashes; i++)
        {
          names[i] = g_strdup (names[i - 1]);
          p = strrchr (names[i], '-');
          *p = '\0';
        }
      names[dashes + 1] = NULL;
   
      info = choose_icon (icon_theme, (const gchar **) names, size, flags);
      
      g_strfreev (names);
    }
  else 
    {
      const gchar *names[2];
      
      names[0] = icon_name;
      names[1] = NULL;

      info = choose_icon (icon_theme, names, size, flags);
    }

  return info;
}

/**
 * gtk_icon_theme_choose_icon:
 * @icon_theme: a #GtkIconTheme
 * @icon_names: (array zero-terminated=1): %NULL-terminated array of
 *     icon names to lookup
 * @size: desired icon size
 * @flags: flags modifying the behavior of the icon lookup
 * 
 * Looks up a named icon and returns a structure containing
 * information such as the filename of the icon. The icon
 * can then be rendered into a pixbuf using
 * gtk_icon_info_load_icon(). (gtk_icon_theme_load_icon()
 * combines these two steps if all you need is the pixbuf.)
 *
 * If @icon_names contains more than one name, this function 
 * tries them all in the given order before falling back to 
 * inherited icon themes.
 * 
 * Return value: a #GtkIconInfo structure containing information
 * about the icon, or %NULL if the icon wasn't found. Free with
 * gtk_icon_info_free()
 *
 * Since: 2.12
 */
GtkIconInfo *
gtk_icon_theme_choose_icon (GtkIconTheme       *icon_theme,
			    const gchar        *icon_names[],
			    gint                size,
			    GtkIconLookupFlags  flags)
{
  g_return_val_if_fail (GTK_IS_ICON_THEME (icon_theme), NULL);
  g_return_val_if_fail (icon_names != NULL, NULL);
  g_return_val_if_fail ((flags & GTK_ICON_LOOKUP_NO_SVG) == 0 ||
			(flags & GTK_ICON_LOOKUP_FORCE_SVG) == 0, NULL);

  return choose_icon (icon_theme, icon_names, size, flags);
}

/* Error quark */
GQuark
gtk_icon_theme_error_quark (void)
{
  return g_quark_from_static_string ("gtk-icon-theme-error-quark");
}

/**
 * gtk_icon_theme_load_icon:
 * @icon_theme: a #GtkIconTheme
 * @icon_name: the name of the icon to lookup
 * @size: the desired icon size. The resulting icon may not be
 *        exactly this size; see gtk_icon_info_load_icon().
 * @flags: flags modifying the behavior of the icon lookup
 * @error: (allow-none): Location to store error information on failure, or %NULL.
 * 
 * Looks up an icon in an icon theme, scales it to the given size
 * and renders it into a pixbuf. This is a convenience function;
 * if more details about the icon are needed, use
 * gtk_icon_theme_lookup_icon() followed by gtk_icon_info_load_icon().
 *
 * Note that you probably want to listen for icon theme changes and
 * update the icon. This is usually done by connecting to the 
 * GtkWidget::style-set signal. If for some reason you do not want to
 * update the icon when the icon theme changes, you should consider
 * using gdk_pixbuf_copy() to make a private copy of the pixbuf
 * returned by this function. Otherwise GTK+ may need to keep the old 
 * icon theme loaded, which would be a waste of memory.
 * 
 * Return value: (transfer full): the rendered icon; this may be a newly
 *  created icon or a new reference to an internal icon, so you must not modify
 *  the icon. Use g_object_unref() to release your reference to the
 *  icon. %NULL if the icon isn't found.
 *
 * Since: 2.4
 **/
GdkPixbuf *
gtk_icon_theme_load_icon (GtkIconTheme         *icon_theme,
			  const gchar          *icon_name,
			  gint                  size,
			  GtkIconLookupFlags    flags,
			  GError              **error)
{
  GtkIconInfo *icon_info;
  GdkPixbuf *pixbuf = NULL;
  
  g_return_val_if_fail (GTK_IS_ICON_THEME (icon_theme), NULL);
  g_return_val_if_fail (icon_name != NULL, NULL);
  g_return_val_if_fail ((flags & GTK_ICON_LOOKUP_NO_SVG) == 0 ||
			(flags & GTK_ICON_LOOKUP_FORCE_SVG) == 0, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);
  
  icon_info = gtk_icon_theme_lookup_icon (icon_theme, icon_name, size,
				          flags | GTK_ICON_LOOKUP_USE_BUILTIN);
  if (!icon_info)
    {
      g_set_error (error, GTK_ICON_THEME_ERROR,  GTK_ICON_THEME_NOT_FOUND,
		   _("Icon '%s' not present in theme"), icon_name);
      return NULL;
    }

  pixbuf = gtk_icon_info_load_icon (icon_info, error);
  gtk_icon_info_free (icon_info);

  return pixbuf;
}

/**
 * gtk_icon_theme_has_icon:
 * @icon_theme: a #GtkIconTheme
 * @icon_name: the name of an icon
 * 
 * Checks whether an icon theme includes an icon
 * for a particular name.
 * 
 * Return value: %TRUE if @icon_theme includes an
 *  icon for @icon_name.
 *
 * Since: 2.4
 **/
gboolean 
gtk_icon_theme_has_icon (GtkIconTheme *icon_theme,
			 const char   *icon_name)
{
  GtkIconThemePrivate *priv;
  GList *l;

  g_return_val_if_fail (GTK_IS_ICON_THEME (icon_theme), FALSE);
  g_return_val_if_fail (icon_name != NULL, FALSE);

  priv = icon_theme->priv;
  
  ensure_valid_themes (icon_theme);

  for (l = priv->dir_mtimes; l; l = l->next)
    {
      IconThemeDirMtime *dir_mtime = l->data;
      GtkIconCache *cache = dir_mtime->cache;
      
      if (cache && _gtk_icon_cache_has_icon (cache, icon_name))
	return TRUE;
    }

  if (g_hash_table_lookup_extended (priv->all_icons,
				    icon_name, NULL, NULL))
    return TRUE;

  if (_builtin_cache &&
      _gtk_icon_cache_has_icon (_builtin_cache, icon_name))
    return TRUE;

  if (icon_theme_builtin_icons &&
      g_hash_table_lookup_extended (icon_theme_builtin_icons,
				    icon_name, NULL, NULL))
    return TRUE;

  return FALSE;
}

static void
add_size (gpointer  key,
	  gpointer  value,
	  gpointer  user_data)
{
  gint **res_p = user_data;

  **res_p = GPOINTER_TO_INT (key);

  (*res_p)++;
}

/**
 * gtk_icon_theme_get_icon_sizes:
 * @icon_theme: a #GtkIconTheme
 * @icon_name: the name of an icon
 * 
 * Returns an array of integers describing the sizes at which
 * the icon is available without scaling. A size of -1 means 
 * that the icon is available in a scalable format. The array 
 * is zero-terminated.
 * 
 * Return value: (array zero-terminated=1): An newly allocated array
 * describing the sizes at which the icon is available. The array
 * should be freed with g_free() when it is no longer needed.
 *
 * Since: 2.6
 **/
gint *
gtk_icon_theme_get_icon_sizes (GtkIconTheme *icon_theme,
			       const char   *icon_name)
{
  GList *l, *d, *icons;
  GHashTable *sizes;
  gint *result, *r;
  guint suffix;  
  GtkIconThemePrivate *priv;

  g_return_val_if_fail (GTK_IS_ICON_THEME (icon_theme), NULL);
  
  priv = icon_theme->priv;

  ensure_valid_themes (icon_theme);

  sizes = g_hash_table_new (g_direct_hash, g_direct_equal);

  for (l = priv->themes; l; l = l->next)
    {
      IconTheme *theme = l->data;
      for (d = theme->dirs; d; d = d->next)
	{
	  IconThemeDir *dir = d->data;

          if (dir->type != ICON_THEME_DIR_SCALABLE && g_hash_table_lookup_extended (sizes, GINT_TO_POINTER (dir->size), NULL, NULL))
            continue;

	  suffix = theme_dir_get_icon_suffix (dir, icon_name, NULL);	  
	  if (suffix != ICON_SUFFIX_NONE)
	    {
	      if (suffix == ICON_SUFFIX_SVG)
		g_hash_table_insert (sizes, GINT_TO_POINTER (-1), NULL);
	      else
		g_hash_table_insert (sizes, GINT_TO_POINTER (dir->size), NULL);
	    }
	}
    }

  for (d = builtin_dirs; d; d = d->next)
    {
      IconThemeDir *dir = d->data;
      
      if (dir->type != ICON_THEME_DIR_SCALABLE && g_hash_table_lookup_extended (sizes, GINT_TO_POINTER (dir->size), NULL, NULL))
        continue;

      suffix = theme_dir_get_icon_suffix (dir, icon_name, NULL);	  
      if (suffix != ICON_SUFFIX_NONE)
	{
	  if (suffix == ICON_SUFFIX_SVG)
	    g_hash_table_insert (sizes, GINT_TO_POINTER (-1), NULL);
	  else
	    g_hash_table_insert (sizes, GINT_TO_POINTER (dir->size), NULL);
	}
    }

  if (icon_theme_builtin_icons)
    {
      icons = g_hash_table_lookup (icon_theme_builtin_icons, icon_name);
      
      while (icons)
        {
	  BuiltinIcon *icon = icons->data;
	
	  g_hash_table_insert (sizes, GINT_TO_POINTER (icon->size), NULL);
          icons = icons->next;
        }      
    }

  r = result = g_new0 (gint, g_hash_table_size (sizes) + 1);

  g_hash_table_foreach (sizes, add_size, &r);
  g_hash_table_destroy (sizes);
  
  return result;
}

static void
add_key_to_hash (gpointer  key,
		 gpointer  value,
		 gpointer  user_data)
{
  GHashTable *hash = user_data;

  g_hash_table_insert (hash, key, NULL);
}

static void
add_key_to_list (gpointer  key,
		 gpointer  value,
		 gpointer  user_data)
{
  GList **list = user_data;

  *list = g_list_prepend (*list, g_strdup (key));
}

/**
 * gtk_icon_theme_list_icons:
 * @icon_theme: a #GtkIconTheme
 * @context: a string identifying a particular type of icon,
 *           or %NULL to list all icons.
 * 
 * Lists the icons in the current icon theme. Only a subset
 * of the icons can be listed by providing a context string.
 * The set of values for the context string is system dependent,
 * but will typically include such values as "Applications" and
 * "MimeTypes".
 *
 * Return value: (element-type utf8) (transfer full): a #GList list
 *  holding the names of all the icons in the theme. You must first
 *  free each element in the list with g_free(), then free the list
 *  itself with g_list_free().
 *
 * Since: 2.4
 **/
GList *
gtk_icon_theme_list_icons (GtkIconTheme *icon_theme,
			   const char   *context)
{
  GtkIconThemePrivate *priv;
  GHashTable *icons;
  GList *list, *l;
  GQuark context_quark;
  
  priv = icon_theme->priv;
  
  ensure_valid_themes (icon_theme);

  if (context)
    {
      context_quark = g_quark_try_string (context);

      if (!context_quark)
	return NULL;
    }
  else
    context_quark = 0;

  icons = g_hash_table_new (g_str_hash, g_str_equal);
  
  l = priv->themes;
  while (l != NULL)
    {
      theme_list_icons (l->data, icons, context_quark);
      l = l->next;
    }

  if (context_quark == 0)
    g_hash_table_foreach (priv->unthemed_icons,
			  add_key_to_hash,
			  icons);

  list = NULL;
  
  g_hash_table_foreach (icons,
			add_key_to_list,
			&list);

  g_hash_table_destroy (icons);
  
  return list;
}

/**
 * gtk_icon_theme_list_contexts:
 * @icon_theme: a #GtkIconTheme
 *
 * Gets the list of contexts available within the current
 * hierarchy of icon themes
 *
 * Return value: (element-type utf8) (transfer full): a #GList list holding the names of all the
 *  contexts in the theme. You must first free each element
 *  in the list with g_free(), then free the list itself
 *  with g_list_free().
 *
 * Since: 2.12
 **/
GList *
gtk_icon_theme_list_contexts (GtkIconTheme *icon_theme)
{
  GtkIconThemePrivate *priv;
  GHashTable *contexts;
  GList *list, *l;

  priv = icon_theme->priv;
  
  ensure_valid_themes (icon_theme);

  contexts = g_hash_table_new (g_str_hash, g_str_equal);

  l = priv->themes;
  while (l != NULL)
    {
      theme_list_contexts (l->data, contexts);
      l = l->next;
    }

  list = NULL;

  g_hash_table_foreach (contexts,
			add_key_to_list,
			&list);

  g_hash_table_destroy (contexts);

  return list;
}

/**
 * gtk_icon_theme_get_example_icon_name:
 * @icon_theme: a #GtkIconTheme
 * 
 * Gets the name of an icon that is representative of the
 * current theme (for instance, to use when presenting
 * a list of themes to the user.)
 * 
 * Return value: the name of an example icon or %NULL.
 *  Free with g_free().
 *
 * Since: 2.4
 **/
char *
gtk_icon_theme_get_example_icon_name (GtkIconTheme *icon_theme)
{
  GtkIconThemePrivate *priv;
  GList *l;
  IconTheme *theme;

  g_return_val_if_fail (GTK_IS_ICON_THEME (icon_theme), NULL);
  
  priv = icon_theme->priv;
  
  ensure_valid_themes (icon_theme);

  l = priv->themes;
  while (l != NULL)
    {
      theme = l->data;
      if (theme->example)
	return g_strdup (theme->example);
      
      l = l->next;
    }
  
  return NULL;
}


static gboolean
rescan_themes (GtkIconTheme *icon_theme)
{
  GtkIconThemePrivate *priv;
  IconThemeDirMtime *dir_mtime;
  GList *d;
  int stat_res;
  GStatBuf stat_buf;
  GTimeVal tv;

  priv = icon_theme->priv;

  for (d = priv->dir_mtimes; d != NULL; d = d->next)
    {
      dir_mtime = d->data;

      stat_res = g_stat (dir_mtime->dir, &stat_buf);

      /* dir mtime didn't change */
      if (stat_res == 0 && dir_mtime->exists &&
	  S_ISDIR (stat_buf.st_mode) &&
	  dir_mtime->mtime == stat_buf.st_mtime)
	continue;
      /* didn't exist before, and still doesn't */
      if (!dir_mtime->exists &&
	  (stat_res != 0 || !S_ISDIR (stat_buf.st_mode)))
	continue;

      return TRUE;
    }

  g_get_current_time (&tv);
  priv->last_stat_time = tv.tv_sec;

  return FALSE;
}

/**
 * gtk_icon_theme_rescan_if_needed:
 * @icon_theme: a #GtkIconTheme
 * 
 * Checks to see if the icon theme has changed; if it has, any
 * currently cached information is discarded and will be reloaded
 * next time @icon_theme is accessed.
 * 
 * Return value: %TRUE if the icon theme has changed and needed
 *   to be reloaded.
 *
 * Since: 2.4
 **/
gboolean
gtk_icon_theme_rescan_if_needed (GtkIconTheme *icon_theme)
{
  gboolean retval;

  g_return_val_if_fail (GTK_IS_ICON_THEME (icon_theme), FALSE);

  retval = rescan_themes (icon_theme);
  if (retval)
      do_theme_change (icon_theme);

  return retval;
}

static void
theme_destroy (IconTheme *theme)
{
  g_free (theme->display_name);
  g_free (theme->comment);
  g_free (theme->name);
  g_free (theme->example);

  g_list_foreach (theme->dirs, (GFunc)theme_dir_destroy, NULL);
  g_list_free (theme->dirs);
  
  g_free (theme);
}

static void
theme_dir_destroy (IconThemeDir *dir)
{
  if (dir->cache)
      _gtk_icon_cache_unref (dir->cache);
  else
    g_hash_table_destroy (dir->icons);
  
  if (dir->icon_data)
    g_hash_table_destroy (dir->icon_data);
  g_free (dir->dir);
  g_free (dir->subdir);
  g_free (dir);
}

static int
theme_dir_size_difference (IconThemeDir *dir, int size, gboolean *smaller)
{
  int min, max;
  switch (dir->type)
    {
    case ICON_THEME_DIR_FIXED:
      *smaller = size < dir->size;
      return abs (size - dir->size);
      break;
    case ICON_THEME_DIR_SCALABLE:
      *smaller = size < dir->min_size;
      if (size < dir->min_size)
	return dir->min_size - size;
      if (size > dir->max_size)
	return size - dir->max_size;
      return 0;
      break;
    case ICON_THEME_DIR_THRESHOLD:
      min = dir->size - dir->threshold;
      max = dir->size + dir->threshold;
      *smaller = size < min;
      if (size < min)
	return min - size;
      if (size > max)
	return size - max;
      return 0;
      break;
    case ICON_THEME_DIR_UNTHEMED:
      g_assert_not_reached ();
      break;
    }
  g_assert_not_reached ();
  return 1000;
}

static const char *
string_from_suffix (IconSuffix suffix)
{
  switch (suffix)
    {
    case ICON_SUFFIX_XPM:
      return ".xpm";
    case ICON_SUFFIX_SVG:
      return ".svg";
    case ICON_SUFFIX_PNG:
      return ".png";
    default:
      g_assert_not_reached();
    }
  return NULL;
}

static IconSuffix
suffix_from_name (const char *name)
{
  IconSuffix retval;

  if (g_str_has_suffix (name, ".png"))
    retval = ICON_SUFFIX_PNG;
  else if (g_str_has_suffix (name, ".svg"))
    retval = ICON_SUFFIX_SVG;
  else if (g_str_has_suffix (name, ".xpm"))
    retval = ICON_SUFFIX_XPM;
  else
    retval = ICON_SUFFIX_NONE;

  return retval;
}

static IconSuffix
best_suffix (IconSuffix suffix,
	     gboolean   allow_svg)
{
  if ((suffix & ICON_SUFFIX_PNG) != 0)
    return ICON_SUFFIX_PNG;
  else if (allow_svg && ((suffix & ICON_SUFFIX_SVG) != 0))
    return ICON_SUFFIX_SVG;
  else if ((suffix & ICON_SUFFIX_XPM) != 0)
    return ICON_SUFFIX_XPM;
  else
    return ICON_SUFFIX_NONE;
}


static IconSuffix
theme_dir_get_icon_suffix (IconThemeDir *dir,
			   const gchar  *icon_name,
			   gboolean     *has_icon_file)
{
  IconSuffix suffix;

  if (dir->cache)
    {
      suffix = (IconSuffix)_gtk_icon_cache_get_icon_flags (dir->cache,
							   icon_name,
							   dir->subdir_index);

      if (has_icon_file)
	*has_icon_file = suffix & HAS_ICON_FILE;

      suffix = suffix & ~HAS_ICON_FILE;
    }
  else
    suffix = GPOINTER_TO_UINT (g_hash_table_lookup (dir->icons, icon_name));

  GTK_NOTE (ICONTHEME, 
	    g_print ("get_icon_suffix%s %u\n", dir->cache ? " (cached)" : "", suffix));

  return suffix;
}

static GtkIconInfo *
theme_lookup_icon (IconTheme          *theme,
		   const char         *icon_name,
		   int                 size,
		   gboolean            allow_svg,
		   gboolean            use_builtin)
{
  GList *dirs, *l;
  IconThemeDir *dir, *min_dir;
  char *file;
  int min_difference, difference;
  BuiltinIcon *closest_builtin = NULL;
  gboolean smaller, has_larger, match;
  IconSuffix suffix;

  min_difference = G_MAXINT;
  min_dir = NULL;
  has_larger = FALSE;
  match = FALSE;

  /* Builtin icons are logically part of the default theme and
   * are searched before other subdirectories of the default theme.
   */
  if (use_builtin && strcmp (theme->name, DEFAULT_THEME_NAME) == 0)
    {
      closest_builtin = find_builtin_icon (icon_name, 
					   size,
					   &min_difference,
					   &has_larger);

      if (min_difference == 0)
	return icon_info_new_builtin (closest_builtin);

      dirs = builtin_dirs;
    }
  else
    dirs = theme->dirs;

  l = dirs;
  while (l != NULL)
    {
      dir = l->data;

      GTK_NOTE (ICONTHEME,
		g_print ("theme_lookup_icon dir %s\n", dir->dir));
      suffix = theme_dir_get_icon_suffix (dir, icon_name, NULL);
      if (best_suffix (suffix, allow_svg) != ICON_SUFFIX_NONE)
	{
	  difference = theme_dir_size_difference (dir, size, &smaller);

	  if (difference == 0)
	    {
              if (dir->type == ICON_THEME_DIR_SCALABLE)
                {
                  /* don't pick scalable if we already found
                   * a matching non-scalable dir
                   */
                  if (!match)
                    {
	              min_dir = dir;
	              break;
                    }
                }
              else
                {
                  /* for a matching non-scalable dir keep
                   * going and look for a closer match
                   */             
                  difference = abs (size - dir->size);
                  if (!match || difference < min_difference)
                    {
                      match = TRUE;
                      min_difference = difference;
	              min_dir = dir;
                    }
                  if (difference == 0)
                    break;
                }
	    } 
  
          if (!match)
            {
	      if (!has_larger)
	        {
	          if (difference < min_difference || smaller)
	  	    {
		      min_difference = difference;
		      min_dir = dir;
		      has_larger = smaller;
	 	    }
	        }
	      else
	        {
	          if (difference < min_difference && smaller)
		    {
		      min_difference = difference;
		      min_dir = dir;
		    }
	        }
	    }
        }

      l = l->next;

      if (l == NULL && dirs == builtin_dirs)
	{
	  dirs = theme->dirs;
	  l = dirs;
	}
    }

  if (min_dir)
    {
      GtkIconInfo *icon_info = icon_info_new ();
      gboolean has_icon_file = FALSE;
      
      suffix = theme_dir_get_icon_suffix (min_dir, icon_name, &has_icon_file);
      suffix = best_suffix (suffix, allow_svg);
      g_assert (suffix != ICON_SUFFIX_NONE);
      
      if (min_dir->dir)
        {
          file = g_strconcat (icon_name, string_from_suffix (suffix), NULL);
          icon_info->filename = g_build_filename (min_dir->dir, file, NULL);
          g_free (file);
#if defined (G_OS_WIN32) && !defined (_WIN64)
          icon_info->cp_filename = g_locale_from_utf8 (icon_info->filename,
						   -1, NULL, NULL, NULL);
#endif
        }
      else
        {
          icon_info->filename = NULL;
#if defined (G_OS_WIN32) && !defined (_WIN64)
          icon_info->cp_filename = NULL;
#endif
        }
      
      if (min_dir->icon_data != NULL)
	icon_info->data = g_hash_table_lookup (min_dir->icon_data, icon_name);

      if (icon_info->data == NULL && min_dir->cache != NULL)
	{
	  icon_info->data = _gtk_icon_cache_get_icon_data (min_dir->cache, icon_name, min_dir->subdir_index);
	  if (icon_info->data)
	    {
	      if (min_dir->icon_data == NULL)
		min_dir->icon_data = g_hash_table_new_full (g_str_hash, g_str_equal,
							    g_free, (GDestroyNotify)icon_data_free);

	      g_hash_table_replace (min_dir->icon_data, g_strdup (icon_name), icon_info->data);
	    }
	}

      if (icon_info->data == NULL && has_icon_file)
	{
	  gchar *icon_file_name, *icon_file_path;

	  icon_file_name = g_strconcat (icon_name, ".icon", NULL);
	  icon_file_path = g_build_filename (min_dir->dir, icon_file_name, NULL);

	  if (g_file_test (icon_file_path, G_FILE_TEST_IS_REGULAR))
	    {
	      if (min_dir->icon_data == NULL)	
		min_dir->icon_data = g_hash_table_new_full (g_str_hash, g_str_equal,
							    g_free, (GDestroyNotify)icon_data_free);
	      load_icon_data (min_dir, icon_file_path, icon_file_name);
	      
	      icon_info->data = g_hash_table_lookup (min_dir->icon_data, icon_name);
	    }
	  g_free (icon_file_name);
	  g_free (icon_file_path);
	}

      if (min_dir->cache)
	{
	  icon_info->cache_pixbuf = _gtk_icon_cache_get_icon (min_dir->cache, icon_name,
							      min_dir->subdir_index);
	}

      icon_info->dir_type = min_dir->type;
      icon_info->dir_size = min_dir->size;
      icon_info->threshold = min_dir->threshold;
      
      return icon_info;
    }

  if (closest_builtin)
    return icon_info_new_builtin (closest_builtin);
  
  return NULL;
}

static void
theme_list_icons (IconTheme  *theme, 
		  GHashTable *icons,
		  GQuark      context)
{
  GList *l = theme->dirs;
  IconThemeDir *dir;
  
  while (l != NULL)
    {
      dir = l->data;

      if (context == dir->context ||
	  context == 0)
	{
	  if (dir->cache)
	    {
	      _gtk_icon_cache_add_icons (dir->cache,
					 dir->subdir,
					 icons);
					 
	    }
	  else
	    {
	      g_hash_table_foreach (dir->icons,
				    add_key_to_hash,
				    icons);
	    }
	}
      l = l->next;
    }
}

static void
theme_list_contexts (IconTheme  *theme, 
		     GHashTable *contexts)
{
  GList *l = theme->dirs;
  IconThemeDir *dir;
  const char *context;

  while (l != NULL)
    {
      dir = l->data;

      context = g_quark_to_string (dir->context);
      g_hash_table_replace (contexts, (gpointer) context, NULL);

      l = l->next;
    }
}

static void
load_icon_data (IconThemeDir *dir, const char *path, const char *name)
{
  GKeyFile *icon_file;
  char *base_name;
  char **split;
  gsize length;
  char *str;
  char *split_point;
  int i;
  gint *ivalues;
  GError *error = NULL;
  
  GtkIconData *data;

  icon_file = g_key_file_new ();
  g_key_file_set_list_separator (icon_file, ',');
  g_key_file_load_from_file (icon_file, path, 0, &error);
  if (error)
    {
      g_error_free (error);
      g_key_file_free (icon_file);      
      return;
    }
  else
    {
      base_name = strip_suffix (name);
      
      data = g_slice_new0 (GtkIconData);
      g_hash_table_replace (dir->icon_data, base_name, data);
      
      ivalues = g_key_file_get_integer_list (icon_file, 
					     "Icon Data", "EmbeddedTextRectangle",
					      &length, NULL);
      if (ivalues)
	{
	  if (length == 4)
	    {
	      data->has_embedded_rect = TRUE;
	      data->x0 = ivalues[0];
	      data->y0 = ivalues[1];
	      data->x1 = ivalues[2];
	      data->y1 = ivalues[3];
	    }
	  
	  g_free (ivalues);
	}
      
      str = g_key_file_get_string (icon_file, "Icon Data", "AttachPoints", NULL);
      if (str)
	{
	  split = g_strsplit (str, "|", -1);
	  
	  data->n_attach_points = g_strv_length (split);
	  data->attach_points = g_new (GdkPoint, data->n_attach_points);

	  i = 0;
	  while (split[i] != NULL && i < data->n_attach_points)
	    {
	      split_point = strchr (split[i], ',');
	      if (split_point)
		{
		  *split_point = 0;
		  split_point++;
		  data->attach_points[i].x = atoi (split[i]);
		  data->attach_points[i].y = atoi (split_point);
		}
	      i++;
	    }
	  
	  g_strfreev (split);
	  g_free (str);
	}
      
      data->display_name = g_key_file_get_locale_string (icon_file, 
							 "Icon Data", "DisplayName",
							 NULL, NULL);
      g_key_file_free (icon_file);
    }
}

static void
scan_directory (GtkIconThemePrivate *icon_theme,
		IconThemeDir *dir, char *full_dir)
{
  GDir *gdir;
  const char *name;

  GTK_NOTE (ICONTHEME, 
	    g_print ("scanning directory %s\n", full_dir));
  dir->icons = g_hash_table_new_full (g_str_hash, g_str_equal,
				      g_free, NULL);
  
  gdir = g_dir_open (full_dir, 0, NULL);

  if (gdir == NULL)
    return;

  while ((name = g_dir_read_name (gdir)))
    {
      char *path;
      char *base_name;
      IconSuffix suffix, hash_suffix;

      if (g_str_has_suffix (name, ".icon"))
	{
	  if (dir->icon_data == NULL)
	    dir->icon_data = g_hash_table_new_full (g_str_hash, g_str_equal,
						    g_free, (GDestroyNotify)icon_data_free);
	  
	  path = g_build_filename (full_dir, name, NULL);
	  if (g_file_test (path, G_FILE_TEST_IS_REGULAR))
	    load_icon_data (dir, path, name);
	  
	  g_free (path);
	  
	  continue;
	}

      suffix = suffix_from_name (name);
      if (suffix == ICON_SUFFIX_NONE)
	continue;

      base_name = strip_suffix (name);

      hash_suffix = GPOINTER_TO_INT (g_hash_table_lookup (dir->icons, base_name));
      g_hash_table_replace (icon_theme->all_icons, base_name, NULL);
      g_hash_table_replace (dir->icons, base_name, GUINT_TO_POINTER (hash_suffix| suffix));
    }
  
  g_dir_close (gdir);
}

static void
theme_subdir_load (GtkIconTheme *icon_theme,
		   IconTheme    *theme,
		   GKeyFile     *theme_file,
		   char         *subdir)
{
  GList *d;
  char *type_string;
  IconThemeDir *dir;
  IconThemeDirType type;
  char *context_string;
  GQuark context;
  int size;
  int min_size;
  int max_size;
  int threshold;
  char *full_dir;
  GError *error = NULL;
  IconThemeDirMtime *dir_mtime;

  size = g_key_file_get_integer (theme_file, subdir, "Size", &error);
  if (error)
    {
      g_error_free (error);
      g_warning ("Theme directory %s of theme %s has no size field\n", 
		 subdir, theme->name);
      return;
    }
  
  type = ICON_THEME_DIR_THRESHOLD;
  type_string = g_key_file_get_string (theme_file, subdir, "Type", NULL);
  if (type_string)
    {
      if (strcmp (type_string, "Fixed") == 0)
	type = ICON_THEME_DIR_FIXED;
      else if (strcmp (type_string, "Scalable") == 0)
	type = ICON_THEME_DIR_SCALABLE;
      else if (strcmp (type_string, "Threshold") == 0)
	type = ICON_THEME_DIR_THRESHOLD;

      g_free (type_string);
    }
  
  context = 0;
  context_string = g_key_file_get_string (theme_file, subdir, "Context", NULL);
  if (context_string)
    {
      context = g_quark_from_string (context_string);
      g_free (context_string);
    }

  max_size = g_key_file_get_integer (theme_file, subdir, "MaxSize", &error);
  if (error)
    {
      max_size = size;

      g_error_free (error);
      error = NULL;
    }

  min_size = g_key_file_get_integer (theme_file, subdir, "MinSize", &error);
  if (error)
    {
      min_size = size;

      g_error_free (error);
      error = NULL;
    }
  
  threshold = g_key_file_get_integer (theme_file, subdir, "Threshold", &error);
  if (error)
    {
      threshold = 2;

      g_error_free (error);
      error = NULL;
    }

  for (d = icon_theme->priv->dir_mtimes; d; d = d->next)
    {
      dir_mtime = (IconThemeDirMtime *)d->data;

      if (!dir_mtime->exists)
	continue; /* directory doesn't exist */

       full_dir = g_build_filename (dir_mtime->dir, subdir, NULL);

      /* First, see if we have a cache for the directory */
      if (dir_mtime->cache != NULL || g_file_test (full_dir, G_FILE_TEST_IS_DIR))
	{
	  if (dir_mtime->cache == NULL)
	    {
	      /* This will return NULL if the cache doesn't exist or is outdated */
	      dir_mtime->cache = _gtk_icon_cache_new_for_path (dir_mtime->dir);
	    }
	  
	  dir = g_new (IconThemeDir, 1);
	  dir->type = type;
	  dir->context = context;
	  dir->size = size;
	  dir->min_size = min_size;
	  dir->max_size = max_size;
	  dir->threshold = threshold;
	  dir->dir = full_dir;
	  dir->icon_data = NULL;
	  dir->subdir = g_strdup (subdir);
	  if (dir_mtime->cache != NULL)
            {
	      dir->cache = _gtk_icon_cache_ref (dir_mtime->cache);
              dir->subdir_index = _gtk_icon_cache_get_directory_index (dir->cache, dir->subdir);
            }
	  else
	    {
	      dir->cache = NULL;
              dir->subdir_index = -1;
	      scan_directory (icon_theme->priv, dir, full_dir);
	    }

	  theme->dirs = g_list_prepend (theme->dirs, dir);
	}
      else
	g_free (full_dir);
    }
}

static void
icon_data_free (GtkIconData *icon_data)
{
  g_free (icon_data->attach_points);
  g_free (icon_data->display_name);
  g_slice_free (GtkIconData, icon_data);
}

/*
 * GtkIconInfo
 */
GType
gtk_icon_info_get_type (void)
{
  static GType our_type = 0;
  
  if (our_type == 0)
    our_type = g_boxed_type_register_static (I_("GtkIconInfo"),
					     (GBoxedCopyFunc) gtk_icon_info_copy,
					     (GBoxedFreeFunc) gtk_icon_info_free);


  return our_type;
}

static GtkIconInfo *
icon_info_new (void)
{
  GtkIconInfo *icon_info = g_slice_new0 (GtkIconInfo);

  icon_info->scale = -1.;
  icon_info->ref_count = 1;

  return icon_info;
}

static GtkIconInfo *
icon_info_new_builtin (BuiltinIcon *icon)
{
  GtkIconInfo *icon_info = icon_info_new ();

  icon_info->cache_pixbuf = g_object_ref (icon->pixbuf);
  icon_info->dir_type = ICON_THEME_DIR_THRESHOLD;
  icon_info->dir_size = icon->size;
  icon_info->threshold = 2;

  return icon_info;
}

/**
 * gtk_icon_info_copy:
 * @icon_info: a #GtkIconInfo
 * 
 * Make a copy of a #GtkIconInfo.
 * 
 * Return value: the new GtkIconInfo
 *
 * Since: 2.4
 **/
GtkIconInfo *
gtk_icon_info_copy (GtkIconInfo *icon_info)
{
  
  g_return_val_if_fail (icon_info != NULL, NULL);

  icon_info->ref_count++;

  return icon_info;
}

/**
 * gtk_icon_info_free:
 * @icon_info: a #GtkIconInfo
 * 
 * Free a #GtkIconInfo and associated information
 *
 * Since: 2.4
 **/
void
gtk_icon_info_free (GtkIconInfo *icon_info)
{
  g_return_if_fail (icon_info != NULL);

  icon_info->ref_count--;
  if (icon_info->ref_count > 0)
    return;
 
  g_free (icon_info->filename);
#if defined (G_OS_WIN32) && !defined (_WIN64)
  g_free (icon_info->cp_filename);
#endif
  if (icon_info->loadable)
    g_object_unref (icon_info->loadable);
  g_slist_foreach (icon_info->emblem_infos, (GFunc)gtk_icon_info_free, NULL);
  g_slist_free (icon_info->emblem_infos);
  if (icon_info->pixbuf)
    g_object_unref (icon_info->pixbuf);
  if (icon_info->cache_pixbuf)
    g_object_unref (icon_info->cache_pixbuf);

  g_slice_free (GtkIconInfo, icon_info);
}

/**
 * gtk_icon_info_get_base_size:
 * @icon_info: a #GtkIconInfo
 * 
 * Gets the base size for the icon. The base size
 * is a size for the icon that was specified by
 * the icon theme creator. This may be different
 * than the actual size of image; an example of
 * this is small emblem icons that can be attached
 * to a larger icon. These icons will be given
 * the same base size as the larger icons to which
 * they are attached.
 * 
 * Return value: the base size, or 0, if no base
 *  size is known for the icon.
 *
 * Since: 2.4
 **/
gint
gtk_icon_info_get_base_size (GtkIconInfo *icon_info)
{
  g_return_val_if_fail (icon_info != NULL, 0);

  return icon_info->dir_size;
}

/**
 * gtk_icon_info_get_filename:
 * @icon_info: a #GtkIconInfo
 * 
 * Gets the filename for the icon. If the
 * %GTK_ICON_LOOKUP_USE_BUILTIN flag was passed
 * to gtk_icon_theme_lookup_icon(), there may be
 * no filename if a builtin icon is returned; in this
 * case, you should use gtk_icon_info_get_builtin_pixbuf().
 * 
 * Return value: the filename for the icon, or %NULL
 *  if gtk_icon_info_get_builtin_pixbuf() should
 *  be used instead. The return value is owned by
 *  GTK+ and should not be modified or freed.
 *
 * Since: 2.4
 **/
const gchar *
gtk_icon_info_get_filename (GtkIconInfo *icon_info)
{
  g_return_val_if_fail (icon_info != NULL, NULL);

  return icon_info->filename;
}

/**
 * gtk_icon_info_get_builtin_pixbuf:
 * @icon_info: a #GtkIconInfo structure
 * 
 * Gets the built-in image for this icon, if any. To allow
 * GTK+ to use built in icon images, you must pass the
 * %GTK_ICON_LOOKUP_USE_BUILTIN to
 * gtk_icon_theme_lookup_icon().
 *
 * Return value: (transfer none): the built-in image pixbuf, or %NULL. No
 *  extra reference is added to the returned pixbuf, so if
 *  you want to keep it around, you must use g_object_ref().
 *  The returned image must not be modified.
 *
 * Since: 2.4
 **/
GdkPixbuf *
gtk_icon_info_get_builtin_pixbuf (GtkIconInfo *icon_info)
{
  g_return_val_if_fail (icon_info != NULL, NULL);

  if (icon_info->filename)
    return NULL;
  
  return icon_info->cache_pixbuf;
}

static gboolean icon_info_ensure_scale_and_pixbuf (GtkIconInfo*, gboolean);

/* Combine the icon with all emblems, the first emblem is placed 
 * in the southeast corner. Scale emblems to be at most 3/4 of the
 * size of the icon itself.
 */
static void 
apply_emblems (GtkIconInfo *info)
{
  GdkPixbuf *icon = NULL;
  gint w, h, pos;
  GSList *l;

  if (info->emblem_infos == NULL)
    return;

  if (info->emblems_applied)
    return;

  w = gdk_pixbuf_get_width (info->pixbuf);
  h = gdk_pixbuf_get_height (info->pixbuf);

  for (l = info->emblem_infos, pos = 0; l; l = l->next, pos++)
    {
      GtkIconInfo *emblem_info = l->data;

      if (icon_info_ensure_scale_and_pixbuf (emblem_info, FALSE))
        {
          GdkPixbuf *emblem = emblem_info->pixbuf;
          gint ew, eh;
          gint x = 0, y = 0; /* silence compiler */
          gdouble scale;

          ew = gdk_pixbuf_get_width (emblem);
          eh = gdk_pixbuf_get_height (emblem);
          if (ew >= w)
            {
              scale = 0.75;
              ew = ew * 0.75;
              eh = eh * 0.75;
            }
          else
            scale = 1.0;

          switch (pos % 4)
            {
            case 0:
              x = w - ew;
              y = h - eh;
              break;
            case 1:
              x = w - ew;
              y = 0;
              break;
            case 2:
              x = 0;
              y = h - eh;
              break;
            case 3:
              x = 0;
              y = 0;
              break;
            }

          if (icon == NULL)
            {
              icon = gdk_pixbuf_copy (info->pixbuf);
              if (icon == NULL)
                break;
            }

          gdk_pixbuf_composite (emblem, icon, x, y, ew, eh, x, y,
                                scale, scale, GDK_INTERP_BILINEAR, 255);
       }
   }

  if (icon)
    {
      g_object_unref (info->pixbuf);
      info->pixbuf = icon;
    }

  info->emblems_applied = TRUE;
}

/* This function contains the complicated logic for deciding
 * on the size at which to load the icon and loading it at
 * that size.
 */
static gboolean
icon_info_ensure_scale_and_pixbuf (GtkIconInfo  *icon_info,
				   gboolean      scale_only)
{
  int image_width, image_height;
  GdkPixbuf *source_pixbuf;
  gboolean is_svg;

  /* First check if we already succeeded have the necessary
   * information (or failed earlier)
   */
  if (scale_only && icon_info->scale >= 0)
    return TRUE;

  if (icon_info->pixbuf)
    {
      apply_emblems (icon_info);
      return TRUE;
    }

  if (icon_info->load_error)
    return FALSE;

  /* SVG icons are a special case - we just immediately scale them
   * to the desired size
   */
  if (icon_info->filename && !icon_info->loadable) 
    {
      GFile *file;

      file = g_file_new_for_path (icon_info->filename);
      icon_info->loadable = G_LOADABLE_ICON (g_file_icon_new (file));
      g_object_unref (file);
    }

  is_svg = FALSE;
  if (G_IS_FILE_ICON (icon_info->loadable))
    {
      GFile *file;
      GFileInfo *file_info;
      const gchar *content_type;
      const gchar *mime_type;

      file = g_file_icon_get_file (G_FILE_ICON (icon_info->loadable));
      file_info = g_file_query_info (file, 
                                     G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                                     G_FILE_QUERY_INFO_NONE,
                                     NULL, NULL);
      if (file_info) 
        {
          content_type = g_file_info_get_content_type (file_info);

          if (content_type)
            {
              mime_type = g_content_type_get_mime_type (content_type);

              if (mime_type && strcmp (mime_type, "image/svg+xml") == 0)
                is_svg = TRUE;
            }

          g_object_unref (file_info);
        }
    }

  if (is_svg)
    {
      GInputStream *stream;

      icon_info->scale = icon_info->desired_size / 1000.;

      if (scale_only)
	return TRUE;
      
      stream = g_loadable_icon_load (icon_info->loadable,
                                     icon_info->desired_size,
                                     NULL, NULL,
                                     &icon_info->load_error);
      if (stream)
        {
          icon_info->pixbuf = gdk_pixbuf_new_from_stream_at_scale (stream,
                                                                   icon_info->desired_size,
                                                                   icon_info->desired_size,
                                                                   TRUE,
                                                                   NULL,
                                                                   &icon_info->load_error);
          g_object_unref (stream);
        }

      if (!icon_info->pixbuf)
        return FALSE;

      apply_emblems (icon_info);
        
      return TRUE;
    }

  /* In many cases, the scale can be determined without actual access
   * to the icon file. This is generally true when we have a size
   * for the directory where the icon is; the image size doesn't
   * matter in that case.
   */
  if (icon_info->forced_size)
    icon_info->scale = -1;
  else if (icon_info->dir_type == ICON_THEME_DIR_FIXED)
    icon_info->scale = 1.0;
  else if (icon_info->dir_type == ICON_THEME_DIR_THRESHOLD)
    {
      if (icon_info->desired_size >= icon_info->dir_size - icon_info->threshold &&
	  icon_info->desired_size <= icon_info->dir_size + icon_info->threshold)
	icon_info->scale = 1.0;
      else if (icon_info->dir_size > 0)
	icon_info->scale =(gdouble) icon_info->desired_size / icon_info->dir_size;
    }
  else if (icon_info->dir_type == ICON_THEME_DIR_SCALABLE)
    {
      if (icon_info->dir_size > 0)
	icon_info->scale = (gdouble) icon_info->desired_size / icon_info->dir_size;
    }

  if (icon_info->scale >= 0. && scale_only)
    return TRUE;

  /* At this point, we need to actually get the icon; either from the
   * builtin image or by loading the file
   */
  source_pixbuf = NULL;
  if (icon_info->cache_pixbuf)
    source_pixbuf = g_object_ref (icon_info->cache_pixbuf);
  else
    {
      GInputStream *stream;

      stream = g_loadable_icon_load (icon_info->loadable,
                                     icon_info->desired_size,
                                     NULL, NULL,
                                     &icon_info->load_error);
      if (stream)
        {
          source_pixbuf = gdk_pixbuf_new_from_stream (stream,
                                                      NULL,
                                                      &icon_info->load_error);
          g_object_unref (stream);
        }
    }

  if (!source_pixbuf)
    return FALSE;

  /* Do scale calculations that depend on the image size
   */
  image_width = gdk_pixbuf_get_width (source_pixbuf);
  image_height = gdk_pixbuf_get_height (source_pixbuf);

  if (icon_info->scale < 0.0)
    {
      gint image_size = MAX (image_width, image_height);
      if (image_size > 0)
	icon_info->scale = (gdouble)icon_info->desired_size / (gdouble)image_size;
      else
	icon_info->scale = 1.0;
      
      if (icon_info->dir_type == ICON_THEME_DIR_UNTHEMED && 
          !icon_info->forced_size)
	icon_info->scale = MIN (icon_info->scale, 1.0);
    }

  /* We don't short-circuit out here for scale_only, since, now
   * we've loaded the icon, we might as well go ahead and finish
   * the job. This is a bit of a waste when we scale here
   * and never get the final pixbuf; at the cost of a bit of
   * extra complexity, we could keep the source pixbuf around
   * but not actually scale it until needed.
   */
  if (icon_info->scale == 1.0)
    icon_info->pixbuf = source_pixbuf;
  else
    {
      icon_info->pixbuf = gdk_pixbuf_scale_simple (source_pixbuf,
						   0.5 + image_width * icon_info->scale,
						   0.5 + image_height * icon_info->scale,
						   GDK_INTERP_BILINEAR);
      g_object_unref (source_pixbuf);
    }

  apply_emblems (icon_info);

  return TRUE;
}

/**
 * gtk_icon_info_load_icon:
 * @icon_info: a #GtkIconInfo structure from gtk_icon_theme_lookup_icon()
 * @error: (allow-none): location to store error information on failure,
 *     or %NULL.
 *
 * Renders an icon previously looked up in an icon theme using
 * gtk_icon_theme_lookup_icon(); the size will be based on the size
 * passed to gtk_icon_theme_lookup_icon(). Note that the resulting
 * pixbuf may not be exactly this size; an icon theme may have icons
 * that differ slightly from their nominal sizes, and in addition GTK+
 * will avoid scaling icons that it considers sufficiently close to the
 * requested size or for which the source image would have to be scaled
 * up too far. (This maintains sharpness.). This behaviour can be changed
 * by passing the %GTK_ICON_LOOKUP_FORCE_SIZE flag when obtaining
 * the #GtkIconInfo. If this flag has been specified, the pixbuf
 * returned by this function will be scaled to the exact size.
 *
 * Return value: (transfer full): the rendered icon; this may be a newly
 *     created icon or a new reference to an internal icon, so you must
 *     not modify the icon. Use g_object_unref() to release your reference
 *     to the icon.
 *
 * Since: 2.4
 **/
GdkPixbuf *
gtk_icon_info_load_icon (GtkIconInfo *icon_info,
			 GError     **error)
{
  g_return_val_if_fail (icon_info != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  if (!icon_info_ensure_scale_and_pixbuf (icon_info, FALSE))
    {
      if (icon_info->load_error)
        g_propagate_error (error, icon_info->load_error);
      else
        g_set_error_literal (error,  
                             GTK_ICON_THEME_ERROR,  
                             GTK_ICON_THEME_NOT_FOUND,
                             _("Failed to load icon"));
 
      return NULL;
    }

  return g_object_ref (icon_info->pixbuf);
}

/**
 * gtk_icon_info_set_raw_coordinates:
 * @icon_info: a #GtkIconInfo
 * @raw_coordinates: whether the coordinates of embedded rectangles
 *   and attached points should be returned in their original
 *   (unscaled) form.
 * 
 * Sets whether the coordinates returned by gtk_icon_info_get_embedded_rect()
 * and gtk_icon_info_get_attach_points() should be returned in their
 * original form as specified in the icon theme, instead of scaled
 * appropriately for the pixbuf returned by gtk_icon_info_load_icon().
 *
 * Raw coordinates are somewhat strange; they are specified to be with
 * respect to the unscaled pixmap for PNG and XPM icons, but for SVG
 * icons, they are in a 1000x1000 coordinate space that is scaled
 * to the final size of the icon.  You can determine if the icon is an SVG
 * icon by using gtk_icon_info_get_filename(), and seeing if it is non-%NULL
 * and ends in '.svg'.
 *
 * This function is provided primarily to allow compatibility wrappers
 * for older API's, and is not expected to be useful for applications.
 *
 * Since: 2.4
 **/
void
gtk_icon_info_set_raw_coordinates (GtkIconInfo *icon_info,
				   gboolean     raw_coordinates)
{
  g_return_if_fail (icon_info != NULL);
  
  icon_info->raw_coordinates = raw_coordinates != FALSE;
}

/* Scale coordinates from the icon data prior to returning
 * them to the user.
 */
static gboolean
icon_info_scale_point (GtkIconInfo  *icon_info,
		       gint          x,
		       gint          y,
		       gint         *x_out,
		       gint         *y_out)
{
  if (icon_info->raw_coordinates)
    {
      *x_out = x;
      *y_out = y;
    }
  else
    {
      if (!icon_info_ensure_scale_and_pixbuf (icon_info, TRUE))
	return FALSE;

      *x_out = 0.5 + x * icon_info->scale;
      *y_out = 0.5 + y * icon_info->scale;
    }

  return TRUE;
}

/**
 * gtk_icon_info_get_embedded_rect:
 * @icon_info: a #GtkIconInfo
 * @rectangle: (out): #GdkRectangle in which to store embedded
 *   rectangle coordinates; coordinates are only stored
 *   when this function returns %TRUE.
 *
 * Gets the coordinates of a rectangle within the icon
 * that can be used for display of information such
 * as a preview of the contents of a text file.
 * See gtk_icon_info_set_raw_coordinates() for further
 * information about the coordinate system.
 * 
 * Return value: %TRUE if the icon has an embedded rectangle
 *
 * Since: 2.4
 **/
gboolean
gtk_icon_info_get_embedded_rect (GtkIconInfo  *icon_info,
				 GdkRectangle *rectangle)
{
  g_return_val_if_fail (icon_info != NULL, FALSE);

  if (icon_info->data && icon_info->data->has_embedded_rect &&
      icon_info_ensure_scale_and_pixbuf (icon_info, TRUE))
    {
      gint scaled_x0, scaled_y0;
      gint scaled_x1, scaled_y1;
      
      if (rectangle)
	{
	  icon_info_scale_point (icon_info,
				 icon_info->data->x0, icon_info->data->y0,
				 &scaled_x0, &scaled_y0);
	  icon_info_scale_point (icon_info,
				 icon_info->data->x1, icon_info->data->y1,
				 &scaled_x1, &scaled_y1);
	  
	  rectangle->x = scaled_x0;
	  rectangle->y = scaled_y0;
	  rectangle->width = scaled_x1 - rectangle->x;
	  rectangle->height = scaled_y1 - rectangle->y;
	}

      return TRUE;
    }
  else
    return FALSE;
}

/**
 * gtk_icon_info_get_attach_points:
 * @icon_info: a #GtkIconInfo
 * @points: (allow-none) (array length=n_points) (out): location to store pointer to an array of points, or %NULL
 *          free the array of points with g_free().
 * @n_points: (allow-none): location to store the number of points in @points, or %NULL
 * 
 * Fetches the set of attach points for an icon. An attach point
 * is a location in the icon that can be used as anchor points for attaching
 * emblems or overlays to the icon.
 * 
 * Return value: %TRUE if there are any attach points for the icon.
 *
 * Since: 2.4
 **/
gboolean
gtk_icon_info_get_attach_points (GtkIconInfo *icon_info,
				 GdkPoint   **points,
				 gint        *n_points)
{
  g_return_val_if_fail (icon_info != NULL, FALSE);
  
  if (icon_info->data && icon_info->data->n_attach_points &&
      icon_info_ensure_scale_and_pixbuf (icon_info, TRUE))
    {
      if (points)
	{
	  gint i;
	  
	  *points = g_new (GdkPoint, icon_info->data->n_attach_points);
	  for (i = 0; i < icon_info->data->n_attach_points; i++)
	    icon_info_scale_point (icon_info,
				   icon_info->data->attach_points[i].x,
				   icon_info->data->attach_points[i].y,
				   &(*points)[i].x,
				   &(*points)[i].y);
	}
	  
      if (n_points)
	*n_points = icon_info->data->n_attach_points;

      return TRUE;
    }
  else
    {
      if (points)
	*points = NULL;
      if (n_points)
	*n_points = 0;
      
      return FALSE;
    }
}

/**
 * gtk_icon_info_get_display_name:
 * @icon_info: a #GtkIconInfo
 * 
 * Gets the display name for an icon. A display name is a
 * string to be used in place of the icon name in a user
 * visible context like a list of icons.
 * 
 * Return value: the display name for the icon or %NULL, if
 *  the icon doesn't have a specified display name. This value
 *  is owned @icon_info and must not be modified or free.
 *
 * Since: 2.4
 **/
const gchar *
gtk_icon_info_get_display_name (GtkIconInfo *icon_info)
{
  g_return_val_if_fail (icon_info != NULL, NULL);

  if (icon_info->data)
    return icon_info->data->display_name;
  else
    return NULL;
}

/*
 * Builtin icons
 */


/**
 * gtk_icon_theme_add_builtin_icon:
 * @icon_name: the name of the icon to register
 * @size: the size at which to register the icon (different
 *        images can be registered for the same icon name
 *        at different sizes.)
 * @pixbuf: #GdkPixbuf that contains the image to use
 *          for @icon_name.
 * 
 * Registers a built-in icon for icon theme lookups. The idea
 * of built-in icons is to allow an application or library
 * that uses themed icons to function requiring files to
 * be present in the file system. For instance, the default
 * images for all of GTK+'s stock icons are registered
 * as built-icons.
 *
 * In general, if you use gtk_icon_theme_add_builtin_icon()
 * you should also install the icon in the icon theme, so
 * that the icon is generally available.
 *
 * This function will generally be used with pixbufs loaded
 * via gdk_pixbuf_new_from_inline().
 *
 * Since: 2.4
 **/
void
gtk_icon_theme_add_builtin_icon (const gchar *icon_name,
				 gint         size,
				 GdkPixbuf   *pixbuf)
{
  BuiltinIcon *default_icon;
  GSList *icons;
  gpointer key;

  g_return_if_fail (icon_name != NULL);
  g_return_if_fail (GDK_IS_PIXBUF (pixbuf));
  
  if (!icon_theme_builtin_icons)
    icon_theme_builtin_icons = g_hash_table_new (g_str_hash, g_str_equal);

  icons = g_hash_table_lookup (icon_theme_builtin_icons, icon_name);
  if (!icons)
    key = g_strdup (icon_name);
  else
    key = (gpointer)icon_name;	/* Won't get stored */

  default_icon = g_new (BuiltinIcon, 1);
  default_icon->size = size;
  default_icon->pixbuf = g_object_ref (pixbuf);
  icons = g_slist_prepend (icons, default_icon);

  /* Replaces value, leaves key untouched
   */
  g_hash_table_insert (icon_theme_builtin_icons, key, icons);
}

/* Look up a builtin icon; the min_difference_p and
 * has_larger_p out parameters allow us to combine
 * this lookup with searching through the actual directories
 * of the "hicolor" icon theme. See theme_lookup_icon()
 * for how they are used.
 */
static BuiltinIcon *
find_builtin_icon (const gchar *icon_name,
		   gint         size,
		   gint        *min_difference_p,
		   gboolean    *has_larger_p)
{
  GSList *icons = NULL;
  gint min_difference = G_MAXINT;
  gboolean has_larger = FALSE;
  BuiltinIcon *min_icon = NULL;
  
  if (!icon_theme_builtin_icons)
    return NULL;

  icons = g_hash_table_lookup (icon_theme_builtin_icons, icon_name);

  while (icons)
    {
      BuiltinIcon *default_icon = icons->data;
      int min, max, difference;
      gboolean smaller;
      
      min = default_icon->size - 2;
      max = default_icon->size + 2;
      smaller = size < min;
      if (size < min)
	difference = min - size;
      else if (size > max)
	difference = size - max;
      else
	difference = 0;
      
      if (difference == 0)
	{
	  min_difference = 0;
	  min_icon = default_icon;
	  break;
	}
      
      if (!has_larger)
	{
	  if (difference < min_difference || smaller)
	    {
	      min_difference = difference;
	      min_icon = default_icon;
	      has_larger = smaller;
	    }
	}
      else
	{
	  if (difference < min_difference && smaller)
	    {
	      min_difference = difference;
	      min_icon = default_icon;
	    }
	}
      
      icons = icons->next;
    }

  if (min_difference_p)
    *min_difference_p = min_difference;
  if (has_larger_p)
    *has_larger_p = has_larger;

  return min_icon;
}

void
_gtk_icon_theme_check_reload (GdkDisplay *display)
{
  gint n_screens, i;
  GdkScreen *screen;
  GtkIconTheme *icon_theme;

  n_screens = gdk_display_get_n_screens (display);
  
  for (i = 0; i < n_screens; i++)
    {
      screen = gdk_display_get_screen (display, i);

      icon_theme = g_object_get_data (G_OBJECT (screen), "gtk-icon-theme");
      if (icon_theme)
	{
	  icon_theme->priv->check_reload = TRUE;
	  ensure_valid_themes (icon_theme);
	  icon_theme->priv->check_reload = FALSE;
	}
    }
}


/**
 * gtk_icon_theme_lookup_by_gicon:
 * @icon_theme: a #GtkIconTheme
 * @icon: the #GIcon to look up
 * @size: desired icon size
 * @flags: flags modifying the behavior of the icon lookup
 * 
 * Looks up an icon and returns a structure containing
 * information such as the filename of the icon. 
 * The icon can then be rendered into a pixbuf using
 * gtk_icon_info_load_icon().
 *
 * Return value: a #GtkIconInfo structure containing 
 *     information about the icon, or %NULL if the icon 
 *     wasn't found. Free with gtk_icon_info_free()
 *
 * Since: 2.14
 */
GtkIconInfo *
gtk_icon_theme_lookup_by_gicon (GtkIconTheme       *icon_theme,
                                GIcon              *icon,
                                gint                size,
                                GtkIconLookupFlags  flags)
{
  GtkIconInfo *info;

  g_return_val_if_fail (GTK_IS_ICON_THEME (icon_theme), NULL);
  g_return_val_if_fail (G_IS_ICON (icon), NULL);

  if (G_IS_LOADABLE_ICON (icon))
    {
      info = icon_info_new ();
      info->loadable = G_LOADABLE_ICON (g_object_ref (icon));

      info->dir_type = ICON_THEME_DIR_UNTHEMED;
      info->dir_size = size;
      info->desired_size = size;
      info->threshold = 2;
      info->forced_size = (flags & GTK_ICON_LOOKUP_FORCE_SIZE) != 0;

      return info;
    }
  else if (G_IS_THEMED_ICON (icon))
    {
      const gchar **names;

      names = (const gchar **)g_themed_icon_get_names (G_THEMED_ICON (icon));
      info = gtk_icon_theme_choose_icon (icon_theme, names, size, flags);

      return info;
    }
  else if (G_IS_EMBLEMED_ICON (icon))
    {
      GIcon *base, *emblem;
      GList *list, *l;
      GtkIconInfo *emblem_info;

      base = g_emblemed_icon_get_icon (G_EMBLEMED_ICON (icon));
      info = gtk_icon_theme_lookup_by_gicon (icon_theme, base, size, flags);
      if (info)
        {
          list = g_emblemed_icon_get_emblems (G_EMBLEMED_ICON (icon));
          for (l = list; l; l = l->next)
            {
              emblem = g_emblem_get_icon (G_EMBLEM (l->data));
	      /* always force size for emblems */
              emblem_info = gtk_icon_theme_lookup_by_gicon (icon_theme, emblem, size / 2, flags | GTK_ICON_LOOKUP_FORCE_SIZE);
              if (emblem_info)
                info->emblem_infos = g_slist_prepend (info->emblem_infos, emblem_info);
            }
        }

      return info;
    }
  else if (GDK_IS_PIXBUF (icon))
    {
      GdkPixbuf *pixbuf;

      pixbuf = GDK_PIXBUF (icon);

      if ((flags & GTK_ICON_LOOKUP_FORCE_SIZE) != 0)
	{
	  gint width, height, max;
	  gdouble scale;
	  GdkPixbuf *scaled;

	  width = gdk_pixbuf_get_width (pixbuf);
	  height = gdk_pixbuf_get_height (pixbuf);
	  max = MAX (width, height);
	  scale = (gdouble) size / (gdouble) max;

	  scaled = gdk_pixbuf_scale_simple (pixbuf,
					    0.5 + width * scale,
					    0.5 + height * scale,
					    GDK_INTERP_BILINEAR);

	  info = gtk_icon_info_new_for_pixbuf (icon_theme, scaled);

	  g_object_unref (scaled);
	}
      else
	{
	  info = gtk_icon_info_new_for_pixbuf (icon_theme, pixbuf);
	}

      return info;
    }

  return NULL;
}

/**
 * gtk_icon_info_new_for_pixbuf:
 * @icon_theme: a #GtkIconTheme
 * @pixbuf: the pixbuf to wrap in a #GtkIconInfo
 *
 * Creates a #GtkIconInfo for a #GdkPixbuf.
 *
 * Returns: a #GtkIconInfo
 *
 * Since: 2.14
 */
GtkIconInfo *
gtk_icon_info_new_for_pixbuf (GtkIconTheme *icon_theme,
                              GdkPixbuf    *pixbuf)
{
  GtkIconInfo *info;

  g_return_val_if_fail (GTK_IS_ICON_THEME (icon_theme), NULL);
  g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), NULL);

  info = icon_info_new ();
  info->pixbuf = g_object_ref (pixbuf);
  info->scale = 1.0;
  info->dir_type = ICON_THEME_DIR_UNTHEMED;

  return info;
}

#if defined (G_OS_WIN32) && !defined (_WIN64)

/* DLL ABI stability backward compatibility versions */

#undef gtk_icon_theme_set_search_path

void
gtk_icon_theme_set_search_path (GtkIconTheme *icon_theme,
				const gchar  *path[],
				gint          n_elements)
{
  const gchar **utf8_path;
  gint i;

  utf8_path = g_new (const gchar *, n_elements);

  for (i = 0; i < n_elements; i++)
    utf8_path[i] = g_locale_to_utf8 (path[i], -1, NULL, NULL, NULL);

  gtk_icon_theme_set_search_path_utf8 (icon_theme, utf8_path, n_elements);

  for (i = 0; i < n_elements; i++)
    g_free ((gchar *) utf8_path[i]);

  g_free (utf8_path);
}

#undef gtk_icon_theme_get_search_path

void
gtk_icon_theme_get_search_path (GtkIconTheme      *icon_theme,
				gchar            **path[],
				gint              *n_elements)
{
  gint i, n;

  gtk_icon_theme_get_search_path_utf8 (icon_theme, path, &n);

  if (n_elements)
    *n_elements = n;

  if (path)
    {
      for (i = 0; i < n; i++)
	{
	  gchar *tem = (*path)[i];

	  (*path)[i] = g_locale_from_utf8 ((*path)[i], -1, NULL, NULL, NULL);
	  g_free (tem);
	}
    }
}

#undef gtk_icon_theme_append_search_path

void
gtk_icon_theme_append_search_path (GtkIconTheme *icon_theme,
				   const gchar  *path)
{
  gchar *utf8_path = g_locale_from_utf8 (path, -1, NULL, NULL, NULL);

  gtk_icon_theme_append_search_path_utf8 (icon_theme, utf8_path);

  g_free (utf8_path);
}

#undef gtk_icon_theme_prepend_search_path

void
gtk_icon_theme_prepend_search_path (GtkIconTheme *icon_theme,
				    const gchar  *path)
{
  gchar *utf8_path = g_locale_from_utf8 (path, -1, NULL, NULL, NULL);

  gtk_icon_theme_prepend_search_path_utf8 (icon_theme, utf8_path);

  g_free (utf8_path);
}

#undef gtk_icon_info_get_filename

const gchar *
gtk_icon_info_get_filename (GtkIconInfo *icon_info)
{
  g_return_val_if_fail (icon_info != NULL, NULL);

  return icon_info->cp_filename;
}

#endif

#define __GTK_ICON_THEME_C__
#include "gtkaliasdef.c"
