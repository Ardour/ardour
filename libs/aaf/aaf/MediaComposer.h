/*
 * Copyright (C) 2024 Adrien Gesta-Fline
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

#ifndef __MediaComposer_h__
#define __MediaComposer_h__

#include "aaf/AAFIParser.h"
#include "aaf/AAFIface.h"

#define AVID_MEDIA_COMPOSER_CURVE_TYPE_LINEAR 0
#define AVID_MEDIA_COMPOSER_CURVE_TYPE_EQUAL_POWER 1

int
mediaComposer_AAF (struct AAF_Iface* aafi);

#endif // !__MediaComposer_h__
