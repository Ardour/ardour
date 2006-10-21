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

#include "ardour_ui.h"
#include "export_session_dialog.h"


ExportSessionDialog::ExportSessionDialog (PublicEditor& editor) 
	: ExportDialog(editor)
{
}
	
	
void 
ExportSessionDialog::export_audio_data ()
{
	if (getSession().start_audio_export (spec)) {
		return;
	}

	gtk_main_iteration ();
	while (spec.running) {
		if (gtk_events_pending()) {
			gtk_main_iteration ();
		} else {
			usleep (10000);
		}
	}
}


void
ExportSessionDialog::set_range (nframes_t start, nframes_t end)
{	
	ExportDialog::set_range (start, end);
	
	// XXX: this is a hack until we figure out what is really wrong
	getSession().request_locate (spec.start_frame, false);
}
