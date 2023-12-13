// -*- c++ -*-
#ifndef _GTKMM_TARGETENTRY_H
#define _GTKMM_TARGETENTRY_H

/* $Id$ */

/* targetentry.h
 *
 * Copyright (C) 2002 The gtkmm Development Team
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

#include <glibmm/utility.h>
#include <glibmm/ustring.h>
#include <glibmm/arrayhandle.h>
#include <gtkmm/enums.h>
#include <gtk/gtk.h> //For GtkTargetEntry.

#ifndef DOXYGEN_SHOULD_SKIP_THIS
extern "C"
{
  typedef struct _GtkTargetEntry GtkTargetEntry;
}
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

namespace Gtk
{

class TargetEntry
{
public:
  TargetEntry();
  explicit TargetEntry(const Glib::ustring& target, Gtk::TargetFlags flags = Gtk::TargetFlags(0), guint info = 0);
  explicit TargetEntry(const GtkTargetEntry& gobject);
  TargetEntry(const TargetEntry& src);
  virtual ~TargetEntry();

  TargetEntry& operator=(const TargetEntry& src);

  Glib::ustring get_target() const;
  void set_target(const Glib::ustring& target);

  Gtk::TargetFlags get_flags() const;
  void set_flags(Gtk::TargetFlags flags);

  guint get_info() const;
  void set_info(guint info);

  //Use this when you have to use an array of GdkTargetEntrys
  //This TargetEntry will still own the string memory.
  GtkTargetEntry* gobj();
  const GtkTargetEntry* gobj() const;

protected:
  GtkTargetEntry gobject_;
};

#ifndef DOXYGEN_SHOULD_SKIP_THIS
struct TargetEntry_Traits
{
  typedef TargetEntry CppType;
  typedef GtkTargetEntry CType;
  typedef GtkTargetEntry CTypeNonConst;

  static const CType& to_c_type(const CppType& item)
    { return *item.gobj(); }

  static const CType& to_c_type(const CType& item)
    { return item; }

  static CppType to_cpp_type(const CType& item)
    { return TargetEntry(item); /* copies string */}

  static void release_c_type(const CType&) {}
};
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

typedef Glib::ArrayHandle< TargetEntry, TargetEntry_Traits > ArrayHandle_TargetEntry;

} /* namespace Gtk */


#endif /* _GTKMM_TARGETENTRY_H */

