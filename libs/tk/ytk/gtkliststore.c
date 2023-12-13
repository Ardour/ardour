/* gtkliststore.c
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
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <gobject/gvaluecollector.h>
#include "gtktreemodel.h"
#include "gtkliststore.h"
#include "gtktreedatalist.h"
#include "gtktreednd.h"
#include "gtkintl.h"
#include "gtkbuildable.h"
#include "gtkbuilderprivate.h"
#include "gtkalias.h"

#define GTK_LIST_STORE_IS_SORTED(list) (((GtkListStore*)(list))->sort_column_id != GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID)
#define VALID_ITER(iter, list_store) ((iter)!= NULL && (iter)->user_data != NULL && list_store->stamp == (iter)->stamp && !g_sequence_iter_is_end ((iter)->user_data) && g_sequence_iter_get_sequence ((iter)->user_data) == list_store->seq)

static void         gtk_list_store_tree_model_init (GtkTreeModelIface *iface);
static void         gtk_list_store_drag_source_init(GtkTreeDragSourceIface *iface);
static void         gtk_list_store_drag_dest_init  (GtkTreeDragDestIface   *iface);
static void         gtk_list_store_sortable_init   (GtkTreeSortableIface   *iface);
static void         gtk_list_store_buildable_init  (GtkBuildableIface      *iface);
static void         gtk_list_store_finalize        (GObject           *object);
static GtkTreeModelFlags gtk_list_store_get_flags  (GtkTreeModel      *tree_model);
static gint         gtk_list_store_get_n_columns   (GtkTreeModel      *tree_model);
static GType        gtk_list_store_get_column_type (GtkTreeModel      *tree_model,
						    gint               index);
static gboolean     gtk_list_store_get_iter        (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter,
						    GtkTreePath       *path);
static GtkTreePath *gtk_list_store_get_path        (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter);
static void         gtk_list_store_get_value       (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter,
						    gint               column,
						    GValue            *value);
static gboolean     gtk_list_store_iter_next       (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter);
static gboolean     gtk_list_store_iter_children   (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter,
						    GtkTreeIter       *parent);
static gboolean     gtk_list_store_iter_has_child  (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter);
static gint         gtk_list_store_iter_n_children (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter);
static gboolean     gtk_list_store_iter_nth_child  (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter,
						    GtkTreeIter       *parent,
						    gint               n);
static gboolean     gtk_list_store_iter_parent     (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter,
						    GtkTreeIter       *child);


static void gtk_list_store_set_n_columns   (GtkListStore *list_store,
					    gint          n_columns);
static void gtk_list_store_set_column_type (GtkListStore *list_store,
					    gint          column,
					    GType         type);

static void gtk_list_store_increment_stamp (GtkListStore *list_store);


/* Drag and Drop */
static gboolean real_gtk_list_store_row_draggable (GtkTreeDragSource *drag_source,
                                                   GtkTreePath       *path);
static gboolean gtk_list_store_drag_data_delete   (GtkTreeDragSource *drag_source,
                                                   GtkTreePath       *path);
static gboolean gtk_list_store_drag_data_get      (GtkTreeDragSource *drag_source,
                                                   GtkTreePath       *path,
                                                   GtkSelectionData  *selection_data);
static gboolean gtk_list_store_drag_data_received (GtkTreeDragDest   *drag_dest,
                                                   GtkTreePath       *dest,
                                                   GtkSelectionData  *selection_data);
static gboolean gtk_list_store_row_drop_possible  (GtkTreeDragDest   *drag_dest,
                                                   GtkTreePath       *dest_path,
						   GtkSelectionData  *selection_data);


/* sortable */
static void     gtk_list_store_sort                  (GtkListStore           *list_store);
static void     gtk_list_store_sort_iter_changed     (GtkListStore           *list_store,
						      GtkTreeIter            *iter,
						      gint                    column);
static gboolean gtk_list_store_get_sort_column_id    (GtkTreeSortable        *sortable,
						      gint                   *sort_column_id,
						      GtkSortType            *order);
static void     gtk_list_store_set_sort_column_id    (GtkTreeSortable        *sortable,
						      gint                    sort_column_id,
						      GtkSortType             order);
static void     gtk_list_store_set_sort_func         (GtkTreeSortable        *sortable,
						      gint                    sort_column_id,
						      GtkTreeIterCompareFunc  func,
						      gpointer                data,
						      GDestroyNotify          destroy);
static void     gtk_list_store_set_default_sort_func (GtkTreeSortable        *sortable,
						      GtkTreeIterCompareFunc  func,
						      gpointer                data,
						      GDestroyNotify          destroy);
static gboolean gtk_list_store_has_default_sort_func (GtkTreeSortable        *sortable);


/* buildable */
static gboolean gtk_list_store_buildable_custom_tag_start (GtkBuildable  *buildable,
							   GtkBuilder    *builder,
							   GObject       *child,
							   const gchar   *tagname,
							   GMarkupParser *parser,
							   gpointer      *data);
static void     gtk_list_store_buildable_custom_tag_end (GtkBuildable *buildable,
							 GtkBuilder   *builder,
							 GObject      *child,
							 const gchar  *tagname,
							 gpointer     *data);

G_DEFINE_TYPE_WITH_CODE (GtkListStore, gtk_list_store, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL,
						gtk_list_store_tree_model_init)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_SOURCE,
						gtk_list_store_drag_source_init)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_DEST,
						gtk_list_store_drag_dest_init)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_SORTABLE,
						gtk_list_store_sortable_init)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
						gtk_list_store_buildable_init))


static void
gtk_list_store_class_init (GtkListStoreClass *class)
{
  GObjectClass *object_class;

  object_class = (GObjectClass*) class;

  object_class->finalize = gtk_list_store_finalize;
}

static void
gtk_list_store_tree_model_init (GtkTreeModelIface *iface)
{
  iface->get_flags = gtk_list_store_get_flags;
  iface->get_n_columns = gtk_list_store_get_n_columns;
  iface->get_column_type = gtk_list_store_get_column_type;
  iface->get_iter = gtk_list_store_get_iter;
  iface->get_path = gtk_list_store_get_path;
  iface->get_value = gtk_list_store_get_value;
  iface->iter_next = gtk_list_store_iter_next;
  iface->iter_children = gtk_list_store_iter_children;
  iface->iter_has_child = gtk_list_store_iter_has_child;
  iface->iter_n_children = gtk_list_store_iter_n_children;
  iface->iter_nth_child = gtk_list_store_iter_nth_child;
  iface->iter_parent = gtk_list_store_iter_parent;
}

static void
gtk_list_store_drag_source_init (GtkTreeDragSourceIface *iface)
{
  iface->row_draggable = real_gtk_list_store_row_draggable;
  iface->drag_data_delete = gtk_list_store_drag_data_delete;
  iface->drag_data_get = gtk_list_store_drag_data_get;
}

static void
gtk_list_store_drag_dest_init (GtkTreeDragDestIface *iface)
{
  iface->drag_data_received = gtk_list_store_drag_data_received;
  iface->row_drop_possible = gtk_list_store_row_drop_possible;
}

static void
gtk_list_store_sortable_init (GtkTreeSortableIface *iface)
{
  iface->get_sort_column_id = gtk_list_store_get_sort_column_id;
  iface->set_sort_column_id = gtk_list_store_set_sort_column_id;
  iface->set_sort_func = gtk_list_store_set_sort_func;
  iface->set_default_sort_func = gtk_list_store_set_default_sort_func;
  iface->has_default_sort_func = gtk_list_store_has_default_sort_func;
}

void
gtk_list_store_buildable_init (GtkBuildableIface *iface)
{
  iface->custom_tag_start = gtk_list_store_buildable_custom_tag_start;
  iface->custom_tag_end = gtk_list_store_buildable_custom_tag_end;
}

static void
gtk_list_store_init (GtkListStore *list_store)
{
  list_store->seq = g_sequence_new (NULL);
  list_store->sort_list = NULL;
  list_store->stamp = g_random_int ();
  list_store->sort_column_id = GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID;
  list_store->columns_dirty = FALSE;
  list_store->length = 0;
}

/**
 * gtk_list_store_new:
 * @n_columns: number of columns in the list store
 * @Varargs: all #GType types for the columns, from first to last
 *
 * Creates a new list store as with @n_columns columns each of the types passed
 * in.  Note that only types derived from standard GObject fundamental types 
 * are supported. 
 *
 * As an example, <literal>gtk_tree_store_new (3, G_TYPE_INT, G_TYPE_STRING,
 * GDK_TYPE_PIXBUF);</literal> will create a new #GtkListStore with three columns, of type
 * int, string and #GdkPixbuf respectively.
 *
 * Return value: a new #GtkListStore
 **/
