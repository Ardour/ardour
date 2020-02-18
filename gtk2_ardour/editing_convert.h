/*
 * Copyright (C) 2016 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2018 Ben Loftis <ben@harrisonconsoles.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef GTK_ARDOUR_EDITING_CONVERT_H
#define GTK_ARDOUR_EDITING_CONVERT_H

#include "pbd/enum_convert.h"

#include "editing.h"

namespace PBD {

DEFINE_ENUM_CONVERT(Editing::ZoomFocus)
DEFINE_ENUM_CONVERT(Editing::GridType)
DEFINE_ENUM_CONVERT(Editing::SnapMode)
DEFINE_ENUM_CONVERT(Editing::EditPoint)
DEFINE_ENUM_CONVERT(Editing::RegionListSortType)
DEFINE_ENUM_CONVERT(Editing::MouseMode)

} // namespace PBD

#endif // GTK_ARDOUR_EDITING_CONVERT_H
