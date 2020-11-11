/*
 * Copyright (C) 2002-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2006-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2008-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2015-2017 Robin Gareus <robin@gareus.org>
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

#include <list>
#include <cerrno>

#include "ardour/amp.h"
#include "ardour/audio_buffer.h"
#include "ardour/buffer_set.h"
#include "ardour/click.h"
#include "ardour/io.h"
#include "ardour/session.h"
#include "ardour/tempo.h"
#include "ardour/types.h"

#include <sndfile.h>

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Temporal;

Pool Click::pool ("click", sizeof (Click), 1024);

/* pre-allocated vector for grid-point-lookup.
 *
 * Since Session::click() is never called concurrently
 * from different threads, this can be static-global.
 * (session.h does not include tempo.h so making this
 *  a Session member variable is tricky.)
 */
static TempoMapPoints _click_points;

void
Session::add_click (samplepos_t pos, bool emphasis)
{
	if (emphasis) {
		if (click_emphasis_data && Config->get_use_click_emphasis () == true) {
			clicks.push_back (new Click (pos, click_emphasis_length, click_emphasis_data));
		} else if (click_data && Config->get_use_click_emphasis () == false) {
			clicks.push_back (new Click (pos, click_length, click_data));
		}
	} else if (click_data) {
		clicks.push_back (new Click (pos, click_length, click_data));
	}
}

void
Session::click (samplepos_t cycle_start, samplecnt_t nframes)
{
	if (_click_io == 0) {
		return;
	}

	/* transport_frame is audible-frame (what you hear,
	 * incl output latency). So internally we're ahead,
	 * we need to prepare frames that the user will hear
	 * in "output latency's" worth of time.
	 */
	samplecnt_t offset = _click_io_latency;

	Glib::Threads::RWLock::WriterLock clickm (click_lock, Glib::Threads::TRY_LOCK);

	/* how far have we moved since the last time the clicks got cleared */
	const samplecnt_t click_distance = cycle_start + offset - _clicks_cleared;

	if (!clickm.locked() || !_clicking || click_data == 0 || ((click_distance + nframes) < 0)) {
		_click_io->silence (nframes);
		return;
	}

	if (_click_rec_only && !actively_recording()) {
		return;
	}

	/* range to check for clicks */
	samplepos_t start = cycle_start + offset;
	/* correct start, potentially */
	start = max (start, (samplepos_t) 0);

	samplecnt_t remain = nframes;

	while (remain > 0) {
		samplecnt_t move = remain;

		Location* loop_location = get_play_loop () ? locations()->auto_loop_location () : NULL;
		if (loop_location) {
			const samplepos_t loop_start = loop_location->start_sample();
			const samplepos_t loop_end = loop_location->end_sample();
			if (start >= loop_end) {
				samplecnt_t off = (start - loop_end) % (loop_end - loop_start);
				start = loop_start + off;
				move = std::min (remain, loop_end - start);
			} else if (start + move >= loop_end) {
				move = std::min (remain, loop_end - start);
			}
			if (move == 0) {
				start = loop_start;
				const samplecnt_t looplen = loop_end - loop_start;
				move = std::min (remain, looplen);
			}
		}

		const samplepos_t end = start + move;

		_click_points.clear ();
		_tempo_map->get_grid (_click_points, samples_to_superclock (start, sample_rate()), samples_to_superclock (end, sample_rate()));

		if (distance (_click_points.begin(), _click_points.end()) == 0) {
			start += move;
			remain -= move;
			continue;
		}

		for (TempoMapPoints::iterator i = _click_points.begin(); i != _click_points.end(); ++i) {

			assert (superclock_to_samples ((*i).sclock(), sample_rate()) >= start && superclock_to_samples ((*i).sclock(), sample_rate()) < end);

			if (i->bbt().is_bar() && (click_emphasis_data && Config->get_use_click_emphasis())) {
				add_click ((*i).sclock(), true);
			} else {
				add_click ((*i).sclock(), false);
			}
		}

		start += move;
		remain -= move;
	}

	clickm.release ();
	run_click (cycle_start, nframes);
}