GtkListStore *
gtk_list_store_new (gint n_columns,
		    ...)
{
  GtkListStore *retval;
  va_list args;
  gint i;

  g_return_val_if_fail (n_columns > 0, NULL);

  retval = g_object_new (GTK_TYPE_LIST_STORE, NULL);
  gtk_list_store_set_n_columns (retval, n_columns);

  va_start (args, n_columns);

  for (i = 0; i < n_columns; i++)
    {
      GType type = va_arg (args, GType);
      if (! _gtk_tree_data_list_check_type (type))
	{
	  g_warning ("%s: Invalid type %s\n", G_STRLOC, g_type_name (type));
	  g_object_unref (retval);
	  return NULL;
	}

      gtk_list_store_set_column_type (retval, i, type);
    }

  va_end (args);

  return retval;
}


/**
 * gtk_list_store_newv:
 * @n_columns: number of columns in the list store
 * @types: (array length=n_columns): an array of #GType types for the columns, from first to last
 *
 * Non-vararg creation function.  Used primarily by language bindings.
 *
 * Return value: (transfer none): a new #GtkListStore
 **/
GtkListStore *
gtk_list_store_newv (gint   n_columns,
		     GType *types)
{
  GtkListStore *retval;
  gint i;

  g_return_val_if_fail (n_columns > 0, NULL);

  retval = g_object_new (GTK_TYPE_LIST_STORE, NULL);
  gtk_list_store_set_n_columns (retval, n_columns);

  for (i = 0; i < n_columns; i++)
    {
      if (! _gtk_tree_data_list_check_type (types[i]))
	{
	  g_warning ("%s: Invalid type %s\n", G_STRLOC, g_type_name (types[i]));
	  g_object_unref (retval);
	  return NULL;
	}

      gtk_list_store_set_column_type (retval, i, types[i]);
    }

  return retval;
}

/**
 * gtk_list_store_set_column_types:
 * @list_store: A #GtkListStore
 * @n_columns: Number of columns for the list store
 * @types: (array length=n_columns): An array length n of #GTypes
 *
 * This function is meant primarily for #GObjects that inherit from #GtkListStore,
 * and should only be used when constructing a new #GtkListStore.  It will not
 * function after a row has been added, or a method on the #GtkTreeModel
 * interface is called.
 **/
void
gtk_list_store_set_column_types (GtkListStore *list_store,
				 gint          n_columns,
				 GType        *types)
{
  gint i;

  g_return_if_fail (GTK_IS_LIST_STORE (list_store));
  g_return_if_fail (list_store->columns_dirty == 0);

  gtk_list_store_set_n_columns (list_store, n_columns);
  for (i = 0; i < n_columns; i++)
    {
      if (! _gtk_tree_data_list_check_type (types[i]))
	{
	  g_warning ("%s: Invalid type %s\n", G_STRLOC, g_type_name (types[i]));
	  continue;
	}
      gtk_list_store_set_column_type (list_store, i, types[i]);
    }
}

static void
gtk_list_store_set_n_columns (GtkListStore *list_store,
			      gint          n_columns)
{
  int i;

  if (list_store->n_columns == n_columns)
    return;

  list_store->column_headers = g_renew (GType, list_store->column_headers, n_columns);
  for (i = list_store->n_columns; i < n_columns; i++)
    list_store->column_headers[i] = G_TYPE_INVALID;
  list_store->n_columns = n_columns;

  if (list_store->sort_list)
    _gtk_tree_data_list_header_free (list_store->sort_list);
  list_store->sort_list = _gtk_tree_data_list_header_new (n_columns, list_store->column_headers);
}

static void
gtk_list_store_set_column_type (GtkListStore *list_store,
				gint          column,
				GType         type)
{
  if (!_gtk_tree_data_list_check_type (type))
    {
      g_warning ("%s: Invalid type %s\n", G_STRLOC, g_type_name (type));
      return;
    }

  list_store->column_headers[column] = type;
}

static void
gtk_list_store_finalize (GObject *object)
{
  GtkListStore *list_store = GTK_LIST_STORE (object);

  g_sequence_foreach (list_store->seq,
		      (GFunc) _gtk_tree_data_list_free, list_store->column_headers);

  g_sequence_free (list_store->seq);

  _gtk_tree_data_list_header_free (list_store->sort_list);
  g_free (list_store->column_headers);
  
  if (list_store->default_sort_destroy)
    {
      GDestroyNotify d = list_store->default_sort_destroy;

      list_store->default_sort_destroy = NULL;
      d (list_store->default_sort_data);
      list_store->default_sort_data = NULL;
    }

  G_OBJECT_CLASS (gtk_list_store_parent_class)->finalize (object);
}

/* Fulfill the GtkTreeModel requirements */
static GtkTreeModelFlags
gtk_list_store_get_flags (GtkTreeModel *tree_model)
{
  return GTK_TREE_MODEL_ITERS_PERSIST | GTK_TREE_MODEL_LIST_ONLY;
}

static gint
gtk_list_store_get_n_columns (GtkTreeModel *tree_model)
{
  GtkListStore *list_store = (GtkListStore *) tree_model;

  list_store->columns_dirty = TRUE;

  return list_store->n_columns;
}

static GType
gtk_list_store_get_column_type (GtkTreeModel *tree_model,
				gint          index)
{
  GtkListStore *list_store = (GtkListStore *) tree_model;

  g_return_val_if_fail (index < GTK_LIST_STORE (tree_model)->n_columns, 
			G_TYPE_INVALID);

  list_store->columns_dirty = TRUE;

  return list_store->column_headers[index];
}

static gboolean
gtk_list_store_get_iter (GtkTreeModel *tree_model,
			 GtkTreeIter  *iter,
			 GtkTreePath  *path)
{
  GtkListStore *list_store = (GtkListStore *) tree_model;
  GSequence *seq;
  gint i;

  list_store->columns_dirty = TRUE;

  seq = list_store->seq;
  
  i = gtk_tree_path_get_indices (path)[0];

  if (i >= g_sequence_get_length (seq))
    return FALSE;

  iter->stamp = list_store->stamp;
  iter->user_data = g_sequence_get_iter_at_pos (seq, i);

  return TRUE;
}

static GtkTreePath *
gtk_list_store_get_path (GtkTreeModel *tree_model,
			 GtkTreeIter  *iter)
{
  GtkTreePath *path;

  g_return_val_if_fail (iter->stamp == GTK_LIST_STORE (tree_model)->stamp, NULL);

  if (g_sequence_iter_is_end (iter->user_data))
    return NULL;
	
  path = gtk_tree_path_new ();
  gtk_tree_path_append_index (path, g_sequence_iter_get_position (iter->user_data));
  
  return path;
}

static void
gtk_list_store_get_value (GtkTreeModel *tree_model,
			  GtkTreeIter  *iter,
			  gint          column,
			  GValue       *value)
{
  GtkListStore *list_store = (GtkListStore *) tree_model;
  GtkTreeDataList *list;
  gint tmp_column = column;

  g_return_if_fail (column < list_store->n_columns);
  g_return_if_fail (VALID_ITER (iter, list_store));
		    
  list = g_sequence_get (iter->user_data);

  while (tmp_column-- > 0 && list)
    list = list->next;

  if (list == NULL)
    g_value_init (value, list_store->column_headers[column]);
  else
    _gtk_tree_data_list_node_to_value (list,
				       list_store->column_headers[column],
				       value);
}

static gboolean
gtk_list_store_iter_next (GtkTreeModel  *tree_model,
			  GtkTreeIter   *iter)
{
  gboolean retval;

  g_return_val_if_fail (GTK_LIST_STORE (tree_model)->stamp == iter->stamp, FALSE);
  iter->user_data = g_sequence_iter_next (iter->user_data);

  retval = g_sequence_iter_is_end (iter->user_data);
  if (retval)
    iter->stamp = 0;

  return !retval;
}

static gboolean
gtk_list_store_iter_children (GtkTreeModel *tree_model,
			      GtkTreeIter  *iter,
			      GtkTreeIter  *parent)
{
  GtkListStore *list_store = (GtkListStore *) tree_model;
  
  /* this is a list, nodes have no children */
  if (parent)
    {
      iter->stamp = 0;
      return FALSE;
    }

  if (g_sequence_get_length (list_store->seq) > 0)
    {
      iter->stamp = list_store->stamp;
      iter->user_data = g_sequence_get_begin_iter (list_store->seq);
      return TRUE;
    }
  else
    {
      iter->stamp = 0;
      return FALSE;
    }
}

static gboolean
gtk_list_store_iter_has_child (GtkTreeModel *tree_model,
			       GtkTreeIter  *iter)
{
  return FALSE;
}

static gint
gtk_list_store_iter_n_children (GtkTreeModel *tree_model,
				GtkTreeIter  *iter)
{
  GtkListStore *list_store = (GtkListStore *) tree_model;

  if (iter == NULL)
    return g_sequence_get_length (list_store->seq);

  g_return_val_if_fail (list_store->stamp == iter->stamp, -1);

  return 0;
}

