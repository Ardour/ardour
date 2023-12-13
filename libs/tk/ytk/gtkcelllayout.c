/* gtkcelllayout.c
 * Copyright (C) 2003  Kristian Rietveld  <kris@gtk.org>
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
#include <stdlib.h>
#include <errno.h>
#include "gtkcelllayout.h"
#include "gtkintl.h"
#include "gtkalias.h"

GType
gtk_cell_layout_get_type (void)
{
  static GType cell_layout_type = 0;

  if (! cell_layout_type)
    {
      const GTypeInfo cell_layout_info =
      {
        sizeof (GtkCellLayoutIface),
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        0,
        0,
        NULL
      };

      cell_layout_type =
        g_type_register_static (G_TYPE_INTERFACE, I_("GtkCellLayout"),
                                &cell_layout_info, 0);

      g_type_interface_add_prerequisite (cell_layout_type, G_TYPE_OBJECT);
    }

  return cell_layout_type;
}

/**
 * gtk_cell_layout_pack_start:
 * @cell_layout: A #GtkCellLayout.
 * @cell: A #GtkCellRenderer.
 * @expand: %TRUE if @cell is to be given extra space allocated to @cell_layout.
 *
 * Packs the @cell into the beginning of @cell_layout. If @expand is %FALSE,
 * then the @cell is allocated no more space than it needs. Any unused space
 * is divided evenly between cells for which @expand is %TRUE.
 *
 * Note that reusing the same cell renderer is not supported. 
 *
 * Since: 2.4
 */
void
gtk_cell_layout_pack_start (GtkCellLayout   *cell_layout,
                            GtkCellRenderer *cell,
                            gboolean         expand)
{
  g_return_if_fail (GTK_IS_CELL_LAYOUT (cell_layout));
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));

  (* GTK_CELL_LAYOUT_GET_IFACE (cell_layout)->pack_start) (cell_layout,
                                                           cell,
                                                           expand);
}

/**
 * gtk_cell_layout_pack_end:
 * @cell_layout: A #GtkCellLayout.
 * @cell: A #GtkCellRenderer.
 * @expand: %TRUE if @cell is to be given extra space allocated to @cell_layout.
 *
 * Adds the @cell to the end of @cell_layout. If @expand is %FALSE, then the
 * @cell is allocated no more space than it needs. Any unused space is
 * divided evenly between cells for which @expand is %TRUE.
 *
 * Note that reusing the same cell renderer is not supported. 
 *
 * Since: 2.4
 */
void
gtk_cell_layout_pack_end (GtkCellLayout   *cell_layout,
                          GtkCellRenderer *cell,
                          gboolean         expand)
{
  g_return_if_fail (GTK_IS_CELL_LAYOUT (cell_layout));
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));

  (* GTK_CELL_LAYOUT_GET_IFACE (cell_layout)->pack_end) (cell_layout,
                                                         cell,
                                                         expand);
}

/**
 * gtk_cell_layout_clear:
 * @cell_layout: A #GtkCellLayout.
 *
 * Unsets all the mappings on all renderers on @cell_layout and
 * removes all renderers from @cell_layout.
 *
 * Since: 2.4
 */
void
gtk_cell_layout_clear (GtkCellLayout *cell_layout)
{
  g_return_if_fail (GTK_IS_CELL_LAYOUT (cell_layout));

  (* GTK_CELL_LAYOUT_GET_IFACE (cell_layout)->clear) (cell_layout);
}

static void
gtk_cell_layout_set_attributesv (GtkCellLayout   *cell_layout,
                                 GtkCellRenderer *cell,
                                 va_list          args)
{
  gchar *attribute;
  gint column;
  GtkCellLayoutIface *iface;

  attribute = va_arg (args, gchar *);

  iface = GTK_CELL_LAYOUT_GET_IFACE (cell_layout);

  (* iface->clear_attributes) (cell_layout, cell);

  while (attribute != NULL)
    {
      column = va_arg (args, gint);
      (* iface->add_attribute) (cell_layout, cell, attribute, column);
      attribute = va_arg (args, gchar *);
    }
}

/**
 * gtk_cell_layout_set_attributes:
 * @cell_layout: A #GtkCellLayout.
 * @cell: A #GtkCellRenderer.
 * @Varargs: A %NULL-terminated list of attributes.
 *
 * Sets the attributes in list as the attributes of @cell_layout. The
 * attributes should be in attribute/column order, as in
 * gtk_cell_layout_add_attribute(). All existing attributes are removed, and
 * replaced with the new attributes.
 *
 * Since: 2.4
 */
