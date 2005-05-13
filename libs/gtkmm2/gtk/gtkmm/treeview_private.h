#ifndef _GTKMM_TREEVIEW_PRIVATE_H
#define _GTKMM_TREEVIEW_PRIVATE_H
/* $Id$ */


/* treeview.h
 *
 * Copyright(C) 2001-2002 The gtkmm Development Team
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

#include <gtkmm/treeviewcolumn.h>
#include <gtkmm/treeview.h>

namespace Gtk
{

#ifndef DOXYGEN_SHOULD_SKIP_THIS
namespace TreeView_Private
{

//This Signal Proxy allows the C++ coder to specify a sigc::slot instead of a static function.
class SignalProxy_CellData
{
public:
  typedef TreeViewColumn::SlotCellData SlotType;

  SignalProxy_CellData(const SlotType& slot);
  ~SignalProxy_CellData();

  static void gtk_callback(GtkTreeViewColumn*, GtkCellRenderer* cell,
                           GtkTreeModel* model, GtkTreeIter* iter, void* data);
  static void gtk_callback_destroy(void* data);

protected:
  SlotType slot_;
};

//SignalProxy_RowSeparator:

//This Signal Proxy allows the C++ coder to specify a sigc::slot instead of a static function.
class SignalProxy_RowSeparator
{
public:
  typedef TreeView::SlotRowSeparator SlotType;

  SignalProxy_RowSeparator(const SlotType& slot);
  ~SignalProxy_RowSeparator();

  static gboolean gtk_callback(GtkTreeModel* model, GtkTreeIter* iter, void* data);
  static void gtk_callback_destroy(void* data);

protected:
  SlotType slot_;
};


} /* namespace TreeView_Private */
#endif //DOXYGEN_SHOULD_SKIP_THIS

} /* namespace Gtk */


#endif /* _GTKMM_TREEVIEW_PRIVATE_H */
