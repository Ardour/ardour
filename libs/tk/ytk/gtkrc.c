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

#include "config.h"

#include <locale.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/stat.h>
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>


#include <glib.h>
#include <glib/gstdio.h>
#include "gdkconfig.h"

#include "gtkversion.h"
#include "gtkrc.h"
#include "gtkbindings.h"
#include "gtkthemes.h"
#include "gtkintl.h"
#include "gtkiconfactory.h"
#include "gtkmain.h"
#include "gtkmodules.h"
#include "gtkprivate.h"
#include "gtksettings.h"
#include "gtkwindow.h"

#include "gtkalias.h"

#ifdef G_OS_WIN32
#include <io.h>
#endif

typedef struct _GtkRcSet    GtkRcSet;
typedef struct _GtkRcNode   GtkRcNode;
typedef struct _GtkRcFile   GtkRcFile;

enum 
{
  PATH_ELT_PSPEC,
  PATH_ELT_UNRESOLVED,
  PATH_ELT_TYPE
};

typedef struct
{
  gint type;
  union 
  {
    GType         class_type;
    gchar        *class_name;
    GPatternSpec *pspec;
  } elt;
} PathElt;

struct _GtkRcSet
{
  GtkPathType   type;

  GPatternSpec *pspec;
  GSList       *path;

  GtkRcStyle   *rc_style;
  gint          priority;
};

struct _GtkRcFile
{
  time_t mtime;
  gchar *name;
  gchar *canonical_name;
  gchar *directory;
  guint  reload    : 1;
  guint  is_string : 1;	/* If TRUE, name is a string to parse with gtk_rc_parse_string() */
};


struct _GtkRcContext
{
  GHashTable *rc_style_ht;
  GtkSettings *settings;
  GSList *rc_sets_widget;
  GSList *rc_sets_widget_class;
  GSList *rc_sets_class;

  /* The files we have parsed, to reread later if necessary */
  GSList *rc_files;

  gchar *theme_name;
  gchar *key_theme_name;
  gchar *font_name;
  
  gchar **pixmap_path;

  gint default_priority;
  GtkStyle *default_style;

  GHashTable *color_hash;

  guint reloading : 1;
};

#define GTK_RC_STYLE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GTK_TYPE_RC_STYLE, GtkRcStylePrivate))

typedef struct _GtkRcStylePrivate GtkRcStylePrivate;

struct _GtkRcStylePrivate
{
  GSList *color_hashes;
};

static GtkRcContext *gtk_rc_context_get              (GtkSettings     *settings);

static guint       gtk_rc_style_hash                 (const gchar     *name);
static gboolean    gtk_rc_style_equal                (const gchar     *a,
                                                      const gchar     *b);
static guint       gtk_rc_styles_hash                (const GSList    *rc_styles);
static gboolean    gtk_rc_styles_equal               (const GSList    *a,
                                                      const GSList    *b);
static GtkRcStyle* gtk_rc_style_find                 (GtkRcContext    *context,
						      const gchar     *name);
static GSList *    gtk_rc_styles_match               (GSList          *rc_styles,
                                                      GSList          *sets,
                                                      guint            path_length,
                                                      gchar           *path,
                                                      gchar           *path_reversed);
static GtkStyle *  gtk_rc_style_to_style             (GtkRcContext    *context,
						      GtkRcStyle      *rc_style);
static GtkStyle*   gtk_rc_init_style                 (GtkRcContext    *context,
						      GSList          *rc_styles);
static void        gtk_rc_parse_default_files        (GtkRcContext    *context);
static void        gtk_rc_parse_named                (GtkRcContext    *context,
						      const gchar     *name,
						      const gchar     *type);
static void        gtk_rc_context_parse_file         (GtkRcContext    *context,
						      const gchar     *filename,
						      gint             priority,
                                                      gboolean         reload);
static void        gtk_rc_parse_any                  (GtkRcContext    *context,
						      const gchar     *input_name,
                                                      gint             input_fd,
                                                      const gchar     *input_string);
static guint       gtk_rc_parse_statement            (GtkRcContext    *context,
						      GScanner        *scanner);
static guint       gtk_rc_parse_style                (GtkRcContext    *context,
						      GScanner        *scanner);
static guint       gtk_rc_parse_assignment           (GScanner        *scanner,
                                                      GtkRcStyle      *style,
						      GtkRcProperty   *prop);
static guint       gtk_rc_parse_bg                   (GScanner        *scanner,
                                                      GtkRcStyle      *style);
static guint       gtk_rc_parse_fg                   (GScanner        *scanner,
                                                      GtkRcStyle      *style);
static guint       gtk_rc_parse_text                 (GScanner        *scanner,
                                                      GtkRcStyle      *style);
static guint       gtk_rc_parse_base                 (GScanner        *scanner,
                                                      GtkRcStyle      *style);
static guint       gtk_rc_parse_xthickness           (GScanner        *scanner,
                                                      GtkRcStyle      *style);
static guint       gtk_rc_parse_ythickness           (GScanner        *scanner,
                                                      GtkRcStyle      *style);
static guint       gtk_rc_parse_bg_pixmap            (GtkRcContext    *context,
						      GScanner        *scanner,
                                                      GtkRcStyle      *rc_style);
static guint       gtk_rc_parse_font                 (GScanner        *scanner,
                                                      GtkRcStyle      *rc_style);
static guint       gtk_rc_parse_fontset              (GScanner        *scanner,
                                                      GtkRcStyle      *rc_style);
static guint       gtk_rc_parse_font_name            (GScanner        *scanner,
                                                      GtkRcStyle      *rc_style);
static guint       gtk_rc_parse_engine               (GtkRcContext    *context,
						      GScanner        *scanner,
                                                      GtkRcStyle     **rc_style);
static guint       gtk_rc_parse_pixmap_path          (GtkRcContext    *context,
						      GScanner        *scanner);
static void        gtk_rc_parse_pixmap_path_string   (GtkRcContext    *context,
						      GScanner        *scanner,
						      const gchar     *pix_path);
static guint       gtk_rc_parse_module_path          (GScanner        *scanner);
static guint       gtk_rc_parse_im_module_file       (GScanner        *scanner);
static guint       gtk_rc_parse_path_pattern         (GtkRcContext    *context,
						      GScanner        *scanner);
static guint       gtk_rc_parse_stock                (GtkRcContext    *context,
						      GScanner        *scanner,
                                                      GtkRcStyle      *rc_style,
                                                      GtkIconFactory  *factory);
static guint       gtk_rc_parse_logical_color        (GScanner        *scanner,
                                                      GtkRcStyle      *rc_style,
                                                      GHashTable      *hash);

static void        gtk_rc_clear_hash_node            (gpointer         key,
                                                      gpointer         data,
                                                      gpointer         user_data);
static void        gtk_rc_clear_styles               (GtkRcContext    *context);
static void        gtk_rc_add_initial_default_files  (void);

static void        gtk_rc_style_finalize             (GObject         *object);
static void        gtk_rc_style_real_merge           (GtkRcStyle      *dest,
                                                      GtkRcStyle      *src);
static GtkRcStyle* gtk_rc_style_real_create_rc_style (GtkRcStyle      *rc_style);
static GtkStyle*   gtk_rc_style_real_create_style    (GtkRcStyle      *rc_style);
static void        gtk_rc_style_copy_icons_and_colors(GtkRcStyle      *rc_style,
                                                      GtkRcStyle      *src_style,
                                                      GtkRcContext    *context);
static gint	   gtk_rc_properties_cmp	     (gconstpointer    bsearch_node1,
						      gconstpointer    bsearch_node2);
static void        gtk_rc_set_free                   (GtkRcSet        *rc_set);

static void	   insert_rc_property		     (GtkRcStyle      *style,
						      GtkRcProperty   *property,
						      gboolean         replace);


static const GScannerConfig gtk_rc_scanner_config =
{
  (
   " \t\r\n"
   )			/* cset_skip_characters */,
  (
   "_"
   G_CSET_a_2_z
   G_CSET_A_2_Z
   )			/* cset_identifier_first */,
  (
   G_CSET_DIGITS
   "-_"
   G_CSET_a_2_z
   G_CSET_A_2_Z
   )			/* cset_identifier_nth */,
  ( "#\n" )		/* cpair_comment_single */,
  
  TRUE			/* case_sensitive */,
  
  TRUE			/* skip_comment_multi */,
  TRUE			/* skip_comment_single */,
  TRUE			/* scan_comment_multi */,
  TRUE			/* scan_identifier */,
  FALSE			/* scan_identifier_1char */,
  FALSE			/* scan_identifier_NULL */,
  TRUE			/* scan_symbols */,
  TRUE			/* scan_binary */,
  TRUE			/* scan_octal */,
  TRUE			/* scan_float */,
  TRUE			/* scan_hex */,
  TRUE			/* scan_hex_dollar */,
  TRUE			/* scan_string_sq */,
  TRUE			/* scan_string_dq */,
  TRUE			/* numbers_2_int */,
  FALSE			/* int_2_float */,
  FALSE			/* identifier_2_string */,
  TRUE			/* char_2_token */,
  TRUE			/* symbol_2_token */,
  FALSE			/* scope_0_fallback */,
};
 
static const gchar symbol_names[] = 
  "include\0"
  "NORMAL\0"
  "ACTIVE\0"
  "PRELIGHT\0"
  "SELECTED\0"
  "INSENSITIVE\0"
  "fg\0"
  "bg\0"
  "text\0"
  "base\0"
  "xthickness\0"
  "ythickness\0"
  "font\0"
  "fontset\0"
  "font_name\0"
  "bg_pixmap\0"
  "pixmap_path\0"
  "style\0"
  "binding\0"
  "bind\0"
  "widget\0"
  "widget_class\0"
  "class\0"
  "lowest\0"
  "gtk\0"
  "application\0"
  "theme\0"
  "rc\0"
  "highest\0"
  "engine\0"
  "module_path\0"
  "stock\0"
  "im_module_file\0"
  "LTR\0"
  "RTL\0"
  "color\0"
  "unbind\0";

static const struct
{
  guint name_offset;
  guint token;
} symbols[] = {
  {   0, GTK_RC_TOKEN_INCLUDE },
  {   8, GTK_RC_TOKEN_NORMAL },
  {  15, GTK_RC_TOKEN_ACTIVE },
  {  22, GTK_RC_TOKEN_PRELIGHT },
  {  31, GTK_RC_TOKEN_SELECTED },
  {  40, GTK_RC_TOKEN_INSENSITIVE },
  {  52, GTK_RC_TOKEN_FG },
  {  55, GTK_RC_TOKEN_BG },
  {  58, GTK_RC_TOKEN_TEXT },
  {  63, GTK_RC_TOKEN_BASE },
  {  68, GTK_RC_TOKEN_XTHICKNESS },
  {  79, GTK_RC_TOKEN_YTHICKNESS },
  {  90, GTK_RC_TOKEN_FONT },
  {  95, GTK_RC_TOKEN_FONTSET },
  { 103, GTK_RC_TOKEN_FONT_NAME },
  { 113, GTK_RC_TOKEN_BG_PIXMAP },
  { 123, GTK_RC_TOKEN_PIXMAP_PATH },
  { 135, GTK_RC_TOKEN_STYLE },
  { 141, GTK_RC_TOKEN_BINDING },
  { 149, GTK_RC_TOKEN_BIND },
  { 154, GTK_RC_TOKEN_WIDGET },
  { 161, GTK_RC_TOKEN_WIDGET_CLASS },
  { 174, GTK_RC_TOKEN_CLASS },
  { 180, GTK_RC_TOKEN_LOWEST },
  { 187, GTK_RC_TOKEN_GTK },
  { 191, GTK_RC_TOKEN_APPLICATION },
  { 203, GTK_RC_TOKEN_THEME },
  { 209, GTK_RC_TOKEN_RC },
  { 212, GTK_RC_TOKEN_HIGHEST },
  { 220, GTK_RC_TOKEN_ENGINE },
  { 227, GTK_RC_TOKEN_MODULE_PATH },
  { 239, GTK_RC_TOKEN_STOCK },
  { 245, GTK_RC_TOKEN_IM_MODULE_FILE },
  { 260, GTK_RC_TOKEN_LTR },
  { 264, GTK_RC_TOKEN_RTL },
  { 268, GTK_RC_TOKEN_COLOR },
  { 274, GTK_RC_TOKEN_UNBIND }
};

static GHashTable *realized_style_ht = NULL;

static gchar *im_module_file = NULL;

static gint    max_default_files = 0;
static gchar **gtk_rc_default_files = NULL;

/* A stack of information of RC files we are parsing currently.
 * The directories for these files are implicitely added to the end of
 * PIXMAP_PATHS.
 */
static GSList *current_files_stack = NULL;

/* RC files and strings that are parsed for every context
 */
static GSList *global_rc_files = NULL;

/* Keep list of all current RC contexts for convenience
 */
static GSList *rc_contexts;

/* RC file handling */

static gchar *
gtk_rc_make_default_dir (const gchar *type)
{
  const gchar *var;
  gchar *path;

  var = g_getenv ("GTK_EXE_PREFIX");

  if (var)
    path = g_build_filename (var, "lib", "gtk-2.0", GTK_BINARY_VERSION, type, NULL);
  else
    path = g_build_filename (GTK_LIBDIR, "gtk-2.0", GTK_BINARY_VERSION, type, NULL);

  return path;
}

/**
 * gtk_rc_get_im_module_path:
 * @returns: a newly-allocated string containing the path in which to 
 *    look for IM modules.
 *
 * Obtains the path in which to look for IM modules. See the documentation
 * of the <link linkend="im-module-path"><envar>GTK_PATH</envar></link>
 * environment variable for more details about looking up modules. This
 * function is useful solely for utilities supplied with GTK+ and should
 * not be used by applications under normal circumstances.
 */
gchar *
gtk_rc_get_im_module_path (void)
{
  gchar **paths = _gtk_get_module_path ("immodules");
  gchar *result = g_strjoinv (G_SEARCHPATH_SEPARATOR_S, paths);
  g_strfreev (paths);

  return result;
}

/**
 * gtk_rc_get_im_module_file:
 * @returns: a newly-allocated string containing the name of the file
 * listing the IM modules available for loading
 *
 * Obtains the path to the IM modules file. See the documentation
 * of the <link linkend="im-module-file"><envar>GTK_IM_MODULE_FILE</envar></link>
 * environment variable for more details.
 */
gchar *
gtk_rc_get_im_module_file (void)
{
  const gchar *var = g_getenv ("GTK_IM_MODULE_FILE");
  gchar *result = NULL;

  if (var)
    result = g_strdup (var);

  if (!result)
    {
      if (im_module_file)
	result = g_strdup (im_module_file);
      else
        result = gtk_rc_make_default_dir ("immodules.cache");
    }

  return result;
}

gchar *
gtk_rc_get_theme_dir (void)
{
  const gchar *var;
  gchar *path;

  var = g_getenv ("GTK_DATA_PREFIX");

  if (var)
    path = g_build_filename (var, "share", "themes", NULL);
  else
    path = g_build_filename (GTK_DATA_PREFIX, "share", "themes", NULL);

  return path;
}

/**
 * gtk_rc_get_module_dir:
 * 
 * Returns a directory in which GTK+ looks for theme engines.
 * For full information about the search for theme engines,
 * see the docs for <envar>GTK_PATH</envar> in
 * <xref linkend="gtk-running"/>.
 * 
 * return value: the directory. (Must be freed with g_free())
 **/
gchar *
gtk_rc_get_module_dir (void)
{
  return gtk_rc_make_default_dir ("engines");
}

static void
gtk_rc_add_initial_default_files (void)
{
  static gint init = FALSE;
  const gchar *var;
  gchar *str;
  gchar **files;
  gint i;

  if (init)
    return;
 
  gtk_rc_default_files = g_new (gchar*, 10);
  max_default_files = 10;

  gtk_rc_default_files[0] = NULL;
  init = TRUE;

  var = g_getenv ("GTK2_RC_FILES");

  if (var)
    {
      files = g_strsplit (var, G_SEARCHPATH_SEPARATOR_S, -1);
      i=0;
      while (files[i])
	{
	  gtk_rc_add_default_file (files[i]);
	  i++;
	}
      g_strfreev (files);
    }
  else
    {
      const gchar *home;
      const gchar * const *config_dirs;
      const gchar *config_dir;

      str = g_build_filename (GTK_DATA_PREFIX, "share", "gtk-2.0", "gtkrc", NULL);
      gtk_rc_add_default_file (str);
      g_free (str);

      config_dirs = g_get_system_config_dirs ();
      for (config_dir = *config_dirs; *config_dirs != NULL; config_dirs++)
        {
          str = g_build_filename (config_dir, "gtk-2.0", "gtkrc", NULL);
          gtk_rc_add_default_file (str);
          g_free (str);
        }

      str = g_build_filename (GTK_SYSCONFDIR, "gtk-2.0", "gtkrc", NULL);
      gtk_rc_add_default_file (str);
      g_free (str);

      home = g_get_home_dir ();
      if (home)
	{
	  str = g_build_filename (home, ".gtkrc-2.0", NULL);
	  gtk_rc_add_default_file (str);
	  g_free (str);
	}
    }
}

/**
 * gtk_rc_add_default_file:
 * @filename: the pathname to the file. If @filename is not absolute, it
 *    is searched in the current directory.
 * 
 * Adds a file to the list of files to be parsed at the
 * end of gtk_init().
 **/
void
gtk_rc_add_default_file (const gchar *filename)
{
  guint n;
  
  gtk_rc_add_initial_default_files ();

  for (n = 0; n < max_default_files; n++) 
    {
      if (gtk_rc_default_files[n] == NULL)
	break;
    }

  if (n == max_default_files)
    {
      max_default_files += 10;
      gtk_rc_default_files = g_renew (gchar*, gtk_rc_default_files, max_default_files);
    }
  
  gtk_rc_default_files[n++] = g_strdup (filename);
  gtk_rc_default_files[n] = NULL;
}

/**
 * gtk_rc_set_default_files:
 * @filenames: A %NULL-terminated list of filenames.
 * 
 * Sets the list of files that GTK+ will read at the
 * end of gtk_init().
 **/
