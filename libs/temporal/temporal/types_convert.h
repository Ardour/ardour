/*
 * Copyright (C) 2020 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef TEMPORAL_TYPES_CONVERT_H
#define TEMPORAL_TYPES_CONVERT_H

#ifdef COMPILER_MSVC
#pragma warning(disable:4101)
#endif

#include "pbd/enum_convert.h"

#include "temporal/types.h"

namespace PBD {

DEFINE_ENUM_CONVERT(Temporal::TimeDomain)

} // namespace PBD

#endif // TEMPORAL_TYPES_CONVERT_H
