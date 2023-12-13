// -*- c++ -*-
#ifndef _GTKMM_COMBOBOXTEXT_H
#define _GTKMM_COMBOBOXTEXT_H

/* comboboxtext.h
 * 
 * Copyright (C) 2003 The gtkmm Development Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
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

#include <gtkmm/combobox.h>

namespace Gtk
{

//This is a C++ convenience class that is equivalent to the gtk_combo_box_new_text() C convenience function.
//In gtkmm-3.0 we simply wrap GtkComboBoxText, which is also in GTK+ 2.24.
//But this C++ class was created before GtkComboBoxText existed and we want to avoid changing the ABI. 

/** This is a specialisation of the ComboBox which has one column of text (a simple list),
 * and appropriate methods for setting and getting the text.
 *
 * You should not call set_model() or attempt to pack more cells into this combo box via its CellLayout base class.
 *
 * You can't use this class with Gtk::Builder::get_widget() or
 * Gtk::Builder::get_widget_derived() to get a GtkComboBoxText object from a
 * Glade file. Gtk::ComboBoxText does not wrap GtkComboBoxText, because
 * Gtk::ComboBoxText was made before GtkComboBoxText, and we don't want to
 * break the ABI.
 * This has been fixed in gtkmm 3.x, which is not ABI-compatible with
 * gtkmm 2.x, and which you are recommended to use.
 *
 * Note that you cannot use this class with Gnome::Glade::Xml::get_widget_derived() to wrap a GtkComboBox added 
 * in the Glade user interface designer, because Glade adds its own TreeModel instead of using the TreeModel from 
 * this class. You could use a normal Gtk::ComboBox instead, though you can not use Glade to add rows to a TreeModel 
 * that is defined in your C++ code.
 *
 * @ingroup Widgets
 */
class ComboBoxText
: public ComboBox
{
#ifndef DOXYGEN_SHOULD_SKIP_THIS
private:
  // noncopyable
  ComboBoxText(const ComboBoxText&);
  ComboBoxText& operator=(const ComboBoxText&);

protected:
  explicit ComboBoxText(const Glib::ConstructParams& construct_params);
  explicit ComboBoxText(GtkComboBox* castitem);

#endif /* DOXYGEN_SHOULD_SKIP_THIS */

public:

  /** Creates a new empty ComboBoxText, without an entry.
   */
  ComboBoxText();
  
  /** Creates a new empty ComboBoxText, optionally with an entry.
   * @param has_entry If this is true then this will have an Entry widget.
   */
  explicit ComboBoxText(bool has_entry); //In gtkmm 3.0 has_entry has a default value but we already have a default constructor here.

  /** Add an item to the end of the drop-down list.
   * @param text The text for the item.
   */
  void append(const Glib::ustring& text);
  
  void insert(int position, const Glib::ustring& text);

  /** Add an item to the beginning of the drop-down list.
   * @param text The text for the item.
   */
  void prepend(const Glib::ustring& text);
  
#ifndef GTKMM_DISABLE_DEPRECATED
  /** Add an item to the end of the drop-down list.
   * @param text The text for the item.
   *
   * @deprecated Use append().
   */
  void append_text(const Glib::ustring& text);
  
  /**
   * @deprecated Use insert().
   */
  void insert_text(int position, const Glib::ustring& text);

  /** Add an item to the beginning of the drop-down list.
   * @param text The text for the item.
   *
   * @deprecated Use prepend().
   */
  void prepend_text(const Glib::ustring& text);
  
  /** Remove all items from the drop-down menu.
   *
   * @deprecated Use remove_all().
   */
  void clear_items();
#endif //GTKMM_DISABLE_DEPRECATED

  /** Get the currently-chosen item.
   * @result The text of the active item.
   */
  Glib::ustring get_active_text() const;

  /** Set the currently-chosen item if it matches the specified text.
   * @text The text of the item that should be selected.
   */
  void set_active_text(const Glib::ustring& text);

  //There is a clear() method in the CellLayout base class, so this would cause confusion.
  //TODO: Remove this when we can break API.
  /// @deprecated Use remove_all(). Since 2.8.
  void clear();

  /** Remove all items from the drop-down menu.
   */
  void remove_all();
  
  /** Remove the specified item if it is in the drop-down menu.
   * @text The text of the item that should be removed.
   */
  void remove_text(const Glib::ustring& text);

protected:

  //Tree model columns:
  //These columns are used by the model that is created by the default constructor
  class TextModelColumns : public Gtk::TreeModel::ColumnRecord
  {
  public:
    TextModelColumns()
    { add(m_column); }

    Gtk::TreeModelColumn<Glib::ustring> m_column;
  };

  TextModelColumns m_text_columns;
};


} // namespace Gtk


#endif /* _GTKMM_COMBOBOXTEXT_H */

