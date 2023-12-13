// -*- c++ -*-
#ifndef _GTKMM_COMBOBOXENTRYTEXT_H
#define _GTKMM_COMBOBOXENTRYTEXT_H

/* comboboxentrytext.h
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

#ifndef GTKMM_DISABLE_DEPRECATED

#include <gtkmm/comboboxentry.h>

namespace Gtk
{

//This is a C++ convenience class that is equivalent to the gtk_combo_box_entry_new_text() C convenience function.
//This is copy/paste/search/replaced from ComboBoxText, but the only alternative I see is to use multiple inheritance.
//murrayc.
//In gtkmm-3.0 we simply wrap GtkComboBoxText, which is also in GTK+ 2.24.
//But this C++ class was created before GtkComboBoxText existed and we want to avoid changing the ABI. 

/** This is a specialisation of the ComboBoxEntry which has one column of text (a simple list),
 * and appropriate methods for setting and getting the text.
 *
 * You should not call set_model() or attempt to pack more cells into this combo box via its CellLayout base class.
 *
 * @deprecated Instead use ComboBoxText with has_entry = true.
 *
 * @ingroup Widgets
 */
class ComboBoxEntryText
: public ComboBoxEntry
{
#ifndef DOXYGEN_SHOULD_SKIP_THIS
private:
  // noncopyable
  ComboBoxEntryText(const ComboBoxEntryText&);
  ComboBoxEntryText& operator=(const ComboBoxEntryText&);

protected:
  explicit ComboBoxEntryText(const Glib::ConstructParams& construct_params);
  explicit ComboBoxEntryText(GtkComboBoxEntry* castitem);

#endif /* DOXYGEN_SHOULD_SKIP_THIS */

public:
  ComboBoxEntryText();

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
#endif //GTKMM_DISABLE_DEPRECATED

  //@deprecated Use get_entry()->get_text() to get the actual entered text.
  Glib::ustring get_active_text() const;

  //@deprecated Use get_entry()->set_text() to set the actual entered text.
  void set_active_text(const Glib::ustring& text);

  //There is a clear() method in the CellLayout base class, so this would cause confusion.
  //TODO: Remove this when we can break API.
  /// @deprecated See clear_items(). Since 2.8.
  void clear();

  /** Remove all items from the drop-down menu.
   */
  void clear_items();

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

#endif //GTKMM_DISABLE_DEPRECATED

#endif /* _GTKMM_COMBOBOXENTRYTEXT_H */

