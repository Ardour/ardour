/*
 * Copyright (C) 2007-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2007 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2008-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2013-2018 Robin Gareus <robin@gareus.org>
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


#ifndef __ardour_directory_names_h__
#define __ardour_directory_names_h__

#include "ardour/libardour_visibility.h"

namespace ARDOUR {

	LIBARDOUR_API extern const char* const old_sound_dir_name;
	LIBARDOUR_API extern const char* const sound_dir_name;
	LIBARDOUR_API extern const char* const ladspa_dir_name;
	LIBARDOUR_API extern const char* const midi_dir_name;
	LIBARDOUR_API extern const char* const midi_patch_dir_name;
	LIBARDOUR_API extern const char* const video_dir_name;
	LIBARDOUR_API extern const char* const dead_dir_name;
	LIBARDOUR_API extern const char* const interchange_dir_name;
	LIBARDOUR_API extern const char* const peak_dir_name;
	LIBARDOUR_API extern const char* const export_dir_name;
	LIBARDOUR_API extern const char* const backup_dir_name;
	LIBARDOUR_API extern const char* const export_formats_dir_name;
	LIBARDOUR_API extern const char* const plugin_metadata_dir_name;
	LIBARDOUR_API extern const char* const templates_dir_name;
	LIBARDOUR_API extern const char* const route_templates_dir_name;
	LIBARDOUR_API extern const char* const surfaces_dir_name;
	LIBARDOUR_API extern const char* const theme_dir_name;
	LIBARDOUR_API extern const char* const ladspa_dir_name;
	LIBARDOUR_API extern const char* const panner_dir_name;
	LIBARDOUR_API extern const char* const backend_dir_name;
	LIBARDOUR_API extern const char* const automation_dir_name;
	LIBARDOUR_API extern const char* const analysis_dir_name;
	LIBARDOUR_API extern const char* const plugins_dir_name;
	LIBARDOUR_API extern const char* const externals_dir_name;
	LIBARDOUR_API extern const char* const lua_dir_name;
	LIBARDOUR_API extern const char* const media_dir_name;

};

#endif
