/* gtktreemodelfilter.c
 * Copyright (C) 2000,2001  Red Hat, Inc., Jonathan Blandford <jrb@redhat.com>
 * Copyright (C) 2001-2003  Kristian Rietveld <kris@gtk.org>
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
#include "gtktreemodelfilter.h"
#include "gtkintl.h"
#include "gtktreednd.h"
#include "gtkprivate.h"
#include "gtkalias.h"
#include <string.h>

/* ITER FORMAT:
 *
 * iter->stamp = filter->priv->stamp
 * iter->user_data = FilterLevel
 * iter->user_data2 = FilterElt
 */

/* all paths, iters, etc prefixed with c_ are paths, iters, etc relative to the
 * child model.
 */

/* A few notes:
 *  There are three model/views involved, so there are two mappings:
 *    * this model -> child model: mapped via offset in FilterElt.
 *    * this model -> parent model (or view): mapped via the array index
 *                                            of FilterElt.
 *
 *  Note that there are two kinds of paths relative to the filter model
 *  (those generated from the array indices): paths taking non-visible
 *  nodes into account, and paths which don't.  Paths which take
 *  non-visible nodes into account should only be used internally and
 *  NEVER be passed along with a signal emission.
 *
 *  The filter model has a reference on every node that is not in the root
 *  level and has a parent with ref_count > 1.  Exception is a virtual root
 *  level; all nodes in the virtual root level are referenced too.
 */

typedef struct _FilterElt FilterElt;
typedef struct _FilterLevel FilterLevel;

struct _FilterElt
{
  GtkTreeIter iter;
  FilterLevel *children;
  gint offset;
  gint ref_count;
  gint zero_ref_count;
  gboolean visible;
};

struct _FilterLevel
{
  GArray *array;
  gint ref_count;
  gint visible_nodes;

  gint parent_elt_index;
  FilterLevel *parent_level;
};

#define GTK_TREE_MODEL_FILTER_GET_PRIVATE(obj)  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GTK_TYPE_TREE_MODEL_FILTER, GtkTreeModelFilterPrivate))

struct _GtkTreeModelFilterPrivate
{
  gpointer root;
  gint stamp;
  guint child_flags;
  GtkTreeModel *child_model;
  gint zero_ref_count;

  GtkTreePath *virtual_root;

  GtkTreeModelFilterVisibleFunc visible_func;
  gpointer visible_data;
  GDestroyNotify visible_destroy;

  gint modify_n_columns;
  GType *modify_types;
  GtkTreeModelFilterModifyFunc modify_func;
  gpointer modify_data;
  GDestroyNotify modify_destroy;

  gint visible_column;

  gboolean visible_method_set;
  gboolean modify_func_set;

  gboolean in_row_deleted;
  gboolean virtual_root_deleted;

  /* signal ids */
  guint changed_id;
  guint inserted_id;
  guint has_child_toggled_id;
  guint deleted_id;
  guint reordered_id;
};

/* properties */
enum
{
  PROP_0,
  PROP_CHILD_MODEL,
  PROP_VIRTUAL_ROOT
};

#define GTK_TREE_MODEL_FILTER_CACHE_CHILD_ITERS(filter) \
        (((GtkTreeModelFilter *)filter)->priv->child_flags & GTK_TREE_MODEL_ITERS_PERSIST)

#define FILTER_ELT(filter_elt) ((FilterElt *)filter_elt)
#define FILTER_LEVEL(filter_level) ((FilterLevel *)filter_level)

#define FILTER_LEVEL_PARENT_ELT(level) (&g_array_index (FILTER_LEVEL ((level))->parent_level->array, FilterElt, FILTER_LEVEL ((level))->parent_elt_index))
#define FILTER_LEVEL_ELT_INDEX(level, elt) (FILTER_ELT ((elt)) - FILTER_ELT (FILTER_LEVEL ((level))->array->data))

/* general code (object/interface init, properties, etc) */
static void         gtk_tree_model_filter_tree_model_init                 (GtkTreeModelIface       *iface);
static void         gtk_tree_model_filter_drag_source_init                (GtkTreeDragSourceIface  *iface);
static void         gtk_tree_model_filter_finalize                        (GObject                 *object);
static void         gtk_tree_model_filter_set_property                    (GObject                 *object,
                                                                           guint                    prop_id,
                                                                           const GValue            *value,
                                                                           GParamSpec              *pspec);
static void         gtk_tree_model_filter_get_property                    (GObject                 *object,
                                                                           guint                    prop_id,
                                                                           GValue                 *value,
                                                                           GParamSpec             *pspec);

/* signal handlers */
static void         gtk_tree_model_filter_row_changed                     (GtkTreeModel           *c_model,
                                                                           GtkTreePath            *c_path,
                                                                           GtkTreeIter            *c_iter,
                                                                           gpointer                data);
static void         gtk_tree_model_filter_row_inserted                    (GtkTreeModel           *c_model,
                                                                           GtkTreePath            *c_path,
                                                                           GtkTreeIter            *c_iter,
                                                                           gpointer                data);
static void         gtk_tree_model_filter_row_has_child_toggled           (GtkTreeModel           *c_model,
                                                                           GtkTreePath            *c_path,
                                                                           GtkTreeIter            *c_iter,
                                                                           gpointer                data);
static void         gtk_tree_model_filter_row_deleted                     (GtkTreeModel           *c_model,
                                                                           GtkTreePath            *c_path,
                                                                           gpointer                data);
static void         gtk_tree_model_filter_rows_reordered                  (GtkTreeModel           *c_model,
                                                                           GtkTreePath            *c_path,
                                                                           GtkTreeIter            *c_iter,
                                                                           gint                   *new_order,
                                                                           gpointer                data);

/* GtkTreeModel interface */
static GtkTreeModelFlags gtk_tree_model_filter_get_flags                       (GtkTreeModel           *model);
static gint         gtk_tree_model_filter_get_n_columns                   (GtkTreeModel           *model);
static GType        gtk_tree_model_filter_get_column_type                 (GtkTreeModel           *model,
                                                                           gint                    index);
static gboolean     gtk_tree_model_filter_get_iter_full                   (GtkTreeModel           *model,
                                                                           GtkTreeIter            *iter,
                                                                           GtkTreePath            *path);
static gboolean     gtk_tree_model_filter_get_iter                        (GtkTreeModel           *model,
                                                                           GtkTreeIter            *iter,
                                                                           GtkTreePath            *path);
static GtkTreePath *gtk_tree_model_filter_get_path                        (GtkTreeModel           *model,
                                                                           GtkTreeIter            *iter);
static void         gtk_tree_model_filter_get_value                       (GtkTreeModel           *model,
                                                                           GtkTreeIter            *iter,
                                                                           gint                    column,
                                                                           GValue                 *value);
static gboolean     gtk_tree_model_filter_iter_next                       (GtkTreeModel           *model,
                                                                           GtkTreeIter            *iter);
static gboolean     gtk_tree_model_filter_iter_children                   (GtkTreeModel           *model,
                                                                           GtkTreeIter            *iter,
                                                                           GtkTreeIter            *parent);
static gboolean     gtk_tree_model_filter_iter_has_child                  (GtkTreeModel           *model,
                                                                           GtkTreeIter            *iter);
static gint         gtk_tree_model_filter_iter_n_children                 (GtkTreeModel           *model,
                                                                           GtkTreeIter            *iter);
static gboolean     gtk_tree_model_filter_iter_nth_child                  (GtkTreeModel           *model,
                                                                           GtkTreeIter            *iter,
                                                                           GtkTreeIter            *parent,
                                                                           gint                    n);
static gboolean     gtk_tree_model_filter_iter_parent                     (GtkTreeModel           *model,
                                                                           GtkTreeIter            *iter,
                                                                           GtkTreeIter            *child);
static void         gtk_tree_model_filter_ref_node                        (GtkTreeModel           *model,
                                                                           GtkTreeIter            *iter);
static void         gtk_tree_model_filter_unref_node                      (GtkTreeModel           *model,
                                                                           GtkTreeIter            *iter);

/* TreeDragSource interface */
static gboolean    gtk_tree_model_filter_row_draggable                    (GtkTreeDragSource      *drag_source,
                                                                           GtkTreePath            *path);
static gboolean    gtk_tree_model_filter_drag_data_get                    (GtkTreeDragSource      *drag_source,
                                                                           GtkTreePath            *path,
                                                                           GtkSelectionData       *selection_data);
static gboolean    gtk_tree_model_filter_drag_data_delete                 (GtkTreeDragSource      *drag_source,
                                                                           GtkTreePath            *path);

/* private functions */
static void        gtk_tree_model_filter_build_level                      (GtkTreeModelFilter     *filter,
                                                                           FilterLevel            *parent_level,
                                                                           gint                    parent_elt_index,
                                                                           gboolean                emit_inserted);

static void        gtk_tree_model_filter_free_level                       (GtkTreeModelFilter     *filter,
                                                                           FilterLevel            *filter_level);

static GtkTreePath *gtk_tree_model_filter_elt_get_path                    (FilterLevel            *level,
                                                                           FilterElt              *elt,
                                                                           GtkTreePath            *root);

static GtkTreePath *gtk_tree_model_filter_add_root                        (GtkTreePath            *src,
                                                                           GtkTreePath            *root);
static GtkTreePath *gtk_tree_model_filter_remove_root                     (GtkTreePath            *src,
                                                                           GtkTreePath            *root);

static void         gtk_tree_model_filter_increment_stamp                 (GtkTreeModelFilter     *filter);

static gboolean     gtk_tree_model_filter_visible                         (GtkTreeModelFilter     *filter,
                                                                           GtkTreeIter            *child_iter);
static void         gtk_tree_model_filter_clear_cache_helper              (GtkTreeModelFilter     *filter,
                                                                           FilterLevel            *level);

static void         gtk_tree_model_filter_real_unref_node                 (GtkTreeModel           *model,
                                                                           GtkTreeIter            *iter,
                                                                           gboolean                propagate_unref);

static void         gtk_tree_model_filter_set_model                       (GtkTreeModelFilter     *filter,
                                                                           GtkTreeModel           *child_model);
static void         gtk_tree_model_filter_ref_path                        (GtkTreeModelFilter     *filter,
                                                                           GtkTreePath            *path);
static void         gtk_tree_model_filter_unref_path                      (GtkTreeModelFilter     *filter,
                                                                           GtkTreePath            *path);
static void         gtk_tree_model_filter_set_root                        (GtkTreeModelFilter     *filter,
                                                                           GtkTreePath            *root);

static GtkTreePath *gtk_real_tree_model_filter_convert_child_path_to_path (GtkTreeModelFilter     *filter,
                                                                           GtkTreePath            *child_path,
                                                                           gboolean                build_levels,
                                                                           gboolean                fetch_children);

static FilterElt   *gtk_tree_model_filter_get_nth                         (GtkTreeModelFilter     *filter,
                                                                           FilterLevel            *level,
                                                                           int                     n);
static gboolean    gtk_tree_model_filter_elt_is_visible_in_target         (FilterLevel            *level,
                                                                           FilterElt              *elt);
static FilterElt   *gtk_tree_model_filter_get_nth_visible                 (GtkTreeModelFilter     *filter,
                                                                           FilterLevel            *level,
                                                                           int                     n);

static FilterElt   *gtk_tree_model_filter_fetch_child                     (GtkTreeModelFilter     *filter,
                                                                           FilterLevel            *level,
                                                                           gint                    offset,
                                                                           gint                   *index);
static void         gtk_tree_model_filter_remove_node                     (GtkTreeModelFilter     *filter,
                                                                           GtkTreeIter            *iter);
static void         gtk_tree_model_filter_update_children                 (GtkTreeModelFilter     *filter,
                                                                           FilterLevel            *level,
                                                                           FilterElt              *elt);
static FilterElt   *bsearch_elt_with_offset                               (GArray                 *array,
                                                                           gint                   offset,
                                                                           gint                  *index);


G_DEFINE_TYPE_WITH_CODE (GtkTreeModelFilter, gtk_tree_model_filter, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL,
						gtk_tree_model_filter_tree_model_init)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_SOURCE,
						gtk_tree_model_filter_drag_source_init))

static void
gtk_tree_model_filter_init (GtkTreeModelFilter *filter)
{
  filter->priv = GTK_TREE_MODEL_FILTER_GET_PRIVATE (filter);

  filter->priv->visible_column = -1;
  filter->priv->zero_ref_count = 0;
  filter->priv->visible_method_set = FALSE;
  filter->priv->modify_func_set = FALSE;
  filter->priv->in_row_deleted = FALSE;
  filter->priv->virtual_root_deleted = FALSE;
}

