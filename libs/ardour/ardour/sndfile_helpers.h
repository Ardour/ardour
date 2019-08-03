/*
 * Copyright (C) 2005-2006 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006-2015 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2008-2009 David Robillard <d@drobilla.net>
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

#ifndef __sndfile_helpers_h__
#define __sndfile_helpers_h__

#include <string>
#include <stdint.h>

#include "ardour/types.h"

// Use this define when initializing arrarys for use in sndfile_*_format()
#define SNDFILE_STR_LENGTH 32

#define SNDFILE_HEADER_FORMATS 7
extern const char * const sndfile_header_formats_strings[SNDFILE_HEADER_FORMATS+1];
extern const char * const sndfile_file_endings_strings[SNDFILE_HEADER_FORMATS+1];

extern int sndfile_header_formats[SNDFILE_HEADER_FORMATS];

#define SNDFILE_BITDEPTH_FORMATS 5
extern const char * const sndfile_bitdepth_formats_strings[SNDFILE_BITDEPTH_FORMATS+1];

extern int sndfile_bitdepth_formats[SNDFILE_BITDEPTH_FORMATS];

#define SNDFILE_ENDIAN_FORMATS 2
extern const char * const sndfile_endian_formats_strings[SNDFILE_ENDIAN_FORMATS+1];

extern int sndfile_endian_formats[SNDFILE_ENDIAN_FORMATS];

int sndfile_bitdepth_format_by_index (int);
int sndfile_header_format_by_index (int);
int sndfile_endian_format_by_index (int);

int sndfile_data_width (int format);

// It'd be nice if libsndfile did this for us
std::string sndfile_major_format (int);
std::string sndfile_minor_format (int);

#endif /* __sndfile_helpers_h__ */
