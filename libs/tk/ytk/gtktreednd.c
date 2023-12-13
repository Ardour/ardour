/* gtktreednd.c
 * Copyright (C) 2001  Red Hat, Inc.
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
#include <string.h>
#include "gtktreednd.h"
#include "gtkintl.h"
#include "gtkalias.h"

GType
gtk_tree_drag_source_get_type (void)
{
  static GType our_type = 0;

  if (!our_type)
    {
      const GTypeInfo our_info =
      {
        sizeof (GtkTreeDragSourceIface), /* class_size */
	NULL,		/* base_init */
	NULL,		/* base_finalize */
	NULL,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	0,
	0,              /* n_preallocs */
	NULL
      };

      our_type = g_type_register_static (G_TYPE_INTERFACE, 
					 I_("GtkTreeDragSource"),
					 &our_info, 0);
    }
  
  return our_type;
}


GType
gtk_tree_drag_dest_get_type (void)
{
  static GType our_type = 0;

  if (!our_type)
    {
      const GTypeInfo our_info =
      {
        sizeof (GtkTreeDragDestIface), /* class_size */
	NULL,		/* base_init */
	NULL,		/* base_finalize */
	NULL,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	0,
	0,              /* n_preallocs */
	NULL
      };

      our_type = g_type_register_static (G_TYPE_INTERFACE, I_("GtkTreeDragDest"), &our_info, 0);
    }
  
  return our_type;
}

/**
 * gtk_tree_drag_source_row_draggable:
 * @drag_source: a #GtkTreeDragSource
 * @path: row on which user is initiating a drag
 * 
 * Asks the #GtkTreeDragSource whether a particular row can be used as
 * the source of a DND operation. If the source doesn't implement
 * this interface, the row is assumed draggable.
 *
 * Return value: %TRUE if the row can be dragged
 **/
gboolean
gtk_tree_drag_source_row_draggable (GtkTreeDragSource *drag_source,
                                    GtkTreePath       *path)
{
  GtkTreeDragSourceIface *iface = GTK_TREE_DRAG_SOURCE_GET_IFACE (drag_source);

  g_return_val_if_fail (path != NULL, FALSE);

  if (iface->row_draggable)
    return (* iface->row_draggable) (drag_source, path);
  else
    return TRUE;
    /* Returning TRUE if row_draggable is not implemented is a fallback.
       Interface implementations such as GtkTreeStore and GtkListStore really should
       implement row_draggable. */
}


/**
 * gtk_tree_drag_source_drag_data_delete:
 * @drag_source: a #GtkTreeDragSource
 * @path: row that was being dragged
 * 
 * Asks the #GtkTreeDragSource to delete the row at @path, because
 * it was moved somewhere else via drag-and-drop. Returns %FALSE
 * if the deletion fails because @path no longer exists, or for
 * some model-specific reason. Should robustly handle a @path no
 * longer found in the model!
 * 
 * Return value: %TRUE if the row was successfully deleted
 **/
gboolean
gtk_tree_drag_source_drag_data_delete (GtkTreeDragSource *drag_source,
                                       GtkTreePath       *path)
{
  GtkTreeDragSourceIface *iface = GTK_TREE_DRAG_SOURCE_GET_IFACE (drag_source);

  g_return_val_if_fail (iface->drag_data_delete != NULL, FALSE);
  g_return_val_if_fail (path != NULL, FALSE);

  return (* iface->drag_data_delete) (drag_source, path);
}

/**
 * gtk_tree_drag_source_drag_data_get:
 * @drag_source: a #GtkTreeDragSource
 * @path: row that was dragged
 * @selection_data: (out): a #GtkSelectionData to fill with data
 *                  from the dragged row
 * 
 * Asks the #GtkTreeDragSource to fill in @selection_data with a
 * representation of the row at @path. @selection_data->target gives
 * the required type of the data.  Should robustly handle a @path no
 * longer found in the model!
 * 
 * Return value: %TRUE if data of the required type was provided 
 **/
gboolean
gtk_tree_drag_source_drag_data_get    (GtkTreeDragSource *drag_source,
                                       GtkTreePath       *path,
                                       GtkSelectionData  *selection_data)
{
  GtkTreeDragSourceIface *iface = GTK_TREE_DRAG_SOURCE_GET_IFACE (drag_source);

  g_return_val_if_fail (iface->drag_data_get != NULL, FALSE);
  g_return_val_if_fail (path != NULL, FALSE);
  g_return_val_if_fail (selection_data != NULL, FALSE);

  return (* iface->drag_data_get) (drag_source, path, selection_data);
}

/**
 * gtk_tree_drag_dest_drag_data_received:
 * @drag_dest: a #GtkTreeDragDest
 * @dest: row to drop in front of
 * @selection_data: data to drop
 * 
 * Asks the #GtkTreeDragDest to insert a row before the path @dest,
 * deriving the contents of the row from @selection_data. If @dest is
 * outside the tree so that inserting before it is impossible, %FALSE
 * will be returned. Also, %FALSE may be returned if the new row is
 * not created for some model-specific reason.  Should robustly handle
 * a @dest no longer found in the model!
 * 
 * Return value: whether a new row was created before position @dest
 **/