static void
gtk_tree_model_filter_class_init (GtkTreeModelFilterClass *filter_class)
{
  GObjectClass *object_class;

  object_class = (GObjectClass *) filter_class;

  object_class->set_property = gtk_tree_model_filter_set_property;
  object_class->get_property = gtk_tree_model_filter_get_property;

  object_class->finalize = gtk_tree_model_filter_finalize;

  /* Properties -- FIXME: disabled translations for now, until I can come up with a
   * better description
   */
  g_object_class_install_property (object_class,
                                   PROP_CHILD_MODEL,
                                   g_param_spec_object ("child-model",
                                                        ("The child model"),
                                                        ("The model for the filtermodel to filter"),
                                                        GTK_TYPE_TREE_MODEL,
                                                        GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class,
                                   PROP_VIRTUAL_ROOT,
                                   g_param_spec_boxed ("virtual-root",
                                                       ("The virtual root"),
                                                       ("The virtual root (relative to the child model) for this filtermodel"),
                                                       GTK_TYPE_TREE_PATH,
                                                       GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_type_class_add_private (object_class, sizeof (GtkTreeModelFilterPrivate));
}

static void
gtk_tree_model_filter_tree_model_init (GtkTreeModelIface *iface)
{
  iface->get_flags = gtk_tree_model_filter_get_flags;
  iface->get_n_columns = gtk_tree_model_filter_get_n_columns;
  iface->get_column_type = gtk_tree_model_filter_get_column_type;
  iface->get_iter = gtk_tree_model_filter_get_iter;
  iface->get_path = gtk_tree_model_filter_get_path;
  iface->get_value = gtk_tree_model_filter_get_value;
  iface->iter_next = gtk_tree_model_filter_iter_next;
  iface->iter_children = gtk_tree_model_filter_iter_children;
  iface->iter_has_child = gtk_tree_model_filter_iter_has_child;
  iface->iter_n_children = gtk_tree_model_filter_iter_n_children;
  iface->iter_nth_child = gtk_tree_model_filter_iter_nth_child;
  iface->iter_parent = gtk_tree_model_filter_iter_parent;
  iface->ref_node = gtk_tree_model_filter_ref_node;
  iface->unref_node = gtk_tree_model_filter_unref_node;
}

static void
gtk_tree_model_filter_drag_source_init (GtkTreeDragSourceIface *iface)
{
  iface->row_draggable = gtk_tree_model_filter_row_draggable;
  iface->drag_data_delete = gtk_tree_model_filter_drag_data_delete;
  iface->drag_data_get = gtk_tree_model_filter_drag_data_get;
}


static void
gtk_tree_model_filter_finalize (GObject *object)
{
  GtkTreeModelFilter *filter = (GtkTreeModelFilter *) object;

  if (filter->priv->virtual_root && !filter->priv->virtual_root_deleted)
    {
      gtk_tree_model_filter_unref_path (filter, filter->priv->virtual_root);
      filter->priv->virtual_root_deleted = TRUE;
    }

  gtk_tree_model_filter_set_model (filter, NULL);

  if (filter->priv->virtual_root)
    gtk_tree_path_free (filter->priv->virtual_root);

  if (filter->priv->root)
    gtk_tree_model_filter_free_level (filter, filter->priv->root);

  g_free (filter->priv->modify_types);
  
  if (filter->priv->modify_destroy)
    filter->priv->modify_destroy (filter->priv->modify_data);

  if (filter->priv->visible_destroy)
    filter->priv->visible_destroy (filter->priv->visible_data);

  /* must chain up */
  G_OBJECT_CLASS (gtk_tree_model_filter_parent_class)->finalize (object);
}

static void
gtk_tree_model_filter_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GtkTreeModelFilter *filter = GTK_TREE_MODEL_FILTER (object);

  switch (prop_id)
    {
      case PROP_CHILD_MODEL:
        gtk_tree_model_filter_set_model (filter, g_value_get_object (value));
        break;
      case PROP_VIRTUAL_ROOT:
        gtk_tree_model_filter_set_root (filter, g_value_get_boxed (value));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
gtk_tree_model_filter_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GtkTreeModelFilter *filter = GTK_TREE_MODEL_FILTER (object);

  switch (prop_id)
    {
      case PROP_CHILD_MODEL:
        g_value_set_object (value, filter->priv->child_model);
        break;
      case PROP_VIRTUAL_ROOT:
        g_value_set_boxed (value, filter->priv->virtual_root);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

/* helpers */

static void
gtk_tree_model_filter_build_level (GtkTreeModelFilter *filter,
                                   FilterLevel        *parent_level,
                                   gint                parent_elt_index,
                                   gboolean            emit_inserted)
{
  GtkTreeIter iter;
  GtkTreeIter first_node;
  GtkTreeIter root;
  FilterElt *parent_elt = NULL;
  FilterLevel *new_level;
  gint length = 0;
  gint i;

  g_assert (filter->priv->child_model != NULL);

  if (filter->priv->in_row_deleted)
    return;

  if (!parent_level)
    {
      if (filter->priv->virtual_root)
        {
          if (gtk_tree_model_get_iter (filter->priv->child_model, &root, filter->priv->virtual_root) == FALSE)
            return;
          length = gtk_tree_model_iter_n_children (filter->priv->child_model, &root);

          if (gtk_tree_model_iter_children (filter->priv->child_model, &iter, &root) == FALSE)
            return;
        }
      else
        {
          if (!gtk_tree_model_get_iter_first (filter->priv->child_model, &iter))
            return;
          length = gtk_tree_model_iter_n_children (filter->priv->child_model, NULL);
        }
    }
  else
    {
      GtkTreeIter parent_iter;
      GtkTreeIter child_parent_iter;

      parent_elt = &g_array_index (parent_level->array, FilterElt, parent_elt_index);

      parent_iter.stamp = filter->priv->stamp;
      parent_iter.user_data = parent_level;
      parent_iter.user_data2 = parent_elt;

      gtk_tree_model_filter_convert_iter_to_child_iter (filter,
                                                        &child_parent_iter,
                                                        &parent_iter);
      if (gtk_tree_model_iter_children (filter->priv->child_model, &iter, &child_parent_iter) == FALSE)
        return;

      /* stamp may have changed */
      gtk_tree_model_filter_convert_iter_to_child_iter (filter,
                                                        &child_parent_iter,
                                                        &parent_iter);
      length = gtk_tree_model_iter_n_children (filter->priv->child_model, &child_parent_iter);
    }

  g_return_if_fail (length > 0);

  new_level = g_new (FilterLevel, 1);
  new_level->array = g_array_sized_new (FALSE, FALSE,
                                        sizeof (FilterElt),
                                        length);
  new_level->ref_count = 0;
  new_level->visible_nodes = 0;
  new_level->parent_elt_index = parent_elt_index;
  new_level->parent_level = parent_level;

  if (parent_elt_index >= 0)
    parent_elt->children = new_level;
  else
    filter->priv->root = new_level;

  /* increase the count of zero ref_counts */
  while (parent_level)
    {
      g_array_index (parent_level->array, FilterElt, parent_elt_index).zero_ref_count++;

      parent_elt_index = parent_level->parent_elt_index;
      parent_level = parent_level->parent_level;
    }
  if (new_level != filter->priv->root)
    filter->priv->zero_ref_count++;

  i = 0;

  first_node = iter;

  do
    {
      if (gtk_tree_model_filter_visible (filter, &iter))
        {
          GtkTreeIter f_iter;
          FilterElt filter_elt;

          filter_elt.offset = i;
          filter_elt.zero_ref_count = 0;
          filter_elt.ref_count = 0;
          filter_elt.children = NULL;
          filter_elt.visible = TRUE;

          if (GTK_TREE_MODEL_FILTER_CACHE_CHILD_ITERS (filter))
            filter_elt.iter = iter;

          g_array_append_val (new_level->array, filter_elt);
          new_level->visible_nodes++;

          f_iter.stamp = filter->priv->stamp;
          f_iter.user_data = new_level;
          f_iter.user_data2 = &(g_array_index (new_level->array, FilterElt, new_level->array->len - 1));

          if (new_level->parent_level || filter->priv->virtual_root)
            gtk_tree_model_filter_ref_node (GTK_TREE_MODEL (filter), &f_iter);

          if (emit_inserted)
            {
              GtkTreePath *f_path;
              GtkTreeIter children;

              f_path = gtk_tree_model_get_path (GTK_TREE_MODEL (filter),
                                                &f_iter);
              gtk_tree_model_row_inserted (GTK_TREE_MODEL (filter),
                                           f_path, &f_iter);
              gtk_tree_path_free (f_path);

              if (gtk_tree_model_iter_children (filter->priv->child_model,
                                                &children, &iter))
                gtk_tree_model_filter_update_children (filter,
                                                       new_level,
                                                       FILTER_ELT (f_iter.user_data2));
            }
        }
      i++;
    }
  while (gtk_tree_model_iter_next (filter->priv->child_model, &iter));

  if (new_level->array->len == 0
      && (new_level != filter->priv->root || filter->priv->virtual_root))
    {
      /* If none of the nodes are visible, we will just pull in the
       * first node of the level and keep a reference on it.  We need this
       * to make sure that we get all signals for this level.
       */
      FilterElt filter_elt;
      GtkTreeIter f_iter;

      filter_elt.offset = 0;
      filter_elt.zero_ref_count = 0;
      filter_elt.ref_count = 0;
      filter_elt.children = NULL;
      filter_elt.visible = FALSE;

      if (GTK_TREE_MODEL_FILTER_CACHE_CHILD_ITERS (filter))
        filter_elt.iter = first_node;

      g_array_append_val (new_level->array, filter_elt);

      f_iter.stamp = filter->priv->stamp;
      f_iter.user_data = new_level;
      f_iter.user_data2 = &(g_array_index (new_level->array, FilterElt, new_level->array->len - 1));

      gtk_tree_model_filter_ref_node (GTK_TREE_MODEL (filter), &f_iter);
    }
  else if (new_level->array->len == 0)
    gtk_tree_model_filter_free_level (filter, new_level);
}

static void
gtk_tree_model_filter_free_level (GtkTreeModelFilter *filter,
                                  FilterLevel        *filter_level)
{
  gint i;

  g_assert (filter_level);

  for (i = 0; i < filter_level->array->len; i++)
    {
      if (g_array_index (filter_level->array, FilterElt, i).children)
        gtk_tree_model_filter_free_level (filter,
                                          FILTER_LEVEL (g_array_index (filter_level->array, FilterElt, i).children));

      if (filter_level->parent_level || filter->priv->virtual_root)
        {
          GtkTreeIter f_iter;

          f_iter.stamp = filter->priv->stamp;
          f_iter.user_data = filter_level;
          f_iter.user_data2 = &(g_array_index (filter_level->array, FilterElt, i));

          gtk_tree_model_filter_unref_node (GTK_TREE_MODEL (filter), &f_iter);
        }
    }

  if (filter_level->ref_count == 0)
    {
      FilterLevel *parent_level = filter_level->parent_level;
      gint parent_elt_index = filter_level->parent_elt_index;

      while (parent_level)
        {
	  g_array_index (parent_level->array, FilterElt, parent_elt_index).zero_ref_count--;

	  parent_elt_index = parent_level->parent_elt_index;
	  parent_level = parent_level->parent_level;
        }

      if (filter_level != filter->priv->root)
        filter->priv->zero_ref_count--;
    }

  if (filter_level->parent_elt_index >= 0)
    FILTER_LEVEL_PARENT_ELT (filter_level)->children = NULL;
  else
    filter->priv->root = NULL;

  g_array_free (filter_level->array, TRUE);
  filter_level->array = NULL;

  g_free (filter_level);
  filter_level = NULL;
}

/* Creates paths suitable for accessing the child model. */
static GtkTreePath *
gtk_tree_model_filter_elt_get_path (FilterLevel *level,
                                    FilterElt   *elt,
                                    GtkTreePath *root)
{
  FilterLevel *walker = level;
  FilterElt *walker2 = elt;
  GtkTreePath *path;
  GtkTreePath *real_path;

  g_return_val_if_fail (level != NULL, NULL);
  g_return_val_if_fail (elt != NULL, NULL);

  path = gtk_tree_path_new ();

  while (walker)
    {
      gtk_tree_path_prepend_index (path, walker2->offset);

      if (!walker->parent_level)
        break;

      walker2 = FILTER_LEVEL_PARENT_ELT (walker);
      walker = walker->parent_level;
    }

  if (root)
    {
      real_path = gtk_tree_model_filter_add_root (path, root);
      gtk_tree_path_free (path);
      return real_path;
    }

  return path;
}

static GtkTreePath *
gtk_tree_model_filter_add_root (GtkTreePath *src,
                                GtkTreePath *root)
{
  GtkTreePath *retval;
  gint i;

  retval = gtk_tree_path_copy (root);

  for (i = 0; i < gtk_tree_path_get_depth (src); i++)
    gtk_tree_path_append_index (retval, gtk_tree_path_get_indices (src)[i]);

  return retval;
}

static GtkTreePath *
gtk_tree_model_filter_remove_root (GtkTreePath *src,
                                   GtkTreePath *root)
{
  GtkTreePath *retval;
  gint i;
  gint depth;
  gint *indices;

  if (gtk_tree_path_get_depth (src) <= gtk_tree_path_get_depth (root))
    return NULL;

  depth = gtk_tree_path_get_depth (src);
  indices = gtk_tree_path_get_indices (src);

  for (i = 0; i < gtk_tree_path_get_depth (root); i++)
    if (indices[i] != gtk_tree_path_get_indices (root)[i])
      return NULL;

  retval = gtk_tree_path_new ();

  for (; i < depth; i++)
    gtk_tree_path_append_index (retval, indices[i]);

  return retval;
}

static void
gtk_tree_model_filter_increment_stamp (GtkTreeModelFilter *filter)
{
  do
    {
      filter->priv->stamp++;
    }
  while (filter->priv->stamp == 0);

  gtk_tree_model_filter_clear_cache (filter);
}

static gboolean
gtk_tree_model_filter_visible (GtkTreeModelFilter *filter,
                               GtkTreeIter        *child_iter)
{
  if (filter->priv->visible_func)
    {
      return filter->priv->visible_func (filter->priv->child_model,
					 child_iter,
					 filter->priv->visible_data)
	? TRUE : FALSE;
    }
  else if (filter->priv->visible_column >= 0)
   {
     GValue val = {0, };

     gtk_tree_model_get_value (filter->priv->child_model, child_iter,
                               filter->priv->visible_column, &val);

     if (g_value_get_boolean (&val))
       {
         g_value_unset (&val);
         return TRUE;
       }

     g_value_unset (&val);
     return FALSE;
   }

  /* no visible function set, so always visible */
  return TRUE;
}

static void
gtk_tree_model_filter_clear_cache_helper (GtkTreeModelFilter *filter,
                                          FilterLevel        *level)
{
  gint i;

  g_assert (level);

  for (i = 0; i < level->array->len; i++)
    {
      if (g_array_index (level->array, FilterElt, i).zero_ref_count > 0)
        gtk_tree_model_filter_clear_cache_helper (filter, g_array_index (level->array, FilterElt, i).children);
     }

  if (level->ref_count == 0 && level != filter->priv->root)
    {
      gtk_tree_model_filter_free_level (filter, level);
      return;
    }
}

static FilterElt *
gtk_tree_model_filter_get_nth (GtkTreeModelFilter *filter,
                               FilterLevel        *level,
                               int                 n)
{
  if (level->array->len <= n)
    return NULL;

  return &g_array_index (level->array, FilterElt, n);
}

static gboolean
gtk_tree_model_filter_elt_is_visible_in_target (FilterLevel *level,
                                                FilterElt   *elt)
{
  gint elt_index;

  if (!elt->visible)
    return FALSE;

  if (level->parent_elt_index == -1)
    return TRUE;

  do
    {
      elt_index = level->parent_elt_index;
      level = level->parent_level;

      if (elt_index >= 0
          && !g_array_index (level->array, FilterElt, elt_index).visible)
        return FALSE;
    }
  while (level);

  return TRUE;
}

static FilterElt *
gtk_tree_model_filter_get_nth_visible (GtkTreeModelFilter *filter,
                                       FilterLevel        *level,
                                       int                 n)
{
  int i = 0;
  FilterElt *elt;

  if (level->visible_nodes <= n)
    return NULL;

  elt = FILTER_ELT (level->array->data);
  while (!elt->visible)
    elt++;

  while (i < n)
    {
      if (elt->visible)
        i++;
      elt++;
    }

  while (!elt->visible)
    elt++;

  return elt;
}

static FilterElt *
gtk_tree_model_filter_fetch_child (GtkTreeModelFilter *filter,
                                   FilterLevel        *level,
                                   gint                offset,
                                   gint               *index)
{
  gint i = 0;
  gint start, middle, end;
  gint len;
  GtkTreePath *c_path = NULL;
  GtkTreeIter c_iter;
  GtkTreePath *c_parent_path = NULL;
  GtkTreeIter c_parent_iter;
  FilterElt elt;

  /* check if child exists and is visible */
  if (level->parent_elt_index >= 0)
    {
      c_parent_path =
        gtk_tree_model_filter_elt_get_path (level->parent_level,
                                            FILTER_LEVEL_PARENT_ELT (level),
                                            filter->priv->virtual_root);
      if (!c_parent_path)
        return NULL;
    }
  else
    {
      if (filter->priv->virtual_root)
        c_parent_path = gtk_tree_path_copy (filter->priv->virtual_root);
      else
        c_parent_path = NULL;
    }

  if (c_parent_path)
    {
      gtk_tree_model_get_iter (filter->priv->child_model,
                               &c_parent_iter,
                               c_parent_path);
      len = gtk_tree_model_iter_n_children (filter->priv->child_model,
                                            &c_parent_iter);

      c_path = gtk_tree_path_copy (c_parent_path);
      gtk_tree_path_free (c_parent_path);
    }
  else
    {
      len = gtk_tree_model_iter_n_children (filter->priv->child_model, NULL);
      c_path = gtk_tree_path_new ();
    }

  gtk_tree_path_append_index (c_path, offset);
  gtk_tree_model_get_iter (filter->priv->child_model, &c_iter, c_path);
  gtk_tree_path_free (c_path);

  if (offset >= len || !gtk_tree_model_filter_visible (filter, &c_iter))
    return NULL;

  /* add child */
  elt.offset = offset;
  elt.zero_ref_count = 0;
  elt.ref_count = 0;
  elt.children = NULL;
  /* visibility should be FALSE as we don't emit row_inserted */
  elt.visible = FALSE;

  if (GTK_TREE_MODEL_FILTER_CACHE_CHILD_ITERS (filter))
    elt.iter = c_iter;

  /* find index (binary search on offset) */
  start = 0;
  end = level->array->len;

  if (start != end)
    {
      while (start != end)
        {
          middle = (start + end) / 2;

          if (g_array_index (level->array, FilterElt, middle).offset <= offset)
            start = middle + 1;
          else
            end = middle;
        }

      if (g_array_index (level->array, FilterElt, middle).offset <= offset)
        i = middle + 1;
      else
        i = middle;
    }
  else
    i = 0;

  g_array_insert_val (level->array, i, elt);
  *index = i;

  for (i = 0; i < level->array->len; i++)
    {
      FilterElt *e = &(g_array_index (level->array, FilterElt, i));
      if (e->children)
        e->children->parent_elt_index = i;
    }

  c_iter.stamp = filter->priv->stamp;
  c_iter.user_data = level;
  c_iter.user_data2 = &g_array_index (level->array, FilterElt, *index);

  if (level->parent_level || filter->priv->virtual_root)
    gtk_tree_model_filter_ref_node (GTK_TREE_MODEL (filter), &c_iter);

  return &g_array_index (level->array, FilterElt, *index);
}

static void
gtk_tree_model_filter_remove_node (GtkTreeModelFilter *filter,
                                   GtkTreeIter        *iter)
{
  FilterElt *elt, *parent;
  FilterLevel *level, *parent_level;
  gint i, length, parent_elt_index;

  gboolean emit_child_toggled = FALSE;

  level = FILTER_LEVEL (iter->user_data);
  elt = FILTER_ELT (iter->user_data2);

  parent_elt_index = level->parent_elt_index;
  if (parent_elt_index >= 0)
    parent = FILTER_LEVEL_PARENT_ELT (level);
  else
    parent = NULL;
  parent_level = level->parent_level;

  length = level->array->len;

  /* we distinguish a couple of cases:
   *  - root level, length > 1: emit row-deleted and remove.
   *  - root level, length == 1: emit row-deleted and keep in cache.
   *  - level, length == 1: parent->ref_count > 1: emit row-deleted and keep.
   *  - level, length > 1: emit row-deleted and remove.
   *  - else, remove level.
   *
   *  if level != root level and visible nodes == 0, emit row-has-child-toggled.
   */

  if (level != filter->priv->root
      && level->visible_nodes == 0
      && parent
      && parent->visible)
    emit_child_toggled = TRUE;

  if (length > 1)
    {
      GtkTreePath *path;
      FilterElt *tmp;

      /* We emit row-deleted, and remove the node from the cache.
       * If it has any children, these will be removed here as well.
       */

      if (elt->children)
        gtk_tree_model_filter_free_level (filter, elt->children);

      path = gtk_tree_model_get_path (GTK_TREE_MODEL (filter), iter);
      elt->visible = FALSE;
      gtk_tree_model_filter_increment_stamp (filter);
      iter->stamp = filter->priv->stamp;
      gtk_tree_model_row_deleted (GTK_TREE_MODEL (filter), path);
      gtk_tree_path_free (path);

      while (elt->ref_count > 1)
        gtk_tree_model_filter_real_unref_node (GTK_TREE_MODEL (filter),
                                               iter, FALSE);

      if (parent_level || filter->priv->virtual_root)
        gtk_tree_model_filter_unref_node (GTK_TREE_MODEL (filter), iter);
      else if (elt->ref_count > 0)
        gtk_tree_model_filter_real_unref_node (GTK_TREE_MODEL (filter),
                                               iter, FALSE);

      /* remove the node */
      tmp = bsearch_elt_with_offset (level->array, elt->offset, &i);

      if (tmp)
        {
          g_array_remove_index (level->array, i);

	  i--;
          for (i = MAX (i, 0); i < level->array->len; i++)
            {
              /* NOTE: here we do *not* decrease offsets, because the node was
               * not removed from the child model
               */
              elt = &g_array_index (level->array, FilterElt, i);
              if (elt->children)
                elt->children->parent_elt_index = i;
            }
        }
    }
  else if ((length == 1 && parent && parent->ref_count > 1)
           || (length == 1 && level == filter->priv->root))
    {
      GtkTreePath *path;

      /* We emit row-deleted, but keep the node in the cache and
       * referenced.  Its children will be removed.
       */

      if (elt->children)
        {
          gtk_tree_model_filter_free_level (filter, elt->children);
          elt->children = NULL;
        }

      path = gtk_tree_model_get_path (GTK_TREE_MODEL (filter), iter);
      elt->visible = FALSE;
      gtk_tree_model_filter_increment_stamp (filter);
      gtk_tree_model_row_deleted (GTK_TREE_MODEL (filter), path);
      gtk_tree_path_free (path);
    }
  else
    {
      GtkTreePath *path;

      /* Blow level away, including any child levels */

      path = gtk_tree_model_get_path (GTK_TREE_MODEL (filter), iter);
      elt->visible = FALSE;
      gtk_tree_model_filter_increment_stamp (filter);
      iter->stamp = filter->priv->stamp;
      gtk_tree_model_row_deleted (GTK_TREE_MODEL (filter), path);
      gtk_tree_path_free (path);

      while (elt->ref_count > 1)
        gtk_tree_model_filter_real_unref_node (GTK_TREE_MODEL (filter),
                                               iter, FALSE);

      gtk_tree_model_filter_free_level (filter, level);
    }

  if (emit_child_toggled)
    {
      GtkTreeIter piter;
      GtkTreePath *ppath;

      piter.stamp = filter->priv->stamp;
      piter.user_data = parent_level;
      piter.user_data2 = parent;

      ppath = gtk_tree_model_get_path (GTK_TREE_MODEL (filter), &piter);

      gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (filter),
                                            ppath, &piter);
      gtk_tree_path_free (ppath);
    }
}

static void
gtk_tree_model_filter_update_children (GtkTreeModelFilter *filter,
				       FilterLevel        *level,
				       FilterElt          *elt)
{
  GtkTreeIter c_iter;
  GtkTreeIter iter;

  if (!elt->visible)
    return;

  iter.stamp = filter->priv->stamp;
  iter.user_data = level;
  iter.user_data2 = elt;

  gtk_tree_model_filter_convert_iter_to_child_iter (filter, &c_iter, &iter);

  if (gtk_tree_model_iter_has_child (filter->priv->child_model, &c_iter))
    {
      GtkTreePath *path = gtk_tree_model_get_path (GTK_TREE_MODEL (filter),
                                                   &iter);
      gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (filter),
                                            path,
                                            &iter);
      if (path)
        gtk_tree_path_free (path);
    }
}

static FilterElt *
bsearch_elt_with_offset (GArray *array,
                         gint    offset,
                         gint   *index)
{
  gint start, middle, end;
  FilterElt *elt;

  start = 0;
  end = array->len;

  if (array->len < 1)
    return NULL;

  if (start == end)
    {
      elt = &g_array_index (array, FilterElt, 0);

      if (elt->offset == offset)
        {
          *index = 0;
          return elt;
        }
      else
        return NULL;
    }

  do
    {
      middle = (start + end) / 2;

      elt = &g_array_index (array, FilterElt, middle);

      if (elt->offset < offset)
        start = middle + 1;
      else if (elt->offset > offset)
        end = middle;
      else
        break;
    }
  while (start != end);

  if (elt->offset == offset)
    {
      *index = middle;
      return elt;
    }

  return NULL;
}

/* TreeModel signals */
static void
gtk_tree_model_filter_row_changed (GtkTreeModel *c_model,
                                   GtkTreePath  *c_path,
                                   GtkTreeIter  *c_iter,
                                   gpointer      data)
{
  GtkTreeModelFilter *filter = GTK_TREE_MODEL_FILTER (data);
  GtkTreeIter iter;
  GtkTreeIter children;
  GtkTreeIter real_c_iter;
  GtkTreePath *path = NULL;

  FilterElt *elt;
  FilterLevel *level;

  gboolean requested_state;
  gboolean current_state;
  gboolean free_c_path = FALSE;
  gboolean signals_emitted = FALSE;

  g_return_if_fail (c_path != NULL || c_iter != NULL);

  if (!c_path)
    {
      c_path = gtk_tree_model_get_path (c_model, c_iter);
      free_c_path = TRUE;
    }

  if (c_iter)
    real_c_iter = *c_iter;
  else
    gtk_tree_model_get_iter (c_model, &real_c_iter, c_path);

  /* is this node above the virtual root? */
  if (filter->priv->virtual_root
      && (gtk_tree_path_get_depth (filter->priv->virtual_root)
          >= gtk_tree_path_get_depth (c_path)))
    goto done;

  /* what's the requested state? */
  requested_state = gtk_tree_model_filter_visible (filter, &real_c_iter);

  /* now, let's see whether the item is there */
  path = gtk_real_tree_model_filter_convert_child_path_to_path (filter,
                                                                c_path,
                                                                FALSE,
                                                                FALSE);

  if (path)
    {
      gtk_tree_model_filter_get_iter_full (GTK_TREE_MODEL (filter),
                                           &iter, path);
      current_state = FILTER_ELT (iter.user_data2)->visible;
    }
  else
    current_state = FALSE;

  if (current_state == FALSE && requested_state == FALSE)
    /* no changes required */
    goto done;

  if (current_state == TRUE && requested_state == FALSE)
    {
      /* get rid of this node */
      level = FILTER_LEVEL (iter.user_data);
      level->visible_nodes--;

      gtk_tree_model_filter_remove_node (filter, &iter);

      goto done;
    }

  if (current_state == TRUE && requested_state == TRUE)
    {
      /* propagate the signal; also get a path taking only visible
       * nodes into account.
       */
      gtk_tree_path_free (path);
      path = gtk_tree_model_get_path (GTK_TREE_MODEL (filter), &iter);

      level = FILTER_LEVEL (iter.user_data);
      elt = FILTER_ELT (iter.user_data2);

      if (gtk_tree_model_filter_elt_is_visible_in_target (level, elt))
        {
          gtk_tree_model_row_changed (GTK_TREE_MODEL (filter), path, &iter);

          /* and update the children */
          if (gtk_tree_model_iter_children (c_model, &children, &real_c_iter))
            gtk_tree_model_filter_update_children (filter, level, elt);
        }

      goto done;
    }

  /* only current == FALSE and requested == TRUE is left,
   * pull in the child
   */
  g_return_if_fail (current_state == FALSE && requested_state == TRUE);

  /* make sure the new item has been pulled in */
  if (!filter->priv->root)
    {
      FilterLevel *root;

      gtk_tree_model_filter_build_level (filter, NULL, -1, TRUE);

      /* We will only proceed below if the item is found.  If the item
       * is found, we can be sure row-inserted has just been emitted
       * for it.
       */
      signals_emitted = TRUE;

      root = FILTER_LEVEL (filter->priv->root);
    }

  gtk_tree_model_filter_increment_stamp (filter);

  /* We need to allow to build new levels, because we are then pulling
   * in a child in an invisible level.  We only want to find path if it
   * is in a visible level (and thus has a parent that is visible).
   */
  if (!path)
    path = gtk_real_tree_model_filter_convert_child_path_to_path (filter,
                                                                  c_path,
                                                                  FALSE,
                                                                  TRUE);

  if (!path)
    /* parent is probably being filtered out */
    goto done;

  gtk_tree_model_filter_get_iter_full (GTK_TREE_MODEL (filter), &iter, path);

  level = FILTER_LEVEL (iter.user_data);
  elt = FILTER_ELT (iter.user_data2);

  /* elt->visible can be TRUE at this point if it was pulled in above */
  if (!elt->visible)
    {
      elt->visible = TRUE;
      level->visible_nodes++;
    }

  if (gtk_tree_model_filter_elt_is_visible_in_target (level, elt))
    {
      /* visibility changed -- reget path */
      gtk_tree_path_free (path);
      path = gtk_tree_model_get_path (GTK_TREE_MODEL (filter), &iter);

      if (!signals_emitted)
        gtk_tree_model_row_inserted (GTK_TREE_MODEL (filter), path, &iter);

      if (level->parent_level && level->visible_nodes == 1)
        {
          /* We know that this is the first visible node in this level, so
           * we need to emit row-has-child-toggled on the parent.  This
           * does not apply to the root level.
           */

          gtk_tree_path_up (path);
          gtk_tree_model_get_iter (GTK_TREE_MODEL (filter), &iter, path);

          gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (filter),
                                                path,
                                                &iter);
        }

      if (!signals_emitted
          && gtk_tree_model_iter_children (c_model, &children, c_iter))
        gtk_tree_model_filter_update_children (filter, level, elt);
    }

done:
  if (path)
    gtk_tree_path_free (path);

  if (free_c_path)
    gtk_tree_path_free (c_path);
}

static void
gtk_tree_model_filter_row_inserted (GtkTreeModel *c_model,
                                    GtkTreePath  *c_path,
                                    GtkTreeIter  *c_iter,
                                    gpointer      data)
{
  GtkTreeModelFilter *filter = GTK_TREE_MODEL_FILTER (data);
  GtkTreePath *path = NULL;
  GtkTreePath *real_path = NULL;
  GtkTreeIter iter;

  GtkTreeIter real_c_iter;

  FilterElt *elt;
  FilterLevel *level;
  FilterLevel *parent_level;

  gint i = 0, offset;

  gboolean free_c_path = FALSE;

  g_return_if_fail (c_path != NULL || c_iter != NULL);

  if (!c_path)
    {
      c_path = gtk_tree_model_get_path (c_model, c_iter);
      free_c_path = TRUE;
    }

  if (c_iter)
    real_c_iter = *c_iter;
  else
    gtk_tree_model_get_iter (c_model, &real_c_iter, c_path);

  /* the row has already been inserted. so we need to fixup the
   * virtual root here first
   */
  if (filter->priv->virtual_root)
    {
      if (gtk_tree_path_get_depth (filter->priv->virtual_root) >=
          gtk_tree_path_get_depth (c_path))
        {
          gint level;
          gint *v_indices, *c_indices;
          gboolean common_prefix = TRUE;

          level = gtk_tree_path_get_depth (c_path) - 1;
          v_indices = gtk_tree_path_get_indices (filter->priv->virtual_root);
          c_indices = gtk_tree_path_get_indices (c_path);

          for (i = 0; i < level; i++)
            if (v_indices[i] != c_indices[i])
              {
                common_prefix = FALSE;
                break;
              }

          if (common_prefix && v_indices[level] >= c_indices[level])
            (v_indices[level])++;
        }
    }

  if (!filter->priv->root)
    {
      /* No point in building the level if this node is not visible. */
      if (!filter->priv->virtual_root
          && !gtk_tree_model_filter_visible (filter, c_iter))
        goto done;

      /* build level will pull in the new child */
      gtk_tree_model_filter_build_level (filter, NULL, -1, FALSE);

      if (filter->priv->root
          && FILTER_LEVEL (filter->priv->root)->visible_nodes)
        goto done_and_emit;
      else
        goto done;
    }

  parent_level = level = FILTER_LEVEL (filter->priv->root);

  /* subtract virtual root if necessary */
  if (filter->priv->virtual_root)
    {
      real_path = gtk_tree_model_filter_remove_root (c_path,
                                                     filter->priv->virtual_root);
      /* not our child */
      if (!real_path)
        goto done;
    }
  else
    real_path = gtk_tree_path_copy (c_path);

  if (gtk_tree_path_get_depth (real_path) - 1 >= 1)
    {
      /* find the parent level */
      while (i < gtk_tree_path_get_depth (real_path) - 1)
        {
          gint j;

          if (!level)
            /* we don't cover this signal */
            goto done;

          elt = bsearch_elt_with_offset (level->array,
                                         gtk_tree_path_get_indices (real_path)[i],
                                         &j);

          if (!elt)
            /* parent is probably being filtered out */
            goto done;

          if (!elt->children)
            {
              GtkTreePath *tmppath;
              GtkTreeIter  tmpiter;

              tmpiter.stamp = filter->priv->stamp;
              tmpiter.user_data = level;
              tmpiter.user_data2 = elt;

              tmppath = gtk_tree_model_get_path (GTK_TREE_MODEL (data),
                                                 &tmpiter);

              if (tmppath)
                {
                  gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (data),
                                                        tmppath, &tmpiter);
                  gtk_tree_path_free (tmppath);
                }

              /* not covering this signal */
              goto done;
            }

          level = elt->children;
          parent_level = level;
          i++;
        }
    }

  if (!parent_level)
    goto done;

  /* let's try to insert the value */
  offset = gtk_tree_path_get_indices (real_path)[gtk_tree_path_get_depth (real_path) - 1];

  /* update the offsets, yes if we didn't insert the node above, there will
   * be a gap here. This will be filled with the node (via fetch_child) when
   * it becomes visible
   */
  for (i = 0; i < level->array->len; i++)
    {
      FilterElt *e = &g_array_index (level->array, FilterElt, i);
      if ((e->offset >= offset))
        e->offset++;
    }

  /* only insert when visible */
  if (gtk_tree_model_filter_visible (filter, &real_c_iter))
    {
      FilterElt felt;

      if (GTK_TREE_MODEL_FILTER_CACHE_CHILD_ITERS (filter))
        felt.iter = real_c_iter;

      felt.offset = offset;
      felt.zero_ref_count = 0;
      felt.ref_count = 0;
      felt.visible = TRUE;
      felt.children = NULL;

      for (i = 0; i < level->array->len; i++)
        if (g_array_index (level->array, FilterElt, i).offset > offset)
          break;

      level->visible_nodes++;

      g_array_insert_val (level->array, i, felt);

      if (level->parent_level || filter->priv->virtual_root)
        {
          GtkTreeIter f_iter;

          f_iter.stamp = filter->priv->stamp;
          f_iter.user_data = level;
          f_iter.user_data2 = &g_array_index (level->array, FilterElt, i);

          gtk_tree_model_filter_ref_node (GTK_TREE_MODEL (filter), &f_iter);
        }
    }

  /* another iteration to update the references of children to parents. */
  for (i = 0; i < level->array->len; i++)
    {
      FilterElt *e = &g_array_index (level->array, FilterElt, i);
      if (e->children)
        e->children->parent_elt_index = i;
    }

  /* don't emit the signal if we aren't visible */
  if (!gtk_tree_model_filter_visible (filter, &real_c_iter))
    goto done;

done_and_emit:
  /* NOTE: pass c_path here and NOT real_path. This function does
   * root subtraction itself
   */
  path = gtk_real_tree_model_filter_convert_child_path_to_path (filter,
                                                                c_path,
                                                                FALSE, TRUE);

  if (!path)
    goto done;

  gtk_tree_model_filter_increment_stamp (filter);

  gtk_tree_model_filter_get_iter_full (GTK_TREE_MODEL (data), &iter, path);

  /* get a path taking only visible nodes into account */
  gtk_tree_path_free (path);
  path = gtk_tree_model_get_path (GTK_TREE_MODEL (data), &iter);

  gtk_tree_model_row_inserted (GTK_TREE_MODEL (data), path, &iter);

  gtk_tree_path_free (path);

done:
  if (real_path)
    gtk_tree_path_free (real_path);

  if (free_c_path)
    gtk_tree_path_free (c_path);
}

static void
gtk_tree_model_filter_row_has_child_toggled (GtkTreeModel *c_model,
                                             GtkTreePath  *c_path,
                                             GtkTreeIter  *c_iter,
                                             gpointer      data)
{
  GtkTreeModelFilter *filter = GTK_TREE_MODEL_FILTER (data);
  GtkTreePath *path;
  GtkTreeIter iter;
  FilterLevel *level;
  FilterElt *elt;
  gboolean requested_state;

  g_return_if_fail (c_path != NULL && c_iter != NULL);

  /* If we get row-has-child-toggled on the virtual root, and there is
   * no root level; try to build it now.
   */
  if (filter->priv->virtual_root && !filter->priv->root
      && !gtk_tree_path_compare (c_path, filter->priv->virtual_root))
    {
      gtk_tree_model_filter_build_level (filter, NULL, -1, TRUE);
      return;
    }

  /* For all other levels, there is a chance that the visibility state
   * of the parent has changed now.
   */

  path = gtk_real_tree_model_filter_convert_child_path_to_path (filter,
                                                                c_path,
                                                                FALSE,
                                                                TRUE);
  if (!path)
    return;

  gtk_tree_model_filter_get_iter_full (GTK_TREE_MODEL (data), &iter, path);

  level = FILTER_LEVEL (iter.user_data);
  elt = FILTER_ELT (iter.user_data2);

  gtk_tree_path_free (path);

  requested_state = gtk_tree_model_filter_visible (filter, c_iter);

  if (!elt->visible && !requested_state)
    {
      /* The parent node currently is not visible and will not become
       * visible, so we will not pass on the row-has-child-toggled event.
       */
      return;
    }
  else if (elt->visible && !requested_state)
    {
      /* The node is no longer visible, so it has to be removed.
       * _remove_node() takes care of emitting row-has-child-toggled
       * when required.
       */
      level->visible_nodes--;

      gtk_tree_model_filter_remove_node (filter, &iter);

      return;
    }
  else if (!elt->visible && requested_state)
    {
      elt->visible = TRUE;
      level->visible_nodes++;

      /* Only insert if the parent is visible in the target */
      if (gtk_tree_model_filter_elt_is_visible_in_target (level, elt))
        {
          path = gtk_tree_model_get_path (GTK_TREE_MODEL (filter), &iter);
          gtk_tree_model_row_inserted (GTK_TREE_MODEL (filter), path, &iter);
          gtk_tree_path_free (path);

          /* We do not update children now, because that will happen
           * below.
           */
        }
    }
  /* For the remaining possibility, elt->visible && requested_state
   * no action is required.
   */

  /* If this node is referenced and has children, build the level so we
   * can monitor it for changes.
   */
  if (elt->ref_count > 1 && gtk_tree_model_iter_has_child (c_model, c_iter))
    gtk_tree_model_filter_build_level (filter, level,
                                       FILTER_LEVEL_ELT_INDEX (level, elt),
                                       TRUE);

  /* get a path taking only visible nodes into account */
  path = gtk_tree_model_get_path (GTK_TREE_MODEL (data), &iter);
  gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (data), path, &iter);
  gtk_tree_path_free (path);
}

