/*
 * Copyright (C) 2017-2023 Adrien Gesta-Fline
 *
 * This file is part of libAAF.
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

#ifndef __AAFIAudioFiles_h__
#define __AAFIAudioFiles_h__

#include <wchar.h>

#include "aaf/AAFIface.h"

wchar_t*
aafi_locate_external_essence_file (AAF_Iface* aafi, const wchar_t* original_file_path, const char* search_location);

int
aafi_extract_audio_essence (AAF_Iface* aafi, aafiAudioEssence* audioEssence, const char* outfilepath, const wchar_t* forcedFileName);

int
aafi_parse_audio_essence (AAF_Iface* aafi, aafiAudioEssence* audioEssence);

#endif // !__AAFIAudioFiles_h__
