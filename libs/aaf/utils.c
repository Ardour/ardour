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

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#if defined(__linux__)
#include <linux/limits.h>
#include <arpa/inet.h>
#include <mntent.h>
#include <unistd.h> /* access() */
#elif defined(__APPLE__)
#include <sys/syslimits.h>
#include <unistd.h> /* access() */
#elif defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#include <limits.h>
#define R_OK 4 /* Test for read permission.  */
#define W_OK 2 /* Test for write permission.  */
#define F_OK 0 /* Test for existence.  */
#ifndef _MSC_VER
#include <unistd.h> // access()
#endif
#endif

#include "aaf/utils.h"

#define BUILD_PATH_DEFAULT_BUF_SIZE 1024

static int
utf8CodeLen (const uint16_t* u16Code);
static int
utf16CodeLen (const uint16_t* u16Code);
static int
utf16CodeToUTF8 (const uint16_t* u16Code, char* u8Code, int* u16Len, int* u8Len);
static long
utf8strLen (const uint16_t* u16str);

#ifdef _WIN32

wchar_t*
laaf_util_windows_utf8toutf16 (const char* str)
{
	if (!str) {
		return NULL;
	}

	int needed = MultiByteToWideChar (CP_UTF8, 0, str, -1, NULL, 0);

	if (needed == 0) {
		return NULL;
	}

	wchar_t* wbuf = malloc ((size_t)needed * sizeof (wchar_t));

	if (!wbuf) {
		return NULL;
	}

	int rc = MultiByteToWideChar (CP_UTF8, 0, str, -1, wbuf, needed);

	if (rc == 0) {
		free (wbuf);
		return NULL;
	}

	return wbuf;
}

char*
laaf_util_windows_utf16toutf8 (const wchar_t* wstr)
{
	if (!wstr) {
		return NULL;
	}

	int needed = WideCharToMultiByte (CP_UTF8, 0, wstr, -1, NULL, 0, 0, 0);

	if (needed == 0) {
		return NULL;
	}

	char* buf = malloc ((size_t)needed * sizeof (char));

	if (!buf) {
		return NULL;
	}

	int rc = WideCharToMultiByte (CP_UTF8, 0, wstr, -1, buf, needed, 0, 0);

	if (rc == 0) {
		free (buf);
		return NULL;
	}

	return buf;
}

#endif

char*
laaf_util_clean_filename (char* fname)
{
	/*
	 * sanitize file/dir name
	 * https://stackoverflow.com/a/31976060
	 */
	if (!fname) {
		return NULL;
	}

	char* p = fname;

	while (*p) {
		if (*p == '/' ||
		    *p == '<' ||
		    *p == '>' ||
		    *p == ':' ||
		    *p == '"' ||
		    *p == '|' ||
		    *p == '?' ||
		    *p == '*' ||
		    *p == '\\' ||
		    (*p > 0 && *p < 0x20)) {
			*p = '_';
		}
		p++;
	}

	/* windows filenames can't end with ' ' or '.' */

	p = fname + strlen (fname) - 1;

	while (*p && (*p == ' ' || *p == '.')) {
		*p = '\0';
		p--;
	}

	if (*fname == '\0')
		return NULL;

	return fname;
}

int
laaf_util_is_fileext (const char* filepath, const char* ext)
{
	if (!filepath || !ext) {
		return 0;
	}

	const char* end    = filepath + strlen (filepath);
	size_t      extlen = 0;

	while (end > filepath && (*end) != '.') {
		--end;
		extlen++;
	}

	if ((*end) == '.') {
		end++;
		extlen--;
	}

	if (!extlen || extlen != strlen (ext)) {
		return 0;
	}

	for (size_t i = 0; i < extlen; i++) {
		if (tolower (*(end + i)) != tolower (*(ext + i))) {
			return 0;
		}
	}

	return 1;
}

