/*
 * Copyright (C) 2014-2015 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __gtk2_ardour_bundle_env_h__
#define __gtk2_ardour_bundle_env_h__

#include <string>

/** This function must do whatever is necessary to create the right runtime
 * environment for the GTK2 version of ardour, on a per-platform basis.
 */

void fixup_bundle_environment (int, char* [], std::string & localedir);

/** Load any fonts required by the GTK2 version of ardour, on a per-platform
 * basis.
 */

void load_custom_fonts();

#endif /* __gtk2_ardour_bundle_env_h__ */
