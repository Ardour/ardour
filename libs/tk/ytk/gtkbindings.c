/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * GtkBindingSet: Keybinding manager for GtkObjects.
 * Copyright (C) 1998 Tim Janik
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
#include <string.h>
#include <stdarg.h>
#include <gdkkeysyms.h>

#include "gtkbindings.h"
#include "gtkkeyhash.h"
#include "gtkwidget.h"
#include "gtkrc.h"
#include "gtkalias.h"


/* --- defines --- */
#define	BINDING_MOD_MASK()	(gtk_accelerator_get_default_mod_mask () | GDK_RELEASE_MASK)


/* --- structures --- */
typedef struct {
  GtkPathType   type;
  GPatternSpec *pspec;
  GSList       *path;
  gpointer      user_data;
  guint         seq_id;
} PatternSpec;


/* --- variables --- */
static GHashTable	*binding_entry_hash_table = NULL;
static GSList           *binding_key_hashes = NULL;
static GSList		*binding_set_list = NULL;
static const gchar	 key_class_binding_set[] = "gtk-class-binding-set";
static GQuark		 key_id_class_binding_set = 0;


/* --- functions --- */
static void
pattern_spec_free (PatternSpec *pspec)
{
  _gtk_rc_free_widget_class_path (pspec->path);
  if (pspec->pspec)
    g_pattern_spec_free (pspec->pspec);
  g_free (pspec);
}

static GtkBindingSignal*
binding_signal_new (const gchar *signal_name,
		    guint	 n_args)
{
  GtkBindingSignal *signal;

  signal = (GtkBindingSignal *) g_slice_alloc0 (sizeof (GtkBindingSignal) + n_args * sizeof (GtkBindingArg));
  signal->next = NULL;
  signal->signal_name = (gchar *)g_intern_string (signal_name);
  signal->n_args = n_args;
  signal->args = (GtkBindingArg *)(signal + 1);
  
  return signal;
}

static void
binding_signal_free (GtkBindingSignal *sig)
{
  guint i;
  
  for (i = 0; i < sig->n_args; i++)
    {
      if (G_TYPE_FUNDAMENTAL (sig->args[i].arg_type) == G_TYPE_STRING)
	g_free (sig->args[i].d.string_data);
    }
  g_slice_free1 (sizeof (GtkBindingSignal) + sig->n_args * sizeof (GtkBindingArg), sig);
}

static guint
binding_entry_hash (gconstpointer  key)
{
  register const GtkBindingEntry *e = key;
  register guint h;

  h = e->keyval;
  h ^= e->modifiers;

  return h;
}

static gint
binding_entries_compare (gconstpointer  a,
			 gconstpointer  b)
{
  register const GtkBindingEntry *ea = a;
  register const GtkBindingEntry *eb = b;

  return (ea->keyval == eb->keyval && ea->modifiers == eb->modifiers);
}

static void
binding_key_hash_insert_entry (GtkKeyHash      *key_hash,
			       GtkBindingEntry *entry)
{
  guint keyval = entry->keyval;
  
  /* We store lowercased accelerators. To deal with this, if <Shift>
   * was specified, uppercase.
   */
  if (entry->modifiers & GDK_SHIFT_MASK)
    {
      if (keyval == GDK_Tab)
	keyval = GDK_ISO_Left_Tab;
      else
	keyval = gdk_keyval_to_upper (keyval);
    }
  
  _gtk_key_hash_add_entry (key_hash, keyval, entry->modifiers & ~GDK_RELEASE_MASK, entry);
}

static void
binding_key_hash_destroy (gpointer data)
{
  GtkKeyHash *key_hash = data;
  
  binding_key_hashes = g_slist_remove (binding_key_hashes, key_hash);
  _gtk_key_hash_free (key_hash);
}

static void
insert_entries_into_key_hash (gpointer key,
			      gpointer value,
			      gpointer data)
{
  GtkKeyHash *key_hash = data;
  GtkBindingEntry *entry = value;

  for (; entry; entry = entry->hash_next)
    binding_key_hash_insert_entry (key_hash, entry);
}

static GtkKeyHash *
binding_key_hash_for_keymap (GdkKeymap *keymap)
{
  static GQuark key_hash_quark = 0;
  GtkKeyHash *key_hash;

  if (!key_hash_quark)
    key_hash_quark = g_quark_from_static_string ("gtk-binding-key-hash");
  
  key_hash = g_object_get_qdata (G_OBJECT (keymap), key_hash_quark);

  if (!key_hash)
    {
      key_hash = _gtk_key_hash_new (keymap, NULL);
      g_object_set_qdata_full (G_OBJECT (keymap), key_hash_quark, key_hash, binding_key_hash_destroy);

      if (binding_entry_hash_table)
	g_hash_table_foreach (binding_entry_hash_table,
			      insert_entries_into_key_hash,
			      key_hash);

      binding_key_hashes = g_slist_prepend (binding_key_hashes, key_hash);
    }

  return key_hash;
}


static GtkBindingEntry*
binding_entry_new (GtkBindingSet  *binding_set,
		   guint           keyval,
		   GdkModifierType modifiers)
{
  GSList *tmp_list;
  GtkBindingEntry *entry;
  
  if (!binding_entry_hash_table)
    binding_entry_hash_table = g_hash_table_new (binding_entry_hash, binding_entries_compare);

  entry = g_new (GtkBindingEntry, 1);
  entry->keyval = keyval;
  entry->modifiers = modifiers;
  entry->binding_set = binding_set,
  entry->destroyed = FALSE;
  entry->in_emission = FALSE;
  entry->marks_unbound = FALSE;
  entry->signals = NULL;

  entry->set_next = binding_set->entries;
  binding_set->entries = entry;

  entry->hash_next = g_hash_table_lookup (binding_entry_hash_table, entry);
  if (entry->hash_next)
    g_hash_table_remove (binding_entry_hash_table, entry->hash_next);
  g_hash_table_insert (binding_entry_hash_table, entry, entry);

  for (tmp_list = binding_key_hashes; tmp_list; tmp_list = tmp_list->next)
    {
      GtkKeyHash *key_hash = tmp_list->data;
      binding_key_hash_insert_entry (key_hash, entry);
    }
  
  return entry;
}

