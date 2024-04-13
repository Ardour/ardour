/*
 * Copyright (C) 2017-2024 Adrien Gesta-Fline
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

#ifndef __AAFDump_h__
#define __AAFDump_h__

#include "aaf/AAFCore.h"
#include "aaf/AAFTypes.h"
#include "aaf/LibCFB.h"

void
aaf_dump_Header (AAF_Data* aafd, const char* padding);

void
aaf_dump_Identification (AAF_Data* aafd, const char* padding);

void
aaf_dump_rawProperties (AAF_Data* aafd, aafByte_t* propStream, const char* padding);

void
aaf_dump_ObjectProperty (AAF_Data* aafd, aafProperty* Prop, const char* padding);

void
aaf_dump_ObjectProperties (AAF_Data* aafd, aafObject* Obj, const char* padding);

void
aaf_dump_TaggedValueSet (AAF_Data* aafd, aafObject* ObjCollection, const char* padding);

void
aaf_dump_nodeStreamProperties (AAF_Data* aafd, cfbNode* node, const char* padding);

void
aaf_dump_MetaDictionary (AAF_Data* aafd, const char* padding);

void
aaf_dump_Classes (AAF_Data* aafd, const char* padding);

#endif // ! __AAFDump_h__
