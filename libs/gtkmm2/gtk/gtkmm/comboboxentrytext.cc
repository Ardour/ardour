// -*- c++ -*-
/* $Id$ */

/* 
 *
 * Copyright 2003 The gtkmm Development Team
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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <gtkmm/comboboxentrytext.h>

#include <gtkmm/liststore.h>
#include <gtkmm/cellrenderertext.h>
#include <gtk/gtkcombobox.h>
#include <gtk/gtkcelllayout.h>

namespace Gtk
{

ComboBoxEntryText::ComboBoxEntryText()
{
  set_model( Gtk::ListStore::create(m_text_columns) );
  set_text_column(m_text_columns.m_column);
}

ComboBoxEntryText::ComboBoxEntryText(GtkComboBoxEntry* castitem)
: Gtk::ComboBoxEntry(castitem)
{
  set_model( Gtk::ListStore::create(m_text_columns) );
  set_text_column(m_text_columns.m_column);
}

void ComboBoxEntryText::append_text(const Glib::ustring& text)
{
  //We can not use gtk_combo_box_append_text() here, because that can only be used if gtk_combo_box_new_text() has been used.

  //Ideally, we would just store the ListStore as a member variable, but we forgot to do that and not it would break the ABI.
  Glib::RefPtr<Gtk::TreeModel> model = get_model();
  Glib::RefPtr<Gtk::ListStore> list_model = Glib::RefPtr<ListStore>::cast_dynamic(model);
  
  if(list_model)
  {
    Gtk::TreeModel::iterator iter = list_model->append();
    Gtk::TreeModel::Row row = *iter;
    row[m_text_columns.m_column] = text;
  }
}

void ComboBoxEntryText::insert_text(int position, const Glib::ustring& text)
{
  //TODO: We should not use gtk_combo_box_insert_text() here, because that can only be used if gtk_combo_box_new_text() has been used.
  gtk_combo_box_insert_text(GTK_COMBO_BOX(gobj()), position, text.c_str());
}

void ComboBoxEntryText::prepend_text(const Glib::ustring& text)
{
  //We can not use gtk_combo_box_prepend_text() here, because that can only be used if gtk_combo_box_new_text() has been used.

  //Ideally, we would just store the ListStore as a member variable, but we forgot to do that and not it would break the ABI.
  Glib::RefPtr<Gtk::TreeModel> model = get_model();
  Glib::RefPtr<Gtk::ListStore> list_model = Glib::RefPtr<ListStore>::cast_dynamic(model);
  
  if(list_model)
  {
    Gtk::TreeModel::iterator iter = list_model->prepend();
    Gtk::TreeModel::Row row = *iter;
    row[m_text_columns.m_column] = text;
  }
}

void ComboBoxEntryText::clear()
{
  //Ideally, we would just store the ListStore as a member variable, but we forgot to do that and not it would break the ABI.
  Glib::RefPtr<Gtk::TreeModel> model = get_model();
  Glib::RefPtr<Gtk::ListStore> list_model = Glib::RefPtr<ListStore>::cast_dynamic(model);
  
  if(list_model)  
    list_model->clear();
}

Glib::ustring ComboBoxEntryText::get_active_text() const
{
  Glib::ustring result;

  //Get the active row:
  TreeModel::iterator active_row = get_active();
  if(active_row)
  {
    Gtk::TreeModel::Row row = *active_row;
    result = row[m_text_columns.m_column];
  }

  return result;
}

void ComboBoxEntryText::set_active_text(const Glib::ustring& text)
{
  //Look for the row with this text, and activate it:
  Glib::RefPtr<Gtk::TreeModel> model = get_model();
  if(model)
  {
    for(Gtk::TreeModel::iterator iter = model->children().begin(); iter != model->children().end(); ++iter)
    {
      const Glib::ustring& this_text = (*iter)[m_text_columns.m_column];

      if(this_text == text)
      {
        set_active(iter);
        return; //success
      }
    }
  }

  //Not found, so mark it as blank:
  unset_active();
}


} // namespace Gtk