static void
binding_entry_free (GtkBindingEntry *entry)
{
  GtkBindingSignal *sig;

  g_assert (entry->set_next == NULL &&
	    entry->hash_next == NULL &&
	    entry->in_emission == FALSE &&
	    entry->destroyed == TRUE);

  entry->destroyed = FALSE;
  
  sig = entry->signals;
  while (sig)
    {
      GtkBindingSignal *prev;
      
      prev = sig;
      sig = prev->next;
      binding_signal_free (prev);
    }
  g_free (entry);
}

static void
binding_entry_destroy (GtkBindingEntry *entry)
{
  GtkBindingEntry *o_entry;
  register GtkBindingEntry *tmp;
  GtkBindingEntry *begin;
  register GtkBindingEntry *last;
  GSList *tmp_list;

  /* unlink from binding set
   */
  last = NULL;
  tmp = entry->binding_set->entries;
  while (tmp)
    {
      if (tmp == entry)
	{
	  if (last)
	    last->set_next = entry->set_next;
	  else
	    entry->binding_set->entries = entry->set_next;
	  break;
	}
      last = tmp;
      tmp = last->set_next;
    }
  entry->set_next = NULL;
  
  o_entry = g_hash_table_lookup (binding_entry_hash_table, entry);
  begin = o_entry;
  last = NULL;
  tmp = begin;
  while (tmp)
    {
      if (tmp == entry)
	{
	  if (last)
	    last->hash_next = entry->hash_next;
	  else
	    begin = entry->hash_next;
	  break;
	}
      last = tmp;
      tmp = last->hash_next;
    }
  entry->hash_next = NULL;
  
  if (!begin)
    g_hash_table_remove (binding_entry_hash_table, entry);
  else if (begin != o_entry)
    {
      g_hash_table_remove (binding_entry_hash_table, entry);
      g_hash_table_insert (binding_entry_hash_table, begin, begin);
    }

  for (tmp_list = binding_key_hashes; tmp_list; tmp_list = tmp_list->next)
    {
      GtkKeyHash *key_hash = tmp_list->data;
      _gtk_key_hash_remove_entry (key_hash, entry);
    }

  entry->destroyed = TRUE;

  if (!entry->in_emission)
    binding_entry_free (entry);
}

static GtkBindingEntry*
binding_ht_lookup_entry (GtkBindingSet  *set,
			 guint		 keyval,
			 GdkModifierType modifiers)
{
  GtkBindingEntry lookup_entry = { 0 };
  GtkBindingEntry *entry;
  
  if (!binding_entry_hash_table)
    return NULL;
  
  lookup_entry.keyval = keyval;
  lookup_entry.modifiers = modifiers;
  
  entry = g_hash_table_lookup (binding_entry_hash_table, &lookup_entry);
  for (; entry; entry = entry->hash_next)
    if (entry->binding_set == set)
      return entry;

  return NULL;
}

static gboolean
binding_compose_params (GtkObject       *object,
			GtkBindingArg	*args,
			GSignalQuery	*query,
			GValue	       **params_p)
{
  GValue *params;
  const GType *types;
  guint i;
  gboolean valid;
  
  params = g_new0 (GValue, query->n_params + 1);
  *params_p = params;

  /* The instance we emit on is the first object in the array
   */
  g_value_init (params, G_TYPE_OBJECT);
  g_value_set_object (params, G_OBJECT (object));
  params++;
  
  types = query->param_types;
  valid = TRUE;
  for (i = 1; i < query->n_params + 1 && valid; i++)
    {
      GValue tmp_value = { 0, };

      g_value_init (params, *types);

      switch (G_TYPE_FUNDAMENTAL (args->arg_type))
	{
	case G_TYPE_DOUBLE:
	  g_value_init (&tmp_value, G_TYPE_DOUBLE);
	  g_value_set_double (&tmp_value, args->d.double_data);
	  break;
	case G_TYPE_LONG:
	  g_value_init (&tmp_value, G_TYPE_LONG);
	  g_value_set_long (&tmp_value, args->d.long_data);
	  break;
	case G_TYPE_STRING:
	  /* gtk_rc_parse_flags/enum() has fancier parsing for this; we can't call
	   * that since we don't have a GParamSpec, so just do something simple
	   */
	  if (G_TYPE_FUNDAMENTAL (*types) == G_TYPE_ENUM)
	    {
	      GEnumClass *class = G_ENUM_CLASS (g_type_class_ref (*types));
	      
	      valid = FALSE;
	      
	      if (args->arg_type == GTK_TYPE_IDENTIFIER)
		{
		  GEnumValue *enum_value = NULL;
		  enum_value = g_enum_get_value_by_name (class, args->d.string_data);
		  if (!enum_value)
		    enum_value = g_enum_get_value_by_nick (class, args->d.string_data);
		  if (enum_value)
		    {
		      g_value_init (&tmp_value, *types);
		      g_value_set_enum (&tmp_value, enum_value->value);
		      valid = TRUE;
		    }
		}

	      g_type_class_unref (class);
	    }
	  /* This is just a hack for compatibility with GTK+-1.2 where a string
	   * could be used for a single flag value / without the support for multiple
	   * values in gtk_rc_parse_flags(), this isn't very useful.
	   */
	  else if (G_TYPE_FUNDAMENTAL (*types) == G_TYPE_FLAGS)
	    {
	      GFlagsClass *class = G_FLAGS_CLASS (g_type_class_ref (*types));
	      
	      valid = FALSE;
	      
	      if (args->arg_type == GTK_TYPE_IDENTIFIER)
		{
		  GFlagsValue *flags_value = NULL;
		  flags_value = g_flags_get_value_by_name (class, args->d.string_data);
		  if (!flags_value)
		    flags_value = g_flags_get_value_by_nick (class, args->d.string_data);
		  if (flags_value)
		    {
		      g_value_init (&tmp_value, *types);
		      g_value_set_flags (&tmp_value, flags_value->value);
		      valid = TRUE;
		    }
		}

	      g_type_class_unref (class);
	    }
	  else
	    {
	      g_value_init (&tmp_value, G_TYPE_STRING);
	      g_value_set_static_string (&tmp_value, args->d.string_data);
	    }
	  break;
	default:
	  valid = FALSE;
	  break;
	}

      if (valid)
	{
	  if (!g_value_transform (&tmp_value, params))
	    valid = FALSE;

	  g_value_unset (&tmp_value);
	}
      
      types++;
      params++;
      args++;
    }
  
  if (!valid)
    {
      guint j;

      for (j = 0; j < i; j++)
	g_value_unset (&(*params_p)[j]);
      
      g_free (*params_p);
      *params_p = NULL;
    }
  
  return valid;
}

