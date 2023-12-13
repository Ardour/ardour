/* GTK - The GIMP Toolkit
 * gtkfilesystemmodel.c: GtkTreeModel wrapping a GtkFileSystem
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

#include "gtkfilesystemmodel.h"

#include <stdlib.h>
#include <string.h>

#include "gtkfilesystem.h"
#include "gtkintl.h"
#include "gtkmarshalers.h"
#include "gtktreedatalist.h"
#include "gtktreednd.h"
#include "gtktreemodel.h"
#include "gtkalias.h"

/*** Structure: how GtkFileSystemModel works
 *
 * This is a custom GtkTreeModel used to hold a collection of files for GtkFileChooser.  There are two use cases:
 *
 *   1. The model populates itself from a folder, using the GIO file enumerator API.  This happens if you use
 *      _gtk_file_system_model_new_for_directory().  This is the normal usage for showing the contents of a folder.
 *
 *   2. The caller populates the model by hand, with files not necessarily in the same folder.  This happens
 *      if you use _gtk_file_system_model_new() and then _gtk_file_system_model_add_and_query_file().  This is
 *      the special kind of usage for "search" and "recent-files", where the file chooser gives the model the
 *      files to be displayed.
 *
 * Internal data structure
 * -----------------------
 *
 * Each file is kept in a FileModelNode structure.  Each FileModelNode holds a GFile* and other data.  All the
 * node structures have the same size, determined at runtime, depending on the number of columns that were passed
 * to _gtk_file_system_model_new() or _gtk_file_system_model_new_for_directory() (that is, the size of a node is
 * not sizeof (FileModelNode), but rather model->node_size).  The last field in the FileModelNode structure,
 * node->values[], is an array of GValue, used to hold the data for those columns.
 *
 * The model stores an array of FileModelNode structures in model->files.  This is a GArray where each element is
 * model->node_size bytes in size (the model computes that node size when initializing itself).  There are
 * convenience macros, get_node() and node_index(), to access that array based on an array index or a pointer to
 * a node inside the array.
 *
 * The model accesses files through two of its fields:
 *
 *   model->files - GArray of FileModelNode structures.
 *
 *   model->file_lookup - hash table that maps a GFile* to an index inside the model->files array.
 *
 * The model->file_lookup hash table is populated lazily.  It is both accessed and populated with the
 * node_get_for_file() function.  The invariant is that the files in model->files[n] for n < g_hash_table_size
 * (model->file_lookup) are already added to the hash table. The hash table will get cleared when we re-sort the
 * files, as the array will be in a different order and the indexes need to be rebuilt.
 *
 * Each FileModelNode has a node->visible field, which indicates whether the node is visible in the GtkTreeView.
 * A node may be invisible if, for example, it corresponds to a hidden file and the file chooser is not showing
 * hidden files.  Also, a file filter may be explicitly set onto the model, for example, to only show files that
 * match "*.jpg".  In this case, node->filtered_out says whether the node failed the filter.  The ultimate
 * decision on whether a node is visible or not in the treeview is distilled into the node->visible field.
 * The reason for having a separate node->filtered_out field is so that the file chooser can query whether
 * a (filtered-out) folder should be made sensitive in the GUI.
 *
 * Visible rows vs. possibly-invisible nodes
 * -----------------------------------------
 *
 * Since not all nodes in the model->files array may be visible, we need a way to map visible row indexes from
 * the treeview to array indexes in our array of files.  And thus we introduce a bit of terminology:
 *
 *   index - An index in the model->files array.  All variables/fields that represent indexes are either called
 *   "index" or "i_*", or simply "i" for things like loop counters.
 *
 *   row - An index in the GtkTreeView, i.e. the index of a row within the outward-facing API of the
 *   GtkFileSystemModel.  However, note that our rows are 1-based, not 0-based, for the reason explained in the
 *   following paragraph.  Variables/fields that represent visible rows are called "row", or "r_*", or simply
 *   "r".
 *
 * Each FileModelNode has a node->row field which is the number of visible rows in the treeview, *before and
 * including* that node.  This means that node->row is 1-based, instead of 0-based --- this makes some code
 * simpler, believe it or not :)  This also means that when the calling GtkTreeView gives us a GtkTreePath, we
 * turn the 0-based treepath into a 1-based row for our purposes.  If a node is not visible, it will have the
 * same row number as its closest preceding visible node.
 *
 * We try to compute the node->row fields lazily.  A node is said to be "valid" if its node->row is accurate.
 * For this, the model keeps a model->n_nodes_valid field which is the count of valid nodes starting from the
 * beginning of the model->files array.  When a node changes its information, or when a node gets deleted, that
 * node and the following ones get invalidated by simply setting model->n_nodes_valid to the array index of the
 * node.  If the model happens to need a node's row number and that node is in the model->files array after
 * model->n_nodes_valid, then the nodes get re-validated up to the sought node.  See node_validate_rows() for
 * this logic.
 *
 * You never access a node->row directly.  Instead, call node_get_tree_row().  That function will validate the nodes
 * up to the sought one if the node is not valid yet, and it will return a proper 0-based row.
 *
 * Sorting
 * -------
 *
 * The model implements the GtkTreeSortable interface.  To avoid re-sorting
 * every time a node gets added (which would lead to O(n^2) performance during
 * the initial population of the model), the model can freeze itself (with
 * freeze_updates()) during the intial population process.  When the model is
 * frozen, sorting will not happen.  The model will sort itself when the freeze
 * count goes back to zero, via corresponding calls to thaw_updates().
 */

/*** DEFINES ***/

/* priority used for all async callbacks in the main loop
 * This should be higher than redraw priorities so multiple callbacks
 * firing can be handled without intermediate redraws */
#define IO_PRIORITY G_PRIORITY_DEFAULT

/* random number that everyone else seems to use, too */
#define FILES_PER_QUERY 100

typedef struct _FileModelNode           FileModelNode;
typedef struct _GtkFileSystemModelClass GtkFileSystemModelClass;

struct _FileModelNode
{
  GFile *               file;           /* file represented by this node or NULL for editable */
  GFileInfo *           info;           /* info for this file or NULL if unknown */

  guint                 row;            /* if valid (see model->n_valid_indexes), visible nodes before and including
					 * this one - see the "Structure" comment above.
					 */

  guint                 visible :1;     /* if the file is currently visible */
  guint                 filtered_out :1;/* if the file is currently filtered out (i.e. it didn't pass the filters) */
  guint                 frozen_add :1;  /* true if the model was frozen and the entry has not been added yet */

  GValue                values[1];      /* actually n_columns values */
};

struct _GtkFileSystemModel
{
  GObject               parent_instance;

  GFile *               dir;            /* directory that's displayed */
  guint                 dir_thaw_source;/* GSource id for unfreezing the model */
  char *                attributes;     /* attributes the file info must contain, or NULL for all attributes */
  GFileMonitor *        dir_monitor;    /* directory that is monitored, or NULL if monitoring was not supported */

  GCancellable *        cancellable;    /* cancellable in use for all operations - cancelled on dispose */
  GArray *              files;          /* array of FileModelNode containing all our files */
  gsize                 node_size;	/* Size of a FileModelNode structure once its ->values field has n_columns */
  guint                 n_nodes_valid;  /* count of valid nodes (i.e. those whose node->row is accurate) */
  GHashTable *          file_lookup;    /* mapping of GFile => array index in model->files
					 * This hash table doesn't always have the same number of entries as the files array;
					 * it can get cleared completely when we resort.
					 * The hash table gets re-populated in node_get_for_file() if this mismatch is
					 * detected.
					 */

  guint                 n_columns;      /* number of columns */
  GType *               column_types;   /* types of each column */
  GtkFileSystemModelGetValue get_func;  /* function to call to fill in values in columns */
  gpointer              get_data;       /* data to pass to get_func */

  GtkFileFilter *       filter;         /* filter to use for deciding which nodes are visible */

  int                   sort_column_id; /* current sorting column */
  GtkSortType           sort_order;     /* current sorting order */
  GList *               sort_list;      /* list of sorting functions */
  GtkTreeIterCompareFunc default_sort_func; /* default sort function */
  gpointer              default_sort_data; /* data to pass to default sort func */
  GDestroyNotify        default_sort_destroy; /* function to call to destroy default_sort_data */

  guint                 frozen;         /* number of times we're frozen */

