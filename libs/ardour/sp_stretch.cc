/*
 * Copyright (C) 2025 Robin Gareus <robin@gareus.org>
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
#include "pbd/progress.h"

#include "staffpad/TimeAndPitch.h"

#include "ardour/audiofilesource.h"
#include "ardour/audioregion.h"
#include "ardour/region_fx_plugin.h"
#include "ardour/session.h"
#include "ardour/stretch.h"
#include "ardour/types.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

SPStretch::SPStretch (Session& s, TimeFXRequest& req)
	: Filter (s)
	, tsr (req)
{
}

SPStretch::~SPStretch ()
{
}

int
SPStretch::run (std::shared_ptr<Region> r, Progress* progress)
{
	std::shared_ptr<AudioRegion> region = std::dynamic_pointer_cast<AudioRegion> (r);

	if (!region) {
		error << "SPStretch::run() passed a non-audio region! WTF?" << endmsg;
		return -1;
	}


	SourceList        nsrcs;
	int               ret     = -1;
	const samplecnt_t bufsize = 1024;
	Sample**          buffers = 0;
	char              suffix[32];
	string            new_name;
	string::size_type at;

#ifndef NDEBUG
	cerr << "SPStretch: source region: position = " << region->position ()
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

	double stretch = region->stretch () * tsr.time_fraction.to_double ();
	double shift   = region->shift () * tsr.pitch_fraction;

	samplecnt_t read_start = region->ancestral_start_sample () +
	                         samplecnt_t (region->start_sample () / (double)region->stretch ());

	samplecnt_t read_duration  = samplecnt_t (region->length_samples () / (double)region->stretch ());
	samplecnt_t write_duration = read_duration * stretch;

	assert (read_duration <= region->master_sources ().front()->length ().samples() - read_start);
	read_duration = std::min (read_duration, region->master_sources ().front()->length ().samples() - read_start);

	uint32_t channels = region->n_channels ();

	std::vector<staffpad::TimeAndPitch*> tap;

	if (channels > 2) {
		/* multiple mono */
		for (uint32_t i = 0; i < channels; ++i) {
			tap.push_back (new staffpad::TimeAndPitch (session.sample_rate () > 48000 ? 8192 : 4096));
			tap.back()->setup (1, bufsize);
			tap.back()->setTimeStretchAndPitchFactor (stretch, shift);
		}
	} else {
		/* mono or mid/side stereo */
		tap.push_back (new staffpad::TimeAndPitch (session.sample_rate () > 48000 ? 8192 : 4096));
		tap.back()->setup (channels, bufsize);
		tap.back()->setTimeStretchAndPitchFactor (stretch, shift);
	}

	int latency = tap[0]->getLatencySamplesForStretchRatio (stretch * shift);

#ifndef NDEBUG
	cerr << "SPStretcher: input-len = " << read_duration
	     << ", rate = " << session.sample_rate ()
	     << ", channels = " << channels
	     << ", stretch = " << stretch
	     << ", latencty = " << latency
	     << ", output-len = " << write_duration << endl;