static gboolean
gtk_binding_entry_activate (GtkBindingEntry *entry,
			    GtkObject	    *object)
{
  GtkBindingSignal *sig;
  gboolean old_emission;
  gboolean handled = FALSE;
  gint i;
  
  old_emission = entry->in_emission;
  entry->in_emission = TRUE;
  
  g_object_ref (object);
  
  for (sig = entry->signals; sig; sig = sig->next)
    {
      GSignalQuery query;
      guint signal_id;
      GValue *params = NULL;
      GValue return_val = { 0, };
      gchar *accelerator = NULL;
      
      signal_id = g_signal_lookup (sig->signal_name, G_OBJECT_TYPE (object));
      if (!signal_id)
	{
	  accelerator = gtk_accelerator_name (entry->keyval, entry->modifiers);
	  g_warning ("gtk_binding_entry_activate(): binding \"%s::%s\": "
		     "could not find signal \"%s\" in the `%s' class ancestry",
		     entry->binding_set->set_name,
		     accelerator,
		     sig->signal_name,
		     g_type_name (G_OBJECT_TYPE (object)));
	  g_free (accelerator);
	  continue;
	}
      
      g_signal_query (signal_id, &query);
      if (query.n_params != sig->n_args ||
	  (query.return_type != G_TYPE_NONE && query.return_type != G_TYPE_BOOLEAN) || 
	  !binding_compose_params (object, sig->args, &query, &params))
	{
	  accelerator = gtk_accelerator_name (entry->keyval, entry->modifiers);
	  g_warning ("gtk_binding_entry_activate(): binding \"%s::%s\": "
		     "signature mismatch for signal \"%s\" in the `%s' class ancestry",
		     entry->binding_set->set_name,
		     accelerator,
		     sig->signal_name,
		     g_type_name (G_OBJECT_TYPE (object)));
	}
      else if (!(query.signal_flags & G_SIGNAL_ACTION))
	{
	  accelerator = gtk_accelerator_name (entry->keyval, entry->modifiers);
	  g_warning ("gtk_binding_entry_activate(): binding \"%s::%s\": "
		     "signal \"%s\" in the `%s' class ancestry cannot be used for action emissions",
		     entry->binding_set->set_name,
		     accelerator,
		     sig->signal_name,
		     g_type_name (G_OBJECT_TYPE (object)));
	}
      g_free (accelerator);
      if (accelerator)
	continue;

      if (query.return_type == G_TYPE_BOOLEAN)
	g_value_init (&return_val, G_TYPE_BOOLEAN);
      
      g_signal_emitv (params, signal_id, 0, &return_val);

      if (query.return_type == G_TYPE_BOOLEAN)
	{
	  if (g_value_get_boolean (&return_val))
	    handled = TRUE;
	  g_value_unset (&return_val);
	}
      else
	handled = TRUE;
      
      for (i = 0; i < query.n_params + 1; i++)
	g_value_unset (&params[i]);
      g_free (params);
      
      if (entry->destroyed)
	break;
    }
  
  g_object_unref (object);

  entry->in_emission = old_emission;
  if (entry->destroyed && !entry->in_emission)
    binding_entry_free (entry);

  return handled;
}

/**
 * gtk_binding_set_new:
 * @set_name: unique name of this binding set
 *
 * GTK+ maintains a global list of binding sets. Each binding set has
 * a unique name which needs to be specified upon creation.
 *
 * Return value: new binding set
 */
GtkBindingSet*
gtk_binding_set_new (const gchar *set_name)
{
  GtkBindingSet *binding_set;
  
  g_return_val_if_fail (set_name != NULL, NULL);
  
  binding_set = g_new (GtkBindingSet, 1);
  binding_set->set_name = (gchar *) g_intern_string (set_name);
  binding_set->widget_path_pspecs = NULL;
  binding_set->widget_class_pspecs = NULL;
  binding_set->class_branch_pspecs = NULL;
  binding_set->entries = NULL;
  binding_set->current = NULL;
  binding_set->parsed = FALSE;
  
  binding_set_list = g_slist_prepend (binding_set_list, binding_set);
  
  return binding_set;
}

/**
 * gtk_binding_set_by_class:
 * @object_class: a valid #GtkObject class
 *
 * This function returns the binding set named after the type name of
 * the passed in class structure. New binding sets are created on
 * demand by this function.
 *
 * Return value: the binding set corresponding to @object_class
 */
GtkBindingSet*
gtk_binding_set_by_class (gpointer object_class)
{
  GtkObjectClass *class = object_class;
  GtkBindingSet* binding_set;

  g_return_val_if_fail (GTK_IS_OBJECT_CLASS (class), NULL);

  if (!key_id_class_binding_set)
    key_id_class_binding_set = g_quark_from_static_string (key_class_binding_set);

  binding_set = g_dataset_id_get_data (class, key_id_class_binding_set);

  if (binding_set)
    return binding_set;

  binding_set = gtk_binding_set_new (g_type_name (G_OBJECT_CLASS_TYPE (class)));
  gtk_binding_set_add_path (binding_set,
			    GTK_PATH_CLASS,
			    g_type_name (G_OBJECT_CLASS_TYPE (class)),
			    GTK_PATH_PRIO_GTK);
  g_dataset_id_set_data (class, key_id_class_binding_set, binding_set);

  return binding_set;
}

/**
 * gtk_binding_set_find:
 * @set_name: unique binding set name
 *
 * Find a binding set by its globally unique name. The @set_name can
 * either be a name used for gtk_binding_set_new() or the type name of
 * a class used in gtk_binding_set_by_class().
 *
 * Return value: (transfer none): %NULL or the specified binding set
 */
GtkBindingSet*
gtk_binding_set_find (const gchar *set_name)
{
  GSList *slist;
  
  g_return_val_if_fail (set_name != NULL, NULL);
  
  for (slist = binding_set_list; slist; slist = slist->next)
    {
      GtkBindingSet *binding_set;
      
      binding_set = slist->data;
      if (g_str_equal (binding_set->set_name, (gpointer) set_name))
	return binding_set;
    }
  return NULL;
}

