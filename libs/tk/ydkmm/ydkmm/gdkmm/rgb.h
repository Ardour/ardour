/* $Id$ */

/* Copyright 2004      The gtkmm Development Team
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

#ifndef _GDKMM_RGB_H
#define _GDKMM_RGB_H

#include <gdkmm/colormap.h>  
#include <gdkmm/visual.h>

namespace Gdk
{

/** Get the preferred colormap for rendering image data.
 * Not a very useful function; historically, GDK could only render RGB image data to one colormap and visual,
 * but in the current version it can render to any colormap and visual. So there's no need to call this function.
 *
 * @result The preferred colormap
 */
Glib::RefPtr<Colormap> rgb_get_colormap();

/** Gets a "preferred visual" chosen by GdkRGB for rendering image data on the default screen.
 * In previous versions of GDK, this was the only visual GdkRGB could use for rendering. In current versions,
 * it's simply the visual GdkRGB would have chosen as the optimal one in those previous versions.
 * GdkRGB can now render to drawables with any visual.
 * @result The Gdk::Visual chosen by GdkRGB.
 */
Glib::RefPtr<Visual> rgb_get_visual();

/** Determines whether the visual is ditherable.
 * This function may be useful for presenting a user interface choice to the user about which dither mode is desired;
 * if the display is not ditherable, it may make sense to gray out or hide the corresponding UI widget.
 * result true if the visual is ditherable.
 */
bool rgb_ditherable();

} //namespace Gdk

#endif //_GDKMM_RGB_H

