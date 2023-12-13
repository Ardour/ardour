/* gtkcelleditable.c
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
#include "gtkcelleditable.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "gtkalias.h"

typedef GtkCellEditableIface GtkCellEditableInterface;
G_DEFINE_INTERFACE(GtkCellEditable, gtk_cell_editable, GTK_TYPE_WIDGET)

static void
gtk_cell_editable_default_init (GtkCellEditableInterface *iface)
{
  /**
   * GtkCellEditable:editing-canceled:
   *
   * Indicates whether editing on the cell has been canceled.
   *
   * Since: 2.20
   */
  g_object_interface_install_property (iface,
                                       g_param_spec_boolean ("editing-canceled",
                                       P_("Editing Canceled"),
                                       P_("Indicates that editing has been canceled"),
                                       FALSE,
                                       GTK_PARAM_READWRITE));

  /**
   * GtkCellEditable::editing-done:
   * @cell_editable: the object on which the signal was emitted
   *
   * This signal is a sign for the cell renderer to update its
   * value from the @cell_editable.
   *
   * Implementations of #GtkCellEditable are responsible for
   * emitting this signal when they are done editing, e.g.
   * #GtkEntry is emitting it when the user presses Enter.
   *
   * gtk_cell_editable_editing_done() is a convenience method
   * for emitting GtkCellEditable::editing-done.
   */
  g_signal_new (I_("editing-done"),
                GTK_TYPE_CELL_EDITABLE,
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET (GtkCellEditableIface, editing_done),
                NULL, NULL,
                _gtk_marshal_VOID__VOID,
                G_TYPE_NONE, 0);

  /**
   * GtkCellEditable::remove-widget:
   * @cell_editable: the object on which the signal was emitted
   *
   * This signal is meant to indicate that the cell is finished
   * editing, and the widget may now be destroyed.
   *
   * Implementations of #GtkCellEditable are responsible for
   * emitting this signal when they are done editing. It must
   * be emitted after the #GtkCellEditable::editing-done signal,
   * to give the cell renderer a chance to update the cell's value
   * before the widget is removed.
   *
   * gtk_cell_editable_remove_widget() is a convenience method
   * for emitting GtkCellEditable::remove-widget.
   */
  g_signal_new (I_("remove-widget"),
                GTK_TYPE_CELL_EDITABLE,
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET (GtkCellEditableIface, remove_widget),
                NULL, NULL,
                _gtk_marshal_VOID__VOID,
                G_TYPE_NONE, 0);
}

/**
 * gtk_cell_editable_start_editing:
 * @cell_editable: A #GtkCellEditable
 * @event: (allow-none): A #GdkEvent, or %NULL
 * 
 * Begins editing on a @cell_editable. @event is the #GdkEvent that began 
 * the editing process. It may be %NULL, in the instance that editing was 
 * initiated through programatic means.
 **/
void
gtk_cell_editable_start_editing (GtkCellEditable *cell_editable,
				 GdkEvent        *event)
{
  g_return_if_fail (GTK_IS_CELL_EDITABLE (cell_editable));

  (* GTK_CELL_EDITABLE_GET_IFACE (cell_editable)->start_editing) (cell_editable, event);
}

/**
 * gtk_cell_editable_editing_done:
 * @cell_editable: A #GtkTreeEditable
 * 
 * Emits the #GtkCellEditable::editing-done signal. 
 **/
void
gtk_cell_editable_editing_done (GtkCellEditable *cell_editable)
{
  g_return_if_fail (GTK_IS_CELL_EDITABLE (cell_editable));

  g_signal_emit_by_name (cell_editable, "editing-done");
}

/**
 * gtk_cell_editable_remove_widget:
 * @cell_editable: A #GtkTreeEditable
 * 
 * Emits the #GtkCellEditable::remove-widget signal.  
 **/
void
gtk_cell_editable_remove_widget (GtkCellEditable *cell_editable)
{
  g_return_if_fail (GTK_IS_CELL_EDITABLE (cell_editable));

  g_signal_emit_by_name (cell_editable, "remove-widget");
}

#define __GTK_CELL_EDITABLE_C__
#include "gtkaliasdef.c"