static gboolean
gtk_list_store_iter_nth_child (GtkTreeModel *tree_model,
			       GtkTreeIter  *iter,
			       GtkTreeIter  *parent,
			       gint          n)
{
  GtkListStore *list_store = (GtkListStore *) tree_model;
  GSequenceIter *child;

  iter->stamp = 0;

  if (parent)
    return FALSE;

  child = g_sequence_get_iter_at_pos (list_store->seq, n);

  if (g_sequence_iter_is_end (child))
    return FALSE;

  iter->stamp = list_store->stamp;
  iter->user_data = child;

  return TRUE;
}

static gboolean
gtk_list_store_iter_parent (GtkTreeModel *tree_model,
			    GtkTreeIter  *iter,
			    GtkTreeIter  *child)
{
  iter->stamp = 0;
  return FALSE;
}

static gboolean
gtk_list_store_real_set_value (GtkListStore *list_store,
			       GtkTreeIter  *iter,
			       gint          column,
			       GValue       *value,
			       gboolean      sort)
{
  GtkTreeDataList *list;
  GtkTreeDataList *prev;
  gint old_column = column;
  GValue real_value = {0, };
  gboolean converted = FALSE;
  gboolean retval = FALSE;

  if (! g_type_is_a (G_VALUE_TYPE (value), list_store->column_headers[column]))
    {
      if (! (g_value_type_compatible (G_VALUE_TYPE (value), list_store->column_headers[column]) &&
	     g_value_type_compatible (list_store->column_headers[column], G_VALUE_TYPE (value))))
	{
	  g_warning ("%s: Unable to convert from %s to %s\n",
		     G_STRLOC,
		     g_type_name (G_VALUE_TYPE (value)),
		     g_type_name (list_store->column_headers[column]));
	  return retval;
	}
      if (!g_value_transform (value, &real_value))
	{
	  g_warning ("%s: Unable to make conversion from %s to %s\n",
		     G_STRLOC,
		     g_type_name (G_VALUE_TYPE (value)),
		     g_type_name (list_store->column_headers[column]));
	  g_value_unset (&real_value);
	  return retval;
	}
      converted = TRUE;
    }

  prev = list = g_sequence_get (iter->user_data);

  while (list != NULL)
    {
      if (column == 0)
	{
	  if (converted)
	    _gtk_tree_data_list_value_to_node (list, &real_value);
	  else
	    _gtk_tree_data_list_value_to_node (list, value);
	  retval = TRUE;
	  if (converted)
	    g_value_unset (&real_value);
         if (sort && GTK_LIST_STORE_IS_SORTED (list_store))
            gtk_list_store_sort_iter_changed (list_store, iter, old_column);
	  return retval;
	}

      column--;
      prev = list;
      list = list->next;
    }

  if (g_sequence_get (iter->user_data) == NULL)
    {
      list = _gtk_tree_data_list_alloc();
      g_sequence_set (iter->user_data, list);
      list->next = NULL;
    }
  else
    {
      list = prev->next = _gtk_tree_data_list_alloc ();
      list->next = NULL;
    }

  while (column != 0)
    {
      list->next = _gtk_tree_data_list_alloc ();
      list = list->next;
      list->next = NULL;
      column --;
    }

  if (converted)
    _gtk_tree_data_list_value_to_node (list, &real_value);
  else
    _gtk_tree_data_list_value_to_node (list, value);

  retval = TRUE;
  if (converted)
    g_value_unset (&real_value);

  if (sort && GTK_LIST_STORE_IS_SORTED (list_store))
    gtk_list_store_sort_iter_changed (list_store, iter, old_column);

  return retval;
}


/**
 * gtk_list_store_set_value:
 * @list_store: A #GtkListStore
 * @iter: A valid #GtkTreeIter for the row being modified
 * @column: column number to modify
 * @value: new value for the cell
 *
 * Sets the data in the cell specified by @iter and @column.
 * The type of @value must be convertible to the type of the
 * column.
 *
 **/
void
gtk_list_store_set_value (GtkListStore *list_store,
			  GtkTreeIter  *iter,
			  gint          column,
			  GValue       *value)
{
  g_return_if_fail (GTK_IS_LIST_STORE (list_store));
  g_return_if_fail (VALID_ITER (iter, list_store));
  g_return_if_fail (column >= 0 && column < list_store->n_columns);
  g_return_if_fail (G_IS_VALUE (value));

  if (gtk_list_store_real_set_value (list_store, iter, column, value, TRUE))
    {
      GtkTreePath *path;

      path = gtk_list_store_get_path (GTK_TREE_MODEL (list_store), iter);
      gtk_tree_model_row_changed (GTK_TREE_MODEL (list_store), path, iter);
      gtk_tree_path_free (path);
    }
}

static GtkTreeIterCompareFunc
gtk_list_store_get_compare_func (GtkListStore *list_store)
{
  GtkTreeIterCompareFunc func = NULL;

  if (GTK_LIST_STORE_IS_SORTED (list_store))
    {
      if (list_store->sort_column_id != -1)
	{
	  GtkTreeDataSortHeader *header;
	  header = _gtk_tree_data_list_get_header (list_store->sort_list,
						   list_store->sort_column_id);
	  g_return_val_if_fail (header != NULL, NULL);
	  g_return_val_if_fail (header->func != NULL, NULL);
	  func = header->func;
	}
      else
	{
	  func = list_store->default_sort_func;
	}
    }

  return func;
}

static void
gtk_list_store_set_vector_internal (GtkListStore *list_store,
				    GtkTreeIter  *iter,
				    gboolean     *emit_signal,
				    gboolean     *maybe_need_sort,
				    gint         *columns,
				    GValue       *values,
				    gint          n_values)
{
  gint i;
  GtkTreeIterCompareFunc func = NULL;

  func = gtk_list_store_get_compare_func (list_store);
  if (func != _gtk_tree_data_list_compare_func)
    *maybe_need_sort = TRUE;

  for (i = 0; i < n_values; i++)
    {
      *emit_signal = gtk_list_store_real_set_value (list_store, 
					       iter, 
					       columns[i],
					       &values[i],
					       FALSE) || *emit_signal;

      if (func == _gtk_tree_data_list_compare_func &&
	  columns[i] == list_store->sort_column_id)
	*maybe_need_sort = TRUE;
    }
}

static void
gtk_list_store_set_valist_internal (GtkListStore *list_store,
				    GtkTreeIter  *iter,
				    gboolean     *emit_signal,
				    gboolean     *maybe_need_sort,
				    va_list	  var_args)
{
  gint column;
  GtkTreeIterCompareFunc func = NULL;

  column = va_arg (var_args, gint);

  func = gtk_list_store_get_compare_func (list_store);
  if (func != _gtk_tree_data_list_compare_func)
    *maybe_need_sort = TRUE;

  while (column != -1)
    {
      GValue value = { 0, };
      gchar *error = NULL;

      if (column < 0 || column >= list_store->n_columns)
	{
	  g_warning ("%s: Invalid column number %d added to iter (remember to end your list of columns with a -1)", G_STRLOC, column);
	  break;
	}
      g_value_init (&value, list_store->column_headers[column]);

      G_VALUE_COLLECT (&value, var_args, 0, &error);
      if (error)
	{
	  g_warning ("%s: %s", G_STRLOC, error);
	  g_free (error);

 	  /* we purposely leak the value here, it might not be
	   * in a sane state if an error condition occoured
	   */
	  break;
	}

      /* FIXME: instead of calling this n times, refactor with above */
      *emit_signal = gtk_list_store_real_set_value (list_store,
						    iter,
						    column,
						    &value,
						    FALSE) || *emit_signal;
      
      if (func == _gtk_tree_data_list_compare_func &&
	  column == list_store->sort_column_id)
	*maybe_need_sort = TRUE;

      g_value_unset (&value);

      column = va_arg (var_args, gint);
    }
}

/**
 * gtk_list_store_set_valuesv:
 * @list_store: A #GtkListStore
 * @iter: A valid #GtkTreeIter for the row being modified
 * @columns: (array length=n_values): an array of column numbers
 * @values: (array length=n_values): an array of GValues
 * @n_values: the length of the @columns and @values arrays
 *
 * A variant of gtk_list_store_set_valist() which
 * takes the columns and values as two arrays, instead of
 * varargs. This function is mainly intended for 
 * language-bindings and in case the number of columns to
 * change is not known until run-time.
 *
 * Since: 2.12
 */
void
gtk_list_store_set_valuesv (GtkListStore *list_store,
			    GtkTreeIter  *iter,
			    gint         *columns,
			    GValue       *values,
			    gint          n_values)
{
  gboolean emit_signal = FALSE;
  gboolean maybe_need_sort = FALSE;

  g_return_if_fail (GTK_IS_LIST_STORE (list_store));
  g_return_if_fail (VALID_ITER (iter, list_store));

  gtk_list_store_set_vector_internal (list_store, iter,
				      &emit_signal,
				      &maybe_need_sort,
				      columns, values, n_values);

  if (maybe_need_sort && GTK_LIST_STORE_IS_SORTED (list_store))
    gtk_list_store_sort_iter_changed (list_store, iter, list_store->sort_column_id);

  if (emit_signal)
    {
      GtkTreePath *path;

      path = gtk_list_store_get_path (GTK_TREE_MODEL (list_store), iter);
      gtk_tree_model_row_changed (GTK_TREE_MODEL (list_store), path, iter);
      gtk_tree_path_free (path);
    }
}