  gboolean              filter_on_thaw :1;/* set when filtering needs to happen upon thawing */
  gboolean              sort_on_thaw :1;/* set when sorting needs to happen upon thawing */

  guint                 show_hidden :1; /* whether to show hidden files */
  guint                 show_folders :1;/* whether to show folders */
  guint                 show_files :1;  /* whether to show files */
  guint                 filter_folders :1;/* whether filter applies to folders */
};

#define GTK_FILE_SYSTEM_MODEL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_FILE_SYSTEM_MODEL, GtkFileSystemModelClass))
#define GTK_IS_FILE_SYSTEM_MODEL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_FILE_SYSTEM_MODEL))
#define GTK_FILE_SYSTEM_MODEL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_FILE_SYSTEM_MODEL, GtkFileSystemModelClass))

struct _GtkFileSystemModelClass
{
  GObjectClass parent_class;

  /* Signals */

  void (*finished_loading) (GtkFileSystemModel *model, GError *error);
};

static void freeze_updates (GtkFileSystemModel *model);
static void thaw_updates (GtkFileSystemModel *model);

static guint node_get_for_file (GtkFileSystemModel *model,
				GFile              *file);

static void add_file (GtkFileSystemModel *model,
		      GFile              *file,
		      GFileInfo          *info);
static void remove_file (GtkFileSystemModel *model,
			 GFile              *file);

/* iter setup:
 * @user_data: the model
 * @user_data2: GUINT_TO_POINTER of array index of current entry
 *
 * All other fields are unused. Note that the array index does not corrspond
 * 1:1 with the path index as entries might not be visible.
 */
#define ITER_INDEX(iter) GPOINTER_TO_UINT((iter)->user_data2)
#define ITER_IS_VALID(model, iter) ((model) == (iter)->user_data)
#define ITER_INIT_FROM_INDEX(model, _iter, _index) G_STMT_START {\
  g_assert (_index < (model)->files->len); \
  (_iter)->user_data = (model); \
  (_iter)->user_data2 = GUINT_TO_POINTER (_index); \
}G_STMT_END

/*** FileModelNode ***/

/* Get a FileModelNode structure given an index in the model->files array of nodes */
#define get_node(_model, _index) ((FileModelNode *) ((_model)->files->data + (_index) * (_model)->node_size))

/* Get an index within the model->files array of nodes, given a FileModelNode* */
#define node_index(_model, _node) (((gchar *) (_node) - (_model)->files->data) / (_model)->node_size)

/* @up_to_index: smallest model->files array index that will be valid after this call
 * @up_to_row: smallest node->row that will be valid after this call
 *
 * If you want to validate up to an index or up to a row, specify the index or
 * the row you want and specify G_MAXUINT for the other argument.  Pass
 * G_MAXUINT for both arguments for "validate everything".
 */
static void
node_validate_rows (GtkFileSystemModel *model, guint up_to_index, guint up_to_row)
{
  guint i, row;

  if (model->files->len == 0)
    return;

  up_to_index = MIN (up_to_index, model->files->len - 1);

  i = model->n_nodes_valid;
  if (i != 0)
    row = get_node (model, i - 1)->row;
  else
    row = 0;

  while (i <= up_to_index && row <= up_to_row)
    {
      FileModelNode *node = get_node (model, i);
      if (node->visible)
        row++;
      node->row = row;
      i++;
    }
  model->n_nodes_valid = i;
}

static guint
node_get_tree_row (GtkFileSystemModel *model, guint index)
{
  if (model->n_nodes_valid <= index)
    node_validate_rows (model, index, G_MAXUINT);

  return get_node (model, index)->row - 1;
}

static void 
node_invalidate_index (GtkFileSystemModel *model, guint id)
{
  model->n_nodes_valid = MIN (model->n_nodes_valid, id);
}

static GtkTreePath *
tree_path_new_from_node (GtkFileSystemModel *model, guint id)
{
  guint r = node_get_tree_row (model, id);

  g_assert (r < model->files->len);

  return gtk_tree_path_new_from_indices (r, -1);
}

static void
emit_row_inserted_for_node (GtkFileSystemModel *model, guint id)
{
  GtkTreePath *path;
  GtkTreeIter iter;

  path = tree_path_new_from_node (model, id);
  ITER_INIT_FROM_INDEX (model, &iter, id);
  gtk_tree_model_row_inserted (GTK_TREE_MODEL (model), path, &iter);
  gtk_tree_path_free (path);
}

static void
emit_row_changed_for_node (GtkFileSystemModel *model, guint id)
{
  GtkTreePath *path;
  GtkTreeIter iter;

  path = tree_path_new_from_node (model, id);
  ITER_INIT_FROM_INDEX (model, &iter, id);
  gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, &iter);
  gtk_tree_path_free (path);
}

static void
emit_row_deleted_for_row (GtkFileSystemModel *model, guint row)
{
  GtkTreePath *path;

  path = gtk_tree_path_new_from_indices (row, -1);
  gtk_tree_model_row_deleted (GTK_TREE_MODEL (model), path);
  gtk_tree_path_free (path);
}

static void
node_set_visible_and_filtered_out (GtkFileSystemModel *model, guint id, gboolean visible, gboolean filtered_out)
{
  FileModelNode *node = get_node (model, id);

  /* Filteredness */

  if (node->filtered_out != filtered_out)
    {
      node->filtered_out = filtered_out;
      if (node->visible && visible)
        emit_row_changed_for_node (model, id);
    }

  /* Visibility */
  
  if (node->visible == visible ||
      node->frozen_add)
    return;

  if (visible)
    {
      node->visible = TRUE;
      node_invalidate_index (model, id);
      emit_row_inserted_for_node (model, id);
    }
  else
    {
      guint row;

      row = node_get_tree_row (model, id);
      g_assert (row < model->files->len);

      node->visible = FALSE;
      node_invalidate_index (model, id);
      emit_row_deleted_for_row (model, row);
    }
}

static gboolean
node_should_be_filtered_out (GtkFileSystemModel *model, guint id)
{
  FileModelNode *node = get_node (model, id);
  GtkFileFilterInfo filter_info = { 0, };
  GtkFileFilterFlags required;
  gboolean result;
  char *mime_type = NULL;
  char *filename = NULL;
  char *uri = NULL;

  if (node->info == NULL)
    return TRUE;

  if (model->filter == NULL)
    return FALSE;

  /* fill info */
  required = gtk_file_filter_get_needed (model->filter);

  filter_info.contains = GTK_FILE_FILTER_DISPLAY_NAME;
  filter_info.display_name = g_file_info_get_display_name (node->info);

  if (required & GTK_FILE_FILTER_MIME_TYPE)
    {
      const char *s = g_file_info_get_content_type (node->info);
      if (s)
	{
	  mime_type = g_content_type_get_mime_type (s);
	  if (mime_type)
	    {
	      filter_info.mime_type = mime_type;
	      filter_info.contains |= GTK_FILE_FILTER_MIME_TYPE;
	    }
	}
    }

  if (required & GTK_FILE_FILTER_FILENAME)
    {
      filename = g_file_get_path (node->file);
      if (filename)
        {
          filter_info.filename = filename;
	  filter_info.contains |= GTK_FILE_FILTER_FILENAME;
        }
    }

  if (required & GTK_FILE_FILTER_URI)
    {
      uri = g_file_get_uri (node->file);
      if (uri)
        {
          filter_info.uri = uri;
	  filter_info.contains |= GTK_FILE_FILTER_URI;
        }
    }

  result = !gtk_file_filter_filter (model->filter, &filter_info);

  g_free (mime_type);
  g_free (filename);
  g_free (uri);

  return result;
}

static gboolean
node_should_be_visible (GtkFileSystemModel *model, guint id, gboolean filtered_out)
{
  FileModelNode *node = get_node (model, id);
  gboolean result;

  if (node->info == NULL)
    return FALSE;

  if (!model->show_hidden &&
      (g_file_info_get_is_hidden (node->info) || g_file_info_get_is_backup (node->info)))
    return FALSE;

  if (_gtk_file_info_consider_as_directory (node->info))
    {
      if (!model->show_folders)
        return FALSE;

      if (!model->filter_folders)
        return TRUE;
    }
  else
    {
      if (!model->show_files)
        return FALSE;
    }

  result = !filtered_out;

  return result;
}

