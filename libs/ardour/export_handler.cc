/*
    Copyright (C) 2008-2009 Paul Davis
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

#include "ardour/export_handler.h"

#include <glibmm.h>
#include <glibmm/convert.h>

#include "pbd/convert.h"

#include "ardour/audiofile_tagger.h"
#include "ardour/export_graph_builder.h"
#include "ardour/export_timespan.h"
#include "ardour/export_channel_configuration.h"
#include "ardour/export_status.h"
#include "ardour/export_format_specification.h"
#include "ardour/export_filename.h"
#include "ardour/soundcloud_upload.h"
#include "pbd/openuri.h"
#include "pbd/basename.h"
#include "pbd/system_exec.h"
#include "ardour/session_metadata.h"

#include "i18n.h"

using namespace std;
using namespace PBD;

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

ExportTimespanPtr
ExportElementFactory::add_timespan ()
{
	return ExportTimespanPtr (new ExportTimespan (session.get_export_status(), session.frame_rate()));
}

ExportChannelConfigPtr
ExportElementFactory::add_channel_config ()
{
	return ExportChannelConfigPtr (new ExportChannelConfiguration (session));
}

ExportFormatSpecPtr
ExportElementFactory::add_format ()
{
	return ExportFormatSpecPtr (new ExportFormatSpecification (session));
}

ExportFormatSpecPtr
ExportElementFactory::add_format (XMLNode const & state)
{
	return ExportFormatSpecPtr (new ExportFormatSpecification (session, state));
}

ExportFormatSpecPtr
ExportElementFactory::add_format_copy (ExportFormatSpecPtr other)
{
	return ExportFormatSpecPtr (new ExportFormatSpecification (*other));
}

ExportFilenamePtr
ExportElementFactory::add_filename ()
{
	return ExportFilenamePtr (new ExportFilename (session));
}

ExportFilenamePtr
ExportElementFactory::add_filename_copy (ExportFilenamePtr other)
{
	return ExportFilenamePtr (new ExportFilename (*other));
}

/*** ExportHandler ***/

ExportHandler::ExportHandler (Session & session)
  : ExportElementFactory (session)
  , session (session)
  , graph_builder (new ExportGraphBuilder (session))
  , export_status (session.get_export_status ())
  , normalizing (false)
  , cue_tracknum (0)
  , cue_indexnum (0)
{
}

ExportHandler::~ExportHandler ()
{
	// TODO remove files that were written but not finished
}

/** Add an export to the `to-do' list */
bool
ExportHandler::add_export_config (ExportTimespanPtr timespan, ExportChannelConfigPtr channel_config,
                                  ExportFormatSpecPtr format, ExportFilenamePtr filename,
                                  BroadcastInfoPtr broadcast_info)
{
	FileSpec spec (channel_config, format, filename, broadcast_info);
	config_map.insert (make_pair (timespan, spec));

	return true;
}

void
ExportHandler::do_export ()
{
	/* Count timespans */

	export_status->init();
	std::set<ExportTimespanPtr> timespan_set;
	for (ConfigMap::iterator it = config_map.begin(); it != config_map.end(); ++it) {
		bool new_timespan = timespan_set.insert (it->first).second;
		if (new_timespan) {
			export_status->total_frames += it->first->get_length();
		}
	}
	export_status->total_timespans = timespan_set.size();

	/* Start export */

	start_timespan ();
}

