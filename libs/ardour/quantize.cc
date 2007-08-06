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

#include <pbd/basename.h>

#include <ardour/types.h>
#include <ardour/quantize.h>
#include <ardour/session.h>
#include <ardour/smf_source.h>
#include <ardour/midi_region.h>
#include <ardour/tempo.h>

#include "i18n.h"

using namespace std;
using namespace ARDOUR;


/** Quantize notes, valid for MIDI regions only.
 *
 * Q is the quantize value in beats, ie 1.0 = quantize to beats,
 * 0.25 = quantize to beats/4, etc.
 */
Quantize::Quantize (Session& s, double q)
	: Filter (s)
	, _q(q)
{
}

Quantize::~Quantize ()
{
}

int
Quantize::run (boost::shared_ptr<Region> r)
{
	boost::shared_ptr<MidiRegion> region = boost::dynamic_pointer_cast<MidiRegion>(r);
	if (!region)
		return -1;

	// FIXME: how to make a whole file region if it isn't?
	//assert(region->whole_file());

	boost::shared_ptr<MidiSource> src = region->midi_source(0);
	src->load_model();

	boost::shared_ptr<MidiModel> model = src->model();
	
	// FIXME: Model really needs to be switched to beat time (double) ASAP
	
	const Tempo& t = session.tempo_map().tempo_at(r->start());
	const Meter& m = session.tempo_map().meter_at(r->start());

	double q_frames = _q * (m.frames_per_bar(t, session.frame_rate()) / (double)m.beats_per_bar());

	for (MidiModel::Notes::iterator i = model->notes().begin(); i != model->notes().end(); ++i) {
		const double new_time = lrint(i->time() / q_frames) * q_frames;
		const double new_dur = ((i->time() != 0 && new_dur < (q_frames * 1.5))
			? q_frames
			: lrint(i->duration() / q_frames) * q_frames);
		
		i->set_time(new_time);
		i->set_duration(new_dur);
	}

	return 0;
#if 0
	SourceList nsrcs;
	SourceList::iterator si;
	nframes_t blocksize = 256 * 1024;
	Sample* buf = 0;
	nframes_t fpos;
	nframes_t fstart;
	nframes_t to_read;
	int ret = -1;

	boost::shared_ptr<AudioRegion> region = boost::dynamic_pointer_cast<AudioRegion>(r);
	if (!region)
		return ret;

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

			/* read it in */
			
			if (region->audio_source (n)->read (buf, fpos, to_read) != to_read) {
				goto out;
			}

			/* swap memory order */
			
			for (nframes_t i = 0; i < to_read/2; ++i) {
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

	if (buf) {
		delete [] buf;
	}

	if (ret) {
		for (si = nsrcs.begin(); si != nsrcs.end(); ++si) {
			boost::shared_ptr<AudioSource> asrc(boost::dynamic_pointer_cast<AudioSource>(*si));
			asrc->mark_for_remove ();
		}
	}
	
	return ret;
#endif
}
