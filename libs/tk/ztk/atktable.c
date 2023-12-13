/* ATK -  Accessibility Toolkit
 * Copyright 2001 Sun Microsystems Inc.
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

#include "atktable.h"
#include "atkmarshal.h"

/**
 * SECTION:atktable
 * @Short_description: The ATK interface implemented for UI components
 *  which contain tabular or row/column information.
 * @Title:AtkTable
 *
 * #AtkTable should be implemented by components which present
 * elements ordered via rows and columns.  It may also be used to
 * present tree-structured information if the nodes of the trees can
 * be said to contain multiple "columns".  Individual elements of an
 * #AtkTable are typically referred to as "cells". Those cells should
 * implement the interface #AtkTableCell, but #Atk doesn't require
 * them to be direct children of the current #AtkTable. They can be
 * grand-children, grand-grand-children etc. #AtkTable provides the
 * API needed to get a individual cell based on the row and column
 * numbers.
 *
 * Children of #AtkTable are frequently "lightweight" objects, that
 * is, they may not have backing widgets in the host UI toolkit.  They
 * are therefore often transient.
 *
 * Since tables are often very complex, #AtkTable includes provision
 * for offering simplified summary information, as well as row and
 * column headers and captions.  Headers and captions are #AtkObjects
 * which may implement other interfaces (#AtkText, #AtkImage, etc.) as
 * appropriate.  #AtkTable summaries may themselves be (simplified)
 * #AtkTables, etc.
 *
 * Note for implementors: in the past, #AtkTable required that all the
 * cells should be direct children of #AtkTable, and provided some
 * index based methods to request the cells. The practice showed that
 * that forcing made #AtkTable implementation complex, and hard to
 * expose other kind of children, like rows or captions. Right now,
 * index-based methods are deprecated.
 */

enum {
  ROW_INSERTED,
  ROW_DELETED,
  COLUMN_INSERTED,
  COLUMN_DELETED,
  ROW_REORDERED,
  COLUMN_REORDERED,
  MODEL_CHANGED,
  LAST_SIGNAL
};

static void  atk_table_base_init (gpointer *g_class);

static guint atk_table_signals[LAST_SIGNAL] = { 0 };

GType
atk_table_get_type (void)
{
  static GType type = 0;
  
  if (!type) {
    GTypeInfo tinfo =
    {
      sizeof (AtkTableIface),
      (GBaseInitFunc) atk_table_base_init,
      (GBaseFinalizeFunc) NULL,
      
    };
    
    type = g_type_register_static (G_TYPE_INTERFACE, "AtkTable", &tinfo, 0);
  }
  
  return type;
}