char*
laaf_util_build_path (const char* sep, const char* first, ...)
{
	char* str = malloc (BUILD_PATH_DEFAULT_BUF_SIZE);

	if (!str) {
		return NULL;
	}

	size_t len    = BUILD_PATH_DEFAULT_BUF_SIZE;
	size_t offset = 0;

	va_list args;

	if (!sep) {
		sep = DIR_SEP_STR;
	}

	int element_count = 0;

	va_start (args, first);

	const char* arg = first;

	do {
		size_t arglen          = strlen (arg);
		size_t argstart        = 0;
		int    has_leading_sep = 0;

		/* trim leading DIR_SEP */
		for (int i = 0; arg[i] != 0x00; i++) {
			if (IS_ANY_DIR_SEP (arg[i])) {
				has_leading_sep = 1;
				argstart++;
			} else {
				break;
			}
		}

		/* trim trailing DIR_SEP */
		for (size_t i = arglen - 1; i >= argstart; i--) {
			if (IS_ANY_DIR_SEP (arg[i])) {
				arglen--;
			} else {
				break;
			}
		}

		size_t reqlen = (arglen - argstart) + 2;

		if (offset + reqlen >= len) {
			reqlen = ((offset + reqlen) > (len + BUILD_PATH_DEFAULT_BUF_SIZE)) ? (reqlen) : (len + BUILD_PATH_DEFAULT_BUF_SIZE);

			char* tmp = realloc (str, (offset + reqlen));

			if (!tmp) {
				free (str);
				return NULL;
			}

			str = tmp;
			len = (offset + reqlen);
		}

		int written = snprintf (str + offset, len - offset, "%s%.*s",
		                        ((element_count == 0 && has_leading_sep) || (element_count > 0)) ? sep : "",
		                        (uint32_t) (arglen - argstart),
		                        arg + argstart);

		if (written < 0 || (size_t)written >= (len - offset)) {
			free (str);
			return NULL;
		}

		offset += (size_t)written;

		element_count++;

	} while ((arg = va_arg (args, char*)) != NULL);

	va_end (args);

	/* do not mix between different dirseps and removes any consecutive dirseps */
	char* i            = str;
	char* o            = str;
	int   dirseppassed = 0;
	while (*i) {
		if (!dirseppassed && IS_ANY_DIR_SEP (*i)) {
			*o = *sep;
			o++;
			dirseppassed = 1;
		} else if (!IS_ANY_DIR_SEP (*i)) {
			dirseppassed = 0;
			*o           = *i;
			o++;
		}
		i++;
	}
	*o = '\0';

	return str;
}

char*
laaf_util_relative_path (const char* filepath, const char* refpath)
{
	if (!filepath || !refpath || filepath[0] == '\0' || refpath[0] == '\0') {
		return NULL;
	}

	int isWindowsPath = 0;
	int aWindowsPath  = 0;
	int bWindowsPath  = 0;

	char* relpath = NULL;

	if (filepath[0] != '\0' && isalpha (filepath[0]) && filepath[1] == ':') {
		aWindowsPath = 1;
	}

	if (refpath[0] != '\0' && isalpha (refpath[0]) && refpath[1] == ':') {
		bWindowsPath = 1;
	}

	isWindowsPath = (aWindowsPath + bWindowsPath);

	if (isWindowsPath == 1) {
		// fprintf( stderr, "Trying to make a relative path out of a windows path and a non-windows path\n" );
		return NULL;
	}

	if (isWindowsPath == 2) {
		if (tolower (filepath[0]) != tolower (refpath[0])) {
			// fprintf( stderr, "Both paths target different drives\n" );
			return NULL;
		}
	}

	int winDriveLetterOffset = isWindowsPath;

	if (strncmp (filepath + winDriveLetterOffset, refpath + winDriveLetterOffset, strlen (refpath)) == 0) {
		relpath = laaf_util_build_path ("/", "./", filepath + strlen (refpath), NULL);
		return relpath;
	}

	char* _filepath = laaf_util_build_path ("/", filepath, NULL);
	char* _refpath  = laaf_util_build_path ("/", refpath, "/", NULL); /* ensures there is always a trailing '/' */

	if (!_filepath || !_refpath) {
		return NULL;
	}

	char*  parents       = NULL;
	size_t parentsLen    = 0;
	size_t parentsOffset = 0;

	char* p = _refpath + strlen (_refpath);

	while (p > (_refpath + winDriveLetterOffset)) {
		while (p > (_refpath + winDriveLetterOffset) && !IS_DIR_SEP (*p)) {
			*p = '\0';
			p--;
		}

		if (strncmp (_filepath + winDriveLetterOffset, _refpath + winDriveLetterOffset, strlen (_refpath + winDriveLetterOffset)) == 0) {
			if (!parents) {
				relpath = laaf_util_build_path ("/", "./", _filepath + strlen (_refpath), NULL);
				goto end;
			} else {
				relpath = laaf_util_build_path ("/", parents, _filepath + strlen (_refpath), NULL);
				goto end;
			}
		}

		int ret = laaf_util_snprintf_realloc (&parents, &parentsLen, parentsOffset, "../");

		assert (ret >= 0);

		parentsOffset += (size_t)ret;
		p--;
	}

end:
	free (parents);
	free (_filepath);
	free (_refpath);

	return relpath;
}