/**
 * gtk_binding_set_activate:
 * @binding_set: a #GtkBindingSet set to activate
 * @keyval:      key value of the binding
 * @modifiers:   key modifier of the binding
 * @object:      object to activate when binding found
 *
 * Find a key binding matching @keyval and @modifiers within
 * @binding_set and activate the binding on @object.
 *
 * Return value: %TRUE if a binding was found and activated
 */
gboolean
gtk_binding_set_activate (GtkBindingSet	 *binding_set,
			  guint		  keyval,
			  GdkModifierType modifiers,
			  GtkObject	 *object)
{
  GtkBindingEntry *entry;
  
  g_return_val_if_fail (binding_set != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_OBJECT (object), FALSE);
  
  keyval = gdk_keyval_to_lower (keyval);
  modifiers = modifiers & BINDING_MOD_MASK ();
  
  entry = binding_ht_lookup_entry (binding_set, keyval, modifiers);
  if (entry)
    return gtk_binding_entry_activate (entry, object);
  
  return FALSE;
}

static void
gtk_binding_entry_clear_internal (GtkBindingSet  *binding_set,
                                  guint           keyval,
                                  GdkModifierType modifiers)
{
  GtkBindingEntry *entry;

  keyval = gdk_keyval_to_lower (keyval);
  modifiers = modifiers & BINDING_MOD_MASK ();

  entry = binding_ht_lookup_entry (binding_set, keyval, modifiers);
  if (entry)
    binding_entry_destroy (entry);

  entry = binding_entry_new (binding_set, keyval, modifiers);
}

/**
 * gtk_binding_entry_add:
 * @binding_set: #a GtkBindingSet to clear an entry of
 * @keyval:      key value of binding to clear
 * @modifiers:   key modifier of binding to clear
 *
 * Adds a binding entry.
 *
 * Deprecated: 2.12: Use gtk_binding_entry_add_signal() instead.
 */

/**
 * gtk_binding_entry_clear:
 * @binding_set: binding set to clear an entry of
 * @keyval:      key value of binding to clear
 * @modifiers:   key modifier of binding to clear
 *
 * Clears a binding entry.
 *
 * Deprecated: 2.12: Use gtk_binding_entry_remove() instead.
 */
void
gtk_binding_entry_clear (GtkBindingSet	*binding_set,
			 guint		 keyval,
			 GdkModifierType modifiers)
{
  g_return_if_fail (binding_set != NULL);

  gtk_binding_entry_clear_internal (binding_set, keyval, modifiers);
}

/**
 * gtk_binding_entry_skip:
 * @binding_set: a #GtkBindingSet to skip an entry of
 * @keyval:      key value of binding to skip
 * @modifiers:   key modifier of binding to skip
 *
 * Install a binding on @binding_set which causes key lookups
 * to be aborted, to prevent bindings from lower priority sets
 * to be activated.
 *
 * Since: 2.12
 */
void
gtk_binding_entry_skip (GtkBindingSet  *binding_set,
                        guint           keyval,
                        GdkModifierType modifiers)
{
  GtkBindingEntry *entry;

  g_return_if_fail (binding_set != NULL);

  keyval = gdk_keyval_to_lower (keyval);
  modifiers = modifiers & BINDING_MOD_MASK ();

  entry = binding_ht_lookup_entry (binding_set, keyval, modifiers);
  if (entry)
    binding_entry_destroy (entry);

  entry = binding_entry_new (binding_set, keyval, modifiers);
  entry->marks_unbound = TRUE;
}

/**
 * gtk_binding_entry_remove:
 * @binding_set: a #GtkBindingSet to remove an entry of
 * @keyval:      key value of binding to remove
 * @modifiers:   key modifier of binding to remove
 *
 * Remove a binding previously installed via
 * gtk_binding_entry_add_signal() on @binding_set.
 */
void
gtk_binding_entry_remove (GtkBindingSet	 *binding_set,
			  guint		  keyval,
			  GdkModifierType modifiers)
{
  GtkBindingEntry *entry;
  
  g_return_if_fail (binding_set != NULL);
  
  keyval = gdk_keyval_to_lower (keyval);
  modifiers = modifiers & BINDING_MOD_MASK ();
  
  entry = binding_ht_lookup_entry (binding_set, keyval, modifiers);
  if (entry)
    binding_entry_destroy (entry);
}

/**
 * gtk_binding_entry_add_signall:
 * @binding_set:  a #GtkBindingSet to add a signal to
 * @keyval:       key value
 * @modifiers:    key modifier
 * @signal_name:  signal name to be bound
 * @binding_args: (transfer none) (element-type GtkBindingArg):
 *     list of #GtkBindingArg signal arguments
 *
 * Override or install a new key binding for @keyval with @modifiers on
 * @binding_set.
 */
void
gtk_binding_entry_add_signall (GtkBindingSet  *binding_set,
                               guint	       keyval,
                               GdkModifierType modifiers,
                               const gchar    *signal_name,
                               GSList	      *binding_args)
{
  _gtk_binding_entry_add_signall (binding_set,
                                  keyval, modifiers,
                                  signal_name, binding_args);
}

