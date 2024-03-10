/*
 * Copyright (C) 2023-2024 Adrien Gesta-Fline
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

#ifndef URI_PARSER_H
#define URI_PARSER_H

#include "aaf/log.h"

#if defined(__linux__)
#include <limits.h>
#include <linux/limits.h>
#elif defined(__APPLE__)
#include <sys/syslimits.h>
#elif defined(_WIN32)
#include <windows.h> // MAX_PATH
#include <limits.h>
#endif

#define MAX_URI_LENGTH 64000

enum uri_option {

	URI_OPT_NONE = 0,

	URI_OPT_IGNORE_USERPASS = (1 << 0),
	URI_OPT_IGNORE_QUERY    = (1 << 1),
	URI_OPT_IGNORE_FRAGMENT = (1 << 2),

	URI_OPT_DECODE_HOSTNAME = (1 << 3),
	URI_OPT_DECODE_USERINFO = (1 << 4),
	URI_OPT_DECODE_USERPASS = (1 << 5),
	URI_OPT_DECODE_PATH     = (1 << 6),
	URI_OPT_DECODE_QUERY    = (1 << 7),
	URI_OPT_DECODE_FRAGMENT = (1 << 8)
};

#define URI_OPT_DECODE_ALL (  \
    URI_OPT_DECODE_HOSTNAME | \
    URI_OPT_DECODE_USERINFO | \
    URI_OPT_DECODE_USERPASS | \
    URI_OPT_DECODE_PATH |     \
    URI_OPT_DECODE_QUERY |    \
    URI_OPT_DECODE_FRAGMENT)

enum uri_type {

	URI_T_GUESSED_OS_LINUX   = (1 << 0),
	URI_T_GUESSED_OS_APPLE   = (1 << 1),
	URI_T_GUESSED_OS_WINDOWS = (1 << 2),

	URI_T_HOST_EMPTY   = (1 << 3),
	URI_T_HOST_IPV4    = (1 << 4),
	URI_T_HOST_IPV6    = (1 << 5),
	URI_T_HOST_REGNAME = (1 << 6),

	URI_T_LOCALHOST = (1 << 7),
};

#define URI_T_GUESSED_OS_MASK ( \
    URI_T_GUESSED_OS_LINUX |    \
    URI_T_GUESSED_OS_APPLE |    \
    URI_T_GUESSED_OS_WINDOWS)

#define URI_T_HOST_MASK ( \
    URI_T_HOST_EMPTY |    \
    URI_T_HOST_IPV4 |     \
    URI_T_HOST_IPV6 |     \
    URI_T_HOST_REGNAME)

enum uri_scheme_type {

	URI_SCHEME_T_UNKNOWN = 0,
	URI_SCHEME_T_AFP,
	URI_SCHEME_T_CIFS,
	URI_SCHEME_T_DATA,
	URI_SCHEME_T_DNS,
	URI_SCHEME_T_FILE,
	URI_SCHEME_T_FTP,
	URI_SCHEME_T_HTTP,
	URI_SCHEME_T_HTTPS,
	URI_SCHEME_T_IMAP,
	URI_SCHEME_T_IRC,
	URI_SCHEME_T_MAILTO,
	URI_SCHEME_T_NFS,
	URI_SCHEME_T_POP,
	URI_SCHEME_T_RTSP,
	URI_SCHEME_T_SFTP,
	URI_SCHEME_T_SIP,
	URI_SCHEME_T_SMB,
	URI_SCHEME_T_SSH,
	URI_SCHEME_T_TEL,
	URI_SCHEME_T_TELNET,
};

struct uri {
	char* scheme;
	char* authority;
	char* userinfo;
	char* user;
	char* pass;
	char* host;
	int   port;
	char* path;
	char* query;
	char* fragment;

	enum uri_scheme_type scheme_t;

	enum uri_option opts;
	enum uri_type   flags;
};

struct uri*
laaf_uri_parse (const char*, enum uri_option, struct aafLog* log);

void
laaf_uri_free (struct uri*);

#endif // ! URI_PARSER_H
