/*
 * Copyright (C) 2005-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006 Hans Fugal <hans@fugal.net>
 * Copyright (C) 2008-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2008-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2015-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015 Nick Mainsbridge <mainsbridge@gmail.com>
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

#include <iostream>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <string>
#include <set>

#include "pbd/error.h"
#include "pbd/pthread_utils.h"
#include "pbd/memento_command.h"
#include "pbd/stateful_diff_command.h"

#include "ardour/audioregion.h"
#include "ardour/midi_stretch.h"
#include "ardour/pitch.h"
#include "ardour/region.h"
#include "ardour/region_factory.h"
#include "ardour/session.h"
#include "ardour/stretch.h"

#include <gtkmm2ext/utils.h>

#include "audio_region_view.h"
#include "audio_time_axis.h"
#include "editor.h"
#include "editor_regions.h"
#include "region_selection.h"
#include "time_fx_dialog.h"

#ifdef USE_RUBBERBAND
#include <rubberband/RubberBandStretcher.h>
using namespace RubberBand;
#endif

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Gtkmm2ext;

/** @return -1 in case of error, 1 if operation was cancelled by the user, 0 if everything went ok */
int
Editor::time_stretch (RegionSelection& regions, Temporal::ratio_t const & ratio)
{
	RegionList audio;
	RegionList midi;

	begin_reversible_command (_("stretch/shrink"));

	for (RegionSelection::iterator i = regions.begin(); i != regions.end(); ++i) {
		if  ((*i)->region()->data_type() == DataType::AUDIO) {
			audio.push_back ((*i)->region());
		} else if  ((*i)->region()->data_type() == DataType::MIDI) {
			midi.push_back ((*i)->region());
		}
	}

	int aret = time_fx (audio, ratio, false);
	if (aret < 0) {
		abort_reversible_command ();
		return aret;
	}

	set<boost::shared_ptr<Playlist> > midi_playlists_affected;

	for (RegionList::iterator i = midi.begin(); i != midi.end(); ++i) {
		boost::shared_ptr<Playlist> playlist = (*i)->playlist();

		if (playlist) {
			playlist->clear_changes ();
		}

	}

	ARDOUR::TimeFXRequest request;
	request.time_fraction = ratio;

	for (RegionList::iterator i = midi.begin(); i != midi.end(); ++i) {
		boost::shared_ptr<Playlist> playlist = (*i)->playlist();

		if (!playlist) {
			continue;
		}

		MidiStretch stretch (*_session, request);
		stretch.run (*i);

		playlist->replace_region (regions.front()->region(), stretch.results[0],
		                          regions.front()->region()->position());
		midi_playlists_affected.insert (playlist);
	}

	for (set<boost::shared_ptr<Playlist> >::iterator p = midi_playlists_affected.begin(); p != midi_playlists_affected.end(); ++p) {
		PBD::StatefulDiffCommand* cmd = new StatefulDiffCommand (*p);
		_session->add_command (cmd);
		if (!cmd->empty ()) {
			++aret;
		}
	}

	if (aret > 0) {
		commit_reversible_command ();
	} else {
		abort_reversible_command ();
	}

	return 0;
}

int
Editor::pitch_shift (RegionSelection& regions, float fraction)
{
	RegionList rl;

	for (RegionSelection::iterator i = regions.begin(); i != regions.end(); ++i) {
		rl.push_back ((*i)->region());
	}

	begin_reversible_command (_("pitch shift"));

	int ret = time_fx (rl, fraction, true);

	if (ret > 0) {
		commit_reversible_command ();
	} else {
		abort_reversible_command ();
	}

	return ret < 0 ? -1 : 0;
}

/** @param val Percentage to time stretch by; ignored if pitch-shifting.
 *  @param pitching true to pitch shift, false to time stretch.
 *  @return -1 in case of error, otherwise number of regions processed */
