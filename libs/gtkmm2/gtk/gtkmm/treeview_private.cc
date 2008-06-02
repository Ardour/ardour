/* $Id$ */

/* Copyright(C) 2001-2002 The gtkmm Development Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or(at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
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
  TreeViewColumn::SlotCellData* the_slot = static_cast<TreeViewColumn::SlotCellData*>(data);

  #ifdef GLIBMM_EXCEPTIONS_ENABLED
  try
  {
  #endif //GLIBMM_EXCEPTIONS_ENABLE
    // use Slot::operator()
    (*the_slot)(Glib::wrap(cell, false), TreeIter(model, iter));
  #ifdef GLIBMM_EXCEPTIONS_ENABLED
  }
  catch(...)
  {
    Glib::exception_handlers_invoke();
  }
  #endif //GLIBMM_EXCEPTIONS_ENABLE
}

void SignalProxy_CellData_gtk_callback_destroy(void* data)
{
  delete static_cast<TreeViewColumn::SlotCellData*>(data);
}


gboolean SignalProxy_RowSeparator_gtk_callback(GtkTreeModel* model, GtkTreeIter* iter, void* data)
{
  TreeView::SlotRowSeparator* the_slot = static_cast<TreeView::SlotRowSeparator*>(data);

  #ifdef GLIBMM_EXCEPTIONS_ENABLED
  try
  {
  #endif //GLIBMM_EXCEPTIONS_ENABLE
    return (*the_slot)(Glib::wrap(model, true), Gtk::TreeIter(model, iter));
  #ifdef GLIBMM_EXCEPTIONS_ENABLED
  }
  catch(...)
  {
    Glib::exception_handlers_invoke();
  }

  return 0; // arbitrary value
  #endif //GLIBMM_EXCEPTIONS_ENABLE
}

void SignalProxy_RowSeparator_gtk_callback_destroy(void* data)
{
  delete static_cast<TreeView::SlotRowSeparator*>(data);
}


} // namespace TreeView_Private

} // namespace Gtk

