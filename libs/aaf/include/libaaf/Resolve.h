/*
 * Copyright (C) 2023 Adrien Gesta-Fline
 *
 * This file is part of libAAF.
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

#ifndef __Resolve_h__
#define __Resolve_h__

#include <libaaf/AAFIParser.h>
#include <libaaf/AAFIface.h>

enum resolve_options {
  RESOLVE_INCLUDE_DISABLED_CLIPS = 1 << 0,
};

#define RESOLVE_ALL (RESOLVE_INCLUDE_DISABLED_CLIPS)

int resolve_AAF(struct AAF_Iface *aafi);

int resolve_parse_aafObject_Selector(struct AAF_Iface *aafi,
                                     aafObject *Selector, td *__ptd);

int resolve_parse_aafObject_DescriptiveMarker(struct AAF_Iface *aafi,
                                              aafObject *DescriptiveMarker,
                                              td *__ptd);

#endif // !__Resolve_h__
