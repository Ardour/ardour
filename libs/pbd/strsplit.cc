/*
 * Copyright (C) 2000-2015 Paul Davis <paul@linuxaudiosystems.com>
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

#include "pbd/strsplit.h"

using namespace std;
using namespace Glib;

void
split (string str, vector<string>& result, char splitchar)
{
	string::size_type pos;
	string remaining;
	string::size_type len = str.length();
	int cnt;

	cnt = 0;

	if (str.empty()) {
		return;
	}

	for (string::size_type n = 0; n < len; ++n) {
		if (str[n] == splitchar) {
			cnt++;
		}
	}

	if (cnt == 0) {
		result.push_back (str);
		return;
	}

	remaining = str;

	while ((pos = remaining.find_first_of (splitchar)) != string::npos) {
		if (pos != 0) {
			result.push_back (remaining.substr (0, pos));
		}
		remaining = remaining.substr (pos+1);
	}

	if (remaining.length()) {

		result.push_back (remaining);
	}
}

void
split (ustring str, vector<ustring>& result, char splitchar)
{
	ustring::size_type pos;
	ustring remaining;
	ustring::size_type len = str.length();
	int cnt;

	cnt = 0;

	if (str.empty()) {
		return;
	}

	for (ustring::size_type n = 0; n < len; ++n) {
		if (str[n] == gunichar(splitchar)) {
			cnt++;
		}
	}

	if (cnt == 0) {
		result.push_back (str);
		return;
	}

	remaining = str;

	while ((pos = remaining.find_first_of (splitchar)) != ustring::npos) {
		result.push_back (remaining.substr (0, pos));
		remaining = remaining.substr (pos+1);
	}

	if (remaining.length()) {

		result.push_back (remaining);
	}
}
