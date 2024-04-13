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

#ifndef laaf_log_h__
#define laaf_log_h__

#include <errno.h>
#include <inttypes.h> // PRIi64 PRIu64
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define __FILENAME__ (strrchr (__FILE__, '/') ? strrchr (__FILE__, '/') + 1 : __FILE__)

enum log_source_id {
	LOG_SRC_ID_LIB_CFB,
	LOG_SRC_ID_AAF_CORE,
	LOG_SRC_ID_AAF_IFACE,
	LOG_SRC_ID_TRACE,
	LOG_SRC_ID_DUMP
};

typedef enum verbosityLevel_e {
	VERB_QUIET = 0,
	VERB_ERROR,
	VERB_WARNING,
	VERB_DEBUG,
	MAX_VERB
} verbosityLevel_e;

#define VERB_SUCCESS 99

struct aafLog {
	void (*log_callback) (struct aafLog* log, void* ctxdata, int lib, int type, const char* srcfile, const char* srcfunc, int lineno, const char* msg, void* user);

	FILE*            fp;
	verbosityLevel_e verb;
	int              ansicolor;

	const char* color_reset;

	char*  _msg;
	size_t _msg_size;
	size_t _msg_pos;

	char*  _previous_msg;
	size_t _previous_pos;

	int _tmp_msg_pos;

	void* user;
};

#define AAF_LOG(log, ctxdata, lib, type, ...) \
	laaf_write_log (log, ctxdata, lib, type, __FILENAME__, __func__, __LINE__, __VA_ARGS__)

#define LOG_BUFFER_WRITE(log, ...)                                                                                \
	log->_tmp_msg_pos = laaf_util_snprintf_realloc (&log->_msg, &log->_msg_size, log->_msg_pos, __VA_ARGS__); \
	log->_msg_pos += (log->_tmp_msg_pos < 0) ? 0 : (size_t)log->_tmp_msg_pos;

#define LOG_BUFFER_RESET(log) \
	log->_msg_pos = 0;

struct aafLog*
laaf_new_log (void);

void
laaf_free_log (struct aafLog* log);

void
laaf_log_callback (struct aafLog* log, void* ctxdata, int lib, int type, const char* srcfile, const char* srcfunc, int lineno, const char* msg, void* user);

void
laaf_write_log (struct aafLog* log, void* ctxdata, enum log_source_id lib, enum verbosityLevel_e type, const char* srcfile, const char* srcfunc, int srcline, const char* format, ...);

#endif // !laaf_log_h__