/**
 * gtk_list_store_set_valist:
 * @list_store: A #GtkListStore
 * @iter: A valid #GtkTreeIter for the row being modified
 * @var_args: va_list of column/value pairs
 *
 * See gtk_list_store_set(); this version takes a va_list for use by language
 * bindings.
 *
 **/
void
gtk_list_store_set_valist (GtkListStore *list_store,
                           GtkTreeIter  *iter,
                           va_list	 var_args)
{
  gboolean emit_signal = FALSE;
  gboolean maybe_need_sort = FALSE;

  g_return_if_fail (GTK_IS_LIST_STORE (list_store));
  g_return_if_fail (VALID_ITER (iter, list_store));

  gtk_list_store_set_valist_internal (list_store, iter, 
				      &emit_signal, 
				      &maybe_need_sort,
				      var_args);

  if (maybe_need_sort && GTK_LIST_STORE_IS_SORTED (list_store))
    gtk_list_store_sort_iter_changed (list_store, iter, list_store->sort_column_id);

  if (emit_signal)
    {
      GtkTreePath *path;

      path = gtk_list_store_get_path (GTK_TREE_MODEL (list_store), iter);
      gtk_tree_model_row_changed (GTK_TREE_MODEL (list_store), path, iter);
      gtk_tree_path_free (path);
    }
}

/**
 * gtk_list_store_set:
 * @list_store: a #GtkListStore
 * @iter: row iterator
 * @Varargs: pairs of column number and value, terminated with -1
 *
 * Sets the value of one or more cells in the row referenced by @iter.
 * The variable argument list should contain integer column numbers,
 * each column number followed by the value to be set.
 * The list is terminated by a -1. For example, to set column 0 with type
 * %G_TYPE_STRING to "Foo", you would write <literal>gtk_list_store_set (store, iter,
 * 0, "Foo", -1)</literal>.
 *
 * The value will be referenced by the store if it is a %G_TYPE_OBJECT, and it
 * will be copied if it is a %G_TYPE_STRING or %G_TYPE_BOXED.
 **/
void
gtk_list_store_set (GtkListStore *list_store,
		    GtkTreeIter  *iter,
		    ...)
{
  va_list var_args;

  va_start (var_args, iter);
  gtk_list_store_set_valist (list_store, iter, var_args);
  va_end (var_args);
}

/**
 * gtk_list_store_remove:
 * @list_store: A #GtkListStore
 * @iter: A valid #GtkTreeIter
 *
 * Removes the given row from the list store.  After being removed, 
 * @iter is set to be the next valid row, or invalidated if it pointed 
 * to the last row in @list_store.
 *
 * Return value: %TRUE if @iter is valid, %FALSE if not.
 **/
gboolean
gtk_list_store_remove (GtkListStore *list_store,
		       GtkTreeIter  *iter)
{
  GtkTreePath *path;
  GSequenceIter *ptr, *next;

  g_return_val_if_fail (GTK_IS_LIST_STORE (list_store), FALSE);
  g_return_val_if_fail (VALID_ITER (iter, list_store), FALSE);

  path = gtk_list_store_get_path (GTK_TREE_MODEL (list_store), iter);

  ptr = iter->user_data;
  next = g_sequence_iter_next (ptr);
  
  _gtk_tree_data_list_free (g_sequence_get (ptr), list_store->column_headers);
  g_sequence_remove (iter->user_data);

  list_store->length--;
  
  gtk_tree_model_row_deleted (GTK_TREE_MODEL (list_store), path);
  gtk_tree_path_free (path);

  if (g_sequence_iter_is_end (next))
    {
      iter->stamp = 0;
      return FALSE;
    }
  else
    {
      iter->stamp = list_store->stamp;
      iter->user_data = next;
      return TRUE;
    }
}

/**
 * gtk_list_store_insert:
 * @list_store: A #GtkListStore
 * @iter: (out): An unset #GtkTreeIter to set to the new row
 * @position: position to insert the new row
 *
 * Creates a new row at @position.  @iter will be changed to point to this new
 * row.  If @position is larger than the number of rows on the list, then the
 * new row will be appended to the list. The row will be empty after this
 * function is called.  To fill in values, you need to call 
 * gtk_list_store_set() or gtk_list_store_set_value().
 *
 **/
void
gtk_list_store_insert (GtkListStore *list_store,
		       GtkTreeIter  *iter,
		       gint          position)
{
  GtkTreePath *path;
  GSequence *seq;
  GSequenceIter *ptr;
  gint length;

  g_return_if_fail (GTK_IS_LIST_STORE (list_store));
  g_return_if_fail (iter != NULL);
  g_return_if_fail (position >= 0);

  list_store->columns_dirty = TRUE;

  seq = list_store->seq;

  length = g_sequence_get_length (seq);
  if (position > length)
    position = length;

  ptr = g_sequence_get_iter_at_pos (seq, position);
  ptr = g_sequence_insert_before (ptr, NULL);

  iter->stamp = list_store->stamp;
  iter->user_data = ptr;

  g_assert (VALID_ITER (iter, list_store));

  list_store->length++;
  
  path = gtk_tree_path_new ();
  gtk_tree_path_append_index (path, position);
  gtk_tree_model_row_inserted (GTK_TREE_MODEL (list_store), path, iter);
  gtk_tree_path_free (path);
}

/**
 * gtk_list_store_insert_before:
 * @list_store: A #GtkListStore
 * @iter: (out): An unset #GtkTreeIter to set to the new row
 * @sibling: (allow-none): A valid #GtkTreeIter, or %NULL
 *
 * Inserts a new row before @sibling. If @sibling is %NULL, then the row will 
 * be appended to the end of the list. @iter will be changed to point to this 
 * new row. The row will be empty after this function is called. To fill in 
 * values, you need to call gtk_list_store_set() or gtk_list_store_set_value().
 *
 **/
void
gtk_list_store_insert_before (GtkListStore *list_store,
			      GtkTreeIter  *iter,
			      GtkTreeIter  *sibling)
{
  GSequenceIter *after;
  
  g_return_if_fail (GTK_IS_LIST_STORE (list_store));
  g_return_if_fail (iter != NULL);
  if (sibling)
    g_return_if_fail (VALID_ITER (sibling, list_store));

  if (!sibling)
    after = g_sequence_get_end_iter (list_store->seq);
  else
    after = sibling->user_data;

  gtk_list_store_insert (list_store, iter, g_sequence_iter_get_position (after));
}

/**
 * gtk_list_store_insert_after:
 * @list_store: A #GtkListStore
 * @iter: (out): An unset #GtkTreeIter to set to the new row
 * @sibling: (allow-none): A valid #GtkTreeIter, or %NULL
 *
 * Inserts a new row after @sibling. If @sibling is %NULL, then the row will be
 * prepended to the beginning of the list. @iter will be changed to point to
 * this new row. The row will be empty after this function is called. To fill
 * in values, you need to call gtk_list_store_set() or gtk_list_store_set_value().
 *
 **/
void
gtk_list_store_insert_after (GtkListStore *list_store,
			     GtkTreeIter  *iter,
			     GtkTreeIter  *sibling)
{
  GSequenceIter *after;

  g_return_if_fail (GTK_IS_LIST_STORE (list_store));
  g_return_if_fail (iter != NULL);
  if (sibling)
    g_return_if_fail (VALID_ITER (sibling, list_store));

  if (!sibling)
    after = g_sequence_get_begin_iter (list_store->seq);
  else
    after = g_sequence_iter_next (sibling->user_data);

  gtk_list_store_insert (list_store, iter, g_sequence_iter_get_position (after));
}

/**
 * gtk_list_store_prepend:
 * @list_store: A #GtkListStore
 * @iter: (out): An unset #GtkTreeIter to set to the prepend row
 *
 * Prepends a new row to @list_store. @iter will be changed to point to this new
 * row. The row will be empty after this function is called. To fill in
 * values, you need to call gtk_list_store_set() or gtk_list_store_set_value().
 *
 **/
void
gtk_list_store_prepend (GtkListStore *list_store,
			GtkTreeIter  *iter)
{
  g_return_if_fail (GTK_IS_LIST_STORE (list_store));
  g_return_if_fail (iter != NULL);

  gtk_list_store_insert (list_store, iter, 0);
}

/**
 * gtk_list_store_append:
 * @list_store: A #GtkListStore
 * @iter: (out): An unset #GtkTreeIter to set to the appended row
 *
 * Appends a new row to @list_store.  @iter will be changed to point to this new
 * row.  The row will be empty after this function is called.  To fill in
 * values, you need to call gtk_list_store_set() or gtk_list_store_set_value().
 *
 **/
