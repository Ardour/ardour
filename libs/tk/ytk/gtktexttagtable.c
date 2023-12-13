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
#include "gtktexttagtable.h"
#include "gtkbuildable.h"
#include "gtkmarshalers.h"
#include "gtktextbuffer.h" /* just for the lame notify_will_remove_tag hack */
#include "gtkintl.h"
#include "gtkalias.h"

#include <stdlib.h>

/**
 * SECTION:gtktexttagtable
 * @Short_description: Collection of tags that can be used together
 * @Title: GtkTextTagTable
 *
 * You may wish to begin by reading the <link linkend="TextWidget">text widget
 * conceptual overview</link> which gives an overview of all the objects and data
 * types related to the text widget and how they work together.
 *
 * <refsect2 id="GtkTextTagTable-BUILDER-UI">
 * <title>GtkTextTagTables as GtkBuildable</title>
 * <para>
 * The GtkTextTagTable implementation of the GtkBuildable interface
 * supports adding tags by specifying "tag" as the "type"
 * attribute of a &lt;child&gt; element.
 * </para>
 * <example>
 * <title>A UI definition fragment specifying tags</title>
 * <programlisting><![CDATA[
 * <object class="GtkTextTagTable">
 *  <child type="tag">
 *    <object class="GtkTextTag"/>
 *  </child>
 * </object>
 * ]]></programlisting>
 * </example>
 * </refsect2>
 */

enum {
  TAG_CHANGED,
  TAG_ADDED,
  TAG_REMOVED,
  LAST_SIGNAL
};

enum {
  LAST_ARG
};

static void gtk_text_tag_table_finalize     (GObject              *object);
static void gtk_text_tag_table_set_property (GObject              *object,
                                             guint                 prop_id,
                                             const GValue         *value,
                                             GParamSpec           *pspec);
static void gtk_text_tag_table_get_property (GObject              *object,
                                             guint                 prop_id,
                                             GValue               *value,
                                             GParamSpec           *pspec);

static void gtk_text_tag_table_buildable_interface_init (GtkBuildableIface   *iface);
static void gtk_text_tag_table_buildable_add_child      (GtkBuildable        *buildable,
							 GtkBuilder          *builder,
							 GObject             *child,
							 const gchar         *type);

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_CODE (GtkTextTagTable, gtk_text_tag_table, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_text_tag_table_buildable_interface_init))

static void
gtk_text_tag_table_class_init (GtkTextTagTableClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = gtk_text_tag_table_set_property;
  object_class->get_property = gtk_text_tag_table_get_property;
  
  object_class->finalize = gtk_text_tag_table_finalize;
  
  signals[TAG_CHANGED] =
    g_signal_new (I_("tag-changed"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkTextTagTableClass, tag_changed),
                  NULL, NULL,
                  _gtk_marshal_VOID__OBJECT_BOOLEAN,
                  G_TYPE_NONE,
                  2,
                  GTK_TYPE_TEXT_TAG,
                  G_TYPE_BOOLEAN);  

  signals[TAG_ADDED] =
    g_signal_new (I_("tag-added"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkTextTagTableClass, tag_added),
                  NULL, NULL,
                  _gtk_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
                  1,
                  GTK_TYPE_TEXT_TAG);

  signals[TAG_REMOVED] =
    g_signal_new (I_("tag-removed"),  
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkTextTagTableClass, tag_removed),
                  NULL, NULL,
                  _gtk_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
                  1,
                  GTK_TYPE_TEXT_TAG);
}

static void
gtk_text_tag_table_init (GtkTextTagTable *table)
{
  table->hash = g_hash_table_new (g_str_hash, g_str_equal);
}

/**
 * gtk_text_tag_table_new:
 * 
 * Creates a new #GtkTextTagTable. The table contains no tags by
 * default.
 * 
 * Return value: a new #GtkTextTagTable
 **/
GtkTextTagTable*
gtk_text_tag_table_new (void)
{
  GtkTextTagTable *table;

  table = g_object_new (GTK_TYPE_TEXT_TAG_TABLE, NULL);

  return table;
}

