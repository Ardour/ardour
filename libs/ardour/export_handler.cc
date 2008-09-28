/*
    Copyright (C) 2008 Paul Davis
    Author: Sakari Bergen

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

#include <ardour/export_handler.h>

#include <pbd/convert.h>
#include <pbd/filesystem.h>

#include <ardour/ardour.h>
#include <ardour/configuration.h>
#include <ardour/export_timespan.h>
#include <ardour/export_channel_configuration.h>
#include <ardour/export_status.h>
#include <ardour/export_format_specification.h>
#include <ardour/export_filename.h>
#include <ardour/export_processor.h>
#include <ardour/export_failed.h>

using namespace PBD;
using std::ofstream;

namespace ARDOUR
{

/*** ExportElementFactory ***/

ExportElementFactory::ExportElementFactory (Session & session) :
  session (session)
{

}

ExportElementFactory::~ExportElementFactory ()
{

}

ExportElementFactory::TimespanPtr
ExportElementFactory::add_timespan ()
{
	return TimespanPtr (new ExportTimespan (session.get_export_status(), session.frame_rate()));
}

ExportElementFactory::ChannelConfigPtr
ExportElementFactory::add_channel_config ()
{
	return ChannelConfigPtr (new ExportChannelConfiguration (session));
}

ExportElementFactory::FormatPtr
ExportElementFactory::add_format ()
{
	return FormatPtr (new ExportFormatSpecification (session));
}

ExportElementFactory::FormatPtr
ExportElementFactory::add_format (XMLNode const & state)
{
	return FormatPtr (new ExportFormatSpecification (session, state));
}

ExportElementFactory::FormatPtr
ExportElementFactory::add_format_copy (FormatPtr other)
{
	return FormatPtr (new ExportFormatSpecification (*other));
}

ExportElementFactory::FilenamePtr
ExportElementFactory::add_filename ()
{
	return FilenamePtr (new ExportFilename (session));
}

ExportElementFactory::FilenamePtr
ExportElementFactory::add_filename_copy (FilenamePtr other)
{
	return FilenamePtr (new ExportFilename (*other));
}

/*** ExportHandler ***/

ExportHandler::ExportHandler (Session & session) :
  ExportElementFactory (session),
  session (session),
  export_status (session.get_export_status ()),
  realtime (false)
{
	processor.reset (new ExportProcessor (session));

	files_written_connection = ExportProcessor::WritingFile.connect (sigc::mem_fun (files_written, &std::list<Glib::ustring>::push_back));
}

ExportHandler::~ExportHandler ()
{
	if (export_status->aborted()) {
		for (std::list<Glib::ustring>::iterator it = files_written.begin(); it != files_written.end(); ++it) {
			sys::remove (sys::path (*it));
		}
	}
	
	channel_config_connection.disconnect();
	files_written_connection.disconnect();
}

bool
ExportHandler::add_export_config (TimespanPtr timespan, ChannelConfigPtr channel_config, FormatPtr format, FilenamePtr filename)
{
	FileSpec spec (channel_config, format, filename);
	ConfigPair pair (timespan, spec);
	config_map.insert (pair);
	
	return true;
}

/// Starts exporting the registered configurations
/** The following happens, when do_export is called:
 * 1. Session is prepared in do_export
 * 2. start_timespan is called, which then registers all necessary channel configs to a timespan
 * 3. The timespan reads each unique channel into a tempfile and calls Session::stop_export when the end is reached
 * 4. stop_export emits ExportReadFinished after stopping the transport, this ends up calling finish_timespan
 * 5. finish_timespan registers all the relevant formats and filenames to relevant channel configurations
 * 6. finish_timespan does a manual call to timespan_thread_finished, which gets the next channel configuration
 *    for the current timespan, calling write_files for it
 * 7. write_files writes the actual export files, composing them from the individual channels from tempfiles and
 *    emits FilesWritten when it is done, which ends up calling timespan_thread_finished
 * 8. Once all channel configs are written, a new timespan is started by calling start_timespan
 * 9. When all timespans are written the session is taken out of export.
 */

