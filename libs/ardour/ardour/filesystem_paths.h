/*
 * Copyright (C) 2007-2015 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2008-2015 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2014-2018 Robin Gareus <robin@gareus.org>
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

#ifndef ARDOUR_FILESYSTEM_PATHS_INCLUDED
#define ARDOUR_FILESYSTEM_PATHS_INCLUDED

#include "pbd/search_path.h"

#include "ardour/libardour_visibility.h"

namespace ARDOUR {

	/**
	 * @return the path to the directory used to store user specific
	 * configuration files for the given @param version of the program.
	 * If @param version is negative, the build-time string PROGRAM_VERSION
	 * will be used to determine the version number.
	 *
	 * @post user_config_directory() exists IF version was negative.
	 *
	 *
	 */
        LIBARDOUR_API std::string user_config_directory (int version = -1);

	/**
	 * @return the path to the directory used to store user specific
	 * caches (e.g. plugin indices, blacklist/whitelist)
	 * it defaults to XDG_CACHE_HOME
	 */
	LIBARDOUR_API std::string user_cache_directory (std::string cachename = "");

	/**
	 * @return the path used to store a persistent indication
	 * that the given version of the program has been used before.
	 *
	 * @param version is the version to check for. If unspecified,
	 * it defaults to the current (build-time) version of the program.
	 */
	LIBARDOUR_API std::string been_here_before_path (int version = -1);

	/**
	 * @return the path to the directory that contains the system wide ardour
	 * modules.
	 */
	LIBARDOUR_API std::string ardour_dll_directory ();

	/**
	 * @return the search path to be used when looking for per-system
	 * configuration files. This may include user configuration files.
	 */
	LIBARDOUR_API PBD::Searchpath ardour_config_search_path ();

	/**
	 * @return the search path to be used when looking for data files
	 * that could be shared by systems (h/w and configuration independent
	 * files, such as icons, XML files, etc)
	 */
	LIBARDOUR_API PBD::Searchpath ardour_data_search_path ();

#ifdef PLATFORM_WINDOWS
	/**
	 * @return our 'Windows' search path ( corresponds to <install_dir>/share/ardour3 )
	 */
	LIBARDOUR_API PBD::Searchpath windows_search_path ();

	/**
	 * @return Convenience function that calls
	 * g_win32_get_package_installation_directory_of_module but returns a
	 * std::string
	 */
	LIBARDOUR_API std::string windows_package_directory_path ();
#endif

	namespace ArdourVideoToolPaths {

		LIBARDOUR_API bool harvid_exe (std::string &harvid_exe);
		LIBARDOUR_API bool xjadeo_exe (std::string &xjadeo_exe);
		LIBARDOUR_API bool transcoder_exe (std::string &ffmpeg_exe, std::string &ffprobe_exe);
	};

} // namespace ARDOUR

#endif
