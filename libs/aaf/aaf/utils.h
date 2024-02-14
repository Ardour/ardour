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

#ifndef __utils_h__
#define __utils_h__

#include "aaf/AAFTypes.h"
#include <stdarg.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#define DIR_SEP '\\'
#define DIR_SEP_STR "\\"
/*
	 * swprintf() specific string format identifiers
	 * https://learn.microsoft.com/en-us/cpp/c-runtime-library/format-specification-syntax-printf-and-wprintf-functions?view=msvc-170#type
	 */
#define WPRIs L"S" // char*
#ifdef XBUILD_WIN
#define WPRIws L"s" // wchar_t*
#else
#define WPRIws L"ls" // wchar_t*
#endif
#else
#define DIR_SEP '/'
#define DIR_SEP_STR "/"
/*
	 * swprintf() specific string format identifiers
	 * https://learn.microsoft.com/en-us/cpp/c-runtime-library/format-specification-syntax-printf-and-wprintf-functions?view=msvc-170#type
	 */
#define WPRIs L"s"   // char*
#define WPRIws L"ls" // wchar_t*
#endif

#define IS_DIR_SEP(c) \
	((((c) == DIR_SEP) || ((c) == '/')))

#define ANSI_COLOR_RED(dbg) (((dbg)->ansicolor) ? "\033[38;5;124m" : "") //"\x1b[31m"
#define ANSI_COLOR_GREEN(dbg) (((dbg)->ansicolor) ? "\x1b[92m" : "")
#define ANSI_COLOR_YELLOW(dbg) (((dbg)->ansicolor) ? "\x1b[33m" : "") //"\x1b[93m"
#define ANSI_COLOR_ORANGE(dbg) (((dbg)->ansicolor) ? "\033[38;5;130m" : "")
#define ANSI_COLOR_BLUE(dbg) (((dbg)->ansicolor) ? "\x1b[34m" : "")
#define ANSI_COLOR_MAGENTA(dbg) (((dbg)->ansicolor) ? "\x1b[35m" : "")
#define ANSI_COLOR_CYAN(dbg) (((dbg)->ansicolor) ? "\033[38;5;81m" : "") //"\x1b[36m"
#define ANSI_COLOR_DARKGREY(dbg) (((dbg)->ansicolor) ? "\x1b[38;5;242m" : "")
#define ANSI_COLOR_BOLD(dbg) (((dbg)->ansicolor) ? "\x1b[1m" : "")
#define ANSI_COLOR_RESET(dbg) (((dbg)->ansicolor) ? "\x1b[0m" : "")

aafPosition_t
laaf_util_converUnit (aafPosition_t value, aafRational_t* valueEditRate, aafRational_t* destEditRate);

char*
laaf_util_wstr2str (const wchar_t* wstr);

wchar_t*
laaf_util_str2wstr (const char* str);

int
laaf_util_wstr_contains_nonlatin (const wchar_t* str);

char*
laaf_util_clean_filename (char* filename);

const char*
laaf_util_fop_get_file (const char* filepath);

int
laaf_util_fop_is_wstr_fileext (const wchar_t* filepath, const wchar_t* ext);

char*
laaf_util_build_path (const char* sep, const char* first, ...);

int
laaf_util_snprintf_realloc (char** str, int* size, size_t offset, const char* format, ...);

int
laaf_util_vsnprintf_realloc (char** str, int* size, int offset, const char* fmt, va_list* args);

char*
laaf_util_c99strdup (const char* src);

int
laaf_util_dump_hex (const unsigned char* stream, size_t stream_sz, char** buf, int* bufsz, int offset);

#ifdef __cplusplus
}
#endif

#endif // ! __utils_h__
