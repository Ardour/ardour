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

    $Id$
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
	const jack_nframes_t blocksize = 256 * 1048;
	Sample buf[blocksize];
	jack_nframes_t fpos;
	jack_nframes_t fend;
	jack_nframes_t fstart;
	jack_nframes_t to_read;
	int ret = -1;

	/* create new sources */

	if (make_new_sources (region, nsrcs)) {
		goto out;
	}

	fend = region->start() + region->length();
	fstart = region->start();

	if (blocksize < fend) {
		fpos =max(fstart, fend - blocksize);
	} else {
		fpos = fstart;
	}

	to_read = min (region->length(), blocksize);

	/* now read it backwards */

	while (1) {

		uint32_t n;

		for (n = 0, si = nsrcs.begin(); n < region->n_channels(); ++n, ++si) {

			/* read it in */
			
			if (region->audio_source (n)->read (buf, fpos, to_read) != to_read) {
				goto out;
			}
			
			/* swap memory order */
			
			for (jack_nframes_t i = 0; i < to_read/2; ++i) {
				swap (buf[i],buf[to_read-1-i]);
			}
			
			/* write it out */

			boost::shared_ptr<AudioSource> asrc(boost::dynamic_pointer_cast<AudioSource>(*si));

			if (asrc && asrc->write (buf, to_read) != to_read) {
				goto out;
			}
		}

		if (fpos == fstart) {
			break;
		} else if (fpos > fstart + to_read) {
			fpos -= to_read;
			to_read = min (fstart - fpos, blocksize);
		} else {
			to_read = fpos-fstart;
			fpos = fstart;
		}
	};

	ret = finish (region, nsrcs);

  out:

	if (ret) {
		for (si = nsrcs.begin(); si != nsrcs.end(); ++si) {
			boost::shared_ptr<AudioSource> asrc(boost::dynamic_pointer_cast<AudioSource>(*si));
			asrc->mark_for_remove ();
		}
	}
	
	return ret;
}
