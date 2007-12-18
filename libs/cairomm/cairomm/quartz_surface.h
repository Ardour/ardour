/* Copyright (C) 2007 The cairomm Development Team
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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef __CAIROMM_QUARTZ_SURFACE_H
#define __CAIROMM_QUARTZ_SURFACE_H

#include <cairomm/surface.h>

#ifdef CAIRO_HAS_QUARTZ_SURFACE
#include <cairo-quartz.h>
#endif

namespace Cairo
{

#ifdef CAIRO_HAS_QUARTZ_SURFACE

/** A QuartzSurface provides a way to render within Apple Mac OS X.  If you
 * want to draw to the screen within a Mac OS X application, you
 * should use this Surface type.
 *
 * @note For this Surface to be available, cairo must have been compiled with
 * (native) Quartz support (requires Cairo > 1.4.0)
 */
class QuartzSurface : public Surface
{
public:

  /** Create a C++ wrapper for the C instance. This C++ instance should then be
   * given to a RefPtr.
   *
   * @param cobject The C instance.
   * @param has_reference whether we already have a reference. Otherwise, the
   * constructor will take an extra reference.
   */
  explicit QuartzSurface(cairo_surface_t* cobject, bool has_reference = false);
  virtual ~QuartzSurface();

  /** Returns the CGContextRef associated with this surface, or NULL if none. Also
   * returns NULL if the surface is not a Quartz surface.
   *
   * @return CGContextRef or NULL if no CGContextRef available.
   */
  CGContextRef get_cg_context() const;

  /** Creates a cairo surface that targets the given CGContext.
   *
   * @param cg_context the CGContext to create a surface for
   * @return the newly created surface
   */
  static RefPtr<QuartzSurface> create(CGContextRef cg_context, int width, int height);

  /** Creates a device-independent-bitmap surface not associated with any
   * particular existing surface or device context. The created bitmap will be
   * unititialized.
   *
   * @param format format of pixels in the surface to create
   * @param width width of the surface, in pixels
   * @param height height of the surface, in pixels
   * @return the newly created surface
   */
  static RefPtr<QuartzSurface> create(Format format, int width, int height);

};

#endif // CAIRO_HAS_QUARTZ_SURFACE


} // namespace Cairo

#endif //__CAIROMM_QUARTZ_SURFACE_H

// vim: ts=2 sw=2 et
