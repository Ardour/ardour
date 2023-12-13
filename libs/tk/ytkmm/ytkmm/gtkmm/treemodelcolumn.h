#ifndef _GTKMM_TREEMODELCOLUMN_H
#define _GTKMM_TREEMODELCOLUMN_H

/* Copyright (c) 2002 The gtkmm Development Team
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

#include <gtkmmconfig.h>
#include <glibmm/value.h>
#include <glib-object.h>
#include <vector>

namespace Gtk
{

class TreeModelColumnBase;


/** Typedefed as TreeModel::ColumnRecord.
 * Keeps a record of @link TreeModelColumn TreeModelColumns@endlink.
 * @ingroup TreeView
 * ColumnRecord objects are used to setup a new instance of a TreeModel
 * (or rather, a new instance of an implementation of the model, such as Gtk::ListStore
 * or Gtk::TreeStore).  It is convenient to do that by deriving from
 * TreeModel::ColumnRecord:
 * @code
 * class MyModelColumns : public Gtk::TreeModel::ColumnRecord
 * {
 * public:
 *   Gtk::TreeModelColumn<Glib::ustring>                filename;
 *   Gtk::TreeModelColumn<Glib::ustring>                description;
 *   Gtk::TreeModelColumn< Glib::RefPtr<Gdk::Pixbuf> >  thumbnail;
 *
 *   MyModelColumns() { add(filename); add(description); add(thumbnail); }
 * };
 * @endcode
 *
 * Whether or not you derive your own ColumnRecord, you need to add the
 * @link TreeModelColumn TreeModelColumns@endlink to the ColumnRecord with the
 * add() method.
 *
 * A ColumnRecord instance, such as an instance of @c MyModelColumns should then
 * be passed to ListStore::create() or TreeStore::create().
 * The @link TreeModelColumn TreeModelColumns@endlink, such as the members
 * @c filename, @c description and @c thumbnail can then be used with Gtk::TreeRow::operator[]()
 * to specify the column you're interested in.
 *
 * Neither TreeModel::ColumnRecord nor the
 * @link TreeModelColumn TreeModelColumns@endlink contain any real data --
 * they merely describe what C++ type is stored in which column
 * of a TreeModel, and save you from having to repeat that type information in several places.
 *
 * Thus TreeModel::ColumnRecord can be made a singleton (as long as you make
 * sure it's instantiated after Gtk::Main), even when creating multiple models
 * from it.
 */
class TreeModelColumnRecord
{
public:
  TreeModelColumnRecord();
  virtual ~TreeModelColumnRecord();

  /** Adds a TreeModelColumn to this record.
   * add() not only registers the @a column, but also assigns a column
   * index to it.  Once registered, the TreeModelColumn is final, and
   * you're free to pass it around by value.
   */
  void add(TreeModelColumnBase& column);
  
  unsigned int size()  const;
  const GType* types() const;

private:
  std::vector<GType> column_types_;

  // noncopyable
  TreeModelColumnRecord(const TreeModelColumnRecord&);
  TreeModelColumnRecord& operator=(const TreeModelColumnRecord&);
};


/** Base class of TreeModelColumn templates.
 * @ingroup TreeView
 */
class TreeModelColumnBase
{
public:
  GType type()  const { return type_;  }
  int index() const { return index_; }

protected:
  explicit TreeModelColumnBase(GType type);

private:
  GType type_;
  int   index_;

  friend class Gtk::TreeModelColumnRecord;
};

/** @relates Gtk::TreeModelColumnBase */
inline bool operator==(const TreeModelColumnBase& lhs, const TreeModelColumnBase& rhs)
  { return (lhs.index() == rhs.index()); }

/** @relates Gtk::TreeModelColumnBase */
inline bool operator!=(const TreeModelColumnBase& lhs, const TreeModelColumnBase& rhs)
  { return (lhs.index() != rhs.index()); }


/** A Gtk::TreeModelColumn describes the C++ type of the data in a model column, and identifies that column in the model.
 * See @link TreeModelColumnRecord Gtk::TreeModel::Columns@endlink for a usage example.
 * @ingroup TreeView
 */
template <class T>
class TreeModelColumn : public TreeModelColumnBase
{
public:
  typedef T               ElementType;
  typedef Glib::Value<T>  ValueType;

  TreeModelColumn() : TreeModelColumnBase(ValueType::value_type()) {}
};

} // namespace Gtk


#endif /* _GTKMM_TREEMODELCOLUMN_H */