static void
gtk_tree_model_filter_row_deleted (GtkTreeModel *c_model,
                                   GtkTreePath  *c_path,
                                   gpointer      data)
{
  GtkTreeModelFilter *filter = GTK_TREE_MODEL_FILTER (data);
  GtkTreePath *path;
  GtkTreeIter iter;
  FilterElt *elt;
  FilterLevel *level, *parent_level = NULL;
  gboolean emit_child_toggled = FALSE;
  gboolean emit_row_deleted = FALSE;
  gint offset;
  gint i;
  gint parent_elt_index = -1;

  g_return_if_fail (c_path != NULL);

  /* special case the deletion of an ancestor of the virtual root */
  if (filter->priv->virtual_root &&
      (gtk_tree_path_is_ancestor (c_path, filter->priv->virtual_root) ||
       !gtk_tree_path_compare (c_path, filter->priv->virtual_root)))
    {
      gint i;
      GtkTreePath *path2;
      FilterLevel *level2 = FILTER_LEVEL (filter->priv->root);

      gtk_tree_model_filter_unref_path (filter, filter->priv->virtual_root);
      filter->priv->virtual_root_deleted = TRUE;

      if (!level2)
        return;

      /* remove everything in the filter model
       *
       * For now, we just iterate over the root level and emit a
       * row_deleted for each FilterElt. Not sure if this is correct.
       */

      gtk_tree_model_filter_increment_stamp (filter);
      path2 = gtk_tree_path_new ();
      gtk_tree_path_append_index (path2, 0);

      for (i = 0; i < level2->visible_nodes; i++)
        gtk_tree_model_row_deleted (GTK_TREE_MODEL (data), path2);

      gtk_tree_path_free (path2);
      gtk_tree_model_filter_free_level (filter, filter->priv->root);

      return;
    }

  /* fixup virtual root */
  if (filter->priv->virtual_root)
    {
      if (gtk_tree_path_get_depth (filter->priv->virtual_root) >=
          gtk_tree_path_get_depth (c_path))
        {
          gint level2;
          gint *v_indices, *c_indices;
          gboolean common_prefix = TRUE;

          level2 = gtk_tree_path_get_depth (c_path) - 1;
          v_indices = gtk_tree_path_get_indices (filter->priv->virtual_root);
          c_indices = gtk_tree_path_get_indices (c_path);

          for (i = 0; i < level2; i++)
            if (v_indices[i] != c_indices[i])
              {
                common_prefix = FALSE;
                break;
              }

          if (common_prefix && v_indices[level2] > c_indices[level2])
            (v_indices[level2])--;
        }
    }

  path = gtk_real_tree_model_filter_convert_child_path_to_path (filter,
                                                                c_path,
                                                                FALSE,
                                                                FALSE);

  if (!path)
    {
      /* The node deleted in the child model is not visible in the
       * filter model.  We will not emit a signal, just fixup the offsets
       * of the other nodes.
       */
      GtkTreePath *real_path;

      if (!filter->priv->root)
        return;

      level = FILTER_LEVEL (filter->priv->root);

      /* subtract vroot if necessary */
      if (filter->priv->virtual_root)
        {
          real_path = gtk_tree_model_filter_remove_root (c_path,
                                                         filter->priv->virtual_root);
          /* we don't handle this */
          if (!real_path)
            return;
        }
      else
        real_path = gtk_tree_path_copy (c_path);

      i = 0;
      if (gtk_tree_path_get_depth (real_path) - 1 >= 1)
        {
          /* find the level where the deletion occurred */
          while (i < gtk_tree_path_get_depth (real_path) - 1)
            {
              gint j;

              if (!level)
                {
                  /* we don't cover this */
                  gtk_tree_path_free (real_path);
                  return;
                }

              elt = bsearch_elt_with_offset (level->array,
                                             gtk_tree_path_get_indices (real_path)[i],
                                             &j);

              if (!elt || !elt->children)
                {
                  /* parent is filtered out, so no level */
                  gtk_tree_path_free (real_path);
                  return;
                }

              level = elt->children;
              i++;
            }
        }

      offset = gtk_tree_path_get_indices (real_path)[gtk_tree_path_get_depth (real_path) - 1];
      gtk_tree_path_free (real_path);

      if (!level)
        return;

      /* decrease offset of all nodes following the deleted node */
      for (i = 0; i < level->array->len; i++)
        {
          elt = &g_array_index (level->array, FilterElt, i);
          if (elt->offset > offset)
            elt->offset--;
          if (elt->children)
            elt->children->parent_elt_index = i;
        }

      return;
    }

  /* a node was deleted, which was in our cache */
  gtk_tree_model_filter_get_iter_full (GTK_TREE_MODEL (data), &iter, path);

  level = FILTER_LEVEL (iter.user_data);
  elt = FILTER_ELT (iter.user_data2);
  offset = elt->offset;

  if (elt->visible)
    {
      /* get a path taking only visible nodes into account */
      gtk_tree_path_free (path);
      path = gtk_tree_model_get_path (GTK_TREE_MODEL (filter), &iter);

      level->visible_nodes--;

      if (level->visible_nodes == 0)
        {
          emit_child_toggled = TRUE;
          parent_level = level->parent_level;
          parent_elt_index = level->parent_elt_index;
        }

      emit_row_deleted = TRUE;
    }

  /* The filter model's reference on the child node is released
   * below.
   */
  while (elt->ref_count > 1)
    gtk_tree_model_filter_real_unref_node (GTK_TREE_MODEL (data), &iter, FALSE);

  if (level->array->len == 1)
    {
      /* kill level */
      gtk_tree_model_filter_free_level (filter, level);
    }
  else
    {
      FilterElt *tmp;

      /* release the filter model's reference on the node */
      if (level->parent_level || filter->priv->virtual_root)
        gtk_tree_model_filter_unref_node (GTK_TREE_MODEL (filter), &iter);
      else if (elt->ref_count > 0)
        gtk_tree_model_filter_real_unref_node (GTK_TREE_MODEL (data), &iter,
                                               FALSE);

      /* remove the row */
      tmp = bsearch_elt_with_offset (level->array, elt->offset, &i);

      offset = tmp->offset;
      g_array_remove_index (level->array, i);

      i--;
      for (i = MAX (i, 0); i < level->array->len; i++)
        {
          elt = &g_array_index (level->array, FilterElt, i);
          if (elt->offset > offset)
            elt->offset--;
          if (elt->children)
            elt->children->parent_elt_index = i;
        }
    }

  if (emit_row_deleted)
    {
      /* emit row_deleted */
      gtk_tree_model_filter_increment_stamp (filter);
      gtk_tree_model_row_deleted (GTK_TREE_MODEL (data), path);
      iter.stamp = filter->priv->stamp;
    }

  if (emit_child_toggled && parent_level)
    {
      GtkTreeIter iter2;
      GtkTreePath *path2;

      iter2.stamp = filter->priv->stamp;
      iter2.user_data = parent_level;
      iter2.user_data2 = &g_array_index (parent_level->array, FilterElt, parent_elt_index);

      /* We set in_row_deleted to TRUE to avoid a level build triggered
       * by row-has-child-toggled (parent model could call iter_has_child
       * for example).
       */
      filter->priv->in_row_deleted = TRUE;
      path2 = gtk_tree_model_get_path (GTK_TREE_MODEL (filter), &iter2);
      gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (filter),
                                            path2, &iter2);
      gtk_tree_path_free (path2);
      filter->priv->in_row_deleted = FALSE;
    }

  gtk_tree_path_free (path);
}

