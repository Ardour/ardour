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

#include <sys/stat.h>

#include <sstream>

#include "ardour/audioengine.h"
#include "ardour/sndfile_helpers.h"

#include "ardour_ui.h"
#include "export_range_markers_dialog.h"

#include "i18n.h"

using namespace Gtk;
using namespace ARDOUR;
using namespace PBD;
using namespace std;

ExportRangeMarkersDialog::ExportRangeMarkersDialog (PublicEditor& editor)
	: ExportDialog(editor)
{
	set_title (_("Export Ranges"));
	file_frame.set_label (_("Export to Directory"));

	do_not_allow_export_cd_markers();

	total_duration = 0;
	current_range_marker_index = 0;
}

Gtk::FileChooserAction
ExportRangeMarkersDialog::browse_action () const
{
	return Gtk::FILE_CHOOSER_ACTION_CREATE_FOLDER;
}

void
ExportRangeMarkersDialog::export_data ()
{
	getSession().locations()->apply(*this, &ExportRangeMarkersDialog::process_range_markers_export);
}

void
ExportRangeMarkersDialog::process_range_markers_export(Locations::LocationList& locations)
{
	Locations::LocationList::iterator locationIter;
	current_range_marker_index = 0;
	init_progress_computing(locations);

	for (locationIter = locations.begin(); locationIter != locations.end(); ++locationIter) {
		Location *currentLocation = (*locationIter);

		if(currentLocation->is_range_marker()){
			// init filename
			string filepath = get_target_filepath(
				get_selected_file_name(),
				currentLocation->name(),
				get_selected_header_format());

			initSpec(filepath);

			spec.start_frame = currentLocation->start();
			spec.end_frame = currentLocation->end();

			if (getSession().start_export(spec)){
				// if export fails
				return;
			}

			// wait until export of this range finished
			gtk_main_iteration();

			while (spec.running){
				if(gtk_events_pending()){
					gtk_main_iteration();
				}else {
					Glib::usleep(10000);
				}
			}

			current_range_marker_index++;

			getSession().stop_export (spec);
		}
	}

	spec.running = false;
}


string
ExportRangeMarkersDialog::get_target_filepath(string path, string filename, string postfix)
{
	string target_path = path;
	if ((target_path.find_last_of ('/')) != string::npos) {
		target_path += '/';
	}

	string target_filepath = target_path + filename + postfix;
	struct stat statbuf;

	for(int counter=1; (stat (target_filepath.c_str(), &statbuf) == 0); counter++){
		// while file exists
		ostringstream scounter;
		scounter.flush();
		scounter << counter;

		target_filepath =
			target_path + filename + "_" + scounter.str() + postfix;
	}

	return target_filepath;
}

bool
ExportRangeMarkersDialog::is_filepath_valid(string &filepath)
{
  	// sanity check file name first
  	struct stat statbuf;

  	if (filepath.empty()) {
  		// warning dialog
 		string txt = _("Please enter a valid target directory.");
		MessageDialog msg (*this, txt, false, MESSAGE_ERROR, BUTTONS_OK, true);
		msg.run();
 		return false;
 	}

	if ( (stat (filepath.c_str(), &statbuf) != 0) ||
		(!S_ISDIR (statbuf.st_mode)) ) {
		string txt = _("Please select an existing target directory. Files are not allowed!");
		MessageDialog msg (*this, txt, false, MESSAGE_ERROR, BUTTONS_OK, true);
		msg.run();
		return false;
	}

 	// directory needs to exist and be writable
 	string dirpath = Glib::path_get_dirname (filepath);
	if (!exists_and_writable (dirpath)) {
 		string txt = _("Cannot write file in: ") + dirpath;
		MessageDialog msg (*this, txt, false, MESSAGE_ERROR, BUTTONS_OK, true);
		msg.run();
 		return false;
  	}

	return true;
}

void
ExportRangeMarkersDialog::init_progress_computing(Locations::LocationList& locations)
{
	// flush vector
	range_markers_durations_aggregated.resize(0);

	framecnt_t duration_before_current_location = 0;
	Locations::LocationList::iterator locationIter;

	for (locationIter = locations.begin(); locationIter != locations.end(); ++locationIter) {
		Location *currentLocation = (*locationIter);

		if(currentLocation->is_range_marker()){
			range_markers_durations_aggregated.push_back (duration_before_current_location);

			framecnt_t duration = currentLocation->end() - currentLocation->start();

			range_markers_durations.push_back (duration);
			duration_before_current_location += duration;
		}
	}

	total_duration = duration_before_current_location;
}


gint
ExportRangeMarkersDialog::progress_timeout ()
{
	double progress = 0.0;

	if (current_range_marker_index >= range_markers_durations.size()){
		progress = 1.0;
	} else{
		progress = ((double) range_markers_durations_aggregated[current_range_marker_index] +
			    (spec.progress * (double) range_markers_durations[current_range_marker_index])) /
			(double) total_duration;
	}

	set_progress_fraction( progress );
	return TRUE;
}