void
gtk_rc_set_default_files (gchar **filenames)
{
  gint i;

  gtk_rc_add_initial_default_files ();

  i = 0;
  while (gtk_rc_default_files[i])
    {
      g_free (gtk_rc_default_files[i]);
      i++;
    }
    
  gtk_rc_default_files[0] = NULL;

  i = 0;
  while (filenames[i] != NULL)
    {
      gtk_rc_add_default_file (filenames[i]);
      i++;
    }
}

/**
 * gtk_rc_get_default_files:
 *
 * Retrieves the current list of RC files that will be parsed
 * at the end of gtk_init().
 *
 * Return value: (transfer none)  (array zero-terminated=1) (element-type filename):
 *     A %NULL-terminated array of filenames.
 *     This memory is owned by GTK+ and must not be freed by the application.
 *     If you want to store this information, you should make a copy.
 **/
gchar **
gtk_rc_get_default_files (void)
{
  gtk_rc_add_initial_default_files ();

  return gtk_rc_default_files;
}

static void
gtk_rc_settings_changed (GtkSettings  *settings,
			 GParamSpec   *pspec,
			 GtkRcContext *context)
{
  gchar *new_theme_name;
  gchar *new_key_theme_name;

  if (context->reloading)
    return;

  g_object_get (settings,
		"gtk-theme-name", &new_theme_name,
		"gtk-key-theme-name", &new_key_theme_name,
		NULL);

  if ((new_theme_name != context->theme_name && 
       !(new_theme_name && context->theme_name && strcmp (new_theme_name, context->theme_name) == 0)) ||
      (new_key_theme_name != context->key_theme_name &&
       !(new_key_theme_name && context->key_theme_name && strcmp (new_key_theme_name, context->key_theme_name) == 0)))
    {
      gtk_rc_reparse_all_for_settings (settings, TRUE);
    }

  g_free (new_theme_name);
  g_free (new_key_theme_name);
}

static void
gtk_rc_font_name_changed (GtkSettings  *settings,
                          GParamSpec   *pspec,
                          GtkRcContext *context)
{
  if (!context->reloading)
    _gtk_rc_context_get_default_font_name (settings);
}

static void
gtk_rc_color_hash_changed (GtkSettings  *settings,
			   GParamSpec   *pspec,
			   GtkRcContext *context)
{
  GHashTable *old_hash;

  old_hash = context->color_hash;

  g_object_get (settings, "color-hash", &context->color_hash, NULL);

  if (old_hash)
    g_hash_table_unref (old_hash);

  gtk_rc_reparse_all_for_settings (settings, TRUE);
}

static GtkRcContext *
gtk_rc_context_get (GtkSettings *settings)
{
  if (!settings->rc_context)
    {
      GtkRcContext *context = settings->rc_context = g_new (GtkRcContext, 1);

      context->settings = settings;
      context->rc_style_ht = NULL;
      context->rc_sets_widget = NULL;
      context->rc_sets_widget_class = NULL;
      context->rc_sets_class = NULL;
      context->rc_files = NULL;
      context->default_style = NULL;
      context->reloading = FALSE;

      g_object_get (settings,
		    "gtk-theme-name", &context->theme_name,
		    "gtk-key-theme-name", &context->key_theme_name,
		    "gtk-font-name", &context->font_name,
		    "color-hash", &context->color_hash,
		    NULL);

      g_signal_connect (settings,
			"notify::gtk-theme-name",
			G_CALLBACK (gtk_rc_settings_changed),
			context);
      g_signal_connect (settings,
			"notify::gtk-key-theme-name",
			G_CALLBACK (gtk_rc_settings_changed),
			context);
      g_signal_connect (settings,
			"notify::gtk-font-name",
			G_CALLBACK (gtk_rc_font_name_changed),
			context);
      g_signal_connect (settings,
			"notify::color-hash",
			G_CALLBACK (gtk_rc_color_hash_changed),
			context);

      context->pixmap_path = NULL;

      context->default_priority = GTK_PATH_PRIO_RC;

      rc_contexts = g_slist_prepend (rc_contexts, settings->rc_context);
    }

  return settings->rc_context;
}

static void 
gtk_rc_clear_rc_files (GtkRcContext *context)
{
  GSList *list;

  list = context->rc_files;
  while (list)
    {
      GtkRcFile *rc_file = list->data;
      
      if (rc_file->canonical_name != rc_file->name)
	g_free (rc_file->canonical_name);
      g_free (rc_file->directory);
      g_free (rc_file->name);
      g_free (rc_file);
      
      list = list->next;
    }
  
  g_slist_free (context->rc_files);
  context->rc_files = NULL;
}

void
_gtk_rc_context_destroy (GtkSettings *settings)
{
  GtkRcContext *context;

  g_return_if_fail (GTK_IS_SETTINGS (settings));

  context = settings->rc_context;
  if (!context)
    return;

  _gtk_settings_reset_rc_values (context->settings);
  gtk_rc_clear_styles (context);
  gtk_rc_clear_rc_files (context);

  if (context->default_style)
    g_object_unref (context->default_style);

  g_strfreev (context->pixmap_path);

  g_free (context->theme_name);
  g_free (context->key_theme_name);
  g_free (context->font_name);

  if (context->color_hash)
    g_hash_table_unref (context->color_hash);

  g_signal_handlers_disconnect_by_func (settings,
					gtk_rc_settings_changed, context);
  g_signal_handlers_disconnect_by_func (settings,
					gtk_rc_font_name_changed, context);
  g_signal_handlers_disconnect_by_func (settings,
					gtk_rc_color_hash_changed, context);

  rc_contexts = g_slist_remove (rc_contexts, context);

  g_free (context);

  settings->rc_context = NULL;
}

static void
gtk_rc_parse_named (GtkRcContext *context,
		    const gchar  *name,
		    const gchar  *type)
{
  gchar *path = NULL;
  const gchar *home_dir;
  gchar *subpath;

  if (type)
    subpath = g_strconcat ("gtk-2.0-", type,
			   G_DIR_SEPARATOR_S "gtkrc",
			   NULL);
  else
    subpath = g_strdup ("gtk-2.0" G_DIR_SEPARATOR_S "gtkrc");
  
  /* First look in the users home directory
   */
  home_dir = g_get_home_dir ();
  if (home_dir)
    {
      path = g_build_filename (home_dir, ".themes", name, subpath, NULL);
      if (!g_file_test (path, G_FILE_TEST_EXISTS))
	{
	  g_free (path);
	  path = NULL;
	}
    }

  if (!path)
    {
      gchar *theme_dir = gtk_rc_get_theme_dir ();
      path = g_build_filename (theme_dir, name, subpath, NULL);
      g_free (theme_dir);
      
      if (!g_file_test (path, G_FILE_TEST_EXISTS))
	{
	  g_free (path);
	  path = NULL;
	}
    }

  if (path)
    {
      gtk_rc_context_parse_file (context, path, GTK_PATH_PRIO_THEME, FALSE);
      g_free (path);
    }

  g_free (subpath);
}

static void
gtk_rc_parse_default_files (GtkRcContext *context)
{
  gint i;

  gtk_rc_add_initial_default_files ();

  for (i = 0; gtk_rc_default_files[i] != NULL; i++)
    gtk_rc_context_parse_file (context, gtk_rc_default_files[i], GTK_PATH_PRIO_RC, FALSE);
}

void
_gtk_rc_init (void)
{
  static gboolean initialized = FALSE;

  if (!initialized)
    {
      initialized = TRUE;
      
      gtk_rc_add_initial_default_files ();
    }
  
  /* Default RC string */
  gtk_rc_parse_string ("style \"gtk-default-tooltips-style\" {\n"
		       "  bg[NORMAL] = \"#eee1b3\"\n"
		       "  fg[NORMAL] = \"#000000\"\n"
		       "}\n"
		       "\n"
		       "style \"gtk-default-progress-bar-style\" {\n"
		       "  bg[PRELIGHT] = \"#4b6983\"\n"
		       "  fg[PRELIGHT] = \"#ffffff\"\n"
		       "  bg[NORMAL]   = \"#c4c2bd\"\n"
		       "}\n"
		       "\n"
		       "style \"gtk-default-entry-style\" {\n"
		       "  bg[SELECTED] = \"#b7c3cd\"\n"
		       "  fg[SELECTED] = \"#000000\"\n"
		       "}\n"
		       "\n"
		       "style \"gtk-default-menu-bar-item-style\" {\n"
		       "  GtkMenuItem::horizontal_padding = 5\n"
		       "}\n"
		       "\n"
		       "style \"gtk-default-menu-item-style\" {\n"
		       "  bg[PRELIGHT] = \"#4b6983\"\n"
		       "  fg[PRELIGHT] = \"#ffffff\"\n"
		       "  base[PRELIGHT] = \"#4b6983\"\n"
		       "  text[PRELIGHT] = \"#ffffff\"\n"
		       "}\n"
		       "\n"
                       /* Work around clipping of accelerator underlines */
                       "style \"gtk-default-label-style\" {\n"
                       "  GtkWidget::draw-border = {0,0,0,1}\n"
                       "}\n"
                       "\n"    
		       "class \"GtkProgressBar\" style : gtk \"gtk-default-progress-bar-style\"\n"
		       "class \"GtkEntry\" style : gtk \"gtk-default-entry-style\"\n"
		       "widget \"gtk-tooltip*\" style : gtk \"gtk-default-tooltips-style\"\n"
		       "widget_class \"*<GtkMenuItem>*\" style : gtk \"gtk-default-menu-item-style\"\n"
		       "widget_class \"*<GtkMenuBar>*<GtkMenuItem>\" style : gtk \"gtk-default-menu-bar-item-style\"\n"
                       "class \"GtkLabel\" style : gtk \"gtk-default-label-style\"\n"
      );
}
  
static void
gtk_rc_context_parse_string (GtkRcContext *context,
			     const gchar  *rc_string)
{
  gtk_rc_parse_any (context, "-", -1, rc_string);
}

void
gtk_rc_parse_string (const gchar *rc_string)
{
  GtkRcFile *rc_file;
  GSList *tmp_list;
      
  g_return_if_fail (rc_string != NULL);

  rc_file = g_new (GtkRcFile, 1);
  rc_file->is_string = TRUE;
  rc_file->name = g_strdup (rc_string);
  rc_file->canonical_name = NULL;
  rc_file->directory = NULL;
  rc_file->mtime = 0;
  rc_file->reload = TRUE;
  
  global_rc_files = g_slist_append (global_rc_files, rc_file);

  for (tmp_list = rc_contexts; tmp_list; tmp_list = tmp_list->next)
    gtk_rc_context_parse_string (tmp_list->data, rc_string);
}

static GtkRcFile *
add_to_rc_file_list (GSList     **rc_file_list,
		     const char  *filename,
		     gboolean     reload)
{
  GSList *tmp_list;
  GtkRcFile *rc_file;
  
  tmp_list = *rc_file_list;
  while (tmp_list)
    {
      rc_file = tmp_list->data;
      if (!strcmp (rc_file->name, filename))
	return rc_file;
      
      tmp_list = tmp_list->next;
    }

  rc_file = g_new (GtkRcFile, 1);
  rc_file->is_string = FALSE;
  rc_file->name = g_strdup (filename);
  rc_file->canonical_name = NULL;
  rc_file->directory = NULL;
  rc_file->mtime = 0;
  rc_file->reload = reload;
  
  *rc_file_list = g_slist_append (*rc_file_list, rc_file);
  
  return rc_file;
}

static void
gtk_rc_context_parse_one_file (GtkRcContext *context,
			       const gchar  *filename,
			       gint          priority,
			       gboolean      reload)
{
  GtkRcFile *rc_file;
  GStatBuf statbuf;
  gint saved_priority;

  g_return_if_fail (filename != NULL);

  saved_priority = context->default_priority;
  context->default_priority = priority;

  rc_file = add_to_rc_file_list (&context->rc_files, filename, reload);

  if (!rc_file->canonical_name)
    {
      /* Get the absolute pathname */

      if (g_path_is_absolute (rc_file->name))
	rc_file->canonical_name = rc_file->name;
      else
	{
	  gchar *cwd;

	  cwd = g_get_current_dir ();
	  rc_file->canonical_name = g_build_filename (cwd, rc_file->name, NULL);
	  g_free (cwd);
	}
      
      rc_file->directory = g_path_get_dirname (rc_file->canonical_name);
    }

  /* If the file is already being parsed (recursion), do nothing
   */
  if (g_slist_find (current_files_stack, rc_file))
    return;

  if (!g_lstat (rc_file->canonical_name, &statbuf))
    {
      gint fd;
      
      rc_file->mtime = statbuf.st_mtime;

      fd = g_open (rc_file->canonical_name, O_RDONLY, 0);
      if (fd < 0)
	goto out;

      /* Temporarily push information for this file on
       * a stack of current files while parsing it.
       */
      current_files_stack = g_slist_prepend (current_files_stack, rc_file);
      gtk_rc_parse_any (context, filename, fd, NULL);
      current_files_stack = g_slist_delete_link (current_files_stack,
						 current_files_stack);

      close (fd);
    }

 out:
  context->default_priority = saved_priority;
}

static gchar *
strchr_len (const gchar *str, gint len, char c)
{
  while (len--)
    {
      if (*str == c)
	return (gchar *)str;

      str++;
    }

  return NULL;
}

static void
gtk_rc_context_parse_file (GtkRcContext *context,
			   const gchar  *filename,
			   gint          priority,
			   gboolean      reload)
{
  gchar *locale_suffixes[2];
  gint n_locale_suffixes = 0;
  gchar *p;
  gchar *locale;
  gint length, j;
  gboolean found = FALSE;

  locale = _gtk_get_lc_ctype ();

  if (strcmp (locale, "C") && strcmp (locale, "POSIX"))
    {
      /* Determine locale-specific suffixes for RC files.
       */
      length = strlen (locale);
      
      p = strchr (locale, '@');
      if (p)
	length = p - locale;

      p = strchr_len (locale, length, '.');
      if (p)
	length = p - locale;
      
      locale_suffixes[n_locale_suffixes++] = g_strndup (locale, length);
      
      p = strchr_len (locale, length, '_');
      if (p)
	{
	  length = p - locale;
	  locale_suffixes[n_locale_suffixes++] = g_strndup (locale, length);
	}
    }

  g_free (locale);
  
  gtk_rc_context_parse_one_file (context, filename, priority, reload);
  for (j = 0; j < n_locale_suffixes; j++)
    {
      if (!found)
	{
	  gchar *name = g_strconcat (filename, ".", locale_suffixes[j], NULL);
	  if (g_file_test (name, G_FILE_TEST_EXISTS))
	    {
	      gtk_rc_context_parse_one_file (context, name, priority, FALSE);
	      found = TRUE;
	    }
	      
	  g_free (name);
	}
      
      g_free (locale_suffixes[j]);
    }
}

void
gtk_rc_parse (const gchar *filename)
{
  GSList *tmp_list;
  
  g_return_if_fail (filename != NULL);

  add_to_rc_file_list (&global_rc_files, filename, TRUE);
  
  for (tmp_list = rc_contexts; tmp_list; tmp_list = tmp_list->next)
    gtk_rc_context_parse_file (tmp_list->data, filename, GTK_PATH_PRIO_RC, TRUE);
}

/* Handling of RC styles */

G_DEFINE_TYPE (GtkRcStyle, gtk_rc_style, G_TYPE_OBJECT)

static void
gtk_rc_style_init (GtkRcStyle *style)
{
  GtkRcStylePrivate *priv = GTK_RC_STYLE_GET_PRIVATE (style);
  guint i;

  style->name = NULL;
  style->font_desc = NULL;

  for (i = 0; i < 5; i++)
    {
      static const GdkColor init_color = { 0, 0, 0, 0, };

      style->bg_pixmap_name[i] = NULL;
      style->color_flags[i] = 0;
      style->fg[i] = init_color;
      style->bg[i] = init_color;
      style->text[i] = init_color;
      style->base[i] = init_color;
    }
  style->xthickness = -1;
  style->ythickness = -1;
  style->rc_properties = NULL;

  style->rc_style_lists = NULL;
  style->icon_factories = NULL;

  priv->color_hashes = NULL;
}

static void
gtk_rc_style_class_init (GtkRcStyleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  
  object_class->finalize = gtk_rc_style_finalize;

  klass->parse = NULL;
  klass->create_rc_style = gtk_rc_style_real_create_rc_style;
  klass->merge = gtk_rc_style_real_merge;
  klass->create_style = gtk_rc_style_real_create_style;

  g_type_class_add_private (object_class, sizeof (GtkRcStylePrivate));
}

static void
gtk_rc_style_finalize (GObject *object)
{
  GSList *tmp_list1, *tmp_list2;
  GtkRcStyle *rc_style;
  GtkRcStylePrivate *rc_priv;
  gint i;

  rc_style = GTK_RC_STYLE (object);
  rc_priv = GTK_RC_STYLE_GET_PRIVATE (rc_style);

  g_free (rc_style->name);
  if (rc_style->font_desc)
    pango_font_description_free (rc_style->font_desc);
      
  for (i = 0; i < 5; i++)
    g_free (rc_style->bg_pixmap_name[i]);
  
  /* Now remove all references to this rc_style from
   * realized_style_ht
   */
  tmp_list1 = rc_style->rc_style_lists;
  while (tmp_list1)
    {
      GSList *rc_styles = tmp_list1->data;
      GtkStyle *style = g_hash_table_lookup (realized_style_ht, rc_styles);
      g_object_unref (style);

      /* Remove the list of styles from the other rc_styles
       * in the list
       */
      tmp_list2 = rc_styles;
      while (tmp_list2)
        {
          GtkRcStyle *other_style = tmp_list2->data;

          if (other_style != rc_style)
            other_style->rc_style_lists = g_slist_remove_all (other_style->rc_style_lists,
							      rc_styles);
          tmp_list2 = tmp_list2->next;
        }

      /* And from the hash table itself
       */
      g_hash_table_remove (realized_style_ht, rc_styles);
      g_slist_free (rc_styles);

      tmp_list1 = tmp_list1->next;
    }
  g_slist_free (rc_style->rc_style_lists);

  if (rc_style->rc_properties)
    {
      guint i;

      for (i = 0; i < rc_style->rc_properties->len; i++)
	{
	  GtkRcProperty *node = &g_array_index (rc_style->rc_properties, GtkRcProperty, i);

	  g_free (node->origin);
	  g_value_unset (&node->value);
	}
      g_array_free (rc_style->rc_properties, TRUE);
      rc_style->rc_properties = NULL;
    }

  g_slist_foreach (rc_style->icon_factories, (GFunc) g_object_unref, NULL);
  g_slist_free (rc_style->icon_factories);

  g_slist_foreach (rc_priv->color_hashes, (GFunc) g_hash_table_unref, NULL);
  g_slist_free (rc_priv->color_hashes);

  G_OBJECT_CLASS (gtk_rc_style_parent_class)->finalize (object);
}

