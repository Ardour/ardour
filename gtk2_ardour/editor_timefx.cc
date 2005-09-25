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

    $Id$
*/

#include <cstdlib>
#include <cmath>

#include <string>

#include <pbd/error.h>
#include <pbd/pthread_utils.h>

#include "editor.h"
#include "audio_time_axis.h"
#include "regionview.h"
#include "region_selection.h"

#include <ardour/session.h>
#include <ardour/region.h>
#include <ardour/audioplaylist.h>
#include <ardour/audio_track.h>
#include <ardour/audioregion.h>
#include <ardour/diskstream.h>

#include "i18n.h"

using namespace ARDOUR;
using namespace SigC;
using namespace Gtk;

Editor::TimeStretchDialog::TimeStretchDialog (Editor& e)
	: ArdourDialog ("time stretch dialog"),
	  editor (e),
	  quick_button (_("Quick but Ugly")),
	  antialias_button (_("Skip Anti-aliasing")),
	  cancel_button (_("Cancel")),
	  action_button (_("Stretch/Shrink it"))
{
	set_modal (true);
	set_position (GTK_WIN_POS_MOUSE);
	set_title (_("ardour: timestretch"));
	set_name (N_("TimeStretchDialog"));

	set_hide_on_stop (false);

	add (packer);

	packer.set_spacing (5);
	packer.set_border_width (5);
	packer.pack_start (upper_button_box);
	packer.pack_start (progress_bar);
	packer.pack_start (lower_button_box);
	
	upper_button_box.set_homogeneous (true);
	upper_button_box.set_spacing (5);
	upper_button_box.set_border_width (5);
	upper_button_box.pack_start (quick_button, true, true);
	upper_button_box.pack_start (antialias_button, true, true);

	lower_button_box.set_homogeneous (true);
	lower_button_box.set_spacing (5);
	lower_button_box.set_border_width (5);
	lower_button_box.pack_start (action_button, true, true);
	lower_button_box.pack_start (cancel_button, true, true);

	action_button.set_name (N_("TimeStretchButton"));
	cancel_button.set_name (N_("TimeStretchButton"));
	quick_button.set_name (N_("TimeStretchButton"));
	antialias_button.set_name (N_("TimeStretchButton"));
	progress_bar.set_name (N_("TimeStretchProgress"));

	action_button.clicked.connect (bind (slot (*this, &ArdourDialog::stop), 1));
}

gint
Editor::TimeStretchDialog::update_progress ()
{
	progress_bar.set_percentage (request.progress);
	return request.running;
}

void
Editor::TimeStretchDialog::cancel_timestretch_in_progress ()
{
	status = -2;
	request.running = false;
}

gint
Editor::TimeStretchDialog::delete_timestretch_in_progress (GdkEventAny* ev)
{
	status = -2;
	request.running = false;
	return TRUE;
}

int
Editor::run_timestretch (AudioRegionSelection& regions, float fraction)
{
	pthread_t thread;

	if (current_timestretch == 0) {
		current_timestretch = new TimeStretchDialog (*this);
	}

	current_timestretch->progress_bar.set_percentage (0.0f);
	current_timestretch->first_cancel = current_timestretch->cancel_button.clicked.connect (bind (slot (*current_timestretch, &ArdourDialog::stop), -1));
	current_timestretch->first_delete = current_timestretch->delete_event.connect (slot (*current_timestretch, &ArdourDialog::wm_close_event));

	current_timestretch->run ();

	if (current_timestretch->run_status() != 1) {
		current_timestretch->close ();
		return 1; /* no error, but we did nothing */
	}

	current_timestretch->status = 0;
	current_timestretch->regions = regions;
	current_timestretch->request.fraction = fraction;
	current_timestretch->request.quick_seek = current_timestretch->quick_button.get_active();
	current_timestretch->request.antialias = !current_timestretch->antialias_button.get_active();
	current_timestretch->request.progress = 0.0f;
	current_timestretch->request.running = true;
	
	/* re-connect the cancel button and delete events */
	
	current_timestretch->first_cancel.disconnect();
	current_timestretch->first_delete.disconnect();
	
	current_timestretch->cancel_button.clicked.connect (slot (current_timestretch, &TimeStretchDialog::cancel_timestretch_in_progress));
	current_timestretch->delete_event.connect (slot (current_timestretch, &TimeStretchDialog::delete_timestretch_in_progress));

	if (pthread_create_and_store ("timestretch", &thread, 0, timestretch_thread, current_timestretch)) {
		current_timestretch->close ();
		error << _("timestretch cannot be started - thread creation error") << endmsg;
		return -1;
	}

	pthread_detach (thread);

	SigC::Connection c = Main::timeout.connect (slot (current_timestretch, &TimeStretchDialog::update_progress), 100);

	while (current_timestretch->request.running) {
		gtk_main_iteration ();
	}

	c.disconnect ();

	current_timestretch->close ();
	return current_timestretch->status;
}

void
Editor::do_timestretch (TimeStretchDialog& dialog)
{
	AudioTrack* at;
	Playlist* playlist;
	AudioRegion* new_region;

	for (AudioRegionSelection::iterator i = dialog.regions.begin(); i != dialog.regions.end(); ) {

		AudioRegion& aregion ((*i)->region);
		TimeAxisView* tv = &(*i)->get_time_axis_view();
		AudioTimeAxisView* atv;
		AudioRegionSelection::iterator tmp;
		
		tmp = i;
		++tmp;

		if ((atv = dynamic_cast<AudioTimeAxisView*> (tv)) == 0) {
			i = tmp;
			continue;
		}

		if ((at = dynamic_cast<AudioTrack*> (&atv->route())) == 0) {
			i = tmp;
			continue;
		}
	
		if ((playlist = at->disk_stream().playlist()) == 0) {
			i = tmp;
			continue;
		}

		dialog.request.region = &aregion;

		if (!dialog.request.running) {
			/* we were cancelled */
			dialog.status = 1;
			return;
		}

		if ((new_region = session->tempoize_region (dialog.request)) == 0) {
			dialog.status = -1;
			dialog.request.running = false;
			return;
		}

		session->add_undo (playlist->get_memento());
		playlist->replace_region (aregion, *new_region, aregion.position());
		session->add_redo_no_execute (playlist->get_memento());

		i = tmp;
	}

	dialog.status = 0;
	dialog.request.running = false;
}

void*
Editor::timestretch_thread (void *arg)
{
	PBD::ThreadCreated (pthread_self(), X_("TimeFX"));

	TimeStretchDialog* tsd = static_cast<TimeStretchDialog*>(arg);

	pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, 0);

	tsd->editor.do_timestretch (*tsd);

	return 0;
}

