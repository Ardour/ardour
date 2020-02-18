/*
 * Copyright (C) 2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2014 Paul Davis <paul@linuxaudiosystems.com>
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

#include "ardour/libardour_visibility.h"

/** These are GQuarks for a subset of UI operations.  We use these
 *  so that the undo system can be queried to find out what operations
 *  are currently in progress, by calling Session::current_operations().
 *
 *  It is only necessary to add a GQuark here if you subsequently want
 *  to be able to find out that a particular operation is in progress.
 */

namespace Operations {

	LIBARDOUR_API extern GQuark capture;
	LIBARDOUR_API extern GQuark paste;
	LIBARDOUR_API extern GQuark duplicate_region;
	LIBARDOUR_API extern GQuark insert_file;
	LIBARDOUR_API extern GQuark insert_region;
	LIBARDOUR_API extern GQuark drag_region_brush;
	LIBARDOUR_API extern GQuark region_drag;
	LIBARDOUR_API extern GQuark selection_grab;
	LIBARDOUR_API extern GQuark region_fill;
	LIBARDOUR_API extern GQuark fill_selection;
	LIBARDOUR_API extern GQuark create_region;
	LIBARDOUR_API extern GQuark region_copy;
	LIBARDOUR_API extern GQuark fixed_time_region_copy;

        LIBARDOUR_API void make_operations_quarks ();
};