GtkRcStyle *
gtk_rc_style_new (void)
{
  GtkRcStyle *style;
  
  style = g_object_new (GTK_TYPE_RC_STYLE, NULL);
  
  return style;
}

/**
 * gtk_rc_style_copy:
 * @orig: the style to copy
 *
 * Makes a copy of the specified #GtkRcStyle. This function
 * will correctly copy an RC style that is a member of a class
 * derived from #GtkRcStyle.
 *
 * Return value: (transfer full): the resulting #GtkRcStyle
 **/
GtkRcStyle *
gtk_rc_style_copy (GtkRcStyle *orig)
{
  GtkRcStyle *style;

  g_return_val_if_fail (GTK_IS_RC_STYLE (orig), NULL);
  
  style = GTK_RC_STYLE_GET_CLASS (orig)->create_rc_style (orig);
  GTK_RC_STYLE_GET_CLASS (style)->merge (style, orig);

  gtk_rc_style_copy_icons_and_colors (style, orig, NULL);

  return style;
}

void
_gtk_rc_style_set_rc_property (GtkRcStyle *rc_style,
			       GtkRcProperty *property)
{
  g_return_if_fail (GTK_IS_RC_STYLE (rc_style));
  g_return_if_fail (property != NULL);

  insert_rc_property (rc_style, property, TRUE);
}

void
_gtk_rc_style_unset_rc_property (GtkRcStyle *rc_style,
				 GQuark      type_name,
				 GQuark      property_name)
{
  GtkRcProperty *node;

  g_return_if_fail (GTK_IS_RC_STYLE (rc_style));

  node = (GtkRcProperty *) _gtk_rc_style_lookup_rc_property (rc_style,
                                                             type_name,
                                                             property_name);

  if (node != NULL)
    {
      guint index = node - (GtkRcProperty *) rc_style->rc_properties->data;
      g_value_unset (&node->value);
      g_free (node->origin);
      g_array_remove_index (rc_style->rc_properties, index);
    }
}

void      
gtk_rc_style_ref (GtkRcStyle *rc_style)
{
  g_return_if_fail (GTK_IS_RC_STYLE (rc_style));

  g_object_ref (rc_style);
}

void      
gtk_rc_style_unref (GtkRcStyle *rc_style)
{
  g_return_if_fail (GTK_IS_RC_STYLE (rc_style));

  g_object_unref (rc_style);
}

static GtkRcStyle *
gtk_rc_style_real_create_rc_style (GtkRcStyle *style)
{
  return g_object_new (G_OBJECT_TYPE (style), NULL);
}

GSList *
_gtk_rc_style_get_color_hashes (GtkRcStyle *rc_style)
{
  GtkRcStylePrivate *priv = GTK_RC_STYLE_GET_PRIVATE (rc_style);

  return priv->color_hashes;
}

static gint
gtk_rc_properties_cmp (gconstpointer bsearch_node1,
		       gconstpointer bsearch_node2)
{
  const GtkRcProperty *prop1 = bsearch_node1;
  const GtkRcProperty *prop2 = bsearch_node2;

  if (prop1->type_name == prop2->type_name)
    return prop1->property_name < prop2->property_name ? -1 : prop1->property_name == prop2->property_name ? 0 : 1;
  else
    return prop1->type_name < prop2->type_name ? -1 : 1;
}

static void
insert_rc_property (GtkRcStyle    *style,
		    GtkRcProperty *property,
		    gboolean       replace)
{
  guint i;
  GtkRcProperty *new_property = NULL;
  GtkRcProperty key = { 0, 0, NULL, { 0, }, };

  key.type_name = property->type_name;
  key.property_name = property->property_name;

  if (!style->rc_properties)
    style->rc_properties = g_array_new (FALSE, FALSE, sizeof (GtkRcProperty));

  i = 0;
  while (i < style->rc_properties->len)
    {
      gint cmp = gtk_rc_properties_cmp (&key, &g_array_index (style->rc_properties, GtkRcProperty, i));

      if (cmp == 0)
	{
	  if (replace)
	    {
	      new_property = &g_array_index (style->rc_properties, GtkRcProperty, i);
	      
	      g_free (new_property->origin);
	      g_value_unset (&new_property->value);
	      
	      *new_property = key;
	      break;
	    }
	  else
	    return;
	}
      else if (cmp < 0)
	break;

      i++;
    }

  if (!new_property)
    {
      g_array_insert_val (style->rc_properties, i, key);
      new_property = &g_array_index (style->rc_properties, GtkRcProperty, i);
    }

  new_property->origin = g_strdup (property->origin);
  g_value_init (&new_property->value, G_VALUE_TYPE (&property->value));
  g_value_copy (&property->value, &new_property->value);
}

static void
gtk_rc_style_real_merge (GtkRcStyle *dest,
			 GtkRcStyle *src)
{
  gint i;

  for (i = 0; i < 5; i++)
    {
      if (!dest->bg_pixmap_name[i] && src->bg_pixmap_name[i])
	dest->bg_pixmap_name[i] = g_strdup (src->bg_pixmap_name[i]);
      
      if (!(dest->color_flags[i] & GTK_RC_FG) && 
	  src->color_flags[i] & GTK_RC_FG)
	{
	  dest->fg[i] = src->fg[i];
	  dest->color_flags[i] |= GTK_RC_FG;
	}
      if (!(dest->color_flags[i] & GTK_RC_BG) && 
	  src->color_flags[i] & GTK_RC_BG)
	{
	  dest->bg[i] = src->bg[i];
	  dest->color_flags[i] |= GTK_RC_BG;
	}
      if (!(dest->color_flags[i] & GTK_RC_TEXT) && 
	  src->color_flags[i] & GTK_RC_TEXT)
	{
	  dest->text[i] = src->text[i];
	  dest->color_flags[i] |= GTK_RC_TEXT;
	}
      if (!(dest->color_flags[i] & GTK_RC_BASE) && 
	  src->color_flags[i] & GTK_RC_BASE)
	{
	  dest->base[i] = src->base[i];
	  dest->color_flags[i] |= GTK_RC_BASE;
	}
    }

  if (dest->xthickness < 0 && src->xthickness >= 0)
    dest->xthickness = src->xthickness;
  if (dest->ythickness < 0 && src->ythickness >= 0)
    dest->ythickness = src->ythickness;

  if (src->font_desc)
    {
      if (!dest->font_desc)
	dest->font_desc = pango_font_description_copy (src->font_desc);
      else
	pango_font_description_merge (dest->font_desc, src->font_desc, FALSE);
    }

  if (src->rc_properties)
    {
      guint i;

      for (i = 0; i < src->rc_properties->len; i++)
	insert_rc_property (dest,
			    &g_array_index (src->rc_properties, GtkRcProperty, i),
			    FALSE);
    }
}

static GtkStyle *
gtk_rc_style_real_create_style (GtkRcStyle *rc_style)
{
  return gtk_style_new ();
}

static void
gtk_rc_style_prepend_empty_icon_factory (GtkRcStyle *rc_style)
{
  GtkIconFactory *factory = gtk_icon_factory_new ();

  rc_style->icon_factories = g_slist_prepend (rc_style->icon_factories, factory);
}

static void
gtk_rc_style_prepend_empty_color_hash (GtkRcStyle *rc_style)
{
  GtkRcStylePrivate *priv = GTK_RC_STYLE_GET_PRIVATE (rc_style);
  GHashTable        *hash = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                   g_free,
                                                   (GDestroyNotify) gdk_color_free);

  priv->color_hashes = g_slist_prepend (priv->color_hashes, hash);
}

static void
gtk_rc_style_append_icon_factories (GtkRcStyle *rc_style,
                                    GtkRcStyle *src_style)
{
  GSList *concat = g_slist_copy (src_style->icon_factories);

  g_slist_foreach (concat, (GFunc) g_object_ref, NULL);

  rc_style->icon_factories = g_slist_concat (rc_style->icon_factories, concat);
}

static void
gtk_rc_style_append_color_hashes (GtkRcStyle *rc_style,
                                  GtkRcStyle *src_style)
{
  GtkRcStylePrivate *priv     = GTK_RC_STYLE_GET_PRIVATE (rc_style);
  GtkRcStylePrivate *src_priv = GTK_RC_STYLE_GET_PRIVATE (src_style);
  GSList            *concat   = g_slist_copy (src_priv->color_hashes);

  g_slist_foreach (concat, (GFunc) g_hash_table_ref, NULL);

  priv->color_hashes = g_slist_concat (priv->color_hashes, concat);
}

static void
gtk_rc_style_copy_icons_and_colors (GtkRcStyle   *rc_style,
                                    GtkRcStyle   *src_style,
                                    GtkRcContext *context)
{
  GtkRcStylePrivate *priv = GTK_RC_STYLE_GET_PRIVATE (rc_style);

  if (src_style)
    {
      GtkRcStylePrivate *src_priv = GTK_RC_STYLE_GET_PRIVATE (src_style);

      /* Append src_style's factories, adding a ref to them */
      if (src_style->icon_factories != NULL)
        {
          /* Add a factory for ourselves if we have none,
           * in case we end up defining more stock icons.
           * I see no real way around this; we need to maintain
           * the invariant that the first factory in the list
           * is always our_factory, the one belonging to us,
           * and if we put src_style factories in the list we can't
           * do that if the style is reopened.
           */
          if (rc_style->icon_factories == NULL)
            gtk_rc_style_prepend_empty_icon_factory (rc_style);

          gtk_rc_style_append_icon_factories (rc_style, src_style);
        }

      /* Also append src_style's color hashes, adding a ref to them */
      if (src_priv->color_hashes != NULL)
        {
          /* See comment above .. */
          if (priv->color_hashes == NULL)
            gtk_rc_style_prepend_empty_color_hash (rc_style);

          gtk_rc_style_append_color_hashes (rc_style, src_style);
        }
    }

  /*  if we didn't get color hashes from the src_style, initialize
   *  the list with the settings' color scheme (if it exists)
   */
  if (priv->color_hashes == NULL && context && context->color_hash != NULL)
    {
      gtk_rc_style_prepend_empty_color_hash (rc_style);

      priv->color_hashes = g_slist_append (priv->color_hashes,
                                           g_hash_table_ref (context->color_hash));
    }
}

static void
gtk_rc_clear_hash_node (gpointer key, 
			gpointer data, 
			gpointer user_data)
{
  g_object_unref (data);
}

static void
gtk_rc_free_rc_sets (GSList *slist)
{
  while (slist)
    {
      GtkRcSet *rc_set;

      rc_set = slist->data;
      gtk_rc_set_free (rc_set);

      slist = slist->next;
    }
}

static void
gtk_rc_clear_styles (GtkRcContext *context)
{
  /* Clear out all old rc_styles */

  if (context->rc_style_ht)
    {
      g_hash_table_foreach (context->rc_style_ht, gtk_rc_clear_hash_node, NULL);
      g_hash_table_destroy (context->rc_style_ht);
      context->rc_style_ht = NULL;
    }

  gtk_rc_free_rc_sets (context->rc_sets_widget);
  g_slist_free (context->rc_sets_widget);
  context->rc_sets_widget = NULL;

  gtk_rc_free_rc_sets (context->rc_sets_widget_class);
  g_slist_free (context->rc_sets_widget_class);
  context->rc_sets_widget_class = NULL;

  gtk_rc_free_rc_sets (context->rc_sets_class);
  g_slist_free (context->rc_sets_class);
  context->rc_sets_class = NULL;
}

/* Reset all our widgets. Also, we have to invalidate cached icons in
 * icon sets so they get re-rendered.
 */
static void
gtk_rc_reset_widgets (GtkSettings *settings)
{
  GList *list, *toplevels;

  _gtk_icon_set_invalidate_caches ();
  
  toplevels = gtk_window_list_toplevels ();
  g_list_foreach (toplevels, (GFunc)g_object_ref, NULL);
  
  for (list = toplevels; list; list = list->next)
    {
      if (gtk_widget_get_screen (list->data) == settings->screen)
	{
	  gtk_widget_reset_rc_styles (list->data);
	}

      g_object_unref (list->data);
    }
  g_list_free (toplevels);
}

static void
gtk_rc_clear_realized_style (gpointer key,
			     gpointer value,
			     gpointer data)
{
  GSList *rc_styles = key;
  GtkStyle *style = value;
  GSList *tmp_list = rc_styles;

  g_object_unref (style);
 
  while (tmp_list)
    {
      GtkRcStyle *rc_style = tmp_list->data;
      
      rc_style->rc_style_lists = g_slist_remove_all (rc_style->rc_style_lists,
						     rc_styles);
      tmp_list = tmp_list->next;
    }

  g_slist_free (rc_styles);
}

/**
 * gtk_rc_reset_styles:
 * @settings: a #GtkSettings
 * 
 * This function recomputes the styles for all widgets that use a
 * particular #GtkSettings object. (There is one #GtkSettings object
 * per #GdkScreen, see gtk_settings_get_for_screen()); It is useful
 * when some global parameter has changed that affects the appearance
 * of all widgets, because when a widget gets a new style, it will
 * both redraw and recompute any cached information about its
 * appearance. As an example, it is used when the default font size
 * set by the operating system changes. Note that this function
 * doesn't affect widgets that have a style set explicitely on them
 * with gtk_widget_set_style().
 *
 * Since: 2.4
 **/
void
gtk_rc_reset_styles (GtkSettings *settings)
{
  GtkRcContext *context;
  gboolean reset = FALSE;

  g_return_if_fail (GTK_IS_SETTINGS (settings));

  context = gtk_rc_context_get (settings);
  
  if (context->default_style)
    {
      g_object_unref (context->default_style);
      context->default_style = NULL;
      reset = TRUE;
    }
  
  /* Clear out styles that have been looked up already
   */
  if (realized_style_ht)
    {
      g_hash_table_foreach (realized_style_ht, gtk_rc_clear_realized_style, NULL);
      g_hash_table_destroy (realized_style_ht);
      realized_style_ht = NULL;
      reset = TRUE;
    }
  
  if (reset)
    gtk_rc_reset_widgets (settings);
}

const gchar*
_gtk_rc_context_get_default_font_name (GtkSettings *settings)
{
  GtkRcContext *context;
  gchar *new_font_name;
  
  g_return_val_if_fail (GTK_IS_SETTINGS (settings), NULL);

  context = gtk_rc_context_get (settings);

  g_object_get (context->settings,
                "gtk-font-name", &new_font_name,
                NULL);

  if (new_font_name != context->font_name && !(new_font_name && strcmp (context->font_name, new_font_name) == 0))
    {
       g_free (context->font_name);
       context->font_name = g_strdup (new_font_name);
 
       gtk_rc_reset_styles (settings);
    }
          
  g_free (new_font_name);

  return context->font_name;
}

/**
 * gtk_rc_reparse_all_for_settings:
 * @settings: a #GtkSettings
 * @force_load: load whether or not anything changed
 * 
 * If the modification time on any previously read file
 * for the given #GtkSettings has changed, discard all style information
 * and then reread all previously read RC files.
 * 
 * Return value: %TRUE if the files were reread.
 **/
gboolean
gtk_rc_reparse_all_for_settings (GtkSettings *settings,
				 gboolean     force_load)
{
  gboolean mtime_modified = FALSE;
  GtkRcFile *rc_file;
  GSList *tmp_list;
  GtkRcContext *context;
  GStatBuf statbuf;

  g_return_val_if_fail (GTK_IS_SETTINGS (settings), FALSE);

  context = gtk_rc_context_get (settings);

  if (context->reloading)
    return FALSE;

  if (!force_load)
    {
      /* Check through and see if any of the RC's have had their
       * mtime modified. If so, reparse everything.
       */
      tmp_list = context->rc_files;
      while (tmp_list)
	{
	  rc_file = tmp_list->data;

	  if (!rc_file->is_string)
	    {
	      if (!g_lstat (rc_file->name, &statbuf) && 
		  (statbuf.st_mtime != rc_file->mtime))
		{
		  mtime_modified = TRUE;
		  break;
		}
	    }
	  
	  tmp_list = tmp_list->next;
	}
    }
      
  if (force_load || mtime_modified)
    {
      _gtk_binding_reset_parsed ();
      gtk_rc_clear_styles (context);
      context->reloading = TRUE;

      _gtk_settings_reset_rc_values (context->settings);
      gtk_rc_clear_rc_files (context);

      gtk_rc_parse_default_files (context);

      tmp_list = global_rc_files;
      while (tmp_list)
	{
	  rc_file = tmp_list->data;

	  if (rc_file->is_string)
	    gtk_rc_context_parse_string (context, rc_file->name);
	  else
	    gtk_rc_context_parse_file (context, rc_file->name, GTK_PATH_PRIO_RC, FALSE);

	  tmp_list = tmp_list->next;
	}

      g_free (context->theme_name);
      g_free (context->key_theme_name);

      g_object_get (context->settings,
		    "gtk-theme-name", &context->theme_name,
		    "gtk-key-theme-name", &context->key_theme_name,
		    NULL);

      if (context->theme_name && context->theme_name[0])
	gtk_rc_parse_named (context, context->theme_name, NULL);
      if (context->key_theme_name && context->key_theme_name[0])
	gtk_rc_parse_named (context, context->key_theme_name, "key");

      context->reloading = FALSE;

      gtk_rc_reset_widgets (context->settings);
    }

  return force_load || mtime_modified;
}

/**
 * gtk_rc_reparse_all:
 * 
 * If the modification time on any previously read file for the
 * default #GtkSettings has changed, discard all style information
 * and then reread all previously read RC files.
 * 
 * Return value:  %TRUE if the files were reread.
 **/