void
gtk_cell_layout_set_attributes (GtkCellLayout   *cell_layout,
                                GtkCellRenderer *cell,
                                ...)
{
  va_list args;

  g_return_if_fail (GTK_IS_CELL_LAYOUT (cell_layout));
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));

  va_start (args, cell);
  gtk_cell_layout_set_attributesv (cell_layout, cell, args);
  va_end (args);
}

/**
 * gtk_cell_layout_add_attribute:
 * @cell_layout: A #GtkCellLayout.
 * @cell: A #GtkCellRenderer.
 * @attribute: An attribute on the renderer.
 * @column: The column position on the model to get the attribute from.
 *
 * Adds an attribute mapping to the list in @cell_layout. The @column is the
 * column of the model to get a value from, and the @attribute is the
 * parameter on @cell to be set from the value. So for example if column 2
 * of the model contains strings, you could have the "text" attribute of a
 * #GtkCellRendererText get its values from column 2.
 *
 * Since: 2.4
 */
void
gtk_cell_layout_add_attribute (GtkCellLayout   *cell_layout,
                               GtkCellRenderer *cell,
                               const gchar     *attribute,
                               gint             column)
{
  g_return_if_fail (GTK_IS_CELL_LAYOUT (cell_layout));
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));
  g_return_if_fail (attribute != NULL);
  g_return_if_fail (column >= 0);

  (* GTK_CELL_LAYOUT_GET_IFACE (cell_layout)->add_attribute) (cell_layout,
                                                              cell,
                                                              attribute,
                                                              column);
}

/**
 * gtk_cell_layout_set_cell_data_func:
 * @cell_layout: A #GtkCellLayout.
 * @cell: A #GtkCellRenderer.
 * @func: The #GtkCellLayoutDataFunc to use.
 * @func_data: The user data for @func.
 * @destroy: The destroy notification for @func_data.
 *
 * Sets the #GtkCellLayoutDataFunc to use for @cell_layout. This function
 * is used instead of the standard attributes mapping for setting the
 * column value, and should set the value of @cell_layout's cell renderer(s)
 * as appropriate. @func may be %NULL to remove and older one.
 *
 * Since: 2.4
 */
void
gtk_cell_layout_set_cell_data_func (GtkCellLayout         *cell_layout,
                                    GtkCellRenderer       *cell,
                                    GtkCellLayoutDataFunc  func,
                                    gpointer               func_data,
                                    GDestroyNotify         destroy)
{
  g_return_if_fail (GTK_IS_CELL_LAYOUT (cell_layout));
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));

  (* GTK_CELL_LAYOUT_GET_IFACE (cell_layout)->set_cell_data_func) (cell_layout,
                                                                   cell,
                                                                   func,
                                                                   func_data,
                                                                   destroy);
}

/**
 * gtk_cell_layout_clear_attributes:
 * @cell_layout: A #GtkCellLayout.
 * @cell: A #GtkCellRenderer to clear the attribute mapping on.
 *
 * Clears all existing attributes previously set with
 * gtk_cell_layout_set_attributes().
 *
 * Since: 2.4
 */
void
gtk_cell_layout_clear_attributes (GtkCellLayout   *cell_layout,
                                  GtkCellRenderer *cell)
{
  g_return_if_fail (GTK_IS_CELL_LAYOUT (cell_layout));
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));

  (* GTK_CELL_LAYOUT_GET_IFACE (cell_layout)->clear_attributes) (cell_layout,
                                                                 cell);
}

/**
 * gtk_cell_layout_reorder:
 * @cell_layout: A #GtkCellLayout.
 * @cell: A #GtkCellRenderer to reorder.
 * @position: New position to insert @cell at.
 *
 * Re-inserts @cell at @position. Note that @cell has already to be packed
 * into @cell_layout for this to function properly.
 *
 * Since: 2.4
 */
void
gtk_cell_layout_reorder (GtkCellLayout   *cell_layout,
                         GtkCellRenderer *cell,
                         gint             position)
{
  g_return_if_fail (GTK_IS_CELL_LAYOUT (cell_layout));
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));

  (* GTK_CELL_LAYOUT_GET_IFACE (cell_layout)->reorder) (cell_layout,
                                                        cell,
                                                        position);
}

/**
 * gtk_cell_layout_get_cells:
 * @cell_layout: a #GtkCellLayout
 * 
 * Returns the cell renderers which have been added to @cell_layout.
 *
 * Return value: (element-type GtkCellRenderer) (transfer container): a list of cell renderers. The list, but not the
 *   renderers has been newly allocated and should be freed with
 *   g_list_free() when no longer needed.
 *
 * Since: 2.12
 */