static void
node_compute_visibility_and_filters (GtkFileSystemModel *model, guint id)
{
  gboolean filtered_out;
  gboolean visible;

  filtered_out = node_should_be_filtered_out (model, id);
  visible = node_should_be_visible (model, id, filtered_out);

  node_set_visible_and_filtered_out (model, id, visible, filtered_out);
}

/*** GtkTreeModel ***/

static GtkTreeModelFlags
gtk_file_system_model_get_flags (GtkTreeModel *tree_model)
{
  /* GTK_TREE_MODEL_ITERS_PERSIST doesn't work with arrays :( */
  return GTK_TREE_MODEL_LIST_ONLY;
}

static gint
gtk_file_system_model_get_n_columns (GtkTreeModel *tree_model)
{
  GtkFileSystemModel *model = GTK_FILE_SYSTEM_MODEL (tree_model);
  
  return model->n_columns;
}

static GType
gtk_file_system_model_get_column_type (GtkTreeModel *tree_model,
				       gint          i)
{
  GtkFileSystemModel *model = GTK_FILE_SYSTEM_MODEL (tree_model);
  
  g_return_val_if_fail (i >= 0 && (guint) i < model->n_columns, G_TYPE_NONE);

  return model->column_types[i];
}

static int
compare_indices (gconstpointer key, gconstpointer _node)
{
  const FileModelNode *node = _node;

  return GPOINTER_TO_UINT (key) - node->row;
}

static gboolean
gtk_file_system_model_iter_nth_child (GtkTreeModel *tree_model,
				      GtkTreeIter  *iter,
				      GtkTreeIter  *parent,
				      gint          n)
{
  GtkFileSystemModel *model = GTK_FILE_SYSTEM_MODEL (tree_model);
  char *node;
  guint id;
  guint row_to_find;

  g_return_val_if_fail (n >= 0, FALSE);

  if (parent != NULL)
    return FALSE;

  row_to_find = n + 1; /* plus one as our node->row numbers are 1-based; see the "Structure" comment at the beginning */

  if (model->n_nodes_valid > 0 &&
      get_node (model, model->n_nodes_valid - 1)->row >= row_to_find)
    {
      /* Fast path - the nodes are valid up to the sought one.
       *
       * First, find a node with the sought row number...*/

      node = bsearch (GUINT_TO_POINTER (row_to_find), 
                      model->files->data,
                      model->n_nodes_valid,
                      model->node_size,
                      compare_indices);
      if (node == NULL)
        return FALSE;

      /* ... Second, back up until we find the first visible node with that row number */

      id = node_index (model, node);
      while (!get_node (model, id)->visible)
        id--;

      g_assert (get_node (model, id)->row == row_to_find);
    }
  else
    {
      /* Slow path - the nodes need to be validated up to the sought one */

      node_validate_rows (model, G_MAXUINT, n); /* note that this is really "n", not row_to_find - see node_validate_rows() */
      id = model->n_nodes_valid - 1;
      if (model->n_nodes_valid == 0 || get_node (model, id)->row != row_to_find)
        return FALSE;
    }

  ITER_INIT_FROM_INDEX (model, iter, id);
  return TRUE;
}

static gboolean
gtk_file_system_model_get_iter (GtkTreeModel *tree_model,
				GtkTreeIter  *iter,
				GtkTreePath  *path)
{
  g_return_val_if_fail (gtk_tree_path_get_depth (path) > 0, FALSE);

  if (gtk_tree_path_get_depth (path) > 1)
    return FALSE;

  return gtk_file_system_model_iter_nth_child (tree_model, 
                                               iter,
                                               NULL, 
                                               gtk_tree_path_get_indices (path)[0]);
}

static GtkTreePath *
gtk_file_system_model_get_path (GtkTreeModel *tree_model,
				GtkTreeIter  *iter)
{
  GtkFileSystemModel *model = GTK_FILE_SYSTEM_MODEL (tree_model);
      
  g_return_val_if_fail (ITER_IS_VALID (model, iter), NULL);

  return tree_path_new_from_node (model, ITER_INDEX (iter));
}

static void
gtk_file_system_model_get_value (GtkTreeModel *tree_model,
				 GtkTreeIter  *iter,
				 gint          column,
				 GValue       *value)
{
  GtkFileSystemModel *model = GTK_FILE_SYSTEM_MODEL (tree_model);
  const GValue *original;
  
  g_return_if_fail ((guint) column < model->n_columns);
  g_return_if_fail (ITER_IS_VALID (model, iter));

  original = _gtk_file_system_model_get_value (model, iter, column);
  if (original)
    {
      g_value_init (value, G_VALUE_TYPE (original));
      g_value_copy (original, value);
    }
  else
    g_value_init (value, model->column_types[column]);
}

static gboolean
gtk_file_system_model_iter_next (GtkTreeModel *tree_model,
				 GtkTreeIter  *iter)
{
  GtkFileSystemModel *model = GTK_FILE_SYSTEM_MODEL (tree_model);
  guint i;

  g_return_val_if_fail (ITER_IS_VALID (model, iter), FALSE);

  for (i = ITER_INDEX (iter) + 1; i < model->files->len; i++) 
    {
      FileModelNode *node = get_node (model, i);

      if (node->visible)
        {
          ITER_INIT_FROM_INDEX (model, iter, i);
          return TRUE;
        }
    }
      
  return FALSE;
}

static gboolean
gtk_file_system_model_iter_children (GtkTreeModel *tree_model,
				     GtkTreeIter  *iter,
				     GtkTreeIter  *parent)
{
  return FALSE;
}

static gboolean
gtk_file_system_model_iter_has_child (GtkTreeModel *tree_model,
				      GtkTreeIter  *iter)
{
  return FALSE;
}

static gint
gtk_file_system_model_iter_n_children (GtkTreeModel *tree_model,
				       GtkTreeIter  *iter)
{
  GtkFileSystemModel *model = GTK_FILE_SYSTEM_MODEL (tree_model);

  if (iter)
    return 0;

  return node_get_tree_row (model, model->files->len - 1) + 1;
}

static gboolean
gtk_file_system_model_iter_parent (GtkTreeModel *tree_model,
				   GtkTreeIter  *iter,
				   GtkTreeIter  *child)
{
  return FALSE;
}

static void
gtk_file_system_model_ref_node (GtkTreeModel *tree_model,
				GtkTreeIter  *iter)
{
  /* nothing to do */
}

static void
gtk_file_system_model_unref_node (GtkTreeModel *tree_model,
				  GtkTreeIter  *iter)
{
  /* nothing to do */
}

static void
gtk_file_system_model_iface_init (GtkTreeModelIface *iface)
{
  iface->get_flags =       gtk_file_system_model_get_flags;
  iface->get_n_columns =   gtk_file_system_model_get_n_columns;
  iface->get_column_type = gtk_file_system_model_get_column_type;
  iface->get_iter =        gtk_file_system_model_get_iter;
  iface->get_path =        gtk_file_system_model_get_path;
  iface->get_value =       gtk_file_system_model_get_value;
  iface->iter_next =       gtk_file_system_model_iter_next;
  iface->iter_children =   gtk_file_system_model_iter_children;
  iface->iter_has_child =  gtk_file_system_model_iter_has_child;
  iface->iter_n_children = gtk_file_system_model_iter_n_children;
  iface->iter_nth_child =  gtk_file_system_model_iter_nth_child;
  iface->iter_parent =     gtk_file_system_model_iter_parent;
  iface->ref_node =        gtk_file_system_model_ref_node;
  iface->unref_node =      gtk_file_system_model_unref_node;
}

/*** GtkTreeSortable ***/

typedef struct _SortData SortData;
struct _SortData {
  GtkFileSystemModel *    model;
  GtkTreeIterCompareFunc  func;
  gpointer                data;
  int                     order;        /* -1 to invert sort order or 1 to keep it */
};

/* returns FALSE if no sort necessary */
static gboolean
sort_data_init (SortData *data, GtkFileSystemModel *model)
{
  GtkTreeDataSortHeader *header;

  if (model->files->len <= 2)
    return FALSE;

  switch (model->sort_column_id)
    {
    case GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID:
      if (!model->default_sort_func)
        return FALSE;
      data->func = model->default_sort_func;
      data->data = model->default_sort_data;
      break;
    case GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID:
      return FALSE;
    default:
      header = _gtk_tree_data_list_get_header (model->sort_list, model->sort_column_id);
      if (header == NULL)
        return FALSE;
      data->func = header->func;
      data->data = header->data;
      break;
    }

  data->order = model->sort_order == GTK_SORT_DESCENDING ? -1 : 1;
  data->model = model;
  return TRUE;
}