gboolean
gtk_rc_reparse_all (void)
{
  GSList *tmp_list;
  gboolean result = FALSE;

  for (tmp_list = rc_contexts; tmp_list; tmp_list = tmp_list->next)
    {
      GtkRcContext *context = tmp_list->data;
      if (gtk_rc_reparse_all_for_settings (context->settings, FALSE))
	result = TRUE;
    }

  return result;
}

static GSList *
gtk_rc_styles_match (GSList       *rc_styles,
		     GSList	  *sets,
		     guint         path_length,
		     gchar        *path,
		     gchar        *path_reversed)
		     
{
  GtkRcSet *rc_set;

  while (sets)
    {
      rc_set = sets->data;
      sets = sets->next;

      if (rc_set->type == GTK_PATH_WIDGET_CLASS)
        {
          if (_gtk_rc_match_widget_class (rc_set->path, path_length, path, path_reversed))
	    rc_styles = g_slist_append (rc_styles, rc_set);
        }
      else
        {
          if (g_pattern_match (rc_set->pspec, path_length, path, path_reversed))
	    rc_styles = g_slist_append (rc_styles, rc_set);
	}
    }

  return rc_styles;
}

static gint
rc_set_compare (gconstpointer a, gconstpointer b)
{
  const GtkRcSet *set_a = a;
  const GtkRcSet *set_b = b;

  return (set_a->priority < set_b->priority) ? 1 : (set_a->priority == set_b->priority ? 0 : -1);
}

static GSList *
sort_and_dereference_sets (GSList *styles)
{
  GSList *tmp_list;
  
  /* At this point, the list of sets is ordered by:
   *
   * a) 'widget' patterns are earlier than 'widget_class' patterns
   *    which are ealier than 'class' patterns.
   * a) For two matches for class patterns, a match to a child type
   *    is before a match to a parent type
   * c) a match later in the RC file (or in a later RC file) is before a
   *    match earlier in the RC file.
   *
   * With a) taking precedence over b) which takes precendence over c).
   *
   * Now sort by priority, which has the highest precendence for sort order
   */
  styles = g_slist_sort (styles, rc_set_compare);

  /* Make styles->data = styles->data->rc_style
   */
  tmp_list = styles;
  while (tmp_list)
    {
      GtkRcSet *set = tmp_list->data;
      tmp_list->data = set->rc_style;
      tmp_list = tmp_list->next;
    }

  return styles;
}

/**
 * gtk_rc_get_style:
 * @widget: a #GtkWidget
 * 
 * Finds all matching RC styles for a given widget,
 * composites them together, and then creates a 
 * #GtkStyle representing the composite appearance.
 * (GTK+ actually keeps a cache of previously 
 * created styles, so a new style may not be
 * created.)
 * 
 * Returns: (transfer none): the resulting style. No refcount is added
 *   to the returned style, so if you want to save this
 *   style around, you should add a reference yourself.
 **/
GtkStyle *
gtk_rc_get_style (GtkWidget *widget)
{
  GtkRcStyle *widget_rc_style;
  GSList *rc_styles = NULL;
  GtkRcContext *context;

  static guint rc_style_key_id = 0;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  context = gtk_rc_context_get (gtk_widget_get_settings (widget));

  /* We allow the specification of a single rc style to be bound
   * tightly to a widget, for application modifications
   */
  if (!rc_style_key_id)
    rc_style_key_id = g_quark_from_static_string ("gtk-rc-style");

  if (context->rc_sets_widget)
    {
      gchar *path, *path_reversed;
      guint path_length;

      gtk_widget_path (widget, &path_length, &path, &path_reversed);
      rc_styles = gtk_rc_styles_match (rc_styles, context->rc_sets_widget, path_length, path, path_reversed);
      g_free (path);
      g_free (path_reversed);
    }
  
  if (context->rc_sets_widget_class)
    {
      gchar *path, *path_reversed;
      guint path_length;

      gtk_widget_class_path (widget, &path_length, &path, &path_reversed);
      rc_styles = gtk_rc_styles_match (rc_styles, context->rc_sets_widget_class, path_length, path, path_reversed);
      g_free (path);
      g_free (path_reversed);
    }

  if (context->rc_sets_class)
    {
      GType type;

      type = G_TYPE_FROM_INSTANCE (widget);
      while (type)
	{
	  gchar *path;
          gchar *path_reversed;
	  guint path_length;

	  path = g_strdup (g_type_name (type));
	  path_length = strlen (path);
	  path_reversed = g_strdup (path);
	  g_strreverse (path_reversed);
	  
	  rc_styles = gtk_rc_styles_match (rc_styles, context->rc_sets_class, path_length, path, path_reversed);
	  g_free (path);
	  g_free (path_reversed);
      
	  type = g_type_parent (type);
	}
    }
  
  rc_styles = sort_and_dereference_sets (rc_styles);
  
  widget_rc_style = g_object_get_qdata (G_OBJECT (widget), rc_style_key_id);

  if (widget_rc_style)
    rc_styles = g_slist_prepend (rc_styles, widget_rc_style);

  if (rc_styles)
    return gtk_rc_init_style (context, rc_styles);
  else
    {
      if (!context->default_style)
	{
	  GtkStyle * style = gtk_style_new ();
	  _gtk_style_init_for_settings (style, context->settings);

	  /* Only after _gtk_style_init_for_settings() do we install the style
	   * as the default, otherwise gtk_rc_reset_styles() can be called and
	   * unref the style while initializing it, causing a segfault.
	   */
	  context->default_style = style;
	}

      return context->default_style;
    }
}

/**
 * gtk_rc_get_style_by_paths:
 * @settings: a #GtkSettings object
 * @widget_path: (allow-none): the widget path to use when looking up the
 *     style, or %NULL if no matching against the widget path should be done
 * @class_path: (allow-none): the class path to use when looking up the style,
 *     or %NULL if no matching against the class path should be done.
 * @type: a type that will be used along with parent types of this type
 *     when matching against class styles, or #G_TYPE_NONE
 *
 * Creates up a #GtkStyle from styles defined in a RC file by providing
 * the raw components used in matching. This function may be useful
 * when creating pseudo-widgets that should be themed like widgets but
 * don't actually have corresponding GTK+ widgets. An example of this
 * would be items inside a GNOME canvas widget.
 *
 * The action of gtk_rc_get_style() is similar to:
 * |[
 *  gtk_widget_path (widget, NULL, &path, NULL);
 *  gtk_widget_class_path (widget, NULL, &class_path, NULL);
 *  gtk_rc_get_style_by_paths (gtk_widget_get_settings (widget), 
 *                             path, class_path,
 *                             G_OBJECT_TYPE (widget));
 * ]|
 * 
 * Return value: (transfer none): A style created by matching with the
 *     supplied paths, or %NULL if nothing matching was specified and the
 *     default style should be used. The returned value is owned by GTK+
 *     as part of an internal cache, so you must call g_object_ref() on
 *     the returned value if you want to keep a reference to it.
 **/
GtkStyle *
gtk_rc_get_style_by_paths (GtkSettings *settings,
			   const char  *widget_path,
			   const char  *class_path,
			   GType        type)
{
  /* We duplicate the code from above to avoid slowing down the above
   * by generating paths when we don't need them. I don't know if
   * this is really worth it.
   */
  GSList *rc_styles = NULL;
  GtkRcContext *context;

  g_return_val_if_fail (GTK_IS_SETTINGS (settings), NULL);

  context = gtk_rc_context_get (settings);

  if (widget_path && context->rc_sets_widget)
    {
      gchar *path;
      gchar *path_reversed;
      guint path_length;

      path_length = strlen (widget_path);
      path = g_strdup (widget_path);
      path_reversed = g_strdup (widget_path);
      g_strreverse (path_reversed);

      rc_styles = gtk_rc_styles_match (rc_styles, context->rc_sets_widget, path_length, path, path_reversed);
      g_free (path);
      g_free (path_reversed);
    }
  
  if (class_path && context->rc_sets_widget_class)
    {
      gchar *path;
      gchar *path_reversed;
      guint path_length;

      path = g_strdup (class_path);
      path_length = strlen (class_path);
      path_reversed = g_strdup (class_path);
      g_strreverse (path_reversed);

      rc_styles = gtk_rc_styles_match (rc_styles, context->rc_sets_widget_class, path_length, path, path_reversed);
      g_free (path);
      g_free (path_reversed);
    }

  if (type != G_TYPE_NONE && context->rc_sets_class)
    {
      while (type)
	{
	  gchar *path;
          gchar *path_reversed;
	  guint path_length;

	  path = g_strdup (g_type_name (type));
	  path_length = strlen (path);
	  path_reversed = g_strdup (path);
	  g_strreverse (path_reversed);
	  
	  rc_styles = gtk_rc_styles_match (rc_styles, context->rc_sets_class, path_length, path, path_reversed);
	  g_free (path);
	  g_free (path_reversed);
      
	  type = g_type_parent (type);
	}
    }
 
  rc_styles = sort_and_dereference_sets (rc_styles);
  
  if (rc_styles)
    return gtk_rc_init_style (context, rc_styles);

  return NULL;
}

static GSList *
gtk_rc_add_rc_sets (GSList      *slist,
		    GtkRcStyle  *rc_style,
		    const gchar *pattern,
		    GtkPathType  path_type)
{
  GtkRcStyle *new_style;
  GtkRcSet *rc_set;
  guint i;
  
  new_style = gtk_rc_style_new ();
  *new_style = *rc_style;
  new_style->name = g_strdup (rc_style->name);
  if (rc_style->font_desc)
    new_style->font_desc = pango_font_description_copy (rc_style->font_desc);
  
  for (i = 0; i < 5; i++)
    new_style->bg_pixmap_name[i] = g_strdup (rc_style->bg_pixmap_name[i]);
  
  rc_set = g_new (GtkRcSet, 1);
  rc_set->type = path_type;
  
  if (path_type == GTK_PATH_WIDGET_CLASS)
    {
      rc_set->pspec = NULL;
      rc_set->path = _gtk_rc_parse_widget_class_path (pattern);
    }
  else
    {
      rc_set->pspec = g_pattern_spec_new (pattern);
      rc_set->path = NULL;
    }
  
  rc_set->rc_style = rc_style;
  
  return g_slist_prepend (slist, rc_set);
}

void
gtk_rc_add_widget_name_style (GtkRcStyle  *rc_style,
			      const gchar *pattern)
{
  GtkRcContext *context;
  
  g_return_if_fail (rc_style != NULL);
  g_return_if_fail (pattern != NULL);

  context = gtk_rc_context_get (gtk_settings_get_default ());
  
  context->rc_sets_widget = gtk_rc_add_rc_sets (context->rc_sets_widget, rc_style, pattern, GTK_PATH_WIDGET);
}

void
gtk_rc_add_widget_class_style (GtkRcStyle  *rc_style,
			       const gchar *pattern)
{
  GtkRcContext *context;
  
  g_return_if_fail (rc_style != NULL);
  g_return_if_fail (pattern != NULL);

  context = gtk_rc_context_get (gtk_settings_get_default ());
  
  context->rc_sets_widget_class = gtk_rc_add_rc_sets (context->rc_sets_widget_class, rc_style, pattern, GTK_PATH_WIDGET_CLASS);
}

void
gtk_rc_add_class_style (GtkRcStyle  *rc_style,
			const gchar *pattern)
{
  GtkRcContext *context;
  
  g_return_if_fail (rc_style != NULL);
  g_return_if_fail (pattern != NULL);

  context = gtk_rc_context_get (gtk_settings_get_default ());
  
  context->rc_sets_class = gtk_rc_add_rc_sets (context->rc_sets_class, rc_style, pattern, GTK_PATH_CLASS);
}

GScanner*
gtk_rc_scanner_new (void)
{
  return g_scanner_new (&gtk_rc_scanner_config);
}

static void
gtk_rc_parse_any (GtkRcContext *context,
		  const gchar  *input_name,
		  gint		input_fd,
		  const gchar  *input_string)
{
  GScanner *scanner;
  guint	   i;
  gboolean done;

  scanner = gtk_rc_scanner_new ();
  
  if (input_fd >= 0)
    {
      g_assert (input_string == NULL);
      
      g_scanner_input_file (scanner, input_fd);
    }
  else
    {
      g_assert (input_string != NULL);
      
      g_scanner_input_text (scanner, input_string, strlen (input_string));
    }
  scanner->input_name = input_name;

  for (i = 0; i < G_N_ELEMENTS (symbols); i++)
    g_scanner_scope_add_symbol (scanner, 0, symbol_names + symbols[i].name_offset, GINT_TO_POINTER (symbols[i].token));
  done = FALSE;
  while (!done)
    {
      if (g_scanner_peek_next_token (scanner) == G_TOKEN_EOF)
	done = TRUE;
      else
	{
	  guint expected_token;
	  
	  expected_token = gtk_rc_parse_statement (context, scanner);

	  if (expected_token != G_TOKEN_NONE)
	    {
	      const gchar *symbol_name = NULL;
	      gchar *msg = NULL;

	      if (scanner->scope_id == 0)
		{
		  /* if we are in scope 0, we know the symbol names
		   * that are associated with certain token values.
		   * so we look them up to make the error messages
		   * more readable.
		   */
		  if (expected_token > GTK_RC_TOKEN_INVALID &&
		      expected_token < GTK_RC_TOKEN_LAST)
		    {
                      const gchar *sym = NULL;

		      for (i = 0; i < G_N_ELEMENTS (symbols); i++)
			if (symbols[i].token == expected_token)
			  sym = symbol_names + symbols[i].name_offset;

		      if (sym)
			msg = g_strconcat ("e.g. `", sym, "'", NULL);
		    }

		  if (scanner->token > (guint) GTK_RC_TOKEN_INVALID &&
		      scanner->token < (guint) GTK_RC_TOKEN_LAST)
		    {
		      symbol_name = "???";
		      for (i = 0; i < G_N_ELEMENTS (symbols); i++)
			if (symbols[i].token == scanner->token)
			  symbol_name = symbol_names + symbols[i].name_offset;
		    }
		}

	      g_scanner_unexp_token (scanner,
				     expected_token,
				     NULL,
				     "keyword",
				     symbol_name,
				     msg,
				     TRUE);
	      g_free (msg);
	      done = TRUE;
	    }
	}
    }
  
  g_scanner_destroy (scanner);
}

static guint	   
gtk_rc_styles_hash (const GSList *rc_styles)
{
  guint result;
  
  result = 0;
  while (rc_styles)
    {
      result += (result << 9) + GPOINTER_TO_UINT (rc_styles->data);
      rc_styles = rc_styles->next;
    }
  
  return result;
}

static gboolean
gtk_rc_styles_equal (const GSList *a,
		     const GSList *b)
{
  while (a && b)
    {
      if (a->data != b->data)
	return FALSE;
      a = a->next;
      b = b->next;
    }
  
  return (a == b);
}

static guint
gtk_rc_style_hash (const gchar *name)
{
  guint result;
  
  result = 0;
  while (*name)
    result += (result << 3) + *name++;
  
  return result;
}

static gboolean
gtk_rc_style_equal (const gchar *a,
		    const gchar *b)
{
  return (strcmp (a, b) == 0);
}

static GtkRcStyle*
gtk_rc_style_find (GtkRcContext *context,
		   const gchar  *name)
{
  if (context->rc_style_ht)
    return g_hash_table_lookup (context->rc_style_ht, (gpointer) name);
  else
    return NULL;
}

static GtkStyle *
gtk_rc_style_to_style (GtkRcContext *context,
		       GtkRcStyle   *rc_style)
{
  GtkStyle *style;

  style = GTK_RC_STYLE_GET_CLASS (rc_style)->create_style (rc_style);
  _gtk_style_init_for_settings (style, context->settings);

  style->rc_style = g_object_ref (rc_style);
  
  GTK_STYLE_GET_CLASS (style)->init_from_rc (style, rc_style);  

  return style;
}

/* Reuses or frees rc_styles */
static GtkStyle *
gtk_rc_init_style (GtkRcContext *context,
		   GSList       *rc_styles)
{
  GtkStyle *style = NULL;
  gint i;

  g_return_val_if_fail (rc_styles != NULL, NULL);
  
  if (!realized_style_ht)
    realized_style_ht = g_hash_table_new ((GHashFunc) gtk_rc_styles_hash,
 (GEqualFunc) gtk_rc_styles_equal);

  style = g_hash_table_lookup (realized_style_ht, rc_styles);

  if (!style)
    {
      GtkRcStyle *base_style = NULL;
      GtkRcStyle *proto_style;
      GtkRcStyleClass *proto_style_class;
      GSList *tmp_styles;
      GType rc_style_type = GTK_TYPE_RC_STYLE;

      /* Find the first style where the RC file specified engine "" {}
       * or the first derived style and use that to create the
       * merged style. If we only have raw GtkRcStyles, use the first
       * style to create the merged style.
       */
      base_style = rc_styles->data;
      tmp_styles = rc_styles;
      while (tmp_styles)
	{
	  GtkRcStyle *rc_style = tmp_styles->data;
          
	  if (rc_style->engine_specified ||
	      G_OBJECT_TYPE (rc_style) != rc_style_type)
	    {
	      base_style = rc_style;
	      break;
	    }
          
	  tmp_styles = tmp_styles->next;
	}
      
      proto_style_class = GTK_RC_STYLE_GET_CLASS (base_style);
      proto_style = proto_style_class->create_rc_style (base_style);

      tmp_styles = rc_styles;
      while (tmp_styles)
	{
	  GtkRcStyle *rc_style = tmp_styles->data;

	  proto_style_class->merge (proto_style, rc_style);	  
          
	  /* Point from each rc_style to the list of styles */
	  if (!g_slist_find (rc_style->rc_style_lists, rc_styles))
	    rc_style->rc_style_lists = g_slist_prepend (rc_style->rc_style_lists, rc_styles);

          gtk_rc_style_append_icon_factories (proto_style, rc_style);
          gtk_rc_style_append_color_hashes (proto_style, rc_style);

	  tmp_styles = tmp_styles->next;
	}

      for (i = 0; i < 5; i++)
	if (proto_style->bg_pixmap_name[i] &&
	    (strcmp (proto_style->bg_pixmap_name[i], "<none>") == 0))
	  {
	    g_free (proto_style->bg_pixmap_name[i]);
	    proto_style->bg_pixmap_name[i] = NULL;
	  }

      style = gtk_rc_style_to_style (context, proto_style);
      g_object_unref (proto_style);

      g_hash_table_insert (realized_style_ht, rc_styles, style);
    }
  else
    g_slist_free (rc_styles);

  return style;
}