int
Editor::time_fx (RegionList& regions, float val, bool pitching)
{
	delete current_timefx;

	if (regions.empty()) {
		current_timefx = 0;
		return 0;
	}

	const timecnt_t oldlen = regions.front()->length();
	const timecnt_t newlen = regions.front()->length() * val;
	const timepos_t pos = regions.front()->position ();

	current_timefx = new TimeFXDialog (*this, pitching, oldlen, newlen, pos);
	current_timefx->regions = regions;

	switch (current_timefx->run ()) {
	case RESPONSE_ACCEPT:
		break;
	default:
		current_timefx->hide ();
		return -1;
	}

	current_timefx->status = 0;
	current_timefx->request.time_fraction = current_timefx->get_time_fraction ();
	current_timefx->request.pitch_fraction = current_timefx->get_pitch_fraction ();

	if (current_timefx->request.time_fraction == 1.0 &&
	    current_timefx->request.pitch_fraction == 1.0) {
		/* nothing to do */
		current_timefx->hide ();
		return 0;
	}

#ifdef USE_RUBBERBAND
	/* parse options */

	RubberBandStretcher::Options options = 0;

	bool realtime = false;
	bool precise = false;
	bool peaklock = true;
	bool longwin = false;
	bool shortwin = false;
	bool preserve_formants = false;
	string txt;

	enum {
		NoTransients,
		BandLimitedTransients,
		Transients
	} transients = Transients;

	precise = current_timefx->precise_button.get_active();
	preserve_formants = current_timefx->preserve_formants_button.get_active();

	txt = current_timefx->stretch_opts_selector.get_active_text ();

	for (size_t i = 0; i < rb_opt_strings.size(); i++) {
		if (txt == rb_opt_strings[i]) {
			rb_current_opt = i;
			break;
		}
	}

	int rb_mode = rb_current_opt;

	if (pitching /*&& rb_current_opt == 6*/) {
		/* The timefx dialog does not show the "stretch_opts_selector"
		 * when pitch-shifting.  So the most recently used option from
		 * "Time Stretch" would be used (if any). That may even be
		 * "resample without preserving pitch", which would be invalid.
		 *
		 * TODO: also show stretch_opts_selector when pitching (except the option
		 * to not preserve pitch) and use separate  rb_current_opt when pitching.
		 *
		 * Actually overhaul this the dialog and processing opts below and use rubberband's
		 * "Crispness" levels:
		 *   -c 0   equivalent to --no-transients --no-lamination --window-long
		 *   -c 1   equivalent to --detector-soft --no-lamination --window-long (for piano)
		 *   -c 2   equivalent to --no-transients --no-lamination
		 *   -c 3   equivalent to --no-transients
		 *   -c 4   equivalent to --bl-transients
		 *   -c 5   default processing options
		 *   -c 6   equivalent to --no-lamination --window-short (may be good for drums)
		 */
		rb_mode = 4;
	}

	switch (rb_mode) {
		case 0:
			transients = NoTransients; peaklock = false; longwin = true; shortwin = false;
			break;
		case 1:
			transients = NoTransients; peaklock = false; longwin = false; shortwin = false;
			break;
		case 2:
			transients = NoTransients; peaklock = true; longwin = false; shortwin = false;
			break;
		case 3:
			transients = BandLimitedTransients; peaklock = true; longwin = false; shortwin = false;
			break;
		case 5:
			transients = Transients; peaklock = false; longwin = false; shortwin = true;
			break;
		case 6:
			transients = NoTransients;
			precise = true;
			preserve_formants = false;
			current_timefx->request.pitch_fraction = 1.0 / current_timefx->request.time_fraction;
			shortwin = true;
			// peaklock = false;
			break;
	#ifdef HAVE_SOUNDTOUCH
		case 7:
			current_timefx->request.use_soundtouch = true;
			break;
	#endif
		default:
			/* default/4 */
			transients = Transients; peaklock = true; longwin = false; shortwin = false;
			break;
	};

	if (realtime)          options |= RubberBandStretcher::OptionProcessRealTime;
	if (precise)           options |= RubberBandStretcher::OptionStretchPrecise;
	if (preserve_formants) options |= RubberBandStretcher::OptionFormantPreserved;
	if (!peaklock)         options |= RubberBandStretcher::OptionPhaseIndependent;
	if (longwin)           options |= RubberBandStretcher::OptionWindowLong;
	if (shortwin)          options |= RubberBandStretcher::OptionWindowShort;

	if (pitching)          options |= RubberBandStretcher::OptionPitchHighQuality;

	switch (transients) {
	case NoTransients:
		options |= RubberBandStretcher::OptionTransientsSmooth;
		break;
	case BandLimitedTransients:
		options |= RubberBandStretcher::OptionTransientsMixed;
		break;
	case Transients:
		options |= RubberBandStretcher::OptionTransientsCrisp;
		break;
	}

	current_timefx->request.opts = (int) options;
#else
	current_timefx->request.quick_seek = current_timefx->quick_button.get_active();
	current_timefx->request.antialias = !current_timefx->antialias_button.get_active();
#endif
	current_timefx->request.done = false;
	current_timefx->request.cancel = false;

	/* re-connect the cancel button and delete events */

	current_timefx->first_cancel.disconnect();
	current_timefx->first_delete.disconnect();

	current_timefx->first_cancel = current_timefx->cancel_button->signal_clicked().connect
		(sigc::mem_fun (current_timefx, &TimeFXDialog::cancel_in_progress));
	current_timefx->first_delete = current_timefx->signal_delete_event().connect
		(sigc::mem_fun (current_timefx, &TimeFXDialog::delete_in_progress));


	if (pthread_create_and_store ("timefx", &current_timefx->request.thread, timefx_thread, current_timefx)) {
		current_timefx->hide ();
		error << _("timefx cannot be started - thread creation error") << endmsg;
		return -1;
	}

	current_timefx->start_updates ();

	while (!current_timefx->request.done && !current_timefx->request.cancel) {
		gtk_main_iteration ();
	}

	pthread_join (current_timefx->request.thread, 0);

	current_timefx->hide ();

	if (current_timefx->status < 0) {
		/* processing was cancelled, some regions may have
		 * been created and removed via RegionFactory::map_remove()
		 * The region-list does not update itself when a region is removed.
		 */
		_regions->redisplay ();
	}
	return current_timefx->status;
}