static int
compare_array_element (gconstpointer a, gconstpointer b, gpointer user_data)
{
  SortData *data = user_data;
  GtkTreeIter itera, iterb;

  ITER_INIT_FROM_INDEX (data->model, &itera, node_index (data->model, a));
  ITER_INIT_FROM_INDEX (data->model, &iterb, node_index (data->model, b));
  return data->func (GTK_TREE_MODEL (data->model), &itera, &iterb, data->data) * data->order;
}

static void
gtk_file_system_model_sort (GtkFileSystemModel *model)
{
  SortData data;

  if (model->frozen)
    {
      model->sort_on_thaw = TRUE;
      return;
    }

  if (sort_data_init (&data, model))
    {
      GtkTreePath *path;
      guint i;
      guint r, n_visible_rows;

      node_validate_rows (model, G_MAXUINT, G_MAXUINT);
      n_visible_rows = node_get_tree_row (model, model->files->len - 1) + 1;
      model->n_nodes_valid = 0;
      g_hash_table_remove_all (model->file_lookup);
      g_qsort_with_data (get_node (model, 1), /* start at index 1; don't sort the editable row */
                         model->files->len - 1,
                         model->node_size,
                         compare_array_element,
                         &data);
      g_assert (model->n_nodes_valid == 0);
      g_assert (g_hash_table_size (model->file_lookup) == 0);
      if (n_visible_rows)
        {
          int *new_order = g_new (int, n_visible_rows);
        
          r = 0;
          for (i = 0; i < model->files->len; i++)
            {
              FileModelNode *node = get_node (model, i);
              if (!node->visible)
                {
                  node->row = r;
                  continue;
                }

              new_order[r] = node->row - 1;
              r++;
              node->row = r;
            }
          g_assert (r == n_visible_rows);
          path = gtk_tree_path_new ();
          gtk_tree_model_rows_reordered (GTK_TREE_MODEL (model),
                                         path,
                                         NULL,
                                         new_order);
          gtk_tree_path_free (path);
          g_free (new_order);
        }
    }

  model->sort_on_thaw = FALSE;
}

static void
gtk_file_system_model_sort_node (GtkFileSystemModel *model, guint node)
{
  /* FIXME: improve */
  gtk_file_system_model_sort (model);
}

static gboolean
gtk_file_system_model_get_sort_column_id (GtkTreeSortable  *sortable,
                                          gint             *sort_column_id,
                                          GtkSortType      *order)
{
  GtkFileSystemModel *model = GTK_FILE_SYSTEM_MODEL (sortable);

  if (sort_column_id)
    *sort_column_id = model->sort_column_id;
  if (order)
    *order = model->sort_order;

  if (model->sort_column_id == GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID ||
      model->sort_column_id == GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID)
    return FALSE;

  return TRUE;
}

static void
gtk_file_system_model_set_sort_column_id (GtkTreeSortable  *sortable,
                                          gint              sort_column_id,
                                          GtkSortType       order)
{
  GtkFileSystemModel *model = GTK_FILE_SYSTEM_MODEL (sortable);

  if ((model->sort_column_id == sort_column_id) &&
      (model->sort_order == order))
    return;

  if (sort_column_id != GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID)
    {
      if (sort_column_id != GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID)
	{
	  GtkTreeDataSortHeader *header = NULL;

	  header = _gtk_tree_data_list_get_header (model->sort_list, 
						   sort_column_id);

	  /* We want to make sure that we have a function */
	  g_return_if_fail (header != NULL);
	  g_return_if_fail (header->func != NULL);
	}
      else
	{
	  g_return_if_fail (model->default_sort_func != NULL);
	}
    }


  model->sort_column_id = sort_column_id;
  model->sort_order = order;

  gtk_tree_sortable_sort_column_changed (sortable);

  gtk_file_system_model_sort (model);
}

static void
gtk_file_system_model_set_sort_func (GtkTreeSortable        *sortable,
                                     gint                    sort_column_id,
                                     GtkTreeIterCompareFunc  func,
                                     gpointer                data,
                                     GDestroyNotify          destroy)
{
  GtkFileSystemModel *model = GTK_FILE_SYSTEM_MODEL (sortable);

  model->sort_list = _gtk_tree_data_list_set_header (model->sort_list, 
                                                     sort_column_id, 
                                                     func, data, destroy);

  if (model->sort_column_id == sort_column_id)
    gtk_file_system_model_sort (model);
}

static void
gtk_file_system_model_set_default_sort_func (GtkTreeSortable        *sortable,
                                             GtkTreeIterCompareFunc  func,
                                             gpointer                data,
                                             GDestroyNotify          destroy)
{
  GtkFileSystemModel *model = GTK_FILE_SYSTEM_MODEL (sortable);

  if (model->default_sort_destroy)
    {
      GDestroyNotify d = model->default_sort_destroy;

      model->default_sort_destroy = NULL;
      d (model->default_sort_data);
    }

  model->default_sort_func = func;
  model->default_sort_data = data;
  model->default_sort_destroy = destroy;

  if (model->sort_column_id == GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID)
    gtk_file_system_model_sort (model);
}

static gboolean
gtk_file_system_model_has_default_sort_func (GtkTreeSortable *sortable)
{
  GtkFileSystemModel *model = GTK_FILE_SYSTEM_MODEL (sortable);

  return (model->default_sort_func != NULL);
}

static void
gtk_file_system_model_sortable_init (GtkTreeSortableIface *iface)
{
  iface->get_sort_column_id = gtk_file_system_model_get_sort_column_id;
  iface->set_sort_column_id = gtk_file_system_model_set_sort_column_id;
  iface->set_sort_func = gtk_file_system_model_set_sort_func;
  iface->set_default_sort_func = gtk_file_system_model_set_default_sort_func;
  iface->has_default_sort_func = gtk_file_system_model_has_default_sort_func;
}

/*** GtkTreeDragSource ***/

static gboolean
drag_source_row_draggable (GtkTreeDragSource *drag_source,
			   GtkTreePath       *path)
{
  GtkFileSystemModel *model = GTK_FILE_SYSTEM_MODEL (drag_source);
  GtkTreeIter iter;

  if (!gtk_file_system_model_get_iter (GTK_TREE_MODEL (model), &iter, path))
    return FALSE;

  return ITER_INDEX (&iter) != 0;
}

static gboolean
drag_source_drag_data_get (GtkTreeDragSource *drag_source,
			   GtkTreePath       *path,
			   GtkSelectionData  *selection_data)
{
  GtkFileSystemModel *model = GTK_FILE_SYSTEM_MODEL (drag_source);
  FileModelNode *node;
  GtkTreeIter iter;
  char *uris[2]; 

  if (!gtk_file_system_model_get_iter (GTK_TREE_MODEL (model), &iter, path))
    return FALSE;

  node = get_node (model, ITER_INDEX (&iter));
  if (node->file == NULL)
    return FALSE;

  uris[0] = g_file_get_uri (node->file);
  uris[1] = NULL;
  gtk_selection_data_set_uris (selection_data, uris);
  g_free (uris[0]);

  return TRUE;
}

static void
drag_source_iface_init (GtkTreeDragSourceIface *iface)
{
  iface->row_draggable = drag_source_row_draggable;
  iface->drag_data_get = drag_source_drag_data_get;
  iface->drag_data_delete = NULL;
}

/*** GtkFileSystemModel ***/

/* Signal IDs */
enum {
  FINISHED_LOADING,
  LAST_SIGNAL
};

static guint file_system_model_signals[LAST_SIGNAL] = { 0 };



G_DEFINE_TYPE_WITH_CODE (GtkFileSystemModel, _gtk_file_system_model, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL,
						gtk_file_system_model_iface_init)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_SORTABLE,
						gtk_file_system_model_sortable_init)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_SOURCE,
						drag_source_iface_init))

static void
gtk_file_system_model_dispose (GObject *object)
{
  GtkFileSystemModel *model = GTK_FILE_SYSTEM_MODEL (object);

  if (model->dir_thaw_source)
    {
      g_source_remove (model->dir_thaw_source);
      model->dir_thaw_source = 0;
    }

  g_cancellable_cancel (model->cancellable);
  if (model->dir_monitor)
    g_file_monitor_cancel (model->dir_monitor);

  G_OBJECT_CLASS (_gtk_file_system_model_parent_class)->dispose (object);
}