void
gtk_list_store_append (GtkListStore *list_store,
		       GtkTreeIter  *iter)
{
  g_return_if_fail (GTK_IS_LIST_STORE (list_store));
  g_return_if_fail (iter != NULL);

  gtk_list_store_insert (list_store, iter, g_sequence_get_length (list_store->seq));
}

static void
gtk_list_store_increment_stamp (GtkListStore *list_store)
{
  do
    {
      list_store->stamp++;
    }
  while (list_store->stamp == 0);
}

/**
 * gtk_list_store_clear:
 * @list_store: a #GtkListStore.
 *
 * Removes all rows from the list store.  
 *
 **/
void
gtk_list_store_clear (GtkListStore *list_store)
{
  GtkTreeIter iter;
  g_return_if_fail (GTK_IS_LIST_STORE (list_store));

  while (g_sequence_get_length (list_store->seq) > 0)
    {
      iter.stamp = list_store->stamp;
      iter.user_data = g_sequence_get_begin_iter (list_store->seq);
      gtk_list_store_remove (list_store, &iter);
    }

  gtk_list_store_increment_stamp (list_store);
}

/**
 * gtk_list_store_iter_is_valid:
 * @list_store: A #GtkListStore.
 * @iter: A #GtkTreeIter.
 *
 * <warning>This function is slow. Only use it for debugging and/or testing
 * purposes.</warning>
 *
 * Checks if the given iter is a valid iter for this #GtkListStore.
 *
 * Return value: %TRUE if the iter is valid, %FALSE if the iter is invalid.
 *
 * Since: 2.2
 **/
gboolean
gtk_list_store_iter_is_valid (GtkListStore *list_store,
                              GtkTreeIter  *iter)
{
  g_return_val_if_fail (GTK_IS_LIST_STORE (list_store), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);

  if (!VALID_ITER (iter, list_store))
    return FALSE;

  if (g_sequence_iter_get_sequence (iter->user_data) != list_store->seq)
    return FALSE;

  return TRUE;
}

static gboolean real_gtk_list_store_row_draggable (GtkTreeDragSource *drag_source,
                                                   GtkTreePath       *path)
{
  return TRUE;
}
  
static gboolean
gtk_list_store_drag_data_delete (GtkTreeDragSource *drag_source,
                                 GtkTreePath       *path)
{
  GtkTreeIter iter;

  if (gtk_list_store_get_iter (GTK_TREE_MODEL (drag_source),
                               &iter,
                               path))
    {
      gtk_list_store_remove (GTK_LIST_STORE (drag_source), &iter);
      return TRUE;
    }
  return FALSE;
}

static gboolean
gtk_list_store_drag_data_get (GtkTreeDragSource *drag_source,
                              GtkTreePath       *path,
                              GtkSelectionData  *selection_data)
{
  /* Note that we don't need to handle the GTK_TREE_MODEL_ROW
   * target, because the default handler does it for us, but
   * we do anyway for the convenience of someone maybe overriding the
   * default handler.
   */

  if (gtk_tree_set_row_drag_data (selection_data,
				  GTK_TREE_MODEL (drag_source),
				  path))
    {
      return TRUE;
    }
  else
    {
      /* FIXME handle text targets at least. */
    }

  return FALSE;
}

static gboolean
gtk_list_store_drag_data_received (GtkTreeDragDest   *drag_dest,
                                   GtkTreePath       *dest,
                                   GtkSelectionData  *selection_data)
{
  GtkTreeModel *tree_model;
  GtkListStore *list_store;
  GtkTreeModel *src_model = NULL;
  GtkTreePath *src_path = NULL;
  gboolean retval = FALSE;

  tree_model = GTK_TREE_MODEL (drag_dest);
  list_store = GTK_LIST_STORE (drag_dest);

  if (gtk_tree_get_row_drag_data (selection_data,
				  &src_model,
				  &src_path) &&
      src_model == tree_model)
    {
      /* Copy the given row to a new position */
      GtkTreeIter src_iter;
      GtkTreeIter dest_iter;
      GtkTreePath *prev;

      if (!gtk_list_store_get_iter (src_model,
                                    &src_iter,
                                    src_path))
        {
          goto out;
        }

      /* Get the path to insert _after_ (dest is the path to insert _before_) */
      prev = gtk_tree_path_copy (dest);

      if (!gtk_tree_path_prev (prev))
        {
          /* dest was the first spot in the list; which means we are supposed
           * to prepend.
           */
          gtk_list_store_prepend (list_store, &dest_iter);

          retval = TRUE;
        }
      else
        {
          if (gtk_list_store_get_iter (tree_model, &dest_iter, prev))
            {
              GtkTreeIter tmp_iter = dest_iter;

              gtk_list_store_insert_after (list_store, &dest_iter, &tmp_iter);

              retval = TRUE;
            }
        }

      gtk_tree_path_free (prev);

      /* If we succeeded in creating dest_iter, copy data from src
       */
      if (retval)
        {
          GtkTreeDataList *dl = g_sequence_get (src_iter.user_data);
          GtkTreeDataList *copy_head = NULL;
          GtkTreeDataList *copy_prev = NULL;
          GtkTreeDataList *copy_iter = NULL;
	  GtkTreePath *path;
          gint col;

          col = 0;
          while (dl)
            {
              copy_iter = _gtk_tree_data_list_node_copy (dl,
                                                         list_store->column_headers[col]);

              if (copy_head == NULL)
                copy_head = copy_iter;

              if (copy_prev)
                copy_prev->next = copy_iter;

              copy_prev = copy_iter;

              dl = dl->next;
              ++col;
            }

	  dest_iter.stamp = list_store->stamp;
          g_sequence_set (dest_iter.user_data, copy_head);

	  path = gtk_list_store_get_path (tree_model, &dest_iter);
	  gtk_tree_model_row_changed (tree_model, path, &dest_iter);
	  gtk_tree_path_free (path);
	}
    }
  else
    {
      /* FIXME maybe add some data targets eventually, or handle text
       * targets in the simple case.
       */
    }

 out:

  if (src_path)
    gtk_tree_path_free (src_path);

  return retval;
}

static gboolean
gtk_list_store_row_drop_possible (GtkTreeDragDest  *drag_dest,
                                  GtkTreePath      *dest_path,
				  GtkSelectionData *selection_data)
{
  gint *indices;
  GtkTreeModel *src_model = NULL;
  GtkTreePath *src_path = NULL;
  gboolean retval = FALSE;

  /* don't accept drops if the list has been sorted */
  if (GTK_LIST_STORE_IS_SORTED (drag_dest))
    return FALSE;

  if (!gtk_tree_get_row_drag_data (selection_data,
				   &src_model,
				   &src_path))
    goto out;

  if (src_model != GTK_TREE_MODEL (drag_dest))
    goto out;

  if (gtk_tree_path_get_depth (dest_path) != 1)
    goto out;

  /* can drop before any existing node, or before one past any existing. */

  indices = gtk_tree_path_get_indices (dest_path);

  if (indices[0] <= g_sequence_get_length (GTK_LIST_STORE (drag_dest)->seq))
    retval = TRUE;

 out:
  if (src_path)
    gtk_tree_path_free (src_path);
  
  return retval;
}

/* Sorting and reordering */

/* Reordering */
static gint
gtk_list_store_reorder_func (GSequenceIter *a,
			     GSequenceIter *b,
			     gpointer       user_data)
{
  GHashTable *new_positions = user_data;
  gint apos = GPOINTER_TO_INT (g_hash_table_lookup (new_positions, a));
  gint bpos = GPOINTER_TO_INT (g_hash_table_lookup (new_positions, b));

  if (apos < bpos)
    return -1;
  if (apos > bpos)
    return 1;
  return 0;
}
  
/**
 * gtk_list_store_reorder:
 * @store: A #GtkListStore.
 * @new_order: (array): an array of integers mapping the new position of each child
 *      to its old position before the re-ordering,
 *      i.e. @new_order<literal>[newpos] = oldpos</literal>.
 *
 * Reorders @store to follow the order indicated by @new_order. Note that
 * this function only works with unsorted stores.
 *
 * Since: 2.2
 **/
void
gtk_list_store_reorder (GtkListStore *store,
			gint         *new_order)
{
  gint i;
  GtkTreePath *path;
  GHashTable *new_positions;
  GSequenceIter *ptr;
  gint *order;
  
  g_return_if_fail (GTK_IS_LIST_STORE (store));
  g_return_if_fail (!GTK_LIST_STORE_IS_SORTED (store));
  g_return_if_fail (new_order != NULL);

  order = g_new (gint, g_sequence_get_length (store->seq));
  for (i = 0; i < g_sequence_get_length (store->seq); i++)
    order[new_order[i]] = i;
  
  new_positions = g_hash_table_new (g_direct_hash, g_direct_equal);

  ptr = g_sequence_get_begin_iter (store->seq);
  i = 0;
  while (!g_sequence_iter_is_end (ptr))
    {
      g_hash_table_insert (new_positions, ptr, GINT_TO_POINTER (order[i++]));

      ptr = g_sequence_iter_next (ptr);
    }
  g_free (order);
  
  g_sequence_sort_iter (store->seq, gtk_list_store_reorder_func, new_positions);

  g_hash_table_destroy (new_positions);
  
  /* emit signal */
  path = gtk_tree_path_new ();
  gtk_tree_model_rows_reordered (GTK_TREE_MODEL (store),
				 path, NULL, new_order);
  gtk_tree_path_free (path);
}