static void
atk_table_base_init (gpointer *g_class)
{
  static gboolean initialized = FALSE;
  
  if (!initialized)
    {
      /**
       * AtkTable::row-inserted:
       * @atktable: the object which received the signal.
       * @arg1: The index of the first row inserted.
       * @arg2: The number of rows inserted.
       *
       * The "row-inserted" signal is emitted by an object which
       * implements the AtkTable interface when a row is inserted.
       */
      atk_table_signals[ROW_INSERTED] =
	g_signal_new ("row_inserted",
		      ATK_TYPE_TABLE,
		      G_SIGNAL_RUN_LAST,
		      G_STRUCT_OFFSET (AtkTableIface, row_inserted),
		      (GSignalAccumulator) NULL, NULL,
		      atk_marshal_VOID__INT_INT,
		      G_TYPE_NONE,
		      2, G_TYPE_INT, G_TYPE_INT);
      /**
       * AtkTable::column-inserted:
       * @atktable: the object which received the signal.
       * @arg1: The index of the column inserted.
       * @arg2: The number of colums inserted.
       *
       * The "column-inserted" signal is emitted by an object which
       * implements the AtkTable interface when a column is inserted.
       */
      atk_table_signals[COLUMN_INSERTED] =
	g_signal_new ("column_inserted",
		      ATK_TYPE_TABLE,
		      G_SIGNAL_RUN_LAST,
		      G_STRUCT_OFFSET (AtkTableIface, column_inserted),
		      (GSignalAccumulator) NULL, NULL,
		      atk_marshal_VOID__INT_INT,
		      G_TYPE_NONE,
		      2, G_TYPE_INT, G_TYPE_INT);
      /**
       * AtkTable::row-deleted:
       * @atktable: the object which received the signal.
       * @arg1: The index of the first row deleted.
       * @arg2: The number of rows deleted.
       *
       * The "row-deleted" signal is emitted by an object which
       * implements the AtkTable interface when a row is deleted.
       */
      atk_table_signals[ROW_DELETED] =
	g_signal_new ("row_deleted",
		      ATK_TYPE_TABLE,
		      G_SIGNAL_RUN_LAST,
		      G_STRUCT_OFFSET (AtkTableIface, row_deleted),
		      (GSignalAccumulator) NULL, NULL,
		      atk_marshal_VOID__INT_INT,
		      G_TYPE_NONE,
		      2, G_TYPE_INT, G_TYPE_INT);
      /**
       * AtkTable::column-deleted:
       * @atktable: the object which received the signal.
       * @arg1: The index of the first column deleted.
       * @arg2: The number of columns deleted.
       *
       * The "column-deleted" signal is emitted by an object which
       * implements the AtkTable interface when a column is deleted.
       */
      atk_table_signals[COLUMN_DELETED] =
	g_signal_new ("column_deleted",
		      ATK_TYPE_TABLE,
		      G_SIGNAL_RUN_LAST,
		      G_STRUCT_OFFSET (AtkTableIface, column_deleted),
		      (GSignalAccumulator) NULL, NULL,
		      atk_marshal_VOID__INT_INT,
		      G_TYPE_NONE,
		      2, G_TYPE_INT, G_TYPE_INT);
      /**
       * AtkTable::row-reordered:
       * @atktable: the object which received the signal.
       *
       * The "row-reordered" signal is emitted by an object which
       * implements the AtkTable interface when the rows are
       * reordered.
       */
      atk_table_signals[ROW_REORDERED] =
	g_signal_new ("row_reordered",
		      ATK_TYPE_TABLE,
		      G_SIGNAL_RUN_LAST,
		      G_STRUCT_OFFSET (AtkTableIface, row_reordered),
		      (GSignalAccumulator) NULL, NULL,
		      g_cclosure_marshal_VOID__VOID,
		      G_TYPE_NONE,
		      0);
      /**
       * AtkTable::column-reordered:
       * @atktable: the object which received the signal.
       *
       * The "column-reordered" signal is emitted by an object which
       * implements the AtkTable interface when the columns are
       * reordered.
       */
      atk_table_signals[COLUMN_REORDERED] =
	g_signal_new ("column_reordered",
		      ATK_TYPE_TABLE,
		      G_SIGNAL_RUN_LAST,
		      G_STRUCT_OFFSET (AtkTableIface, column_reordered),
		      (GSignalAccumulator) NULL, NULL,
		      g_cclosure_marshal_VOID__VOID,
		      G_TYPE_NONE,
		      0);

      /**
       * AtkTable::model-changed:
       * @atktable: the object which received the signal.
       *
       * The "model-changed" signal is emitted by an object which
       * implements the AtkTable interface when the model displayed by
       * the table changes.
       */
      atk_table_signals[MODEL_CHANGED] =
        g_signal_new ("model_changed",
                      ATK_TYPE_TABLE,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (AtkTableIface, model_changed),
                      (GSignalAccumulator) NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);

      initialized = TRUE;
    }
}

/**
 * atk_table_ref_at:
 * @table: a GObject instance that implements AtkTableIface
 * @row: a #gint representing a row in @table
 * @column: a #gint representing a column in @table
 *
 * Get a reference to the table cell at @row, @column. This cell
 * should implement the interface #AtkTableCell
 *
 * Returns: (transfer full): an #AtkObject representing the referred
 * to accessible
 **/