static void
gtk_file_system_model_finalize (GObject *object)
{
  GtkFileSystemModel *model = GTK_FILE_SYSTEM_MODEL (object);
  guint i;

  for (i = 0; i < model->files->len; i++)
    {
      int v;

      FileModelNode *node = get_node (model, i);
      if (node->file)
        g_object_unref (node->file);
      if (node->info)
        g_object_unref (node->info);

      for (v = 0; v < model->n_columns; v++)
	if (G_VALUE_TYPE (&node->values[v]) != G_TYPE_INVALID)
	  g_value_unset (&node->values[v]);
    }
  g_array_free (model->files, TRUE);

  g_object_unref (model->cancellable);
  g_free (model->attributes);
  if (model->dir)
    g_object_unref (model->dir);
  if (model->dir_monitor)
    g_object_unref (model->dir_monitor);
  g_hash_table_destroy (model->file_lookup);
  if (model->filter)
    g_object_unref (model->filter);

  g_slice_free1 (sizeof (GType) * model->n_columns, model->column_types);

  _gtk_tree_data_list_header_free (model->sort_list);
  if (model->default_sort_destroy)
    model->default_sort_destroy (model->default_sort_data);

  G_OBJECT_CLASS (_gtk_file_system_model_parent_class)->finalize (object);
}

static void
_gtk_file_system_model_class_init (GtkFileSystemModelClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->finalize = gtk_file_system_model_finalize;
  gobject_class->dispose = gtk_file_system_model_dispose;

  file_system_model_signals[FINISHED_LOADING] =
    g_signal_new (I_("finished-loading"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkFileSystemModelClass, finished_loading),
		  NULL, NULL,
		  _gtk_marshal_VOID__POINTER,
		  G_TYPE_NONE, 1, G_TYPE_POINTER);
}

static void
_gtk_file_system_model_init (GtkFileSystemModel *model)
{
  model->show_files = TRUE;
  model->show_folders = TRUE;
  model->show_hidden = FALSE;
  model->filter_folders = FALSE;

  model->sort_column_id = GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID;

  model->file_lookup = g_hash_table_new (g_file_hash, (GEqualFunc) g_file_equal);
  model->cancellable = g_cancellable_new ();
}

/*** API ***/

static void
gtk_file_system_model_closed_enumerator (GObject *object, GAsyncResult *res, gpointer data)
{
  g_file_enumerator_close_finish (G_FILE_ENUMERATOR (object), res, NULL);
}

static gboolean
thaw_func (gpointer data)
{
  GtkFileSystemModel *model = data;

  thaw_updates (model);
  model->dir_thaw_source = 0;

  return FALSE;
}

static void
gtk_file_system_model_got_files (GObject *object, GAsyncResult *res, gpointer data)
{
  GFileEnumerator *enumerator = G_FILE_ENUMERATOR (object);
  GtkFileSystemModel *model = data;
  GList *walk, *files;
  GError *error = NULL;

  gdk_threads_enter ();

  files = g_file_enumerator_next_files_finish (enumerator, res, &error);

  if (files)
    {
      if (model->dir_thaw_source == 0)
        {
          freeze_updates (model);
          model->dir_thaw_source = gdk_threads_add_timeout_full (IO_PRIORITY + 1,
                                                                 50,
                                                                 thaw_func,
                                                                 model,
                                                                 NULL);
        }

      for (walk = files; walk; walk = walk->next)
        {
          const char *name;
          GFileInfo *info;
          GFile *file;
          
          info = walk->data;
          name = g_file_info_get_name (info);
          if (name == NULL)
            {
              /* Shouldn't happen, but the APIs allow it */
              g_object_unref (info);
              continue;
            }
          file = g_file_get_child (model->dir, name);
          add_file (model, file, info);
          g_object_unref (file);
          g_object_unref (info);
        }
      g_list_free (files);

      g_file_enumerator_next_files_async (enumerator,
					  g_file_is_native (model->dir) ? 50 * FILES_PER_QUERY : FILES_PER_QUERY,
					  IO_PRIORITY,
					  model->cancellable,
					  gtk_file_system_model_got_files,
					  model);
    }
  else
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_file_enumerator_close_async (enumerator,
                                         IO_PRIORITY,
                                         model->cancellable,
                                         gtk_file_system_model_closed_enumerator,
                                         NULL);
          if (model->dir_thaw_source != 0)
            {
              g_source_remove (model->dir_thaw_source);
              model->dir_thaw_source = 0;
              thaw_updates (model);
            }

          g_signal_emit (model, file_system_model_signals[FINISHED_LOADING], 0, error);
        }

      if (error)
        g_error_free (error);
    }

  gdk_threads_leave ();
}

static void
gtk_file_system_model_query_done (GObject *     object,
                                  GAsyncResult *res,
                                  gpointer      data)
{
  GtkFileSystemModel *model = data; /* only a valid pointer if not cancelled */
  GFile *file = G_FILE (object);
  GFileInfo *info;
  guint id;

  info = g_file_query_info_finish (file, res, NULL);
  if (info == NULL)
    return;

  gdk_threads_enter ();

  _gtk_file_system_model_update_file (model, file, info);

  id = node_get_for_file (model, file);
  gtk_file_system_model_sort_node (model, id);

  g_object_unref (info);

  gdk_threads_leave ();
}

static void
gtk_file_system_model_monitor_change (GFileMonitor *      monitor,
                                      GFile *             file,
                                      GFile *             other_file,
                                      GFileMonitorEvent   type,
                                      GtkFileSystemModel *model)
{
  switch (type)
    {
      case G_FILE_MONITOR_EVENT_CREATED:
      case G_FILE_MONITOR_EVENT_CHANGED:
      case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED:
        /* We can treat all of these the same way */
        g_file_query_info_async (file,
                                 model->attributes,
                                 G_FILE_QUERY_INFO_NONE,
                                 IO_PRIORITY,
                                 model->cancellable,
                                 gtk_file_system_model_query_done,
                                 model);
        break;
      case G_FILE_MONITOR_EVENT_DELETED:
	gdk_threads_enter ();
        remove_file (model, file);
	gdk_threads_leave ();
        break;
      case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
        /* FIXME: use freeze/thaw with this somehow? */
      case G_FILE_MONITOR_EVENT_PRE_UNMOUNT:
      case G_FILE_MONITOR_EVENT_UNMOUNTED:
      default:
        /* ignore these */
        break;
    }
}

static void
gtk_file_system_model_got_enumerator (GObject *dir, GAsyncResult *res, gpointer data)
{
  GtkFileSystemModel *model = data;
  GFileEnumerator *enumerator;
  GError *error = NULL;

  gdk_threads_enter ();

  enumerator = g_file_enumerate_children_finish (G_FILE (dir), res, &error);
  if (enumerator == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      {
        g_signal_emit (model, file_system_model_signals[FINISHED_LOADING], 0, error);
        g_error_free (error);
      }
    }
  else
    {
      g_file_enumerator_next_files_async (enumerator,
                                          g_file_is_native (model->dir) ? 50 * FILES_PER_QUERY : FILES_PER_QUERY,
                                          IO_PRIORITY,
                                          model->cancellable,
                                          gtk_file_system_model_got_files,
                                          model);
      g_object_unref (enumerator);
      model->dir_monitor = g_file_monitor_directory (model->dir,
                                                     G_FILE_MONITOR_NONE,
                                                     model->cancellable,
                                                     NULL); /* we don't mind if directory monitoring isn't supported, so the GError is NULL here */
      if (model->dir_monitor)
        g_signal_connect (model->dir_monitor,
                          "changed",
                          G_CALLBACK (gtk_file_system_model_monitor_change),
                          model);
    }

  gdk_threads_leave ();
}

