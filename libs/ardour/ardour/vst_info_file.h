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

LIBARDOUR_API extern void vstfx_free_info (VSTInfo *);
LIBARDOUR_API extern void vstfx_free_info_list (std::vector<VSTInfo *> *infos);

#ifdef LXVST_SUPPORT
LIBARDOUR_API extern std::vector<VSTInfo*> * vstfx_get_info_lx (char *);
#endif

#ifdef WINDOWS_VST_SUPPORT
LIBARDOUR_API extern std::vector<VSTInfo*> * vstfx_get_info_fst (char *);
#endif

#endif /* __vstfx_h__ */

