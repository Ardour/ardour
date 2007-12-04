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

#include <ardour/types.h>
#include <ardour/stretch.h>
#include <ardour/audiofilesource.h>
#include <ardour/session.h>
#include <ardour/audioregion.h>

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace soundtouch;

Stretch::Stretch (Session& s, TimeFXRequest& req)
	: AudioFilter (s)
	, tsr (req)
{
	float percentage;

	/* the soundtouch code wants a *tempo* change percentage, which is 
	   of opposite sign to the length change.  
	*/

	percentage = -tsr.time_fraction;

	st.setSampleRate (s.frame_rate());
	st.setChannels (1);
	st.setTempoChange (percentage);
	st.setPitchSemiTones (0);
	st.setRateChange (0);
	
	st.setSetting(SETTING_USE_QUICKSEEK, tsr.quick_seek);
	st.setSetting(SETTING_USE_AA_FILTER, tsr.antialias);

	tsr.progress = 0.0f;
}

Stretch::~Stretch ()
{
}

int
Stretch::run (boost::shared_ptr<AudioRegion> region)
{
	SourceList nsrcs;
	nframes_t total_frames;
	nframes_t done;
	int ret = -1;
	const nframes_t bufsize = 16384;
	gain_t *gain_buffer = 0;
	Sample *buffer = 0;
	char suffix[32];
	string new_name;
	string::size_type at;

	tsr.progress = 0.0f;
	tsr.done = false;

	total_frames = region->length() * region->n_channels();
	done = 0;

	/* the name doesn't need to be super-precise, but allow for 2 fractional
	   digits just to disambiguate close but not identical stretches.
	*/
	
	snprintf (suffix, sizeof (suffix), "@%d", (int) floor (tsr.time_fraction * 100.0f));

	/* create new sources */
	
	if (make_new_sources (region, nsrcs, suffix)) {
		goto out;
	}

	gain_buffer = new gain_t[bufsize];
	buffer = new Sample[bufsize];

	// soundtouch throws runtime_error on error

	try {
		for (uint32_t i = 0; i < nsrcs.size(); ++i) {

			nframes_t pos = 0;
			nframes_t this_read = 0;

			st.clear();

			while (!tsr.cancel && pos < region->length()) {
				nframes_t this_time;
			
				this_time = min (bufsize, region->length() - pos);

				/* read from the master (original) sources for the region, 
				   not the ones currently in use, in case it's already been 
				   subject to timefx.  
				*/

				if ((this_read = region->master_read_at (buffer, buffer, gain_buffer, pos + region->position(), this_time)) != this_time) {
					error << string_compose (_("tempoize: error reading data from %1"), nsrcs[i]->name()) << endmsg;
					goto out;
				}
			
				pos += this_read;
				done += this_read;

				tsr.progress = (float) done / total_frames;
				
				st.putSamples (buffer, this_read);
			
				while ((this_read = st.receiveSamples (buffer, bufsize)) > 0 && !tsr.cancel) {
					if (nsrcs[i]->write (buffer, this_read) != this_read) {
						error << string_compose (_("error writing tempo-adjusted data to %1"), nsrcs[i]->name()) << endmsg;
						goto out;
					}
				}
			}
		
			if (!tsr.cancel) {
				st.flush ();
			}
		
			while (!tsr.cancel && (this_read = st.receiveSamples (buffer, bufsize)) > 0) {
				if (nsrcs[i]->write (buffer, this_read) != this_read) {
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

		// note: tsr.fraction is a percentage of original length. 100 = no change, 
		// 50 is half as long, 200 is twice as long, etc.

		float stretch = (*x)->stretch() * (tsr.time_fraction/100.0);

		start = (nframes_t) floor (astart + ((astart - (*x)->start()) / stretch));
		length = (nframes_t) floor (alength / stretch);

		(*x)->set_ancestral_data (start, length, stretch, (*x)->shift());
	}

  out:

	if (gain_buffer) {
		delete [] gain_buffer;
	}

	if (buffer) {
		delete [] buffer;
	}

	if (ret || tsr.cancel) {
		for (SourceList::iterator si = nsrcs.begin(); si != nsrcs.end(); ++si) {
			(*si)->mark_for_remove ();
		}
	}
	
	tsr.done = true;

	return ret;
}
