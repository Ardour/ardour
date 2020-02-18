/*
 * Copyright (C) 2007 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2008-2015 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
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


#ifndef __ardour_filename_extensions_h__
#define __ardour_filename_extensions_h__

#include "ardour/libardour_visibility.h"

namespace ARDOUR {

	LIBARDOUR_API extern const char* const template_suffix;
	LIBARDOUR_API extern const char* const statefile_suffix;
	LIBARDOUR_API extern const char* const pending_suffix;
	LIBARDOUR_API extern const char* const peakfile_suffix;
	LIBARDOUR_API extern const char* const backup_suffix;
	LIBARDOUR_API extern const char* const temp_suffix;
	LIBARDOUR_API extern const char* const history_suffix;
	LIBARDOUR_API extern const char* const export_preset_suffix;
	LIBARDOUR_API extern const char* const export_format_suffix;
	LIBARDOUR_API extern const char* const session_archive_suffix;
	LIBARDOUR_API extern const char* const template_archive_suffix;

}

#endif
