/*
    Copyright (C) 2004 Paul Davis 

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

#include <algorithm>

#include <pbd/basename.h>

#include <ardour/types.h>
#include <ardour/reverse.h>
#include <ardour/audiofilesource.h>
#include <ardour/session.h>
#include <ardour/audioregion.h>

#include "i18n.h"

using namespace std;
using namespace ARDOUR;

Reverse::Reverse (Session& s)
	: AudioFilter (s)
{
}

Reverse::~Reverse ()
{
}

int
Reverse::run (boost::shared_ptr<AudioRegion> region)
{
	SourceList nsrcs;
	SourceList::iterator si;
	nframes_t blocksize = 256 * 1024;
	Sample* buf = 0;
	nframes_t fpos;
	nframes_t fstart;
	nframes_t to_read;
	int ret = -1;

	/* create new sources */

	if (make_new_sources (region, nsrcs)) {
		goto out;
	}

	fstart = region->start();

	if (blocksize > region->length()) {
		blocksize = region->length();
	}

	fpos = max (fstart, (fstart + region->length() - blocksize));
	buf = new Sample[blocksize];
	to_read = blocksize;

	/* now read it backwards */

	while (to_read) {

		uint32_t n;

		for (n = 0, si = nsrcs.begin(); n < region->n_channels(); ++n, ++si) {

			/* read it in, with any amplitude scaling */
			
			if (region->read (buf, fpos, to_read, n) != to_read) {
				goto out;
			}

			/* swap memory order */
			
			for (nframes_t i = 0; i < to_read/2; ++i) {
				swap (buf[i],buf[to_read-1-i]);
			}
			
			/* write it out */

			if ((*si)->write (buf, to_read) != to_read) {
				goto out;
			}
		}

		if (fpos > fstart + blocksize) {
			fpos -= to_read;
			to_read = blocksize;
		} else {
			to_read = fpos - fstart;
			fpos = fstart;
		}
	};

	ret = finish (region, nsrcs);

  out:

	if (buf) {
		delete [] buf;
	}

	if (ret) {
		for (si = nsrcs.begin(); si != nsrcs.end(); ++si) {
			(*si)->mark_for_remove ();
		}
	}
	
	return ret;
}
