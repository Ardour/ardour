/*
    Copyright (C) 2006 Andre Raue

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

#ifndef __export_range_markers_dialog_h__
#define __export_range_markers_dialog_h__

#include <ardour/location.h>

#include "export_dialog.h"


class ExportRangeMarkersDialog : public ExportDialog 
{
  public:
	ExportRangeMarkersDialog (PublicEditor&);
  
	Gtk::FileChooserAction browse_action() const;

  protected:
	virtual bool is_filepath_valid(string &filepath);

  	void export_audio_data();

	bool wants_dir() { return true; }
  
  private:
	// keeps the duration of all range_markers before the current
  	vector<nframes_t>	range_markers_durations_aggregated;
  	vector<nframes_t>	range_markers_durations;
	// duration of all range markers
  	nframes_t	total_duration;
  	// index of range marker, that get's exported right now
  	unsigned int	current_range_marker_index;
	
  	// sets value of progress bar
  	virtual gint progress_timeout ();
  
  	// initializes range_markers_durations_aggregated, range_markers_durations
	// and total_duration
  	void init_progress_computing(ARDOUR::Locations::LocationList& locations);

  	// searches for a filename like "<filename><nr>.<postfix>" in path, that 
  	// does not exist
    string get_target_filepath(string path, string filename, string postfix);

  	void process_range_markers_export(ARDOUR::Locations::LocationList&);
};


#endif // __export_range_markers_dialog_h__