gboolean
gtk_tree_drag_dest_drag_data_received (GtkTreeDragDest  *drag_dest,
                                       GtkTreePath      *dest,
                                       GtkSelectionData *selection_data)
{
  GtkTreeDragDestIface *iface = GTK_TREE_DRAG_DEST_GET_IFACE (drag_dest);

  g_return_val_if_fail (iface->drag_data_received != NULL, FALSE);
  g_return_val_if_fail (dest != NULL, FALSE);
  g_return_val_if_fail (selection_data != NULL, FALSE);

  return (* iface->drag_data_received) (drag_dest, dest, selection_data);
}


/**
 * gtk_tree_drag_dest_row_drop_possible:
 * @drag_dest: a #GtkTreeDragDest
 * @dest_path: destination row
 * @selection_data: the data being dragged
 * 
 * Determines whether a drop is possible before the given @dest_path,
 * at the same depth as @dest_path. i.e., can we drop the data in
 * @selection_data at that location. @dest_path does not have to
 * exist; the return value will almost certainly be %FALSE if the
 * parent of @dest_path doesn't exist, though.
 * 
 * Return value: %TRUE if a drop is possible before @dest_path
 **/
gboolean
gtk_tree_drag_dest_row_drop_possible (GtkTreeDragDest   *drag_dest,
                                      GtkTreePath       *dest_path,
				      GtkSelectionData  *selection_data)
{
  GtkTreeDragDestIface *iface = GTK_TREE_DRAG_DEST_GET_IFACE (drag_dest);

  g_return_val_if_fail (iface->row_drop_possible != NULL, FALSE);
  g_return_val_if_fail (selection_data != NULL, FALSE);
  g_return_val_if_fail (dest_path != NULL, FALSE);

  return (* iface->row_drop_possible) (drag_dest, dest_path, selection_data);
}

typedef struct _TreeRowData TreeRowData;

struct _TreeRowData
{
  GtkTreeModel *model;
  gchar path[4];
};

/**
 * gtk_tree_set_row_drag_data:
 * @selection_data: some #GtkSelectionData
 * @tree_model: a #GtkTreeModel
 * @path: a row in @tree_model
 * 
 * Sets selection data of target type %GTK_TREE_MODEL_ROW. Normally used
 * in a drag_data_get handler.
 * 
 * Return value: %TRUE if the #GtkSelectionData had the proper target type to allow us to set a tree row
 **/
gboolean
gtk_tree_set_row_drag_data (GtkSelectionData *selection_data,
			    GtkTreeModel     *tree_model,
			    GtkTreePath      *path)
{
  TreeRowData *trd;
  gchar *path_str;
  gint len;
  gint struct_size;
  
  g_return_val_if_fail (selection_data != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TREE_MODEL (tree_model), FALSE);
  g_return_val_if_fail (path != NULL, FALSE);

  if (selection_data->target != gdk_atom_intern_static_string ("GTK_TREE_MODEL_ROW"))
    return FALSE;
  
  path_str = gtk_tree_path_to_string (path);

  len = strlen (path_str);

  /* the old allocate-end-of-struct-to-hold-string trick */
  struct_size = sizeof (TreeRowData) + len + 1 -
    (sizeof (TreeRowData) - G_STRUCT_OFFSET (TreeRowData, path));

  trd = g_malloc (struct_size); 

  strcpy (trd->path, path_str);

  g_free (path_str);
  
  trd->model = tree_model;
  
  gtk_selection_data_set (selection_data,
                          gdk_atom_intern_static_string ("GTK_TREE_MODEL_ROW"),
                          8, /* bytes */
                          (void*)trd,
                          struct_size);

  g_free (trd);
  
  return TRUE;
}

/**
 * gtk_tree_get_row_drag_data:
 * @selection_data: a #GtkSelectionData
 * @tree_model: (out): a #GtkTreeModel
 * @path: (out): row in @tree_model
 * 
 * Obtains a @tree_model and @path from selection data of target type
 * %GTK_TREE_MODEL_ROW. Normally called from a drag_data_received handler.
 * This function can only be used if @selection_data originates from the same
 * process that's calling this function, because a pointer to the tree model
 * is being passed around. If you aren't in the same process, then you'll
 * get memory corruption. In the #GtkTreeDragDest drag_data_received handler,
 * you can assume that selection data of type %GTK_TREE_MODEL_ROW is
 * in from the current process. The returned path must be freed with
 * gtk_tree_path_free().
 * 
 * Return value: %TRUE if @selection_data had target type %GTK_TREE_MODEL_ROW and
 *  is otherwise valid
 **/
gboolean
gtk_tree_get_row_drag_data (GtkSelectionData  *selection_data,
			    GtkTreeModel     **tree_model,
			    GtkTreePath      **path)
{
  TreeRowData *trd;
  
  g_return_val_if_fail (selection_data != NULL, FALSE);  

  if (tree_model)
    *tree_model = NULL;

  if (path)
    *path = NULL;
  
  if (selection_data->target != gdk_atom_intern_static_string ("GTK_TREE_MODEL_ROW"))
    return FALSE;

  if (selection_data->length < 0)
    return FALSE;

  trd = (void*) selection_data->data;

  if (tree_model)
    *tree_model = trd->model;

  if (path)
    *path = gtk_tree_path_new_from_string (trd->path);
  
  return TRUE;
}

#define __GTK_TREE_DND_C__
#include "gtkaliasdef.c"
