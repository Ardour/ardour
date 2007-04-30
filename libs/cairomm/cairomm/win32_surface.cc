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

#include <cairomm/win32_surface.h>
#include <cairomm/private.h>

namespace Cairo
{

#ifdef CAIRO_HAS_WIN32_SURFACE

Win32Surface::Win32Surface(cairo_surface_t* cobject, bool has_reference) :
    Surface(cobject, has_reference)
{}

Win32Surface::~Win32Surface()
{
  // surface is destroyed in base class
}

HDC Win32Surface::get_dc() const
{
  return cairo_win32_surface_get_dc(m_cobject);
}

RefPtr<Win32Surface> Win32Surface::create(HDC hdc)
{
  cairo_surface_t* cobject = cairo_win32_surface_create(hdc);
  check_status_and_throw_exception(cairo_surface_status(cobject));
  return RefPtr<Win32Surface>(new Win32Surface(cobject, true /* has reference */));
}

RefPtr<Win32Surface> Win32Surface::create(Format format, int width, int height)
{
  cairo_surface_t* cobject = cairo_win32_surface_create_with_dib((cairo_format_t)format, width, height);
  check_status_and_throw_exception(cairo_surface_status(cobject));
  return RefPtr<Win32Surface>(new Win32Surface(cobject, true /* has reference */));
}

#endif // CAIRO_HAS_WIN32_SURFACE

} //namespace Cairo

// vim: ts=2 sw=2 et
