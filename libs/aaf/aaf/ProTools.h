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

#ifndef __ProTools_h__
#define __ProTools_h__

#include "aaf/AAFIParser.h"
#include "aaf/AAFIface.h"

enum protools_options {
	AAFI_PROTOOLS_OPT_REMOVE_SAMPLE_ACCURATE_EDIT = 1 << 0,
	AAFI_PROTOOLS_OPT_REPLACE_CLIP_FADES          = 1 << 1,
};

#define PROTOOLS_ALL_OPT (AAFI_PROTOOLS_OPT_REMOVE_SAMPLE_ACCURATE_EDIT | AAFI_PROTOOLS_OPT_REPLACE_CLIP_FADES)

int
protools_AAF (struct AAF_Iface* aafi);

int
protools_post_processing (AAF_Iface* aafi);

#endif // ! __ProTools_h__
