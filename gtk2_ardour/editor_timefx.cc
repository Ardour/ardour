/*
    Copyright (C) 2000 Paul Davis

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

#include <iostream>
#include <cstdlib>
#include <cmath>
#include <ctime>

#include <string>

#include "pbd/error.h"
#include "pbd/pthread_utils.h"
#include "pbd/memento_command.h"
#include "pbd/stateful_diff_command.h"

#include <gtkmm2ext/utils.h>

#include "audio_region_view.h"
#include "audio_time_axis.h"
#include "editor.h"
#include "region_selection.h"
#include "time_fx_dialog.h"

#include "ardour/session.h"
#include "ardour/region.h"
#include "ardour/audioplaylist.h"
#include "ardour/audio_track.h"
#include "ardour/audioregion.h"
#include "ardour/stretch.h"
#include "ardour/midi_stretch.h"
#include "ardour/pitch.h"

#ifdef USE_RUBBERBAND
#include "rubberband/RubberBandStretcher.h"
using namespace RubberBand;
#endif

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Gtkmm2ext;

/** @return -1 in case of error, 1 if operation was cancelled by the user, 0 if everything went ok */
int
Editor::time_stretch (RegionSelection& regions, float fraction)
{
	// FIXME: kludge, implement stretching of selection of both types

	if (regions.front()->region()->data_type() == DataType::AUDIO) {
		// Audio, pop up timefx dialog
		return time_fx (regions, fraction, false);
	} else {
		// MIDI, just stretch
		RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (&regions.front()->get_time_axis_view());
		if (!rtv)
			return -1;

		boost::shared_ptr<Playlist> playlist = rtv->track()->playlist();

		ARDOUR::TimeFXRequest request;
		request.time_fraction = fraction;
		MidiStretch stretch(*_session, request);
		begin_reversible_command ("midi stretch");
		stretch.run(regions.front()->region());
                playlist->clear_changes ();
		playlist->replace_region (regions.front()->region(), stretch.results[0],
				regions.front()->region()->position());
		_session->add_command (new StatefulDiffCommand (playlist));
		commit_reversible_command ();
	}

	return 0;
}

int
Editor::pitch_shift (RegionSelection& regions, float fraction)
{
	return time_fx (regions, fraction, true);
}

/** @param val Percentage to time stretch by; ignored if pitch-shifting.
 *  @param pitching true to pitch shift, false to time stretch.
 *  @return -1 in case of error, 1 if operation was cancelled by the user, 0 if everything went ok */
int
Editor::time_fx (RegionSelection& regions, float val, bool pitching)
{
	delete current_timefx;

	current_timefx = new TimeFXDialog (*this, pitching);

	switch (current_timefx->run ()) {
	case RESPONSE_ACCEPT:
		break;
	default:
		current_timefx->hide ();
		return 1;
	}

	current_timefx->status = 0;
	current_timefx->regions = regions;

	if (pitching) {

		float cents = current_timefx->pitch_octave_adjustment.get_value() * 1200.0;
		float pitch_fraction;
		cents += current_timefx->pitch_semitone_adjustment.get_value() * 100.0;
		cents += current_timefx->pitch_cent_adjustment.get_value();

		if (cents == 0.0) {
			// user didn't change anything
			current_timefx->hide ();
			return 0;
		}

		// one octave == 1200 cents
		// adding one octave doubles the frequency
		// ratio is 2^^octaves

		pitch_fraction = pow(2, cents/1200);

		current_timefx->request.time_fraction = 1.0;
		current_timefx->request.pitch_fraction = pitch_fraction;

	} else {

		current_timefx->request.time_fraction = val;
		current_timefx->request.pitch_fraction = 1.0;

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

	for (int i = 0; i <= 6; i++) {
		if (txt == rb_opt_strings[i]) {
			rb_current_opt = i;
			break;
		}
	}

	switch (rb_current_opt) {
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
			current_timefx->request.pitch_fraction = 1/val;
			shortwin = true;
			// peaklock = false;
			break;
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

	pthread_detach (current_timefx->request.thread);

	while (!current_timefx->request.done && !current_timefx->request.cancel) {
		gtk_main_iteration ();
	}

	current_timefx->hide ();
	return current_timefx->status;
}

void
Editor::do_timefx (TimeFXDialog& dialog)
{
	Track*    t;
	boost::shared_ptr<Playlist> playlist;
	boost::shared_ptr<Region>   new_region;
	bool in_command = false;

	uint32_t const N = dialog.regions.size ();

	for (RegionSelection::iterator i = dialog.regions.begin(); i != dialog.regions.end(); ) {
		AudioRegionView* arv = dynamic_cast<AudioRegionView*>(*i);

		if (!arv) {
			continue;
		}

		boost::shared_ptr<AudioRegion> region (arv->audio_region());
		TimeAxisView* tv = &(arv->get_time_axis_view());
		RouteTimeAxisView* rtv;
		RegionSelection::iterator tmp;

		tmp = i;
		++tmp;

		if ((rtv = dynamic_cast<RouteTimeAxisView*> (tv)) == 0) {
			i = tmp;
			continue;
		}

		if ((t = dynamic_cast<Track*> (rtv->route().get())) == 0) {
			i = tmp;
			continue;
		}

		if ((playlist = t->playlist()) == 0) {
			i = tmp;
			continue;
		}

		if (dialog.request.cancel) {
			/* we were cancelled */
			dialog.status = 1;
			return;
		}

		Filter* fx;

		if (dialog.pitching) {
			fx = new Pitch (*_session, dialog.request);
		} else {
#ifdef USE_RUBBERBAND
			fx = new RBStretch (*_session, dialog.request);
#else
			fx = new STStretch (*_session, dialog.request);
#endif
		}

		current_timefx->descend (1.0 / N);

		if (fx->run (region, current_timefx)) {
			dialog.status = -1;
			dialog.request.done = true;
			delete fx;
			return;
		}

		if (!fx->results.empty()) {
			new_region = fx->results.front();

			if (!in_command) {
				_session->begin_reversible_command (dialog.pitching ? _("pitch shift") : _("time stretch"));
				in_command = true;
			}

                        playlist->clear_changes ();
			playlist->replace_region (region, new_region, region->position());
			_session->add_command (new StatefulDiffCommand (playlist));
		}

		current_timefx->ascend ();

		i = tmp;
		delete fx;
	}

	if (in_command) {
		_session->commit_reversible_command ();
	}

	dialog.status = 0;
	dialog.request.done = true;
}

void*
Editor::timefx_thread (void *arg)
{
	SessionEvent::create_per_thread_pool ("timefx events", 64);

	TimeFXDialog* tsd = static_cast<TimeFXDialog*>(arg);

	pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, 0);

	tsd->editor.do_timefx (*tsd);

        /* GACK! HACK! sleep for a bit so that our request buffer for the GUI
           event loop doesn't die before any changes we made are processed
           by the GUI ...
        */

        struct timespec t = { 2, 0 };
        nanosleep (&t, 0);

	return 0;
}