void
_gtk_binding_entry_add_signall (GtkBindingSet  *binding_set,
                                guint	       keyval,
                                GdkModifierType modifiers,
                                const gchar    *signal_name,
                                GSList	      *binding_args)
{
  GtkBindingEntry *entry;
  GtkBindingSignal *signal, **signal_p;
  GSList *slist;
  guint n = 0;
  GtkBindingArg *arg;
  
  g_return_if_fail (binding_set != NULL);
  g_return_if_fail (signal_name != NULL);
  
  keyval = gdk_keyval_to_lower (keyval);
  modifiers = modifiers & BINDING_MOD_MASK ();
  
  signal = binding_signal_new (signal_name, g_slist_length (binding_args));
  
  arg = signal->args;
  for (slist = binding_args; slist; slist = slist->next)
    {
      GtkBindingArg *tmp_arg;
      
      tmp_arg = slist->data;
      if (!tmp_arg)
	{
	  g_warning ("gtk_binding_entry_add_signall(): arg[%u] is `NULL'", n);
	  binding_signal_free (signal);
	  return;
	}
      switch (G_TYPE_FUNDAMENTAL (tmp_arg->arg_type))
	{
	case  G_TYPE_LONG:
	  arg->arg_type = G_TYPE_LONG;
	  arg->d.long_data = tmp_arg->d.long_data;
	  break;
	case  G_TYPE_DOUBLE:
	  arg->arg_type = G_TYPE_DOUBLE;
	  arg->d.double_data = tmp_arg->d.double_data;
	  break;
	case  G_TYPE_STRING:
          if (tmp_arg->arg_type != GTK_TYPE_IDENTIFIER)
	    arg->arg_type = G_TYPE_STRING;
	  else
	    arg->arg_type = GTK_TYPE_IDENTIFIER;
	  arg->d.string_data = g_strdup (tmp_arg->d.string_data);
	  if (!arg->d.string_data)
	    {
	      g_warning ("gtk_binding_entry_add_signall(): value of `string' arg[%u] is `NULL'", n);
	      binding_signal_free (signal);
	      return;
	    }
	  break;
	default:
	  g_warning ("gtk_binding_entry_add_signall(): unsupported type `%s' for arg[%u]",
		     g_type_name (arg->arg_type), n);
	  binding_signal_free (signal);
	  return;
	}
      arg++;
      n++;
    }
  
  entry = binding_ht_lookup_entry (binding_set, keyval, modifiers);
  if (!entry)
    {
      gtk_binding_entry_clear_internal (binding_set, keyval, modifiers);
      entry = binding_ht_lookup_entry (binding_set, keyval, modifiers);
    }
  signal_p = &entry->signals;
  while (*signal_p)
    signal_p = &(*signal_p)->next;
  *signal_p = signal;
}

/**
 * gtk_binding_entry_add_signal:
 * @binding_set: a #GtkBindingSet to install an entry for
 * @keyval:      key value of binding to install
 * @modifiers:   key modifier of binding to install
 * @signal_name: signal to execute upon activation
 * @n_args:      number of arguments to @signal_name
 * @Varargs:     arguments to @signal_name
 *
 * Override or install a new key binding for @keyval with @modifiers on
 * @binding_set. When the binding is activated, @signal_name will be
 * emitted on the target widget, with @n_args @Varargs used as
 * arguments.
 */
void
gtk_binding_entry_add_signal (GtkBindingSet  *binding_set,
			      guint           keyval,
			      GdkModifierType modifiers,
			      const gchar    *signal_name,
			      guint           n_args,
			      ...)
{
  GSList *slist, *free_slist;
  va_list args;
  guint i;

  g_return_if_fail (binding_set != NULL);
  g_return_if_fail (signal_name != NULL);
  
  va_start (args, n_args);
  slist = NULL;
  for (i = 0; i < n_args; i++)
    {
      GtkBindingArg *arg;

      arg = g_slice_new0 (GtkBindingArg);
      slist = g_slist_prepend (slist, arg);

      arg->arg_type = va_arg (args, GType);
      switch (G_TYPE_FUNDAMENTAL (arg->arg_type))
	{
	case G_TYPE_CHAR:
	case G_TYPE_UCHAR:
	case G_TYPE_INT:
	case G_TYPE_UINT:
	case G_TYPE_BOOLEAN:
	case G_TYPE_ENUM:
	case G_TYPE_FLAGS:
	  arg->arg_type = G_TYPE_LONG;
	  arg->d.long_data = va_arg (args, gint);
	  break;
	case G_TYPE_LONG:
	case G_TYPE_ULONG:
	  arg->arg_type = G_TYPE_LONG;
	  arg->d.long_data = va_arg (args, glong);
	  break;
	case G_TYPE_FLOAT:
	case G_TYPE_DOUBLE:
	  arg->arg_type = G_TYPE_DOUBLE;
	  arg->d.double_data = va_arg (args, gdouble);
	  break;
	case G_TYPE_STRING:
	  if (arg->arg_type != GTK_TYPE_IDENTIFIER)
	    arg->arg_type = G_TYPE_STRING;
	  arg->d.string_data = va_arg (args, gchar*);
	  if (!arg->d.string_data)
	    {
	      g_warning ("gtk_binding_entry_add_signal(): type `%s' arg[%u] is `NULL'",
			 g_type_name (arg->arg_type),
			 i);
	      i += n_args + 1;
	    }
	  break;
	default:
	  g_warning ("gtk_binding_entry_add_signal(): unsupported type `%s' for arg[%u]",
		     g_type_name (arg->arg_type), i);
	  i += n_args + 1;
	  break;
	}
    }
  va_end (args);

  if (i == n_args || i == 0)
    {
      slist = g_slist_reverse (slist);
      _gtk_binding_entry_add_signall (binding_set, keyval, modifiers, signal_name, slist);
    }

  free_slist = slist;
  while (slist)
    {
      g_slice_free (GtkBindingArg, slist->data);
      slist = slist->next;
    }
  g_slist_free (free_slist);
}

/**
 * gtk_binding_set_add_path:
 * @binding_set:  a #GtkBindingSet to add a path to
 * @path_type:    path type the pattern applies to
 * @path_pattern: the actual match pattern
 * @priority:     binding priority
 *
 * This function is used internally by the GtkRC parsing mechanism to
 * assign match patterns to #GtkBindingSet structures.
 */