/*********************
 * Parsing functions *
 *********************/

static gboolean
lookup_color (GtkRcStyle *style,
              const char *color_name,
              GdkColor   *color)
{
  GtkRcStylePrivate *priv = GTK_RC_STYLE_GET_PRIVATE (style);
  GSList *iter;

  for (iter = priv->color_hashes; iter != NULL; iter = iter->next)
    {
      GHashTable *hash  = iter->data;
      GdkColor   *match = g_hash_table_lookup (hash, color_name);

      if (match)
        {
          color->red = match->red;
          color->green = match->green;
          color->blue = match->blue;
          return TRUE;
        }
    }

  return FALSE;
}

static guint
rc_parse_token_or_compound (GScanner   *scanner,
                            GtkRcStyle *style,
			    GString    *gstring,
			    GTokenType  delimiter)
{
  guint token = g_scanner_get_next_token (scanner);

  /* we either scan a single token (skipping comments)
   * or a compund statement.
   * compunds are enclosed in (), [] or {} braces, we read
   * them in via deep recursion.
   */

  switch (token)
    {
      gchar *string;
    case G_TOKEN_INT:
      g_string_append_printf (gstring, " 0x%lx", scanner->value.v_int);
      break;
    case G_TOKEN_FLOAT:
      {
	gchar    fbuf[G_ASCII_DTOSTR_BUF_SIZE];
	g_ascii_formatd (fbuf,
			 sizeof (fbuf),
			 "%f",
			 scanner->value.v_float);
	g_string_append_printf (gstring, " %s", (char*)fbuf);
      }
      break;
    case G_TOKEN_STRING:
      string = g_strescape (scanner->value.v_string, NULL);
      g_string_append (gstring, " \"");
      g_string_append (gstring, string);
      g_string_append_c (gstring, '"');
      g_free (string);
      break;
    case G_TOKEN_IDENTIFIER:
      g_string_append_c (gstring, ' ');
      g_string_append (gstring, scanner->value.v_identifier);
      break;
    case G_TOKEN_COMMENT_SINGLE:
    case G_TOKEN_COMMENT_MULTI:
      return rc_parse_token_or_compound (scanner, style, gstring, delimiter);
    case G_TOKEN_LEFT_PAREN:
      g_string_append_c (gstring, ' ');
      g_string_append_c (gstring, token);
      token = rc_parse_token_or_compound (scanner, style, gstring, G_TOKEN_RIGHT_PAREN);
      if (token != G_TOKEN_NONE)
	return token;
      break;
    case G_TOKEN_LEFT_CURLY:
      g_string_append_c (gstring, ' ');
      g_string_append_c (gstring, token);
      token = rc_parse_token_or_compound (scanner, style, gstring, G_TOKEN_RIGHT_CURLY);
      if (token != G_TOKEN_NONE)
	return token;
      break;
    case G_TOKEN_LEFT_BRACE:
      g_string_append_c (gstring, ' ');
      g_string_append_c (gstring, token);
      token = rc_parse_token_or_compound (scanner, style, gstring, G_TOKEN_RIGHT_BRACE);
      if (token != G_TOKEN_NONE)
	return token;
      break;
    case '@':
      if (g_scanner_peek_next_token (scanner) == G_TOKEN_IDENTIFIER)
        {
          GdkColor color;
          gchar    rbuf[G_ASCII_DTOSTR_BUF_SIZE];
          gchar    gbuf[G_ASCII_DTOSTR_BUF_SIZE];
          gchar    bbuf[G_ASCII_DTOSTR_BUF_SIZE];

          g_scanner_get_next_token (scanner);

          if (!style || !lookup_color (style, scanner->value.v_identifier,
                                       &color))
            {
              g_scanner_warn (scanner, "Invalid symbolic color '%s'",
                              scanner->value.v_identifier);
              return G_TOKEN_IDENTIFIER;
            }


          g_string_append_printf (gstring, " { %s, %s, %s }",
                                  g_ascii_formatd (rbuf, sizeof (rbuf),
                                                   "%0.4f",
                                                   color.red / 65535.0),
                                  g_ascii_formatd (gbuf, sizeof (gbuf),
                                                   "%0.4f",
                                                   color.green / 65535.0),
                                  g_ascii_formatd (bbuf, sizeof (bbuf),
                                                   "%0.4f",
                                                   color.blue / 65535.0));
          break;
        }
      else
        return G_TOKEN_IDENTIFIER;
    default:
      if (token >= 256 || token < 1)
	return delimiter ? delimiter : G_TOKEN_STRING;
      g_string_append_c (gstring, ' ');
      g_string_append_c (gstring, token);
      if (token == delimiter)
	return G_TOKEN_NONE;
      break;
    }
  if (!delimiter)
    return G_TOKEN_NONE;
  else
    return rc_parse_token_or_compound (scanner, style, gstring, delimiter);
}

static guint
gtk_rc_parse_assignment (GScanner      *scanner,
                         GtkRcStyle    *style,
			 GtkRcProperty *prop)
{
#define MY_SCAN_IDENTIFIER      TRUE
#define MY_SCAN_SYMBOLS         FALSE
#define MY_IDENTIFIER_2_STRING  FALSE
#define MY_CHAR_2_TOKEN         TRUE
#define MY_SCAN_IDENTIFIER_NULL FALSE
#define MY_NUMBERS_2_INT        TRUE

  gboolean scan_identifier      = scanner->config->scan_identifier;
  gboolean scan_symbols         = scanner->config->scan_symbols;
  gboolean identifier_2_string  = scanner->config->identifier_2_string;
  gboolean char_2_token         = scanner->config->char_2_token;
  gboolean scan_identifier_NULL = scanner->config->scan_identifier_NULL;
  gboolean numbers_2_int        = scanner->config->numbers_2_int;
  gboolean negate = FALSE;
  gboolean is_color = FALSE;
  guint    token;

  /* check that this is an assignment */
  if (g_scanner_get_next_token (scanner) != '=')
    return '=';

  /* adjust scanner mode */
  scanner->config->scan_identifier      = MY_SCAN_IDENTIFIER;
  scanner->config->scan_symbols         = MY_SCAN_SYMBOLS;
  scanner->config->identifier_2_string  = MY_IDENTIFIER_2_STRING;
  scanner->config->char_2_token         = MY_CHAR_2_TOKEN;
  scanner->config->scan_identifier_NULL = MY_SCAN_IDENTIFIER_NULL;
  scanner->config->numbers_2_int        = MY_NUMBERS_2_INT;

  /* record location */
  if (g_getenv ("GTK_DEBUG"))
    prop->origin = g_strdup_printf ("%s:%u", scanner->input_name, scanner->line);
  else
    prop->origin = NULL;

  /* parse optional symbolic color prefix */
  if (g_scanner_peek_next_token (scanner) == '@')
    {
      g_scanner_get_next_token (scanner); /* eat color prefix */
      is_color = TRUE;
    }

  /* parse optional sign */
  if (!is_color && g_scanner_peek_next_token (scanner) == '-')
    {
      g_scanner_get_next_token (scanner); /* eat sign */
      negate = TRUE;
    }

  /* parse one of LONG, DOUBLE and STRING or, if that fails, create an
   * unparsed compund
   */
  token = g_scanner_peek_next_token (scanner);

  if (is_color && token != G_TOKEN_IDENTIFIER)
    {
      token = G_TOKEN_IDENTIFIER;
      goto out;
    }

  switch (token)
    {
    case G_TOKEN_INT:
      g_scanner_get_next_token (scanner);
      g_value_init (&prop->value, G_TYPE_LONG);
      g_value_set_long (&prop->value, negate ? -scanner->value.v_int : scanner->value.v_int);
      token = G_TOKEN_NONE;
      break;
    case G_TOKEN_FLOAT:
      g_scanner_get_next_token (scanner);
      g_value_init (&prop->value, G_TYPE_DOUBLE);
      g_value_set_double (&prop->value, negate ? -scanner->value.v_float : scanner->value.v_float);
      token = G_TOKEN_NONE;
      break;
    case G_TOKEN_STRING:
      g_scanner_get_next_token (scanner);
      if (negate)
	token = G_TOKEN_INT;
      else
	{
	  g_value_init (&prop->value, G_TYPE_STRING);
	  g_value_set_string (&prop->value, scanner->value.v_string);
	  token = G_TOKEN_NONE;
	}
      break;
    case G_TOKEN_IDENTIFIER:
      if (is_color)
        {
          GdkColor  color;
          gchar     rbuf[G_ASCII_DTOSTR_BUF_SIZE];
          gchar     gbuf[G_ASCII_DTOSTR_BUF_SIZE];
          gchar     bbuf[G_ASCII_DTOSTR_BUF_SIZE];
          GString  *gstring;

          g_scanner_get_next_token (scanner);

          if (!style || !lookup_color (style, scanner->value.v_identifier,
                                       &color))
            {
              g_scanner_warn (scanner, "Invalid symbolic color '%s'",
                              scanner->value.v_identifier);
              token = G_TOKEN_IDENTIFIER;
              break;
            }

          gstring = g_string_new (NULL);

          g_string_append_printf (gstring, " { %s, %s, %s }",
                                  g_ascii_formatd (rbuf, sizeof (rbuf),
                                                   "%0.4f",
                                                   color.red / 65535.0),
                                  g_ascii_formatd (gbuf, sizeof (gbuf),
                                                   "%0.4f",
                                                   color.green / 65535.0),
                                  g_ascii_formatd (bbuf, sizeof (bbuf),
                                                   "%0.4f",
                                                   color.blue / 65535.0));

          g_value_init (&prop->value, G_TYPE_GSTRING);
          g_value_take_boxed (&prop->value, gstring);
          token = G_TOKEN_NONE;
          break;
        }
      /* fall through */
    case G_TOKEN_LEFT_PAREN:
    case G_TOKEN_LEFT_CURLY:
    case G_TOKEN_LEFT_BRACE:
      if (!negate)
	{
          GString  *gstring  = g_string_new (NULL);
          gboolean  parse_on = TRUE;

          /*  allow identifier(foobar) to support color expressions  */
          if (token == G_TOKEN_IDENTIFIER)
            {
              g_scanner_get_next_token (scanner);

              g_string_append_c (gstring, ' ');
              g_string_append (gstring, scanner->value.v_identifier);

              /* temporarily reset scanner mode to default, so we
               * don't peek the next token in a mode that only makes
               * sense in this function; because if anything but
               * G_TOKEN_LEFT_PAREN follows, the next token will be
               * parsed by our caller.
               *
               * FIXME: right fix would be to call g_scanner_unget()
               *        but that doesn't exist
               */
              scanner->config->scan_identifier      = scan_identifier;
              scanner->config->scan_symbols         = scan_symbols;
              scanner->config->identifier_2_string  = identifier_2_string;
              scanner->config->char_2_token         = char_2_token;
              scanner->config->scan_identifier_NULL = scan_identifier_NULL;
              scanner->config->numbers_2_int        = numbers_2_int;

              token = g_scanner_peek_next_token (scanner);

              /* restore adjusted scanner mode */
              scanner->config->scan_identifier      = MY_SCAN_IDENTIFIER;
              scanner->config->scan_symbols         = MY_SCAN_SYMBOLS;
              scanner->config->identifier_2_string  = MY_IDENTIFIER_2_STRING;
              scanner->config->char_2_token         = MY_CHAR_2_TOKEN;
              scanner->config->scan_identifier_NULL = MY_SCAN_IDENTIFIER_NULL;
              scanner->config->numbers_2_int        = MY_NUMBERS_2_INT;

              if (token != G_TOKEN_LEFT_PAREN)
                {
                  token = G_TOKEN_NONE;
                  parse_on = FALSE;
                }
            }

          if (parse_on)
            token = rc_parse_token_or_compound (scanner, style, gstring, 0);

	  if (token == G_TOKEN_NONE)
	    {
	      g_string_append_c (gstring, ' ');
	      g_value_init (&prop->value, G_TYPE_GSTRING);
	      g_value_take_boxed (&prop->value, gstring);
	    }
	  else
	    g_string_free (gstring, TRUE);
	  break;
	}
      /* fall through */
    default:
      g_scanner_get_next_token (scanner);
      token = G_TOKEN_INT;
      break;
    }

 out:

  /* restore scanner mode */
  scanner->config->scan_identifier      = scan_identifier;
  scanner->config->scan_symbols         = scan_symbols;
  scanner->config->identifier_2_string  = identifier_2_string;
  scanner->config->char_2_token         = char_2_token;
  scanner->config->scan_identifier_NULL = scan_identifier_NULL;
  scanner->config->numbers_2_int        = numbers_2_int;

  return token;
}

static gboolean
is_c_identifier (const gchar *string)
{
  const gchar *p;
  gboolean is_varname;

  is_varname = strchr ("_" G_CSET_a_2_z G_CSET_A_2_Z, string[0]) != NULL;
  for (p = string + 1; *p && is_varname; p++)
    is_varname &= strchr (G_CSET_DIGITS "-_" G_CSET_a_2_z G_CSET_A_2_Z, *p) != NULL;

  return is_varname;
}

static void
parse_include_file (GtkRcContext *context,
		    GScanner     *scanner,
		    const gchar  *filename)
{
  char *to_parse = NULL;
  
  if (g_path_is_absolute (filename))
    {
      /* For abolute paths, we call gtk_rc_context_parse_file unconditionally. We
       * don't print an error in this case.
       */
      to_parse = g_strdup (filename);
    }
  else
    {
      /* if a relative path, we look relative to all the RC files in the
       * include stack. We require the file to be found in this case
       * so we can give meaningful error messages, and because on reparsing
       * non-absolute paths don't make sense.
       */
      GSList *tmp_list = current_files_stack;
      while (tmp_list)
	{
	  GtkRcFile *curfile = tmp_list->data;
	  gchar *tmpname = g_build_filename (curfile->directory, filename, NULL);

	  if (g_file_test (tmpname, G_FILE_TEST_EXISTS))
	    {
	      to_parse = tmpname;
	      break;
	    }

	  g_free (tmpname);
	  
	  tmp_list = tmp_list->next;
	}
    }

  if (to_parse)
    {
      gtk_rc_context_parse_file (context, to_parse, context->default_priority, FALSE);
      g_free (to_parse);
    }
  else
    {
      g_scanner_warn (scanner, 
		      _("Unable to find include file: \"%s\""),
		      filename);
    }

}

static guint
gtk_rc_parse_statement (GtkRcContext *context,
			GScanner     *scanner)
{
  guint token;
  
  token = g_scanner_peek_next_token (scanner);
  switch (token)
    {
    case GTK_RC_TOKEN_INCLUDE:
      token = g_scanner_get_next_token (scanner);
      if (token != GTK_RC_TOKEN_INCLUDE)
	return GTK_RC_TOKEN_INCLUDE;
      token = g_scanner_get_next_token (scanner);
      if (token != G_TOKEN_STRING)
	return G_TOKEN_STRING;
      parse_include_file (context, scanner, scanner->value.v_string);
      return G_TOKEN_NONE;
      
    case GTK_RC_TOKEN_STYLE:
      return gtk_rc_parse_style (context, scanner);
      
    case GTK_RC_TOKEN_BINDING:
      return _gtk_binding_parse_binding (scanner);
      
    case GTK_RC_TOKEN_PIXMAP_PATH:
      return gtk_rc_parse_pixmap_path (context, scanner);
      
    case GTK_RC_TOKEN_WIDGET:
      return gtk_rc_parse_path_pattern (context, scanner);
      
    case GTK_RC_TOKEN_WIDGET_CLASS:
      return gtk_rc_parse_path_pattern (context, scanner);
      
    case GTK_RC_TOKEN_CLASS:
      return gtk_rc_parse_path_pattern (context, scanner);
      
    case GTK_RC_TOKEN_MODULE_PATH:
      return gtk_rc_parse_module_path (scanner);
      
    case GTK_RC_TOKEN_IM_MODULE_FILE:
      return gtk_rc_parse_im_module_file (scanner);

    case G_TOKEN_IDENTIFIER:
      if (is_c_identifier (scanner->next_value.v_identifier))
	{
	  GtkRcProperty prop = { 0, 0, NULL, { 0, }, };
	  gchar *name;
	  
	  g_scanner_get_next_token (scanner); /* eat identifier */
	  name = g_strdup (scanner->value.v_identifier);
	  
	  token = gtk_rc_parse_assignment (scanner, NULL, &prop);
	  if (token == G_TOKEN_NONE)
	    {
	      GtkSettingsValue svalue;

	      svalue.origin = prop.origin;
	      memcpy (&svalue.value, &prop.value, sizeof (prop.value));
	      g_strcanon (name, G_CSET_DIGITS "-" G_CSET_a_2_z G_CSET_A_2_Z, '-');
	      _gtk_settings_set_property_value_from_rc (context->settings,
							name,
							&svalue);
	    }
	  g_free (prop.origin);
	  if (G_VALUE_TYPE (&prop.value))
	    g_value_unset (&prop.value);
	  g_free (name);
	  
	  return token;
	}
      else
	{
	  g_scanner_get_next_token (scanner);
	  return G_TOKEN_IDENTIFIER;
	}
    default:
      g_scanner_get_next_token (scanner);
      return /* G_TOKEN_SYMBOL */ GTK_RC_TOKEN_STYLE;
    }
}

static void
fixup_rc_set (GSList     *list,
	      GtkRcStyle *orig,
	      GtkRcStyle *new)
{
  while (list)
    {
      GtkRcSet *set = list->data;
      if (set->rc_style == orig)
	set->rc_style = new;
      list = list->next;
    }
}

static void
fixup_rc_sets (GtkRcContext *context,
	       GtkRcStyle   *orig,
	       GtkRcStyle   *new)
{
  fixup_rc_set (context->rc_sets_widget, orig, new);
  fixup_rc_set (context->rc_sets_widget_class, orig, new);
  fixup_rc_set (context->rc_sets_class, orig, new);
}

