/*
 *  Copyright (C) 2002 Steve Harris <steve@plugin.org.uk>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef GDITHER_TYPES_H
#define GDITHER_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GDitherNone = 0,
    GDitherRect,
    GDitherTri,
    GDitherShaped
} GDitherType;

typedef enum {
    GDither8bit = 8,
    GDither16bit = 16,
    GDither32bit = 32,
    GDitherFloat = 25,
    GDitherDouble = 54,
    GDitherPerformanceTest = 23
} GDitherSize;

typedef void *GDither;

#ifdef __cplusplus
}
#endif

#endif
