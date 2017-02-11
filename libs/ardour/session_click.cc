/*
    Copyright (C) 2002 Paul Davis

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

Pool Click::pool ("click", sizeof (Click), 1024);

void
Session::add_click (framepos_t pos, bool emphasis)
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
Session::click (framepos_t start, framecnt_t nframes)
{
	vector<TempoMap::BBTPoint> points;
	framecnt_t click_distance;

	if (_click_io == 0) {
		return;
	}

	Glib::Threads::RWLock::WriterLock clickm (click_lock, Glib::Threads::TRY_LOCK);

	/* how far have we moved since the last time the clicks got cleared
	 */

	click_distance = start - _clicks_cleared;

	if (!clickm.locked() || !_clicking || click_data == 0 || ((click_distance + nframes) < _worst_track_latency)) {
		_click_io->silence (nframes);
		return;
	}

	start -= _worst_track_latency;
	/* start could be negative at this point */
	const framepos_t end = start + nframes;
	/* correct start, potentially */
	start = max (start, (framepos_t) 0);

	_tempo_map->get_grid (points, start, end);

	if (distance (points.begin(), points.end()) == 0) {
		goto run_clicks;
	}

	for (vector<TempoMap::BBTPoint>::iterator i = points.begin(); i != points.end(); ++i) {
		switch ((*i).beat) {
		case 1:
			add_click ((*i).frame, true);
			break;
		default:
			if (click_emphasis_data == 0 || (Config->get_use_click_emphasis () == false) || (click_emphasis_data && (*i).beat != 1)) { // XXX why is this check needed ??
				add_click ((*i).frame, false);
			}
			break;
		}
	}

  run_clicks:
	clickm.release ();
	run_click (start, nframes);
}

void
Session::run_click (framepos_t start, framepos_t nframes)
{
	Glib::Threads::RWLock::ReaderLock clickm (click_lock, Glib::Threads::TRY_LOCK);

	if (!clickm.locked() || click_data == 0) {
		_click_io->silence (nframes);
		return;
	}

	Sample *buf;
	BufferSet& bufs = get_scratch_buffers(ChanCount(DataType::AUDIO, 1));
	buf = bufs.get_audio(0).data();
	memset (buf, 0, sizeof (Sample) * nframes);

	for (list<Click*>::iterator i = clicks.begin(); i != clicks.end(); ) {

		framecnt_t copy;
		framecnt_t internal_offset;
		Click *clk;

		clk = *i;

		if (clk->start < start) {
			internal_offset = 0;
		} else {
			internal_offset = clk->start - start;
		}

		if (nframes < internal_offset) {
		         /* we've just located or something..
			    effectively going backwards.
			    lets get the flock out of here */
		        break;
		}

		copy = min (clk->duration - clk->offset, nframes - internal_offset);

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
Session::setup_click_sounds (Sample** data, Sample const * default_data, framecnt_t* length, framecnt_t default_length, string const & path)
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
	_clicks_cleared = _transport_frame;
}
