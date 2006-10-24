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

#include "pbd/convert.h"

#include "i18n.h"

using std::string;
using std::vector;

namespace PBD {

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
	return std::atoi (s.c_str());
}

double
atof (const string& s)
{
	return std::atof (s.c_str());
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

void
url_decode (string& url)
{
	string::iterator last;
	string::iterator next;

	for (string::iterator i = url.begin(); i != url.end(); ++i) {
		if ((*i) == '+') {
			*i = ' ';
		}
	}

	if (url.length() <= 3) {
		return;
	}

	last = url.end();

	--last; /* points at last char */
	--last; /* points at last char - 1 */

	for (string::iterator i = url.begin(); i != last; ) {

		if (*i == '%') {

			next = i;

			url.erase (i);
			
			i = next;
			++next;
			
			if (isxdigit (*i) && isxdigit (*next)) {
				/* replace first digit with char */
				*i = int_from_hex (*i,*next);
				++i; /* points at 2nd of 2 digits */
				url.erase (i);
			}
		} else {
			++i;
		}
	}
}

string
length2string (const int32_t frames, const float sample_rate)
{
    int secs = (int) (frames / sample_rate);
    int hrs =  secs / 3600;
    secs -= (hrs * 3600);
    int mins = secs / 60;
    secs -= (mins * 60);

    int total_secs = (hrs * 3600) + (mins * 60) + secs;
    int frames_remaining = (int) floor (frames - (total_secs * sample_rate));
    float fractional_secs = (float) frames_remaining / sample_rate;

    char duration_str[32];
    sprintf (duration_str, "%02d:%02d:%05.2f", hrs, mins, (float) secs + fractional_secs);

    return duration_str;
}

} // namespace PBD