void
ExportHandler::do_export (bool rt)
{
	/* Count timespans */

	export_status->init();
	std::set<TimespanPtr> timespan_set;
	for (ConfigMap::iterator it = config_map.begin(); it != config_map.end(); ++it) {
		timespan_set.insert (it->first);
	}
	export_status->total_timespans = timespan_set.size();

	/* Start export */

	realtime = rt;

	session.ExportReadFinished.connect (sigc::mem_fun (*this, &ExportHandler::finish_timespan));
	start_timespan ();
}

struct LocationSortByStart {
    bool operator() (Location *a, Location *b) {
	    return a->start() < b->start();
    }
};

void
ExportHandler::export_cd_marker_file (TimespanPtr timespan, FormatPtr file_format, std::string filename, CDMarkerFormat format)
{
	string filepath;
	void (ExportHandler::*header_func) (CDMarkerStatus &);
	void (ExportHandler::*track_func) (CDMarkerStatus &);
	void (ExportHandler::*index_func) (CDMarkerStatus &);

	switch (format) {
	  case CDMarkerTOC:
		filepath = filename + ".toc";
		header_func = &ExportHandler::write_toc_header;
		track_func = &ExportHandler::write_track_info_toc;
		index_func = &ExportHandler::write_index_info_toc;
		break;
	  case CDMarkerCUE:
		filepath = filename + ".cue";
		header_func = &ExportHandler::write_cue_header;
		track_func = &ExportHandler::write_track_info_cue;
		index_func = &ExportHandler::write_index_info_cue;
		break;
	  default:
		return;
	}
	
	CDMarkerStatus status (filepath, timespan, file_format, filename);

	if (!status.out) {
		error << string_compose(_("Editor: cannot open \"%1\" as export file for CD marker file"), filepath) << endmsg;
		return;
	}
	
	(this->*header_func) (status);
	
	/* Get locations and sort */
	
	Locations::LocationList const & locations (session.locations()->list());
	Locations::LocationList::const_iterator i;
	Locations::LocationList temp;

	for (i = locations.begin(); i != locations.end(); ++i) {
		if ((*i)->start() >= timespan->get_start() && (*i)->end() <= timespan->get_end() && (*i)->is_cd_marker() && !(*i)->is_end()) {
			temp.push_back (*i);
		}
	}

	if (temp.empty()) {
		// TODO One index marker for whole thing
		return;
	}
	
	LocationSortByStart cmp;
	temp.sort (cmp);
	Locations::LocationList::const_iterator nexti;

	/* Start actual marker stuff */

	nframes_t last_end_time = timespan->get_start(), last_start_time = timespan->get_start();
	status.track_position = last_start_time - timespan->get_start();

	for (i = temp.begin(); i != temp.end(); ++i) {

		status.marker = *i;

		if ((*i)->start() < last_end_time) {
			if ((*i)->is_mark()) {
				/* Index within track */
				
				status.index_position = (*i)->start() - timespan->get_start();
				(this->*index_func) (status);
			}
			
			continue;
		}

		/* A track, defined by a cd range marker or a cd location marker outside of a cd range */
		
		status.track_position = last_end_time - timespan->get_start();
		status.track_start_frame = (*i)->start() - timespan->get_start();  // everything before this is the pregap
		status.track_duration = 0;
		
		if ((*i)->is_mark()) {
			// a mark track location needs to look ahead to the next marker's start to determine length
			nexti = i;
			++nexti;
			
			if (nexti != temp.end()) {
				status.track_duration = (*nexti)->start() - last_end_time;
				
				last_start_time = (*i)->start();
				last_end_time = (*nexti)->start();
			} else {
				// this was the last marker, use timespan end
				status.track_duration = timespan->get_end() - last_end_time;
				
				last_start_time = (*i)->start();
				last_end_time = timespan->get_end();
			}
		} else {
			// range
			status.track_duration = (*i)->end() - last_end_time;
			
			last_start_time = (*i)->start();
			last_end_time = (*i)->end();
		}
		
		(this->*track_func) (status);
	}
}

void
ExportHandler::write_cue_header (CDMarkerStatus & status)
{
	Glib::ustring title = status.timespan->name().compare ("Session") ? status.timespan->name() : (Glib::ustring) session.name();

	status.out << "REM Cue file generated by Ardour" << endl;
	status.out << "TITLE \"" << title << "\"" << endl;

	// TODO
	if (!status.format->format_name().compare ("WAV")) {
		  status.out << "FILE " << status.filename  << " WAVE" << endl;
	} else {
		  status.out << "FILE " << status.filename  << ' ' << status.format->format_name() << endl;
	}
}