void
ExportHandler::start_timespan ()
{
	export_status->timespan++;

	if (config_map.empty()) {
		// freewheeling has to be stopped from outside the process cycle
		export_status->running = false;
		return;
	}

	/* finish_timespan pops the config_map entry that has been done, so
	   this is the timespan to do this time
	*/
	current_timespan = config_map.begin()->first;
	
	export_status->total_frames_current_timespan = current_timespan->get_length();
	export_status->timespan_name = current_timespan->name();
	export_status->processed_frames_current_timespan = 0;

	/* Register file configurations to graph builder */

	/* Here's the config_map entries that use this timespan */
	timespan_bounds = config_map.equal_range (current_timespan);
	graph_builder->reset ();
	graph_builder->set_current_timespan (current_timespan);
	handle_duplicate_format_extensions();
	for (ConfigMap::iterator it = timespan_bounds.first; it != timespan_bounds.second; ++it) {
		// Filenames can be shared across timespans
		FileSpec & spec = it->second;
		spec.filename->set_timespan (it->first);
		graph_builder->add_config (spec);
	}

	/* start export */

	normalizing = false;
	session.ProcessExport.connect_same_thread (process_connection, boost::bind (&ExportHandler::process, this, _1));
	process_position = current_timespan->get_start();
	session.start_audio_export (process_position);
}

void
ExportHandler::handle_duplicate_format_extensions()
{
	typedef std::map<std::string, int> ExtCountMap;

	ExtCountMap counts;
	for (ConfigMap::iterator it = timespan_bounds.first; it != timespan_bounds.second; ++it) {
		counts[it->second.format->extension()]++;
	}

	bool duplicates_found = false;
	for (ExtCountMap::iterator it = counts.begin(); it != counts.end(); ++it) {
		if (it->second > 1) { duplicates_found = true; }
	}

	// Set this always, as the filenames are shared...
	for (ConfigMap::iterator it = timespan_bounds.first; it != timespan_bounds.second; ++it) {
		it->second.filename->include_format_name = duplicates_found;
	}
}

int
ExportHandler::process (framecnt_t frames)
{
	if (!export_status->running) {
		return 0;
	} else if (normalizing) {
		return process_normalize ();
	} else {
		return process_timespan (frames);
	}
}

int
ExportHandler::process_timespan (framecnt_t frames)
{
	/* update position */

	framecnt_t frames_to_read = 0;
	framepos_t const end = current_timespan->get_end();

	bool const last_cycle = (process_position + frames >= end);

	if (last_cycle) {
		frames_to_read = end - process_position;
		export_status->stop = true;
	} else {
		frames_to_read = frames;
	}

	process_position += frames_to_read;
	export_status->processed_frames += frames_to_read;
	export_status->processed_frames_current_timespan += frames_to_read;

	/* Do actual processing */
	int ret = graph_builder->process (frames_to_read, last_cycle);

	/* Start normalizing if necessary */
	if (last_cycle) {
		normalizing = graph_builder->will_normalize();
		if (normalizing) {
			export_status->total_normalize_cycles = graph_builder->get_normalize_cycle_count();
			export_status->current_normalize_cycle = 0;
		} else {
			finish_timespan ();
			return 0;
		}
	}

	return ret;
}

int
ExportHandler::process_normalize ()
{
	if (graph_builder->process_normalize ()) {
		finish_timespan ();
		export_status->normalizing = false;
	} else {
		export_status->normalizing = true;
	}

	export_status->current_normalize_cycle++;

	return 0;
}

void
ExportHandler::command_output(std::string output, size_t size)
{
	std::cerr << "command: " << size << ", " << output << std::endl;
	info << output << endmsg;
}

