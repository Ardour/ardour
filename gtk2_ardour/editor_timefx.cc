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

#include <pbd/error.h>
#include <pbd/pthread_utils.h>
#include <pbd/memento_command.h>

#include <gtkmm2ext/utils.h>

#include "editor.h"
#include "audio_time_axis.h"
#include "audio_region_view.h"
#include "region_selection.h"

#include <ardour/session.h>
#include <ardour/region.h>
#include <ardour/audioplaylist.h>
#include <ardour/audio_track.h>
#include <ardour/audioregion.h>
#include <ardour/audio_diskstream.h>
#include <ardour/stretch.h>
#include <ardour/pitch.h>

#ifdef USE_RUBBERBAND
#include <rubberband/RubberBandStretcher.h>
using namespace RubberBand;
#endif

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace sigc;
using namespace Gtk;
using namespace Gtkmm2ext;

Editor::TimeFXDialog::TimeFXDialog (Editor& e, bool pitch)
	: ArdourDialog (X_("time fx dialog")),
	  editor (e),
	  pitching (pitch),
	  pitch_octave_adjustment (0.0, -4.0, 4.0, 1, 2.0),
	  pitch_semitone_adjustment (0.0, -12.0, 12.0, 1.0, 4.0),
	  pitch_cent_adjustment (0.0, -499.0, 500.0, 5.0, 15.0),
	  pitch_octave_spinner (pitch_octave_adjustment),
	  pitch_semitone_spinner (pitch_semitone_adjustment),
	  pitch_cent_spinner (pitch_cent_adjustment),
	  quick_button (_("Quick but Ugly")),
	  antialias_button (_("Skip Anti-aliasing")),
	  stretch_opts_label (_("Contents:")),
	  precise_button (_("Minimize time distortion")),
	  preserve_formants_button(_("Preserve Formants"))
{
	set_modal (true);
	set_position (Gtk::WIN_POS_MOUSE);
	set_name (N_("TimeFXDialog"));

	if (pitching) {
		set_title(_("Pitch Shift"));
	} else {
		set_title(_("Time Stretch"));
	}

	cancel_button = add_button (_("Cancel"), Gtk::RESPONSE_CANCEL);

	get_vbox()->set_spacing (5);
	get_vbox()->set_border_width (12);

	if (pitching) {

		upper_button_box.set_spacing (5);
		upper_button_box.set_border_width (5);
		
		Gtk::Label* l;

		l = manage (new Label (_("Octaves")));
		upper_button_box.pack_start (*l, false, false);
		upper_button_box.pack_start (pitch_octave_spinner, false, false);

		l = manage (new Label (_("Semitones (12TET)")));
		upper_button_box.pack_start (*l, false, false);
		upper_button_box.pack_start (pitch_semitone_spinner, false, false);

		l = manage (new Label (_("Cents")));
		upper_button_box.pack_start (*l, false, false);
		upper_button_box.pack_start (pitch_cent_spinner, false, false);

		pitch_cent_spinner.set_digits (1);

		upper_button_box.pack_start (preserve_formants_button, false, false);


		add_button (_("Shift"), Gtk::RESPONSE_ACCEPT);

		get_vbox()->pack_start (upper_button_box, false, false);

	} else {

#ifdef USE_RUBBERBAND
		opts_box.set_spacing (5);
		opts_box.set_border_width (5);
		vector<string> strings;

		set_popdown_strings (stretch_opts_selector, editor.rb_opt_strings);
		/* set default */
		stretch_opts_selector.set_active_text (editor.rb_opt_strings[4]);

		opts_box.pack_start (precise_button, false, false);
		opts_box.pack_start (stretch_opts_label, false, false);
		opts_box.pack_start (stretch_opts_selector, false, false);

		get_vbox()->pack_start (opts_box, false, false);

#else
		upper_button_box.set_homogeneous (true);
		upper_button_box.set_spacing (5);
		upper_button_box.set_border_width (5);

		upper_button_box.pack_start (quick_button, true, true);
		upper_button_box.pack_start (antialias_button, true, true);

		quick_button.set_name (N_("TimeFXButton"));
		antialias_button.set_name (N_("TimeFXButton"));

		get_vbox()->pack_start (upper_button_box, false, false);

#endif	
		add_button (_("Stretch/Shrink"), Gtk::RESPONSE_ACCEPT);
	}

	get_vbox()->pack_start (progress_bar);

	progress_bar.set_name (N_("TimeFXProgress"));

	show_all_children ();
}

gint
Editor::TimeFXDialog::update_progress ()
{
	progress_bar.set_fraction (request.progress);
	return !request.done;
}