static void
gtk_tree_model_filter_rows_reordered (GtkTreeModel *c_model,
                                      GtkTreePath  *c_path,
                                      GtkTreeIter  *c_iter,
                                      gint         *new_order,
                                      gpointer      data)
{
  FilterElt *elt;
  FilterLevel *level;
  GtkTreeModelFilter *filter = GTK_TREE_MODEL_FILTER (data);

  GtkTreePath *path;
  GtkTreeIter iter;

  gint *tmp_array;
  gint i, j, elt_count;
  gint length;

  GArray *new_array;

  g_return_if_fail (new_order != NULL);

  if (c_path == NULL || gtk_tree_path_get_depth (c_path) == 0)
    {
      length = gtk_tree_model_iter_n_children (c_model, NULL);

      if (filter->priv->virtual_root)
        {
          gint new_pos = -1;

          /* reorder root level of path */
          for (i = 0; i < length; i++)
            if (new_order[i] == gtk_tree_path_get_indices (filter->priv->virtual_root)[0])
              new_pos = i;

          if (new_pos < 0)
            return;

          gtk_tree_path_get_indices (filter->priv->virtual_root)[0] = new_pos;
          return;
        }

      path = gtk_tree_path_new ();
      level = FILTER_LEVEL (filter->priv->root);
    }
  else
    {
      GtkTreeIter child_iter;

      /* virtual root anchor reordering */
      if (filter->priv->virtual_root &&
	  gtk_tree_path_is_ancestor (c_path, filter->priv->virtual_root))
        {
          gint new_pos = -1;
          gint length;
          gint level;
          GtkTreeIter real_c_iter;

          level = gtk_tree_path_get_depth (c_path);

          if (c_iter)
            real_c_iter = *c_iter;
          else
            gtk_tree_model_get_iter (c_model, &real_c_iter, c_path);

          length = gtk_tree_model_iter_n_children (c_model, &real_c_iter);

          for (i = 0; i < length; i++)
            if (new_order[i] == gtk_tree_path_get_indices (filter->priv->virtual_root)[level])
              new_pos = i;

          if (new_pos < 0)
            return;

          gtk_tree_path_get_indices (filter->priv->virtual_root)[level] = new_pos;
          return;
        }

      path = gtk_real_tree_model_filter_convert_child_path_to_path (filter,
                                                                    c_path,
                                                                    FALSE,
                                                                    FALSE);

      if (!path && filter->priv->virtual_root &&
          gtk_tree_path_compare (c_path, filter->priv->virtual_root))
        return;

      if (!path && !filter->priv->virtual_root)
        return;

      if (!path)
        {
          /* root level mode */
          if (!c_iter)
            gtk_tree_model_get_iter (c_model, c_iter, c_path);
          length = gtk_tree_model_iter_n_children (c_model, c_iter);
          path = gtk_tree_path_new ();
          level = FILTER_LEVEL (filter->priv->root);
        }
      else
        {
          gtk_tree_model_filter_get_iter_full (GTK_TREE_MODEL (data),
                                               &iter, path);

          level = FILTER_LEVEL (iter.user_data);
          elt = FILTER_ELT (iter.user_data2);

          if (!elt->children)
            {
              gtk_tree_path_free (path);
              return;
            }

          level = elt->children;

          gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (filter), &child_iter, &iter);
          length = gtk_tree_model_iter_n_children (c_model, &child_iter);
        }
    }

  if (!level || level->array->len < 1)
    {
      gtk_tree_path_free (path);
      return;
    }

  /* NOTE: we do not bail out here if level->array->len < 2 like
   * GtkTreeModelSort does. This because we do some special tricky
   * reordering.
   */

  /* construct a new array */
  new_array = g_array_sized_new (FALSE, FALSE, sizeof (FilterElt),
                                 level->array->len);
  tmp_array = g_new (gint, level->array->len);

  for (i = 0, elt_count = 0; i < length; i++)
    {
      FilterElt *e = NULL;
      gint old_offset = -1;

      for (j = 0; j < level->array->len; j++)
        if (g_array_index (level->array, FilterElt, j).offset == new_order[i])
          {
            e = &g_array_index (level->array, FilterElt, j);
            old_offset = j;
            break;
          }

      if (!e)
        continue;

      tmp_array[elt_count] = old_offset;
      g_array_append_val (new_array, *e);
      g_array_index (new_array, FilterElt, elt_count).offset = i;
      elt_count++;
    }

  g_array_free (level->array, TRUE);
  level->array = new_array;

  /* fix up stuff */
  for (i = 0; i < level->array->len; i++)
    {
      FilterElt *e = &g_array_index (level->array, FilterElt, i);
      if (e->children)
        e->children->parent_elt_index = i;
    }

  /* emit rows_reordered */
  if (!gtk_tree_path_get_indices (path))
    gtk_tree_model_rows_reordered (GTK_TREE_MODEL (data), path, NULL,
                                   tmp_array);
  else
    {
      /* get a path taking only visible nodes into account */
      gtk_tree_path_free (path);
      path = gtk_tree_model_get_path (GTK_TREE_MODEL (data), &iter);

      gtk_tree_model_rows_reordered (GTK_TREE_MODEL (data), path, &iter,
                                     tmp_array);
    }

  /* done */
  g_free (tmp_array);
  gtk_tree_path_free (path);
}

