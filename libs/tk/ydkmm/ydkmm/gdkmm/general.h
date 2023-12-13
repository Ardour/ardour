/* $Id$ */

/* Copyright 2002      The gtkmm Development Team
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

#ifndef _GDKMM_GENERAL_H
#define _GDKMM_GENERAL_H

#include <gdkmm/pixbuf.h>
#include <cairomm/context.h>

namespace Gdk
{

int screen_width();
int screen_height();

int screen_width_mm();
int screen_height_mm();

void flush();

namespace Cairo
{

#ifndef GDKMM_DISABLE_DEPRECATED
/** Sets the specified Gdk::Color as the source color of the Cairo context.
 * @param @context The cairo context.
 * @param color The color to use as the source color. 
 *
 * @deprecated Use the overloaded function that takes a const RefPtr
 * reference.
 *
 * @newin{2,10}
 */
void set_source_color(::Cairo::RefPtr< ::Cairo::Context >& context, const Gdk::Color& color);
#endif

/** Sets the specified Gdk::Color as the source color of the Cairo context.
 * @param @context The cairo context.
 * @param color The color to use as the source color. 
 *
 * @newin{2,12}
 */
void set_source_color(const ::Cairo::RefPtr< ::Cairo::Context >& context, const Gdk::Color& color);

#ifndef GDKMM_DISABLE_DEPRECATED
/** Sets the given pixbuf as the source pattern for the Cairo context. The pattern has an extend mode of CAIRO_EXTEND_NONE and is aligned so that the origin of pixbuf is pixbuf_x, pixbuf_y.
 * @param @context The cairo context.
 * @param pixbuf A Gdk::Pixbuf
 * @param pixbuf_x X coordinate of location to place upper left corner of pixbuf.
 * @param pixbuf_y Y coordinate of location to place upper left corner of pixbuf.
 *
 * @deprecated Use the overloaded function that takes a const RefPtr
 * reference.
 *
 * @newin{2,10}
 */
void set_source_pixbuf(::Cairo::RefPtr< ::Cairo::Context >& context, const Glib::RefPtr<Gdk::Pixbuf>& pixbuf, double pixbuf_x = 0, double pixbuf_y = 0);
#endif

/** Sets the given pixbuf as the source pattern for the Cairo context. The pattern has an extend mode of CAIRO_EXTEND_NONE and is aligned so that the origin of pixbuf is pixbuf_x, pixbuf_y.
 * @param @context The cairo context.
 * @param pixbuf A Gdk::Pixbuf
 * @param pixbuf_x X coordinate of location to place upper left corner of pixbuf.
 * @param pixbuf_y Y coordinate of location to place upper left corner of pixbuf.
 *
 * @newin{2,12}
 */
void set_source_pixbuf(const ::Cairo::RefPtr< ::Cairo::Context >& context, const Glib::RefPtr<Gdk::Pixbuf>& pixbuf, double pixbuf_x = 0, double pixbuf_y = 0);

#ifndef GDKMM_DISABLE_DEPRECATED
/** Sets the given pixbuf as the source pattern for the Cairo context. The pattern has an extend mode of CAIRO_EXTEND_NONE and is aligned so that the origin of pixbuf is pixmap_x, pixmap_y.
 * @param @context The cairo context.
 * @param pixmap A Gdk::Pixmap
 * @param pixmap_x X coordinate of location to place upper left corner of pixmap.
 * @param pixmap_y Y coordinate of location to place upper left corner of pixmap.
 *
 * @deprecated Use the overloaded function that takes a const RefPtr
 * reference.
 *
 * @newin{2,10}
 */
void set_source_pixmap(::Cairo::RefPtr< ::Cairo::Context >& context, const Glib::RefPtr<Gdk::Pixmap>& pixmap, double pixmap_x, double pixmap_y);
#endif

/** Sets the given pixbuf as the source pattern for the Cairo context. The pattern has an extend mode of CAIRO_EXTEND_NONE and is aligned so that the origin of pixbuf is pixmap_x, pixmap_y.
 * @param @context The cairo context.
 * @param pixmap A Gdk::Pixmap
 * @param pixmap_x X coordinate of location to place upper left corner of pixmap.
 * @param pixmap_y Y coordinate of location to place upper left corner of pixmap.
 *
 * @newin{2,12}
 */
void set_source_pixmap(const ::Cairo::RefPtr< ::Cairo::Context >& context, const Glib::RefPtr<Gdk::Pixmap>& pixmap, double pixmap_x, double pixmap_y);

#ifndef GDKMM_DISABLE_DEPRECATED
/** Adds the given rectangle to the current path of the context.
 *
 * @param context A cairo context.
 * @param rectangle A rectangle to add to the path of the context. 
 *
 * @deprecated Use add_rectangle_to_path() instead
 * reference.
 *
 * @newin{2,10}
 */
void rectangle(::Cairo::RefPtr< ::Cairo::Context >& context, const Gdk::Rectangle& rectangle);
#endif

/** Adds the given rectangle to the current path of the context.
 *
 * @param context A cairo context.
 * @param rectangle A rectangle to add to the path of the context. 
 *
 * @newin{2,12}
 */
void add_rectangle_to_path(const ::Cairo::RefPtr< ::Cairo::Context >& context, const Gdk::Rectangle& rectangle);

#ifndef GDKMM_DISABLE_DEPRECATED
/** Adds the given region to the current path of the context.
 *
 * @param context A cairo context.
 * @param region A region to add to the path of the context. 
 *
 * @deprecated Use add_region_to_path() instead
 *
 * @newin{2,10}
 */
void region(::Cairo::RefPtr< ::Cairo::Context >& context, const Gdk::Region& region);
#endif

/** Adds the given region to the current path of the context.
 *
 * @param context A cairo context.
 * @param region A region to add to the path of the context. 
 *
 * @newin{2,12}
 */
void add_region_to_path(const ::Cairo::RefPtr< ::Cairo::Context >& context, const Gdk::Region& region);

} //namespace Cairo

} //namespace Gdk

#endif //_GDKMM_GENERAL_H