void
ExportHandler::finish_timespan ()
{
	while (config_map.begin() != timespan_bounds.second) {

		ExportFormatSpecPtr fmt = config_map.begin()->second.format;
		std::string filename = config_map.begin()->second.filename->get_path(fmt);

		if (fmt->with_cue()) {
			export_cd_marker_file (current_timespan, fmt, filename, CDMarkerCUE);
		}

		if (fmt->with_toc()) {
			export_cd_marker_file (current_timespan, fmt, filename, CDMarkerTOC);
		}

		if (fmt->tag()) {
			AudiofileTagger::tag_file(filename, *SessionMetadata::Metadata());
		}

		if (!fmt->command().empty()) {

#if 0			// would be nicer with C++11 initialiser...
			std::map<char, std::string> subs {
				{ 'f', filename },
				{ 'd', Glib::path_get_dirname(filename) },
				{ 'b', PBD::basename_nosuffix(filename) },
				{ 'u', upload_username },
				{ 'p', upload_password}
			};
#endif

			PBD::ScopedConnection command_connection;
			std::map<char, std::string> subs;
			subs.insert (std::pair<char, std::string> ('f', filename));
			subs.insert (std::pair<char, std::string> ('d', Glib::path_get_dirname(filename)));
			subs.insert (std::pair<char, std::string> ('b', PBD::basename_nosuffix(filename)));
			subs.insert (std::pair<char, std::string> ('u', upload_username));
			subs.insert (std::pair<char, std::string> ('p', upload_password));


			std::cerr << "running command: " << fmt->command() << "..." << std::endl;
			SystemExec *se = new SystemExec(fmt->command(), subs);
			se->ReadStdout.connect_same_thread(command_connection, boost::bind(&ExportHandler::command_output, this, _1, _2));
			if (se->start (2) == 0) {
				// successfully started
				std::cerr << "started!" << std::endl;
				while (se->is_running ()) {
					// wait for system exec to terminate
					// std::cerr << "waiting..." << std::endl;
					usleep (1000);
				}
			}
			std::cerr << "done! deleting..." << std::endl;
			delete (se);
		}

		if (fmt->upload()) {
			SoundcloudUploader *soundcloud_uploader = new SoundcloudUploader;
			std::string token = soundcloud_uploader->Get_Auth_Token(upload_username, upload_password);
			std::cerr
				<< "uploading "
				<< filename << std::endl
				<< "username = " << upload_username
				<< ", password = " << upload_password
				<< " - token = " << token << " ..."
				<< std::endl;
			std::string path = soundcloud_uploader->Upload (
					filename,
					PBD::basename_nosuffix(filename), // title
					token,
					upload_public,
					this);

			if (path.length() != 0) {
				if (upload_open) {
				std::cerr << "opening " << path << " ..." << std::endl;
				open_uri(path.c_str());  // open the soundcloud website to the new file
				}
			} else {
				error << _("upload to Soundcloud failed.  Perhaps your email or password are incorrect?\n") << endmsg;
			}
			delete soundcloud_uploader;
		}
		config_map.erase (config_map.begin());
	}

	start_timespan ();
}

/*** CD Marker stuff ***/

struct LocationSortByStart {
    bool operator() (Location *a, Location *b) {
	    return a->start() < b->start();
    }
};