void
ExportHandler::write_toc_header (CDMarkerStatus & status)
{
	Glib::ustring title = status.timespan->name().compare ("Session") ? status.timespan->name() : (Glib::ustring) session.name();

	status.out << "CD_DA" << endl;
	status.out << "CD_TEXT {" << endl << "  LANGUAGE_MAP {" << endl << "    0 : EN" << endl << "  }" << endl;
	status.out << "  LANGUAGE 0 {" << endl << "    TITLE \"" << title << "\"" << endl << "  }" << endl << "}" << endl;
}

void
ExportHandler::write_track_info_cue (CDMarkerStatus & status)
{
	gchar buf[18];

	status.out << endl << "TRACK " << status.track_number << " AUDIO" << endl;
	status.out << "FLAGS " ;
	
	if (status.marker->cd_info.find("scms") != status.marker->cd_info.end())  {
		status.out << "SCMS ";
	} else {
		status.out << "DCP ";
	}
	
	if (status.marker->cd_info.find("preemph") != status.marker->cd_info.end())  {
		status.out << "PRE";
	}
	status.out << endl;
	
	if (status.marker->cd_info.find("isrc") != status.marker->cd_info.end())  {
		status.out << "ISRC " << status.marker->cd_info["isrc"] << endl;
		
	}
	if (status.marker->name() != "") {
		status.out << "TITLE \"" << status.marker->name() << "\"" << endl;
	}
	
	if (status.marker->cd_info.find("performer") != status.marker->cd_info.end()) {
		status.out << "PERFORMER \"" <<  status.marker->cd_info["performer"] << "\"" << endl;
	}
	
	if (status.marker->cd_info.find("string_composer") != status.marker->cd_info.end()) {
		status.out << "SONGWRITER \"" << status.marker->cd_info["string_composer"]  << "\"" << endl;
	}

	frames_to_cd_frames_string (buf, status.track_position);
	status.out << "INDEX 00" << buf << endl;
	
	frames_to_cd_frames_string (buf, status.track_start_frame);
	status.out << "INDEX 01" << buf << endl;
	
	status.index_number = 2;
	status.track_number++;
}

void
ExportHandler::write_track_info_toc (CDMarkerStatus & status)
{
	gchar buf[18];

	status.out << endl << "TRACK AUDIO" << endl;
		
	if (status.marker->cd_info.find("scms") != status.marker->cd_info.end())  {
		status.out << "NO ";
	}
	status.out << "COPY" << endl;
	
	if (status.marker->cd_info.find("preemph") != status.marker->cd_info.end())  {
		status.out << "PRE_EMPHASIS" << endl;
	} else {
		status.out << "NO PRE_EMPHASIS" << endl;
	}
	
	if (status.marker->cd_info.find("isrc") != status.marker->cd_info.end())  {
		status.out << "ISRC \"" << status.marker->cd_info["isrc"] << "\"" << endl;
	}
	
	status.out << "CD_TEXT {" << endl << "  LANGUAGE 0 {" << endl << "     TITLE \"" << status.marker->name() << "\"" << endl;
	if (status.marker->cd_info.find("performer") != status.marker->cd_info.end()) {
		status.out << "     PERFORMER \"" << status.marker->cd_info["performer"]  << "\"" << endl;
	}
	if (status.marker->cd_info.find("string_composer") != status.marker->cd_info.end()) {
		status.out  << "     COMPOSER \"" << status.marker->cd_info["string_composer"] << "\"" << endl;
	}
	
	if (status.marker->cd_info.find("isrc") != status.marker->cd_info.end()) {			  
		status.out  << "     ISRC \"";
		status.out << status.marker->cd_info["isrc"].substr(0,2) << "-";
		status.out << status.marker->cd_info["isrc"].substr(2,3) << "-";
		status.out << status.marker->cd_info["isrc"].substr(5,2) << "-";
		status.out << status.marker->cd_info["isrc"].substr(7,5) << "\"" << endl;
	}
	
	status.out << "  }" << endl << "}" << endl;
	
	frames_to_cd_frames_string (buf, status.track_position);
	status.out << "FILE \"" << status.filename << "\" " << buf;
	
	frames_to_cd_frames_string (buf, status.track_duration);
	status.out << buf << endl;
	
	frames_to_cd_frames_string (buf, status.track_start_frame - status.track_position);
	status.out << "START" << buf << endl;
}