AtkObject*
atk_table_ref_at (AtkTable *table,
                  gint     row,
                  gint     column)
{
  AtkTableIface *iface;

  g_return_val_if_fail (ATK_IS_TABLE (table), NULL);
  g_return_val_if_fail (row >= 0, NULL);
  g_return_val_if_fail (column >= 0, NULL);

  iface = ATK_TABLE_GET_IFACE (table);

  if (iface->ref_at)
    return (iface->ref_at) (table, row, column);
  else
    return NULL;
}

/**
 * atk_table_get_index_at:
 * @table: a GObject instance that implements AtkTableIface
 * @row: a #gint representing a row in @table
 * @column: a #gint representing a column in @table
 *
 * Gets a #gint representing the index at the specified @row and
 * @column.
 *
 * Deprecated: Since 2.12. Use atk_table_ref_at() in order to get the
 * accessible that represents the cell at (@row, @column)
 *
 * Returns: a #gint representing the index at specified position.
 * The value -1 is returned if the object at row,column is not a child
 * of table or table does not implement this interface.
 **/
gint
atk_table_get_index_at (AtkTable *table,
                        gint     row,
                        gint     column)
{
  AtkTableIface *iface;

  g_return_val_if_fail (ATK_IS_TABLE (table), -1);
  g_return_val_if_fail (row >= 0, -1);
  g_return_val_if_fail (column >= 0, -1);

  iface = ATK_TABLE_GET_IFACE (table);

  if (iface->get_index_at)
    return (iface->get_index_at) (table, row, column);
  else
    return -1;
}

/**
 * atk_table_get_row_at_index:
 * @table: a GObject instance that implements AtkTableInterface
 * @index_: a #gint representing an index in @table
 *
 * Gets a #gint representing the row at the specified @index_.
 *
 * Deprecated: since 2.12.
 *
 * Returns: a gint representing the row at the specified index,
 * or -1 if the table does not implement this method.
 **/
gint
atk_table_get_row_at_index (AtkTable *table,
                            gint     index)
{
  AtkTableIface *iface;

  g_return_val_if_fail (ATK_IS_TABLE (table), -1);

  iface = ATK_TABLE_GET_IFACE (table);

  if (iface->get_row_at_index)
    return (iface->get_row_at_index) (table, index);
  else
    return -1;
}

/**
 * atk_table_get_column_at_index:
 * @table: a GObject instance that implements AtkTableInterface
 * @index_: a #gint representing an index in @table
 *
 * Gets a #gint representing the column at the specified @index_.
 *
 * Deprecated: Since 2.12.
 *
 * Returns: a gint representing the column at the specified index,
 * or -1 if the table does not implement this method.
 **/
gint
atk_table_get_column_at_index (AtkTable *table,
                               gint     index)
{
  AtkTableIface *iface;

  g_return_val_if_fail (ATK_IS_TABLE (table), 0);

  iface = ATK_TABLE_GET_IFACE (table);

  if (iface->get_column_at_index)
    return (iface->get_column_at_index) (table, index);
  else
    return -1;
}

/**
 * atk_table_get_caption:
 * @table: a GObject instance that implements AtkTableInterface
 *
 * Gets the caption for the @table.
 *
 * Returns: (nullable) (transfer none): a AtkObject* representing the
 * table caption, or %NULL if value does not implement this interface.
 **/
AtkObject*
atk_table_get_caption (AtkTable *table)
{
  AtkTableIface *iface;

  g_return_val_if_fail (ATK_IS_TABLE (table), NULL);

  iface = ATK_TABLE_GET_IFACE (table);

  if (iface->get_caption)
    return (iface->get_caption) (table);
  else
    return NULL;
}

/**
 * atk_table_get_n_columns:
 * @table: a GObject instance that implements AtkTableIface
 *
 * Gets the number of columns in the table.
 *
 * Returns: a gint representing the number of columns, or 0
 * if value does not implement this interface.
 **/