char*
laaf_util_absolute_path (const char* relpath)
{
	if (!relpath) {
		return NULL;
	}

#ifdef _WIN32
	// char *abspath = NULL;
	wchar_t buf[_MAX_PATH];

	wchar_t* wrelpath = laaf_util_windows_utf8toutf16 (relpath);

	if (!wrelpath) {
		return NULL;
	}

	if (!_wfullpath (buf, wrelpath, _MAX_PATH)) {
		free (wrelpath);
		return NULL;
	}

	char* abspath = laaf_util_windows_utf16toutf8 (buf);

	if (!abspath) {
		free (wrelpath);
		return NULL;
	}

	free (wrelpath);

	return abspath;

#else
	char buf[PATH_MAX + 1];

	if (!realpath (relpath, buf)) {
		return NULL;
	}

	return laaf_util_c99strdup (buf);

#endif
}

int
laaf_util_snprintf_realloc (char** str, size_t* size, size_t offset, const char* format, ...)
{
	size_t tmpsize = 0;

	if (!size) {
		size = &tmpsize;
	}

	int     retval = 0;
	size_t  needed = 0;
	va_list ap;

	va_start (ap, format);

	while (0 <= (retval = vsnprintf ((*str) + offset, (*size) - offset, format, ap)) && ((*size) - offset) < (needed = (unsigned)retval + 1)) {
		va_end (ap);

		*size *= 2;

		if (((*size) - offset) < needed)
			*size = needed + offset;

		char* p = realloc (*str, *size);

		if (p) {
			*str = p;
		} else {
			free (*str);
			*str  = NULL;
			*size = 0;
			return 0;
		}

		va_start (ap, format);
	}

	va_end (ap);

	return (retval > 0) ? retval : 0;
}

int
laaf_util_file_exists (const char* filepath)
{
#ifdef _WIN32
	int needed = MultiByteToWideChar (CP_UTF8, 0, filepath, -1, NULL, 0);

	if (needed == 0) {
		return -1;
	}

	wchar_t* wfilepath = malloc ((size_t)needed * sizeof (wchar_t));

	if (!wfilepath) {
		return -1;
	}

	int rc = MultiByteToWideChar (CP_UTF8, 0, filepath, -1, wfilepath, needed);

	if (rc == 0) {
		free (wfilepath);
		return -1;
	}

	if (_waccess (wfilepath, F_OK) == 0) {
		free (wfilepath);
		return 1;
	}

	free (wfilepath);

#else
	if (access (filepath, F_OK) == 0) {
		return 1;
	}
#endif

	return 0;
}

static int
utf8CodeLen (const uint16_t* u16Code)
{
	if (u16Code[0] < 0x80) {
		return 1;
	} else if (u16Code[0] < 0x800) {
		return 2;
	} else if (u16Code[0] < 0xD800 ||
	           u16Code[0] > 0xDFFF) {
		return 3;
	} else if (((u16Code[0] & 0xFC00) == 0xD800) &&
	           ((u16Code[1] & 0xFC00) == 0xDC00)) {
		return 4;
	} else {
		return -1;
	}
}