void
Editor::TimeFXDialog::cancel_in_progress ()
{
	status = -2;
	request.cancel = true;
	first_cancel.disconnect();
}

gint
Editor::TimeFXDialog::delete_in_progress (GdkEventAny* ev)
{
	status = -2;
	request.cancel = true;
	first_delete.disconnect();
	return TRUE;
}

int
Editor::time_stretch (RegionSelection& regions, float fraction)
{
	return time_fx (regions, fraction, false);
}

int
Editor::pitch_shift (RegionSelection& regions, float fraction)
{
	return time_fx (regions, fraction, true);
}

int
Editor::time_fx (RegionSelection& regions, float val, bool pitching)
{
	if (current_timefx != 0) {
		delete current_timefx;
	}

	current_timefx = new TimeFXDialog (*this, pitching);

	current_timefx->progress_bar.set_fraction (0.0f);

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

	if (txt == rb_opt_strings[0]) {
		transients = NoTransients; peaklock = false; longwin = true; shortwin = false; 
	} else if (txt == rb_opt_strings[1]) {
		transients = NoTransients; peaklock = false; longwin = false; shortwin = false; 
	} else if (txt == rb_opt_strings[2]) {
		transients = NoTransients; peaklock = true; longwin = false; shortwin = false; 
	} else if (txt == rb_opt_strings[3]) {
		transients = BandLimitedTransients; peaklock = true; longwin = false; shortwin = false; 
	} else if (txt == rb_opt_strings[5]) {
		transients = Transients; peaklock = false; longwin = false; shortwin = true; 
	} else {
		/* default/4 */

		transients = Transients; peaklock = true; longwin = false; shortwin = false; 
	};


	if (realtime)    options |= RubberBandStretcher::OptionProcessRealTime;
	if (precise)     options |= RubberBandStretcher::OptionStretchPrecise;
	if (preserve_formants)	options |= RubberBandStretcher::OptionFormantPreserved;

	if (!peaklock)   options |= RubberBandStretcher::OptionPhaseIndependent;
	if (longwin)     options |= RubberBandStretcher::OptionWindowLong;
	if (shortwin)    options |= RubberBandStretcher::OptionWindowShort;
		
		
		
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
	current_timefx->request.progress = 0.0f;
	current_timefx->request.done = false;
	current_timefx->request.cancel = false;
	
	/* re-connect the cancel button and delete events */
	
	current_timefx->first_cancel.disconnect();
	current_timefx->first_delete.disconnect();
	
	current_timefx->first_cancel = current_timefx->cancel_button->signal_clicked().connect 
		(mem_fun (current_timefx, &TimeFXDialog::cancel_in_progress));
	current_timefx->first_delete = current_timefx->signal_delete_event().connect 
		(mem_fun (current_timefx, &TimeFXDialog::delete_in_progress));

	if (pthread_create_and_store ("timefx", &current_timefx->request.thread, 0, timefx_thread, current_timefx)) {
		current_timefx->hide ();
		error << _("timefx cannot be started - thread creation error") << endmsg;
		return -1;
	}

	pthread_detach (current_timefx->request.thread);

	sigc::connection c = Glib::signal_timeout().connect (mem_fun (current_timefx, &TimeFXDialog::update_progress), 100);

	while (!current_timefx->request.done && !current_timefx->request.cancel) {
		gtk_main_iteration ();
	}

	c.disconnect ();
	
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
	
		if ((playlist = t->diskstream()->playlist()) == 0) {
			i = tmp;
			continue;
		}

		if (dialog.request.cancel) {
			/* we were cancelled */
			dialog.status = 1;
			return;
		}

		AudioFilter* fx;

		if (dialog.pitching) {
			fx = new Pitch (*session, dialog.request);
		} else {
			fx = new Stretch (*session, dialog.request);
		}

		if (fx->run (region)) {
			dialog.status = -1;
			dialog.request.done = true;
			delete fx;
			return;
		}

		if (!fx->results.empty()) {
			new_region = fx->results.front();

			if (!in_command) {
				session->begin_reversible_command (dialog.pitching ? _("pitch shift") : _("time stretch"));
				in_command = true;
			}

			XMLNode &before = playlist->get_state();
			playlist->replace_region (region, new_region, region->position());
			XMLNode &after = playlist->get_state();
			session->add_command (new MementoCommand<Playlist>(*playlist, &before, &after));
		}

		i = tmp;
		delete fx;
	}

	if (in_command) {
		session->commit_reversible_command ();
	}

	dialog.status = 0;
	dialog.request.done = true;
}

void*
Editor::timefx_thread (void *arg)
{
	PBD::notify_gui_about_thread_creation (pthread_self(), X_("TimeFX"));

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