static GHashTable *
save_positions (GSequence *seq)
{
  GHashTable *positions = g_hash_table_new (g_direct_hash, g_direct_equal);
  GSequenceIter *ptr;

  ptr = g_sequence_get_begin_iter (seq);
  while (!g_sequence_iter_is_end (ptr))
    {
      g_hash_table_insert (positions, ptr,
			   GINT_TO_POINTER (g_sequence_iter_get_position (ptr)));
      ptr = g_sequence_iter_next (ptr);
    }

  return positions;
}

static int *
generate_order (GSequence *seq,
		GHashTable *old_positions)
{
  GSequenceIter *ptr;
  int *order = g_new (int, g_sequence_get_length (seq));
  int i;

  i = 0;
  ptr = g_sequence_get_begin_iter (seq);
  while (!g_sequence_iter_is_end (ptr))
    {
      int old_pos = GPOINTER_TO_INT (g_hash_table_lookup (old_positions, ptr));
      order[i++] = old_pos;
      ptr = g_sequence_iter_next (ptr);
    }

  g_hash_table_destroy (old_positions);

  return order;
}

/**
 * gtk_list_store_swap:
 * @store: A #GtkListStore.
 * @a: A #GtkTreeIter.
 * @b: Another #GtkTreeIter.
 *
 * Swaps @a and @b in @store. Note that this function only works with
 * unsorted stores.
 *
 * Since: 2.2
 **/
void
gtk_list_store_swap (GtkListStore *store,
		     GtkTreeIter  *a,
		     GtkTreeIter  *b)
{
  GHashTable *old_positions;
  gint *order;
  GtkTreePath *path;

  g_return_if_fail (GTK_IS_LIST_STORE (store));
  g_return_if_fail (!GTK_LIST_STORE_IS_SORTED (store));
  g_return_if_fail (VALID_ITER (a, store));
  g_return_if_fail (VALID_ITER (b, store));

  if (a->user_data == b->user_data)
    return;

  old_positions = save_positions (store->seq);
  
  g_sequence_swap (a->user_data, b->user_data);

  order = generate_order (store->seq, old_positions);
  path = gtk_tree_path_new ();
  
  gtk_tree_model_rows_reordered (GTK_TREE_MODEL (store),
				 path, NULL, order);

  gtk_tree_path_free (path);
  g_free (order);
}

static void
gtk_list_store_move_to (GtkListStore *store,
			GtkTreeIter  *iter,
			gint	      new_pos)
{
  GHashTable *old_positions;
  GtkTreePath *path;
  gint *order;
  
  old_positions = save_positions (store->seq);
  
  g_sequence_move (iter->user_data, g_sequence_get_iter_at_pos (store->seq, new_pos));

  order = generate_order (store->seq, old_positions);
  
  path = gtk_tree_path_new ();
  gtk_tree_model_rows_reordered (GTK_TREE_MODEL (store),
				 path, NULL, order);
  gtk_tree_path_free (path);
  g_free (order);
}

/**
 * gtk_list_store_move_before:
 * @store: A #GtkListStore.
 * @iter: A #GtkTreeIter.
 * @position: (allow-none): A #GtkTreeIter, or %NULL.
 *
 * Moves @iter in @store to the position before @position. Note that this
 * function only works with unsorted stores. If @position is %NULL, @iter
 * will be moved to the end of the list.
 *
 * Since: 2.2
 **/
void
gtk_list_store_move_before (GtkListStore *store,
                            GtkTreeIter  *iter,
			    GtkTreeIter  *position)
{
  gint pos;
  
  g_return_if_fail (GTK_IS_LIST_STORE (store));
  g_return_if_fail (!GTK_LIST_STORE_IS_SORTED (store));
  g_return_if_fail (VALID_ITER (iter, store));
  if (position)
    g_return_if_fail (VALID_ITER (position, store));

  if (position)
    pos = g_sequence_iter_get_position (position->user_data);
  else
    pos = -1;
  
  gtk_list_store_move_to (store, iter, pos);
}

/**
 * gtk_list_store_move_after:
 * @store: A #GtkListStore.
 * @iter: A #GtkTreeIter.
 * @position: (allow-none): A #GtkTreeIter or %NULL.
 *
 * Moves @iter in @store to the position after @position. Note that this
 * function only works with unsorted stores. If @position is %NULL, @iter
 * will be moved to the start of the list.
 *
 * Since: 2.2
 **/
void
gtk_list_store_move_after (GtkListStore *store,
			   GtkTreeIter  *iter,
			   GtkTreeIter  *position)
{
  gint pos;
  
  g_return_if_fail (GTK_IS_LIST_STORE (store));
  g_return_if_fail (!GTK_LIST_STORE_IS_SORTED (store));
  g_return_if_fail (VALID_ITER (iter, store));
  if (position)
    g_return_if_fail (VALID_ITER (position, store));

  if (position)
    pos = g_sequence_iter_get_position (position->user_data) + 1;
  else
    pos = 0;
  
  gtk_list_store_move_to (store, iter, pos);
}
    
/* Sorting */
static gint
gtk_list_store_compare_func (GSequenceIter *a,
			     GSequenceIter *b,
			     gpointer      user_data)
{
  GtkListStore *list_store = user_data;
  GtkTreeIter iter_a;
  GtkTreeIter iter_b;
  gint retval;
  GtkTreeIterCompareFunc func;
  gpointer data;

  if (list_store->sort_column_id != -1)
    {
      GtkTreeDataSortHeader *header;

      header = _gtk_tree_data_list_get_header (list_store->sort_list,
					       list_store->sort_column_id);
      g_return_val_if_fail (header != NULL, 0);
      g_return_val_if_fail (header->func != NULL, 0);

      func = header->func;
      data = header->data;
    }
  else
    {
      g_return_val_if_fail (list_store->default_sort_func != NULL, 0);
      func = list_store->default_sort_func;
      data = list_store->default_sort_data;
    }

  iter_a.stamp = list_store->stamp;
  iter_a.user_data = (gpointer)a;
  iter_b.stamp = list_store->stamp;
  iter_b.user_data = (gpointer)b;

  g_assert (VALID_ITER (&iter_a, list_store));
  g_assert (VALID_ITER (&iter_b, list_store));
  
  retval = (* func) (GTK_TREE_MODEL (list_store), &iter_a, &iter_b, data);

  if (list_store->order == GTK_SORT_DESCENDING)
    {
      if (retval > 0)
	retval = -1;
      else if (retval < 0)
	retval = 1;
    }

  return retval;
}

static void
gtk_list_store_sort (GtkListStore *list_store)
{
  gint *new_order;
  GtkTreePath *path;
  GHashTable *old_positions;

  if (!GTK_LIST_STORE_IS_SORTED (list_store) ||
      g_sequence_get_length (list_store->seq) <= 1)
    return;

  old_positions = save_positions (list_store->seq);

  g_sequence_sort_iter (list_store->seq, gtk_list_store_compare_func, list_store);

  /* Let the world know about our new order */
  new_order = generate_order (list_store->seq, old_positions);

  path = gtk_tree_path_new ();
  gtk_tree_model_rows_reordered (GTK_TREE_MODEL (list_store),
				 path, NULL, new_order);
  gtk_tree_path_free (path);
  g_free (new_order);
}

static gboolean
iter_is_sorted (GtkListStore *list_store,
                GtkTreeIter  *iter)
{
  GSequenceIter *cmp;

  if (!g_sequence_iter_is_begin (iter->user_data))
    {
      cmp = g_sequence_iter_prev (iter->user_data);
      if (gtk_list_store_compare_func (cmp, iter->user_data, list_store) > 0)
	return FALSE;
    }

  cmp = g_sequence_iter_next (iter->user_data);
  if (!g_sequence_iter_is_end (cmp))
    {
      if (gtk_list_store_compare_func (iter->user_data, cmp, list_store) > 0)
	return FALSE;
    }
  
  return TRUE;
}

static void
gtk_list_store_sort_iter_changed (GtkListStore *list_store,
				  GtkTreeIter  *iter,
				  gint          column)

{
  GtkTreePath *path;

  path = gtk_list_store_get_path (GTK_TREE_MODEL (list_store), iter);
  gtk_tree_model_row_changed (GTK_TREE_MODEL (list_store), path, iter);
  gtk_tree_path_free (path);

  if (!iter_is_sorted (list_store, iter))
    {
      GHashTable *old_positions;
      gint *order;

      old_positions = save_positions (list_store->seq);
      g_sequence_sort_changed_iter (iter->user_data,
				    gtk_list_store_compare_func,
				    list_store);
      order = generate_order (list_store->seq, old_positions);
      path = gtk_tree_path_new ();
      gtk_tree_model_rows_reordered (GTK_TREE_MODEL (list_store),
                                     path, NULL, order);
      gtk_tree_path_free (path);
      g_free (order);
    }
}

