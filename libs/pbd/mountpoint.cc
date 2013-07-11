/*
    Copyright (C) 2002 Paul Davis 

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

    $Id$
*/

#include <cstdio>
#include <cstring>
#include <string>
#include <cstring>
#include <limits.h>

#include "pbd/mountpoint.h"

using std::string;

#ifdef WAF_BUILD
#include "libpbd-config.h"
#endif

#ifdef HAVE_GETMNTENT
#include <mntent.h>

struct mntent_sorter {
    bool operator() (const mntent *a, const mntent *b) {
	    return strcmp (a->mnt_dir, b->mnt_dir);
    }
};

string
mountpoint (string path)
{
	FILE *mntf;
	mntent *mnt;
	unsigned int maxmatch = 0;
	unsigned int matchlen;
	const char *cpath = path.c_str();
	char best[PATH_MAX+1];
	
	if ((mntf = setmntent ("/etc/mtab", "r")) == 0) {
		return "";
	}

	best[0] = '\0';

	while ((mnt = getmntent (mntf))) {
		unsigned int n;

		n = 0;
		matchlen = 0;

		/* note: strcmp's semantics are not 
		   strict enough to use for this.
		*/

		while (cpath[n] && mnt->mnt_dir[n]) {
			if (cpath[n] != mnt->mnt_dir[n]) {
				break;
			}
			matchlen++;
			n++;
		}

		if (cpath[matchlen] == '\0') {

			endmntent (mntf);
			return mnt->mnt_dir;

		} else {

			if (matchlen > maxmatch) {
				snprintf (best, sizeof(best), "%s", mnt->mnt_dir);
				maxmatch = matchlen;
			}
		}
	}

	endmntent (mntf);

	return best;
}

#elif defined(WIN32)

string
mountpoint (string path)
{
	// TODO ... if needed
}

#else // !HAVE_GETMNTENT

#include <sys/param.h>
#include <sys/ucred.h>
#include <sys/mount.h>

string
mountpoint (string path)
{
	struct statfs *mntbufp = 0;
	int count;
	unsigned int maxmatch = 0;
	unsigned int matchlen;
	const char *cpath = path.c_str();
	char best[PATH_MAX+1];
	
	/* From the manpage, under "BUGS" : "The memory allocated by getmntinfo() cannot be free(3)'d by the 
	   application."
	   
	   Thus: we do NOT try to free memory allocated by getmntinfo()
	*/

	if ((count = getmntinfo(&mntbufp, MNT_NOWAIT)) == 0) {
		return "\0";
	}

	best[0] = '\0';

	for (int i = 0; i < count; ++i) {
		unsigned int n = 0;
		matchlen = 0;

		/* note: strcmp's semantics are not 
		   strict enough to use for this.
		*/

		while (cpath[n] && mntbufp[i].f_mntonname[n]) {
			if (cpath[n] != mntbufp[i].f_mntonname[n]) {
				break;
			}
			matchlen++;
			n++;
		}

		if (cpath[matchlen] == '\0') {
			snprintf(best, sizeof(best), "%s", mntbufp[i].f_mntonname);
			return best;

		} else {

			if (matchlen > maxmatch) {
				snprintf (best, sizeof(best), "%s", mntbufp[i].f_mntonname);
				maxmatch = matchlen;
			}
		}
	}
	
	return best;
}
#endif // HAVE_GETMNTENT

#ifdef TEST_MOUNTPOINT
		
main (int argc, char *argv[])
{
	printf ("mp of %s = %s\n", argv[1], mountpoint (argv[1]).c_str());
	exit (0);
}

#endif // TEST_MOUNTPOINT
