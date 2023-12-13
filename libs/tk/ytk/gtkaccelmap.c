/* GTK - The GIMP Toolkit
 * Copyright (C) 1998, 2001 Tim Janik
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

#include "config.h"

#include "gtkaccelmap.h"

#include "gtkmarshalers.h"
#include "gtkwindow.h"  /* in lack of GtkAcceleratable */
#include "gtkintl.h" 
#include "gtkalias.h"

#include <glib/gstdio.h>

#include <string.h>
#include <errno.h>
#include <fcntl.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef G_OS_WIN32
#include <io.h>
#endif


/* --- structures --- */
struct _GtkAccelMap
{
  GObject parent_instance;
};

struct _GtkAccelMapClass
{
  GObjectClass parent_class;
};

typedef struct {
  const gchar *accel_path;
  guint        accel_key;
  guint        accel_mods;
  guint	       std_accel_key;
  guint	       std_accel_mods;
  guint        changed    :  1;
  guint        lock_count : 15;
  GSList      *groups;
} AccelEntry;

/* --- signals --- */
enum {
  CHANGED,
  LAST_SIGNAL
};

/* --- variables --- */

static GHashTable  *accel_entry_ht = NULL;	/* accel_path -> AccelEntry */
static GSList      *accel_filters = NULL;
static gulong	    accel_map_signals[LAST_SIGNAL] = { 0, };
static GtkAccelMap *accel_map;

/* --- prototypes --- */
static void do_accel_map_changed (AccelEntry *entry);

/* --- functions --- */
static guint
accel_entry_hash (gconstpointer key)
{
  const AccelEntry *entry = key;

  return g_str_hash (entry->accel_path);
}

static gboolean
accel_entry_equal (gconstpointer key1,
		   gconstpointer key2)
{
  const AccelEntry *entry1 = key1;
  const AccelEntry *entry2 = key2;

  return g_str_equal (entry1->accel_path, entry2->accel_path);
}

static inline AccelEntry*
accel_path_lookup (const gchar *accel_path)
{
  AccelEntry ekey;

  ekey.accel_path = accel_path;

  /* safety NULL check for return_if_fail()s */
  return accel_path ? g_hash_table_lookup (accel_entry_ht, &ekey) : NULL;
}

void
_gtk_accel_map_init (void)
{
  g_assert (accel_entry_ht == NULL);

  accel_entry_ht = g_hash_table_new (accel_entry_hash, accel_entry_equal);
}

gboolean
_gtk_accel_path_is_valid (const gchar *accel_path)
{
  gchar *p;

  if (!accel_path || accel_path[0] != '<' ||
      accel_path[1] == '<' || accel_path[1] == '>' || !accel_path[1])
    return FALSE;
  p = strchr (accel_path, '>');
  if (!p || (p[1] != 0 && p[1] != '/'))
    return FALSE;
  return TRUE;
}

/**
 * gtk_accel_map_add_entry:
 * @accel_path: valid accelerator path
 * @accel_key:  the accelerator key
 * @accel_mods: the accelerator modifiers
 *
 * Registers a new accelerator with the global accelerator map.
 * This function should only be called once per @accel_path
 * with the canonical @accel_key and @accel_mods for this path.
 * To change the accelerator during runtime programatically, use
 * gtk_accel_map_change_entry().
 * The accelerator path must consist of "&lt;WINDOWTYPE&gt;/Category1/Category2/.../Action",
 * where &lt;WINDOWTYPE&gt; should be a unique application-specific identifier, that
 * corresponds to the kind of window the accelerator is being used in, e.g. "Gimp-Image",
 * "Abiword-Document" or "Gnumeric-Settings".
 * The Category1/.../Action portion is most appropriately chosen by the action the
 * accelerator triggers, i.e. for accelerators on menu items, choose the item's menu path,
 * e.g. "File/Save As", "Image/View/Zoom" or "Edit/Select All".
 * So a full valid accelerator path may look like:
 * "&lt;Gimp-Toolbox&gt;/File/Dialogs/Tool Options...".
 * 
 * Note that @accel_path string will be stored in a #GQuark. Therefore, if you
 * pass a static string, you can save some memory by interning it first with 
 * g_intern_static_string().
 */