static gboolean
gtk_list_store_get_sort_column_id (GtkTreeSortable  *sortable,
				   gint             *sort_column_id,
				   GtkSortType      *order)
{
  GtkListStore *list_store = (GtkListStore *) sortable;

  if (sort_column_id)
    * sort_column_id = list_store->sort_column_id;
  if (order)
    * order = list_store->order;

  if (list_store->sort_column_id == GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID ||
      list_store->sort_column_id == GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID)
    return FALSE;

  return TRUE;
}

static void
gtk_list_store_set_sort_column_id (GtkTreeSortable  *sortable,
				   gint              sort_column_id,
				   GtkSortType       order)
{
  GtkListStore *list_store = (GtkListStore *) sortable;

  if ((list_store->sort_column_id == sort_column_id) &&
      (list_store->order == order))
    return;

  if (sort_column_id != GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID)
    {
      if (sort_column_id != GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID)
	{
	  GtkTreeDataSortHeader *header = NULL;

	  header = _gtk_tree_data_list_get_header (list_store->sort_list, 
						   sort_column_id);

	  /* We want to make sure that we have a function */
	  g_return_if_fail (header != NULL);
	  g_return_if_fail (header->func != NULL);
	}
      else
	{
	  g_return_if_fail (list_store->default_sort_func != NULL);
	}
    }


  list_store->sort_column_id = sort_column_id;
  list_store->order = order;

  gtk_tree_sortable_sort_column_changed (sortable);

  gtk_list_store_sort (list_store);
}

static void
gtk_list_store_set_sort_func (GtkTreeSortable        *sortable,
			      gint                    sort_column_id,
			      GtkTreeIterCompareFunc  func,
			      gpointer                data,
			      GDestroyNotify          destroy)
{
  GtkListStore *list_store = (GtkListStore *) sortable;

  list_store->sort_list = _gtk_tree_data_list_set_header (list_store->sort_list, 
							  sort_column_id, 
							  func, data, destroy);

  if (list_store->sort_column_id == sort_column_id)
    gtk_list_store_sort (list_store);
}

static void
gtk_list_store_set_default_sort_func (GtkTreeSortable        *sortable,
				      GtkTreeIterCompareFunc  func,
				      gpointer                data,
				      GDestroyNotify          destroy)
{
  GtkListStore *list_store = (GtkListStore *) sortable;

  if (list_store->default_sort_destroy)
    {
      GDestroyNotify d = list_store->default_sort_destroy;

      list_store->default_sort_destroy = NULL;
      d (list_store->default_sort_data);
    }

  list_store->default_sort_func = func;
  list_store->default_sort_data = data;
  list_store->default_sort_destroy = destroy;

  if (list_store->sort_column_id == GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID)
    gtk_list_store_sort (list_store);
}

static gboolean
gtk_list_store_has_default_sort_func (GtkTreeSortable *sortable)
{
  GtkListStore *list_store = (GtkListStore *) sortable;

  return (list_store->default_sort_func != NULL);
}


/**
 * gtk_list_store_insert_with_values:
 * @list_store: A #GtkListStore
 * @iter: (out) (allow-none): An unset #GtkTreeIter to set to the new row, or %NULL.
 * @position: position to insert the new row
 * @Varargs: pairs of column number and value, terminated with -1
 *
 * Creates a new row at @position.  @iter will be changed to point to this new
 * row.  If @position is larger than the number of rows on the list, then the
 * new row will be appended to the list. The row will be filled with the 
 * values given to this function. 
 * 
 * Calling
 * <literal>gtk_list_store_insert_with_values(list_store, iter, position...)</literal> 
 * has the same effect as calling 
 * |[
 * gtk_list_store_insert (list_store, iter, position);
 * gtk_list_store_set (list_store, iter, ...);
 * ]|
 * with the difference that the former will only emit a row_inserted signal,
 * while the latter will emit row_inserted, row_changed and, if the list store
 * is sorted, rows_reordered. Since emitting the rows_reordered signal
 * repeatedly can affect the performance of the program, 
 * gtk_list_store_insert_with_values() should generally be preferred when
 * inserting rows in a sorted list store.
 *
 * Since: 2.6
 */
void
gtk_list_store_insert_with_values (GtkListStore *list_store,
				   GtkTreeIter  *iter,
				   gint          position,
				   ...)
{
  GtkTreePath *path;
  GSequence *seq;
  GSequenceIter *ptr;
  GtkTreeIter tmp_iter;
  gint length;
  gboolean changed = FALSE;
  gboolean maybe_need_sort = FALSE;
  va_list var_args;

  /* FIXME: refactor to reduce overlap with gtk_list_store_set() */
  g_return_if_fail (GTK_IS_LIST_STORE (list_store));

  if (!iter)
    iter = &tmp_iter;

  list_store->columns_dirty = TRUE;

  seq = list_store->seq;

  length = g_sequence_get_length (seq);
  if (position > length)
    position = length;

  ptr = g_sequence_get_iter_at_pos (seq, position);
  ptr = g_sequence_insert_before (ptr, NULL);

  iter->stamp = list_store->stamp;
  iter->user_data = ptr;

  g_assert (VALID_ITER (iter, list_store));

  list_store->length++;  

  va_start (var_args, position);
  gtk_list_store_set_valist_internal (list_store, iter, 
				      &changed, &maybe_need_sort,
				      var_args);
  va_end (var_args);

  /* Don't emit rows_reordered here */
  if (maybe_need_sort && GTK_LIST_STORE_IS_SORTED (list_store))
    g_sequence_sort_changed_iter (iter->user_data,
				  gtk_list_store_compare_func,
				  list_store);

  /* Just emit row_inserted */
  path = gtk_list_store_get_path (GTK_TREE_MODEL (list_store), iter);
  gtk_tree_model_row_inserted (GTK_TREE_MODEL (list_store), path, iter);
  gtk_tree_path_free (path);
}


/**
 * gtk_list_store_insert_with_valuesv:
 * @list_store: A #GtkListStore
 * @iter: (out) (allow-none): An unset #GtkTreeIter to set to the new row, or %NULL.
 * @position: position to insert the new row
 * @columns: (array length=n_values): an array of column numbers
 * @values: (array length=n_values): an array of GValues 
 * @n_values: the length of the @columns and @values arrays
 * 
 * A variant of gtk_list_store_insert_with_values() which
 * takes the columns and values as two arrays, instead of
 * varargs. This function is mainly intended for 
 * language-bindings.
 *
 * Since: 2.6
 */
void
gtk_list_store_insert_with_valuesv (GtkListStore *list_store,
				    GtkTreeIter  *iter,
				    gint          position,
				    gint         *columns, 
				    GValue       *values,
				    gint          n_values)
{
  GtkTreePath *path;
  GSequence *seq;
  GSequenceIter *ptr;
  GtkTreeIter tmp_iter;
  gint length;
  gboolean changed = FALSE;
  gboolean maybe_need_sort = FALSE;

  /* FIXME refactor to reduce overlap with 
   * gtk_list_store_insert_with_values() 
   */
  g_return_if_fail (GTK_IS_LIST_STORE (list_store));

  if (!iter)
    iter = &tmp_iter;

  list_store->columns_dirty = TRUE;

  seq = list_store->seq;

  length = g_sequence_get_length (seq);
  if (position > length)
    position = length;

  ptr = g_sequence_get_iter_at_pos (seq, position);
  ptr = g_sequence_insert_before (ptr, NULL);

  iter->stamp = list_store->stamp;
  iter->user_data = ptr;

  g_assert (VALID_ITER (iter, list_store));

  list_store->length++;  

  gtk_list_store_set_vector_internal (list_store, iter,
				      &changed, &maybe_need_sort,
				      columns, values, n_values);

  /* Don't emit rows_reordered here */
  if (maybe_need_sort && GTK_LIST_STORE_IS_SORTED (list_store))
    g_sequence_sort_changed_iter (iter->user_data,
				  gtk_list_store_compare_func,
				  list_store);

  /* Just emit row_inserted */
  path = gtk_list_store_get_path (GTK_TREE_MODEL (list_store), iter);
  gtk_tree_model_row_inserted (GTK_TREE_MODEL (list_store), path, iter);
  gtk_tree_path_free (path);
}

/* GtkBuildable custom tag implementation
 *
 * <columns>
 *   <column type="..."/>
 *   <column type="..."/>
 * </columns>
 */
typedef struct {
  gboolean translatable;
  gchar *context;
  int id;
} ColInfo;