GList *
gtk_cell_layout_get_cells (GtkCellLayout *cell_layout)
{
  GtkCellLayoutIface *iface;

  g_return_val_if_fail (GTK_IS_CELL_LAYOUT (cell_layout), NULL);

  iface = GTK_CELL_LAYOUT_GET_IFACE (cell_layout);  
  if (iface->get_cells)
    return iface->get_cells (cell_layout);

  return NULL;
}

typedef struct {
  GtkCellLayout   *cell_layout;
  GtkCellRenderer *renderer;
  gchar           *attr_name;
} AttributesSubParserData;

static void
attributes_start_element (GMarkupParseContext *context,
			  const gchar         *element_name,
			  const gchar        **names,
			  const gchar        **values,
			  gpointer             user_data,
			  GError             **error)
{
  AttributesSubParserData *parser_data = (AttributesSubParserData*)user_data;
  guint i;

  if (strcmp (element_name, "attribute") == 0)
    {
      for (i = 0; names[i]; i++)
	if (strcmp (names[i], "name") == 0)
	  parser_data->attr_name = g_strdup (values[i]);
    }
  else if (strcmp (element_name, "attributes") == 0)
    return;
  else
    g_warning ("Unsupported tag for GtkCellLayout: %s\n", element_name);
}

static void
attributes_text_element (GMarkupParseContext *context,
			 const gchar         *text,
			 gsize                text_len,
			 gpointer             user_data,
			 GError             **error)
{
  AttributesSubParserData *parser_data = (AttributesSubParserData*)user_data;
  glong l;
  gchar *endptr;
  gchar *string;
  
  if (!parser_data->attr_name)
    return;

  errno = 0;
  string = g_strndup (text, text_len);
  l = strtol (string, &endptr, 0);
  if (errno || endptr == string)
    {
      g_set_error (error, 
                   GTK_BUILDER_ERROR,
                   GTK_BUILDER_ERROR_INVALID_VALUE,
                   "Could not parse integer `%s'",
                   string);
      g_free (string);
      return;
    }
  g_free (string);

  gtk_cell_layout_add_attribute (parser_data->cell_layout,
				 parser_data->renderer,
				 parser_data->attr_name, l);
  g_free (parser_data->attr_name);
  parser_data->attr_name = NULL;
}

static const GMarkupParser attributes_parser =
  {
    attributes_start_element,
    NULL,
    attributes_text_element,
  };

gboolean
_gtk_cell_layout_buildable_custom_tag_start (GtkBuildable  *buildable,
					     GtkBuilder    *builder,
					     GObject       *child,
					     const gchar   *tagname,
					     GMarkupParser *parser,
					     gpointer      *data)
{
  AttributesSubParserData *parser_data;

  if (!child)
    return FALSE;

  if (strcmp (tagname, "attributes") == 0)
    {
      parser_data = g_slice_new0 (AttributesSubParserData);
      parser_data->cell_layout = GTK_CELL_LAYOUT (buildable);
      parser_data->renderer = GTK_CELL_RENDERER (child);
      parser_data->attr_name = NULL;

      *parser = attributes_parser;
      *data = parser_data;
      return TRUE;
    }

  return FALSE;
}

void
_gtk_cell_layout_buildable_custom_tag_end (GtkBuildable *buildable,
					   GtkBuilder   *builder,
					   GObject      *child,
					   const gchar  *tagname,
					   gpointer     *data)
{
  AttributesSubParserData *parser_data;

  parser_data = (AttributesSubParserData*)data;
  g_assert (!parser_data->attr_name);
  g_slice_free (AttributesSubParserData, parser_data);
}

void
_gtk_cell_layout_buildable_add_child (GtkBuildable      *buildable,
				      GtkBuilder        *builder,
				      GObject           *child,
				      const gchar       *type)
{
  GtkCellLayoutIface *iface;
  
  g_return_if_fail (GTK_IS_CELL_LAYOUT (buildable));
  g_return_if_fail (GTK_IS_CELL_RENDERER (child));

  iface = GTK_CELL_LAYOUT_GET_IFACE (buildable);
  g_return_if_fail (iface->pack_start != NULL);
  iface->pack_start (GTK_CELL_LAYOUT (buildable), GTK_CELL_RENDERER (child), FALSE);
}

#define __GTK_CELL_LAYOUT_C__
#include "gtkaliasdef.c"