/* TreeModelIface implementation */
static GtkTreeModelFlags
gtk_tree_model_filter_get_flags (GtkTreeModel *model)
{
  GtkTreeModelFlags flags;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (model), 0);
  g_return_val_if_fail (GTK_TREE_MODEL_FILTER (model)->priv->child_model != NULL, 0);

  flags = gtk_tree_model_get_flags (GTK_TREE_MODEL_FILTER (model)->priv->child_model);

  if ((flags & GTK_TREE_MODEL_LIST_ONLY) == GTK_TREE_MODEL_LIST_ONLY)
    return GTK_TREE_MODEL_LIST_ONLY;

  return 0;
}

static gint
gtk_tree_model_filter_get_n_columns (GtkTreeModel *model)
{
  GtkTreeModelFilter *filter = (GtkTreeModelFilter *)model;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (model), 0);
  g_return_val_if_fail (filter->priv->child_model != NULL, 0);

  if (filter->priv->child_model == NULL)
    return 0;

  /* so we can't modify the modify func after this ... */
  filter->priv->modify_func_set = TRUE;

  if (filter->priv->modify_n_columns > 0)
    return filter->priv->modify_n_columns;

  return gtk_tree_model_get_n_columns (filter->priv->child_model);
}

static GType
gtk_tree_model_filter_get_column_type (GtkTreeModel *model,
                                       gint          index)
{
  GtkTreeModelFilter *filter = (GtkTreeModelFilter *)model;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (model), G_TYPE_INVALID);
  g_return_val_if_fail (filter->priv->child_model != NULL, G_TYPE_INVALID);

  /* so we can't modify the modify func after this ... */
  filter->priv->modify_func_set = TRUE;

  if (filter->priv->modify_types)
    {
      g_return_val_if_fail (index < filter->priv->modify_n_columns, G_TYPE_INVALID);

      return filter->priv->modify_types[index];
    }

  return gtk_tree_model_get_column_type (filter->priv->child_model, index);
}

