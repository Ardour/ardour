// -*- c++ -*-
/* $Id$ */

/* Copyright 2002      The gtkmm Development Team
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

#include <gdkmm/general.h>
#include <gdk/gdk.h>

namespace Gdk
{

int screen_width()
{
  return gdk_screen_width();
}

int screen_height()
{
  return gdk_screen_height();
}

int screen_width_mm()
{
  return gdk_screen_width_mm();
}

int screen_height_mm()
{
  return gdk_screen_height_mm();
}


void flush()
{
  gdk_flush();
}

namespace Cairo
{

#ifndef GDKMM_DISABLE_DEPRECATED
void set_source_color(::Cairo::RefPtr< ::Cairo::Context >& context, const Gdk::Color& color)
{
  gdk_cairo_set_source_color(context->cobj(), const_cast<GdkColor*>(color.gobj()));
}
#endif

void set_source_color(const ::Cairo::RefPtr< ::Cairo::Context >& context, const Gdk::Color& color)
{
  gdk_cairo_set_source_color(context->cobj(), const_cast<GdkColor*>(color.gobj()));
}

#ifndef GDKMM_DISABLE_DEPRECATED
void set_source_pixbuf(::Cairo::RefPtr< ::Cairo::Context >& context, const Glib::RefPtr<Gdk::Pixbuf>& pixbuf, double pixbuf_x, double pixbuf_y)
{
  gdk_cairo_set_source_pixbuf(context->cobj(), pixbuf->gobj(), pixbuf_x, pixbuf_y);
}
#endif

void set_source_pixbuf(const ::Cairo::RefPtr< ::Cairo::Context >& context, const Glib::RefPtr<Gdk::Pixbuf>& pixbuf, double pixbuf_x, double pixbuf_y)
{
  gdk_cairo_set_source_pixbuf(context->cobj(), pixbuf->gobj(), pixbuf_x, pixbuf_y);
}

#ifndef GDKMM_DISABLE_DEPRECATED
void set_source_pixmap(::Cairo::RefPtr< ::Cairo::Context >& context, const Glib::RefPtr<Gdk::Pixmap>& pixmap, double pixmap_x, double pixmap_y)
{
  gdk_cairo_set_source_pixmap(context->cobj(), pixmap->gobj(), pixmap_x, pixmap_y);
}
#endif

void set_source_pixmap(const ::Cairo::RefPtr< ::Cairo::Context >& context, const Glib::RefPtr<Gdk::Pixmap>& pixmap, double pixmap_x, double pixmap_y)
{
  gdk_cairo_set_source_pixmap(context->cobj(), pixmap->gobj(), pixmap_x, pixmap_y);
}

#ifndef GDKMM_DISABLE_DEPRECATED
void rectangle(::Cairo::RefPtr< ::Cairo::Context >& context, const Gdk::Rectangle& rectangle)
{
  gdk_cairo_rectangle(context->cobj(), const_cast<GdkRectangle*>(rectangle.gobj()));
}
#endif

void add_rectangle_to_path(const ::Cairo::RefPtr< ::Cairo::Context >& context, const Gdk::Rectangle& rectangle)
{
  gdk_cairo_rectangle(context->cobj(), const_cast<GdkRectangle*>(rectangle.gobj()));
}

#ifndef GDKMM_DISABLE_DEPRECATED
void region(::Cairo::RefPtr< ::Cairo::Context >& context, const Gdk::Region& region)
{
  gdk_cairo_region(context->cobj(), const_cast<GdkRegion*>(region.gobj()));
}
#endif

void add_region_to_path(const ::Cairo::RefPtr< ::Cairo::Context >& context, const Gdk::Region& region)
{
  gdk_cairo_region(context->cobj(), const_cast<GdkRegion*>(region.gobj()));
}

} //namespace Cairo

} //namespace Gdk

