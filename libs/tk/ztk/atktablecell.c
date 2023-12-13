/* ATK -  Accessibility Toolkit
 * Copyright 2014 SUSE LLC.
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

#include "atktablecell.h"


/**
 * SECTION:atktablecell
 * @Short_description: The ATK interface implemented for a cell inside
 * a two-dimentional #AtkTable
 * @Title:AtkTableCell
 *
 * Being #AtkTable a component which present elements ordered via rows
 * and columns, an #AtkTableCell is the interface which each of those
 * elements, so "cells" should implement.
 *
 * See also #AtkTable.
 */

typedef AtkTableCellIface AtkTableCellInterface;
G_DEFINE_INTERFACE (AtkTableCell, atk_table_cell, ATK_TYPE_OBJECT)

static gboolean atk_table_cell_real_get_row_column_span (AtkTableCell *cell,
                                                         gint         *row,
                                                         gint         *column,
                                                         gint         *row_span,
                                                         gint         *column_span);

static void
atk_table_cell_default_init (AtkTableCellInterface *iface)
{
  iface->get_row_column_span = atk_table_cell_real_get_row_column_span;
}

/**
 * atk_table_cell_get_column_span:
 * @cell: a GObject instance that implements AtkTableCellIface
 *
 * Returns the number of columns occupied by this cell accessible.
 *
 * Returns: a gint representing the number of columns occupied by this cell,
 * or 0 if the cell does not implement this method.
 *
 * Since: 2.12
 */
gint
atk_table_cell_get_column_span (AtkTableCell *cell)
{
  AtkTableCellIface *iface;

  g_return_val_if_fail (ATK_IS_TABLE_CELL (cell), 0);

  iface = ATK_TABLE_CELL_GET_IFACE (cell);

  if (iface->get_column_span)
    return (iface->get_column_span) (cell);
  else
    return 0;
}

/**
 * atk_table_cell_get_column_header_cells:
 * @cell: a GObject instance that implements AtkTableCellIface
 *
 * Returns the column headers as an array of cell accessibles.
 *
 * Returns: (element-type AtkObject) (transfer full): a GPtrArray of AtkObjects
 * representing the column header cells.
 *
 * Since: 2.12
 */
GPtrArray *
atk_table_cell_get_column_header_cells (AtkTableCell *cell)
{
  AtkTableCellIface *iface;

  g_return_val_if_fail (ATK_IS_TABLE_CELL (cell), NULL);

  iface = ATK_TABLE_CELL_GET_IFACE (cell);

  if (iface->get_column_header_cells)
    return (iface->get_column_header_cells) (cell);
  else
    return NULL;
}

/**
 * atk_table_cell_get_position:
 * @cell: a GObject instance that implements AtkTableCellIface
 * @row: (out): the row of the given cell.
 * @column: (out): the column of the given cell.
 *
 * Retrieves the tabular position of this cell.
 *
 * Returns: TRUE if successful; FALSE otherwise.
 *
 * Since: 2.12
 */
gboolean
atk_table_cell_get_position (AtkTableCell *cell,
                             gint         *row,
                             gint         *column)
{
  AtkTableCellIface *iface;
  gint tmp_row, tmp_column;
  gint *real_row = (row ? row : &tmp_row);
  gint *real_column = (column ? column : &tmp_column);

  *real_row = -1;
  *real_column = -1;

  g_return_val_if_fail (ATK_IS_TABLE_CELL (cell), FALSE);

  iface = ATK_TABLE_CELL_GET_IFACE (cell);

  if (iface->get_position)
    return (iface->get_position) (cell, real_row, real_column);
  else
    return FALSE;
}

/**
 * atk_table_cell_get_row_span:
 * @cell: a GObject instance that implements AtkTableCellIface
 *
 * Returns the number of rows occupied by this cell accessible.
 *
 * Returns: a gint representing the number of rows occupied by this cell,
 * or 0 if the cell does not implement this method.
 *
 * Since: 2.12
 */