static void
gtk_file_system_model_set_n_columns (GtkFileSystemModel *model,
                                     gint                n_columns,
                                     va_list             args)
{
  guint i;

  g_assert (model->files == NULL);
  g_assert (n_columns > 0);

  model->n_columns = n_columns;
  model->column_types = g_slice_alloc (sizeof (GType) * n_columns);

  model->node_size = sizeof (FileModelNode) + sizeof (GValue) * (n_columns - 1); /* minus 1 because FileModelNode.values[] has a default size of 1 */

  for (i = 0; i < (guint) n_columns; i++)
    {
      GType type = va_arg (args, GType);
      if (! _gtk_tree_data_list_check_type (type))
	{
	  g_error ("%s: type %s cannot be a column type for GtkFileSystemModel\n", G_STRLOC, g_type_name (type));
          return; /* not reached */
	}

      model->column_types[i] = type;
    }

  model->sort_list = _gtk_tree_data_list_header_new (n_columns, model->column_types);

  model->files = g_array_sized_new (FALSE, FALSE, model->node_size, FILES_PER_QUERY);
  /* add editable node at start */
  g_array_set_size (model->files, 1);
  memset (get_node (model, 0), 0, model->node_size);
}

static void
gtk_file_system_model_set_directory (GtkFileSystemModel *model,
                                     GFile *             dir,
			             const gchar *       attributes)
{
  g_assert (G_IS_FILE (dir));

  model->dir = g_object_ref (dir);
  model->attributes = g_strdup (attributes);

  g_file_enumerate_children_async (model->dir,
                                   attributes,
                                   G_FILE_QUERY_INFO_NONE,
                                   IO_PRIORITY,
                                   model->cancellable,
                                   gtk_file_system_model_got_enumerator,
                                   model);

}

static GtkFileSystemModel *
_gtk_file_system_model_new_valist (GtkFileSystemModelGetValue get_func,
                                   gpointer            get_data,
                                   guint               n_columns,
                                   va_list             args)
{
  GtkFileSystemModel *model;

  model = g_object_new (GTK_TYPE_FILE_SYSTEM_MODEL, NULL);
  model->get_func = get_func;
  model->get_data = get_data;

  gtk_file_system_model_set_n_columns (model, n_columns, args);

  return model;
}

/**
 * _gtk_file_system_model_new:
 * @get_func: function to call for getting a value
 * @get_data: user data argument passed to @get_func
 * @n_columns: number of columns
 * @...: @n_columns #GType types for the columns
 *
 * Creates a new #GtkFileSystemModel object. You need to add files
 * to the list using _gtk_file_system_model_add_and_query_file()
 * or _gtk_file_system_model_update_file().
 *
 * Return value: the newly created #GtkFileSystemModel
 **/
GtkFileSystemModel *
_gtk_file_system_model_new (GtkFileSystemModelGetValue get_func,
                            gpointer            get_data,
                            guint               n_columns,
                            ...)
{
  GtkFileSystemModel *model;
  va_list args;

  g_return_val_if_fail (get_func != NULL, NULL);
  g_return_val_if_fail (n_columns > 0, NULL);

  va_start (args, n_columns);
  model = _gtk_file_system_model_new_valist (get_func, get_data, n_columns, args);
  va_end (args);

  return model;
}

/**
 * _gtk_file_system_model_new_for_directory:
 * @directory: the directory to show.
 * @attributes: (allow-none): attributes to immediately load or %NULL for all
 * @get_func: function that the model should call to query data about a file
 * @get_data: user data to pass to the @get_func
 * @n_columns: number of columns
 * @...: @n_columns #GType types for the columns
 *
 * Creates a new #GtkFileSystemModel object. The #GtkFileSystemModel
 * object wraps the given @directory as a #GtkTreeModel.
 * The model will query the given directory with the given @attributes
 * and add all files inside the directory automatically. If supported,
 * it will also monitor the drectory and update the model's
 * contents to reflect changes, if the @directory supports monitoring.
 * 
 * Return value: the newly created #GtkFileSystemModel
 **/
GtkFileSystemModel *
_gtk_file_system_model_new_for_directory (GFile *                    dir,
                                          const gchar *              attributes,
                                          GtkFileSystemModelGetValue get_func,
                                          gpointer                   get_data,
                                          guint                      n_columns,
                                          ...)
{
  GtkFileSystemModel *model;
  va_list args;

  g_return_val_if_fail (G_IS_FILE (dir), NULL);
  g_return_val_if_fail (get_func != NULL, NULL);
  g_return_val_if_fail (n_columns > 0, NULL);

  va_start (args, n_columns);
  model = _gtk_file_system_model_new_valist (get_func, get_data, n_columns, args);
  va_end (args);

  gtk_file_system_model_set_directory (model, dir, attributes);

  return model;
}

static void
gtk_file_system_model_refilter_all (GtkFileSystemModel *model)
{
  guint i;

  if (model->frozen)
    {
      model->filter_on_thaw = TRUE;
      return;
    }

  freeze_updates (model);

  /* start at index 1, don't change the editable */
  for (i = 1; i < model->files->len; i++)
    node_compute_visibility_and_filters (model, i);

  model->filter_on_thaw = FALSE;
  thaw_updates (model);
}

/**
 * _gtk_file_system_model_set_show_hidden:
 * @model: a #GtkFileSystemModel
 * @show_hidden: whether hidden files should be displayed
 * 
 * Sets whether hidden files should be included in the #GtkTreeModel
 * for display.
 **/
void
_gtk_file_system_model_set_show_hidden (GtkFileSystemModel *model,
					gboolean            show_hidden)
{
  g_return_if_fail (GTK_IS_FILE_SYSTEM_MODEL (model));

  show_hidden = show_hidden != FALSE;

  if (show_hidden != model->show_hidden)
    {
      model->show_hidden = show_hidden;
      gtk_file_system_model_refilter_all (model);
    }
}

/**
 * _gtk_file_system_model_set_show_folders:
 * @model: a #GtkFileSystemModel
 * @show_folders: whether folders should be displayed
 * 
 * Sets whether folders should be included in the #GtkTreeModel for
 * display.
 **/
void
_gtk_file_system_model_set_show_folders (GtkFileSystemModel *model,
					 gboolean            show_folders)
{
  g_return_if_fail (GTK_IS_FILE_SYSTEM_MODEL (model));

  show_folders = show_folders != FALSE;

  if (show_folders != model->show_folders)
    {
      model->show_folders = show_folders;
      gtk_file_system_model_refilter_all (model);
    }
}

/**
 * _gtk_file_system_model_set_show_files:
 * @model: a #GtkFileSystemModel
 * @show_files: whether files (as opposed to folders) should
 *              be displayed.
 * 
 * Sets whether files (as opposed to folders) should be included
 * in the #GtkTreeModel for display.
 **/
void
_gtk_file_system_model_set_show_files (GtkFileSystemModel *model,
				       gboolean            show_files)
{
  g_return_if_fail (GTK_IS_FILE_SYSTEM_MODEL (model));

  show_files = show_files != FALSE;

  if (show_files != model->show_files)
    {
      model->show_files = show_files;
      gtk_file_system_model_refilter_all (model);
    }
}

/**
 * _gtk_file_system_model_set_filter_folders:
 * @model: a #GtkFileSystemModel
 * @filter_folders: whether the filter applies to folders
 * 
 * Sets whether the filter set by _gtk_file_system_model_set_filter()
 * applies to folders. By default, it does not and folders are always
 * visible.
 **/
void
_gtk_file_system_model_set_filter_folders (GtkFileSystemModel *model,
					   gboolean            filter_folders)
{
  g_return_if_fail (GTK_IS_FILE_SYSTEM_MODEL (model));

  filter_folders = filter_folders != FALSE;

  if (filter_folders != model->filter_folders)
    {
      model->filter_folders = filter_folders;
      gtk_file_system_model_refilter_all (model);
    }
}

/**
 * _gtk_file_system_model_get_cancellable:
 * @model: the model
 *
 * Gets the cancellable used by the @model. This is the cancellable used
 * internally by the @model that will be cancelled when @model is 
 * disposed. So you can use it for operations that should be cancelled
 * when the model goes away.
 *
 * Returns: The cancellable used by @model
 **/
GCancellable *
_gtk_file_system_model_get_cancellable (GtkFileSystemModel *model)
{
  g_return_val_if_fail (GTK_IS_FILE_SYSTEM_MODEL (model), NULL);

  return model->cancellable;
}

/**
 * _gtk_file_system_model_iter_is_visible:
 * @model: the model
 * @iter: a valid iterator
 *
 * Checks if the iterator is visible. A visible iterator references
 * a row that is currently exposed using the #GtkTreeModel API. If
 * the iterator is invisible, it references a file that is not shown
 * for some reason, such as being filtered out by the current filter or
 * being a hidden file.
 *
 * Returns: %TRUE if the iterator is visible
 **/
