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

#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

#include "aaf/AAFCore.h"
#include "aaf/AAFIParser.h"
#include "aaf/AAFIface.h"
#include "aaf/LibCFB.h"

#include "aaf/log.h"
#include "aaf/utils.h"

/*
 * swprintf() specific string format identifiers
 * https://learn.microsoft.com/en-us/cpp/c-runtime-library/format-specification-syntax-printf-and-wprintf-functions?view=msvc-170#size-prefixes-for-printf-and-wprintf-format-type-specifiers
 */
#ifdef __MINGW32__
#define WPRIws L"ls" // wchar_t*
#define WPRIs L"s"   // char*
#else
#define WPRIws L"ls" // wchar_t*
#define WPRIs L"S"   // char*
#endif

struct aafLog*
laaf_new_log (void)
{
	struct aafLog* log = calloc (1, sizeof (struct aafLog));

	if (!log) {
		return NULL;
	}

	log->log_callback = &laaf_log_callback;
	log->fp           = stdout;
	log->ansicolor    = 0;

	return log;
}

void
laaf_free_log (struct aafLog* log)
{
	if (!log) {
		return;
	}

	free (log->_msg);
	free (log);
}

void
laaf_log_callback (struct aafLog* log, void* ctxdata, int libid, int type, const char* srcfile, const char* srcfunc, int lineno, const char* msg, void* user)
{
	AAF_Iface* aafi = NULL;
	AAF_Data*  aafd = NULL;
	CFB_Data*  cfbd = NULL;

	const char* lib     = "";
	const char* typestr = "";
	const char* color   = "";

	if (log->fp == NULL) {
		LOG_BUFFER_RESET (log);
		return;
	}

	switch (libid) {
		case LOG_SRC_ID_LIB_CFB:
			lib  = "libCFB";
			aafi = (AAF_Iface*)ctxdata;
			break;
		case LOG_SRC_ID_AAF_CORE:
			lib  = "AAFCore";
			aafd = (AAF_Data*)ctxdata;
			break;
		case LOG_SRC_ID_AAF_IFACE:
			lib  = "AAFIface";
			cfbd = (CFB_Data*)ctxdata;
			break;
		case LOG_SRC_ID_TRACE:
			lib  = "trace";
			aafi = (AAF_Iface*)ctxdata;
			break;
		case LOG_SRC_ID_DUMP:
			lib = "dump";
			break;
	}

	switch (type) {
		case VERB_SUCCESS:
			typestr = "success";
			color   = ANSI_COLOR_GREEN (log);
			break;
		case VERB_ERROR:
			typestr = " error ";
			color   = ANSI_COLOR_RED (log);
			break;
		case VERB_WARNING:
			typestr = "warning";
			color   = ANSI_COLOR_YELLOW (log);
			break;
		case VERB_DEBUG:
			typestr = " debug ";
			color   = ANSI_COLOR_DARKGREY (log);
			break;
	}

	const char* eol = "";

	if (libid != LOG_SRC_ID_TRACE && libid != LOG_SRC_ID_DUMP) {
#ifdef __MINGW32__
		fwprintf (log->fp, L"[%" WPRIs "%" WPRIs "%" WPRIs "] %" WPRIs "%" WPRIs ":%i in %" WPRIs "()%" WPRIs " : ",
		          color,
		          typestr,
		          ANSI_COLOR_RESET (log),
		          ANSI_COLOR_DARKGREY (log),
		          srcfile,
		          lineno,
		          srcfunc,
		          ANSI_COLOR_RESET (log));
#else
		fprintf (log->fp, "[%s%s%s] %s%s:%i in %s()%s : ",
		         color,
		         typestr,
		         ANSI_COLOR_RESET (log),
		         ANSI_COLOR_DARKGREY (log),
		         srcfile,
		         lineno,
		         srcfunc,
		         ANSI_COLOR_RESET (log));
#endif
	}

	if (libid != LOG_SRC_ID_DUMP) {
		eol = "\n";
	}

#ifdef __MINGW32__
	wchar_t* tmp = laaf_util_windows_utf8toutf16 (msg);
	if (!tmp) {
		return;
	}
	fwprintf (log->fp, L"%" WPRIws "%s", tmp, eol);
	free (tmp);
#else
	fprintf (log->fp, "%s%s", msg, eol);
#endif

	fflush (log->fp);

	LOG_BUFFER_RESET (log);

	/* avoids -Wunused-parameter -Wunused-but-set-variable */
	(void)aafi;
	(void)aafd;
	(void)cfbd;
	(void)lib;
	(void)user;
}

void
laaf_write_log (struct aafLog* log, void* ctxdata, enum log_source_id lib, enum verbosityLevel_e type, const char* srcfile, const char* srcfunc, int srcline, const char* format, ...)
{
	if (!log) {
		return;
	}

	if (!log->log_callback) {
		return;
	}

	if (type != VERB_SUCCESS && (log->verb == VERB_QUIET || type > log->verb)) {
		return;
	}

	va_list ap;

	int    rc      = 0;
	size_t msgsize = 0;

	va_start (ap, format);

#ifdef _WIN32
	/* https://stackoverflow.com/a/4116308 */
	FILE* dummy = fopen ("NUL", "wb");

	if (!dummy) {
		// fprintf( stderr, "Could not fopen() dummy null file\n" );
		return;
	}

	rc = vfprintf (dummy, format, ap);

	fclose (dummy);

	if (rc < 0) {
		// fprintf( stderr, "vfwprintf() error : %s\n", strerror(errno) );
		va_end (ap);
		return;
	}

	rc++;
	rc *= (int)sizeof (wchar_t);

#else
	rc = vsnprintf (NULL, 0, format, ap);

	if (rc < 0) {
		// fprintf( stderr, "vsnprintf() error : %s\n", strerror(errno) );
		va_end (ap);
		return;
	}

	rc++;
#endif

	va_end (ap);

	msgsize = (size_t)rc;

	if (log->_msg_pos) {
		log->_previous_pos = log->_msg_pos;
		log->_previous_msg = laaf_util_c99strdup (log->_msg);
		if (!log->_previous_msg) {
			// fprintf( stderr, "Out of memory\n" );
			return;
		}
	}

	va_start (ap, format);

	if (msgsize >= log->_msg_size) {
		char* msgtmp = realloc (log->_msg, (msgsize * sizeof (char)));

		if (!msgtmp) {
			// fprintf( stderr, "Out of memory\n" );
			va_end (ap);
			return;
		}

		log->_msg      = msgtmp;
		log->_msg_size = msgsize;
	}

	rc = vsnprintf (log->_msg, log->_msg_size, format, ap);

	if (rc < 0 || (size_t)rc >= log->_msg_size) {
		// fprintf( stderr, "vsnprintf() error\n" );
		va_end (ap);
		return;
	}

	log->log_callback (log, (void*)ctxdata, lib, type, srcfile, srcfunc, srcline, log->_msg, log->user);

	va_end (ap);

	if (log->_previous_pos) {
		log->_msg_pos = log->_previous_pos;
		strcpy (log->_msg, log->_previous_msg);
		free (log->_previous_msg);
		log->_previous_msg = NULL;
		log->_previous_pos = 0;
	}
}
