// Generated by gmmproc 2.45.3 -- DO NOT MODIFY!

#undef GTK_DISABLE_DEPRECATED
 
#ifndef GTKMM_DISABLE_DEPRECATED


#include <glibmm.h>

#include <ytkmm/comboboxentry.h>
#include <ytkmm/private/comboboxentry_p.h>


// -*- c++ -*-
/* $Id: comboboxentry.ccg,v 1.2 2004/10/10 20:41:20 murrayc Exp $ */

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

#include <ytk/ytk.h>

namespace Gtk
{


Entry* ComboBoxEntry::get_entry()
{
  return Glib::wrap((GtkEntry*)(gtk_bin_get_child((GtkBin*)gobj())));
}

const Entry* ComboBoxEntry::get_entry() const
{
  GtkBin* base = (GtkBin*)const_cast<GtkComboBoxEntry*>(gobj());
  return Glib::wrap((GtkEntry*)(gtk_bin_get_child(base)));
}

Glib::ustring ComboBoxEntry::get_active_text() const
{
  //gtk_combo_box_get_active_text() can be used with text-comboboxes, 
  //or GtkComboBoxEntry, which is quite stupid. murrayc:
  //See also: https://bugzilla.gnome.org/show_bug.cgi?id=612396#c44
  return Glib::convert_return_gchar_ptr_to_ustring (gtk_combo_box_get_active_text(GTK_COMBO_BOX(gobj())));
}


} // namespace Gtk


namespace
{
} // anonymous namespace


namespace Glib
{

Gtk::ComboBoxEntry* wrap(GtkComboBoxEntry* object, bool take_copy)
{
  return dynamic_cast<Gtk::ComboBoxEntry *> (Glib::wrap_auto ((GObject*)(object), take_copy));
}

} /* namespace Glib */

namespace Gtk
{


/* The *_Class implementation: */

const Glib::Class& ComboBoxEntry_Class::init()
{
  if(!gtype_) // create the GType if necessary
  {
    // Glib::Class has to know the class init function to clone custom types.
    class_init_func_ = &ComboBoxEntry_Class::class_init_function;

    // This is actually just optimized away, apparently with no harm.
    // Make sure that the parent type has been created.
    //CppClassParent::CppObjectType::get_type();

    // Create the wrapper type, with the same class/instance size as the base type.
    register_derived_type(gtk_combo_box_entry_get_type());

    // Add derived versions of interfaces, if the C type implements any interfaces:

  }

  return *this;
}


void ComboBoxEntry_Class::class_init_function(void* g_class, void* class_data)
{
  BaseClassType *const klass = static_cast<BaseClassType*>(g_class);
  CppClassParent::class_init_function(klass, class_data);


}


Glib::ObjectBase* ComboBoxEntry_Class::wrap_new(GObject* o)
{
  return manage(new ComboBoxEntry((GtkComboBoxEntry*)(o)));

}


/* The implementation: */

ComboBoxEntry::ComboBoxEntry(const Glib::ConstructParams& construct_params)
:
  Gtk::ComboBox(construct_params)
{
  }

ComboBoxEntry::ComboBoxEntry(GtkComboBoxEntry* castitem)
:
  Gtk::ComboBox((GtkComboBox*)(castitem))
{
  }

ComboBoxEntry::~ComboBoxEntry()
{
  destroy_();
}

ComboBoxEntry::CppClassType ComboBoxEntry::comboboxentry_class_; // initialize static member

GType ComboBoxEntry::get_type()
{
  return comboboxentry_class_.init().get_type();
}


GType ComboBoxEntry::get_base_type()
{
  return gtk_combo_box_entry_get_type();
}


ComboBoxEntry::ComboBoxEntry()
:
  // Mark this class as non-derived to allow C++ vfuncs to be skipped.
  Glib::ObjectBase(0),
  Gtk::ComboBox(Glib::ConstructParams(comboboxentry_class_.init()))
{
  

}

ComboBoxEntry::ComboBoxEntry(const Glib::RefPtr<TreeModel>& model, const TreeModelColumnBase& text_column)
:
  // Mark this class as non-derived to allow C++ vfuncs to be skipped.
  Glib::ObjectBase(0),
  Gtk::ComboBox(Glib::ConstructParams(comboboxentry_class_.init(), "model", Glib::unwrap(model), "text_column", (text_column).index(), static_cast<char*>(0)))
{
  

}

ComboBoxEntry::ComboBoxEntry(const Glib::RefPtr<TreeModel>& model, int text_column)
:
  // Mark this class as non-derived to allow C++ vfuncs to be skipped.
  Glib::ObjectBase(0),
  Gtk::ComboBox(Glib::ConstructParams(comboboxentry_class_.init(), "model", Glib::unwrap(model), "text_column", text_column, static_cast<char*>(0)))
{
  

}

void ComboBoxEntry::set_text_column(const TreeModelColumnBase& text_column) const
{
  gtk_combo_box_entry_set_text_column(const_cast<GtkComboBoxEntry*>(gobj()), (text_column).index());
}

void ComboBoxEntry::set_text_column(int text_column) const
{
  gtk_combo_box_entry_set_text_column(const_cast<GtkComboBoxEntry*>(gobj()), text_column);
}

int ComboBoxEntry::get_text_column() const
{
  return gtk_combo_box_entry_get_text_column(const_cast<GtkComboBoxEntry*>(gobj()));
}


Glib::PropertyProxy< int > ComboBoxEntry::property_text_column() 
{
  return Glib::PropertyProxy< int >(this, "text-column");
}

Glib::PropertyProxy_ReadOnly< int > ComboBoxEntry::property_text_column() const
{
  return Glib::PropertyProxy_ReadOnly< int >(this, "text-column");
}


} // namespace Gtk

#endif // GTKMM_DISABLE_DEPRECATED


