/*
    Copyright (C) 2006 Paul Davis
    Author: Andre Raue

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

#include <cassert>

#include <pbd/pthread_utils.h>
#include <ardour/audioregion.h>

#include "export_region_dialog.h"

#include "i18n.h"


ExportRegionDialog::ExportRegionDialog (PublicEditor& editor, boost::shared_ptr<ARDOUR::Region> region) 
	: ExportDialog(editor)
{
	set_title (_("ardour: export region"));
	file_frame.set_label (_("Export to File")),

	audio_region = boost::dynamic_pointer_cast<ARDOUR::AudioRegion>(region);
	assert(audio_region);

	do_not_allow_track_and_master_selection();
	do_not_allow_channel_count_selection();
}


void
ExportRegionDialog::export_audio_data()
{
	pthread_t thr;
	pthread_create_and_store ("region export", &thr, 0, ExportRegionDialog::_export_region_thread, this);

	gtk_main_iteration ();
	while (spec.running) {
		if (gtk_events_pending()) {
			gtk_main_iteration ();
		} else {
			usleep (10000);
		}
	}
}


void*
ExportRegionDialog::_export_region_thread (void *arg)
{
	PBD::notify_gui_about_thread_creation (pthread_self(), X_("Export Region"));

	static_cast<ExportRegionDialog*>(arg)->export_region ();
	return 0;
}

void
ExportRegionDialog::export_region ()
{
	audio_region->exportme (getSession(), spec);
}