static guint
gtk_rc_parse_style (GtkRcContext *context,
		    GScanner     *scanner)
{
  GtkRcStyle *rc_style;
  GtkRcStyle *orig_style;
  GtkRcStyle *parent_style = NULL;
  GtkRcStylePrivate *rc_priv = NULL;
  guint token;
  gint i;
  GtkIconFactory *our_factory = NULL;
  GHashTable *our_hash = NULL;

  token = g_scanner_get_next_token (scanner);
  if (token != GTK_RC_TOKEN_STYLE)
    return GTK_RC_TOKEN_STYLE;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_STRING)
    return G_TOKEN_STRING;
  
  rc_style = gtk_rc_style_find (context, scanner->value.v_string);
  if (rc_style)
    orig_style = g_object_ref (rc_style);
  else
    orig_style = NULL;

  if (!rc_style)
    {
      rc_style = gtk_rc_style_new ();
      rc_style->name = g_strdup (scanner->value.v_string);
      
      for (i = 0; i < 5; i++)
	rc_style->bg_pixmap_name[i] = NULL;

      for (i = 0; i < 5; i++)
	rc_style->color_flags[i] = 0;
    }

  rc_priv = GTK_RC_STYLE_GET_PRIVATE (rc_style);

  /* If there's a list, its first member is always the factory belonging
   * to this RcStyle
   */
  if (rc_style->icon_factories)
    our_factory = rc_style->icon_factories->data;
  if (rc_priv->color_hashes)
    our_hash = rc_priv->color_hashes->data;

  token = g_scanner_peek_next_token (scanner);
  if (token == G_TOKEN_EQUAL_SIGN)
    {
      token = g_scanner_get_next_token (scanner);
      
      token = g_scanner_get_next_token (scanner);
      if (token != G_TOKEN_STRING)
	{
	  token = G_TOKEN_STRING;
	  goto err;
	}
      
      parent_style = gtk_rc_style_find (context, scanner->value.v_string);
      if (parent_style)
	{
	  for (i = 0; i < 5; i++)
	    {
	      rc_style->color_flags[i] = parent_style->color_flags[i];
	      rc_style->fg[i] = parent_style->fg[i];
	      rc_style->bg[i] = parent_style->bg[i];
	      rc_style->text[i] = parent_style->text[i];
	      rc_style->base[i] = parent_style->base[i];
	    }

	  rc_style->xthickness = parent_style->xthickness;
	  rc_style->ythickness = parent_style->ythickness;
	  
	  if (parent_style->font_desc)
	    {
	      if (rc_style->font_desc)
		pango_font_description_free (rc_style->font_desc);
	      rc_style->font_desc = pango_font_description_copy (parent_style->font_desc);
	    }

	  if (parent_style->rc_properties)
	    {
	      guint i;

	      for (i = 0; i < parent_style->rc_properties->len; i++)
		insert_rc_property (rc_style,
				    &g_array_index (parent_style->rc_properties, GtkRcProperty, i),
				    TRUE);
	    }
	  
	  for (i = 0; i < 5; i++)
	    {
	      g_free (rc_style->bg_pixmap_name[i]);
	      rc_style->bg_pixmap_name[i] = g_strdup (parent_style->bg_pixmap_name[i]);
	    }
	}
    }

  /*  get icon_factories and color_hashes from the parent style;
   *  if the parent_style doesn't have color_hashes, initializes
   *  the color_hashes with the settings' color scheme (if it exists)
   */
  gtk_rc_style_copy_icons_and_colors (rc_style, parent_style, context);

  if (rc_style->icon_factories)
    our_factory = rc_style->icon_factories->data;
  if (rc_priv->color_hashes)
    our_hash = rc_priv->color_hashes->data;

  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_LEFT_CURLY)
    {
      token = G_TOKEN_LEFT_CURLY;
      goto err;
    }
  
  token = g_scanner_peek_next_token (scanner);
  while (token != G_TOKEN_RIGHT_CURLY)
    {
      switch (token)
	{
	case GTK_RC_TOKEN_BG:
	  token = gtk_rc_parse_bg (scanner, rc_style);
	  break;
	case GTK_RC_TOKEN_FG:
	  token = gtk_rc_parse_fg (scanner, rc_style);
	  break;
	case GTK_RC_TOKEN_TEXT:
	  token = gtk_rc_parse_text (scanner, rc_style);
	  break;
	case GTK_RC_TOKEN_BASE:
	  token = gtk_rc_parse_base (scanner, rc_style);
	  break;
	case GTK_RC_TOKEN_XTHICKNESS:
	  token = gtk_rc_parse_xthickness (scanner, rc_style);
	  break;
	case GTK_RC_TOKEN_YTHICKNESS:
	  token = gtk_rc_parse_ythickness (scanner, rc_style);
	  break;
	case GTK_RC_TOKEN_BG_PIXMAP:
	  token = gtk_rc_parse_bg_pixmap (context, scanner, rc_style);
	  break;
	case GTK_RC_TOKEN_FONT:
	  token = gtk_rc_parse_font (scanner, rc_style);
	  break;
	case GTK_RC_TOKEN_FONTSET:
	  token = gtk_rc_parse_fontset (scanner, rc_style);
	  break;
	case GTK_RC_TOKEN_FONT_NAME:
	  token = gtk_rc_parse_font_name (scanner, rc_style);
	  break;
	case GTK_RC_TOKEN_ENGINE:
	  token = gtk_rc_parse_engine (context, scanner, &rc_style);
	  break;
        case GTK_RC_TOKEN_STOCK:
          if (our_factory == NULL)
            gtk_rc_style_prepend_empty_icon_factory (rc_style);
          our_factory = rc_style->icon_factories->data;
          token = gtk_rc_parse_stock (context, scanner, rc_style, our_factory);
          break;
        case GTK_RC_TOKEN_COLOR:
          if (our_hash == NULL)
            {
              gtk_rc_style_prepend_empty_color_hash (rc_style);
              our_hash = rc_priv->color_hashes->data;
            }
          token = gtk_rc_parse_logical_color (scanner, rc_style, our_hash);
          break;
	case G_TOKEN_IDENTIFIER:
	  if (is_c_identifier (scanner->next_value.v_identifier))
	    {
	      GtkRcProperty prop = { 0, 0, NULL, { 0, }, };
	      gchar *name;

	      g_scanner_get_next_token (scanner); /* eat type name */
	      prop.type_name = g_quark_from_string (scanner->value.v_identifier);
	      if (g_scanner_get_next_token (scanner) != ':' ||
		  g_scanner_get_next_token (scanner) != ':')
		{
		  token = ':';
		  break;
		}
	      if (g_scanner_get_next_token (scanner) != G_TOKEN_IDENTIFIER ||
		  !is_c_identifier (scanner->value.v_identifier))
		{
		  token = G_TOKEN_IDENTIFIER;
		  break;
		}

	      /* it's important that we do the same canonification as GParamSpecPool here */
	      name = g_strdup (scanner->value.v_identifier);
	      g_strcanon (name, G_CSET_DIGITS "-" G_CSET_a_2_z G_CSET_A_2_Z, '-');
	      prop.property_name = g_quark_from_string (name);
	      g_free (name);

	      token = gtk_rc_parse_assignment (scanner, rc_style, &prop);
	      if (token == G_TOKEN_NONE)
		{
		  g_return_val_if_fail (G_VALUE_TYPE (&prop.value) != 0, G_TOKEN_ERROR);
		  insert_rc_property (rc_style, &prop, TRUE);
		}
	      
	      g_free (prop.origin);
	      if (G_VALUE_TYPE (&prop.value))
		g_value_unset (&prop.value);
	    }
	  else
	    {
	      g_scanner_get_next_token (scanner);
	      token = G_TOKEN_IDENTIFIER;
	    }
	  break;
	default:
	  g_scanner_get_next_token (scanner);
	  token = G_TOKEN_RIGHT_CURLY;
	  break;
	}

      if (token != G_TOKEN_NONE)
	goto err;

      token = g_scanner_peek_next_token (scanner);
    } /* while (token != G_TOKEN_RIGHT_CURLY) */
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_RIGHT_CURLY)
    {
      token = G_TOKEN_RIGHT_CURLY;
      goto err;
    }
  
  if (rc_style != orig_style)
    {
      if (!context->rc_style_ht)
	context->rc_style_ht = g_hash_table_new ((GHashFunc) gtk_rc_style_hash,
						 (GEqualFunc) gtk_rc_style_equal);
      
      g_hash_table_replace (context->rc_style_ht, rc_style->name, rc_style);

      /* If we copied the data into a new rc style, fix up references to the old rc style
       * in bindings that we have.
       */
      if (orig_style)
	fixup_rc_sets (context, orig_style, rc_style);
    }

  if (orig_style)
    g_object_unref (orig_style);
  
  return G_TOKEN_NONE;

 err:
  if (rc_style != orig_style)
    g_object_unref (rc_style);

  if (orig_style)
    g_object_unref (orig_style);
  
  return token;
}

const GtkRcProperty*
_gtk_rc_style_lookup_rc_property (GtkRcStyle *rc_style,
				  GQuark      type_name,
				  GQuark      property_name)
{
  GtkRcProperty *node = NULL;

  g_return_val_if_fail (GTK_IS_RC_STYLE (rc_style), NULL);

  if (rc_style->rc_properties)
    {
      GtkRcProperty key;

      key.type_name = type_name;
      key.property_name = property_name;

      node = bsearch (&key,
		      rc_style->rc_properties->data, rc_style->rc_properties->len,
		      sizeof (GtkRcProperty), gtk_rc_properties_cmp);
    }

  return node;
}

static guint
gtk_rc_parse_bg (GScanner   *scanner,
		 GtkRcStyle *style)
{
  GtkStateType state;
  guint token;
  
  token = g_scanner_get_next_token (scanner);
  if (token != GTK_RC_TOKEN_BG)
    return GTK_RC_TOKEN_BG;
  
  token = gtk_rc_parse_state (scanner, &state);
  if (token != G_TOKEN_NONE)
    return token;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;

  style->color_flags[state] |= GTK_RC_BG;
  return gtk_rc_parse_color_full (scanner, style, &style->bg[state]);
}

static guint
gtk_rc_parse_fg (GScanner   *scanner,
		 GtkRcStyle *style)
{
  GtkStateType state;
  guint token;
  
  token = g_scanner_get_next_token (scanner);
  if (token != GTK_RC_TOKEN_FG)
    return GTK_RC_TOKEN_FG;
  
  token = gtk_rc_parse_state (scanner, &state);
  if (token != G_TOKEN_NONE)
    return token;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;
  
  style->color_flags[state] |= GTK_RC_FG;
  return gtk_rc_parse_color_full (scanner, style, &style->fg[state]);
}

static guint
gtk_rc_parse_text (GScanner   *scanner,
		   GtkRcStyle *style)
{
  GtkStateType state;
  guint token;
  
  token = g_scanner_get_next_token (scanner);
  if (token != GTK_RC_TOKEN_TEXT)
    return GTK_RC_TOKEN_TEXT;
  
  token = gtk_rc_parse_state (scanner, &state);
  if (token != G_TOKEN_NONE)
    return token;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;
  
  style->color_flags[state] |= GTK_RC_TEXT;
  return gtk_rc_parse_color_full (scanner, style, &style->text[state]);
}

static guint
gtk_rc_parse_base (GScanner   *scanner,
		   GtkRcStyle *style)
{
  GtkStateType state;
  guint token;
  
  token = g_scanner_get_next_token (scanner);
  if (token != GTK_RC_TOKEN_BASE)
    return GTK_RC_TOKEN_BASE;
  
  token = gtk_rc_parse_state (scanner, &state);
  if (token != G_TOKEN_NONE)
    return token;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;

  style->color_flags[state] |= GTK_RC_BASE;
  return gtk_rc_parse_color_full (scanner, style, &style->base[state]);
}

static guint
gtk_rc_parse_xthickness (GScanner   *scanner,
			 GtkRcStyle *style)
{
  if (g_scanner_get_next_token (scanner) != (guint) GTK_RC_TOKEN_XTHICKNESS)
    return GTK_RC_TOKEN_XTHICKNESS;

  if (g_scanner_get_next_token (scanner) != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;

  if (g_scanner_get_next_token (scanner) != G_TOKEN_INT)
    return G_TOKEN_INT;

  style->xthickness = scanner->value.v_int;

  return G_TOKEN_NONE;
}

static guint
gtk_rc_parse_ythickness (GScanner   *scanner,
			 GtkRcStyle *style)
{
  if (g_scanner_get_next_token (scanner) != (guint) GTK_RC_TOKEN_YTHICKNESS)
    return GTK_RC_TOKEN_YTHICKNESS;

  if (g_scanner_get_next_token (scanner) != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;

  if (g_scanner_get_next_token (scanner) != G_TOKEN_INT)
    return G_TOKEN_INT;

  style->ythickness = scanner->value.v_int;

  return G_TOKEN_NONE;
}

static guint
gtk_rc_parse_bg_pixmap (GtkRcContext *context,
			GScanner     *scanner,
			GtkRcStyle   *rc_style)
{
  GtkStateType state;
  guint token;
  gchar *pixmap_file;
  
  token = g_scanner_get_next_token (scanner);
  if (token != GTK_RC_TOKEN_BG_PIXMAP)
    return GTK_RC_TOKEN_BG_PIXMAP;
  
  token = gtk_rc_parse_state (scanner, &state);
  if (token != G_TOKEN_NONE)
    return token;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_STRING)
    return G_TOKEN_STRING;
  
  if ((strcmp (scanner->value.v_string, "<parent>") == 0) ||
      (strcmp (scanner->value.v_string, "<none>") == 0))
    pixmap_file = g_strdup (scanner->value.v_string);
  else
    pixmap_file = gtk_rc_find_pixmap_in_path (context->settings,
					      scanner, scanner->value.v_string);
  
  if (pixmap_file)
    {
      g_free (rc_style->bg_pixmap_name[state]);
      rc_style->bg_pixmap_name[state] = pixmap_file;
    }
  
  return G_TOKEN_NONE;
}

static gchar*
gtk_rc_check_pixmap_dir (const gchar *dir, 
			 const gchar *pixmap_file)
{
  gchar *buf;

  buf = g_build_filename (dir, pixmap_file, NULL);

  if (g_file_test (buf, G_FILE_TEST_EXISTS))
    return buf;
   
  g_free (buf);
 
   return NULL;
 }

/**
 * gtk_rc_find_pixmap_in_path:
 * @settings: a #GtkSettings
 * @scanner: Scanner used to get line number information for the
 *   warning message, or %NULL
 * @pixmap_file: name of the pixmap file to locate.
 * 
 * Looks up a file in pixmap path for the specified #GtkSettings.
 * If the file is not found, it outputs a warning message using
 * g_warning() and returns %NULL.
 *
 * Return value: the filename. 
 **/
gchar*
gtk_rc_find_pixmap_in_path (GtkSettings  *settings,
			    GScanner     *scanner,
			    const gchar  *pixmap_file)
{
  gint i;
  gchar *filename;
  GSList *tmp_list;

  GtkRcContext *context = gtk_rc_context_get (settings);
    
  if (context->pixmap_path)
    for (i = 0; context->pixmap_path[i] != NULL; i++)
      {
	filename = gtk_rc_check_pixmap_dir (context->pixmap_path[i], pixmap_file);
	if (filename)
	  return filename;
      }
  
  tmp_list = current_files_stack;
  while (tmp_list)
    {
      GtkRcFile *curfile = tmp_list->data;
      filename = gtk_rc_check_pixmap_dir (curfile->directory, pixmap_file);
      if (filename)
 	return filename;
       
      tmp_list = tmp_list->next;
    }
  
  if (scanner)
    g_scanner_warn (scanner, 
                    _("Unable to locate image file in pixmap_path: \"%s\""),
                    pixmap_file);
  else
    g_warning (_("Unable to locate image file in pixmap_path: \"%s\""),
	       pixmap_file);
    
  return NULL;
}

/**
 * gtk_rc_find_module_in_path:
 * @module_file: name of a theme engine
 * 
 * Searches for a theme engine in the GTK+ search path. This function
 * is not useful for applications and should not be used.
 * 
 * Return value: The filename, if found (must be freed with g_free()),
 *   otherwise %NULL.
 **/
gchar*
gtk_rc_find_module_in_path (const gchar *module_file)
{
  return _gtk_find_module (module_file, "engines");
}

static guint
gtk_rc_parse_font (GScanner   *scanner,
		   GtkRcStyle *rc_style)
{
  guint token;
  
  token = g_scanner_get_next_token (scanner);
  if (token != GTK_RC_TOKEN_FONT)
    return GTK_RC_TOKEN_FONT;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_STRING)
    return G_TOKEN_STRING;

  /* Ignore, do nothing */
  
  return G_TOKEN_NONE;
}

static guint
gtk_rc_parse_fontset (GScanner	 *scanner,
		      GtkRcStyle *rc_style)
{
  guint token;
  
  token = g_scanner_get_next_token (scanner);
  if (token != GTK_RC_TOKEN_FONTSET)
    return GTK_RC_TOKEN_FONTSET;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_STRING)
    return G_TOKEN_STRING;

  /* Do nothing - silently ignore */
  
  return G_TOKEN_NONE;
}

static guint
gtk_rc_parse_font_name (GScanner   *scanner,
			GtkRcStyle *rc_style)
{
  guint token;
  
  token = g_scanner_get_next_token (scanner);
  if (token != GTK_RC_TOKEN_FONT_NAME)
    return GTK_RC_TOKEN_FONT;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_STRING)
    return G_TOKEN_STRING;

  if (rc_style->font_desc)
    pango_font_description_free (rc_style->font_desc);

  rc_style->font_desc = 
    pango_font_description_from_string (scanner->value.v_string);
  
  return G_TOKEN_NONE;
}