static void
foreach_unref (GtkTextTag *tag, gpointer data)
{
  GSList *tmp;
  
  /* We don't want to emit the remove signal here; so we just unparent
   * and unref the tag.
   */

  tmp = tag->table->buffers;
  while (tmp != NULL)
    {
      _gtk_text_buffer_notify_will_remove_tag (GTK_TEXT_BUFFER (tmp->data),
                                               tag);
      
      tmp = tmp->next;
    }
  
  tag->table = NULL;
  g_object_unref (tag);
}

static void
gtk_text_tag_table_finalize (GObject *object)
{
  GtkTextTagTable *table;

  table = GTK_TEXT_TAG_TABLE (object);
  
  gtk_text_tag_table_foreach (table, foreach_unref, NULL);

  g_hash_table_destroy (table->hash);
  g_slist_free (table->anonymous);

  g_slist_free (table->buffers);

  G_OBJECT_CLASS (gtk_text_tag_table_parent_class)->finalize (object);
}
static void
gtk_text_tag_table_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  switch (prop_id)
    {

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
gtk_text_tag_table_get_property (GObject      *object,
                                 guint         prop_id,
                                 GValue       *value,
                                 GParamSpec   *pspec)
{
  switch (prop_id)
    {

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_text_tag_table_buildable_interface_init (GtkBuildableIface   *iface)
{
  iface->add_child = gtk_text_tag_table_buildable_add_child;
}

static void
gtk_text_tag_table_buildable_add_child (GtkBuildable        *buildable,
					GtkBuilder          *builder,
					GObject             *child,
					const gchar         *type)
{
  if (type && strcmp (type, "tag") == 0)
    gtk_text_tag_table_add (GTK_TEXT_TAG_TABLE (buildable),
			    GTK_TEXT_TAG (child));
}

/**
 * gtk_text_tag_table_add:
 * @table: a #GtkTextTagTable
 * @tag: a #GtkTextTag
 *
 * Add a tag to the table. The tag is assigned the highest priority
 * in the table.
 *
 * @tag must not be in a tag table already, and may not have
 * the same name as an already-added tag.
 **/
void
gtk_text_tag_table_add (GtkTextTagTable *table,
                        GtkTextTag      *tag)
{
  guint size;

  g_return_if_fail (GTK_IS_TEXT_TAG_TABLE (table));
  g_return_if_fail (GTK_IS_TEXT_TAG (tag));
  g_return_if_fail (tag->table == NULL);

  if (tag->name && g_hash_table_lookup (table->hash, tag->name))
    {
      g_warning ("A tag named '%s' is already in the tag table.",
                 tag->name);
      return;
    }
  
  g_object_ref (tag);

  if (tag->name)
    g_hash_table_insert (table->hash, tag->name, tag);
  else
    {
      table->anonymous = g_slist_prepend (table->anonymous, tag);
      table->anon_count += 1;
    }

  tag->table = table;

  /* We get the highest tag priority, as the most-recently-added
     tag. Note that we do NOT use gtk_text_tag_set_priority,
     as it assumes the tag is already in the table. */
  size = gtk_text_tag_table_get_size (table);
  g_assert (size > 0);
  tag->priority = size - 1;

  g_signal_emit (table, signals[TAG_ADDED], 0, tag);
}

/**
 * gtk_text_tag_table_lookup:
 * @table: a #GtkTextTagTable 
 * @name: name of a tag
 * 
 * Look up a named tag.
 * 
 * Return value: (transfer none): The tag, or %NULL if none by that name is in the table.
 **/
GtkTextTag*
gtk_text_tag_table_lookup (GtkTextTagTable *table,
                           const gchar     *name)
{
  g_return_val_if_fail (GTK_IS_TEXT_TAG_TABLE (table), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  return g_hash_table_lookup (table->hash, name);
}

/**
 * gtk_text_tag_table_remove:
 * @table: a #GtkTextTagTable
 * @tag: a #GtkTextTag
 * 
 * Remove a tag from the table. This will remove the table's
 * reference to the tag, so be careful - the tag will end
 * up destroyed if you don't have a reference to it.
 **/
void
gtk_text_tag_table_remove (GtkTextTagTable *table,
                           GtkTextTag      *tag)
{
  GSList *tmp;
  
  g_return_if_fail (GTK_IS_TEXT_TAG_TABLE (table));
  g_return_if_fail (GTK_IS_TEXT_TAG (tag));
  g_return_if_fail (tag->table == table);

  /* Our little bad hack to be sure buffers don't still have the tag
   * applied to text in the buffer
   */
  tmp = table->buffers;
  while (tmp != NULL)
    {
      _gtk_text_buffer_notify_will_remove_tag (GTK_TEXT_BUFFER (tmp->data),
                                               tag);
      
      tmp = tmp->next;
    }
  
  /* Set ourselves to the highest priority; this means
     when we're removed, there won't be any gaps in the
     priorities of the tags in the table. */
  gtk_text_tag_set_priority (tag, gtk_text_tag_table_get_size (table) - 1);

  tag->table = NULL;

  if (tag->name)
    g_hash_table_remove (table->hash, tag->name);
  else
    {
      table->anonymous = g_slist_remove (table->anonymous, tag);
      table->anon_count -= 1;
    }

  g_signal_emit (table, signals[TAG_REMOVED], 0, tag);

  g_object_unref (tag);
}

struct ForeachData
{
  GtkTextTagTableForeach func;
  gpointer data;
};

static void
hash_foreach (gpointer key, gpointer value, gpointer data)
{
  struct ForeachData *fd = data;

  g_return_if_fail (GTK_IS_TEXT_TAG (value));

  (* fd->func) (value, fd->data);
}

static void
list_foreach (gpointer data, gpointer user_data)
{
  struct ForeachData *fd = user_data;

  g_return_if_fail (GTK_IS_TEXT_TAG (data));

  (* fd->func) (data, fd->data);
}

/**
 * gtk_text_tag_table_foreach:
 * @table: a #GtkTextTagTable
 * @func: (scope call): a function to call on each tag
 * @data: user data
 *
 * Calls @func on each tag in @table, with user data @data.
 * Note that the table may not be modified while iterating 
 * over it (you can't add/remove tags).
 **/
void
gtk_text_tag_table_foreach (GtkTextTagTable       *table,
                            GtkTextTagTableForeach func,
                            gpointer               data)
{
  struct ForeachData d;

  g_return_if_fail (GTK_IS_TEXT_TAG_TABLE (table));
  g_return_if_fail (func != NULL);

  d.func = func;
  d.data = data;

  g_hash_table_foreach (table->hash, hash_foreach, &d);
  g_slist_foreach (table->anonymous, list_foreach, &d);
}

/**
 * gtk_text_tag_table_get_size:
 * @table: a #GtkTextTagTable
 * 
 * Returns the size of the table (number of tags)
 * 
 * Return value: number of tags in @table
 **/
gint
gtk_text_tag_table_get_size (GtkTextTagTable *table)
{
  g_return_val_if_fail (GTK_IS_TEXT_TAG_TABLE (table), 0);

  return g_hash_table_size (table->hash) + table->anon_count;
}

void
_gtk_text_tag_table_add_buffer (GtkTextTagTable *table,
                                gpointer         buffer)
{
  g_return_if_fail (GTK_IS_TEXT_TAG_TABLE (table));

  table->buffers = g_slist_prepend (table->buffers, buffer);
}

static void
foreach_remove_tag (GtkTextTag *tag, gpointer data)
{
  GtkTextBuffer *buffer;

  buffer = GTK_TEXT_BUFFER (data);

  _gtk_text_buffer_notify_will_remove_tag (buffer, tag);
}

void
_gtk_text_tag_table_remove_buffer (GtkTextTagTable *table,
                                   gpointer         buffer)
{
  g_return_if_fail (GTK_IS_TEXT_TAG_TABLE (table));

  gtk_text_tag_table_foreach (table, foreach_remove_tag, buffer);
  
  table->buffers = g_slist_remove (table->buffers, buffer);
}

#define __GTK_TEXT_TAG_TABLE_C__
#include "gtkaliasdef.c"