void
gtk_accel_map_add_entry (const gchar    *accel_path,
			 guint           accel_key,
			 GdkModifierType accel_mods)
{
  AccelEntry *entry;

  g_return_if_fail (_gtk_accel_path_is_valid (accel_path));

  if (!accel_key)
    accel_mods = 0;
  else
    accel_mods &= gtk_accelerator_get_default_mod_mask ();

  entry = accel_path_lookup (accel_path);
  if (entry)
    {
      if (!entry->std_accel_key && !entry->std_accel_mods &&
	  (accel_key || accel_mods))
	{
	  entry->std_accel_key = accel_key;
	  entry->std_accel_mods = accel_mods;
	  if (!entry->changed)
	    gtk_accel_map_change_entry (entry->accel_path, accel_key, accel_mods, TRUE);
	}
    }
  else
    {
      entry = g_slice_new0 (AccelEntry);
      entry->accel_path = g_intern_string (accel_path);
      entry->std_accel_key = accel_key;
      entry->std_accel_mods = accel_mods;
      entry->accel_key = accel_key;
      entry->accel_mods = accel_mods;
      entry->changed = FALSE;
      entry->lock_count = 0;
      g_hash_table_insert (accel_entry_ht, entry, entry);

      do_accel_map_changed (entry);
    }
}

/**
 * gtk_accel_map_lookup_entry:
 * @accel_path:  a valid accelerator path
 * @key:         the accelerator key to be filled in (optional)
 * @returns:     %TRUE if @accel_path is known, %FALSE otherwise
 *
 * Looks up the accelerator entry for @accel_path and fills in @key.
 */
gboolean
gtk_accel_map_lookup_entry (const gchar *accel_path,
			    GtkAccelKey *key)
{
  AccelEntry *entry;

  g_return_val_if_fail (_gtk_accel_path_is_valid (accel_path), FALSE);

  entry = accel_path_lookup (accel_path);
  if (entry && key)
    {
      key->accel_key = entry->accel_key;
      key->accel_mods = entry->accel_mods;
      key->accel_flags = 0;
    }

  return entry ? TRUE : FALSE;
}

static void
hash2slist_foreach (gpointer  key,
		    gpointer  value,
		    gpointer  user_data)
{
  GSList **slist_p = user_data;

  *slist_p = g_slist_prepend (*slist_p, value);
}

static GSList*
g_hash_table_slist_values (GHashTable *hash_table)
{
  GSList *slist = NULL;

  g_return_val_if_fail (hash_table != NULL, NULL);

  g_hash_table_foreach (hash_table, hash2slist_foreach, &slist);

  return slist;
}

/* if simulate==TRUE, return whether accel_path can be changed to
 * accel_key && accel_mods. otherwise, return whether accel_path
 * was actually changed.
 */
