/*
 * Copyright (C) 2008-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2008-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2008-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2019 Robin Gareus <robin@gareus.org>
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
#include <cmath>

#include <glibmm.h>

#include <rubberband/RubberBandStretcher.h>

#include "pbd/error.h"

#include "ardour/audioregion.h"
#include "ardour/audiosource.h"
#include "ardour/pitch.h"
#include "ardour/progress.h"
#include "ardour/session.h"
#include "ardour/stretch.h"
#include "ardour/types.h"

#include "pbd/i18n.h"

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

	SourceList        nsrcs;
	int               ret         = -1;
	const samplecnt_t bufsize     = 8192;
	gain_t*           gain_buffer = 0;
	Sample**          buffers     = 0;
	char              suffix[32];
	string            new_name;
	string::size_type at;

#ifndef NDEBUG
	cerr << "RBEffect: source region: position = " << region->position_sample ()
	     << ", start = " << region->start_sample ()
	     << ", length = " << region->length_samples ()
	     << ", ancestral_start = " << region->ancestral_start_sample ()
	     << ", ancestral_length = " << region->ancestral_length_samples ()
	     << ", stretch " << region->stretch ()
	     << ", shift " << region->shift () << endl;
#endif

	/*
	 * We have two cases to consider:
	 *
	 * 1. The region has not been stretched before.
	 *
	 * In this case, we just want to read region->length() samples
	 * from region->start().
	 *
	 * We will create a new region of region->length() *
	 * tsr.time_fraction samples.  The new region will have its
	 * start set to 0 (because it has a new audio file that begins
	 * at the start of the stretched area) and its ancestral_start
	 * set to region->start() (so that we know where to begin
	 * reading if we want to stretch it again).
	 *
	 * 2. The region has been stretched before.
	 *
	 * The region starts at region->start() samples into its
	 * (possibly previously stretched) source file.  But we don't
	 * want to read from its source file; we want to read from the
	 * file it was originally stretched from.
	 *
	 * The region's source begins at region->ancestral_start()
	 * samples into its master source file.  Thus, we need to start
	 * reading at region->ancestral_start() + (region->start() /
	 * region->stretch()) samples into the master source.  This
	 * value will also become the ancestral_start for the new
	 * region.
	 *
	 * We cannot use region->ancestral_length() to establish how
	 * many samples to read, because it won't be up to date if the
	 * region has been trimmed since it was last stretched.  We
	 * must read region->length() / region->stretch() samples and
	 * stretch them by tsr.time_fraction * region->stretch(), for
	 * a new region of region->length() * tsr.time_fraction
	 * samples.
	 *
	 * Case 1 is of course a special case of 2, where
	 * region->ancestral_start() == 0 and region->stretch() == 1.
	 *
	 * When we ask to read from a region, we supply a position on
	 * the global timeline.  The read function calculates the
	 * offset into the source as (position - region->position()) +
	 * region->start().  This calculation is used regardless of
	 * whether we are reading from a master or
	 * previously-stretched region.  In order to read from a point
	 * n samples into the master source, we need to provide n -
	 * region->start() + region->position() as our position
	 * argument to master_read_at().
	 *
	 * Note that region->ancestral_length() is not used.
	 *
	 * I hope this is clear.
	 */

	double stretch = region->stretch () * tsr.time_fraction;
	double shift   = region->shift () * tsr.pitch_fraction;

	samplecnt_t read_start = region->ancestral_start_sample () +
	                         samplecnt_t (region->start_sample () / (double)region->stretch ());

	samplecnt_t read_duration = samplecnt_t (region->length_samples () / (double)region->stretch ());

	uint32_t channels = region->n_channels ();

#ifndef NDEBUG
	cerr << "RBStretcher: input-len = " << read_duration
	     << ", rate = " << session.sample_rate ()
	     << ", channels = " << channels
	     << ", opts = " << tsr.opts
	     << ", stretch = " << stretch
	     << ", shift = " << shift << endl;