/* A special case of _get_iter; this function can also get iters which
 * are not visible.  These iters should ONLY be passed internally, never
 * pass those along with a signal emission.
 */
static gboolean
gtk_tree_model_filter_get_iter_full (GtkTreeModel *model,
                                     GtkTreeIter  *iter,
                                     GtkTreePath  *path)
{
  GtkTreeModelFilter *filter = (GtkTreeModelFilter *)model;
  gint *indices;
  FilterLevel *level;
  FilterElt *elt;
  gint depth, i;
  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (model), FALSE);
  g_return_val_if_fail (filter->priv->child_model != NULL, FALSE);

  indices = gtk_tree_path_get_indices (path);

  if (filter->priv->root == NULL)
    gtk_tree_model_filter_build_level (filter, NULL, -1, FALSE);
  level = FILTER_LEVEL (filter->priv->root);

  depth = gtk_tree_path_get_depth (path);
  if (!depth)
    {
      iter->stamp = 0;
      return FALSE;
    }

  for (i = 0; i < depth - 1; i++)
    {
      if (!level || indices[i] >= level->array->len)
        {
          return FALSE;
        }

      elt = gtk_tree_model_filter_get_nth (filter, level, indices[i]);

      if (!elt->children)
        gtk_tree_model_filter_build_level (filter, level,
                                           FILTER_LEVEL_ELT_INDEX (level, elt),
                                           FALSE);
      level = elt->children;
    }

  if (!level || indices[i] >= level->array->len)
    {
      iter->stamp = 0;
      return FALSE;
    }

  iter->stamp = filter->priv->stamp;
  iter->user_data = level;

  elt = gtk_tree_model_filter_get_nth (filter, level, indices[depth - 1]);
  iter->user_data2 = elt;

  return TRUE;
}

static gboolean
gtk_tree_model_filter_get_iter (GtkTreeModel *model,
                                GtkTreeIter  *iter,
                                GtkTreePath  *path)
{
  GtkTreeModelFilter *filter = (GtkTreeModelFilter *)model;
  gint *indices;
  FilterLevel *level;
  FilterElt *elt;
  gint depth, i;
  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (model), FALSE);
  g_return_val_if_fail (filter->priv->child_model != NULL, FALSE);

  indices = gtk_tree_path_get_indices (path);

  if (filter->priv->root == NULL)
    gtk_tree_model_filter_build_level (filter, NULL, -1, FALSE);
  level = FILTER_LEVEL (filter->priv->root);

  depth = gtk_tree_path_get_depth (path);
  if (!depth)
    {
      iter->stamp = 0;
      return FALSE;
    }

  for (i = 0; i < depth - 1; i++)
    {
      if (!level || indices[i] >= level->visible_nodes)
        {
          return FALSE;
        }

      elt = gtk_tree_model_filter_get_nth_visible (filter, level, indices[i]);

      if (!elt->children)
        gtk_tree_model_filter_build_level (filter, level,
                                           FILTER_LEVEL_ELT_INDEX (level, elt),
                                           FALSE);
      level = elt->children;
    }

  if (!level || indices[i] >= level->visible_nodes)
    {
      iter->stamp = 0;
      return FALSE;
    }

  iter->stamp = filter->priv->stamp;
  iter->user_data = level;

  elt = gtk_tree_model_filter_get_nth_visible (filter, level,
                                               indices[depth - 1]);
  iter->user_data2 = elt;

  return TRUE;
}

static GtkTreePath *
gtk_tree_model_filter_get_path (GtkTreeModel *model,
                                GtkTreeIter  *iter)
{
  GtkTreePath *retval;
  FilterLevel *level;
  FilterElt *elt;
  gint elt_index;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (model), NULL);
  g_return_val_if_fail (GTK_TREE_MODEL_FILTER (model)->priv->child_model != NULL, NULL);
  g_return_val_if_fail (GTK_TREE_MODEL_FILTER (model)->priv->stamp == iter->stamp, NULL);

  level = iter->user_data;
  elt = iter->user_data2;
  elt_index = FILTER_LEVEL_ELT_INDEX (level, elt);

  if (!elt->visible)
    return NULL;

  retval = gtk_tree_path_new ();

  while (level)
    {
      int i = 0, index = 0;

      while (i < elt_index)
        {
          if (g_array_index (level->array, FilterElt, i).visible)
            index++;
          i++;

          g_assert (i < level->array->len);
        }

      gtk_tree_path_prepend_index (retval, index);
      elt_index = level->parent_elt_index;
      level = level->parent_level;
    }

  return retval;
}

static void
gtk_tree_model_filter_get_value (GtkTreeModel *model,
                                 GtkTreeIter  *iter,
                                 gint          column,
                                 GValue       *value)
{
  GtkTreeIter child_iter;
  GtkTreeModelFilter *filter = GTK_TREE_MODEL_FILTER (model);

  g_return_if_fail (GTK_IS_TREE_MODEL_FILTER (model));
  g_return_if_fail (GTK_TREE_MODEL_FILTER (model)->priv->child_model != NULL);
  g_return_if_fail (GTK_TREE_MODEL_FILTER (model)->priv->stamp == iter->stamp);

  if (filter->priv->modify_func)
    {
      g_return_if_fail (column < filter->priv->modify_n_columns);

      g_value_init (value, filter->priv->modify_types[column]);
      filter->priv->modify_func (model,
                           iter,
                           value,
                           column,
                           filter->priv->modify_data);

      return;
    }

  gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model), &child_iter, iter);
  gtk_tree_model_get_value (GTK_TREE_MODEL_FILTER (model)->priv->child_model,
                            &child_iter, column, value);
}

static gboolean
gtk_tree_model_filter_iter_next (GtkTreeModel *model,
                                 GtkTreeIter  *iter)
{
  int i;
  FilterLevel *level;
  FilterElt *elt;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (model), FALSE);
  g_return_val_if_fail (GTK_TREE_MODEL_FILTER (model)->priv->child_model != NULL, FALSE);
  g_return_val_if_fail (GTK_TREE_MODEL_FILTER (model)->priv->stamp == iter->stamp, FALSE);

  level = iter->user_data;
  elt = iter->user_data2;

  i = elt - FILTER_ELT (level->array->data);

  while (i < level->array->len - 1)
    {
      i++;
      elt++;

      if (elt->visible)
        {
          iter->user_data2 = elt;
          return TRUE;
        }
    }

  /* no next visible iter */
  iter->stamp = 0;

  return FALSE;
}

static gboolean
gtk_tree_model_filter_iter_children (GtkTreeModel *model,
                                     GtkTreeIter  *iter,
                                     GtkTreeIter  *parent)
{
  GtkTreeModelFilter *filter = (GtkTreeModelFilter *)model;
  FilterLevel *level;

  iter->stamp = 0;
  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (model), FALSE);
  g_return_val_if_fail (filter->priv->child_model != NULL, FALSE);
  if (parent)
    g_return_val_if_fail (filter->priv->stamp == parent->stamp, FALSE);

  if (!parent)
    {
      int i = 0;

      if (!filter->priv->root)
        gtk_tree_model_filter_build_level (filter, NULL, -1, FALSE);
      if (!filter->priv->root)
        return FALSE;

      level = filter->priv->root;

      if (!level->visible_nodes)
        return FALSE;

      iter->stamp = filter->priv->stamp;
      iter->user_data = level;

      while (i < level->array->len)
        {
          if (!g_array_index (level->array, FilterElt, i).visible)
            {
              i++;
              continue;
            }

          iter->user_data2 = &g_array_index (level->array, FilterElt, i);
          return TRUE;
        }

      iter->stamp = 0;
      return FALSE;
    }
  else
    {
      int i = 0;
      FilterElt *elt;

      elt = FILTER_ELT (parent->user_data2);

      if (elt->children == NULL)
        gtk_tree_model_filter_build_level (filter,
                                           FILTER_LEVEL (parent->user_data),
                                           FILTER_LEVEL_ELT_INDEX (parent->user_data, elt),
                                           FALSE);

      if (elt->children == NULL)
        return FALSE;

      if (elt->children->visible_nodes <= 0)
        return FALSE;

      iter->stamp = filter->priv->stamp;
      iter->user_data = elt->children;

      level = FILTER_LEVEL (iter->user_data);

      while (i < level->array->len)
        {
          if (!g_array_index (level->array, FilterElt, i).visible)
            {
              i++;
              continue;
            }

          iter->user_data2 = &g_array_index (level->array, FilterElt, i);
          return TRUE;
        }

      iter->stamp = 0;
      return FALSE;
    }

  iter->stamp = 0;
  return FALSE;
}

static gboolean
gtk_tree_model_filter_iter_has_child (GtkTreeModel *model,
                                      GtkTreeIter  *iter)
{
  GtkTreeIter child_iter;
  GtkTreeModelFilter *filter = (GtkTreeModelFilter *)model;
  FilterElt *elt;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (model), FALSE);
  g_return_val_if_fail (filter->priv->child_model != NULL, FALSE);
  g_return_val_if_fail (filter->priv->stamp == iter->stamp, FALSE);

  filter = GTK_TREE_MODEL_FILTER (model);

  gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model), &child_iter, iter);
  elt = FILTER_ELT (iter->user_data2);

  if (!elt->visible)
    return FALSE;

  /* we need to build the level to check if not all children are filtered
   * out
   */
  if (!elt->children
      && gtk_tree_model_iter_has_child (filter->priv->child_model, &child_iter))
    gtk_tree_model_filter_build_level (filter, FILTER_LEVEL (iter->user_data),
                                       FILTER_LEVEL_ELT_INDEX (iter->user_data, elt),
                                       FALSE);

  if (elt->children && elt->children->visible_nodes > 0)
    return TRUE;

  return FALSE;
}

static gint
gtk_tree_model_filter_iter_n_children (GtkTreeModel *model,
                                       GtkTreeIter  *iter)
{
  GtkTreeIter child_iter;
  GtkTreeModelFilter *filter = (GtkTreeModelFilter *)model;
  FilterElt *elt;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (model), 0);
  g_return_val_if_fail (filter->priv->child_model != NULL, 0);
  if (iter)
    g_return_val_if_fail (filter->priv->stamp == iter->stamp, 0);

  if (!iter)
    {
      if (!filter->priv->root)
        gtk_tree_model_filter_build_level (filter, NULL, -1, FALSE);

      if (filter->priv->root)
        return FILTER_LEVEL (filter->priv->root)->visible_nodes;

      return 0;
    }

  elt = FILTER_ELT (iter->user_data2);

  if (!elt->visible)
    return 0;

  gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model), &child_iter, iter);

  if (!elt->children &&
      gtk_tree_model_iter_has_child (filter->priv->child_model, &child_iter))
    gtk_tree_model_filter_build_level (filter,
                                       FILTER_LEVEL (iter->user_data),
                                       FILTER_LEVEL_ELT_INDEX (iter->user_data, elt),
                                       FALSE);

  if (elt->children)
    return elt->children->visible_nodes;

  return 0;
}

static gboolean
gtk_tree_model_filter_iter_nth_child (GtkTreeModel *model,
                                      GtkTreeIter  *iter,
                                      GtkTreeIter  *parent,
                                      gint          n)
{
  FilterElt *elt;
  FilterLevel *level;
  GtkTreeIter children;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (model), FALSE);
  if (parent)
    g_return_val_if_fail (GTK_TREE_MODEL_FILTER (model)->priv->stamp == parent->stamp, FALSE);

  /* use this instead of has_child to force us to build the level, if needed */
  if (gtk_tree_model_filter_iter_children (model, &children, parent) == FALSE)
    {
      iter->stamp = 0;
      return FALSE;
    }

  level = children.user_data;
  elt = FILTER_ELT (level->array->data);

  if (n >= level->visible_nodes)
    {
      iter->stamp = 0;
      return FALSE;
    }

  elt = gtk_tree_model_filter_get_nth_visible (GTK_TREE_MODEL_FILTER (model),
                                               level, n);

  iter->stamp = GTK_TREE_MODEL_FILTER (model)->priv->stamp;
  iter->user_data = level;
  iter->user_data2 = elt;

  return TRUE;
}

static gboolean
gtk_tree_model_filter_iter_parent (GtkTreeModel *model,
                                   GtkTreeIter  *iter,
                                   GtkTreeIter  *child)
{
  FilterLevel *level;

  iter->stamp = 0;
  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (model), FALSE);
  g_return_val_if_fail (GTK_TREE_MODEL_FILTER (model)->priv->child_model != NULL, FALSE);
  g_return_val_if_fail (GTK_TREE_MODEL_FILTER (model)->priv->stamp == child->stamp, FALSE);

  level = child->user_data;

  if (level->parent_level)
    {
      iter->stamp = GTK_TREE_MODEL_FILTER (model)->priv->stamp;
      iter->user_data = level->parent_level;
      iter->user_data2 = FILTER_LEVEL_PARENT_ELT (level);

      return TRUE;
    }

  return FALSE;
}

