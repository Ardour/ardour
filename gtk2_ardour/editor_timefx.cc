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

#include <cstdlib>
#include <cmath>

#include <string>

#include <pbd/error.h>
#include <pbd/pthread_utils.h>
#include <pbd/memento_command.h>

#include <gtkmm2ext/window_title.h>

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

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace sigc;
using namespace Gtk;
using namespace Gtkmm2ext;

Editor::TimeFXDialog::TimeFXDialog (Editor& e, bool pitch)
	: ArdourDialog (X_("time fx dialog")),
	  editor (e),
	  pitching (pitch),
	  pitch_octave_adjustment (0.0, 0.0, 4.0, 1, 2.0),
	  pitch_semitone_adjustment (0.0, 0.0, 12.0, 1.0, 4.0),
	  pitch_cent_adjustment (0.0, 0.0, 150.0, 5.0, 15.0),
	  pitch_octave_spinner (pitch_octave_adjustment),
	  pitch_semitone_spinner (pitch_semitone_adjustment),
	  pitch_cent_spinner (pitch_cent_adjustment),
	  quick_button (_("Quick but Ugly")),
	  antialias_button (_("Skip Anti-aliasing"))
{
	set_modal (true);
	set_position (Gtk::WIN_POS_MOUSE);
	set_name (N_("TimeFXDialog"));

	WindowTitle title(Glib::get_application_name());
	if (pitching) {
		title += _("Pitch Shift");
	} else {
		title += _("Time Stretch");
	}
	set_title(title.get_string());

	cancel_button = add_button (_("Cancel"), Gtk::RESPONSE_CANCEL);

	get_vbox()->set_spacing (5);
	get_vbox()->set_border_width (12);
	get_vbox()->pack_start (upper_button_box, false, false);
	get_vbox()->pack_start (progress_bar);

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

		add_button (_("Shift"), Gtk::RESPONSE_ACCEPT);

	} else {

		upper_button_box.set_homogeneous (true);
		upper_button_box.set_spacing (5);
		upper_button_box.set_border_width (5);
		upper_button_box.pack_start (quick_button, true, true);
		upper_button_box.pack_start (antialias_button, true, true);
	
		add_button (_("Stretch/Shrink"), Gtk::RESPONSE_ACCEPT);
	}

	quick_button.set_name (N_("TimeFXButton"));
	antialias_button.set_name (N_("TimeFXButton"));
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
		cents += current_timefx->pitch_semitone_adjustment.get_value() * 100.0;
		cents += current_timefx->pitch_cent_adjustment.get_value();

		if (cents == 0.0) {
			// user didn't change anything
			current_timefx->hide ();
			return 0;
		}

		// we now have the pitch shift in cents. divide by 1200 to get octaves
		// then multiply by 2.0 because 1 octave == doubling the frequency
		
		cents /= 1200.0;
		cents /= 2.0;

		// add 1.0 to convert to RB scale

		cents += 1.0;

		current_timefx->request.time_fraction = 1.0;
		current_timefx->request.pitch_fraction = cents;

	} else {

		current_timefx->request.time_fraction = val;
		current_timefx->request.pitch_fraction = 1.0;

	}

	current_timefx->request.quick_seek = current_timefx->quick_button.get_active();
	current_timefx->request.antialias = !current_timefx->antialias_button.get_active();
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

	while (!current_timefx->request.done) {
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

		Filter* fx;

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
				begin_reversible_command (dialog.pitching ? _("pitch shift") : _("time stretch"));
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
		commit_reversible_command ();
	}

	dialog.status = 0;
	dialog.request.done = true;
}

void*
Editor::timefx_thread (void *arg)
{
	PBD::ThreadCreated (pthread_self(), X_("TimeFX"));

	TimeFXDialog* tsd = static_cast<TimeFXDialog*>(arg);

	pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, 0);

	tsd->editor.do_timefx (*tsd);

	return 0;
}

