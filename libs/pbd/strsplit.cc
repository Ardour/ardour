#include <pbd/strsplit.h>

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

	while ((pos = remaining.find_first_of (':')) != string::npos) {
		result.push_back (remaining.substr (0, pos));
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
		if (str[n] == splitchar) {
			cnt++;
		}
	}

	if (cnt == 0) {
		result.push_back (str);
		return;
	}

	remaining = str;

	while ((pos = remaining.find_first_of (':')) != ustring::npos) {
		result.push_back (remaining.substr (0, pos));
		remaining = remaining.substr (pos+1);
	}

	if (remaining.length()) {

		result.push_back (remaining);
	}
}
