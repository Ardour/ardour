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

#include <pbd/error.h>
#include <rubberband/RubberBandStretcher.h>

#include <ardour/types.h>
#include <ardour/pitch.h>
#include <ardour/audiofilesource.h>
#include <ardour/session.h>
#include <ardour/audioregion.h>

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace RubberBand;

Pitch::Pitch (Session& s, TimeFXRequest& req)
	: AudioFilter (s)
	, tsr (req)

{
	tsr.progress = 0.0f;
}

Pitch::~Pitch ()
{
}

int
Pitch::run (boost::shared_ptr<AudioRegion> region)
{
	SourceList nsrcs;
	nframes_t done;
	int ret = -1;
	const nframes_t bufsize = 256;
	gain_t* gain_buffer = 0;
	Sample** buffers = 0;
	char suffix[32];
	string new_name;
	string::size_type at;
	nframes_t pos = 0;
	int avail = 0;

	RubberBandStretcher pitcher (session.frame_rate(), region->n_channels(),
				     RubberBandStretcher::DefaultOptions,
				     1.0, tsr.fraction);
	
	pitcher.setExpectedInputDuration(region->length());
	pitcher.setDebugLevel(1);

	tsr.progress = 0.0f;
	tsr.done = false;

	uint32_t channels = region->n_channels();
	nframes_t duration = region->length();

	/* the name doesn't need to be super-precise, but allow for 2 fractional
	   digits just to disambiguate close but not identical stretches.
	*/
	
	snprintf (suffix, sizeof (suffix), "@%d", (int) floor (tsr.fraction * 100.0f));

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
		while (pos < duration && !tsr.cancel) {
			
			nframes_t this_read = 0;
			
			for (uint32_t i = 0; i < channels; ++i) {
				
				this_read = 0;
				nframes_t this_time;
				
				this_time = min(bufsize, duration - pos);
				
				this_read = region->master_read_at
					(buffers[i],
					 buffers[i],
					 gain_buffer,
					 pos + region->position(),
					 this_time,
					 i);
				
				if (this_read != this_time) {
					error << string_compose
						(_("tempoize: error reading data from %1"),
						 nsrcs[i]->name()) << endmsg;
					goto out;
				}
			}
			
			pos += this_read;
			done += this_read;

			tsr.progress = ((float) done / duration) * 0.75;

			pitcher.study(buffers, this_read, pos == duration);
		}
		
		done = 0;
		pos = 0;

		while (pos < duration && !tsr.cancel) {
			
			nframes_t this_read = 0;
			
			for (uint32_t i = 0; i < channels; ++i) {
				
				this_read = 0;
				nframes_t this_time;
				
				this_time = min(bufsize, duration - pos);
				
				this_read = region->master_read_at
					(buffers[i],
					 buffers[i],
					 gain_buffer,
					 pos + region->position(),
					 this_time,
					 i);
				
				if (this_read != this_time) {
					error << string_compose
						(_("tempoize: error reading data from %1"),
						 nsrcs[i]->name()) << endmsg;
					goto out;
				}
			}

			pos += this_read;
			done += this_read;

			tsr.progress = 0.75 + ((float) done / duration) * 0.25;

			pitcher.process(buffers, this_read, pos == duration);

			int avail = 0;

			while ((avail = pitcher.available()) > 0) {

				this_read = min(bufsize, uint32_t(avail));

				pitcher.retrieve(buffers, this_read);
			
				for (uint32_t i = 0; i < nsrcs.size(); ++i) {

					if (nsrcs[i]->write(buffers[i], this_read) !=
					    this_read) {
						error << string_compose (_("error writing tempo-adjusted data to %1"), nsrcs[i]->name()) << endmsg;
						goto out;
					}
				}
			}
		}

		while ((avail = pitcher.available()) >= 0) {

			uint32_t this_read = min(bufsize, uint32_t(avail));

			pitcher.retrieve(buffers, this_read);

			for (uint32_t i = 0; i < nsrcs.size(); ++i) {

				if (nsrcs[i]->write(buffers[i], this_read) !=
				    this_read) {
					error << string_compose (_("error writing tempo-adjusted data to %1"), nsrcs[i]->name()) << endmsg;
					goto out;
				}
			}
		}

	} catch (runtime_error& err) {
		error << _("timefx code failure. please notify ardour-developers.") << endmsg;
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

	for (vector<boost::shared_ptr<AudioRegion> >::iterator x = results.begin(); x != results.end(); ++x) {
		nframes64_t astart = (*x)->ancestral_start();
		nframes64_t alength = (*x)->ancestral_length();
		nframes_t start;
		nframes_t length;

		float shift = (*x)->shift() * (tsr.fraction/100.0);

		(*x)->set_ancestral_data ((*x)->ancestral_start(), (*x)->ancestral_length(), (*x)->stretch(),  shift);
	}

  out:

	if (gain_buffer) {
		delete [] gain_buffer;
	}

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
	
	tsr.done = true;

	return ret;
}