static gboolean
internal_change_entry (const gchar    *accel_path,
		       guint           accel_key,
		       GdkModifierType accel_mods,
		       gboolean        replace,
		       gboolean	       simulate)
{
  GSList *node, *slist, *win_list, *group_list, *replace_list = NULL;
  GHashTable *group_hm, *window_hm;
  gboolean change_accel, removable, can_change = TRUE, seen_accel = FALSE;
  GQuark entry_quark;
  AccelEntry *entry = accel_path_lookup (accel_path);

  /* not much todo if there's no entry yet */
  if (!entry)
    {
      if (!simulate)
	{
	  gtk_accel_map_add_entry (accel_path, 0, 0);
	  entry = accel_path_lookup (accel_path);
	  entry->accel_key = accel_key;
	  entry->accel_mods = accel_mods;
	  entry->changed = TRUE;

	  do_accel_map_changed (entry);
	}
      return TRUE;
    }

  /* if there's nothing to change, not much todo either */
  if (entry->accel_key == accel_key && entry->accel_mods == accel_mods)
    {
      if (!simulate)
	entry->changed = TRUE;
      return simulate ? TRUE : FALSE;
    }

  /* The no-change case has already been handled, so 
   * simulate doesn't make a difference here.
   */
  if (entry->lock_count > 0)
    return FALSE;

  /* nobody's interested, easy going */
  if (!entry->groups)
    {
      if (!simulate)
	{
	  entry->accel_key = accel_key;
	  entry->accel_mods = accel_mods;
	  entry->changed = TRUE;

	  do_accel_map_changed (entry);
	}
      return TRUE;
    }

  /* 1) fetch all accel groups affected by this entry */
  entry_quark = g_quark_try_string (entry->accel_path);
  group_hm = g_hash_table_new (NULL, NULL);
  window_hm = g_hash_table_new (NULL, NULL);
  for (slist = entry->groups; slist; slist = slist->next)
    g_hash_table_insert (group_hm, slist->data, slist->data);

  /* 2) collect acceleratables affected */
  group_list = g_hash_table_slist_values (group_hm);
  for (slist = group_list; slist; slist = slist->next)
    {
      GtkAccelGroup *group = slist->data;

      for (node = group->acceleratables; node; node = node->next)
	g_hash_table_insert (window_hm, node->data, node->data);
    }
  g_slist_free (group_list);

  /* 3) include all accel groups used by acceleratables */
  win_list = g_hash_table_slist_values (window_hm);
  g_hash_table_destroy (window_hm);
  for (slist = win_list; slist; slist = slist->next)
    for (node = gtk_accel_groups_from_object (slist->data); node; node = node->next)
      g_hash_table_insert (group_hm, node->data, node->data);
  group_list = g_hash_table_slist_values (group_hm);
  g_hash_table_destroy (group_hm);
  
  /* 4) walk the acceleratables and figure whether they occupy accel_key&accel_mods */
  if (accel_key)
    for (slist = win_list; slist; slist = slist->next)
      if (GTK_IS_WINDOW (slist->data))	/* bad kludge in lack of a GtkAcceleratable */
	if (_gtk_window_query_nonaccels (slist->data, accel_key, accel_mods))
	  {
	    seen_accel = TRUE;
	    break;
	  }
  removable = !seen_accel;
  
  /* 5) walk all accel groups and search for locks */
  if (removable)
    for (slist = group_list; slist; slist = slist->next)
      {
	GtkAccelGroup *group = slist->data;
	GtkAccelGroupEntry *ag_entry;
	guint i, n;
	
	n = 0;
	ag_entry = entry->accel_key ? gtk_accel_group_query (group, entry->accel_key, entry->accel_mods, &n) : NULL;
	for (i = 0; i < n; i++)
	  if (ag_entry[i].accel_path_quark == entry_quark)
	    {
	      can_change = !(ag_entry[i].key.accel_flags & GTK_ACCEL_LOCKED);
	      if (!can_change)
		goto break_loop_step5;
	    }
	
	n = 0;
	ag_entry = accel_key ? gtk_accel_group_query (group, accel_key, accel_mods, &n) : NULL;
	for (i = 0; i < n; i++)
	  {
	    seen_accel = TRUE;
	    removable = !group->lock_count && !(ag_entry[i].key.accel_flags & GTK_ACCEL_LOCKED);
	    if (!removable)
	      goto break_loop_step5;
	    if (ag_entry[i].accel_path_quark)
	      replace_list = g_slist_prepend (replace_list, GUINT_TO_POINTER (ag_entry[i].accel_path_quark));
	  }
      }
 break_loop_step5:
  
  /* 6) check whether we can remove existing accelerators */
  if (removable && can_change)
    for (slist = replace_list; slist; slist = slist->next)
      if (!internal_change_entry (g_quark_to_string (GPOINTER_TO_UINT (slist->data)), 0, 0, FALSE, TRUE))
	{
	  removable = FALSE;
	  break;
	}
  
  /* 7) check conditions and proceed if possible */
  change_accel = can_change && (!seen_accel || (removable && replace));
  
  if (change_accel && !simulate)
    {
      /* ref accel groups */
      for (slist = group_list; slist; slist = slist->next)
	g_object_ref (slist->data);

      /* 8) remove existing accelerators */
      for (slist = replace_list; slist; slist = slist->next)
	internal_change_entry (g_quark_to_string (GPOINTER_TO_UINT (slist->data)), 0, 0, FALSE, FALSE);

      /* 9) install new accelerator */
      entry->accel_key = accel_key;
      entry->accel_mods = accel_mods;
      entry->changed = TRUE;

      for (slist = group_list; slist; slist = slist->next)
	_gtk_accel_group_reconnect (slist->data, g_quark_from_string (entry->accel_path));

      /* unref accel groups */
      for (slist = group_list; slist; slist = slist->next)
	g_object_unref (slist->data);

      do_accel_map_changed (entry);
    }
  g_slist_free (replace_list);
  g_slist_free (group_list);
  g_slist_free (win_list);

  return change_accel;
}

