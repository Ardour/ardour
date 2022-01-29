/*
 * Copyright (C) 2022 Robin Gareus <robin@gareus.org>
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

#ifndef _ardour_clip_library_h_
#define _ardour_clip_library_h_

#ifdef WAF_BUILD
#include "libardour-config.h"
#endif

#include <string>

#include "boost/shared_ptr.hpp"

#include "pbd/signals.h"

#include "ardour/libardour_visibility.h"

namespace ARDOUR {

class Region;

extern LIBARDOUR_API PBD::Signal2<void, std::string, void*> LibraryClipAdded;

LIBARDOUR_API std::string clip_library_dir (bool create_if_missing = false);
LIBARDOUR_API bool export_to_clip_library (boost::shared_ptr<Region> r, void* src = NULL);

} // namespace ARDOUR

#endif /* _ardour_clip_library_h_*/
