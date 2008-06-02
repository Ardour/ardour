/* Copyright (C) 2008 The cairomm Development Team
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

#ifndef __CAIROMM_CONTEXT_PRIVATE_H
#define __CAIROMM_CONTEXT_PRIVATE_H

#ifdef ENABLE_GCC_VISIBILITY
  #define VISIBILITY_HIDDEN __attribute__ ((visibility("hidden")))
#else
  #define VISIBILITY_HIDDEN
#endif

#include <cairomm/refptr.h>
#include <cairomm/surface.h>

namespace Cairo
{

namespace Private
{

VISIBILITY_HIDDEN RefPtr<Surface> wrap_surface_quartz(cairo_surface_t*);
VISIBILITY_HIDDEN RefPtr<Surface> wrap_surface_win32(cairo_surface_t*);
VISIBILITY_HIDDEN RefPtr<Surface> wrap_surface_xlib(cairo_surface_t*);

} // namespace Private

} // namespace Cairo

#endif // __CAIROMM_CONTEXT_PRIVATE_H

// vim: ts=2 sw=2 et
