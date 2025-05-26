// -*- c++ -*-
/* $Id$ */

/* 
 *
 * Copyright 2003 The gtkmm Development Team
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


//Allow us to use deprecated GTK+ API.
//This whole C++ class is deprecated anyway.
#undef GTK_DISABLE_DEPRECATED

#include <ytkmm/comboboxentrytext.h>

#include <ytkmm/liststore.h>
#include <ytkmm/cellrenderertext.h>
#include <ytk/ytk.h>


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

void ComboBoxEntryText::append(const Glib::ustring& text)
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

void ComboBoxEntryText::insert(int position, const Glib::ustring& text)
{
  //TODO: We should not use gtk_combo_box_insert_text() here, because that can only be used if gtk_combo_box_new_text() has been used.
  gtk_combo_box_insert_text(GTK_COMBO_BOX(gobj()), position, text.c_str());
}

void ComboBoxEntryText::prepend(const Glib::ustring& text)
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


void ComboBoxEntryText::append_text(const Glib::ustring& text)
{
  append(text);
}

void ComboBoxEntryText::insert_text(int position, const Glib::ustring& text)
{
  insert(position, text);
}

void ComboBoxEntryText::prepend_text(const Glib::ustring& text)
{
  prepend(text);
}


void ComboBoxEntryText::clear_items()
{
  //Ideally, we would just store the ListStore as a member variable, but we forgot to do that and not it would break the ABI.
  Glib::RefPtr<Gtk::TreeModel> model = get_model();
  Glib::RefPtr<Gtk::ListStore> list_model = Glib::RefPtr<ListStore>::cast_dynamic(model);

  if(list_model)  
    list_model->clear();
}

void ComboBoxEntryText::remove_text(const Glib::ustring& text)
{
  //Ideally, we would just store the ListStore as a member variable, but we forgot to do that and not it would break the ABI.
  Glib::RefPtr<Gtk::TreeModel> model = get_model();
  Glib::RefPtr<Gtk::ListStore> list_model = Glib::RefPtr<ListStore>::cast_dynamic(model);

  //Look for the row with this text, and remove it:
  if(list_model)
  {
    for(Gtk::TreeModel::iterator iter = list_model->children().begin(); iter != list_model->children().end(); ++iter)
    {
      const Glib::ustring& this_text = (*iter)[m_text_columns.m_column];

      if(this_text == text)
      {
        list_model->erase(iter);
        return; //success
      }
    }
  }
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

//deprecated.
void ComboBoxEntryText::clear()
{
  clear_items();
}

#endif //GTKMM_DISABLE_DEPRECATED

} // namespace Gtk


