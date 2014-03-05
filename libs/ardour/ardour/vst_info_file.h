/*
    Copyright (C) 2012-2014 Paul Davis
    Author: Robin Gareus <robin@gareus.org>

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

#ifndef __vst_info_file_h__
#define __vst_info_file_h__

#include "ardour/libardour_visibility.h"
#include "ardour/vst_types.h"
#include <vector>

#ifndef VST_SCANNER_APP
namespace ARDOUR {
#endif

enum VSTScanMode {
	VST_SCAN_CACHE_ONLY,
	VST_SCAN_USE_APP,
	VST_SCAN_INTERNAL
};

LIBARDOUR_API extern std::string get_personal_vst_info_cache_dir ();
LIBARDOUR_API extern std::string get_personal_vst_blacklist_dir ();
LIBARDOUR_API extern void vstfx_free_info_list (std::vector<VSTInfo *> *infos);

#ifdef LXVST_SUPPORT
LIBARDOUR_API extern std::vector<VSTInfo*> * vstfx_get_info_lx (char *, enum VSTScanMode mode = VST_SCAN_USE_APP);
#endif

#ifdef WINDOWS_VST_SUPPORT
LIBARDOUR_API extern std::vector<VSTInfo*> * vstfx_get_info_fst (char *, enum VSTScanMode mode = VST_SCAN_USE_APP);
#endif

#ifndef VST_SCANNER_APP
} // namespace
#endif

#endif /* __vstfx_h__ */