typedef struct {
  GtkBuilder *builder;
  GObject *object;
  GSList *column_type_names;
  GType *column_types;
  GValue *values;
  gint *colids;
  ColInfo **columns;
  gint last_row;
  gint n_columns;
  gint row_column;
  GQuark error_quark;
  gboolean is_data;
  const gchar *domain;
} SubParserData;

static void
list_store_start_element (GMarkupParseContext *context,
			  const gchar         *element_name,
			  const gchar        **names,
			  const gchar        **values,
			  gpointer             user_data,
			  GError             **error)
{
  guint i;
  SubParserData *data = (SubParserData*)user_data;

  if (strcmp (element_name, "col") == 0)
    {
      int i, id = -1;
      gchar *context = NULL;
      gboolean translatable = FALSE;
      ColInfo *info;

      if (data->row_column >= data->n_columns)
        {
	  g_set_error (error, data->error_quark, 0,
	  	       "Too many columns, maximum is %d\n", data->n_columns - 1);
          return;
        }

      for (i = 0; names[i]; i++)
	if (strcmp (names[i], "id") == 0)
	  {
	    errno = 0;
	    id = atoi (values[i]);
	    if (errno)
              {
	        g_set_error (error, data->error_quark, 0,
		  	     "the id tag %s could not be converted to an integer",
			     values[i]);
                return;
              }
	    if (id < 0 || id >= data->n_columns)
              {
                g_set_error (error, data->error_quark, 0,
                             "id value %d out of range", id);
                return;
              }
	  }
	else if (strcmp (names[i], "translatable") == 0)
	  {
	    if (!_gtk_builder_boolean_from_string (values[i], &translatable,
						   error))
	      return;
	  }
	else if (strcmp (names[i], "comments") == 0)
	  {
	    /* do nothing, comments are for translators */
	  }
	else if (strcmp (names[i], "context") == 0) 
	  {
	    context = g_strdup (values[i]);
	  }

      if (id == -1)
        {
	  g_set_error (error, data->error_quark, 0,
	  	       "<col> needs an id attribute");
          return;
        }
      
      info = g_slice_new0 (ColInfo);
      info->translatable = translatable;
      info->context = context;
      info->id = id;

      data->colids[data->row_column] = id;
      data->columns[data->row_column] = info;
      data->row_column++;
      data->is_data = TRUE;
    }
  else if (strcmp (element_name, "row") == 0)
    ;
  else if (strcmp (element_name, "column") == 0)
    for (i = 0; names[i]; i++)
      if (strcmp (names[i], "type") == 0)
	data->column_type_names = g_slist_prepend (data->column_type_names,
						   g_strdup (values[i]));
  else if (strcmp (element_name, "columns") == 0)
    ;
  else if (strcmp (element_name, "data") == 0)
    ;
  else
    g_set_error (error, data->error_quark, 0,
		 "Unknown start tag: %s", element_name);
}

static void
list_store_end_element (GMarkupParseContext *context,
			const gchar         *element_name,
			gpointer             user_data,
			GError             **error)
{
  SubParserData *data = (SubParserData*)user_data;

  g_assert (data->builder);
  
  if (strcmp (element_name, "row") == 0)
    {
      GtkTreeIter iter;
      int i;

      gtk_list_store_insert_with_valuesv (GTK_LIST_STORE (data->object),
					  &iter,
					  data->last_row,
					  data->colids,
					  data->values,
					  data->row_column);
      for (i = 0; i < data->row_column; i++)
	{
	  ColInfo *info = data->columns[i];
	  g_free (info->context);
	  g_slice_free (ColInfo, info);
	  data->columns[i] = NULL;
	  g_value_unset (&data->values[i]);
	}
      g_free (data->values);
      data->values = g_new0 (GValue, data->n_columns);
      data->last_row++;
      data->row_column = 0;
    }
  else if (strcmp (element_name, "columns") == 0)
    {
      GType *column_types;
      GSList *l;
      int i;
      GType type;

      data->column_type_names = g_slist_reverse (data->column_type_names);
      column_types = g_new0 (GType, g_slist_length (data->column_type_names));

      for (l = data->column_type_names, i = 0; l; l = l->next, i++)
	{
	  type = gtk_builder_get_type_from_name (data->builder, l->data);
	  if (type == G_TYPE_INVALID)
	    {
	      g_warning ("Unknown type %s specified in treemodel %s",
			 (const gchar*)l->data,
			 gtk_buildable_get_name (GTK_BUILDABLE (data->object)));
	      continue;
	    }
	  column_types[i] = type;

	  g_free (l->data);
	}

      gtk_list_store_set_column_types (GTK_LIST_STORE (data->object), i,
				       column_types);

      g_free (column_types);
    }
  else if (strcmp (element_name, "col") == 0)
    data->is_data = FALSE;
  else if (strcmp (element_name, "data") == 0)
    ;
  else if (strcmp (element_name, "column") == 0)
    ;
  else
    g_set_error (error, data->error_quark, 0,
		 "Unknown end tag: %s", element_name);
}

static void
list_store_text (GMarkupParseContext *context,
		 const gchar         *text,
		 gsize                text_len,
		 gpointer             user_data,
		 GError             **error)
{
  SubParserData *data = (SubParserData*)user_data;
  gint i;
  GError *tmp_error = NULL;
  gchar *string;
  ColInfo *info;
  
  if (!data->is_data)
    return;

  i = data->row_column - 1;
  info = data->columns[i];

  string = g_strndup (text, text_len);
  if (info->translatable && text_len)
    {
      gchar *translated;

      /* FIXME: This will not use the domain set in the .ui file,
       * since the parser is not telling the builder about the domain.
       * However, it will work for gtk_builder_set_translation_domain() calls.
       */
      translated = _gtk_builder_parser_translate (data->domain,
						  info->context,
						  string);
      g_free (string);
      string = translated;
    }

  if (!gtk_builder_value_from_string_type (data->builder,
					   data->column_types[info->id],
					   string,
					   &data->values[i],
					   &tmp_error))
    {
      g_set_error (error,
		   tmp_error->domain,
		   tmp_error->code,
		   "Could not convert '%s' to type %s: %s\n",
		   text, g_type_name (data->column_types[info->id]),
		   tmp_error->message);
      g_error_free (tmp_error);
    }
  g_free (string);
}

static const GMarkupParser list_store_parser =
  {
    list_store_start_element,
    list_store_end_element,
    list_store_text
  };

static gboolean
gtk_list_store_buildable_custom_tag_start (GtkBuildable  *buildable,
					   GtkBuilder    *builder,
					   GObject       *child,
					   const gchar   *tagname,
					   GMarkupParser *parser,
					   gpointer      *data)
{
  SubParserData *parser_data;

  if (child)
    return FALSE;

  if (strcmp (tagname, "columns") == 0)
    {

      parser_data = g_slice_new0 (SubParserData);
      parser_data->builder = builder;
      parser_data->object = G_OBJECT (buildable);
      parser_data->column_type_names = NULL;

      *parser = list_store_parser;
      *data = parser_data;
      return TRUE;
    }
  else if (strcmp (tagname, "data") == 0)
    {
      gint n_columns = gtk_list_store_get_n_columns (GTK_TREE_MODEL (buildable));
      if (n_columns == 0)
	g_error ("Cannot append data to an empty model");

      parser_data = g_slice_new0 (SubParserData);
      parser_data->builder = builder;
      parser_data->object = G_OBJECT (buildable);
      parser_data->values = g_new0 (GValue, n_columns);
      parser_data->colids = g_new0 (gint, n_columns);
      parser_data->columns = g_new0 (ColInfo*, n_columns);
      parser_data->column_types = GTK_LIST_STORE (buildable)->column_headers;
      parser_data->n_columns = n_columns;
      parser_data->last_row = 0;
      parser_data->error_quark = g_quark_from_static_string ("GtkListStore");
      parser_data->domain = gtk_builder_get_translation_domain (builder);
      
      *parser = list_store_parser;
      *data = parser_data;
      return TRUE;
    }
  else
    g_warning ("Unknown custom list store tag: %s", tagname);
  
  return FALSE;
}

static void
gtk_list_store_buildable_custom_tag_end (GtkBuildable *buildable,
					 GtkBuilder   *builder,
					 GObject      *child,
					 const gchar  *tagname,
					 gpointer     *data)
{
  SubParserData *sub = (SubParserData*)data;
  
  if (strcmp (tagname, "columns") == 0)
    {
      g_slist_free (sub->column_type_names);
      g_slice_free (SubParserData, sub);
    }
  else if (strcmp (tagname, "data") == 0)
    {
      int i;
      for (i = 0; i < sub->n_columns; i++)
	{
	  ColInfo *info = sub->columns[i];
	  if (info)
	    {
	      g_free (info->context);
	      g_slice_free (ColInfo, info);
	    }
	}
      g_free (sub->colids);
      g_free (sub->columns);
      g_free (sub->values);
      g_slice_free (SubParserData, sub);
    }
  else
    g_warning ("Unknown custom list store tag: %s", tagname);
}

#define __GTK_LIST_STORE_C__
#include "gtkaliasdef.c"