static int
utf16CodeLen (const uint16_t* u16Code)
{
	if (u16Code[0] < 0xD800 ||
	    u16Code[0] > 0xDFFF) {
		return 1;
	} else if (((u16Code[0] & 0xFC00) == 0xD800) &&
	           ((u16Code[1] & 0xFC00) == 0xDC00)) {
		return 2;
	} else {
		return -1;
	}
}

static int
utf16CodeToUTF8 (const uint16_t* u16Code, char* u8Code, int* u16Len, int* u8Len)
{
	int len8  = utf8CodeLen (u16Code);
	int len16 = utf16CodeLen (u16Code);

	if (len8 < 0 || len16 < 0) {
		return -1;
	}

	*u8Len  = len8;
	*u16Len = len16;

	if (len8 == 1) {
		u8Code[0] = (char)(u16Code[0]);
	} else if (len8 == 2) {
		u8Code[0] = (char)(0xC0 | (u16Code[0] >> 6));
		u8Code[1] = (char)(0x80 | (u16Code[0] & 0x3F));
	} else if (len8 == 3) {
		u8Code[0] = (char)(0xE0 | (u16Code[0] >> 12));
		u8Code[1] = (char)(0x80 | ((u16Code[0] >> 6) & 0x3F));
		u8Code[2] = (char)(0x80 | (u16Code[0] & 0x3F));
	} else {
		uint32_t c = (u16Code[0] & 0x03FF) << 10;

		c |= (u16Code[1] & 0x03FF);
		c += 0x10000;

		u8Code[0] = (char)(0xF0 | ((c >> 18) & 0x07));
		u8Code[1] = (char)(0x80 | ((c >> 12) & 0x3F));
		u8Code[2] = (char)(0x80 | ((c >> 6) & 0x3F));
		u8Code[3] = (char)(0x80 | (c & 0x3F));
	}

	return *u8Len;
}

static long
utf8strLen (const uint16_t* u16str)
{
	long            len = 0;
	const uint16_t* p   = u16str;

	while (*p != 0x0000) {
		int u8CodeLen  = utf8CodeLen (p);
		int u16CodeLen = utf16CodeLen (p);

		if (u8CodeLen < 0 || u16CodeLen < 0) {
			len = -1;
			break;
		}

		p += u16CodeLen;
		len += u8CodeLen;
	}

	return len;
}

size_t
laaf_util_utf8strCharLen (const char* u8str)
{
	size_t count = 0;

	while (*u8str) {
		count += (*u8str++ & 0xC0) != 0x80;
	}

	return count;
}

char*
laaf_util_utf16Toutf8 (const uint16_t* u16str)
{
	long u8len = utf8strLen (u16str);

	if (u8len < 0) {
		return 0;
	}

	char* u8str = calloc ((size_t)u8len + 1, sizeof (char));

	if (!u8str) {
		return NULL;
	}

	const uint16_t* u16ptr = u16str;
	char*           u8ptr  = u8str;

	while (*u16ptr != 0x0000) {
		int u8codelen  = 0;
		int u16codelen = 0;

		utf16CodeToUTF8 (u16ptr, u8ptr, &u16codelen, &u8codelen);

		if (u16codelen < 0 || u8codelen < 0) {
			free (u8str);
			return NULL;
		}

		u8ptr += u8codelen;
		u16ptr += u16codelen;
	}

	*u8ptr = 0x00;

	return u8str;
}

