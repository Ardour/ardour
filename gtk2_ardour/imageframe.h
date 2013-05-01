// -*- c++ -*-
#ifndef _LIBGNOMECANVASMM_IMAGEFRAME_H
#define _LIBGNOMECANVASMM_IMAGEFRAME_H

#include <glibmm.h>


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

#include <stdint.h>
#include <libgnomecanvasmm/item.h>
#include <libgnomecanvasmm/group.h>
#include <libgnomecanvasmm/shape.h>

#include <libgnomecanvas/libgnomecanvas.h>
#include <gtk/gtkenums.h>

#ifdef __cplusplus
extern "C" {
#endif
#include <libart_lgpl/art_misc.h>
#ifdef __cplusplus
}
#endif

#include <libart_lgpl/art_pixbuf.h>


#ifndef DOXYGEN_SHOULD_SKIP_THIS
typedef struct _GnomeCanvasImageFrame GnomeCanvasImageFrame;
typedef struct _GnomeCanvasImageFrameClass GnomeCanvasImageFrameClass;
#endif /* DOXYGEN_SHOULD_SKIP_THIS */


namespace Gnome
{

namespace Canvas
{ class ImageFrame_Class; } // namespace Canvas

} // namespace Gnome
namespace Gnome
{

namespace Canvas
{

//class Group;


class ImageFrame : public Item
{
  public:
#ifndef DOXYGEN_SHOULD_SKIP_THIS
  typedef ImageFrame CppObjectType;
  typedef ImageFrame_Class CppClassType;
  typedef GnomeCanvasImageFrame BaseObjectType;
  typedef GnomeCanvasImageFrameClass BaseClassType;
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

  virtual ~ImageFrame();

#ifndef DOXYGEN_SHOULD_SKIP_THIS

private:
  friend class ImageFrame_Class;
  static CppClassType rect_class_;

  // noncopyable
  ImageFrame(const ImageFrame&);
  ImageFrame& operator=(const ImageFrame&);

protected:
  explicit ImageFrame(const Glib::ConstructParams& construct_params);
  explicit ImageFrame(GnomeCanvasImageFrame* castitem);

#endif /* DOXYGEN_SHOULD_SKIP_THIS */

public:
#ifndef DOXYGEN_SHOULD_SKIP_THIS
  static GType get_type()      G_GNUC_CONST;
  static GType get_base_type() G_GNUC_CONST;
#endif

  ///Provides access to the underlying C GtkObject.
  GnomeCanvasImageFrame*       gobj()       { return reinterpret_cast<GnomeCanvasImageFrame*>(gobject_); }

  ///Provides access to the underlying C GtkObject.
  const GnomeCanvasImageFrame* gobj() const { return reinterpret_cast<GnomeCanvasImageFrame*>(gobject_); }


public:
  //C++ methods used to invoke GTK+ virtual functions:

protected:
  //GTK+ Virtual Functions (override these to change behaviour):

  //Default Signal Handlers::


private:


public:
    ImageFrame(Group& parentx, ArtPixBuf* pbuf, double x, double y, Gtk::AnchorType anchor, double w, double h);
    explicit ImageFrame(Group& parent);

    Glib::PropertyProxy<double> property_x();
    Glib::PropertyProxy_ReadOnly<double> property_x() const;
    Glib::PropertyProxy<double> property_y();
    Glib::PropertyProxy_ReadOnly<double> property_y() const;
    Glib::PropertyProxy<double> property_width();
    Glib::PropertyProxy_ReadOnly<double> property_width() const;
    Glib::PropertyProxy<double> property_drawwidth();
    Glib::PropertyProxy_ReadOnly<double> property_drawwidth() const;
    Glib::PropertyProxy<double> property_height();
    Glib::PropertyProxy_ReadOnly<double> property_height() const;
    Glib::PropertyProxy<Gtk::AnchorType> property_anchor() ;
    Glib::PropertyProxy_ReadOnly<Gtk::AnchorType> property_anchor() const;

};

} /* namespace Canvas */
} /* namespace Gnome */

namespace Glib
{
  /** @relates Gnome::Canvas::ImageFrame
   * @param object The C instance
   * @param take_copy False if the result should take ownership of the C instance. True if it should take a new copy or ref.
   * @result A C++ instance that wraps this C instance.
   */
  Gnome::Canvas::ImageFrame* wrap(GnomeCanvasImageFrame* object, bool take_copy = false);
}
#endif /* _LIBGNOMECANVASMM_IMAGEFRAME_H */

