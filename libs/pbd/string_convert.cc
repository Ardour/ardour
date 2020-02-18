/*
 * Copyright (C) 2015-2017 Tim Mayberry <mojofunk@gmail.com>
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

#include "pbd/string_convert.h"

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>
#include <cerrno>
#include <cstdio>
#include <limits>

#include <glib.h>
#include <glib/gprintf.h>

#include "pbd/compose.h"
#include "pbd/debug.h"
#include "pbd/i18n.h"

#define DEBUG_SCONVERT(msg) DEBUG_TRACE (PBD::DEBUG::StringConvert, string_compose ("%1: %2\n", __LINE__, msg));

#define CONVERT_BUF_SIZE 32

namespace PBD {

bool string_to_bool (const std::string& str, bool& val)
{
	if (str.empty ()) {
		return false;

	} else if (str == X_("1")) {
		val = true;
		return true;

	} else if (str == X_("0")) {
		val = false;
		return true;

	} else if (str == X_("y")) {
		val = true;
		return true;

	} else if (str == X_("n")) {
		val = false;
		return true;

	} else if (g_ascii_strncasecmp (str.c_str(), X_("yes"), str.length()) == 0) {
		val = true;
		return true;

	} else if (g_ascii_strncasecmp (str.c_str(), X_("no"), str.length()) == 0) {
		val = false;
		return true;

	} else if (g_ascii_strncasecmp (str.c_str(), X_("true"), str.length()) == 0) {
		val = true;
		return true;

	} else if (g_ascii_strncasecmp (str.c_str(), X_("false"), str.length()) == 0) {
		val = false;
		return true;
	}

	DEBUG_SCONVERT (
	    string_compose ("string_to_bool conversion failed for %1", str));

	return false;
}

bool string_to_int16 (const std::string& str, int16_t& val)
{
	if (sscanf (str.c_str (), "%" SCNi16, &val) != 1) {
		DEBUG_SCONVERT (
		    string_compose ("string_to_int16 conversion failed for %1", str));
		return false;
	}
	return true;
}

bool string_to_uint16 (const std::string& str, uint16_t& val)
{
	if (sscanf (str.c_str (), "%" SCNu16, &val) != 1) {
		DEBUG_SCONVERT (
		    string_compose ("string_to_uint16 conversion failed for %1", str));
		return false;
	}
	return true;
}

bool string_to_int32 (const std::string& str, int32_t& val)
{
	if (sscanf (str.c_str (), "%" SCNi32, &val) != 1) {
		DEBUG_SCONVERT (
		    string_compose ("string_to_int32 conversion failed for %1", str));
		return false;
	}
	return true;
}

bool string_to_uint32 (const std::string& str, uint32_t& val)
{
	if (sscanf (str.c_str (), "%" SCNu32, &val) != 1) {
		DEBUG_SCONVERT (
		    string_compose ("string_to_uint32 conversion failed for %1", str));
		return false;
	}
	return true;
}

bool string_to_int64 (const std::string& str, int64_t& val)
{
	if (sscanf (str.c_str (), "%" SCNi64, &val) != 1) {
		DEBUG_SCONVERT (
		    string_compose ("string_to_int64 conversion failed for %1", str));
		return false;
	}
	return true;
}

bool string_to_uint64 (const std::string& str, uint64_t& val)
{
	if (sscanf (str.c_str (), "%" SCNu64, &val) != 1) {
		DEBUG_SCONVERT (
		    string_compose ("string_to_uint64 conversion failed for %1", str));
		return false;
	}
	return true;
}

template <class FloatType>
static bool
_string_to_infinity (const std::string& str, FloatType& val)
{
	if (!g_ascii_strncasecmp (str.c_str (), X_ ("inf"), str.length ()) ||
	    !g_ascii_strncasecmp (str.c_str (), X_ ("+inf"), str.length ()) ||
	    !g_ascii_strncasecmp (str.c_str (), X_ ("INFINITY"), str.length ()) ||
	    !g_ascii_strncasecmp (str.c_str (), X_ ("+INFINITY"), str.length ())) {
		val = std::numeric_limits<FloatType>::infinity ();
		return true;
	} else if (!g_ascii_strncasecmp (str.c_str (), X_ ("-inf"), str.length ()) ||
	           !g_ascii_strncasecmp (str.c_str (), X_ ("-INFINITY"), str.length ())) {
		val = -std::numeric_limits<FloatType>::infinity ();
		return true;
	}
	return false;
}

bool
_string_to_double (const std::string& str, double& val)
{
	val = g_ascii_strtod (str.c_str (), NULL);

	// It is possible that the conversion was successful and another thread
	// has set errno meanwhile but as most conversions are currently not
	// checked for error conditions this is better than nothing.
	if (errno == ERANGE) {
		DEBUG_SCONVERT (string_compose ("string_to_double possible conversion failure for %1", str));
		// There should not be any conversion failures as we control the string
		// contents so returning false here should not have any impact...
		return false;
	}
	return true;
}

bool string_to_float (const std::string& str, float& val)
{
	double tmp;
	if (_string_to_double (str, tmp)) {
		val = (float)tmp;
		return true;
	}

	if (_string_to_infinity (str, val)) {
		return true;
	}

	return false;
}

bool string_to_double (const std::string& str, double& val)
{
	if (_string_to_double (str, val)) {
		return true;
	}

	if (_string_to_infinity (str, val)) {
		return true;
	}

	return false;
}

bool bool_to_string (bool val, std::string& str)
{
	if (val) {
		str = X_("1");
	} else {
		str = X_("0");
	}
	return true;
}

bool int16_to_string (int16_t val, std::string& str)
{
	char buffer[CONVERT_BUF_SIZE];

	int retval = g_snprintf (buffer, sizeof(buffer), "%" PRIi16, val);

	if (retval <= 0 || retval >= (int)sizeof(buffer)) {
		DEBUG_SCONVERT (
		    string_compose ("int16_to_string conversion failure for %1", val));
		return false;
	}
	str = buffer;
	return true;
}

bool uint16_to_string (uint16_t val, std::string& str)
{
	char buffer[CONVERT_BUF_SIZE];

	int retval = g_snprintf (buffer, sizeof(buffer), "%" PRIu16, val);

	if (retval <= 0 || retval >= (int)sizeof(buffer)) {
		DEBUG_SCONVERT (
		    string_compose ("uint16_to_string conversion failure for %1", val));
		return false;
	}
	str = buffer;
	return true;
}

bool int32_to_string (int32_t val, std::string& str)
{
	char buffer[CONVERT_BUF_SIZE];

	int retval = g_snprintf (buffer, sizeof(buffer), "%" PRIi32, val);

	if (retval <= 0 || retval >= (int)sizeof(buffer)) {
		DEBUG_SCONVERT (
		    string_compose ("int32_to_string conversion failure for %1", val));
		return false;
	}
	str = buffer;
	return true;
}

bool uint32_to_string (uint32_t val, std::string& str)
{
	char buffer[CONVERT_BUF_SIZE];

	int retval = g_snprintf (buffer, sizeof(buffer), "%" PRIu32, val);

	if (retval <= 0 || retval >= (int)sizeof(buffer)) {
		DEBUG_SCONVERT (
		    string_compose ("uint32_to_string conversion failure for %1", val));
		return false;
	}
	str = buffer;
	return true;
}

bool int64_to_string (int64_t val, std::string& str)
{
	char buffer[CONVERT_BUF_SIZE];

	int retval = g_snprintf (buffer, sizeof(buffer), "%" PRIi64, val);

	if (retval <= 0 || retval >= (int)sizeof(buffer)) {
		DEBUG_SCONVERT (
		    string_compose ("int64_to_string conversion failure for %1", val));
		return false;
	}
	str = buffer;
	return true;
}

bool uint64_to_string (uint64_t val, std::string& str)
{
	char buffer[CONVERT_BUF_SIZE];

	int retval = g_snprintf (buffer, sizeof(buffer), "%" PRIu64, val);

	if (retval <= 0 || retval >= (int)sizeof(buffer)) {
		DEBUG_SCONVERT (
		    string_compose ("uint64_to_string conversion failure for %1", val));
		return false;
	}
	str = buffer;
	return true;
}

template <class FloatType>
static bool
_infinity_to_string (FloatType val, std::string& str)
{
	if (val == std::numeric_limits<FloatType>::infinity ()) {
		str = "inf";
		return true;
	} else if (val == -std::numeric_limits<FloatType>::infinity ()) {
		str = "-inf";
		return true;
	}
	return false;
}

static bool
_double_to_string (double val, std::string& str)
{
	char buffer[G_ASCII_DTOSTR_BUF_SIZE];

	char* d_cstr = g_ascii_dtostr (buffer, sizeof(buffer), val);

	if (d_cstr == NULL) {
		return false;
	}
	str = d_cstr;
	return true;
}

bool float_to_string (float val, std::string& str)
{
	if (_infinity_to_string (val, str)) {
		return true;
	}

	if (_double_to_string (val, str)) {
		return true;
	}

	DEBUG_SCONVERT (string_compose ("float_to_string conversion failure for %1", val));
	return false;
}

bool double_to_string (double val, std::string& str)
{
	if (_infinity_to_string (val, str)) {
		return true;
	}

	if (_double_to_string (val, str)) {
		return true;
	}

	DEBUG_SCONVERT (string_compose ("double_to_string conversion failure for %1", val));
	return false;
}

} // namespace PBD