gint
atk_table_get_n_columns (AtkTable *table)
{
  AtkTableIface *iface;

  g_return_val_if_fail (ATK_IS_TABLE (table), 0);

  iface = ATK_TABLE_GET_IFACE (table);

  if (iface->get_n_columns)
    return (iface->get_n_columns) (table);
  else
    return 0;
}

/**
 * atk_table_get_column_description:
 * @table: a GObject instance that implements AtkTableIface
 * @column: a #gint representing a column in @table
 *
 * Gets the description text of the specified @column in the table
 *
 * Returns: a gchar* representing the column description, or %NULL
 * if value does not implement this interface.
 **/
const gchar*
atk_table_get_column_description (AtkTable *table,
                                  gint     column)
{
  AtkTableIface *iface;

  g_return_val_if_fail (ATK_IS_TABLE (table), NULL);

  iface = ATK_TABLE_GET_IFACE (table);

  if (iface->get_column_description)
    return (iface->get_column_description) (table, column);
  else
    return NULL;
}

/**
 * atk_table_get_column_extent_at:
 * @table: a GObject instance that implements AtkTableIface
 * @row: a #gint representing a row in @table
 * @column: a #gint representing a column in @table
 *
 * Gets the number of columns occupied by the accessible object
 * at the specified @row and @column in the @table.
 *
 * Returns: a gint representing the column extent at specified position, or 0
 * if value does not implement this interface.
 **/
gint
atk_table_get_column_extent_at (AtkTable *table,
                                gint     row,
                                gint     column)
{
  AtkTableIface *iface;

  g_return_val_if_fail (ATK_IS_TABLE (table), 0);

  iface = ATK_TABLE_GET_IFACE (table);

  if (iface->get_column_extent_at)
    return (iface->get_column_extent_at) (table, row, column);
  else
    return 0;
}

/**
 * atk_table_get_column_header:
 * @table: a GObject instance that implements AtkTableIface
 * @column: a #gint representing a column in the table
 *
 * Gets the column header of a specified column in an accessible table.
 *
 * Returns: (nullable) (transfer none): a AtkObject* representing the
 * specified column header, or %NULL if value does not implement this
 * interface.
 **/
AtkObject*
atk_table_get_column_header (AtkTable *table, gint column)
{
  AtkTableIface *iface;

  g_return_val_if_fail (ATK_IS_TABLE (table), NULL);

  iface = ATK_TABLE_GET_IFACE (table);

  if (iface->get_column_header)
    return (iface->get_column_header) (table, column);
  else
    return NULL;
}

/**
 * atk_table_get_n_rows:
 * @table: a GObject instance that implements AtkTableIface
 *
 * Gets the number of rows in the table.
 *
 * Returns: a gint representing the number of rows, or 0
 * if value does not implement this interface.
 **/
gint
atk_table_get_n_rows (AtkTable *table)
{
  AtkTableIface *iface;

  g_return_val_if_fail (ATK_IS_TABLE (table), 0);

  iface = ATK_TABLE_GET_IFACE (table);

  if (iface->get_n_rows)
    return (iface->get_n_rows) (table);
  else
    return 0;
}

/**
 * atk_table_get_row_description:
 * @table: a GObject instance that implements AtkTableIface
 * @row: a #gint representing a row in @table
 *
 * Gets the description text of the specified row in the table
 *
 * Returns: (nullable) a gchar* representing the row description, or
 * %NULL if value does not implement this interface.
 **/
const gchar*
atk_table_get_row_description (AtkTable *table,
                               gint      row)
{
  AtkTableIface *iface;

  g_return_val_if_fail (ATK_IS_TABLE (table), NULL);

  iface = ATK_TABLE_GET_IFACE (table);

  if (iface->get_row_description)
    return (iface->get_row_description) (table, row);
  else
    return NULL;
}

/**
 * atk_table_get_row_extent_at:
 * @table: a GObject instance that implements AtkTableIface
 * @row: a #gint representing a row in @table
 * @column: a #gint representing a column in @table
 *
 * Gets the number of rows occupied by the accessible object
 * at a specified @row and @column in the @table.
 *
 * Returns: a gint representing the row extent at specified position, or 0
 * if value does not implement this interface.
 **/