static void
gtk_tree_model_filter_ref_node (GtkTreeModel *model,
                                GtkTreeIter  *iter)
{
  GtkTreeModelFilter *filter = (GtkTreeModelFilter *)model;
  GtkTreeIter child_iter;
  FilterLevel *level;
  FilterElt *elt;

  g_return_if_fail (GTK_IS_TREE_MODEL_FILTER (model));
  g_return_if_fail (GTK_TREE_MODEL_FILTER (model)->priv->child_model != NULL);
  g_return_if_fail (GTK_TREE_MODEL_FILTER (model)->priv->stamp == iter->stamp);

  gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model), &child_iter, iter);

  gtk_tree_model_ref_node (filter->priv->child_model, &child_iter);

  level = iter->user_data;
  elt = iter->user_data2;

  elt->ref_count++;
  level->ref_count++;
  if (level->ref_count == 1)
    {
      FilterLevel *parent_level = level->parent_level;
      gint parent_elt_index = level->parent_elt_index;

      /* we were at zero -- time to decrease the zero_ref_count val */
      while (parent_level)
        {
          g_array_index (parent_level->array, FilterElt, parent_elt_index).zero_ref_count--;

          parent_elt_index = parent_level->parent_elt_index;
	  parent_level = parent_level->parent_level;
        }

      if (filter->priv->root != level)
	filter->priv->zero_ref_count--;
    }
}

static void
gtk_tree_model_filter_unref_node (GtkTreeModel *model,
                                  GtkTreeIter  *iter)
{
  gtk_tree_model_filter_real_unref_node (model, iter, TRUE);
}

static void
gtk_tree_model_filter_real_unref_node (GtkTreeModel *model,
                                       GtkTreeIter  *iter,
                                       gboolean      propagate_unref)
{
  GtkTreeModelFilter *filter = (GtkTreeModelFilter *)model;
  FilterLevel *level;
  FilterElt *elt;

  g_return_if_fail (GTK_IS_TREE_MODEL_FILTER (model));
  g_return_if_fail (filter->priv->child_model != NULL);
  g_return_if_fail (filter->priv->stamp == iter->stamp);

  if (propagate_unref)
    {
      GtkTreeIter child_iter;
      gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model), &child_iter, iter);
      gtk_tree_model_unref_node (filter->priv->child_model, &child_iter);
    }

  level = iter->user_data;
  elt = iter->user_data2;

  g_return_if_fail (elt->ref_count > 0);

  elt->ref_count--;
  level->ref_count--;
  if (level->ref_count == 0)
    {
      FilterLevel *parent_level = level->parent_level;
      gint parent_elt_index = level->parent_elt_index;

      /* we are at zero -- time to increase the zero_ref_count val */
      while (parent_level)
        {
          g_array_index (parent_level->array, FilterElt, parent_elt_index).zero_ref_count++;

          parent_elt_index = parent_level->parent_elt_index;
          parent_level = parent_level->parent_level;
        }

      if (filter->priv->root != level)
	filter->priv->zero_ref_count++;
    }
}

/* TreeDragSource interface implementation */
static gboolean
gtk_tree_model_filter_row_draggable (GtkTreeDragSource *drag_source,
                                     GtkTreePath       *path)
{
  GtkTreeModelFilter *tree_model_filter = (GtkTreeModelFilter *)drag_source;
  GtkTreePath *child_path;
  gboolean draggable;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (drag_source), FALSE);
  g_return_val_if_fail (path != NULL, FALSE);

  child_path = gtk_tree_model_filter_convert_path_to_child_path (tree_model_filter, path);
  draggable = gtk_tree_drag_source_row_draggable (GTK_TREE_DRAG_SOURCE (tree_model_filter->priv->child_model), child_path);
  gtk_tree_path_free (child_path);

  return draggable;
}

static gboolean
gtk_tree_model_filter_drag_data_get (GtkTreeDragSource *drag_source,
                                     GtkTreePath       *path,
                                     GtkSelectionData  *selection_data)
{
  GtkTreeModelFilter *tree_model_filter = (GtkTreeModelFilter *)drag_source;
  GtkTreePath *child_path;
  gboolean gotten;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (drag_source), FALSE);
  g_return_val_if_fail (path != NULL, FALSE);

  child_path = gtk_tree_model_filter_convert_path_to_child_path (tree_model_filter, path);
  gotten = gtk_tree_drag_source_drag_data_get (GTK_TREE_DRAG_SOURCE (tree_model_filter->priv->child_model), child_path, selection_data);
  gtk_tree_path_free (child_path);

  return gotten;
}

static gboolean
gtk_tree_model_filter_drag_data_delete (GtkTreeDragSource *drag_source,
                                        GtkTreePath       *path)
{
  GtkTreeModelFilter *tree_model_filter = (GtkTreeModelFilter *)drag_source;
  GtkTreePath *child_path;
  gboolean deleted;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (drag_source), FALSE);
  g_return_val_if_fail (path != NULL, FALSE);

  child_path = gtk_tree_model_filter_convert_path_to_child_path (tree_model_filter, path);
  deleted = gtk_tree_drag_source_drag_data_delete (GTK_TREE_DRAG_SOURCE (tree_model_filter->priv->child_model), child_path);
  gtk_tree_path_free (child_path);

  return deleted;
}

/* bits and pieces */
static void
gtk_tree_model_filter_set_model (GtkTreeModelFilter *filter,
                                 GtkTreeModel       *child_model)
{
  g_return_if_fail (GTK_IS_TREE_MODEL_FILTER (filter));

  if (filter->priv->child_model)
    {
      g_signal_handler_disconnect (filter->priv->child_model,
                                   filter->priv->changed_id);
      g_signal_handler_disconnect (filter->priv->child_model,
                                   filter->priv->inserted_id);
      g_signal_handler_disconnect (filter->priv->child_model,
                                   filter->priv->has_child_toggled_id);
      g_signal_handler_disconnect (filter->priv->child_model,
                                   filter->priv->deleted_id);
      g_signal_handler_disconnect (filter->priv->child_model,
                                   filter->priv->reordered_id);

      /* reset our state */
      if (filter->priv->root)
        gtk_tree_model_filter_free_level (filter, filter->priv->root);

      filter->priv->root = NULL;
      g_object_unref (filter->priv->child_model);
      filter->priv->visible_column = -1;

      /* FIXME: do we need to destroy more here? */
    }

  filter->priv->child_model = child_model;

  if (child_model)
    {
      g_object_ref (filter->priv->child_model);
      filter->priv->changed_id =
        g_signal_connect (child_model, "row-changed",
                          G_CALLBACK (gtk_tree_model_filter_row_changed),
                          filter);
      filter->priv->inserted_id =
        g_signal_connect (child_model, "row-inserted",
                          G_CALLBACK (gtk_tree_model_filter_row_inserted),
                          filter);
      filter->priv->has_child_toggled_id =
        g_signal_connect (child_model, "row-has-child-toggled",
                          G_CALLBACK (gtk_tree_model_filter_row_has_child_toggled),
                          filter);
      filter->priv->deleted_id =
        g_signal_connect (child_model, "row-deleted",
                          G_CALLBACK (gtk_tree_model_filter_row_deleted),
                          filter);
      filter->priv->reordered_id =
        g_signal_connect (child_model, "rows-reordered",
                          G_CALLBACK (gtk_tree_model_filter_rows_reordered),
                          filter);

      filter->priv->child_flags = gtk_tree_model_get_flags (child_model);
      filter->priv->stamp = g_random_int ();
    }
}

static void
gtk_tree_model_filter_ref_path (GtkTreeModelFilter *filter,
                                GtkTreePath        *path)
{
  int len;
  GtkTreePath *p;

  len = gtk_tree_path_get_depth (path);
  p = gtk_tree_path_copy (path);
  while (len--)
    {
      GtkTreeIter iter;

      gtk_tree_model_get_iter (GTK_TREE_MODEL (filter->priv->child_model), &iter, p);
      gtk_tree_model_ref_node (GTK_TREE_MODEL (filter->priv->child_model), &iter);
      gtk_tree_path_up (p);
    }

  gtk_tree_path_free (p);
}

static void
gtk_tree_model_filter_unref_path (GtkTreeModelFilter *filter,
                                  GtkTreePath        *path)
{
  int len;
  GtkTreePath *p;

  len = gtk_tree_path_get_depth (path);
  p = gtk_tree_path_copy (path);
  while (len--)
    {
      GtkTreeIter iter;

      gtk_tree_model_get_iter (GTK_TREE_MODEL (filter->priv->child_model), &iter, p);
      gtk_tree_model_unref_node (GTK_TREE_MODEL (filter->priv->child_model), &iter);
      gtk_tree_path_up (p);
    }

  gtk_tree_path_free (p);
}

static void
gtk_tree_model_filter_set_root (GtkTreeModelFilter *filter,
                                GtkTreePath        *root)
{
  g_return_if_fail (GTK_IS_TREE_MODEL_FILTER (filter));

  if (!root)
    filter->priv->virtual_root = NULL;
  else
    filter->priv->virtual_root = gtk_tree_path_copy (root);
}

/* public API */

/**
 * gtk_tree_model_filter_new:
 * @child_model: A #GtkTreeModel.
 * @root: (allow-none): A #GtkTreePath or %NULL.
 *
 * Creates a new #GtkTreeModel, with @child_model as the child_model
 * and @root as the virtual root.
 *
 * Return value: (transfer full): A new #GtkTreeModel.
 *
 * Since: 2.4
 */
GtkTreeModel *
gtk_tree_model_filter_new (GtkTreeModel *child_model,
                           GtkTreePath  *root)
{
  GtkTreeModel *retval;
  GtkTreeModelFilter *filter;

  g_return_val_if_fail (GTK_IS_TREE_MODEL (child_model), NULL);

  retval = g_object_new (GTK_TYPE_TREE_MODEL_FILTER, 
			 "child-model", child_model,
			 "virtual-root", root,
			 NULL);

  filter = GTK_TREE_MODEL_FILTER (retval);
  if (filter->priv->virtual_root)
    {
      gtk_tree_model_filter_ref_path (filter, filter->priv->virtual_root);
      filter->priv->virtual_root_deleted = FALSE;
    }

  return retval;
}

/**
 * gtk_tree_model_filter_get_model:
 * @filter: A #GtkTreeModelFilter.
 *
 * Returns a pointer to the child model of @filter.
 *
 * Return value: (transfer none): A pointer to a #GtkTreeModel.
 *
 * Since: 2.4
 */
GtkTreeModel *
gtk_tree_model_filter_get_model (GtkTreeModelFilter *filter)
{
  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (filter), NULL);

  return filter->priv->child_model;
}

/**
 * gtk_tree_model_filter_set_visible_func:
 * @filter: A #GtkTreeModelFilter.
 * @func: A #GtkTreeModelFilterVisibleFunc, the visible function.
 * @data: (allow-none): User data to pass to the visible function, or %NULL.
 * @destroy: (allow-none): Destroy notifier of @data, or %NULL.
 *
 * Sets the visible function used when filtering the @filter to be @func. The
 * function should return %TRUE if the given row should be visible and
 * %FALSE otherwise.
 *
 * If the condition calculated by the function changes over time (e.g. because
 * it depends on some global parameters), you must call 
 * gtk_tree_model_filter_refilter() to keep the visibility information of 
 * the model uptodate.
 *
 * Note that @func is called whenever a row is inserted, when it may still be
 * empty. The visible function should therefore take special care of empty
 * rows, like in the example below.
 *
 * <informalexample><programlisting>
 * static gboolean
 * visible_func (GtkTreeModel *model,
 *               GtkTreeIter  *iter,
 *               gpointer      data)
 * {
 *   /&ast; Visible if row is non-empty and first column is "HI" &ast;/
 *   gchar *str;
 *   gboolean visible = FALSE;
 *
 *   gtk_tree_model_get (model, iter, 0, &str, -1);
 *   if (str && strcmp (str, "HI") == 0)
 *     visible = TRUE;
 *   g_free (str);
 *
 *   return visible;
 * }
 * </programlisting></informalexample>
 *
 * Since: 2.4
 */
void
gtk_tree_model_filter_set_visible_func (GtkTreeModelFilter            *filter,
                                        GtkTreeModelFilterVisibleFunc  func,
                                        gpointer                       data,
                                        GDestroyNotify                 destroy)
{
  g_return_if_fail (GTK_IS_TREE_MODEL_FILTER (filter));
  g_return_if_fail (func != NULL);
  g_return_if_fail (filter->priv->visible_method_set == FALSE);

  filter->priv->visible_func = func;
  filter->priv->visible_data = data;
  filter->priv->visible_destroy = destroy;

  filter->priv->visible_method_set = TRUE;
}

/**
 * gtk_tree_model_filter_set_modify_func:
 * @filter: A #GtkTreeModelFilter.
 * @n_columns: The number of columns in the filter model.
 * @types: (array length=n_columns): The #GType<!-- -->s of the columns.
 * @func: A #GtkTreeModelFilterModifyFunc
 * @data: (allow-none): User data to pass to the modify function, or %NULL.
 * @destroy: (allow-none): Destroy notifier of @data, or %NULL.
 *
 * With the @n_columns and @types parameters, you give an array of column
 * types for this model (which will be exposed to the parent model/view).
 * The @func, @data and @destroy parameters are for specifying the modify
 * function. The modify function will get called for <emphasis>each</emphasis>
 * data access, the goal of the modify function is to return the data which 
 * should be displayed at the location specified using the parameters of the 
 * modify function.
 *
 * Since: 2.4
 */
