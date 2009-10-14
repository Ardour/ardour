// -*- c++ -*-
#ifndef _LIBGNOMECANVASMM_SIMPLELINE_H
#define _LIBGNOMECANVASMM_SIMPLELINE_H

#include <glibmm.h>


/* line.h
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
#include <libgnomecanvas/gnome-canvas-util.h>
#include <libgnomecanvas/libgnomecanvas.h>
#include "canvas-simpleline.h"

#ifndef DOXYGEN_SHOULD_SKIP_THIS
typedef struct _GnomeCanvasSimpleLine GnomeCanvasSimpleLine;
typedef struct _GnomeCanvasSimpleLineClass GnomeCanvasSimpleLineClass;
#endif /* DOXYGEN_SHOULD_SKIP_THIS */


namespace Gnome
{

namespace Canvas
{ class SimpleLine_Class; } // namespace Canvas

} // namespace Gnome
namespace Gnome
{

namespace Canvas
{

class GnomeGroup;

class SimpleLine : public Item
{
  public:
#ifndef DOXYGEN_SHOULD_SKIP_THIS
  typedef SimpleLine CppObjectType;
  typedef SimpleLine_Class CppClassType;
  typedef GnomeCanvasSimpleLine BaseObjectType;
  typedef GnomeCanvasSimpleLineClass BaseClassType;
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

  virtual ~SimpleLine();

#ifndef DOXYGEN_SHOULD_SKIP_THIS

private:
  friend class SimpleLine_Class;
  static CppClassType line_class_;

  // noncopyable
  SimpleLine(const SimpleLine&);
  SimpleLine& operator=(const SimpleLine&);

protected:
  explicit SimpleLine(const Glib::ConstructParams& construct_params);
  explicit SimpleLine(GnomeCanvasSimpleLine* castitem);

#endif /* DOXYGEN_SHOULD_SKIP_THIS */

public:
#ifndef DOXYGEN_SHOULD_SKIP_THIS
  static GType get_type()      G_GNUC_CONST;
  static GType get_base_type() G_GNUC_CONST;
#endif

  ///Provides access to the underlying C GtkObject.
  GnomeCanvasSimpleLine*       gobj()       { return reinterpret_cast<GnomeCanvasSimpleLine*>(gobject_); }

  ///Provides access to the underlying C GtkObject.
  const GnomeCanvasSimpleLine* gobj() const { return reinterpret_cast<GnomeCanvasSimpleLine*>(gobject_); }


public:
  //C++ methods used to invoke GTK+ virtual functions:

protected:
  //GTK+ Virtual Functions (override these to change behaviour):

  //Default Signal Handlers::


private:

public:
  explicit SimpleLine(Group& parent);
  SimpleLine(Group& parent, double x1, double y1, double x2, double y2);

  /**
   *
   * You rarely need to use properties because there are get_ and set_ methods for almost all of them.
   * @return A PropertyProxy that allows you to get or set the property of the value, or receive notification when
   * the value of the property changes.
   */
  Glib::PropertyProxy<double> property_x1() ;

/**
   *
   * You rarely need to use properties because there are get_ and set_ methods for almost all of them.
   * @return A PropertyProxy that allows you to get or set the property of the value, or receive notification when
   * the value of the property changes.
   */
  Glib::PropertyProxy_ReadOnly<double> property_x1() const;

  /**
   *
   * You rarely need to use properties because there are get_ and set_ methods for almost all of them.
   * @return A PropertyProxy that allows you to get or set the property of the value, or receive notification when
   * the value of the property changes.
   */
  Glib::PropertyProxy<double> property_x2() ;

/**
   *
   * You rarely need to use properties because there are get_ and set_ methods for almost all of them.
   * @return A PropertyProxy that allows you to get or set the property of the value, or receive notification when
   * the value of the property changes.
   */
  Glib::PropertyProxy_ReadOnly<double> property_x2() const;


  /**
   *
   * You rarely need to use properties because there are get_ and set_ methods for almost all of them.
   * @return A PropertyProxy that allows you to get or set the property of the value, or receive notification when
   * the value of the property changes.
   */
  Glib::PropertyProxy<double> property_y1() ;

/**
   *
   * You rarely need to use properties because there are get_ and set_ methods for almost all of them.
   * @return A PropertyProxy that allows you to get or set the property of the value, or receive notification when
   * the value of the property changes.
   */
  Glib::PropertyProxy_ReadOnly<double> property_y1() const;

  /**
   *
   * You rarely need to use properties because there are get_ and set_ methods for almost all of them.
   * @return A PropertyProxy that allows you to get or set the property of the value, or receive notification when
   * the value of the property changes.
   */
  Glib::PropertyProxy<double> property_y2() ;

/**
   *
   * You rarely need to use properties because there are get_ and set_ methods for almost all of them.
   * @return A PropertyProxy that allows you to get or set the property of the value, or receive notification when
   * the value of the property changes.
   */
  Glib::PropertyProxy_ReadOnly<double> property_y2() const;

  /**
   *
   * You rarely need to use properties because there are get_ and set_ methods for almost all of them.
   * @return A PropertyProxy that allows you to get or set the property of the value, or receive notification when
   * the value of the property changes.
   */
  Glib::PropertyProxy<guint> property_color_rgba() ;

/**
   *
   * You rarely need to use properties because there are get_ and set_ methods for almost all of them.
   * @return A PropertyProxy that allows you to get or set the property of the value, or receive notification when
   * the value of the property changes.
   */
  Glib::PropertyProxy_ReadOnly<guint> property_color_rgba() const;
};

} /* namespace Canvas */
} /* namespace Gnome */


namespace Glib
{
  /** @relates Gnome::Canvas::SimpleLine
   * @param object The C instance
   * @param take_copy False if the result should take ownership of the C instance. True if it should take a new copy or ref.
   * @result A C++ instance that wraps this C instance.
   */
  Gnome::Canvas::SimpleLine* wrap(GnomeCanvasSimpleLine* object, bool take_copy = false);
}
#endif /* _LIBGNOMECANVASMM_LINE_H */