void
gtk_binding_set_add_path (GtkBindingSet	     *binding_set,
			  GtkPathType	      path_type,
			  const gchar	     *path_pattern,
			  GtkPathPriorityType priority)
{
  PatternSpec *pspec;
  GSList **slist_p, *slist;
  static guint seq_id = 0;
  
  g_return_if_fail (binding_set != NULL);
  g_return_if_fail (path_pattern != NULL);
  g_return_if_fail (priority <= GTK_PATH_PRIO_MASK);

  priority &= GTK_PATH_PRIO_MASK;
  
  switch (path_type)
    {
    case  GTK_PATH_WIDGET:
      slist_p = &binding_set->widget_path_pspecs;
      break;
    case  GTK_PATH_WIDGET_CLASS:
      slist_p = &binding_set->widget_class_pspecs;
      break;
    case  GTK_PATH_CLASS:
      slist_p = &binding_set->class_branch_pspecs;
      break;
    default:
      g_assert_not_reached ();
      slist_p = NULL;
      break;
    }
  
  pspec = g_new (PatternSpec, 1);
  pspec->type = path_type;
  if (path_type == GTK_PATH_WIDGET_CLASS)
    {
      pspec->pspec = NULL;
      pspec->path = _gtk_rc_parse_widget_class_path (path_pattern);
    }
  else
    {
      pspec->pspec = g_pattern_spec_new (path_pattern);
      pspec->path = NULL;
    }
    
  pspec->seq_id = priority << 28;
  pspec->user_data = binding_set;
  
  slist = *slist_p;
  while (slist)
    {
      PatternSpec *tmp_pspec;
      
      tmp_pspec = slist->data;
      slist = slist->next;
      
      if (g_pattern_spec_equal (tmp_pspec->pspec, pspec->pspec))
	{
	  GtkPathPriorityType lprio = tmp_pspec->seq_id >> 28;

	  pattern_spec_free (pspec);
	  pspec = NULL;
	  if (lprio < priority)
	    {
	      tmp_pspec->seq_id &= 0x0fffffff;
	      tmp_pspec->seq_id |= priority << 28;
	    }
	  break;
	}
    }
  if (pspec)
    {
      pspec->seq_id |= seq_id++ & 0x0fffffff;
      *slist_p = g_slist_prepend (*slist_p, pspec);
    }
}

static gboolean
binding_match_activate (GSList          *pspec_list,
			GtkObject	*object,
			guint	         path_length,
			gchar           *path,
			gchar           *path_reversed,
                        gboolean        *unbound)
{
  GSList *slist;

  *unbound = FALSE;

  for (slist = pspec_list; slist; slist = slist->next)
    {
      PatternSpec *pspec;
      GtkBindingSet *binding_set;

      binding_set = NULL;
      pspec = slist->data;
      
      if (pspec->type == GTK_PATH_WIDGET_CLASS)
        {
          if (_gtk_rc_match_widget_class (pspec->path, path_length, path, path_reversed))
	    binding_set = pspec->user_data;
        }
      else
        {
          if (g_pattern_match (pspec->pspec, path_length, path, path_reversed))
	    binding_set = pspec->user_data;
        }

      if (binding_set)
        {
          if (binding_set->current->marks_unbound)
            {
              *unbound = TRUE;
              return FALSE;
            }

          if (gtk_binding_entry_activate (binding_set->current, object))
            return TRUE;
        }
    }

  return FALSE;
}

static gint
gtk_binding_pattern_compare (gconstpointer new_pattern,
			     gconstpointer existing_pattern)
{
  register const PatternSpec *np  = new_pattern;
  register const PatternSpec *ep  = existing_pattern;

  /* walk the list as long as the existing patterns have
   * higher priorities.
   */

  return np->seq_id < ep->seq_id;
}

static GSList*
gtk_binding_entries_sort_patterns (GSList      *entries,
				   GtkPathType  path_id,
				   gboolean     is_release)
{
  GSList *patterns;
  GSList *tmp_list;

  patterns = NULL;
  for (tmp_list = entries; tmp_list; tmp_list = tmp_list->next)
    {
      GtkBindingEntry *entry = tmp_list->data;
      GtkBindingSet *binding_set;

      binding_set = entry->binding_set;
      binding_set->current = NULL;
    }
  
  for (; entries; entries = entries->next)
    {
      GtkBindingEntry *entry = entries->data;
      GtkBindingSet *binding_set;
      GSList *slist = NULL;

      if (is_release != ((entry->modifiers & GDK_RELEASE_MASK) != 0))
	continue;

      binding_set = entry->binding_set;

      if (binding_set->current)
	continue;
      binding_set->current = entry;

      switch (path_id)
	{
	case GTK_PATH_WIDGET:
	  slist = binding_set->widget_path_pspecs;
	  break;
	case GTK_PATH_WIDGET_CLASS:
	  slist = binding_set->widget_class_pspecs;
	  break;
	case GTK_PATH_CLASS:
	  slist = binding_set->class_branch_pspecs;
	  break;
	}

      for (; slist; slist = slist->next)
	{
	  PatternSpec *pspec;

	  pspec = slist->data;
	  patterns = g_slist_insert_sorted (patterns, pspec, gtk_binding_pattern_compare);
	}
    }

  return patterns;
}

static gboolean
gtk_bindings_activate_list (GtkObject *object,
			    GSList    *entries,
			    gboolean   is_release)
{
  GtkWidget *widget = GTK_WIDGET (object);
  gboolean handled = FALSE;

  if (!entries)
    return FALSE;

  if (!handled)
    {
      guint path_length;
      gchar *path, *path_reversed;
      GSList *patterns;
      gboolean unbound;

      gtk_widget_path (widget, &path_length, &path, &path_reversed);
      patterns = gtk_binding_entries_sort_patterns (entries, GTK_PATH_WIDGET, is_release);
      handled = binding_match_activate (patterns, object, path_length, path, path_reversed, &unbound);
      g_slist_free (patterns);
      g_free (path);
      g_free (path_reversed);

      if (unbound)
        return FALSE;
    }

  if (!handled)
    {
      guint path_length;
      gchar *path, *path_reversed;
      GSList *patterns;
      gboolean unbound;

      gtk_widget_class_path (widget, &path_length, &path, &path_reversed);
      patterns = gtk_binding_entries_sort_patterns (entries, GTK_PATH_WIDGET_CLASS, is_release);
      handled = binding_match_activate (patterns, object, path_length, path, path_reversed, &unbound);
      g_slist_free (patterns);
      g_free (path);
      g_free (path_reversed);

      if (unbound)
        return FALSE;
    }

  if (!handled)
    {
      GSList *patterns;
      GType class_type;
      gboolean unbound = FALSE;

      patterns = gtk_binding_entries_sort_patterns (entries, GTK_PATH_CLASS, is_release);
      class_type = G_TYPE_FROM_INSTANCE (object);
      while (class_type && !handled)
	{
	  guint path_length;
	  gchar *path;
	  gchar *path_reversed;

	  path = g_strdup (g_type_name (class_type));
	  path_reversed = g_strdup (path);
	  g_strreverse (path_reversed);
	  path_length = strlen (path);
	  handled = binding_match_activate (patterns, object, path_length, path, path_reversed, &unbound);
	  g_free (path);
	  g_free (path_reversed);

          if (unbound)
            break;

	  class_type = g_type_parent (class_type);
	}
      g_slist_free (patterns);

      if (unbound)
        return FALSE;
    }

  return handled;
}