/**
 * gtk_accel_map_change_entry:
 * @accel_path:  a valid accelerator path
 * @accel_key:   the new accelerator key
 * @accel_mods:  the new accelerator modifiers
 * @replace:     %TRUE if other accelerators may be deleted upon conflicts
 * @returns:     %TRUE if the accelerator could be changed, %FALSE otherwise
 *
 * Changes the @accel_key and @accel_mods currently associated with @accel_path.
 * Due to conflicts with other accelerators, a change may not always be possible,
 * @replace indicates whether other accelerators may be deleted to resolve such
 * conflicts. A change will only occur if all conflicts could be resolved (which
 * might not be the case if conflicting accelerators are locked). Successful
 * changes are indicated by a %TRUE return value.
 * 
 * Note that @accel_path string will be stored in a #GQuark. Therefore, if you
 * pass a static string, you can save some memory by interning it first with 
 * g_intern_static_string().
 */
gboolean
gtk_accel_map_change_entry (const gchar    *accel_path,
			    guint           accel_key,
			    GdkModifierType accel_mods,
			    gboolean        replace)
{
  g_return_val_if_fail (_gtk_accel_path_is_valid (accel_path), FALSE);

  return internal_change_entry (accel_path, accel_key, accel_key ? accel_mods : 0, replace, FALSE);
}

static guint
accel_map_parse_accel_path (GScanner *scanner)
{
  guint accel_key = 0;
  GdkModifierType accel_mods = 0;
  gchar *path, *accel;
  
  /* parse accel path */
  g_scanner_get_next_token (scanner);
  if (scanner->token != G_TOKEN_STRING)
    return G_TOKEN_STRING;

  /* test if the next token is an accelerator */
  g_scanner_peek_next_token (scanner);
  if (scanner->next_token != G_TOKEN_STRING)
    {
      /* if not so, eat that token and error out */
      g_scanner_get_next_token (scanner);
      return G_TOKEN_STRING;
    }

  /* get the full accelerator specification */
  path = g_strdup (scanner->value.v_string);
  g_scanner_get_next_token (scanner);
  accel = g_strdup (scanner->value.v_string);

  /* ensure the entry is present */
  gtk_accel_map_add_entry (path, 0, 0);

  /* and propagate it */
  gtk_accelerator_parse (accel, &accel_key, &accel_mods);
  gtk_accel_map_change_entry (path, accel_key, accel_mods, TRUE);

  g_free (accel);
  g_free (path);

  /* check correct statement end */
  g_scanner_get_next_token (scanner);
  if (scanner->token != ')')
    return ')';
  else
    return G_TOKEN_NONE;
}

