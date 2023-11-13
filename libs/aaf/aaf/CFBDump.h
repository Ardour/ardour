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

#ifndef __CFBDump_h__
#define __CFBDump_h__

#include <stdio.h>
#include <wchar.h>

#include "aaf/LibCFB.h"

void
cfb_dump_node (CFB_Data* cfbd, cfbNode* node, int print_stream);

void
cfb_dump_nodePath (CFB_Data* cfbd, const wchar_t* path, int print_stream);

void
cfb_dump_nodeStream (CFB_Data* cfbd, cfbNode* node);

void
cfb_dump_nodePathStream (CFB_Data* cfbd, const wchar_t* path);

void
cfb_dump_nodePaths (CFB_Data* cfbd, uint32_t prevPath, char* strArray[], uint32_t* str_i, cfbNode* node);

void
cfb_dump_header (CFB_Data* cfbd);

void
cfb_dump_FAT (CFB_Data* cfbd);

void
cfb_dump_MiniFAT (CFB_Data* cfbd);

void
cfb_dump_DiFAT (CFB_Data* cfbd);

#endif // !__CFBDump_h__