/**
 * gtk_bindings_activate:
 * @object: object to activate when binding found
 * @keyval: key value of the binding
 * @modifiers: key modifier of the binding
 *
 * Find a key binding matching @keyval and @modifiers and activate the
 * binding on @object.
 *
 * Return value: %TRUE if a binding was found and activated
 */
gboolean
gtk_bindings_activate (GtkObject       *object,
		       guint	        keyval,
		       GdkModifierType  modifiers)
{
  GSList *entries = NULL;
  GdkDisplay *display;
  GtkKeyHash *key_hash;
  gboolean handled = FALSE;
  gboolean is_release;

  g_return_val_if_fail (GTK_IS_OBJECT (object), FALSE);

  if (!GTK_IS_WIDGET (object))
    return FALSE;

  is_release = (modifiers & GDK_RELEASE_MASK) != 0;
  modifiers = modifiers & BINDING_MOD_MASK () & ~GDK_RELEASE_MASK;

  display = gtk_widget_get_display (GTK_WIDGET (object));
  key_hash = binding_key_hash_for_keymap (gdk_keymap_get_for_display (display));
  
  entries = _gtk_key_hash_lookup_keyval (key_hash, keyval, modifiers);

  handled = gtk_bindings_activate_list (object, entries, is_release);

  g_slist_free (entries);

  return handled;
}

/**
 * gtk_bindings_activate_event:
 * @object: a #GtkObject (generally must be a widget)
 * @event: a #GdkEventKey
 * 
 * Looks up key bindings for @object to find one matching
 * @event, and if one was found, activate it.
 * 
 * Return value: %TRUE if a matching key binding was found
 *
 * Since: 2.4
 */
gboolean
gtk_bindings_activate_event (GtkObject   *object,
                             GdkEventKey *event)
{
  GSList *entries = NULL;
  GdkDisplay *display;
  GtkKeyHash *key_hash;
  gboolean handled = FALSE;

  g_return_val_if_fail (GTK_IS_OBJECT (object), FALSE);

  if (!GTK_IS_WIDGET (object))
    return FALSE;

  display = gtk_widget_get_display (GTK_WIDGET (object));
  key_hash = binding_key_hash_for_keymap (gdk_keymap_get_for_display (display));

  entries = _gtk_key_hash_lookup (key_hash,
				  event->hardware_keycode,
				  event->state,
				  BINDING_MOD_MASK () & ~GDK_RELEASE_MASK,
				  event->group);
  
  handled = gtk_bindings_activate_list (object, entries,
					event->type == GDK_KEY_RELEASE);

  g_slist_free (entries);

  return handled;
}

static guint
gtk_binding_parse_signal (GScanner       *scanner,
			  GtkBindingSet  *binding_set,
			  guint		  keyval,
			  GdkModifierType modifiers)
{
  gchar *signal;
  guint expected_token = 0;
  GSList *args;
  GSList *slist;
  gboolean done;
  gboolean negate;
  gboolean need_arg;
  gboolean seen_comma;

  g_return_val_if_fail (scanner != NULL, G_TOKEN_ERROR);
  
  g_scanner_get_next_token (scanner);
  if (scanner->token != G_TOKEN_STRING)
    return G_TOKEN_STRING;
  g_scanner_peek_next_token (scanner);
  if (scanner->next_token != '(')
    {
      g_scanner_get_next_token (scanner);
      return '(';
    }
  signal = g_strdup (scanner->value.v_string);
  g_scanner_get_next_token (scanner);

  negate = FALSE;
  args = NULL;
  done = FALSE;
  need_arg = TRUE;
  seen_comma = FALSE;
  scanner->config->scan_symbols = FALSE;
  do
    {
      if (need_arg)
	expected_token = G_TOKEN_INT;
      else
	expected_token = ')';
      g_scanner_get_next_token (scanner);
      switch ((guint) scanner->token)
	{
	  GtkBindingArg *arg;

	case G_TOKEN_FLOAT:
	  if (need_arg)
	    {
	      need_arg = FALSE;
	      arg = g_new (GtkBindingArg, 1);
	      arg->arg_type = G_TYPE_DOUBLE;
	      arg->d.double_data = scanner->value.v_float;
	      if (negate)
		{
		  arg->d.double_data = - arg->d.double_data;
		  negate = FALSE;
		}
	      args = g_slist_prepend (args, arg);
	    }
	  else
	    done = TRUE;
	  break;
	case G_TOKEN_INT:
	  if (need_arg)
	    {
	      need_arg = FALSE;
	      arg = g_new (GtkBindingArg, 1);
	      arg->arg_type = G_TYPE_LONG;
	      arg->d.long_data = scanner->value.v_int;
	      if (negate)
		{
		  arg->d.long_data = - arg->d.long_data;
		  negate = FALSE;
		}
	      args = g_slist_prepend (args, arg);
	    }
          else
	    done = TRUE;
	  break;
	case G_TOKEN_STRING:
	  if (need_arg && !negate)
	    {
	      need_arg = FALSE;
	      arg = g_new (GtkBindingArg, 1);
	      arg->arg_type = G_TYPE_STRING;
	      arg->d.string_data = g_strdup (scanner->value.v_string);
	      args = g_slist_prepend (args, arg);
	    }
	  else
	    done = TRUE;
	  break;
	case G_TOKEN_IDENTIFIER:
	  if (need_arg && !negate)
	    {
	      need_arg = FALSE;
	      arg = g_new (GtkBindingArg, 1);
	      arg->arg_type = GTK_TYPE_IDENTIFIER;
	      arg->d.string_data = g_strdup (scanner->value.v_identifier);
	      args = g_slist_prepend (args, arg);
	    }
	  else
	    done = TRUE;
	  break;
	case '-':
	  if (!need_arg)
	    done = TRUE;
	  else if (negate)
	    {
	      expected_token = G_TOKEN_INT;
	      done = TRUE;
	    }
	  else
	    negate = TRUE;
	  break;
	case ',':
	  seen_comma = TRUE;
	  if (need_arg)
	    done = TRUE;
	  else
	    need_arg = TRUE;
	  break;
	case ')':
	  if (!(need_arg && seen_comma) && !negate)
	    {
	      args = g_slist_reverse (args);
	      _gtk_binding_entry_add_signall (binding_set,
                                              keyval,
                                              modifiers,
                                              signal,
                                              args);
	      expected_token = G_TOKEN_NONE;
	    }
	  done = TRUE;
	  break;
	default:
	  done = TRUE;
	  break;
	}
    }
  while (!done);
  scanner->config->scan_symbols = TRUE;
  
  for (slist = args; slist; slist = slist->next)
    {
      GtkBindingArg *arg;

      arg = slist->data;
      if (G_TYPE_FUNDAMENTAL (arg->arg_type) == G_TYPE_STRING)
	g_free (arg->d.string_data);
      g_free (arg);
    }
  g_slist_free (args);
  g_free (signal);

  return expected_token;
}