static guint	   
gtk_rc_parse_engine (GtkRcContext *context,
		     GScanner	  *scanner,
		     GtkRcStyle	 **rc_style)
{
  guint token;
  GtkThemeEngine *engine;
  guint result = G_TOKEN_NONE;
  GtkRcStyle *new_style = NULL;
  gboolean parsed_curlies = FALSE;
  GtkRcStylePrivate *rc_priv, *new_priv;
  
  token = g_scanner_get_next_token (scanner);
  if (token != GTK_RC_TOKEN_ENGINE)
    return GTK_RC_TOKEN_ENGINE;

  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_STRING)
    return G_TOKEN_STRING;

  if (!scanner->value.v_string[0])
    {
      /* Support engine "" {} to mean override to the default engine
       */
      token = g_scanner_get_next_token (scanner);
      if (token != G_TOKEN_LEFT_CURLY)
	return G_TOKEN_LEFT_CURLY;
      
      token = g_scanner_get_next_token (scanner);
      if (token != G_TOKEN_RIGHT_CURLY)
	return G_TOKEN_RIGHT_CURLY;

      parsed_curlies = TRUE;

      rc_priv = GTK_RC_STYLE_GET_PRIVATE (*rc_style);

      if (G_OBJECT_TYPE (*rc_style) != GTK_TYPE_RC_STYLE)
	{
	  new_style = gtk_rc_style_new ();
	  gtk_rc_style_real_merge (new_style, *rc_style);

          new_style->name = g_strdup ((*rc_style)->name);

          /* take over icon factories and color hashes 
           * from the to-be-deleted style
           */
          new_style->icon_factories = (*rc_style)->icon_factories;
          (*rc_style)->icon_factories = NULL;
          new_priv = GTK_RC_STYLE_GET_PRIVATE (new_style);
          new_priv->color_hashes = rc_priv->color_hashes;
          rc_priv->color_hashes = NULL;
	}
      else
	(*rc_style)->engine_specified = TRUE;
    }
  else
    {
      engine = gtk_theme_engine_get (scanner->value.v_string);
      
      token = g_scanner_get_next_token (scanner);
      if (token != G_TOKEN_LEFT_CURLY)
	return G_TOKEN_LEFT_CURLY;
      
      if (engine)
	{
	  GtkRcStyleClass *new_class;
	  
	  rc_priv = GTK_RC_STYLE_GET_PRIVATE (*rc_style);
	  new_style = gtk_theme_engine_create_rc_style (engine);
	  g_type_module_unuse (G_TYPE_MODULE (engine));
	  
	  new_class = GTK_RC_STYLE_GET_CLASS (new_style);

	  new_class->merge (new_style, *rc_style);

          new_style->name = g_strdup ((*rc_style)->name);

          /* take over icon factories and color hashes 
           * from the to-be-deleted style
           */
          new_style->icon_factories = (*rc_style)->icon_factories;
          (*rc_style)->icon_factories = NULL;
          new_priv = GTK_RC_STYLE_GET_PRIVATE (new_style);
          new_priv->color_hashes = rc_priv->color_hashes;
          rc_priv->color_hashes = NULL;
	  
	  if (new_class->parse)
	    {
	      parsed_curlies = TRUE;
	      result = new_class->parse (new_style, context->settings, scanner);
	      
	      if (result != G_TOKEN_NONE)
		{
                  /* copy icon factories and color hashes back
                   */
                  (*rc_style)->icon_factories = new_style->icon_factories;
                  new_style->icon_factories = NULL;
                  rc_priv->color_hashes = new_priv->color_hashes;
                  new_priv->color_hashes = NULL;

		  g_object_unref (new_style);
		  new_style = NULL;
		}
	    }
	}
    }

  if (!parsed_curlies)
    {
      /* Skip over remainder, looking for nested {}'s
       */
      guint count = 1;
      
      result = G_TOKEN_RIGHT_CURLY;
      while ((token = g_scanner_get_next_token (scanner)) != G_TOKEN_EOF)
	{
	  if (token == G_TOKEN_LEFT_CURLY)
	    count++;
	  else if (token == G_TOKEN_RIGHT_CURLY)
	    count--;
	  
	  if (count == 0)
	    {
	      result = G_TOKEN_NONE;
	      break;
	    }
	}
    }

  if (new_style)
    {
      new_style->engine_specified = TRUE;

      g_object_unref (*rc_style);
      *rc_style = new_style;
    }

  return result;
}

guint
gtk_rc_parse_state (GScanner	 *scanner,
		    GtkStateType *state)
{
  guint old_scope;
  guint token;

  g_return_val_if_fail (scanner != NULL, G_TOKEN_ERROR);
  g_return_val_if_fail (state != NULL, G_TOKEN_ERROR);
  
  /* we don't know where we got called from, so we reset the scope here.
   * if we bail out due to errors, we *don't* reset the scope, so the
   * error messaging code can make sense of our tokens.
   */
  old_scope = g_scanner_set_scope (scanner, 0);
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_LEFT_BRACE)
    return G_TOKEN_LEFT_BRACE;
  
  token = g_scanner_get_next_token (scanner);
  switch (token)
    {
    case GTK_RC_TOKEN_ACTIVE:
      *state = GTK_STATE_ACTIVE;
      break;
    case GTK_RC_TOKEN_INSENSITIVE:
      *state = GTK_STATE_INSENSITIVE;
      break;
    case GTK_RC_TOKEN_NORMAL:
      *state = GTK_STATE_NORMAL;
      break;
    case GTK_RC_TOKEN_PRELIGHT:
      *state = GTK_STATE_PRELIGHT;
      break;
    case GTK_RC_TOKEN_SELECTED:
      *state = GTK_STATE_SELECTED;
      break;
    default:
      return /* G_TOKEN_SYMBOL */ GTK_RC_TOKEN_NORMAL;
    }
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_RIGHT_BRACE)
    return G_TOKEN_RIGHT_BRACE;
  
  g_scanner_set_scope (scanner, old_scope);

  return G_TOKEN_NONE;
}

guint
gtk_rc_parse_priority (GScanner	           *scanner,
		       GtkPathPriorityType *priority)
{
  guint old_scope;
  guint token;

  g_return_val_if_fail (scanner != NULL, G_TOKEN_ERROR);
  g_return_val_if_fail (priority != NULL, G_TOKEN_ERROR);

  /* we don't know where we got called from, so we reset the scope here.
   * if we bail out due to errors, we *don't* reset the scope, so the
   * error messaging code can make sense of our tokens.
   */
  old_scope = g_scanner_set_scope (scanner, 0);
  
  token = g_scanner_get_next_token (scanner);
  if (token != ':')
    return ':';
  
  token = g_scanner_get_next_token (scanner);
  switch (token)
    {
    case GTK_RC_TOKEN_LOWEST:
      *priority = GTK_PATH_PRIO_LOWEST;
      break;
    case GTK_RC_TOKEN_GTK:
      *priority = GTK_PATH_PRIO_GTK;
      break;
    case GTK_RC_TOKEN_APPLICATION:
      *priority = GTK_PATH_PRIO_APPLICATION;
      break;
    case GTK_RC_TOKEN_THEME:
      *priority = GTK_PATH_PRIO_THEME;
      break;
    case GTK_RC_TOKEN_RC:
      *priority = GTK_PATH_PRIO_RC;
      break;
    case GTK_RC_TOKEN_HIGHEST:
      *priority = GTK_PATH_PRIO_HIGHEST;
      break;
    default:
      return /* G_TOKEN_SYMBOL */ GTK_RC_TOKEN_APPLICATION;
    }
  
  g_scanner_set_scope (scanner, old_scope);

  return G_TOKEN_NONE;
}

/**
 * gtk_rc_parse_color:
 * @scanner: a #GScanner
 * @color: (out): a pointer to a #GdkColor structure in which to store
 *     the result
 *
 * Parses a color in the <link linkend="color=format">format</link> expected
 * in a RC file. 
 *
 * Note that theme engines should use gtk_rc_parse_color_full() in 
 * order to support symbolic colors.
 *
 * Returns: %G_TOKEN_NONE if parsing succeeded, otherwise the token
 *     that was expected but not found
 */
guint
gtk_rc_parse_color (GScanner *scanner,
		    GdkColor *color)
{
  return gtk_rc_parse_color_full (scanner, NULL, color);
}

/**
 * gtk_rc_parse_color_full:
 * @scanner: a #GScanner
 * @style: (allow-none): a #GtkRcStyle, or %NULL
 * @color: (out): a pointer to a #GdkColor structure in which to store
 *     the result
 *
 * Parses a color in the <link linkend="color=format">format</link> expected
 * in a RC file. If @style is not %NULL, it will be consulted to resolve
 * references to symbolic colors.
 *
 * Returns: %G_TOKEN_NONE if parsing succeeded, otherwise the token
 *     that was expected but not found
 *
 * Since: 2.12
 */
guint
gtk_rc_parse_color_full (GScanner   *scanner,
                         GtkRcStyle *style,
                         GdkColor   *color)
{
  guint token;

  g_return_val_if_fail (scanner != NULL, G_TOKEN_ERROR);

  /* we don't need to set our own scope here, because
   * we don't need own symbols
   */
  
  token = g_scanner_get_next_token (scanner);
  switch (token)
    {
      gint token_int;
      GdkColor c1, c2;
      gboolean negate;
      gdouble l;

    case G_TOKEN_LEFT_CURLY:
      token = g_scanner_get_next_token (scanner);
      if (token == G_TOKEN_INT)
	token_int = scanner->value.v_int;
      else if (token == G_TOKEN_FLOAT)
	token_int = scanner->value.v_float * 65535.0;
      else
	return G_TOKEN_FLOAT;
      color->red = CLAMP (token_int, 0, 65535);
      
      token = g_scanner_get_next_token (scanner);
      if (token != G_TOKEN_COMMA)
	return G_TOKEN_COMMA;
      
      token = g_scanner_get_next_token (scanner);
      if (token == G_TOKEN_INT)
	token_int = scanner->value.v_int;
      else if (token == G_TOKEN_FLOAT)
	token_int = scanner->value.v_float * 65535.0;
      else
	return G_TOKEN_FLOAT;
      color->green = CLAMP (token_int, 0, 65535);
      
      token = g_scanner_get_next_token (scanner);
      if (token != G_TOKEN_COMMA)
	return G_TOKEN_COMMA;
      
      token = g_scanner_get_next_token (scanner);
      if (token == G_TOKEN_INT)
	token_int = scanner->value.v_int;
      else if (token == G_TOKEN_FLOAT)
	token_int = scanner->value.v_float * 65535.0;
      else
	return G_TOKEN_FLOAT;
      color->blue = CLAMP (token_int, 0, 65535);
      
      token = g_scanner_get_next_token (scanner);
      if (token != G_TOKEN_RIGHT_CURLY)
	return G_TOKEN_RIGHT_CURLY;
      return G_TOKEN_NONE;
      
    case G_TOKEN_STRING:
      if (!gdk_color_parse (scanner->value.v_string, color))
	{
          g_scanner_warn (scanner, "Invalid color constant '%s'",
                          scanner->value.v_string);
          return G_TOKEN_STRING;
	}
      return G_TOKEN_NONE;

    case '@':
      token = g_scanner_get_next_token (scanner);
      if (token != G_TOKEN_IDENTIFIER)
	return G_TOKEN_IDENTIFIER;

      if (!style || !lookup_color (style, scanner->value.v_identifier, color))
        {
          g_scanner_warn (scanner, "Invalid symbolic color '%s'",
                          scanner->value.v_identifier);
          return G_TOKEN_IDENTIFIER;
        }

      return G_TOKEN_NONE;

    case G_TOKEN_IDENTIFIER:
      if (strcmp (scanner->value.v_identifier, "mix") == 0)
        {
          token = g_scanner_get_next_token (scanner);
          if (token != G_TOKEN_LEFT_PAREN)
            return G_TOKEN_LEFT_PAREN;

          negate = FALSE;
          if (g_scanner_peek_next_token (scanner) == '-')
            {
              g_scanner_get_next_token (scanner); /* eat sign */
              negate = TRUE;
            }

          token = g_scanner_get_next_token (scanner);
          if (token != G_TOKEN_FLOAT)
            return G_TOKEN_FLOAT;

          l = negate ? -scanner->value.v_float : scanner->value.v_float;

          token = g_scanner_get_next_token (scanner);
          if (token != G_TOKEN_COMMA)
            return G_TOKEN_COMMA;

          token = gtk_rc_parse_color_full (scanner, style, &c1);
          if (token != G_TOKEN_NONE)
            return token;

	  token = g_scanner_get_next_token (scanner);
	  if (token != G_TOKEN_COMMA)
            return G_TOKEN_COMMA;

	  token = gtk_rc_parse_color_full (scanner, style, &c2);
	  if (token != G_TOKEN_NONE)
            return token;

	  token = g_scanner_get_next_token (scanner);
	  if (token != G_TOKEN_RIGHT_PAREN)
            return G_TOKEN_RIGHT_PAREN;

	  color->red   = l * c1.red   + (1.0 - l) * c2.red;
	  color->green = l * c1.green + (1.0 - l) * c2.green;
	  color->blue  = l * c1.blue  + (1.0 - l) * c2.blue;

	  return G_TOKEN_NONE;
	}
      else if (strcmp (scanner->value.v_identifier, "shade") == 0)
        {
	  token = g_scanner_get_next_token (scanner);
          if (token != G_TOKEN_LEFT_PAREN)
            return G_TOKEN_LEFT_PAREN;

          negate = FALSE;
          if (g_scanner_peek_next_token (scanner) == '-')
            {
              g_scanner_get_next_token (scanner); /* eat sign */
              negate = TRUE;
            }

          token = g_scanner_get_next_token (scanner);
          if (token != G_TOKEN_FLOAT)
            return G_TOKEN_FLOAT;

          l = negate ? -scanner->value.v_float : scanner->value.v_float;

          token = g_scanner_get_next_token (scanner);
          if (token != G_TOKEN_COMMA)
            return G_TOKEN_COMMA;

          token = gtk_rc_parse_color_full (scanner, style, &c1);
          if (token != G_TOKEN_NONE)
            return token;

          token = g_scanner_get_next_token (scanner);
          if (token != G_TOKEN_RIGHT_PAREN)
            return G_TOKEN_RIGHT_PAREN;

          _gtk_style_shade (&c1, color, l);

          return G_TOKEN_NONE;
        }
      else if (strcmp (scanner->value.v_identifier, "lighter") == 0 ||
               strcmp (scanner->value.v_identifier, "darker") == 0)
        {
          if (scanner->value.v_identifier[0] == 'l')
            l = 1.3;
          else
	    l = 0.7;

	  token = g_scanner_get_next_token (scanner);
          if (token != G_TOKEN_LEFT_PAREN)
            return G_TOKEN_LEFT_PAREN;

          token = gtk_rc_parse_color_full (scanner, style, &c1);
          if (token != G_TOKEN_NONE)
            return token;

          token = g_scanner_get_next_token (scanner);
          if (token != G_TOKEN_RIGHT_PAREN)
            return G_TOKEN_RIGHT_PAREN;

          _gtk_style_shade (&c1, color, l);

          return G_TOKEN_NONE;
        }
      else
        return G_TOKEN_IDENTIFIER;

    default:
      return G_TOKEN_STRING;
    }
}

static guint
gtk_rc_parse_pixmap_path (GtkRcContext *context,
			  GScanner     *scanner)
{
  guint token;
  
  token = g_scanner_get_next_token (scanner);
  if (token != GTK_RC_TOKEN_PIXMAP_PATH)
    return GTK_RC_TOKEN_PIXMAP_PATH;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_STRING)
    return G_TOKEN_STRING;
  
  gtk_rc_parse_pixmap_path_string (context, scanner, scanner->value.v_string);
  
  return G_TOKEN_NONE;
}

static void
gtk_rc_parse_pixmap_path_string (GtkRcContext *context,
				 GScanner     *scanner,
				 const gchar  *pix_path)
{
  g_strfreev (context->pixmap_path);
  context->pixmap_path = g_strsplit (pix_path, G_SEARCHPATH_SEPARATOR_S, -1);
}

static guint
gtk_rc_parse_module_path (GScanner *scanner)
{
  guint token;
  
  token = g_scanner_get_next_token (scanner);
  if (token != GTK_RC_TOKEN_MODULE_PATH)
    return GTK_RC_TOKEN_MODULE_PATH;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_STRING)
    return G_TOKEN_STRING;

  g_warning ("module_path directive is now ignored\n");

  return G_TOKEN_NONE;
}

static guint
gtk_rc_parse_im_module_file (GScanner *scanner)
{
  guint token;
  
  token = g_scanner_get_next_token (scanner);
  if (token != GTK_RC_TOKEN_IM_MODULE_FILE)
    return GTK_RC_TOKEN_IM_MODULE_FILE;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_STRING)
    return G_TOKEN_STRING;

  g_free (im_module_file);
    
  im_module_file = g_strdup (scanner->value.v_string);

  return G_TOKEN_NONE;
}

static guint
gtk_rc_parse_path_pattern (GtkRcContext *context,
			   GScanner     *scanner)
{
  guint token;
  GtkPathType path_type;
  gchar *pattern;
  gboolean is_binding;
  GtkPathPriorityType priority = context->default_priority;
  
  token = g_scanner_get_next_token (scanner);
  switch (token)
    {
    case GTK_RC_TOKEN_WIDGET:
      path_type = GTK_PATH_WIDGET;
      break;
    case GTK_RC_TOKEN_WIDGET_CLASS:
      path_type = GTK_PATH_WIDGET_CLASS;
      break;
    case GTK_RC_TOKEN_CLASS:
      path_type = GTK_PATH_CLASS;
      break;
    default:
      return GTK_RC_TOKEN_WIDGET_CLASS;
    }

  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_STRING)
    return G_TOKEN_STRING;

  pattern = g_strdup (scanner->value.v_string);

  token = g_scanner_get_next_token (scanner);
  if (token == GTK_RC_TOKEN_STYLE)
    is_binding = FALSE;
  else if (token == GTK_RC_TOKEN_BINDING)
    is_binding = TRUE;
  else
    {
      g_free (pattern);
      return GTK_RC_TOKEN_STYLE;
    }
  
  if (g_scanner_peek_next_token (scanner) == ':')
    {
      token = gtk_rc_parse_priority (scanner, &priority);
      if (token != G_TOKEN_NONE)
	{
	  g_free (pattern);
	  return token;
	}
    }
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_STRING)
    {
      g_free (pattern);
      return G_TOKEN_STRING;
    }

  if (is_binding)
    {
      GtkBindingSet *binding;

      binding = gtk_binding_set_find (scanner->value.v_string);
      if (!binding)
	{
	  g_free (pattern);
	  return G_TOKEN_STRING;
	}
      gtk_binding_set_add_path (binding, path_type, pattern, priority);
    }
  else
    {
      GtkRcStyle *rc_style;
      GtkRcSet *rc_set;

      rc_style = gtk_rc_style_find (context, scanner->value.v_string);
      
      if (!rc_style)
	{
	  g_free (pattern);
	  return G_TOKEN_STRING;
	}

      rc_set = g_new (GtkRcSet, 1);
      rc_set->type = path_type;
      
      if (path_type == GTK_PATH_WIDGET_CLASS)
        {
          rc_set->pspec = NULL;
          rc_set->path = _gtk_rc_parse_widget_class_path (pattern);
        }
      else
        {
          rc_set->pspec = g_pattern_spec_new (pattern);
          rc_set->path = NULL;
        }
      
      rc_set->rc_style = rc_style;
      rc_set->priority = priority;

      if (path_type == GTK_PATH_WIDGET)
	context->rc_sets_widget = g_slist_prepend (context->rc_sets_widget, rc_set);
      else if (path_type == GTK_PATH_WIDGET_CLASS)
	context->rc_sets_widget_class = g_slist_prepend (context->rc_sets_widget_class, rc_set);
      else
	context->rc_sets_class = g_slist_prepend (context->rc_sets_class, rc_set);
    }

  g_free (pattern);
  return G_TOKEN_NONE;
}