gint
atk_table_get_row_extent_at (AtkTable *table,
                             gint     row,
                             gint     column)
{
  AtkTableIface *iface;

  g_return_val_if_fail (ATK_IS_TABLE (table), 0);

  iface = ATK_TABLE_GET_IFACE (table);

  if (iface->get_row_extent_at)
    return (iface->get_row_extent_at) (table, row, column);
  else
    return 0;
}

/**
 * atk_table_get_row_header:
 * @table: a GObject instance that implements AtkTableIface
 * @row: a #gint representing a row in the table
 *
 * Gets the row header of a specified row in an accessible table.
 *
 * Returns: (nullable) (transfer none): a AtkObject* representing the
 * specified row header, or %NULL if value does not implement this
 * interface.
 **/
AtkObject*
atk_table_get_row_header (AtkTable *table, gint row)
{
  AtkTableIface *iface;

  g_return_val_if_fail (ATK_IS_TABLE (table), NULL);

  iface = ATK_TABLE_GET_IFACE (table);

  if (iface->get_row_header)
    return (iface->get_row_header) (table, row);
  else
    return NULL;
}

/**
 * atk_table_get_summary:
 * @table: a GObject instance that implements AtkTableIface
 *
 * Gets the summary description of the table.
 *
 * Returns: (transfer full): a AtkObject* representing a summary description
 * of the table, or zero if value does not implement this interface.
 **/
AtkObject*
atk_table_get_summary (AtkTable *table)
{
  AtkTableIface *iface;

  g_return_val_if_fail (ATK_IS_TABLE (table), NULL);

  iface = ATK_TABLE_GET_IFACE (table);

  if (iface->get_summary)
    return (iface->get_summary) (table);
  else
    return NULL;
}

/**
 * atk_table_get_selected_rows:
 * @table: a GObject instance that implements AtkTableIface
 * @selected: a #gint** that is to contain the selected row numbers
 *
 * Gets the selected rows of the table by initializing **selected with 
 * the selected row numbers. This array should be freed by the caller.
 *
 * Returns: a gint representing the number of selected rows,
 * or zero if value does not implement this interface.
 **/
gint
atk_table_get_selected_rows (AtkTable *table, gint **selected)
{
  AtkTableIface *iface;

  g_return_val_if_fail (ATK_IS_TABLE (table), 0);

  iface = ATK_TABLE_GET_IFACE (table);

  if (iface->get_selected_rows)
    return (iface->get_selected_rows) (table, selected);
  else
    return 0;
}

/**
 * atk_table_get_selected_columns:
 * @table: a GObject instance that implements AtkTableIface
 * @selected: a #gint** that is to contain the selected columns numbers
 *
 * Gets the selected columns of the table by initializing **selected with 
 * the selected column numbers. This array should be freed by the caller.
 *
 * Returns: a gint representing the number of selected columns,
 * or %0 if value does not implement this interface.
 **/
gint 
atk_table_get_selected_columns (AtkTable *table, gint **selected)
{
  AtkTableIface *iface;

  g_return_val_if_fail (ATK_IS_TABLE (table), 0);

  iface = ATK_TABLE_GET_IFACE (table);

  if (iface->get_selected_columns)
    return (iface->get_selected_columns) (table, selected);
  else
    return 0;
}

/**
 * atk_table_is_column_selected:
 * @table: a GObject instance that implements AtkTableIface
 * @column: a #gint representing a column in @table
 *
 * Gets a boolean value indicating whether the specified @column
 * is selected
 *
 * Returns: a gboolean representing if the column is selected, or 0
 * if value does not implement this interface.
 **/
gboolean
atk_table_is_column_selected (AtkTable *table,
                              gint     column)
{
  AtkTableIface *iface;

  g_return_val_if_fail (ATK_IS_TABLE (table), FALSE);

  iface = ATK_TABLE_GET_IFACE (table);

  if (iface->is_column_selected)
    return (iface->is_column_selected) (table, column);
  else
    return FALSE;
}