static void
accel_map_parse_statement (GScanner *scanner)
{
  guint expected_token;

  g_scanner_get_next_token (scanner);

  if (scanner->token == G_TOKEN_SYMBOL)
    {
      guint (*parser_func) (GScanner*);

      parser_func = (guint (*) (GScanner *))scanner->value.v_symbol;

      expected_token = parser_func (scanner);
    }
  else
    expected_token = G_TOKEN_SYMBOL;

  /* skip rest of statement on errrors
   */
  if (expected_token != G_TOKEN_NONE)
    {
      register guint level;

      level = 1;
      if (scanner->token == ')')
	level--;
      if (scanner->token == '(')
	level++;

      while (!g_scanner_eof (scanner) && level > 0)
	{
	  g_scanner_get_next_token (scanner);

	  if (scanner->token == '(')
	    level++;
	  else if (scanner->token == ')')
	    level--;
	}
    }
}

/**
 * gtk_accel_map_load_scanner:
 * @scanner: a #GScanner which has already been provided with an input file
 *
 * #GScanner variant of gtk_accel_map_load().
 */
void
gtk_accel_map_load_scanner (GScanner *scanner)
{
  gboolean skip_comment_single;
  gboolean symbol_2_token;
  gchar *cpair_comment_single;
  gpointer saved_symbol;
  
  g_return_if_fail (scanner != NULL);

  /* configure scanner */
  skip_comment_single = scanner->config->skip_comment_single;
  scanner->config->skip_comment_single = TRUE;
  cpair_comment_single = scanner->config->cpair_comment_single;
  scanner->config->cpair_comment_single = ";\n";
  symbol_2_token = scanner->config->symbol_2_token;
  scanner->config->symbol_2_token = FALSE;
  saved_symbol = g_scanner_lookup_symbol (scanner, "gtk_accel_path");
  g_scanner_scope_add_symbol (scanner, 0, "gtk_accel_path", 
			      accel_map_parse_accel_path);

  /* outer parsing loop
   */
  g_scanner_peek_next_token (scanner);
  while (scanner->next_token == '(')
    {
      g_scanner_get_next_token (scanner);

      accel_map_parse_statement (scanner);

      g_scanner_peek_next_token (scanner);
    }

  /* restore config */
  scanner->config->skip_comment_single = skip_comment_single;
  scanner->config->cpair_comment_single = cpair_comment_single;
  scanner->config->symbol_2_token = symbol_2_token;
  g_scanner_scope_remove_symbol (scanner, 0, "gtk_accel_path");
  if (saved_symbol)
    g_scanner_scope_add_symbol (scanner, 0, "gtk_accel_path", saved_symbol);
}

/**
 * gtk_accel_map_load_fd:
 * @fd: a valid readable file descriptor
 *
 * Filedescriptor variant of gtk_accel_map_load().
 *
 * Note that the file descriptor will not be closed by this function.
 */
void
gtk_accel_map_load_fd (gint fd)
{
  GScanner *scanner;

  g_return_if_fail (fd >= 0);

  /* create and setup scanner */
  scanner = g_scanner_new (NULL);
  g_scanner_input_file (scanner, fd);

  gtk_accel_map_load_scanner (scanner);

  g_scanner_destroy (scanner);
}

/**
 * gtk_accel_map_load:
 * @file_name: a file containing accelerator specifications,
 *   in the GLib file name encoding
 *
 * Parses a file previously saved with gtk_accel_map_save() for
 * accelerator specifications, and propagates them accordingly.
 */
void
gtk_accel_map_load (const gchar *file_name)
{
  gint fd;

  g_return_if_fail (file_name != NULL);

  if (!g_file_test (file_name, G_FILE_TEST_IS_REGULAR))
    return;

  fd = g_open (file_name, O_RDONLY, 0);
  if (fd < 0)
    return;

  gtk_accel_map_load_fd (fd);

  close (fd);
}

static gboolean
write_all (gint   fd,
	   gchar *buf,
	   gsize  to_write)
{
  while (to_write > 0)
    {
      gssize count = write (fd, buf, to_write);
      if (count < 0)
	{
	  if (errno != EINTR)
	    return FALSE;
	}
      else
	{
	  to_write -= count;
	  buf += count;
	}
    }

  return TRUE;
}