#endif

	RubberBandStretcher stretcher (session.sample_rate (), channels,
	                               (RubberBandStretcher::Options)tsr.opts,
	                               stretch, shift);

	progress->set_progress (0);
	tsr.done = false;

	stretcher.setDebugLevel (1);
	stretcher.setExpectedInputDuration (read_duration);

	/* the name doesn't need to be super-precise, but allow for 2 fractional
	 * digits just to disambiguate close but not identical FX
	 */

	if (stretch == 1.0) {
		snprintf (suffix, sizeof (suffix), "@%d", (int)floor (shift * 100.0f));
	} else if (shift == 1.0) {
		snprintf (suffix, sizeof (suffix), "@%d", (int)floor (stretch * 100.0f));
	} else {
		snprintf (suffix, sizeof (suffix), "@%d-%d",
		          (int)floor (stretch * 100.0f),
		          (int)floor (shift * 100.0f));
	}

	/* create new sources */

	samplepos_t pos = 0;

	if (make_new_sources (region, nsrcs, suffix)) {
		goto out;
	}

	gain_buffer = new gain_t[bufsize];
	buffers     = new float*[channels];

	for (uint32_t i = 0; i < channels; ++i) {
		buffers[i] = new float[bufsize];
	}

	/* we read from the master (original) sources for the region,
	 * not the ones currently in use, in case it's already been
	 * subject to timefx. */

	/* study first, process afterwards. */
	try {
		while (pos < read_duration && !tsr.cancel) {
			samplecnt_t this_read = 0;

			for (uint32_t i = 0; i < channels; ++i) {
				samplepos_t this_time = min (bufsize, read_duration - pos);

				samplepos_t this_position;
				this_position = read_start + pos -
				                region->start_sample () + region->position_sample ();

				this_read = region->master_read_at (buffers[i],
				                                    buffers[i],
				                                    gain_buffer,
				                                    this_position,
				                                    this_time,
				                                    i);

				if (this_read != this_time) {
					error << string_compose (_("tempoize: error reading data from %1 at %2 (wanted %3, got %4)"),
					                         region->name (), this_position, this_time, this_read)
					      << endmsg;
					goto out;
				}
			}

			pos += this_read;

			progress->set_progress (((float)pos / read_duration) * 0.25);

			stretcher.study (buffers, this_read, pos == read_duration);
		}

		/* done studing, start process */
		pos = 0;

		while (pos < read_duration && !tsr.cancel) {
			samplecnt_t this_read = 0;

			for (uint32_t i = 0; i < channels; ++i) {
				samplepos_t this_time;
				this_time = min (bufsize, read_duration - pos);
				this_time = min (this_time, (samplepos_t)stretcher.getSamplesRequired ());

				samplepos_t this_position;
				this_position = read_start + pos -
				                region->start_sample () + region->position_sample ();

				this_read = region->master_read_at (buffers[i],
				                                    buffers[i],
				                                    gain_buffer,
				                                    this_position,
				                                    this_time,
				                                    i);

				if (this_read != this_time) {
					error << string_compose (_("tempoize: error reading data from %1 at %2 (wanted %3, got %4)"),
					                         region->name (), pos + region->position_sample (), this_time, this_read)
					      << endmsg;
					goto out;
				}
			}

			pos += this_read;

			progress->set_progress (0.25 + ((float)pos / read_duration) * 0.75);

			stretcher.process (buffers, this_read, pos == read_duration);

			samplecnt_t avail = 0;
			while ((avail = stretcher.available ()) > 0) {
				this_read = min (bufsize, avail);

				this_read = stretcher.retrieve (buffers, this_read);

				for (uint32_t i = 0; i < nsrcs.size (); ++i) {
					boost::shared_ptr<AudioSource> asrc = boost::dynamic_pointer_cast<AudioSource> (nsrcs[i]);
					if (!asrc) {
						continue;
					}

					if (asrc->write (buffers[i], this_read) != this_read) {
						error << string_compose (_("error writing tempo-adjusted data to %1"), nsrcs[i]->name ()) << endmsg;
						goto out;
					}
				}
			}
		}

		/* completing */

		samplecnt_t avail = 0;
		while ((avail = stretcher.available ()) >= 0 && !tsr.cancel) {
			if (avail == 0) {
				/* wait for stretcher threads */
				Glib::usleep (10000);
				continue;
			}

			samplecnt_t this_read = min (bufsize, avail);

			this_read = stretcher.retrieve (buffers, this_read);

			for (uint32_t i = 0; i < nsrcs.size (); ++i) {
				boost::shared_ptr<AudioSource> asrc = boost::dynamic_pointer_cast<AudioSource> (nsrcs[i]);
				if (!asrc) {
					continue;
				}

				if (asrc->write (buffers[i], this_read) !=
				    this_read) {
					error << string_compose (_("error writing tempo-adjusted data to %1"), nsrcs[i]->name ()) << endmsg;
					goto out;
				}
			}
		}

	} catch (runtime_error& err) {
		error << string_compose (_("programming error: %1"), X_("timefx code failure")) << endmsg;
		error << err.what () << endmsg;
		goto out;
	}

	new_name = region->name ();
	at       = new_name.find ('@');

	/* remove any existing stretch indicator */

	if (at != string::npos && at > 2) {
		new_name = new_name.substr (0, at - 1);
	}

	new_name += suffix;

	if (!tsr.cancel) {
		ret = finish (region, nsrcs, new_name);
	}

	/* now reset ancestral data for each new region */

	for (vector<boost::shared_ptr<Region> >::iterator x = results.begin (); x != results.end (); ++x) {
		(*x)->set_ancestral_data (timecnt_t (read_start, timepos_t()),
		                          timecnt_t (read_duration, read_start),
		                          stretch,
		                          shift);
		(*x)->set_master_sources (region->master_sources ());
		/* multiply the old (possibly previously stretched) region length by the extra
		 * stretch this time around to get its new length. this is a non-music based edit atm.
		 */
#warning NUTEMPO FIXME should use (*x)->position() sa 2nd arg also needs to figure out units for first arg
		(*x)->set_length (timecnt_t (samplepos_t ((*x)->length_samples () * tsr.time_fraction), (*x)->position_samples()));
	}

	/* stretch region gain envelope */
	/* XXX: assuming we've only processed one input region into one result here */

	if (ret == 0 && tsr.time_fraction != 1) {
		boost::shared_ptr<AudioRegion> result = boost::dynamic_pointer_cast<AudioRegion> (results.front ());
		assert (result);
		result->envelope ()->x_scale (tsr.time_fraction);
	}

out:

	delete[] gain_buffer;

	if (buffers) {
		for (uint32_t i = 0; i < channels; ++i) {
			delete[] buffers[i];
		}
		delete[] buffers;
	}

	if (ret || tsr.cancel) {
		for (SourceList::iterator si = nsrcs.begin (); si != nsrcs.end (); ++si) {
			(*si)->mark_for_remove ();
		}
	}

	return ret;
}