/**
 * atk_table_is_row_selected:
 * @table: a GObject instance that implements AtkTableIface
 * @row: a #gint representing a row in @table
 *
 * Gets a boolean value indicating whether the specified @row
 * is selected
 *
 * Returns: a gboolean representing if the row is selected, or 0
 * if value does not implement this interface.
 **/
gboolean
atk_table_is_row_selected (AtkTable *table,
                           gint     row)
{
  AtkTableIface *iface;

  g_return_val_if_fail (ATK_IS_TABLE (table), FALSE);

  iface = ATK_TABLE_GET_IFACE (table);

  if (iface->is_row_selected)
    return (iface->is_row_selected) (table, row);
  else
    return FALSE;
}

/**
 * atk_table_is_selected:
 * @table: a GObject instance that implements AtkTableIface
 * @row: a #gint representing a row in @table
 * @column: a #gint representing a column in @table
 *
 * Gets a boolean value indicating whether the accessible object
 * at the specified @row and @column is selected
 *
 * Returns: a gboolean representing if the cell is selected, or 0
 * if value does not implement this interface.
 **/
gboolean
atk_table_is_selected (AtkTable *table,
                       gint     row,
                       gint     column)
{
  AtkTableIface *iface;

  g_return_val_if_fail (ATK_IS_TABLE (table), FALSE);

  iface = ATK_TABLE_GET_IFACE (table);

  if (iface->is_selected)
    return (iface->is_selected) (table, row, column);
  else
    return FALSE;
}

/**
 * atk_table_add_row_selection:
 * @table: a GObject instance that implements AtkTableIface
 * @row: a #gint representing a row in @table
 *
 * Adds the specified @row to the selection. 
 *
 * Returns: a gboolean representing if row was successfully added to selection,
 * or 0 if value does not implement this interface.
 **/
gboolean
atk_table_add_row_selection (AtkTable *table,
                       		 gint     row)
{
  AtkTableIface *iface;

  g_return_val_if_fail (ATK_IS_TABLE (table), FALSE);

  iface = ATK_TABLE_GET_IFACE (table);

  if (iface->add_row_selection)
    return (iface->add_row_selection) (table, row);
  else
    return FALSE;
}
/**
 * atk_table_remove_row_selection:
 * @table: a GObject instance that implements AtkTableIface
 * @row: a #gint representing a row in @table
 *
 * Removes the specified @row from the selection. 
 *
 * Returns: a gboolean representing if the row was successfully removed from
 * the selection, or 0 if value does not implement this interface.
 **/
gboolean
atk_table_remove_row_selection (AtkTable *table,
                       		    gint     row)
{
  AtkTableIface *iface;

  g_return_val_if_fail (ATK_IS_TABLE (table), FALSE);

  iface = ATK_TABLE_GET_IFACE (table);

  if (iface->remove_row_selection)
    return (iface->remove_row_selection) (table, row);
  else
    return FALSE;
}
/**
 * atk_table_add_column_selection:
 * @table: a GObject instance that implements AtkTableIface
 * @column: a #gint representing a column in @table
 *
 * Adds the specified @column to the selection. 
 *
 * Returns: a gboolean representing if the column was successfully added to 
 * the selection, or 0 if value does not implement this interface.
 **/
gboolean
atk_table_add_column_selection (AtkTable *table,
                       		    gint     column)
{
  AtkTableIface *iface;

  g_return_val_if_fail (ATK_IS_TABLE (table), FALSE);

  iface = ATK_TABLE_GET_IFACE (table);

  if (iface->add_column_selection)
    return (iface->add_column_selection) (table, column);
  else
    return FALSE;
}
/**
 * atk_table_remove_column_selection:
 * @table: a GObject instance that implements AtkTableIface
 * @column: a #gint representing a column in @table
 *
 * Adds the specified @column to the selection. 
 *
 * Returns: a gboolean representing if the column was successfully removed from
 * the selection, or 0 if value does not implement this interface.
 **/