void
Editor::do_timefx ()
{
	typedef std::map<boost::shared_ptr<Region>, boost::shared_ptr<Region> > ResultMap;
	ResultMap results;

	uint32_t const N = current_timefx->regions.size ();

	for (RegionList::const_iterator i = current_timefx->regions.begin(); i != current_timefx->regions.end(); ++i) {
		boost::shared_ptr<Playlist> playlist = (*i)->playlist();
		if (playlist) {
			playlist->clear_changes ();
		}
	}

	for (RegionList::const_iterator i = current_timefx->regions.begin(); i != current_timefx->regions.end(); ++i) {

		boost::shared_ptr<AudioRegion> region = boost::dynamic_pointer_cast<AudioRegion> (*i);
		boost::shared_ptr<Playlist> playlist;

		if (!region || (playlist = region->playlist()) == 0) {
			continue;
		}

		if (current_timefx->request.cancel) {
			break;
		}

		Filter* fx;

		if (current_timefx->pitching) {
			fx = new Pitch (*_session, current_timefx->request);
		} else {
#ifdef USE_RUBBERBAND
		#ifdef HAVE_SOUNDTOUCH
			if (current_timefx->request.use_soundtouch) {
				fx = new STStretch (*_session, current_timefx->request);
			} else {
				fx = new RBStretch (*_session, current_timefx->request);
			}
		#else
			fx = new RBStretch (*_session, current_timefx->request);
		#endif
#else
			fx = new STStretch (*_session, current_timefx->request);
#endif
		}

		current_timefx->descend (1.0 / N);

		if (fx->run (region, current_timefx)) {
			current_timefx->request.cancel = true;
			delete fx;
			break;
		}

		if (!fx->results.empty()) {
			results[region] = fx->results.front();
		}

		current_timefx->ascend ();
		delete fx;
	}

	pthread_setcancelstate (PTHREAD_CANCEL_DISABLE, NULL);
	if (current_timefx->request.cancel) {
		current_timefx->status = -1;
		for (ResultMap::const_iterator i = results.begin(); i != results.end(); ++i) {
			boost::weak_ptr<Region> w = i->second;
			RegionFactory::map_remove (w);
		}
	} else {
		current_timefx->status = 0;
		for (ResultMap::const_iterator i = results.begin(); i != results.end(); ++i) {
			boost::shared_ptr<Region> region = i->first;
			boost::shared_ptr<Region> new_region = i->second;
			boost::shared_ptr<Playlist> playlist = region->playlist();
			playlist->replace_region (region, new_region, region->position());

			PBD::StatefulDiffCommand* cmd = new StatefulDiffCommand (playlist);
			_session->add_command (cmd);
			if (!cmd->empty ()) {
				++current_timefx->status;
			}
		}
	}
	pthread_setcancelstate (PTHREAD_CANCEL_ENABLE, NULL);

	current_timefx->request.done = true;
}

void*
Editor::timefx_thread (void *arg)
{
	SessionEvent::create_per_thread_pool ("timefx events", 64);

	TimeFXDialog* tsd = static_cast<TimeFXDialog*>(arg);

	pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, 0);

	tsd->editor.do_timefx ();

	/* GACK! HACK! sleep for a bit so that our request buffer for the GUI
	   event loop doesn't die before any changes we made are processed
	   by the GUI ...
	*/
	Glib::usleep(G_USEC_PER_SEC / 5);
	return 0;
}