gboolean
_gtk_file_system_model_iter_is_visible (GtkFileSystemModel *model,
					GtkTreeIter        *iter)
{
  FileModelNode *node;

  g_return_val_if_fail (GTK_IS_FILE_SYSTEM_MODEL (model), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);

  node = get_node (model, ITER_INDEX (iter));
  return node->visible;
}

/**
 * _gtk_file_system_model_iter_is_filtered_out:
 * @model: the model
 * @iter: a valid iterator
 *
 * Checks if the iterator is filtered out.  This is only useful for rows
 * that refer to folders, as those are always visible regardless
 * of what the current filter says.  This function lets you see
 * the results of the filter.
 *
 * Returns: %TRUE if the iterator passed the current filter; %FALSE if the
 * filter would not have let the row pass.
 **/
gboolean
_gtk_file_system_model_iter_is_filtered_out (GtkFileSystemModel *model,
					     GtkTreeIter        *iter)
{
  FileModelNode *node;

  g_return_val_if_fail (GTK_IS_FILE_SYSTEM_MODEL (model), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);

  node = get_node (model, ITER_INDEX (iter));
  return node->filtered_out;
}

/**
 * _gtk_file_system_model_get_info:
 * @model: a #GtkFileSystemModel
 * @iter: a #GtkTreeIter pointing to a row of @model
 * 
 * Gets the #GFileInfo structure for a particular row
 * of @model.
 * 
 * Return value: a #GFileInfo structure. This structure
 *   is owned by @model and must not be modified or freed.
 *   If you want to keep the information for later use,
 *   you must take a reference, since the structure may be
 *   freed on later changes to the file system.  If you have
 *   called _gtk_file_system_model_add_editable() and the @iter
 *   corresponds to the row that this function returned, the
 *   return value will be NULL.
 **/
GFileInfo *
_gtk_file_system_model_get_info (GtkFileSystemModel *model,
				 GtkTreeIter        *iter)
{
  FileModelNode *node;

  g_return_val_if_fail (GTK_IS_FILE_SYSTEM_MODEL (model), NULL);
  g_return_val_if_fail (iter != NULL, NULL);

  node = get_node (model, ITER_INDEX (iter));
  g_assert (node->info == NULL || G_IS_FILE_INFO (node->info));
  return node->info;
}

/**
 * _gtk_file_system_model_get_file:
 * @model: a #GtkFileSystemModel
 * @iter: a #GtkTreeIter pointing to a row of @model
 * 
 * Gets the file for a particular row in @model. 
 *
 * Return value: the file. This object is owned by @model and
 *   or freed. If you want to save the path for later use,
 *   you must take a ref, since the object may be freed
 *   on later changes to the file system.
 **/
GFile *
_gtk_file_system_model_get_file (GtkFileSystemModel *model,
				 GtkTreeIter        *iter)
{
  FileModelNode *node;

  g_return_val_if_fail (GTK_IS_FILE_SYSTEM_MODEL (model), NULL);

  node = get_node (model, ITER_INDEX (iter));
  return node->file;
}

/**
 * _gtk_file_system_model_get_value:
 * @model: a #GtkFileSystemModel
 * @iter: a #GtkTreeIter pointing to a row of @model
 * @column: the column to get the value for
 *
 * Gets the value associated with the given row @iter and @column.
 * If no value is available yet and the default value should be used,
 * %NULL is returned.
 * This is a performance optimization for the calls 
 * gtk_tree_model_get() or gtk_tree_model_get_value(), which copy 
 * the value and spend a considerable amount of time in iterator 
 * lookups. Both of which are slow.
 *
 * Returns: a pointer to the actual value as stored in @model or %NULL
 *          if no value available yet.
 **/
const GValue *
_gtk_file_system_model_get_value (GtkFileSystemModel *model,
                                  GtkTreeIter *       iter,
                                  int                 column)
{
  FileModelNode *node;

  g_return_val_if_fail (GTK_IS_FILE_SYSTEM_MODEL (model), NULL);
  g_return_val_if_fail (column >= 0 && (guint) column < model->n_columns, NULL);

  node = get_node (model, ITER_INDEX (iter));
    
  if (!G_VALUE_TYPE (&node->values[column]))
    {
      g_value_init (&node->values[column], model->column_types[column]);
      if (!model->get_func (model, 
                            node->file, 
                            node->info, 
                            column, 
                            &node->values[column],
                            model->get_data))
        {
          g_value_unset (&node->values[column]);
          return NULL;
        }
    }
  
  return &node->values[column];
}

static guint
node_get_for_file (GtkFileSystemModel *model,
                   GFile *             file)
{
  guint i;

  i = GPOINTER_TO_UINT (g_hash_table_lookup (model->file_lookup, file));
  if (i != 0)
    return i;

  /* Node 0 is the editable row and has no associated file or entry in the table, so we start counting from 1.
   *
   * The invariant here is that the files in model->files[n] for n < g_hash_table_size (model->file_lookup)
   * are already added to the hash table. The table can get cleared when we re-sort; this loop merely rebuilds
   * our (file -> index) mapping on demand.
   *
   * If we exit the loop, the next pending batch of mappings will be resolved when this function gets called again
   * with another file that is not yet in the mapping.
   */
  for (i = g_hash_table_size (model->file_lookup) + 1; i < model->files->len; i++)
    {
      FileModelNode *node = get_node (model, i);

      g_hash_table_insert (model->file_lookup, node->file, GUINT_TO_POINTER (i));
      if (g_file_equal (node->file, file))
        return i;
    }

  return 0;
}

/**
 * _gtk_file_system_model_get_iter_for_file:
 * @model: the model
 * @iter: the iterator to be initialized
 * @file: the file to look up
 *
 * Initializes @iter to point to the row used for @file, if @file is part 
 * of the model. Note that upon successful return, @iter may point to an 
 * invisible row in the @model. Use 
 * _gtk_file_system_model_iter_is_visible() to make sure it is visible to
 * the tree view.
 *
 * Returns: %TRUE if file is part of the model and @iter was initialized
 **/
gboolean
_gtk_file_system_model_get_iter_for_file (GtkFileSystemModel *model,
					  GtkTreeIter        *iter,
					  GFile *             file)
{
  guint i;

  g_return_val_if_fail (GTK_IS_FILE_SYSTEM_MODEL (model), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (G_IS_FILE (file), FALSE);

  i = node_get_for_file (model, file);

  if (i == 0)
    return FALSE;

  ITER_INIT_FROM_INDEX (model, iter, i);
  return TRUE;
}

/* When an element is added or removed to the model->files array, we need to
 * update the model->file_lookup mappings of (node, index), as the indexes
 * change.  This function adds the specified increment to the index in that pair
 * if the index is equal or after the specified id.  We use this to slide the
 * mappings up or down when a node is added or removed, respectively.
 */
static void
adjust_file_lookup (GtkFileSystemModel *model, guint id, int increment)
{
  GHashTableIter iter;
  gpointer key;
  gpointer value;

  g_hash_table_iter_init (&iter, model->file_lookup);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      guint index = GPOINTER_TO_UINT (value);

      if (index >= id)
	{
	  index += increment;
	  g_hash_table_iter_replace (&iter, GUINT_TO_POINTER (index));
	}
    }
}

/**
 * add_file:
 * @model: the model
 * @file: the file to add
 * @info: the information to associate with the file
 *
 * Adds the given @file with its associated @info to the @model. 
 * If the model is frozen, the file will only show up after it is thawn.
 **/
static void
add_file (GtkFileSystemModel *model,
	  GFile              *file,
	  GFileInfo          *info)
{
  FileModelNode *node;
  
  g_return_if_fail (GTK_IS_FILE_SYSTEM_MODEL (model));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (G_IS_FILE_INFO (info));

  node = g_slice_alloc0 (model->node_size);
  node->file = g_object_ref (file);
  if (info)
    node->info = g_object_ref (info);
  node->frozen_add = model->frozen ? TRUE : FALSE;

  g_array_append_vals (model->files, node, 1);
  g_slice_free1 (model->node_size, node);

  if (!model->frozen)
    node_compute_visibility_and_filters (model, model->files->len -1);

  gtk_file_system_model_sort_node (model, model->files->len -1);
}