gint
atk_table_cell_get_row_span (AtkTableCell *cell)
{
  AtkTableCellIface *iface;

  g_return_val_if_fail (ATK_IS_TABLE_CELL (cell), 0);

  iface = ATK_TABLE_CELL_GET_IFACE (cell);

  if (iface->get_row_span)
    return (iface->get_row_span) (cell);
  else
    return 0;
}

/**
 * atk_table_cell_get_row_header_cells:
 * @cell: a GObject instance that implements AtkTableCellIface
 *
 * Returns the row headers as an array of cell accessibles.
 *
 * Returns: (element-type AtkObject) (transfer full): a GPtrArray of AtkObjects
 * representing the row header cells.
 *
 * Since: 2.12
 */
GPtrArray *
atk_table_cell_get_row_header_cells (AtkTableCell *cell)
{
  AtkTableCellIface *iface;

  g_return_val_if_fail (ATK_IS_TABLE_CELL (cell), NULL);

  iface = ATK_TABLE_CELL_GET_IFACE (cell);

  if (iface->get_row_header_cells)
    return (iface->get_row_header_cells) (cell);
  else
    return NULL;
}

/**
 * atk_table_cell_get_row_column_span:
 * @cell: a GObject instance that implements AtkTableCellIface
 * @row: (out): the row index of the given cell.
 * @column: (out): the column index of the given cell.
 * @row_span: (out): the number of rows occupied by this cell.
 * @column_span: (out): the number of columns occupied by this cell.
 *
 * Gets the row and column indexes and span of this cell accessible.
 *
 * Note: If the object does not implement this function, then, by default, atk
 * will implement this function by calling get_row_span and get_column_span
 * on the object.
 *
 * Returns: TRUE if successful; FALSE otherwise.
 *
 * Since: 2.12
 */
gboolean
atk_table_cell_get_row_column_span (AtkTableCell *cell,
                                    gint         *row,
                                    gint         *column,
                                    gint         *row_span,
                                    gint         *column_span)
{
  AtkTableCellIface *iface;
  gint local_row = 0, local_column = 0;
  gint local_row_span = 0, local_column_span = 0;
  gint *real_row, *real_column;
  gint *real_row_span, *real_column_span;

  g_return_val_if_fail (ATK_IS_TABLE_CELL (cell), FALSE);

  real_row = (row ? row : &local_row);
  real_column = (column ? column : &local_column);
  real_row_span = (row_span ? row_span : &local_row_span);
  real_column_span = (column_span ? column_span : &local_column_span);

  iface = ATK_TABLE_CELL_GET_IFACE (cell);

  if (iface->get_row_column_span)
    return (iface->get_row_column_span) (cell, real_row, real_column,
                                           real_row_span,
                                           real_column_span);
  else
    return FALSE;
}

/**
 * atk_table_cell_get_table:
 * @cell: a GObject instance that implements AtkTableCellIface
 *
 * Returns a reference to the accessible of the containing table.
 *
 * Returns: (transfer full): the atk object for the containing table.
 *
 * Since: 2.12
 */
AtkObject *
atk_table_cell_get_table (AtkTableCell *cell)
{
  AtkTableCellIface *iface;

  g_return_val_if_fail (ATK_IS_TABLE_CELL (cell), FALSE);

  iface = ATK_TABLE_CELL_GET_IFACE (cell);

  if (iface->get_table)
    return (iface->get_table) (cell);
  else
    return NULL;
}

static gboolean
atk_table_cell_real_get_row_column_span (AtkTableCell *cell,
                                         gint         *row,
                                         gint         *column,
                                         gint         *row_span,
                                         gint         *column_span)
{
  atk_table_cell_get_position (cell, row, column);
  *row_span = atk_table_cell_get_row_span (cell);
  *column_span = atk_table_cell_get_column_span (cell);
  return (row != 0 && column != 0 && row_span > 0 && column_span > 0);
}
