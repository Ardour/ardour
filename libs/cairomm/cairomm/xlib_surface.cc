/* Copyright (C) 2005 The cairomm Development Team
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

#include <cairomm/xlib_surface.h>
#include <cairomm/private.h>


namespace Cairo
{

#ifdef CAIRO_HAS_XLIB_SURFACE

XlibSurface::XlibSurface(cairo_surface_t* cobject, bool has_reference) :
    Surface(cobject, has_reference)
{}

XlibSurface::~XlibSurface()
{
  // surface is destroyed in base class
}

RefPtr<XlibSurface> XlibSurface::create(Display* dpy, Drawable drawable, Visual* visual, int width, int height)
{
  cairo_surface_t* cobject = cairo_xlib_surface_create(dpy, drawable, visual, width, height);
  check_status_and_throw_exception(cairo_surface_status(cobject));
  return RefPtr<XlibSurface>(new XlibSurface(cobject, true /* has reference */));
}

RefPtr<XlibSurface> XlibSurface::create(Display* dpy, Pixmap bitmap, Screen* screen, int width, int height)
{
  cairo_surface_t* cobject = cairo_xlib_surface_create_for_bitmap(dpy, bitmap, screen, width, height);
  check_status_and_throw_exception(cairo_surface_status(cobject));
  return RefPtr<XlibSurface>(new XlibSurface(cobject, true /* has reference */));
}

void XlibSurface::set_size(int width, int height)
{
  cairo_xlib_surface_set_size(m_cobject, width, height);
  check_object_status_and_throw_exception(*this);
}

void XlibSurface::set_drawable(Drawable drawable, int width, int height)
{
  cairo_xlib_surface_set_drawable(m_cobject, drawable, width, height);
  check_object_status_and_throw_exception(*this);
}

Drawable XlibSurface::get_drawable() const
{
  Drawable drawable = cairo_xlib_surface_get_drawable(m_cobject);
  check_object_status_and_throw_exception(*this);
  return drawable;
}

const Display* XlibSurface::get_display() const
{
  const Display* dpy = cairo_xlib_surface_get_display(m_cobject);
  check_object_status_and_throw_exception(*this);
  return dpy;
}

Display* XlibSurface::get_display()
{
  Display* dpy = cairo_xlib_surface_get_display(m_cobject);
  check_object_status_and_throw_exception(*this);
  return dpy;
}

Screen* XlibSurface::get_screen()
{
  Screen* screen = cairo_xlib_surface_get_screen(m_cobject);
  check_object_status_and_throw_exception(*this);
  return screen;
}

const Screen* XlibSurface::get_screen() const
{
  const Screen* screen = cairo_xlib_surface_get_screen(m_cobject);
  check_object_status_and_throw_exception(*this);
  return screen;
}

Visual* XlibSurface::get_visual()
{
  Visual* visual = cairo_xlib_surface_get_visual(m_cobject);
  check_object_status_and_throw_exception(*this);
  return visual;
}

const Visual* XlibSurface::get_visual() const
{
  const Visual* visual = cairo_xlib_surface_get_visual(m_cobject);
  check_object_status_and_throw_exception(*this);
  return visual;
}

int XlibSurface::get_depth() const
{
  int depth = cairo_xlib_surface_get_depth(m_cobject);
  check_object_status_and_throw_exception(*this);
  return depth;
}

int XlibSurface::get_height() const
{
  int h = cairo_xlib_surface_get_height(m_cobject);
  check_object_status_and_throw_exception(*this);
  return h;
}

int XlibSurface::get_width() const
{
  int w = cairo_xlib_surface_get_width(m_cobject);
  check_object_status_and_throw_exception(*this);
  return w;
}

#endif // CAIRO_HAS_XLIB_SURFACE

} //namespace Cairo

// vim: ts=2 sw=2 et
