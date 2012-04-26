/*
    Copyright (C) 2006 Paul Davis 

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <cmath>
#include <stdint.h>
#include <stdlib.h>
#include <cstdio>
#include <ctype.h>
#include <cstring>
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>

#include "pbd/convert.h"

#include "i18n.h"

using std::string;
using std::vector;
using Glib::ustring;

namespace PBD {

string
capitalize (const string& str)
{
        string ret = str;
        if (!str.empty()) {
                /* XXX not unicode safe */
                ret[0] = toupper (str[0]);
        }
        return ret;
}

string
short_version (string orig, string::size_type target_length)
{
	/* this tries to create a recognizable abbreviation
	   of "orig" by removing characters until we meet
	   a certain target length.

	   note that we deliberately leave digits in the result
	   without modification.
	*/


	string::size_type pos;

	/* remove white-space and punctuation, starting at end */

	while (orig.length() > target_length) {
		if ((pos = orig.find_last_of (_("\"\n\t ,<.>/?:;'[{}]~`!@#$%^&*()_-+="))) == string::npos) {
			break;
		}
		orig.replace (pos, 1, "");
	}

	/* remove lower-case vowels, starting at end */

	while (orig.length() > target_length) {
		if ((pos = orig.find_last_of (_("aeiou"))) == string::npos) {
			break;
		}
		orig.replace (pos, 1, "");
	}

	/* remove upper-case vowels, starting at end */

	while (orig.length() > target_length) {
		if ((pos = orig.find_last_of (_("AEIOU"))) == string::npos) {
			break;
		}
		orig.replace (pos, 1, "");
	}

	/* remove lower-case consonants, starting at end */

	while (orig.length() > target_length) {
		if ((pos = orig.find_last_of (_("bcdfghjklmnpqrtvwxyz"))) == string::npos) {
			break;
		}
		orig.replace (pos, 1, "");
	}

	/* remove upper-case consonants, starting at end */

	while (orig.length() > target_length) {
		if ((pos = orig.find_last_of (_("BCDFGHJKLMNPQRTVWXYZ"))) == string::npos) {
			break;
		}
		orig.replace (pos, 1, "");
	}

	/* whatever the length is now, use it */
	
	return orig;
}

int
atoi (const string& s)
{
	return ::atoi (s.c_str());
}

int32_t
atol (const string& s)
{
	return (int32_t) ::atol (s.c_str());
}

int64_t
atoll (const string& s)
{
	return (int64_t) ::atoll (s.c_str());
}

double
atof (const string& s)
{
	return ::atof (s.c_str());
}

vector<string>
internationalize (const char *package_name, const char **array)
{
	vector<string> v;

	for (uint32_t i = 0; array[i]; ++i) {
		v.push_back (dgettext(package_name, array[i]));
	}

	return v;
}

static int32_t 
int_from_hex (char hic, char loc) 
{
	int hi;		/* hi byte */
	int lo;		/* low byte */

	hi = (int) hic;

	if( ('0'<=hi) && (hi<='9') ) {
		hi -= '0';
	} else if( ('a'<= hi) && (hi<= 'f') ) {
		hi -= ('a'-10);
	} else if( ('A'<=hi) && (hi<='F') ) {
		hi -= ('A'-10);
	}
	
	lo = (int) loc;
	
	if( ('0'<=lo) && (lo<='9') ) {
		lo -= '0';
	} else if( ('a'<=lo) && (lo<='f') ) {
		lo -= ('a'-10);
	} else if( ('A'<=lo) && (lo<='F') ) {
		lo -= ('A'-10);
	}

	return lo + (16 * hi);
}

string
url_decode (string const & url)
{
	string decoded;

	for (string::size_type i = 0; i < url.length(); ++i) {
		if (url[i] == '+') {
			decoded += ' ';
		} else if (url[i] == '%' && i <= url.length() - 3) {
			decoded += char (int_from_hex (url[i + 1], url[i + 2]));
			i += 2;
		} else {
			decoded += url[i];
		}
	}

	return decoded;
}

#if 0
string
length2string (const int32_t frames, const float sample_rate)
{
    int32_t secs = (int32_t) (frames / sample_rate);
    int32_t hrs =  secs / 3600;
    secs -= (hrs * 3600);
    int32_t mins = secs / 60;
    secs -= (mins * 60);

    int32_t total_secs = (hrs * 3600) + (mins * 60) + secs;
    int32_t frames_remaining = (int) floor (frames - (total_secs * sample_rate));
    float fractional_secs = (float) frames_remaining / sample_rate;

    char duration_str[32];
    sprintf (duration_str, "%02" PRIi32 ":%02" PRIi32 ":%05.2f", hrs, mins, (float) secs + fractional_secs);

    return duration_str;
}
#endif

string
length2string (const int64_t frames, const double sample_rate)
{
	int64_t secs = (int64_t) floor (frames / sample_rate);
	int64_t hrs =  secs / 3600LL;
	secs -= (hrs * 3600LL);
	int64_t mins = secs / 60LL;
	secs -= (mins * 60LL);
	
	int64_t total_secs = (hrs * 3600LL) + (mins * 60LL) + secs;
	int64_t frames_remaining = (int64_t) floor (frames - (total_secs * sample_rate));
	float fractional_secs = (float) frames_remaining / sample_rate;
	
	char duration_str[64];
	sprintf (duration_str, "%02" PRIi64 ":%02" PRIi64 ":%05.2f", hrs, mins, (float) secs + fractional_secs);
	
	return duration_str;
}

static bool 
chars_equal_ignore_case(char x, char y)
{
	/* app should have called setlocale() if its wants this comparison to be
	   locale sensitive.
	*/
	return toupper (x) == toupper (y);
}

bool 
strings_equal_ignore_case (const string& a, const string& b)
{
	if (a.length() == b.length()) {
		return std::equal (a.begin(), a.end(), b.begin(), chars_equal_ignore_case);
	}
	return false;
}

bool
string_is_affirmative (const std::string& str)
{
	/* to be used only with XML data - not intended to handle user input */

	if (str.empty ()) {
		return false;
	}

	/* the use of g_strncasecmp() is solely to get around issues with
	 * charsets posed by trying to use C++ for the same
	 * comparison. switching a std::string to its lower- or upper-case
	 * version has several issues, but handled by default
	 * in the way we desire when doing it in C.
	 */

	return str == "1" || str == "y" || str == "Y" || (!g_strncasecmp(str.c_str(), "yes", str.length())) ||
		(!g_strncasecmp(str.c_str(), "true", str.length()));
}

/** A wrapper for dgettext that takes a msgid of the form Context|Text.
 *  If Context|Text is translated, the translation is returned, otherwise
 *  just Text is returned.  Useful for getting translations of words or phrases
 *  that have different meanings in different contexts.
 */
const char *
sgettext (const char* domain_name, const char* msgid)
{
	const char * msgval = dgettext (domain_name, msgid);
	if (msgval == msgid) {
		const char * p = strrchr (msgid, '|');
		if (p) {
			msgval = p + 1;
		}
	}
	return msgval;
}

} // namespace PBD