#endif

	progress->set_progress (0);
	tsr.done = false;

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
	if (make_new_sources (region, nsrcs, suffix)) {
		goto out;
	}

	/* and allocate buffers .. */
	buffers = new float*[channels];
	for (uint32_t i = 0; i < channels; ++i) {
		buffers[i] = new float[bufsize];
	}

	/* start process */
	try {
		samplepos_t pos     = 0;
		samplepos_t written = 0;

		while (written < write_duration && !tsr.cancel) {
			samplecnt_t available;

			if (tap[0]->getSamplesToNextHop () <= 0 && tap[0]->getNumAvailableOutputSamples () <= 0) {
				std::runtime_error ("StaffPad: does not accept samples.");
			}

			while ((available = tap[0]->getNumAvailableOutputSamples ()) <= 0) {
				samplecnt_t required = tap[0]->getSamplesToNextHop ();

				while (required > 0) {
					samplecnt_t to_feed = std::min (bufsize, required);
					samplecnt_t to_read = std::min (to_feed, read_duration - pos);

					for (uint32_t i = 0; i < channels; ++i) {
						samplepos_t this_position = read_start + pos -
						                            region->start_sample () + region->position_sample ();

						/* we read from the master (original) sources for the region,
						 * not the ones currently in use, in case it's already been
						 * subject to timefx. */

						samplecnt_t this_read = region->master_read_at (buffers[i],
						                                                this_position,
						                                                to_read,
						                                                i);

						if (this_read != to_read) {
							error << string_compose (_("tempoize: error reading data from %1 at %2 (wanted %3, got %4)"),
							                         region->name (), pos + region->position_sample (), to_read, this_read)
							      << endmsg;
							goto out;
						}
					}

					if (to_feed > to_read) {
						/* zero pad */
						for (uint32_t i = 0; i < channels; ++i) {
							memset (&buffers[i][to_read], 0, sizeof (float) * (to_feed - to_read));
						}
					}

					if (channels > 2) {
						for (uint32_t i = 0; i < channels; ++i) {
							tap[i]->feedAudio (&buffers[i], to_feed);
						}
					} else {
						tap[0]->feedAudio (buffers, to_feed);
					}

					required -= to_feed;
					pos      += to_read;
				}
			}

			while (written < write_duration && available > 0) {
				samplecnt_t this_read;
				this_read = std::min<samplecnt_t> (available, bufsize);
				this_read = std::min<samplecnt_t> (this_read, write_duration - written);

				if (channels > 2) {
					for (uint32_t i = 0; i < channels; ++i) {
						tap[i]->retrieveAudio (&buffers[i], this_read);
					}
				} else {
					tap[0]->retrieveAudio (buffers, this_read);
				}

				available -= this_read;

				if (latency >= this_read) {
					latency -= this_read;
					continue;
				}
				if (latency > 0) {
					for (uint32_t i = 0; i < channels; ++i) {
						memmove (buffers[i], &buffers[i][latency], sizeof (float) * (this_read - latency));
					}
					this_read -= latency;
					latency = 0;
				}

				for (uint32_t i = 0; i < nsrcs.size (); ++i) {
					std::shared_ptr<AudioSource> asrc = std::dynamic_pointer_cast<AudioSource> (nsrcs[i]);
					if (!asrc) {
						continue;
					}

					if (asrc->write (buffers[i], this_read) != this_read) {
						error << string_compose (_("error writing tempo-adjusted data to %1"), nsrcs[i]->name ()) << endmsg;
						goto out;
					}
				}
				written += this_read;
			}

			progress->set_progress ((float)written / write_duration);
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

	/* apply automation scaling before calling set_length, which trims automation */
	if (ret == 0 && !tsr.time_fraction.is_unity ()) {
		for (auto& r : results) {
			std::shared_ptr<AudioRegion> ar = std::dynamic_pointer_cast<AudioRegion> (r);
			assert (ar);
			ar->envelope ()->x_scale (tsr.time_fraction);
			ar->foreach_plugin ([&] (std::weak_ptr<RegionFxPlugin> wfx) {
				shared_ptr<RegionFxPlugin> rfx = wfx.lock ();
				if (rfx) {
					rfx->x_scale_automation (tsr.time_fraction);
				}
			});
		}
	}

	/* now reset ancestral data for each new region */

	for (vector<std::shared_ptr<Region>>::iterator x = results.begin (); x != results.end (); ++x) {
		(*x)->set_ancestral_data (timepos_t (read_start),
		                          timecnt_t (read_duration, timepos_t (read_start)),
		                          stretch,
		                          shift);
		(*x)->set_master_sources (region->master_sources ());
		/* multiply the old (possibly previously stretched) region length by the extra
		 * stretch this time around to get its new length. this is a non-music based edit atm.
		 */
		(*x)->set_length_unchecked ((*x)->length ().scale (tsr.time_fraction));
		(*x)->set_whole_file (true);
	}

out:

	if (buffers) {
		for (uint32_t i = 0; i < channels; ++i) {
			delete[] buffers[i];
		}
		delete[] buffers;
	}

	for (auto const& t: tap) {
		delete t;
	}

	if (ret || tsr.cancel) {
		for (SourceList::iterator si = nsrcs.begin (); si != nsrcs.end (); ++si) {
			(*si)->mark_for_remove ();
		}
	}

	return ret;
}
