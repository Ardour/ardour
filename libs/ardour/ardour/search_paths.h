/*
 * Copyright (C) 2014-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2016-2018 Robin Gareus <robin@gareus.org>
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

#ifndef __libardour_search_paths_h__
#define __libardour_search_paths_h__

#include "ardour/libardour_visibility.h"

#include "pbd/search_path.h"

namespace ARDOUR {

	LIBARDOUR_API const char *vst_search_path ();

	/**
	 * return a SearchPath containing directories in which to look for
	 * backend plugins.
	 *
	 * If ARDOUR_BACKEND_PATH is defined then the SearchPath returned
	 * will contain only those directories specified in it, otherwise it will
	 * contain the user and system directories which may contain audio/MIDI
	 * backends.
	 */
	LIBARDOUR_API PBD::Searchpath backend_search_path ();

	/**
	 * return a Searchpath containing directories in which to look for
	 * control surface plugins.
	 *
	 * If ARDOUR_SURFACES_PATH is defined then the Searchpath returned
	 * will contain only those directories specified in it, otherwise it will
	 * contain the user and system directories which may contain control
	 * surface plugins.
	 */
	LIBARDOUR_API PBD::Searchpath control_protocol_search_path ();

	/**
	 * return a Searchpath containing directories in which to look for
	 * export_formats.
	 */
	LIBARDOUR_API PBD::Searchpath export_formats_search_path ();

	/**
	 * return a Searchpath containing directories in which to look for
	 * LADSPA plugins.
	 *
	 * If LADSPA_PATH is defined then the Searchpath returned
	 * will contain the directories specified in it as well as the
	 * user and system directories.
	 */
	LIBARDOUR_API PBD::Searchpath ladspa_search_path ();

	/**
	 * return a Searchpath containing directories in which to look for
	 * lv2 plugins.
	 */
	LIBARDOUR_API PBD::Searchpath lv2_bundled_search_path ();

	/**
	 * return a Searchpath containing directories in which to look for
	 * MIDI patch files ("*.midnam") aka MIDNAM files
	 *
	 * If ARDOUR_MIDI_PATCH_PATH is defined then the Searchpath returned
	 * will contain only those directories specified in it, otherwise it will
	 * contain the user and system directories.
	 */
	LIBARDOUR_API PBD::Searchpath midi_patch_search_path ();

	/**
	 * return a Searchpath containing directories in which to look for
	 * theme files.
	 *
	 * If ARDOUR_THEMES_PATH is defined then the Searchpath returned
	 * will contain only those directories specified in it, otherwise it will
	 * contain the user and system directories
	 */
	LIBARDOUR_API PBD::Searchpath theme_search_path ();


	/**
	 * return a Searchpath containing directories in which to look for
	 * panner plugins.
	 *
	 * If ARDOUR_PANNER_PATH is defined then the Searchpath returned
	 * will contain only those directories specified in it, otherwise it will
	 * contain the user and system directories which may contain panner
	 * plugins.
	 */
	LIBARDOUR_API PBD::Searchpath panner_search_path ();

	/**
	 * return a Searchpath containing directories in which to look for
	 * plugin metadata ( tags and favorite/hidden status ).
	 */
	LIBARDOUR_API PBD::Searchpath plugin_metadata_search_path ();

	/**
	 * return a Searchpath containing directories in which to look for
	 * route templates.
	 */
	LIBARDOUR_API PBD::Searchpath route_template_search_path ();

	/**
	 * return a Searchpath containing directories in which to look for
	 * other templates.
	 */
	LIBARDOUR_API PBD::Searchpath template_search_path ();


	/**
	 * return a Searchpath containing directories in which to look for
	 * lua scripts
	 */
	LIBARDOUR_API PBD::Searchpath lua_search_path ();


} // namespace ARDOUR

#endif /* __libardour_search_paths_h__ */
