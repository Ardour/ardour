/*
    Copyright (C) 2009 John Emmas

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/
#ifndef __platform_fallback_folders_h__
#define __platform_fallback_folders_h__

#include <pbd/msvc_pbd.h>
#include <glib/gtypes.h>

#ifdef PLATFORM_WINDOWS // Would not be relevant for Cygwin!!
	LIBPBD_API gchar* PBD_APICALLTYPE get_win_special_folder (int csidl);
#endif

namespace PBD {

	typedef enum fallback_folder_t {
		FOLDER_LOCALE,
		FOLDER_GTK,
		FOLDER_CONFIG,
		FOLDER_ARDOUR,
		FOLDER_MODULE,
		FOLDER_DATA,
		FOLDER_ICONS,
		FOLDER_PIXMAPS,
		FOLDER_CONTROL_SURFACES,
		FOLDER_VAMP,
		FOLDER_LADSPA,
		FOLDER_VST,
		FOLDER_BUNDLED_LV2,
		FALLBACK_FOLDER_MAX
	};

#ifdef __cplusplus
extern "C" {
#endif	/* __cplusplus */

	LIBPBD_API G_CONST_RETURN gchar* PBD_APICALLTYPE get_platform_fallback_folder (PBD::fallback_folder_t index);
	LIBPBD_API G_CONST_RETURN gchar* G_CONST_RETURN * PBD_APICALLTYPE alloc_platform_fallback_folders ();
	LIBPBD_API void PBD_APICALLTYPE free_platform_fallback_folders ();

#ifdef __cplusplus
}		/* extern "C" */
#endif	/* __cplusplus */

}  // namespace PBD

#endif /* __platform_fallback_folders_h__ */