int
laaf_util_vsnprintf_realloc (char** str, size_t* size, size_t offset, const char* fmt, va_list args)
{
	FILE*   dummy = NULL;
	va_list args1;

	size_t tmpsize = 0;

	if (size == NULL) {
		size = &tmpsize;
	}

	va_copy (args1, args);

	/* https://stackoverflow.com/a/4116308 */
#ifndef _WIN32
	dummy = fopen ("/dev/null", "wb");
#else
	dummy = fopen ("NUL", "wb");
#endif

	if (!dummy) {
		// fprintf( stderr, "Could not fopen() dummy null file\n" );
		goto err;
	}

	int retval = vfprintf (dummy, fmt, args);

	if (retval < 0) {
		// fprintf( stderr, "vfprintf() error : %s\n", strerror(errno) );
		goto err;
	}

	unsigned needed = (unsigned)retval + 1;

	if (needed >= (*size) - offset) {
		char* p = realloc (*str, (offset + needed) * sizeof (char));

		if (p) {
			*str  = p;
			*size = offset + needed;
		} else {
			goto err;
		}
	}

	int written = vsnprintf ((*str) + offset, (*size) - offset, fmt, args1);

	// assert( written >= 0 && (size_t)written < (*size)-offset );

	if (written < 0 && (size_t)written >= (*size) - offset) {
		fprintf (stderr, "vsnprintf() error : %s\n", strerror (errno));
		goto err;
	}

	goto end;

err:
	written = -1;

end:

	if (dummy) {
		fclose (dummy);
	}

	return written;
}

char*
laaf_util_c99strdup (const char* src)
{
	if (!src) {
		return NULL;
	}

	size_t len = 0;

	while (src[len]) {
		len++;
	}

	char* str = malloc (len + 1);

	if (!str) {
		return NULL;
	}

	char* p = str;

	while (*src) {
		*(p++) = *(src++);
	}

	*p = '\0';

	return str;
}

int
laaf_util_dump_hex (const unsigned char* stream, size_t stream_sz, char** buf, size_t* bufsz, size_t offset, const char* padding)
{
	if (stream == NULL) {
		return -1;
	}

	size_t   initialOffset = offset;
	uint32_t i             = 0;

	char hex[49];
	char ascii[19];

	size_t count = 0;

	int rc = laaf_util_snprintf_realloc (buf, bufsz, offset, "%s______________________________ Hex Dump ______________________________\n\n", padding);

	if (rc < 0) {
		goto end;
	}

	offset += (size_t)rc;

	while (count < stream_sz) {
		size_t lineLen = (stream_sz - count) / 16;

		if (lineLen <= 0)
			lineLen = (stream_sz) % 16;
		else
			lineLen = 16;

		memset (&hex, 0x20, sizeof (hex));
		memset (&ascii, 0x00, sizeof (ascii));

		uint32_t linepos = 0;

		for (i = 0; i < lineLen; i++) {
			rc = snprintf (&hex[linepos], sizeof (hex) - (linepos), "%02x%s", *(const unsigned char*)(stream + count + i), (i == 7) ? "  " : " ");

			if (rc < 0) {
				goto end;
			}

			linepos += (uint32_t)rc;

			if (i < 8) {
				if (isalnum (*(stream + count + i)))
					ascii[i] = *(const char*)(stream + count + i);
				else
					ascii[i] = '.';
			} else if (i > 8) {
				if (isalnum (*(stream + count + i)))
					ascii[i + 1] = *(const char*)(stream + count + i);
				else
					ascii[i + 1] = '.';
			} else {
				if (isalnum (*(stream + count + i))) {
					ascii[i]     = ' ';
					ascii[i + 1] = *(const char*)(stream + count + i);
				} else {
					ascii[i]     = ' ';
					ascii[i + 1] = '.';
				}
			}
		}

		/* Fill with blank the rest of the line */
		if (lineLen < 16) {
			for (i = linepos; i < 48; i++) {
				hex[linepos++] = 0x20;
			}
		}

		/* terminate  the line */
		hex[48] = 0x00;

		count += lineLen;

		rc = laaf_util_snprintf_realloc (buf, bufsz, offset, "%s%s  |  %s\n", padding, hex, ascii);

		if (rc < 0) {
			goto end;
		}

		offset += (size_t)rc;
	}

	rc = laaf_util_snprintf_realloc (buf, bufsz, offset, "%s______________________________________________________________________\n\n", padding);

	if (rc < 0) {
		goto end;
	}

end:
	return (int)(offset - initialOffset); /* bytes written */
}