void
ExportHandler::write_index_info_cue (CDMarkerStatus & status)
{
	gchar buf[18];

	snprintf (buf, sizeof(buf), "INDEX %02d", cue_indexnum);
	status.out << buf;
	frames_to_cd_frames_string (buf, status.index_position);
	status.out << buf << endl;
	
	cue_indexnum++;
}

void
ExportHandler::write_index_info_toc (CDMarkerStatus & status)
{
	gchar buf[18];

	frames_to_cd_frames_string (buf, status.index_position - status.track_position);
	status.out << "INDEX" << buf << endl;
}

void
ExportHandler::frames_to_cd_frames_string (char* buf, nframes_t when)
{
	nframes_t remainder;
	nframes_t fr = session.nominal_frame_rate();
	int mins, secs, frames;

	mins = when / (60 * fr);
	remainder = when - (mins * 60 * fr);
	secs = remainder / fr;
	remainder -= secs * fr;
	frames = remainder / (fr / 75);
	sprintf (buf, " %02d:%02d:%02d", mins, secs, frames);
}

void
ExportHandler::start_timespan ()
{
	export_status->timespan++;

	if (config_map.empty()) {
		export_status->finish ();
		return;
	}

	current_timespan = config_map.begin()->first;
	
	/* Register channel configs with timespan */
	
	timespan_bounds = config_map.equal_range (current_timespan);
	
	for (ConfigMap::iterator it = timespan_bounds.first; it != timespan_bounds.second; ++it) {
		it->second.channel_config->register_with_timespan (current_timespan);
	}

	/* connect stuff and start export */

	current_timespan->process_connection = session.ProcessExport.connect (sigc::mem_fun (*current_timespan, &ExportTimespan::process));
	session.start_audio_export (current_timespan->get_start(), realtime);
}

void
ExportHandler::finish_timespan ()
{
	current_timespan->process_connection.disconnect ();
	
	/* Register formats and filenames to relevant channel configs */
	
	export_status->total_formats = 0;
	export_status->format = 0;
	
	for (ConfigMap::iterator it = timespan_bounds.first; it != timespan_bounds.second; ++it) {
	
		export_status->total_formats++;
	
		/* Setup filename */
		
		it->second.filename->set_timespan (current_timespan);
		it->second.filename->set_channel_config (it->second.channel_config);
		
		/* Do actual registration */
	
		ChannelConfigPtr chan_config = it->second.channel_config;
		chan_config->register_file_config (it->second.format, it->second.filename);
	}
	
	/* Start writing files by doing a manual call to timespan_thread_finished */
	
	current_map_it = timespan_bounds.first;
	timespan_thread_finished ();
}

void
ExportHandler::timespan_thread_finished ()
{
	channel_config_connection.disconnect();

	if (current_map_it != timespan_bounds.second) {
	
		/* Get next configuration as long as no new export process is started */
		
		ChannelConfigPtr cc = current_map_it->second.channel_config;
		while (!cc->write_files(processor)) {
	
			++current_map_it;
			
			if (current_map_it == timespan_bounds.second) {
			
				/* reached end of bounds, this call will end up in the else block below */
			
				timespan_thread_finished ();
				return;
			}
			
			cc = current_map_it->second.channel_config;
		}
	
		channel_config_connection = cc->FilesWritten.connect (sigc::mem_fun (*this, &ExportHandler::timespan_thread_finished));
		++current_map_it;
	
	} else { /* All files are written from current timespan, reset timespan and start new */
	
		/* Unregister configs and remove configs with this timespan */
	
		for (ConfigMap::iterator it = timespan_bounds.first; it != timespan_bounds.second;) {
			it->second.channel_config->unregister_all ();
			
			ConfigMap::iterator to_erase = it;
			++it;
			config_map.erase (to_erase);
		}
		
		/* Start new timespan */
	
		start_timespan ();
	
	}
}

} // namespace ARDOUR
