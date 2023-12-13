/* $Id$ */

/* Copyright(C) 2001-2002 The gtkmm Development Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or(at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <gtkmm/treeview_private.h>
#include <glibmm.h>


namespace Gtk
{

namespace TreeView_Private
{

void SignalProxy_CellData_gtk_callback(GtkTreeViewColumn*, GtkCellRenderer* cell,
                                        GtkTreeModel* model, GtkTreeIter* iter, void* data)
{
  if(!model)
    g_warning("SignalProxy_CellData_gtk_callback(): model is NULL, which is unusual.\n");

  TreeViewColumn::SlotCellData* the_slot = static_cast<TreeViewColumn::SlotCellData*>(data);

  try
  {
    // use Slot::operator()
    Gtk::TreeModel::iterator cppiter = TreeIter(model, iter);
    if(!cppiter->get_model_gobject())
    {
      g_warning("SignalProxy_CellData_gtk_callback() The cppiter has no model\n");
      return; 
    }

    (*the_slot)(Glib::wrap(cell, false), cppiter);
  }
  catch(...)
  {
    Glib::exception_handlers_invoke();
  }
}

void SignalProxy_CellData_gtk_callback_destroy(void* data)
{
  delete static_cast<TreeViewColumn::SlotCellData*>(data);
}


gboolean SignalProxy_RowSeparator_gtk_callback(GtkTreeModel* model, GtkTreeIter* iter, void* data)
{
  TreeView::SlotRowSeparator* the_slot = static_cast<TreeView::SlotRowSeparator*>(data);

  try
  {
    return (*the_slot)(Glib::wrap(model, true), Gtk::TreeIter(model, iter));
  }
  catch(...)
  {
    Glib::exception_handlers_invoke();
  }

  return 0; // arbitrary value
}

void SignalProxy_RowSeparator_gtk_callback_destroy(void* data)
{
  delete static_cast<TreeView::SlotRowSeparator*>(data);
}


} // namespace TreeView_Private

} // namespace Gtk

