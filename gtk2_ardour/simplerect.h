// -*- c++ -*-
#ifndef _LIBGNOMECANVASMM_SIMPLERECT_H
#define _LIBGNOMECANVASMM_SIMPLERECT_H

#include <glibmm.h>

/* $Id$ */

/* rect.h
 * 
 * Copyright (C) 1998 EMC Capital Management Inc.
 * Developed by Havoc Pennington <hp@pobox.com>
 *
 * Copyright (C) 1999 The Gtk-- Development Team
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

#include <libgnomecanvasmm/item.h>
#include <libgnomecanvasmm/group.h>
#include <libgnomecanvasmm/shape.h>
#include "canvas-simplerect.h"


#ifndef DOXYGEN_SHOULD_SKIP_THIS
typedef struct _GnomeCanvasSimpleRect GnomeCanvasSimpleRect;
typedef struct _GnomeCanvasSimpleRectClass GnomeCanvasSimpleRectClass;
#endif /* DOXYGEN_SHOULD_SKIP_THIS */


namespace Gnome
{

namespace Canvas
{ class SimpleRect_Class; } // namespace Canvas

} // namespace Gnome
namespace Gnome
{

namespace Canvas
{

//class Group;


class SimpleRect : public Item
{
  public:
#ifndef DOXYGEN_SHOULD_SKIP_THIS
  typedef SimpleRect CppObjectType;
  typedef SimpleRect_Class CppClassType;
  typedef GnomeCanvasSimpleRect BaseObjectType;
  typedef GnomeCanvasSimpleRectClass BaseClassType;
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

  virtual ~SimpleRect();

#ifndef DOXYGEN_SHOULD_SKIP_THIS

private:
  friend class SimpleRect_Class;
  static CppClassType rect_class_;

  // noncopyable
  SimpleRect(const SimpleRect&);
  SimpleRect& operator=(const SimpleRect&);

protected:
  explicit SimpleRect(const Glib::ConstructParams& construct_params);
  explicit SimpleRect(GnomeCanvasSimpleRect* castitem);

#endif /* DOXYGEN_SHOULD_SKIP_THIS */

public:
#ifndef DOXYGEN_SHOULD_SKIP_THIS
  static GType get_type()      G_GNUC_CONST;
  static GType get_base_type() G_GNUC_CONST;
#endif

  ///Provides access to the underlying C GtkObject.
  GnomeCanvasSimpleRect*       gobj()       { return reinterpret_cast<GnomeCanvasSimpleRect*>(gobject_); }

  ///Provides access to the underlying C GtkObject.
  const GnomeCanvasSimpleRect* gobj() const { return reinterpret_cast<GnomeCanvasSimpleRect*>(gobject_); }


public:
  //C++ methods used to invoke GTK+ virtual functions:

protected:
  //GTK+ Virtual Functions (override these to change behaviour):

  //Default Signal Handlers::


private:


public:
  SimpleRect(Group& parent, double x1, double y1, double x2, double y2);
  explicit SimpleRect(Group& parent);


};

} /* namespace Canvas */
} /* namespace Gnome */

namespace Glib
{
  /** @relates Gnome::Canvas::SimpleRect
   * @param object The C instance
   * @param take_copy False if the result should take ownership of the C instance. True if it should take a new copy or ref.
   * @result A C++ instance that wraps this C instance.
   */
  Gnome::Canvas::SimpleRect* wrap(GnomeCanvasSimpleRect* object, bool take_copy = false);
}
#endif /* _LIBGNOMECANVASMM_RECT_H */