static inline guint
gtk_binding_parse_bind (GScanner       *scanner,
			GtkBindingSet  *binding_set)
{
  guint keyval = 0;
  GdkModifierType modifiers = 0;
  gboolean unbind = FALSE;

  g_return_val_if_fail (scanner != NULL, G_TOKEN_ERROR);
  
  g_scanner_get_next_token (scanner);
  if (scanner->token != (guint) GTK_RC_TOKEN_BIND &&
      scanner->token != (guint) GTK_RC_TOKEN_UNBIND)
    return GTK_RC_TOKEN_BIND;
  unbind = scanner->token == (guint) GTK_RC_TOKEN_UNBIND;
  g_scanner_get_next_token (scanner);
  if (scanner->token != (guint) G_TOKEN_STRING)
    return G_TOKEN_STRING;
  gtk_accelerator_parse (scanner->value.v_string, &keyval, &modifiers);
  modifiers &= BINDING_MOD_MASK ();
  if (keyval == 0)
    return G_TOKEN_STRING;

  if (unbind)
    {
      gtk_binding_entry_skip (binding_set, keyval, modifiers);
      return G_TOKEN_NONE;
    }

  g_scanner_get_next_token (scanner);

  if (scanner->token != '{')
    return '{';

  gtk_binding_entry_clear_internal (binding_set, keyval, modifiers);

  g_scanner_peek_next_token (scanner);
  while (scanner->next_token != '}')
    {
      switch (scanner->next_token)
	{
	  guint expected_token;

	case G_TOKEN_STRING:
	  expected_token = gtk_binding_parse_signal (scanner,
						     binding_set,
						     keyval,
						     modifiers);
	  if (expected_token != G_TOKEN_NONE)
	    return expected_token;
	  break;
	default:
	  g_scanner_get_next_token (scanner);
	  return '}';
	}
      g_scanner_peek_next_token (scanner);
    }
  g_scanner_get_next_token (scanner);

  return G_TOKEN_NONE;
}

/**
 * gtk_binding_parse_binding:
 * @scanner: GtkRC scanner
 *
 * Parse a binding entry from a gtkrc file.
 *
 * Return value: expected token upon errors, %G_TOKEN_NONE on success.
 *
 * Deprecated: 2.12: There should be no need to call this function outside GTK+.
 */
guint
gtk_binding_parse_binding (GScanner *scanner)
{
  return _gtk_binding_parse_binding (scanner);
}

guint
_gtk_binding_parse_binding (GScanner *scanner)
{
  gchar *name;
  GtkBindingSet *binding_set;

  g_return_val_if_fail (scanner != NULL, G_TOKEN_ERROR);

  g_scanner_get_next_token (scanner);
  if (scanner->token != (guint) GTK_RC_TOKEN_BINDING)
    return GTK_RC_TOKEN_BINDING;
  g_scanner_get_next_token (scanner);
  if (scanner->token != (guint) G_TOKEN_STRING)
    return G_TOKEN_STRING;
  name = g_strdup (scanner->value.v_string);

  g_scanner_get_next_token (scanner);
  if (scanner->token != '{')
    {
      g_free (name);
      return G_TOKEN_STRING;
    }

  binding_set = gtk_binding_set_find (name);
  if (!binding_set)
    {
      binding_set = gtk_binding_set_new (name);
      binding_set->parsed = 1;
    }
  g_free (name);

  g_scanner_peek_next_token (scanner);
  while (scanner->next_token != '}')
    {
      switch ((guint) scanner->next_token)
	{
	  guint expected_token;

	case GTK_RC_TOKEN_BIND:
	case GTK_RC_TOKEN_UNBIND:
	  expected_token = gtk_binding_parse_bind (scanner, binding_set);
	  if (expected_token != G_TOKEN_NONE)
	    return expected_token;
	  break;
	default:
	  g_scanner_get_next_token (scanner);
	  return '}';
	}
      g_scanner_peek_next_token (scanner);
    }
  g_scanner_get_next_token (scanner);

  return G_TOKEN_NONE;
}

static void
free_pattern_specs (GSList *pattern_specs)
{
  GSList *slist;

  for (slist = pattern_specs; slist; slist = slist->next)
    {
      PatternSpec *pspec;

      pspec = slist->data;

      pattern_spec_free (pspec);
    }

  g_slist_free (pattern_specs);
}

static void
binding_set_delete (GtkBindingSet *binding_set)
{
  GtkBindingEntry *entry, *next;

  entry = binding_set->entries;
  while (entry)
    {
      next = entry->set_next;
      binding_entry_destroy (entry);
      entry = next;
    }
  
  free_pattern_specs (binding_set->widget_path_pspecs);
  free_pattern_specs (binding_set->widget_class_pspecs);
  free_pattern_specs (binding_set->class_branch_pspecs);

  g_free (binding_set);
}

/**
 * _gtk_binding_reset_parsed:
 * 
 * Remove all binding sets that were added by gtk_binding_parse_binding().
 */
void
_gtk_binding_reset_parsed (void)
{
  GSList *slist, *next;
  
  slist = binding_set_list;
  while (slist)
    {
      GtkBindingSet *binding_set;

      binding_set = slist->data;
      next = slist->next;

      if (binding_set->parsed)
	{
	  binding_set_list = g_slist_delete_link (binding_set_list, slist);
	  binding_set_delete (binding_set);
	}

      slist = next;
    }
}

#define __GTK_BINDINGS_C__
#include "gtkaliasdef.c"