static guint
gtk_rc_parse_hash_key (GScanner  *scanner,
                       gchar    **hash_key)
{
  guint token;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_LEFT_BRACE)
    return G_TOKEN_LEFT_BRACE;

  token = g_scanner_get_next_token (scanner);
  
  if (token != G_TOKEN_STRING)
    return G_TOKEN_STRING;
  
  *hash_key = g_strdup (scanner->value.v_string);
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_RIGHT_BRACE)
    {
      g_free (*hash_key);
      return G_TOKEN_RIGHT_BRACE;
    }
  
  return G_TOKEN_NONE;
}

static guint
gtk_rc_parse_icon_source (GtkRcContext   *context,
			  GScanner	 *scanner,
                          GtkIconSet     *icon_set,
                          gboolean       *icon_set_valid)
{
  guint token;
  gchar *full_filename;
  GtkIconSource *source = NULL;

  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_LEFT_CURLY)
    return G_TOKEN_LEFT_CURLY;

  token = g_scanner_get_next_token (scanner);
  
  if (token != G_TOKEN_STRING && token != '@')
    return G_TOKEN_STRING;
  
  if (token == G_TOKEN_STRING)
    {
      /* Filename */

      source = gtk_icon_source_new ();      
      full_filename = gtk_rc_find_pixmap_in_path (context->settings, scanner, scanner->value.v_string);
      if (full_filename)
	{
	  gtk_icon_source_set_filename (source, full_filename);
	  g_free (full_filename);
	}
    }
  else
    {
      /* Icon name */
      
      token = g_scanner_get_next_token (scanner);
  
      if (token != G_TOKEN_STRING)
	return G_TOKEN_STRING;

      source = gtk_icon_source_new ();
      gtk_icon_source_set_icon_name (source, scanner->value.v_string);
    }

  /* We continue parsing even if we didn't find the pixmap so that rest of the
   * file is read, even if the syntax is bad. However we don't validate the 
   * icon_set so the caller can choose not to install it.
   */
  token = g_scanner_get_next_token (scanner);

  if (token == G_TOKEN_RIGHT_CURLY)
    goto done;
  else if (token != G_TOKEN_COMMA)
    {
      gtk_icon_source_free (source);
      return G_TOKEN_COMMA;
    }

  /* Get the direction */
  
  token = g_scanner_get_next_token (scanner);

  switch (token)
    {
    case GTK_RC_TOKEN_RTL:
      gtk_icon_source_set_direction_wildcarded (source, FALSE);
      gtk_icon_source_set_direction (source, GTK_TEXT_DIR_RTL);
      break;

    case GTK_RC_TOKEN_LTR:
      gtk_icon_source_set_direction_wildcarded (source, FALSE);
      gtk_icon_source_set_direction (source, GTK_TEXT_DIR_LTR);
      break;
      
    case '*':
      break;
      
    default:
      gtk_icon_source_free (source);
      return GTK_RC_TOKEN_RTL;
      break;
    }

  token = g_scanner_get_next_token (scanner);

  if (token == G_TOKEN_RIGHT_CURLY)
    goto done;
  else if (token != G_TOKEN_COMMA)
    {
      gtk_icon_source_free (source);
      return G_TOKEN_COMMA;
    }

  /* Get the state */
  
  token = g_scanner_get_next_token (scanner);
  
  switch (token)
    {
    case GTK_RC_TOKEN_NORMAL:
      gtk_icon_source_set_state_wildcarded (source, FALSE);
      gtk_icon_source_set_state (source, GTK_STATE_NORMAL);
      break;

    case GTK_RC_TOKEN_PRELIGHT:
      gtk_icon_source_set_state_wildcarded (source, FALSE);
      gtk_icon_source_set_state (source, GTK_STATE_PRELIGHT);
      break;
      

    case GTK_RC_TOKEN_INSENSITIVE:
      gtk_icon_source_set_state_wildcarded (source, FALSE);
      gtk_icon_source_set_state (source, GTK_STATE_INSENSITIVE);
      break;

    case GTK_RC_TOKEN_ACTIVE:
      gtk_icon_source_set_state_wildcarded (source, FALSE);
      gtk_icon_source_set_state (source, GTK_STATE_ACTIVE);
      break;

    case GTK_RC_TOKEN_SELECTED:
      gtk_icon_source_set_state_wildcarded (source, FALSE);
      gtk_icon_source_set_state (source, GTK_STATE_SELECTED);
      break;

    case '*':
      break;
      
    default:
      gtk_icon_source_free (source);
      return GTK_RC_TOKEN_PRELIGHT;
      break;
    }  

  token = g_scanner_get_next_token (scanner);

  if (token == G_TOKEN_RIGHT_CURLY)
    goto done;
  else if (token != G_TOKEN_COMMA)
    {
      gtk_icon_source_free (source);
      return G_TOKEN_COMMA;
    }
  
  /* Get the size */
  
  token = g_scanner_get_next_token (scanner);

  if (token != '*')
    {
      GtkIconSize size;
      
      if (token != G_TOKEN_STRING)
        {
          gtk_icon_source_free (source);
          return G_TOKEN_STRING;
        }

      size = gtk_icon_size_from_name (scanner->value.v_string);

      if (size != GTK_ICON_SIZE_INVALID)
        {
          gtk_icon_source_set_size_wildcarded (source, FALSE);
          gtk_icon_source_set_size (source, size);
        }
    }

  /* Check the close brace */
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_RIGHT_CURLY)
    {
      gtk_icon_source_free (source);
      return G_TOKEN_RIGHT_CURLY;
    }

 done:
  if (gtk_icon_source_get_filename (source) ||
      gtk_icon_source_get_icon_name (source))
    {
      gtk_icon_set_add_source (icon_set, source);
      *icon_set_valid = TRUE;
    }
  gtk_icon_source_free (source);
  
  return G_TOKEN_NONE;
}

static guint
gtk_rc_parse_stock (GtkRcContext   *context,
		    GScanner       *scanner,
                    GtkRcStyle     *rc_style,
                    GtkIconFactory *factory)
{
  GtkIconSet *icon_set = NULL;
  gboolean icon_set_valid = FALSE;
  gchar *stock_id = NULL;
  guint token;
  
  token = g_scanner_get_next_token (scanner);
  if (token != GTK_RC_TOKEN_STOCK)
    return GTK_RC_TOKEN_STOCK;
  
  token = gtk_rc_parse_hash_key (scanner, &stock_id);
  if (token != G_TOKEN_NONE)
    return token;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    {
      g_free (stock_id);
      return G_TOKEN_EQUAL_SIGN;
    }

  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_LEFT_CURLY)
    {
      g_free (stock_id);
      return G_TOKEN_LEFT_CURLY;
    }

  token = g_scanner_peek_next_token (scanner);
  while (token != G_TOKEN_RIGHT_CURLY)
    {
      if (icon_set == NULL)
        icon_set = gtk_icon_set_new ();
      
      token = gtk_rc_parse_icon_source (context, 
                                        scanner, icon_set, &icon_set_valid);
      if (token != G_TOKEN_NONE)
        {
          g_free (stock_id);
          gtk_icon_set_unref (icon_set);
          return token;
        }

      token = g_scanner_get_next_token (scanner);
      
      if (token != G_TOKEN_COMMA &&
          token != G_TOKEN_RIGHT_CURLY)
        {
          g_free (stock_id);
          gtk_icon_set_unref (icon_set);
          return G_TOKEN_RIGHT_CURLY;
        }
    }

  if (icon_set)
    {
      if (icon_set_valid)
        gtk_icon_factory_add (factory,
                              stock_id,
                              icon_set);

      gtk_icon_set_unref (icon_set);
    }
  
  g_free (stock_id);

  return G_TOKEN_NONE;
}

static guint
gtk_rc_parse_logical_color (GScanner   *scanner,
                            GtkRcStyle *rc_style,
                            GHashTable *hash)
{
  gchar *color_id = NULL;
  guint token;
  GdkColor color;

  token = g_scanner_get_next_token (scanner);
  if (token != GTK_RC_TOKEN_COLOR)
    return GTK_RC_TOKEN_COLOR;

  token = gtk_rc_parse_hash_key (scanner, &color_id);
  if (token != G_TOKEN_NONE)
    return token;

  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    {
      g_free (color_id);
      return G_TOKEN_EQUAL_SIGN;
    }

  token = gtk_rc_parse_color_full (scanner, rc_style, &color);
  if (token != G_TOKEN_NONE)
    {
      g_free (color_id);
      return token;
    }

  /* Because the hash is created with destroy functions,
   * g_hash_table_insert will free any old values for us,
   * if a mapping with the specified key already exists.
   */
  g_hash_table_insert (hash, color_id, gdk_color_copy (&color));

  return G_TOKEN_NONE;
}


GSList *
_gtk_rc_parse_widget_class_path (const gchar *pattern)
{
  GSList *result;
  PathElt *path_elt;
  const gchar *current;
  const gchar *class_start;
  const gchar *class_end;
  const gchar *pattern_end;
  const gchar *pattern_start;
  gchar *sub_pattern;

  result = NULL;
  current = pattern;
  while ((class_start = strchr (current, '<')) && 
	 (class_end = strchr (class_start, '>')))
    {
      /* Add patterns, but ignore single dots */
      if (!(class_start == current || 
	    (class_start == current + 1 && current[0] == '.')))
        {
          pattern_end = class_start - 1;
          pattern_start = current;
          
          path_elt = g_new (PathElt, 1);
          
          sub_pattern = g_strndup (pattern_start, pattern_end - pattern_start + 1);
	  path_elt->type = PATH_ELT_PSPEC;
          path_elt->elt.pspec = g_pattern_spec_new (sub_pattern);
          g_free (sub_pattern);
          
          result = g_slist_prepend (result, path_elt);
        }
      
      path_elt = g_new (PathElt, 1);
      
      /* The < > need to be removed from the string. */
      sub_pattern = g_strndup (class_start + 1, class_end - class_start - 1);
      
      path_elt->type = PATH_ELT_UNRESOLVED;
      path_elt->elt.class_name = sub_pattern;
      
      result = g_slist_prepend (result, path_elt);
      
      current = class_end + 1;
    }
  
  /* Add the rest, if anything is left */
  if (strlen (current) > 0)
    {
      path_elt = g_new (PathElt, 1);
      path_elt->type = PATH_ELT_PSPEC;
      path_elt->elt.pspec = g_pattern_spec_new (current);
      
      result = g_slist_prepend (result, path_elt);
    }
  
  return g_slist_reverse (result);
}

static void
free_path_elt (gpointer data, 
	       gpointer user_data)
{
  PathElt *path_elt = data;

  switch (path_elt->type)
    {
    case PATH_ELT_PSPEC:
      g_pattern_spec_free (path_elt->elt.pspec);
      break;
    case PATH_ELT_UNRESOLVED:
      g_free (path_elt->elt.class_name);
      break;
    case PATH_ELT_TYPE:
      break;
    default:
      g_assert_not_reached ();
    }

  g_free (path_elt);
}

void
_gtk_rc_free_widget_class_path (GSList *list)
{
  g_slist_foreach (list, free_path_elt, NULL);
  g_slist_free (list);
}

static void
gtk_rc_set_free (GtkRcSet *rc_set)
{
  if (rc_set->pspec)
    g_pattern_spec_free (rc_set->pspec);

  _gtk_rc_free_widget_class_path (rc_set->path);
  
  g_free (rc_set);
}

static gboolean
match_class (PathElt *path_elt, 
	     gchar   *type_name)
{
  GType type;
  
  if (path_elt->type == PATH_ELT_UNRESOLVED)
    {
      type = g_type_from_name (path_elt->elt.class_name);
      if (type != G_TYPE_INVALID)
        {
          g_free (path_elt->elt.class_name);
          path_elt->elt.class_type = type;
	  path_elt->type = PATH_ELT_TYPE;
        }
      else
	return g_str_equal (type_name, path_elt->elt.class_name);
    }
  
  return g_type_is_a (g_type_from_name (type_name), path_elt->elt.class_type);
}

static gboolean
match_widget_class_recursive (GSList *list, 
			      guint   length, 
			      gchar  *path, 
			      gchar  *path_reversed)
{
  PathElt *path_elt;
  
  /* break out if we cannot match anymore. */
  if (list == NULL)
    {
      if (length > 0)
        return FALSE;
      else
        return TRUE;
    }

  /* there are two possibilities:
   *  1. The next pattern should match the class.
   *  2. First normal matching, and then maybe a class */
  
  path_elt = list->data;

  if (path_elt->type != PATH_ELT_PSPEC)
    {
      gchar *class_start = path;
      gchar *class_end;
      
      /* ignore leading dot */
      if (class_start[0] == '.')
        class_start++;
      class_end = strchr (class_start, '.');

      if (class_end == NULL)
        {
          if (!match_class (path_elt, class_start))
            return FALSE;
	  else
	    return match_widget_class_recursive (list->next, 0, "", "");
        }
      else
        {
          class_end[0] = '\0';
          if (!match_class (path_elt, class_start))
            {
              class_end[0] = '.';
              return FALSE;
            }
          else
            {
              gboolean result;
              gint new_length = length - (class_end - path);
              gchar old_char = path_reversed[new_length];
              
              class_end[0] = '.';
              
              path_reversed[new_length] = '\0';
              result = match_widget_class_recursive (list->next, new_length, class_end, path_reversed);
              path_reversed[new_length] = old_char;
              
              return result;
            }
        }
    }
  else
    {
      PathElt *class_elt;
      gchar *class_start;
      gchar *class_end;
      gboolean result = FALSE;
      
      /* If there is nothing after this (ie. no class match), 
       * just compare the pspec. 
       */
      if (list->next == NULL)
        return g_pattern_match (path_elt->elt.pspec, length, path, path_reversed);
      
      class_elt = (PathElt *)list->next->data;
      g_assert (class_elt->type != PATH_ELT_PSPEC);
      
      class_start = path;
      if (class_start[0] == '.')
        class_start++;
      
      while (TRUE)
        {
	  class_end = strchr (class_start, '.');
          
          /* It should be cheaper to match the class first. (either the pattern
           * is simple, and will match most of the times, or it may be complex
           * and matching is slow) 
	   */
          if (class_end == NULL)
	    {
	      result = match_class (class_elt, class_start);
	    }
          else
            {
              class_end[0] = '\0';
              result = match_class (class_elt, class_start);
              class_end[0] = '.';
            }
          
          if (result)
            {
              gchar old_char;
              result = FALSE;
              
              /* terminate the string in front of the class. It does not matter
               * that the class becomes unusable, because it is not needed 
	       * inside the recursion 
	       */
              old_char = class_start[0];
              class_start[0] = '\0';
              
              if (g_pattern_match (path_elt->elt.pspec, class_start - path, path, path_reversed + length - (class_start - path)))
                {
                  if (class_end != NULL)
                    {
                      gint new_length = length - (class_end - path);
                      gchar path_reversed_char = path_reversed[new_length];
                      
                      path_reversed[new_length] = '\0';
                      
                      result = match_widget_class_recursive (list->next->next, new_length, class_end, path_reversed);
                      
                      path_reversed[new_length] = path_reversed_char;
                    }
                  else
                    result = match_widget_class_recursive (list->next->next, 0, "", "");
                }
                
              class_start[0] = old_char;
            }
          
          if (result)
            return TRUE;
          
          /* get next class in path, or break out */
          if (class_end != NULL)
            class_start = class_end + 1;
          else
            return FALSE;
        }
    }
}

gboolean
_gtk_rc_match_widget_class (GSList  *list,
                            gint     length,
                            gchar   *path,
                            gchar   *path_reversed)
{
  return match_widget_class_recursive (list, length, path, path_reversed);
}

#if defined (G_OS_WIN32) && !defined (_WIN64)

/* DLL ABI stability backward compatibility versions */

#undef gtk_rc_add_default_file

void
gtk_rc_add_default_file (const gchar *filename)
{
  gchar *utf8_filename = g_locale_to_utf8 (filename, -1, NULL, NULL, NULL);

  gtk_rc_add_default_file_utf8 (utf8_filename);

  g_free (utf8_filename);
}

#undef gtk_rc_set_default_files

void
gtk_rc_set_default_files (gchar **filenames)
{
  gchar **utf8_filenames;
  int n = 0, i;

  while (filenames[n++] != NULL)
    ;

  utf8_filenames = g_new (gchar *, n + 1);

  for (i = 0; i < n; i++)
    utf8_filenames[i] = g_locale_to_utf8 (filenames[i], -1, NULL, NULL, NULL);

  utf8_filenames[n] = NULL;

  gtk_rc_set_default_files_utf8 (utf8_filenames);

  g_strfreev (utf8_filenames);
}

#undef gtk_rc_parse

void
gtk_rc_parse (const gchar *filename)
{
  gchar *utf8_filename = g_locale_to_utf8 (filename, -1, NULL, NULL, NULL);

  gtk_rc_parse_utf8 (utf8_filename);

  g_free (utf8_filename);
}

#endif

#define __GTK_RC_C__
#include "gtkaliasdef.c"