/**
 * remove_file:
 * @model: the model
 * @file: file to remove from the model. The file must have been 
 *        added to the model previously
 *
 * Removes the given file from the model. If the file is not part of 
 * @model, this function does nothing.
 **/
static void
remove_file (GtkFileSystemModel *model,
	     GFile              *file)
{
  FileModelNode *node;
  gboolean was_visible;
  guint id;
  guint row;

  g_return_if_fail (GTK_IS_FILE_SYSTEM_MODEL (model));
  g_return_if_fail (G_IS_FILE (file));

  id = node_get_for_file (model, file);
  if (id == 0)
    return;

  node = get_node (model, id);
  was_visible = node->visible;
  row = node_get_tree_row (model, id);

  node_invalidate_index (model, id);

  g_hash_table_remove (model->file_lookup, file);
  g_object_unref (node->file);
  adjust_file_lookup (model, id, -1);

  if (node->info)
    g_object_unref (node->info);

  g_array_remove_index (model->files, id);

  /* We don't need to resort, as removing a row doesn't change the sorting order of the other rows */

  if (was_visible)
    emit_row_deleted_for_row (model, row);
}

/**
 * _gtk_file_system_model_update_file:
 * @model: the model
 * @file: the file
 * @info: the new file info
 *
 * Tells the file system model that the file changed and that the 
 * new @info should be used for it now.  If the file is not part of 
 * @model, it will get added automatically.
 **/
void
_gtk_file_system_model_update_file (GtkFileSystemModel *model,
                                    GFile              *file,
                                    GFileInfo          *info)
{
  FileModelNode *node;
  guint i, id;
  GFileInfo *old_info;

  g_return_if_fail (GTK_IS_FILE_SYSTEM_MODEL (model));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (G_IS_FILE_INFO (info));

  id = node_get_for_file (model, file);
  if (id == 0)
    {
      add_file (model, file, info);
      id = node_get_for_file (model, file);
    }

  node = get_node (model, id);

  old_info = node->info;
  node->info = g_object_ref (info);
  if (old_info)
    g_object_unref (old_info);

  for (i = 0; i < model->n_columns; i++)
    {
      if (G_VALUE_TYPE (&node->values[i]))
        g_value_unset (&node->values[i]);
    }

  if (node->visible)
    emit_row_changed_for_node (model, id);
}

/**
 * _gtk_file_system_model_set_filter:
 * @mode: a #GtkFileSystemModel
 * @filter: (allow-none): %NULL or filter to use
 * 
 * Sets a filter to be used for deciding if a row should be visible or not.
 * Whether this filter applies to directories can be toggled with
 * _gtk_file_system_model_set_filter_folders().
 **/
void
_gtk_file_system_model_set_filter (GtkFileSystemModel      *model,
				   GtkFileFilter *          filter)
{
  GtkFileFilter *old_filter;

  g_return_if_fail (GTK_IS_FILE_SYSTEM_MODEL (model));
  g_return_if_fail (filter == NULL || GTK_IS_FILE_FILTER (filter));
  
  if (filter)
    g_object_ref (filter);

  old_filter = model->filter;
  model->filter = filter;

  if (old_filter)
    g_object_unref (old_filter);

  gtk_file_system_model_refilter_all (model);
}

/**
 * freeze_updates:
 * @model: a #GtkFileSystemModel
 *
 * Freezes most updates on the model, so that performing multiple operations on
 * the files in the model do not cause any events.  Use thaw_updates() to resume
 * proper operations. It is fine to call this function multiple times as long as
 * freeze and thaw calls are balanced.
 **/
static void
freeze_updates (GtkFileSystemModel *model)
{
  g_return_if_fail (GTK_IS_FILE_SYSTEM_MODEL (model));

  model->frozen++;
}

/**
 * thaw_updates:
 * @model: a #GtkFileSystemModel
 *
 * Undoes the effect of a previous call to freeze_updates() 
 **/
static void
thaw_updates (GtkFileSystemModel *model)
{
  gboolean stuff_added;

  g_return_if_fail (GTK_IS_FILE_SYSTEM_MODEL (model));
  g_return_if_fail (model->frozen > 0);

  model->frozen--;
  if (model->frozen > 0)
    return;

  stuff_added = get_node (model, model->files->len - 1)->frozen_add;

  if (model->filter_on_thaw)
    gtk_file_system_model_refilter_all (model);
  if (model->sort_on_thaw)
    gtk_file_system_model_sort (model);
  if (stuff_added)
    {
      guint i;

      for (i = 0; i < model->files->len; i++)
        {
          FileModelNode *node = get_node (model, i);

          if (!node->frozen_add)
            continue;
          node->frozen_add = FALSE;
          node_compute_visibility_and_filters (model, i);
        }
    }
}

/**
 * _gtk_file_system_model_clear_cache:
 * @model: a #GtkFileSystemModel
 * @column: the column to clear or -1 for all columns
 *
 * Clears the cached values in the model for the given @column. Use 
 * this function whenever your get_value function would return different
 * values for a column.
 * The file chooser uses this for example when the icon theme changes to 
 * invalidate the cached pixbufs.
 **/
void
_gtk_file_system_model_clear_cache (GtkFileSystemModel *model,
                                    int                 column)
{
  guint i;
  int start, end;
  gboolean changed;

  g_return_if_fail (GTK_IS_FILE_SYSTEM_MODEL (model));
  g_return_if_fail (column >= -1 && (guint) column < model->n_columns);

  if (column > -1)
    {
      start = column;
      end = column + 1;
    }
  else
    {
      start = 0;
      end = model->n_columns;
    }

  for (i = 0; i < model->files->len; i++)
    {
      FileModelNode *node = get_node (model, i);
      changed = FALSE;
      for (column = start; column < end; column++)
        {
          if (!G_VALUE_TYPE (&node->values[column]))
            continue;
          
          g_value_unset (&node->values[column]);
          changed = TRUE;
        }

      if (changed && node->visible)
	emit_row_changed_for_node (model, i);
    }

  /* FIXME: resort? */
}

/**
 * _gtk_file_system_model_add_and_query_file:
 * @model: a #GtkFileSystemModel
 * @file: the file to add
 * @attributes: attributes to query before adding the file
 *
 * This is a conenience function that calls g_file_query_info_async() on 
 * the given file, and when successful, adds it to the model.
 * Upon failure, the @file is discarded.
 **/
void
_gtk_file_system_model_add_and_query_file (GtkFileSystemModel *model,
                                           GFile *             file,
                                           const char *        attributes)
{
  g_return_if_fail (GTK_IS_FILE_SYSTEM_MODEL (model));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (attributes != NULL);

  g_file_query_info_async (file,
                           attributes,
                           G_FILE_QUERY_INFO_NONE,
                           IO_PRIORITY,
                           model->cancellable,
                           gtk_file_system_model_query_done,
                           model);
}

/**
 * _gtk_file_system_model_add_editable:
 * @model: a #GtkFileSystemModel
 * @iter: Location to return the iter corresponding to the editable row
 * 
 * Adds an “empty” row at the beginning of the model.  This does not refer to
 * any file, but is a temporary placeholder for a file name that the user will
 * type when a corresponding cell is made editable.  When your code is done
 * using this temporary row, call _gtk_file_system_model_remove_editable().
 **/
void
_gtk_file_system_model_add_editable (GtkFileSystemModel *model, GtkTreeIter *iter)
{
  g_return_if_fail (GTK_IS_FILE_SYSTEM_MODEL (model));
  g_return_if_fail (!get_node (model, 0)->visible);

  node_set_visible_and_filtered_out (model, 0, TRUE, FALSE);
  ITER_INIT_FROM_INDEX (model, iter, 0);

  /* we don't want file system changes to affect the model while
   * editing is in place
   */
  freeze_updates (model);
}

/**
 * _gtk_file_system_model_remove_editable:
 * @model: a #GtkFileSystemModel
 * 
 * Removes the “empty” row at the beginning of the model that was
 * created with _gtk_file_system_model_add_editable().  You should call
 * this function when your code is finished editing this temporary row.
 **/
void
_gtk_file_system_model_remove_editable (GtkFileSystemModel *model)
{
  g_return_if_fail (GTK_IS_FILE_SYSTEM_MODEL (model));
  g_return_if_fail (get_node (model, 0)->visible);

  thaw_updates (model);

  node_set_visible_and_filtered_out (model, 0, FALSE, FALSE);
}