static void
accel_map_print (gpointer        data,
		 const gchar    *accel_path,
		 guint           accel_key,
		 GdkModifierType accel_mods,
		 gboolean        changed)
{
  GString *gstring = g_string_new (changed ? NULL : "; ");
  gint fd = GPOINTER_TO_INT (data);
  gchar *tmp, *name;

  g_string_append (gstring, "(gtk_accel_path \"");

  tmp = g_strescape (accel_path, NULL);
  g_string_append (gstring, tmp);
  g_free (tmp);

  g_string_append (gstring, "\" \"");

  name = gtk_accelerator_name (accel_key, accel_mods);
  tmp = g_strescape (name, NULL);
  g_free (name);
  g_string_append (gstring, tmp);
  g_free (tmp);

  g_string_append (gstring, "\")\n");

  write_all (fd, gstring->str, gstring->len);

  g_string_free (gstring, TRUE);
}

/**
 * gtk_accel_map_save_fd:
 * @fd: a valid writable file descriptor
 *
 * Filedescriptor variant of gtk_accel_map_save().
 *
 * Note that the file descriptor will not be closed by this function.
 */
void
gtk_accel_map_save_fd (gint fd)
{
  GString *gstring;

  g_return_if_fail (fd >= 0);

  gstring = g_string_new ("; ");
  if (g_get_prgname ())
    g_string_append (gstring, g_get_prgname ());
  g_string_append (gstring, " GtkAccelMap rc-file         -*- scheme -*-\n");
  g_string_append (gstring, "; this file is an automated accelerator map dump\n");
  g_string_append (gstring, ";\n");

  write_all (fd, gstring->str, gstring->len);
  
  g_string_free (gstring, TRUE);

  gtk_accel_map_foreach (GINT_TO_POINTER (fd), accel_map_print);
}

/**
 * gtk_accel_map_save:
 * @file_name: the name of the file to contain accelerator specifications,
 *   in the GLib file name encoding
 *
 * Saves current accelerator specifications (accelerator path, key
 * and modifiers) to @file_name.
 * The file is written in a format suitable to be read back in by
 * gtk_accel_map_load().
 */
void
gtk_accel_map_save (const gchar *file_name)
{
  gint fd;

  g_return_if_fail (file_name != NULL);

  fd = g_open (file_name, O_CREAT | O_TRUNC | O_WRONLY, 0644);
  if (fd < 0)
    return;

  gtk_accel_map_save_fd (fd);

  close (fd);
}

/**
 * gtk_accel_map_foreach:
 * @data:         data to be passed into @foreach_func
 * @foreach_func: function to be executed for each accel map entry which
 *                is not filtered out
 *
 * Loops over the entries in the accelerator map whose accel path 
 * doesn't match any of the filters added with gtk_accel_map_add_filter(), 
 * and execute @foreach_func on each. The signature of @foreach_func is 
 * that of #GtkAccelMapForeach, the @changed parameter indicates whether
 * this accelerator was changed during runtime (thus, would need
 * saving during an accelerator map dump).
 */
void
gtk_accel_map_foreach (gpointer           data,
		       GtkAccelMapForeach foreach_func)
{
  GSList *entries, *slist, *node;

  g_return_if_fail (foreach_func != NULL);

  entries = g_hash_table_slist_values (accel_entry_ht);
  for (slist = entries; slist; slist = slist->next)
    {
      AccelEntry *entry = slist->data;
      gboolean changed = entry->accel_key != entry->std_accel_key || entry->accel_mods != entry->std_accel_mods;

      for (node = accel_filters; node; node = node->next)
	if (g_pattern_match_string (node->data, entry->accel_path))
	  goto skip_accel;
      foreach_func (data, entry->accel_path, entry->accel_key, entry->accel_mods, changed);
    skip_accel:
      /* noop */;
    }
  g_slist_free (entries);
}

