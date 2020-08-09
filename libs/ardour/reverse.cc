/*
 * Copyright (C) 2006-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2008-2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2008-2017 Paul Davis <paul@linuxaudiosystems.com>
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

#include <algorithm>

#include "ardour/audioregion.h"
#include "ardour/audiosource.h"
#include "ardour/reverse.h"
#include "ardour/types.h"

using namespace std;
using namespace ARDOUR;

namespace ARDOUR { class Progress; class Session; }

Reverse::Reverse (Session& s)
	: Filter (s)
{
}

Reverse::~Reverse ()
{
}

int
Reverse::run (boost::shared_ptr<Region> r, Progress*)
{
	SourceList nsrcs;
	SourceList::iterator si;
	samplecnt_t blocksize = 256 * 1024;
	Sample* buf = 0;
	samplepos_t fpos;
	samplepos_t fstart;
	samplecnt_t to_read;
	int ret = -1;

	boost::shared_ptr<AudioRegion> region = boost::dynamic_pointer_cast<AudioRegion>(r);

	if (!region) {
		return ret;
	}

	/* create new sources */

	if (make_new_sources (region, nsrcs, "", false)) {
		goto out;
	}

	fstart = region->start_sample();

	if (blocksize > region->length_samples()) {
		blocksize = region->length_samples();
	}

	fpos = max (fstart, (fstart + region->length_samples() - blocksize));

	buf = new Sample[blocksize];
	to_read = blocksize;

	/* now read it backwards */

	while (to_read) {

		uint32_t n;

		for (n = 0, si = nsrcs.begin(); n < region->n_channels(); ++n, ++si) {

			/* read it in directly from the source */

			if (region->audio_source (n)->read (buf, fpos, to_read) != to_read) {
				goto out;
			}

			/* swap memory order */

			for (samplecnt_t i = 0; i < to_read/2; ++i) {
				swap (buf[i],buf[to_read-1-i]);
			}

			/* write it out */

			boost::shared_ptr<AudioSource> asrc(boost::dynamic_pointer_cast<AudioSource>(*si));

			if (asrc && asrc->write (buf, to_read) != to_read) {
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

	delete [] buf;

	if (ret) {
		for (si = nsrcs.begin(); si != nsrcs.end(); ++si) {
			boost::shared_ptr<AudioSource> asrc(boost::dynamic_pointer_cast<AudioSource>(*si));
			asrc->mark_for_remove ();
		}
	}

	return ret;
}