gboolean
atk_table_remove_column_selection (AtkTable *table,
                       			   gint     column)
{
  AtkTableIface *iface;

  g_return_val_if_fail (ATK_IS_TABLE (table), FALSE);

  iface = ATK_TABLE_GET_IFACE (table);

  if (iface->remove_column_selection)
    return (iface->remove_column_selection) (table, column);
  else
    return FALSE;
}

/**
 * atk_table_set_caption:
 * @table: a GObject instance that implements AtkTableIface
 * @caption: a #AtkObject representing the caption to set for @table
 *
 * Sets the caption for the table.
 **/
void
atk_table_set_caption (AtkTable       *table,
                       AtkObject      *caption)
{
  AtkTableIface *iface;

  g_return_if_fail (ATK_IS_TABLE (table));

  iface = ATK_TABLE_GET_IFACE (table);

  if (iface->set_caption)
    (iface->set_caption) (table, caption);
}

/**
 * atk_table_set_column_description:
 * @table: a GObject instance that implements AtkTableIface
 * @column: a #gint representing a column in @table
 * @description: a #gchar representing the description text
 * to set for the specified @column of the @table
 *
 * Sets the description text for the specified @column of the @table.
 **/
void
atk_table_set_column_description (AtkTable       *table,
                                  gint           column,
                                  const gchar    *description)
{
  AtkTableIface *iface;

  g_return_if_fail (ATK_IS_TABLE (table));

  iface = ATK_TABLE_GET_IFACE (table);

  if (iface->set_column_description)
    (iface->set_column_description) (table, column, description);
}

/**
 * atk_table_set_column_header:
 * @table: a GObject instance that implements AtkTableIface
 * @column: a #gint representing a column in @table
 * @header: an #AtkTable
 *
 * Sets the specified column header to @header.
 **/
void
atk_table_set_column_header (AtkTable  *table,
                             gint      column,
                             AtkObject *header)
{
  AtkTableIface *iface;

  g_return_if_fail (ATK_IS_TABLE (table));

  iface = ATK_TABLE_GET_IFACE (table);

  if (iface->set_column_header)
    (iface->set_column_header) (table, column, header);
}

/**
 * atk_table_set_row_description:
 * @table: a GObject instance that implements AtkTableIface
 * @row: a #gint representing a row in @table
 * @description: a #gchar representing the description text
 * to set for the specified @row of @table
 *
 * Sets the description text for the specified @row of @table.
 **/
void
atk_table_set_row_description (AtkTable       *table,
                               gint           row,
                               const gchar    *description)
{
  AtkTableIface *iface;

  g_return_if_fail (ATK_IS_TABLE (table));

  iface = ATK_TABLE_GET_IFACE (table);

  if (iface->set_row_description)
    (iface->set_row_description) (table, row, description);
}

/**
 * atk_table_set_row_header:
 * @table: a GObject instance that implements AtkTableIface
 * @row: a #gint representing a row in @table
 * @header: an #AtkTable 
 *
 * Sets the specified row header to @header.
 **/
void
atk_table_set_row_header (AtkTable  *table,
                          gint      row,
                          AtkObject *header)
{
  AtkTableIface *iface;

  g_return_if_fail (ATK_IS_TABLE (table));

  iface = ATK_TABLE_GET_IFACE (table);

  if (iface->set_row_header)
    (iface->set_row_header) (table, row, header);
}

/**
 * atk_table_set_summary:
 * @table: a GObject instance that implements AtkTableIface
 * @accessible: an #AtkObject representing the summary description
 * to set for @table
 *
 * Sets the summary description of the table.
 **/
void
atk_table_set_summary (AtkTable       *table,
                       AtkObject      *accessible)
{
  AtkTableIface *iface;

  g_return_if_fail (ATK_IS_TABLE (table));

  iface = ATK_TABLE_GET_IFACE (table);

  if (iface->set_summary)
    (iface->set_summary) (table, accessible);
}
