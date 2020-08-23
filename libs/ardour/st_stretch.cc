/*
 * Copyright (C) 2008-2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2008-2012 Carl Hetherington <carl@carlh.net>
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
#include <cmath>

#include "pbd/error.h"

#include "ardour/types.h"
#include "ardour/stretch.h"
#include "ardour/audiofilesource.h"
#include "ardour/session.h"
#include "ardour/audioregion.h"
#include "ardour/progress.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace soundtouch;

STStretch::STStretch (Session& s, TimeFXRequest& req)
	: Filter (s)
	, tsr (req)
{
}

STStretch::~STStretch ()
{
}

int
STStretch::run (boost::shared_ptr<Region> r, Progress* progress)
{
	boost::shared_ptr<AudioRegion> region = boost::dynamic_pointer_cast<AudioRegion> (r);

	if (!region) {
		error << "STStretch::run() passed a non-audio region! WTF?" << endmsg;
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
	cerr << "STStretch: source region: position = " << region->position ()
	     << ", start = " << region->start ()
	     << ", length = " << region->length ()
	     << ", ancestral_start = " << region->ancestral_start ()
	     << ", ancestral_length = " << region->ancestral_length ()
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
	stretch = std::min(20.0, std::max(0.02, stretch));
	samplecnt_t read_start = region->ancestral_start () +
	                         samplecnt_t (region->start () / (double)region->stretch ());

	samplecnt_t read_duration =
	    samplecnt_t (region->length () / (double)region->stretch ());

	uint32_t channels = region->n_channels ();

#ifndef NDEBUG
	cerr << "RBStretcher: input-len = " << read_duration
	     << ", rate = " << session.sample_rate ()
	     << ", channels = " << channels
	     << ", opts = " << tsr.opts
	     << ", stretch = " << stretch << endl;
#endif


	soundtouch::SoundTouch st[channels];
	for (uint32_t i = 0; i < channels; ++i) {
		st[i].setSampleRate(session.sample_rate());
		st[i].setChannels(1);
		st[i].setTempo(1.0 / stretch);

		st[i].setSetting(SETTING_USE_QUICKSEEK, tsr.quick_seek);
		st[i].setSetting(SETTING_USE_AA_FILTER, tsr.antialias);
        st[i].setSetting(SETTING_SEQUENCE_MS, 40);
        st[i].setSetting(SETTING_SEEKWINDOW_MS, 15);
        st[i].setSetting(SETTING_OVERLAP_MS, 8);
	}

	progress->set_progress (0);
	tsr.done = false;


	/* the name doesn't need to be super-precise, but allow for 2 fractional
	 * digits just to disambiguate close but not identical FX
	 */

	snprintf (suffix, sizeof (suffix), "@%d", (int)floor (stretch * 100.0f));

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

	try {
		/* start process */
		pos = 0;

		while (pos < read_duration && !tsr.cancel) {
			samplecnt_t this_read = 0;

			for (uint32_t i = 0; i < channels; ++i) {
				samplepos_t this_time;
				this_time = min (bufsize, read_duration - pos);

				samplepos_t this_position;
				this_position = read_start + pos -
				                region->start () + region->position ();

				this_read = region->master_read_at (buffers[i],
				                                    buffers[i],
				                                    gain_buffer,
				                                    this_position,
				                                    this_time,
				                                    i);

				if (this_read != this_time) {
					error << string_compose (_("tempoize: error reading data from %1 at %2 (wanted %3, got %4)"),
					                         region->name (), pos + region->position (), this_time, this_read)
					      << endmsg;
					goto out;
				}

				st[i].putSamples (buffers[i], this_read);
			}
			pos += this_read;
			progress->set_progress (0.25 + ((float)pos / read_duration) * 0.75);

			for (uint32_t i = 0; i < channels; ++i) {
				samplecnt_t avail = 0;
				while ((avail = st[i].numSamples ()) > 0) {
					this_read = min (bufsize, avail);

					this_read = st[i].receiveSamples(buffers[i], this_read);
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

		if (!tsr.cancel) {
			for (uint32_t i = 0; i < channels; ++i) {
				st[i].flush ();
			}
		}

		/* completing */
		for (uint32_t i = 0; i < channels; ++i) {
			samplecnt_t avail = 0;
			samplecnt_t this_read = 0;
			while ((avail = st[i].numSamples ()) > 0) {
				this_read = min (bufsize, avail);

				this_read = st[i].receiveSamples(buffers[i], this_read);

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
		(*x)->set_ancestral_data (read_start,
		                          read_duration,
		                          stretch,
		                          1.0);
		(*x)->set_master_sources (region->master_sources ());
		/* multiply the old (possibly previously stretched) region length by the extra
		 * stretch this time around to get its new length. this is a non-music based edit atm.
		 */
		(*x)->set_length ((*x)->length () * tsr.time_fraction, 0);
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