void
Session::run_click (samplepos_t start, samplepos_t nframes)
{
	Glib::Threads::RWLock::ReaderLock clickm (click_lock, Glib::Threads::TRY_LOCK);

	/* align to output */
	start += _click_io_latency;

	if (!clickm.locked() || click_data == 0) {
		_click_io->silence (nframes);
		return;
	}

	Sample *buf;
	BufferSet& bufs = get_scratch_buffers(ChanCount(DataType::AUDIO, 1));
	buf = bufs.get_audio(0).data();
	memset (buf, 0, sizeof (Sample) * nframes);

	/* given a large output latency, `start' can be offset by by > 1 cycle.
	 * and needs to be mapped back into the loop-range */
	Location* loop_location = get_play_loop () ? locations()->auto_loop_location () : NULL;
	if (_count_in_samples > 0) {
		loop_location = NULL;
	}
	bool crossloop = false;
	samplecnt_t span = nframes;
	if (loop_location) {
		const samplepos_t loop_start = loop_location->start_sample ();
		const samplepos_t loop_end = loop_location->end_sample ();
		if (start >= loop_end) {
			samplecnt_t off = (start - loop_end) % (loop_end - loop_start);
			start = loop_start + off;
			span = std::min (nframes, loop_end - start);
		} else if (start + nframes >= loop_end) {
			crossloop = true;
			span = std::min (nframes, loop_end - start);
		}
	}

	for (list<Click*>::iterator i = clicks.begin(); i != clicks.end(); ) {

		Click *clk = *i;

		if (loop_location) {
			const samplepos_t loop_start = loop_location->start_sample ();
			const samplepos_t loop_end = loop_location->end_sample ();
			/* remove any clicks that are outside loop location, and not currently playing */
			if ((clk->start < loop_start || clk->start >= loop_end) && clk->offset == 0) {
				delete clk;
				i = clicks.erase (i);
				continue;
			}
		}

		samplecnt_t internal_offset;

		if (clk->start <= start || clk->offset > 0) {
			internal_offset = 0;
		} else if (clk->start < start + span) {
			/* queue click at offset in current cycle */
			internal_offset = clk->start - start;
		} else if (crossloop) {
			/* When loop wraps around in current cycle, take
			 * clicks at loop-start into account */
			const samplepos_t loop_start = loop_location->start_sample ();
			internal_offset = clk->start - loop_start + span;
		} else if (_count_in_samples > 0) {
			++i;
			continue;
		} else {
			/* this can happen when locating
			 * with an active click */
			delete clk;
			i = clicks.erase (i);
			continue;
		}

		if (internal_offset >= nframes) {
			break;
		}

		samplecnt_t copy = min (clk->duration - clk->offset, nframes - internal_offset);
		memcpy (buf + internal_offset, &clk->data[clk->offset], copy * sizeof (Sample));
		clk->offset += copy;

		if (clk->offset >= clk->duration) {
			delete clk;
			i = clicks.erase (i);
		} else {
			++i;
		}
	}

	_click_gain->run (bufs, 0, 0, 1.0, nframes, false);
	_click_io->copy_to_outputs (bufs, DataType::AUDIO, nframes, 0);
}

void
Session::setup_click_sounds (Sample** data, Sample const * default_data, samplecnt_t* length, samplecnt_t default_length, string const & path)
{
	if (*data != default_data) {
		delete[] *data;
		*data = 0;
	}

	if (path.empty ()) {

		*data = const_cast<Sample*> (default_data);
		*length = default_length;

	} else {

		SF_INFO info;
		SNDFILE* sndfile;

		info.format = 0;
		if ((sndfile = sf_open (path.c_str(), SFM_READ, &info)) == 0) {
			char errbuf[256];
			sf_error_str (0, errbuf, sizeof (errbuf) - 1);
			warning << string_compose (_("cannot open click soundfile %1 (%2)"), path, errbuf) << endmsg;
			_clicking = false;
			return;
		}

		/* read the (possibly multi-channel) click data into a temporary buffer */

		sf_count_t const samples = info.frames * info.channels;

		Sample* tmp = new Sample[samples];

		if (sf_readf_float (sndfile, tmp, info.frames) != info.frames) {

			warning << _("cannot read data from click soundfile") << endmsg;
			*data = 0;
			_clicking = false;

		} else {

			*data = new Sample[info.frames];
			*length = info.frames;

			/* mix down to mono */

			for (int i = 0; i < info.frames; ++i) {
				(*data)[i] = 0;
				for (int j = 0; j < info.channels; ++j) {
					(*data)[i] = tmp[i * info.channels + j];
				}
				(*data)[i] /= info.channels;
			}
		}

		delete[] tmp;
		sf_close (sndfile);
	}
}

void
Session::setup_click_sounds (int which)
{
	clear_clicks ();

	if (which == 0 || which == 1) {
		setup_click_sounds (
			&click_data,
			default_click,
			&click_length,
			default_click_length,
			Config->get_click_sound ()
			);
	}

	if (which == 0 || which == -1) {
		setup_click_sounds (
			&click_emphasis_data,
			default_click_emphasis,
			&click_emphasis_length,
			default_click_emphasis_length,
			Config->get_click_emphasis_sound ()
			);
	}
}

void
Session::clear_clicks ()
{
	Glib::Threads::RWLock::WriterLock lm (click_lock);

	for (Clicks::iterator i = clicks.begin(); i != clicks.end(); ++i) {
		delete *i;
	}

	clicks.clear ();
	_clicks_cleared = _transport_sample;
}

void
Session::click_io_resync_latency (bool playback)
{
	if (deletion_in_progress() || !playback) {
		return;
	}

	_click_io_latency = _click_io->connected_latency (true);
}
