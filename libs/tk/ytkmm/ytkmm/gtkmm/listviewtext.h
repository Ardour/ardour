/* Copyright(C) 2006 The gtkmm Development Team
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

#ifndef _GTKMM_LISTVIEW_TEXT_H
#define _GTKMM_LISTVIEW_TEXT_H

#include <gtkmm/treeview.h>
#include <gtkmm/liststore.h>

#include <vector>

namespace Gtk
{

/** A simple listbox which presents some lines of information in columns and lets the user select some of them.
 *
 * This is a convenience class, based on Gtk::TreeView, which allows only text values and does not allow child items.
 * In most cases you will actually need the functionality offered by a real Gtk::TreeView with your own type-safe 
 * Gtk::TreeModel::ColumnRecord.
 *
 * @ingroup Widgets
 * @ingroup Containers
 * @ingroup TreeView
 *
 * @newin{2,10}
 */
class ListViewText : public Gtk::TreeView
{
public:

  ListViewText(guint columns_count, bool editable = false, Gtk::SelectionMode mode = Gtk::SELECTION_SINGLE);
  virtual ~ListViewText();

  /** Adds a title to column @a column.
   * @param column the column number.
   * @param title the title for column @a column.
   */
  void set_column_title(guint column, const Glib::ustring& title);

  /** Gets the title of column @a column.
   * @param column the column number.
   * @return the title of column @a column.
   */
  Glib::ustring get_column_title(guint column) const;

#ifndef GTKMM_DISABLE_DEPRECATED
  /** Add a new row at the end of the list
   * @param column_one_value the new text for the new row, column 0
   * @return the number of the row added
   *
   * @deprecated Use append().
   */
  guint append_text(const Glib::ustring& column_one_value = Glib::ustring());

  /** Insert a new row at the beginning of the list
   * @param column_one_value the new text for the new row, column 0
   *
   * @deprecated Use prepend().
   */
  void prepend_text(const Glib::ustring& column_one_value = Glib::ustring());

  /** Insert a new row at an arbitrary position in the list
   * @param row The row number
   * @param column_one_value the new text for the new row, column 0
   *
   * @deprecated Use insert().
   */
  void insert_text(guint row, const Glib::ustring& column_one_value = Glib::ustring());
#endif //GTKMM_DISABLE_DEPRECATED  
  
  /** Add a new row at the end of the list
   * @param column_one_value the new text for the new row, column 0
   * @return the number of the row added
   */
  guint append(const Glib::ustring& column_one_value = Glib::ustring());

  /** Insert a new row at the beginning of the list
   * @param column_one_value the new text for the new row, column 0
   */
  void prepend(const Glib::ustring& column_one_value = Glib::ustring());

  /** Insert a new row at an arbitrary position in the list
   * @param row The row number
   * @param column_one_value the new text for the new row, column 0
   */
  void insert(guint row, const Glib::ustring& column_one_value = Glib::ustring());

  /// Discard all row:
  void clear_items();

  /** Obtain the value of an existing cell from the list.
   * @param row the number of the row in the listbox.
   * @param column the number of the column in the row.
   * @return the value of that cell, if it exists.
   */
  Glib::ustring get_text(guint row, guint column = 0) const;

  /** Change an existing value of cell of the list.
   * @param row the number of the row in the list.
   * @param column the number of the column in the row.	 
   * @param value the new contents of that row and column.
   */
  void set_text(guint row, guint column, const Glib::ustring& value);

  /** Change an existing value of a column 0 of a row of the list
   * @param row the number of the row in the list.
   * @param value the new contents of column 0 of the row.
   */
  void set_text(guint row, const Glib::ustring& value);

  /// @return the number of rows in the listbox
  guint size() const;

  /// @return the number of columns in the listbox
  guint get_num_columns() const;

  typedef std::vector<int> SelectionList;

  /** Returns a vector of the indexes of the selected rows
    * @return a SelectionList with the selection results
    */
  SelectionList get_selected();

protected:

 class TextModelColumns : public Gtk::TreeModel::ColumnRecord
  {
  public:
    TextModelColumns(guint columns_count);
    ~TextModelColumns();

    guint get_num_columns() const;

    Gtk::TreeModelColumn<Glib::ustring>* m_columns;

  protected:
    guint m_columns_count;
  };

  Glib::RefPtr<Gtk::ListStore> m_model;
  TextModelColumns m_model_columns;
};

} //namespace Gtk

#endif //_GTKMM_LISTVIEW_TEXT_H