void
gtk_tree_model_filter_set_modify_func (GtkTreeModelFilter           *filter,
                                       gint                          n_columns,
                                       GType                        *types,
                                       GtkTreeModelFilterModifyFunc  func,
                                       gpointer                      data,
                                       GDestroyNotify                destroy)
{
  g_return_if_fail (GTK_IS_TREE_MODEL_FILTER (filter));
  g_return_if_fail (func != NULL);
  g_return_if_fail (filter->priv->modify_func_set == FALSE);

  if (filter->priv->modify_destroy)
    {
      GDestroyNotify d = filter->priv->modify_destroy;

      filter->priv->modify_destroy = NULL;
      d (filter->priv->modify_data);
    }

  filter->priv->modify_n_columns = n_columns;
  filter->priv->modify_types = g_new0 (GType, n_columns);
  memcpy (filter->priv->modify_types, types, sizeof (GType) * n_columns);
  filter->priv->modify_func = func;
  filter->priv->modify_data = data;
  filter->priv->modify_destroy = destroy;

  filter->priv->modify_func_set = TRUE;
}

/**
 * gtk_tree_model_filter_set_visible_column:
 * @filter: A #GtkTreeModelFilter.
 * @column: A #gint which is the column containing the visible information.
 *
 * Sets @column of the child_model to be the column where @filter should
 * look for visibility information. @columns should be a column of type
 * %G_TYPE_BOOLEAN, where %TRUE means that a row is visible, and %FALSE
 * if not.
 *
 * Since: 2.4
 */
void
gtk_tree_model_filter_set_visible_column (GtkTreeModelFilter *filter,
                                          gint column)
{
  g_return_if_fail (GTK_IS_TREE_MODEL_FILTER (filter));
  g_return_if_fail (column >= 0);
  g_return_if_fail (filter->priv->visible_method_set == FALSE);

  filter->priv->visible_column = column;

  filter->priv->visible_method_set = TRUE;
}

/* conversion */

/**
 * gtk_tree_model_filter_convert_child_iter_to_iter:
 * @filter: A #GtkTreeModelFilter.
 * @filter_iter: (out): An uninitialized #GtkTreeIter.
 * @child_iter: A valid #GtkTreeIter pointing to a row on the child model.
 *
 * Sets @filter_iter to point to the row in @filter that corresponds to the
 * row pointed at by @child_iter.  If @filter_iter was not set, %FALSE is
 * returned.
 *
 * Return value: %TRUE, if @filter_iter was set, i.e. if @child_iter is a
 * valid iterator pointing to a visible row in child model.
 *
 * Since: 2.4
 */
gboolean
gtk_tree_model_filter_convert_child_iter_to_iter (GtkTreeModelFilter *filter,
                                                  GtkTreeIter        *filter_iter,
                                                  GtkTreeIter        *child_iter)
{
  gboolean ret;
  GtkTreePath *child_path, *path;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (filter), FALSE);
  g_return_val_if_fail (filter->priv->child_model != NULL, FALSE);
  g_return_val_if_fail (filter_iter != NULL, FALSE);
  g_return_val_if_fail (child_iter != NULL, FALSE);
  g_return_val_if_fail (filter_iter != child_iter, FALSE);

  filter_iter->stamp = 0;

  child_path = gtk_tree_model_get_path (filter->priv->child_model, child_iter);
  g_return_val_if_fail (child_path != NULL, FALSE);

  path = gtk_tree_model_filter_convert_child_path_to_path (filter,
                                                           child_path);
  gtk_tree_path_free (child_path);

  if (!path)
    return FALSE;

  ret = gtk_tree_model_get_iter (GTK_TREE_MODEL (filter), filter_iter, path);
  gtk_tree_path_free (path);

  return ret;
}

/**
 * gtk_tree_model_filter_convert_iter_to_child_iter:
 * @filter: A #GtkTreeModelFilter.
 * @child_iter: (out): An uninitialized #GtkTreeIter.
 * @filter_iter: A valid #GtkTreeIter pointing to a row on @filter.
 *
 * Sets @child_iter to point to the row pointed to by @filter_iter.
 *
 * Since: 2.4
 */
void
gtk_tree_model_filter_convert_iter_to_child_iter (GtkTreeModelFilter *filter,
                                                  GtkTreeIter        *child_iter,
                                                  GtkTreeIter        *filter_iter)
{
  g_return_if_fail (GTK_IS_TREE_MODEL_FILTER (filter));
  g_return_if_fail (filter->priv->child_model != NULL);
  g_return_if_fail (child_iter != NULL);
  g_return_if_fail (filter_iter != NULL);
  g_return_if_fail (filter_iter->stamp == filter->priv->stamp);
  g_return_if_fail (filter_iter != child_iter);

  if (GTK_TREE_MODEL_FILTER_CACHE_CHILD_ITERS (filter))
    {
      *child_iter = FILTER_ELT (filter_iter->user_data2)->iter;
    }
  else
    {
      GtkTreePath *path;

      path = gtk_tree_model_filter_elt_get_path (filter_iter->user_data,
                                                 filter_iter->user_data2,
                                                 filter->priv->virtual_root);
      gtk_tree_model_get_iter (filter->priv->child_model, child_iter, path);
      gtk_tree_path_free (path);
    }
}

/* The path returned can only be used internally in the filter model. */
static GtkTreePath *
gtk_real_tree_model_filter_convert_child_path_to_path (GtkTreeModelFilter *filter,
                                                       GtkTreePath        *child_path,
                                                       gboolean            build_levels,
                                                       gboolean            fetch_children)
{
  gint *child_indices;
  GtkTreePath *retval;
  GtkTreePath *real_path;
  FilterLevel *level;
  FilterElt *tmp;
  gint i;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (filter), NULL);
  g_return_val_if_fail (filter->priv->child_model != NULL, NULL);
  g_return_val_if_fail (child_path != NULL, NULL);

  if (!filter->priv->virtual_root)
    real_path = gtk_tree_path_copy (child_path);
  else
    real_path = gtk_tree_model_filter_remove_root (child_path,
                                                   filter->priv->virtual_root);

  if (!real_path)
    return NULL;

  retval = gtk_tree_path_new ();
  child_indices = gtk_tree_path_get_indices (real_path);

  if (filter->priv->root == NULL && build_levels)
    gtk_tree_model_filter_build_level (filter, NULL, -1, FALSE);
  level = FILTER_LEVEL (filter->priv->root);

  for (i = 0; i < gtk_tree_path_get_depth (real_path); i++)
    {
      gint j;
      gboolean found_child = FALSE;

      if (!level)
        {
          gtk_tree_path_free (real_path);
          gtk_tree_path_free (retval);
          return NULL;
        }

      tmp = bsearch_elt_with_offset (level->array, child_indices[i], &j);
      if (tmp)
        {
          gtk_tree_path_append_index (retval, j);
          if (!tmp->children && build_levels)
            gtk_tree_model_filter_build_level (filter, level,
                                               FILTER_LEVEL_ELT_INDEX (level, tmp),
                                               FALSE);
          level = tmp->children;
          found_child = TRUE;
        }

      if (!found_child && fetch_children)
        {
          tmp = gtk_tree_model_filter_fetch_child (filter, level,
                                                   child_indices[i],
                                                   &j);

          /* didn't find the child, let's try to bring it back */
          if (!tmp || tmp->offset != child_indices[i])
            {
              /* not there */
              gtk_tree_path_free (real_path);
              gtk_tree_path_free (retval);
              return NULL;
            }

          gtk_tree_path_append_index (retval, j);
          if (!tmp->children && build_levels)
            gtk_tree_model_filter_build_level (filter, level,
                                               FILTER_LEVEL_ELT_INDEX (level, tmp),
                                               FALSE);
          level = tmp->children;
          found_child = TRUE;
        }
      else if (!found_child && !fetch_children)
        {
          /* no path */
          gtk_tree_path_free (real_path);
          gtk_tree_path_free (retval);
          return NULL;
        }
    }

  gtk_tree_path_free (real_path);
  return retval;
}

/**
 * gtk_tree_model_filter_convert_child_path_to_path:
 * @filter: A #GtkTreeModelFilter.
 * @child_path: A #GtkTreePath to convert.
 *
 * Converts @child_path to a path relative to @filter. That is, @child_path
 * points to a path in the child model. The rerturned path will point to the
 * same row in the filtered model. If @child_path isn't a valid path on the
 * child model or points to a row which is not visible in @filter, then %NULL
 * is returned.
 *
 * Return value: A newly allocated #GtkTreePath, or %NULL.
 *
 * Since: 2.4
 */
GtkTreePath *
gtk_tree_model_filter_convert_child_path_to_path (GtkTreeModelFilter *filter,
                                                  GtkTreePath        *child_path)
{
  GtkTreeIter iter;
  GtkTreePath *path;

  /* this function does the sanity checks */
  path = gtk_real_tree_model_filter_convert_child_path_to_path (filter,
                                                                child_path,
                                                                TRUE,
                                                                TRUE);

  if (!path)
      return NULL;

  /* get a new path which only takes visible nodes into account.
   * -- if this gives any performance issues, we can write a special
   *    version of convert_child_path_to_path immediately returning
   *    a visible-nodes-only path.
   */
  gtk_tree_model_filter_get_iter_full (GTK_TREE_MODEL (filter), &iter, path);

  gtk_tree_path_free (path);
  path = gtk_tree_model_get_path (GTK_TREE_MODEL (filter), &iter);

  return path;
}

/**
 * gtk_tree_model_filter_convert_path_to_child_path:
 * @filter: A #GtkTreeModelFilter.
 * @filter_path: A #GtkTreePath to convert.
 *
 * Converts @filter_path to a path on the child model of @filter. That is,
 * @filter_path points to a location in @filter. The returned path will
 * point to the same location in the model not being filtered. If @filter_path
 * does not point to a location in the child model, %NULL is returned.
 *
 * Return value: A newly allocated #GtkTreePath, or %NULL.
 *
 * Since: 2.4
 */
GtkTreePath *
gtk_tree_model_filter_convert_path_to_child_path (GtkTreeModelFilter *filter,
                                                  GtkTreePath        *filter_path)
{
  gint *filter_indices;
  GtkTreePath *retval;
  FilterLevel *level;
  gint i;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (filter), NULL);
  g_return_val_if_fail (filter->priv->child_model != NULL, NULL);
  g_return_val_if_fail (filter_path != NULL, NULL);

  /* convert path */
  retval = gtk_tree_path_new ();
  filter_indices = gtk_tree_path_get_indices (filter_path);
  if (!filter->priv->root)
    gtk_tree_model_filter_build_level (filter, NULL, -1, FALSE);
  level = FILTER_LEVEL (filter->priv->root);

  for (i = 0; i < gtk_tree_path_get_depth (filter_path); i++)
    {
      FilterElt *elt;

      if (!level || level->visible_nodes <= filter_indices[i])
        {
          gtk_tree_path_free (retval);
          return NULL;
        }

      elt = gtk_tree_model_filter_get_nth_visible (filter, level,
                                                   filter_indices[i]);

      if (elt->children == NULL)
        gtk_tree_model_filter_build_level (filter, level,
                                           FILTER_LEVEL_ELT_INDEX (level, elt),
                                           FALSE);

      if (!level || level->visible_nodes <= filter_indices[i])
        {
          gtk_tree_path_free (retval);
          return NULL;
        }

      gtk_tree_path_append_index (retval, elt->offset);
      level = elt->children;
    }

  /* apply vroot */

  if (filter->priv->virtual_root)
    {
      GtkTreePath *real_retval;

      real_retval = gtk_tree_model_filter_add_root (retval,
                                                    filter->priv->virtual_root);
      gtk_tree_path_free (retval);

      return real_retval;
    }

  return retval;
}

static gboolean
gtk_tree_model_filter_refilter_helper (GtkTreeModel *model,
                                       GtkTreePath  *path,
                                       GtkTreeIter  *iter,
                                       gpointer      data)
{
  /* evil, don't try this at home, but certainly speeds things up */
  gtk_tree_model_filter_row_changed (model, path, iter, data);

  return FALSE;
}

/**
 * gtk_tree_model_filter_refilter:
 * @filter: A #GtkTreeModelFilter.
 *
 * Emits ::row_changed for each row in the child model, which causes
 * the filter to re-evaluate whether a row is visible or not.
 *
 * Since: 2.4
 */
void
gtk_tree_model_filter_refilter (GtkTreeModelFilter *filter)
{
  g_return_if_fail (GTK_IS_TREE_MODEL_FILTER (filter));

  /* S L O W */
  gtk_tree_model_foreach (filter->priv->child_model,
                          gtk_tree_model_filter_refilter_helper,
                          filter);
}

/**
 * gtk_tree_model_filter_clear_cache:
 * @filter: A #GtkTreeModelFilter.
 *
 * This function should almost never be called. It clears the @filter
 * of any cached iterators that haven't been reffed with
 * gtk_tree_model_ref_node(). This might be useful if the child model
 * being filtered is static (and doesn't change often) and there has been
 * a lot of unreffed access to nodes. As a side effect of this function,
 * all unreffed iters will be invalid.
 *
 * Since: 2.4
 */
void
gtk_tree_model_filter_clear_cache (GtkTreeModelFilter *filter)
{
  g_return_if_fail (GTK_IS_TREE_MODEL_FILTER (filter));

  if (filter->priv->zero_ref_count > 0)
    gtk_tree_model_filter_clear_cache_helper (filter,
                                              FILTER_LEVEL (filter->priv->root));
}

#define __GTK_TREE_MODEL_FILTER_C__
#include "gtkaliasdef.c"