void
ExportHandler::export_cd_marker_file (ExportTimespanPtr timespan, ExportFormatSpecPtr file_format,
                                      std::string filename, CDMarkerFormat format)
{
	string filepath = get_cd_marker_filename(filename, format);

	try {
		void (ExportHandler::*header_func) (CDMarkerStatus &);
		void (ExportHandler::*track_func) (CDMarkerStatus &);
		void (ExportHandler::*index_func) (CDMarkerStatus &);

		switch (format) {
		case CDMarkerTOC:
			header_func = &ExportHandler::write_toc_header;
			track_func = &ExportHandler::write_track_info_toc;
			index_func = &ExportHandler::write_index_info_toc;
			break;
		case CDMarkerCUE:
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
			if ((*i)->start() >= timespan->get_start() && (*i)->end() <= timespan->get_end() && (*i)->is_cd_marker() && !(*i)->is_session_range()) {
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

		framepos_t last_end_time = timespan->get_start(), last_start_time = timespan->get_start();
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

	} catch (std::exception& e) {
		error << string_compose (_("an error occured while writing a TOC/CUE file: %1"), e.what()) << endmsg;
		::unlink (filepath.c_str());
	} catch (Glib::Exception& e) {
		error << string_compose (_("an error occured while writing a TOC/CUE file: %1"), e.what()) << endmsg;
		::unlink (filepath.c_str());
	}
}

string
ExportHandler::get_cd_marker_filename(std::string filename, CDMarkerFormat format)
{
	/* do not strip file suffix because there may be more than one format, 
	   and we do not want the CD marker file from one format to overwrite
	   another (e.g. foo.wav.cue > foo.aiff.cue)
	*/

	switch (format) {
	  case CDMarkerTOC:
		return filename + ".toc";
	  case CDMarkerCUE:
		return filename + ".cue";
	  default:
		return filename + ".marker"; // Should not be reached when actually creating a file
	}
}

void
ExportHandler::write_cue_header (CDMarkerStatus & status)
{
	string title = status.timespan->name().compare ("Session") ? status.timespan->name() : (string) session.name();

	status.out << "REM Cue file generated by " << PROGRAM_NAME << endl;
	status.out << "TITLE " << cue_escape_cdtext (title) << endl;

	/*  The original cue sheet sepc metions five file types
		WAVE, AIFF,
		BINARY   = "header-less" audio (44.1 kHz, 16 Bit, little endian),
		MOTOROLA = "header-less" audio (44.1 kHz, 16 Bit, big endian),
		and MP3
		
		We try to use these file types whenever appropriate and 
		default to our own names otherwise.
	*/
	status.out << "FILE \"" << Glib::path_get_basename(status.filename) << "\" ";
	if (!status.format->format_name().compare ("WAV")  || !status.format->format_name().compare ("BWF")) {
		status.out  << "WAVE";
	} else if (status.format->format_id() == ExportFormatBase::F_RAW &&
	           status.format->sample_format() == ExportFormatBase::SF_16 &&
	           status.format->sample_rate() == ExportFormatBase::SR_44_1) {
		// Format is RAW 16bit 44.1kHz
		if (status.format->endianness() == ExportFormatBase::E_Little) {
			status.out << "BINARY";
		} else {
			status.out << "MOTOROLA";
		}
	} else {
		// no special case for AIFF format it's name is already "AIFF"
		status.out << status.format->format_name();
	}
	status.out << endl;
}

void
ExportHandler::write_toc_header (CDMarkerStatus & status)
{
	string title = status.timespan->name().compare ("Session") ? status.timespan->name() : (string) session.name();

	status.out << "CD_DA" << endl;
	status.out << "CD_TEXT {" << endl << "  LANGUAGE_MAP {" << endl << "    0 : EN" << endl << "  }" << endl;
	status.out << "  LANGUAGE 0 {" << endl << "    TITLE " << toc_escape_cdtext (title) << endl ;
	status.out << "    PERFORMER \"\"" << endl << "  }" << endl << "}" << endl;
}

void
ExportHandler::write_track_info_cue (CDMarkerStatus & status)
{
	gchar buf[18];

	snprintf (buf, sizeof(buf), "  TRACK %02d AUDIO", status.track_number);
	status.out << buf << endl;

	status.out << "    FLAGS" ;
	if (status.marker->cd_info.find("scms") != status.marker->cd_info.end())  {
		status.out << " SCMS ";
	} else {
		status.out << " DCP ";
	}

	if (status.marker->cd_info.find("preemph") != status.marker->cd_info.end())  {
		status.out << " PRE";
	}
	status.out << endl;

	if (status.marker->cd_info.find("isrc") != status.marker->cd_info.end())  {
		status.out << "    ISRC " << status.marker->cd_info["isrc"] << endl;
	}

	if (status.marker->name() != "") {
		status.out << "    TITLE " << cue_escape_cdtext (status.marker->name()) << endl;
	}

	if (status.marker->cd_info.find("performer") != status.marker->cd_info.end()) {
		status.out <<  "    PERFORMER " << cue_escape_cdtext (status.marker->cd_info["performer"]) << endl;
	}

	if (status.marker->cd_info.find("composer") != status.marker->cd_info.end()) {
		status.out << "    SONGWRITER " << cue_escape_cdtext (status.marker->cd_info["composer"]) << endl;
	}

	if (status.track_position != status.track_start_frame) {
		frames_to_cd_frames_string (buf, status.track_position);
		status.out << "    INDEX 00" << buf << endl;
	}

	frames_to_cd_frames_string (buf, status.track_start_frame);
	status.out << "    INDEX 01" << buf << endl;

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

	status.out << "CD_TEXT {" << endl << "  LANGUAGE 0 {" << endl;
	status.out << "     TITLE " << toc_escape_cdtext (status.marker->name()) << endl;
	
	status.out << "     PERFORMER ";
	if (status.marker->cd_info.find("performer") != status.marker->cd_info.end()) {
		status.out << toc_escape_cdtext (status.marker->cd_info["performer"]) << endl;
	} else {
		status.out << "\"\"" << endl;
	}
	
	if (status.marker->cd_info.find("composer") != status.marker->cd_info.end()) {
		status.out  << "     SONGWRITER " << toc_escape_cdtext (status.marker->cd_info["composer"]) << endl;
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
	status.out << "FILE " << toc_escape_filename (status.filename) << ' ' << buf;

	frames_to_cd_frames_string (buf, status.track_duration);
	status.out << buf << endl;

	frames_to_cd_frames_string (buf, status.track_start_frame - status.track_position);
	status.out << "START" << buf << endl;
}

void
ExportHandler::write_index_info_cue (CDMarkerStatus & status)
{
	gchar buf[18];

	snprintf (buf, sizeof(buf), "    INDEX %02d", cue_indexnum);
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
ExportHandler::frames_to_cd_frames_string (char* buf, framepos_t when)
{
	framecnt_t remainder;
	framecnt_t fr = session.nominal_frame_rate();
	int mins, secs, frames;

	mins = when / (60 * fr);
	remainder = when - (mins * 60 * fr);
	secs = remainder / fr;
	remainder -= secs * fr;
	frames = remainder / (fr / 75);
	sprintf (buf, " %02d:%02d:%02d", mins, secs, frames);
}

std::string
ExportHandler::toc_escape_cdtext (const std::string& txt)
{
	Glib::ustring check (txt);
	std::string out;
	std::string latin1_txt;
	char buf[5];

	try {
		latin1_txt = Glib::convert (txt, "ISO-8859-1", "UTF-8");
	} catch (Glib::ConvertError& err) {
		throw Glib::ConvertError (err.code(), string_compose (_("Cannot convert %1 to Latin-1 text"), txt));
	}

	out = '"';

	for (std::string::const_iterator c = latin1_txt.begin(); c != latin1_txt.end(); ++c) {

		if ((*c) == '"') {
			out += "\\\"";
		} else if ((*c) == '\\') {
			out += "\\134";
		} else if (isprint (*c)) {
			out += *c;
		} else {
			snprintf (buf, sizeof (buf), "\\%03o", (int) (unsigned char) *c);
			out += buf;
		}
	}
	
	out += '"';

	return out;
}

std::string
ExportHandler::toc_escape_filename (const std::string& txt)
{
	std::string out;

	out = '"';

	// We iterate byte-wise not character-wise over a UTF-8 string here,
	// because we only want to translate backslashes and double quotes
	for (std::string::const_iterator c = txt.begin(); c != txt.end(); ++c) {

		if (*c == '"') {
			out += "\\\"";
		} else if (*c == '\\') {
			out += "\\134";
		} else {
			out += *c;
		}
	}
	
	out += '"';

	return out;
}

std::string
ExportHandler::cue_escape_cdtext (const std::string& txt)
{
	std::string latin1_txt;
	std::string out;
	
	try {
		latin1_txt = Glib::convert (txt, "ISO-8859-1", "UTF-8");
	} catch (Glib::ConvertError& err) {
		throw Glib::ConvertError (err.code(), string_compose (_("Cannot convert %1 to Latin-1 text"), txt));
	}
	
	// does not do much mor than UTF-8 to Latin1 translation yet, but
	// that may have to change if cue parsers in burning programs change 
	out = '"' + latin1_txt + '"';

	return out;
}

} // namespace ARDOUR
