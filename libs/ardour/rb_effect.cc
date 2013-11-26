/*
    Copyright (C) 2004-2007 Paul Davis

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
#include <cmath>

#include <rubberband/RubberBandStretcher.h>

#include "pbd/error.h"

#include "ardour/audioregion.h"
#include "ardour/audiosource.h"
#include "ardour/pitch.h"
#include "ardour/progress.h"
#include "ardour/session.h"
#include "ardour/stretch.h"
#include "ardour/types.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace RubberBand;

Pitch::Pitch (Session& s, TimeFXRequest& req)
	: RBEffect (s, req)
{
}

RBStretch::RBStretch (Session& s, TimeFXRequest& req)
	: RBEffect (s, req)
{
}

RBEffect::RBEffect (Session& s, TimeFXRequest& req)
	: Filter (s)
	, tsr (req)

{

}

RBEffect::~RBEffect ()
{
}

int
RBEffect::run (boost::shared_ptr<Region> r, Progress* progress)
{
	boost::shared_ptr<AudioRegion> region = boost::dynamic_pointer_cast<AudioRegion> (r);

	if (!region) {
		error << "RBEffect::run() passed a non-audio region! WTF?" << endmsg;
		return -1;
	}

	SourceList nsrcs;
	framecnt_t done;
	int ret = -1;
	const framecnt_t bufsize = 256;
	gain_t* gain_buffer = 0;
	Sample** buffers = 0;
	char suffix[32];
	string new_name;
	string::size_type at;
	framepos_t pos = 0;
	framecnt_t avail = 0;
	boost::shared_ptr<AudioRegion> result;

	cerr << "RBEffect: source region: position = " << region->position()
	     << ", start = " << region->start()
	     << ", length = " << region->length()
	     << ", ancestral_start = " << region->ancestral_start()
	     << ", ancestral_length = " << region->ancestral_length()
	     << ", stretch " << region->stretch()
	     << ", shift " << region->shift() << endl;

	/*
	   We have two cases to consider:

	   1. The region has not been stretched before.

	   In this case, we just want to read region->length() frames
	   from region->start().

	   We will create a new region of region->length() *
	   tsr.time_fraction frames.  The new region will have its
	   start set to 0 (because it has a new audio file that begins
	   at the start of the stretched area) and its ancestral_start
	   set to region->start() (so that we know where to begin
	   reading if we want to stretch it again).

	   2. The region has been stretched before.

	   The region starts at region->start() frames into its
	   (possibly previously stretched) source file.  But we don't
	   want to read from its source file; we want to read from the
	   file it was originally stretched from.

	   The region's source begins at region->ancestral_start()
	   frames into its master source file.  Thus, we need to start
	   reading at region->ancestral_start() + (region->start() /
	   region->stretch()) frames into the master source.  This
	   value will also become the ancestral_start for the new
	   region.

	   We cannot use region->ancestral_length() to establish how
	   many frames to read, because it won't be up to date if the
	   region has been trimmed since it was last stretched.  We
	   must read region->length() / region->stretch() frames and
	   stretch them by tsr.time_fraction * region->stretch(), for
	   a new region of region->length() * tsr.time_fraction
	   frames.

	   Case 1 is of course a special case of 2, where
	   region->ancestral_start() == 0 and region->stretch() == 1.

	   When we ask to read from a region, we supply a position on
	   the global timeline.  The read function calculates the
	   offset into the source as (position - region->position()) +
	   region->start().  This calculation is used regardless of
	   whether we are reading from a master or
	   previously-stretched region.  In order to read from a point
	   n frames into the master source, we need to provide n -
	   region->start() + region->position() as our position
	   argument to master_read_at().

	   Note that region->ancestral_length() is not used.

	   I hope this is clear.
	*/

	double stretch = region->stretch() * tsr.time_fraction;
	double shift = region->shift() * tsr.pitch_fraction;

	framecnt_t read_start = region->ancestral_start() +
		framecnt_t(region->start() / (double)region->stretch());

	framecnt_t read_duration =
		framecnt_t(region->length() / (double)region->stretch());

	uint32_t channels = region->n_channels();

	RubberBandStretcher stretcher
		(session.frame_rate(), channels,
		 (RubberBandStretcher::Options) tsr.opts, stretch, shift);

	progress->set_progress (0);
	tsr.done = false;

	stretcher.setExpectedInputDuration(read_duration);
	stretcher.setDebugLevel(1);

	/* the name doesn't need to be super-precise, but allow for 2 fractional
	   digits just to disambiguate close but not identical FX
	*/

	if (stretch == 1.0) {
		snprintf (suffix, sizeof (suffix), "@%d", (int) floor (shift * 100.0f));
	} else if (shift == 1.0) {
		snprintf (suffix, sizeof (suffix), "@%d", (int) floor (stretch * 100.0f));
	} else {
		snprintf (suffix, sizeof (suffix), "@%d-%d",
			  (int) floor (stretch * 100.0f),
			  (int) floor (shift * 100.0f));
	}

	/* create new sources */

	if (make_new_sources (region, nsrcs, suffix)) {
		goto out;
	}

	gain_buffer = new gain_t[bufsize];
	buffers = new float *[channels];

	for (uint32_t i = 0; i < channels; ++i) {
		buffers[i] = new float[bufsize];
	}

	/* we read from the master (original) sources for the region,
	   not the ones currently in use, in case it's already been
	   subject to timefx.  */

	/* study first, process afterwards. */

	pos = 0;
	avail = 0;
	done = 0;

	try {
		while (pos < read_duration && !tsr.cancel) {

			framecnt_t this_read = 0;

			for (uint32_t i = 0; i < channels; ++i) {

				this_read = 0;

				framepos_t this_time;
				this_time = min(bufsize, read_duration - pos);

				framepos_t this_position;
				this_position = read_start + pos -
					region->start() + region->position();

				this_read = region->master_read_at
					(buffers[i],
					 buffers[i],
					 gain_buffer,
					 this_position,
					 this_time,
					 i);

				if (this_read != this_time) {
					error << string_compose
						(_("tempoize: error reading data from %1 at %2 (wanted %3, got %4)"),
						 region->name(), this_position, this_time, this_read) << endmsg;
					goto out;
				}
			}

			pos += this_read;
			done += this_read;

			progress->set_progress (((float) done / read_duration) * 0.25);

			stretcher.study(buffers, this_read, pos == read_duration);
		}

		done = 0;
		pos = 0;

		while (pos < read_duration && !tsr.cancel) {

			framecnt_t this_read = 0;

			for (uint32_t i = 0; i < channels; ++i) {

				this_read = 0;
				framepos_t this_time;
				this_time = min(bufsize, read_duration - pos);

				framepos_t this_position;
				this_position = read_start + pos -
					region->start() + region->position();

				this_read = region->master_read_at
					(buffers[i],
					 buffers[i],
					 gain_buffer,
					 this_position,
					 this_time,
					 i);

				if (this_read != this_time) {
					error << string_compose
						(_("tempoize: error reading data from %1 at %2 (wanted %3, got %4)"),
						 region->name(), pos + region->position(), this_time, this_read) << endmsg;
					goto out;
				}
			}

			pos += this_read;
			done += this_read;

			progress->set_progress (0.25 + ((float) done / read_duration) * 0.75);

			stretcher.process(buffers, this_read, pos == read_duration);

			framecnt_t avail = 0;

			while ((avail = stretcher.available()) > 0) {

				this_read = min (bufsize, avail);

				stretcher.retrieve(buffers, this_read);

				for (uint32_t i = 0; i < nsrcs.size(); ++i) {

					boost::shared_ptr<AudioSource> asrc = boost::dynamic_pointer_cast<AudioSource>(nsrcs[i]);
					if (!asrc) {
						continue;
					}

					if (asrc->write(buffers[i], this_read) != this_read) {
						error << string_compose (_("error writing tempo-adjusted data to %1"), nsrcs[i]->name()) << endmsg;
						goto out;
					}
				}
			}
		}

		while ((avail = stretcher.available()) >= 0) {

			framecnt_t this_read = min (bufsize, avail);

			stretcher.retrieve(buffers, this_read);

			for (uint32_t i = 0; i < nsrcs.size(); ++i) {

				boost::shared_ptr<AudioSource> asrc = boost::dynamic_pointer_cast<AudioSource>(nsrcs[i]);
				if (!asrc) {
					continue;
				}

				if (asrc->write(buffers[i], this_read) !=
				    this_read) {
					error << string_compose (_("error writing tempo-adjusted data to %1"), nsrcs[i]->name()) << endmsg;
					goto out;
				}
			}
		}

	} catch (runtime_error& err) {
		error << string_compose (_("programming error: %1"), X_("timefx code failure")) << endmsg;
		error << err.what() << endmsg;
		goto out;
	}

	new_name = region->name();
	at = new_name.find ('@');

	// remove any existing stretch indicator

	if (at != string::npos && at > 2) {
		new_name = new_name.substr (0, at - 1);
	}

	new_name += suffix;

	ret = finish (region, nsrcs, new_name);

	/* now reset ancestral data for each new region */

	for (vector<boost::shared_ptr<Region> >::iterator x = results.begin(); x != results.end(); ++x) {

		(*x)->set_ancestral_data (read_start,
					  read_duration,
					  stretch,
					  shift);
		(*x)->set_master_sources (region->master_sources());
		/* multiply the old (possibly previously stretched) region length by the extra
		   stretch this time around to get its new length
		*/
		(*x)->set_length ((*x)->length() * tsr.time_fraction);
	}

	/* stretch region gain envelope */
	/* XXX: assuming we've only processed one input region into one result here */

	if (tsr.time_fraction != 1) {
		result = boost::dynamic_pointer_cast<AudioRegion> (results.front());
		assert (result);
		result->envelope()->x_scale (tsr.time_fraction);
	}

  out:

	delete [] gain_buffer;

	if (buffers) {
		for (uint32_t i = 0; i < channels; ++i) {
			delete buffers[i];
		}
		delete [] buffers;
	}

	if (ret || tsr.cancel) {
		for (SourceList::iterator si = nsrcs.begin(); si != nsrcs.end(); ++si) {
			(*si)->mark_for_remove ();
		}
	}

	return ret;
}





