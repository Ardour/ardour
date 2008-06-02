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

#include <cairomm/quartz_surface.h>
#include <cairomm/private.h>

namespace Cairo
{

#ifdef CAIRO_HAS_QUARTZ_SURFACE

QuartzSurface::QuartzSurface(cairo_surface_t* cobject, bool has_reference) :
    Surface(cobject, has_reference)
{}

QuartzSurface::~QuartzSurface()
{
  // surface is destroyed in base class
}

CGContextRef QuartzSurface::get_cg_context() const
{
  return cairo_quartz_surface_get_cg_context(m_cobject);
}

RefPtr<QuartzSurface> QuartzSurface::create(CGContextRef cg_context, int width, int height)
{
  cairo_surface_t* cobject = cairo_quartz_surface_create_for_cg_context(cg_context, 
          width, height);
  check_status_and_throw_exception(cairo_surface_status(cobject));
  return RefPtr<QuartzSurface>(new QuartzSurface(cobject, true /* has reference */));
}

RefPtr<QuartzSurface> QuartzSurface::create(Format format, int width, int height)
{
  cairo_surface_t* cobject = cairo_quartz_surface_create((cairo_format_t)format, width, height);
  check_status_and_throw_exception(cairo_surface_status(cobject));
  return RefPtr<QuartzSurface>(new QuartzSurface(cobject, true /* has reference */));
}

#endif // CAIRO_HAS_QUARTZ_SURFACE

} //namespace Cairo

// vim: ts=2 sw=2 et