/**
 * gtk_accel_map_foreach_unfiltered:
 * @data:         data to be passed into @foreach_func
 * @foreach_func: function to be executed for each accel map entry
 *
 * Loops over all entries in the accelerator map, and execute
 * @foreach_func on each. The signature of @foreach_func is that of
 * #GtkAccelMapForeach, the @changed parameter indicates whether
 * this accelerator was changed during runtime (thus, would need
 * saving during an accelerator map dump).
 */
void
gtk_accel_map_foreach_unfiltered (gpointer           data,
				  GtkAccelMapForeach foreach_func)
{
  GSList *entries, *slist;

  g_return_if_fail (foreach_func != NULL);

  entries = g_hash_table_slist_values (accel_entry_ht);
  for (slist = entries; slist; slist = slist->next)
    {
      AccelEntry *entry = slist->data;
      gboolean changed = entry->accel_key != entry->std_accel_key || entry->accel_mods != entry->std_accel_mods;

      foreach_func (data, entry->accel_path, entry->accel_key, entry->accel_mods, changed);
    }
  g_slist_free (entries);
}

/**
 * gtk_accel_map_add_filter:
 * @filter_pattern: a pattern (see #GPatternSpec)
 *
 * Adds a filter to the global list of accel path filters.
 *
 * Accel map entries whose accel path matches one of the filters
 * are skipped by gtk_accel_map_foreach().
 *
 * This function is intended for GTK+ modules that create their own
 * menus, but don't want them to be saved into the applications accelerator
 * map dump.
 */
void
gtk_accel_map_add_filter (const gchar *filter_pattern)
{
  GPatternSpec *pspec;
  GSList *slist;

  g_return_if_fail (filter_pattern != NULL);

  pspec = g_pattern_spec_new (filter_pattern);
  for (slist = accel_filters; slist; slist = slist->next)
    if (g_pattern_spec_equal (pspec, slist->data))
      {
	g_pattern_spec_free (pspec);
	return;
      }
  accel_filters = g_slist_prepend (accel_filters, pspec);
}

void
_gtk_accel_map_add_group (const gchar   *accel_path,
			  GtkAccelGroup *accel_group)
{
  AccelEntry *entry;

  g_return_if_fail (_gtk_accel_path_is_valid (accel_path));
  g_return_if_fail (GTK_IS_ACCEL_GROUP (accel_group));

  entry = accel_path_lookup (accel_path);
  if (!entry)
    {
      gtk_accel_map_add_entry (accel_path, 0, 0);
      entry = accel_path_lookup (accel_path);
    }
  entry->groups = g_slist_prepend (entry->groups, accel_group);
}

void
_gtk_accel_map_remove_group (const gchar   *accel_path,
			     GtkAccelGroup *accel_group)
{
  AccelEntry *entry;

  entry = accel_path_lookup (accel_path);
  g_return_if_fail (entry != NULL);
  g_return_if_fail (g_slist_find (entry->groups, accel_group));

  entry->groups = g_slist_remove (entry->groups, accel_group);
}


/**
 * gtk_accel_map_lock_path:
 * @accel_path: a valid accelerator path
 * 
 * Locks the given accelerator path. If the accelerator map doesn't yet contain
 * an entry for @accel_path, a new one is created.
 *
 * Locking an accelerator path prevents its accelerator from being changed 
 * during runtime. A locked accelerator path can be unlocked by 
 * gtk_accel_map_unlock_path(). Refer to gtk_accel_map_change_entry() 
 * for information about runtime accelerator changes.
 *
 * If called more than once, @accel_path remains locked until
 * gtk_accel_map_unlock_path() has been called an equivalent number
 * of times.
 *
 * Note that locking of individual accelerator paths is independent from 
 * locking the #GtkAccelGroup containing them. For runtime accelerator
 * changes to be possible both the accelerator path and its #GtkAccelGroup
 * have to be unlocked. 
 *
 * Since: 2.4
 **/
