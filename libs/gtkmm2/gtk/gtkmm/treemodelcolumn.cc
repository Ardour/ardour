/* $Id$ */

/* Copyright (c) 2002 The gtkmm Development Team
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

#include <gtkmm/treemodelcolumn.h>


namespace Gtk
{

/**** Gtk::TreeModelColumnRecord *******************************************/

TreeModelColumnRecord::TreeModelColumnRecord()
:
  column_types_ ()
{}

TreeModelColumnRecord::~TreeModelColumnRecord()
{}

void TreeModelColumnRecord::add(TreeModelColumnBase& column)
{
  g_return_if_fail(column.index_ == -1); //Check that it hasn't been set before.

  column.index_ = column_types_.size();
  column_types_.push_back(column.type_);
}

unsigned int TreeModelColumnRecord::size() const
{
  return column_types_.size();
}

const GType* TreeModelColumnRecord::types() const
{
  g_return_val_if_fail(!column_types_.empty(), 0);

  // According to Josuttis' book, &vector.front() to get a builtin array is
  // quasi-standard. It should work fine with any std::vector implementation.
  return &column_types_.front();
}


/**** Gtk::TreeModelColumnBase *********************************************/

TreeModelColumnBase::TreeModelColumnBase(GType type)
:
  type_  (type),
  index_ (-1) //default to an invalid index.
{}

} // namespace Gtk

