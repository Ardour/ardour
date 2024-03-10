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

#ifndef __utils_h__
#define __utils_h__

#include <stdarg.h>
#include <stdint.h>

#include "aaf/AAFTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#define DIR_SEP '\\'
#define DIR_SEP_STR "\\"
#else
#define DIR_SEP '/'
#define DIR_SEP_STR "/"
#endif

#if defined(__linux__)
#include <limits.h>
#include <linux/limits.h>
#elif defined(__APPLE__)
#include <sys/syslimits.h>
#elif defined(_WIN32)
#include <windows.h> // MAX_PATH
#include <limits.h>
#include <wchar.h>
#endif

#define AAF_DIR_SEP '/'
#define AAF_DIR_SEP_STR "/"

#define IS_DIR_SEP(c) \
	((((c) == DIR_SEP) || ((c) == '/')))

#define IS_ANY_DIR_SEP(c) \
	((((c) == '/') || ((c) == '\\')))

#define ANSI_COLOR_RED(log) (((log)->ansicolor) ? "\x1b[38;5;124m" : "")
#define ANSI_COLOR_GREEN(log) (((log)->ansicolor) ? "\x1b[92m" : "")
#define ANSI_COLOR_YELLOW(log) (((log)->ansicolor) ? "\x1b[33m" : "")
#define ANSI_COLOR_ORANGE(log) (((log)->ansicolor) ? "\x1b[38;5;130m" : "")
#define ANSI_COLOR_BLUE(log) (((log)->ansicolor) ? "\x1b[34m" : "")
#define ANSI_COLOR_MAGENTA(log) (((log)->ansicolor) ? "\x1b[35m" : "")
#define ANSI_COLOR_CYAN(log) (((log)->ansicolor) ? "\x1b[38;5;81m" : "")
#define ANSI_COLOR_DARKGREY(log) (((log)->ansicolor) ? "\x1b[38;5;242m" : "")
#define ANSI_COLOR_BOLD(log) (((log)->ansicolor) ? "\x1b[1m" : "")
#define ANSI_COLOR_RESET(log) (((log)->ansicolor) ? (log->color_reset) ? log->color_reset : "\x1b[0m" : "")

#define TREE_LINE "\xe2\x94\x82"                               /* │ */
#define TREE_PADDED_LINE "\xe2\x94\x82\x20\x20"                /* │   */
#define TREE_ENTRY "\xe2\x94\x9c\xe2\x94\x80\xe2\x94\x80"      /* ├── */
#define TREE_LAST_ENTRY "\xe2\x94\x94\xe2\x94\x80\xe2\x94\x80" /* └── */

size_t
laaf_util_utf8strCharLen (const char* u8str);

char*
laaf_util_utf16Toutf8 (const uint16_t* u16str);

#ifdef _WIN32
wchar_t*
laaf_util_windows_utf8toutf16 (const char* str);
char*
laaf_util_windows_utf16toutf8 (const wchar_t* wstr);
#endif

int
laaf_util_file_exists (const char* filepath);

char*
laaf_util_clean_filename (char* filename);

int
laaf_util_is_fileext (const char* filepath, const char* ext);

char*
laaf_util_build_path (const char* sep, const char* first, ...);

char*
laaf_util_relative_path (const char* filepath, const char* refpath);

char*
laaf_util_absolute_path (const char* relpath);

char*
laaf_util_c99strdup (const char* src);

int
laaf_util_snprintf_realloc (char** str, size_t* size, size_t offset, const char* format, ...);

int
laaf_util_vsnprintf_realloc (char** str, size_t* size, size_t offset, const char* fmt, va_list args);

int
laaf_util_dump_hex (const unsigned char* stream, size_t stream_sz, char** buf, size_t* bufsz, size_t offset, const char* padding);

#ifdef __cplusplus
}
#endif

#endif // ! __utils_h__