void 
gtk_accel_map_lock_path (const gchar *accel_path)
{
  AccelEntry *entry;

  g_return_if_fail (_gtk_accel_path_is_valid (accel_path));

  entry = accel_path_lookup (accel_path);
  
  if (!entry)
    {
      gtk_accel_map_add_entry (accel_path, 0, 0);
      entry = accel_path_lookup (accel_path);
    }

  entry->lock_count += 1;
}

/**
 * gtk_accel_map_unlock_path:
 * @accel_path: a valid accelerator path
 * 
 * Undoes the last call to gtk_accel_map_lock_path() on this @accel_path.
 * Refer to gtk_accel_map_lock_path() for information about accelerator path locking.
 *
 * Since: 2.4
 **/
void 
gtk_accel_map_unlock_path (const gchar *accel_path)
{
  AccelEntry *entry;

  g_return_if_fail (_gtk_accel_path_is_valid (accel_path));

  entry = accel_path_lookup (accel_path);

  g_return_if_fail (entry != NULL && entry->lock_count > 0);

  entry->lock_count -= 1;  
}

G_DEFINE_TYPE (GtkAccelMap, gtk_accel_map, G_TYPE_OBJECT)

static void
gtk_accel_map_class_init (GtkAccelMapClass *accel_map_class)
{
  /**
   * GtkAccelMap::changed:
   * @object: the global accel map object
   * @accel_path: the path of the accelerator that changed
   * @accel_key: the key value for the new accelerator
   * @accel_mods: the modifier mask for the new accelerator
   *
   * Notifies of a change in the global accelerator map.
   * The path is also used as the detail for the signal,
   * so it is possible to connect to
   * changed::<replaceable>accel_path</replaceable>.
   *
   * Since: 2.4
   */
  accel_map_signals[CHANGED] = g_signal_new (I_("changed"),
					     G_TYPE_FROM_CLASS (accel_map_class),
					     G_SIGNAL_DETAILED|G_SIGNAL_RUN_LAST,
					     0,
					     NULL, NULL,
					     _gtk_marshal_VOID__STRING_UINT_FLAGS,
					     G_TYPE_NONE, 3,
					     G_TYPE_STRING, G_TYPE_UINT, GDK_TYPE_MODIFIER_TYPE);
}

static void
gtk_accel_map_init (GtkAccelMap *accel_map)
{
}

/**
 * gtk_accel_map_get:
 * 
 * Gets the singleton global #GtkAccelMap object. This object
 * is useful only for notification of changes to the accelerator
 * map via the ::changed signal; it isn't a parameter to the
 * other accelerator map functions.
 * 
 * Return value: the global #GtkAccelMap object
 *
 * Since: 2.4
 **/
GtkAccelMap *
gtk_accel_map_get (void)
{
  if (!accel_map)
    accel_map = g_object_new (GTK_TYPE_ACCEL_MAP, NULL);

  return accel_map;
}

static void
do_accel_map_changed (AccelEntry *entry)
{
  if (accel_map)
    g_signal_emit (accel_map,
		   accel_map_signals[CHANGED],
		   g_quark_from_string (entry->accel_path),
		   entry->accel_path,
		   entry->accel_key,
		   entry->accel_mods);
}

#if defined (G_OS_WIN32) && !defined (_WIN64)

#undef gtk_accel_map_load

void
gtk_accel_map_load (const gchar *file_name)
{
  gchar *utf8_file_name = g_locale_to_utf8 (file_name, -1, NULL, NULL, NULL);

  gtk_accel_map_load_utf8 (utf8_file_name);

  g_free (utf8_file_name);
}

#undef gtk_accel_map_save

void
gtk_accel_map_save (const gchar *file_name)
{
  gchar *utf8_file_name = g_locale_to_utf8 (file_name, -1, NULL, NULL, NULL);

  gtk_accel_map_save_utf8 (utf8_file_name);

  g_free (utf8_file_name);
}

#endif

#define __GTK_ACCEL_MAP_C__
#include "gtkaliasdef.c"
