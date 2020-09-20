/*
 * Copyright (C) 2008-2013 Sakari Bergen <sakari.bergen@beatwaves.net>
 * Copyright (C) 2008-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2013-2015 Colin Fletcher <colin.m.fletcher@googlemail.com>
 * Copyright (C) 2015-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015 Johannes Mueller <github@johannes-mueller.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "pbd/gstdio_compat.h"
#include <glibmm.h>
#include <glibmm/convert.h>

#include "pbd/convert.h"

#include "ardour/audioengine.h"
#include "ardour/audiofile_tagger.h"
#include "ardour/audio_port.h"
#include "ardour/debug.h"
#include "ardour/export_graph_builder.h"
#include "ardour/export_handler.h"
#include "ardour/export_timespan.h"
#include "ardour/export_channel_configuration.h"
#include "ardour/export_status.h"
#include "ardour/export_format_specification.h"
#include "ardour/export_filename.h"
#include "ardour/soundcloud_upload.h"
#include "ardour/system_exec.h"
#include "pbd/openuri.h"
#include "pbd/basename.h"
#include "ardour/session_metadata.h"

#include "pbd/i18n.h"

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
	return ExportTimespanPtr (new ExportTimespan (session.get_export_status(), session.sample_rate()));
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
  , post_processing (false)
  , cue_tracknum (0)
  , cue_indexnum (0)
{
}

ExportHandler::~ExportHandler ()
{
	graph_builder->cleanup (export_status->aborted () );
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

int
ExportHandler::do_export ()
{
	/* Count timespans */

	export_status->init();
	std::set<ExportTimespanPtr> timespan_set;
	for (ConfigMap::iterator it = config_map.begin(); it != config_map.end(); ++it) {
		bool new_timespan = timespan_set.insert (it->first).second;
		if (new_timespan) {
			export_status->total_samples += it->first->get_length();
		}
	}
	export_status->total_timespans = timespan_set.size();

	if (export_status->total_timespans > 1) {
		// always include timespan if there's more than one.
		for (ConfigMap::iterator it = config_map.begin(); it != config_map.end(); ++it) {
			FileSpec & spec = it->second;
			spec.filename->include_timespan = true;
		}
	}

	/* Start export */

	Glib::Threads::Mutex::Lock l (export_status->lock());
	return start_timespan ();
}

int
ExportHandler::start_timespan ()
{
	/* stop freewheeling and wait for latency callbacks */
	if (AudioEngine::instance()->freewheeling ()) {
		AudioEngine::instance()->freewheel (false);
		do {
			Glib::usleep (AudioEngine::instance()->usecs_per_cycle ());
		} while (AudioEngine::instance()->freewheeling ());
		session.reset_xrun_count ();
	}

	if (config_map.empty()) {
		// freewheeling has to be stopped from outside the process cycle
		export_status->set_running (false);
		return -1;
	}

	export_status->timespan++;

	/* finish_timespan pops the config_map entry that has been done, so
	   this is the timespan to do this time
	*/
	current_timespan = config_map.begin()->first;

	export_status->total_samples_current_timespan = current_timespan->get_length();
	export_status->timespan_name = current_timespan->name();
	export_status->processed_samples_current_timespan = 0;

	/* Register file configurations to graph builder */

	/* Here's the config_map entries that use this timespan */
	timespan_bounds = config_map.equal_range (current_timespan);
	graph_builder->reset ();
	graph_builder->set_current_timespan (current_timespan);
	handle_duplicate_format_extensions();
	bool realtime = current_timespan->realtime ();
	bool region_export = true;
	for (ConfigMap::iterator it = timespan_bounds.first; it != timespan_bounds.second; ++it) {
		// Filenames can be shared across timespans
		FileSpec & spec = it->second;
		spec.filename->set_timespan (it->first);
		switch (spec.channel_config->region_processing_type ()) {
			case RegionExportChannelFactory::None:
				region_export = false;
				break;
			default:
				break;
		}
		graph_builder->add_config (spec, realtime);
	}

	// ExportDialog::update_realtime_selection does not allow this
	assert (!region_export || !realtime);

	/* start export */

	post_processing = false;
	session.ProcessExport.connect_same_thread (process_connection, boost::bind (&ExportHandler::process, this, _1));
	process_position = current_timespan->get_start();
	// TODO check if it's a RegionExport.. set flag to skip  process_without_events()
	return session.start_audio_export (process_position, realtime, region_export);
}

void
ExportHandler::handle_duplicate_format_extensions()
{
	typedef std::map<std::string, int> ExtCountMap;

	ExtCountMap counts;
	for (ConfigMap::iterator it = timespan_bounds.first; it != timespan_bounds.second; ++it) {
		std::string pfx;
		if (it->second.filename->include_timespan) {
			pfx = it->first->name();
		}
		if (it->second.filename->include_channel_config && it->second.channel_config) {
			/* stem-export has multiple files in the same timestamp, but a different channel_config for each.
			 * However channel_config is only set in ExportGraphBuilder::Encoder::init_writer()
			 * so we cannot yet use   it->second.filename->get_path(it->second.format).
			 * We have to explicily check uniqueness of "channel-config + extension" here:
			 */
			counts[pfx + it->second.channel_config->name() + it->second.format->extension()]++;
		} else {
			counts[pfx + it->second.format->extension()]++;
		}
	}

	bool duplicates_found = false;
	for (ExtCountMap::iterator it = counts.begin(); it != counts.end(); ++it) {
		if (it->second > 1) { duplicates_found = true; }
	}

	// Set this always, as the filenames are shared...
	for (ConfigMap::iterator it = timespan_bounds.first; it != timespan_bounds.second; ++it) {
		assert (it->second.filename->include_format_name == duplicates_found);
		it->second.filename->include_format_name = duplicates_found;
	}
}

int
ExportHandler::process (samplecnt_t samples)
{
	if (!export_status->running ()) {
		return 0;
	} else if (post_processing) {
		Glib::Threads::Mutex::Lock l (export_status->lock());
		if (AudioEngine::instance()->freewheeling ()) {
			return post_process ();
		} else {
			// wait until we're freewheeling
			return 0;
		}
	} else if (samples > 0) {
		Glib::Threads::Mutex::Lock l (export_status->lock());
		return process_timespan (samples);
	}
	return 0;
}

int
ExportHandler::process_timespan (samplecnt_t samples)
{
	export_status->active_job = ExportStatus::Exporting;
	/* update position */

	samplecnt_t samples_to_read = 0;
	samplepos_t const end = current_timespan->get_end();

	bool const last_cycle = (process_position + samples >= end);

	if (last_cycle) {
		samples_to_read = end - process_position;
		export_status->stop = true;
	} else {
		samples_to_read = samples;
	}

	/* Do actual processing */
	samplecnt_t ret = graph_builder->process (samples_to_read, last_cycle);
	if (ret > 0) {
		process_position += ret;
		export_status->processed_samples += ret;
		export_status->processed_samples_current_timespan += ret;
	}

	/* Start post-processing/normalizing if necessary */
	if (last_cycle) {
		post_processing = graph_builder->need_postprocessing ();
		if (post_processing) {
			export_status->total_postprocessing_cycles = graph_builder->get_postprocessing_cycle_count();
			export_status->current_postprocessing_cycle = 0;
		} else {
			finish_timespan ();
		}
		return 1; /* trigger realtime_stop() */
	}

	return 0;
}

int
ExportHandler::post_process ()
{
	if (graph_builder->post_process ()) {
		finish_timespan ();
		export_status->active_job = ExportStatus::Exporting;
	} else {
		if (graph_builder->realtime ()) {
			export_status->active_job = ExportStatus::Encoding;
		} else {
			export_status->active_job = ExportStatus::Normalizing;
		}
	}

	export_status->current_postprocessing_cycle++;

	return 0;
}

void
ExportHandler::command_output(std::string output, size_t size)
{
	std::cerr << "command: " << size << ", " << output << std::endl;
	info << output << endmsg;
}

void*
ExportHandler::start_timespan_bg (void* eh)
{
	char name[64];
	snprintf (name, 64, "Export-TS-%p", (void*)DEBUG_THREAD_SELF);
	pthread_set_name (name);
	ExportHandler* self = static_cast<ExportHandler*> (eh);
	self->process_connection.disconnect ();
	Glib::Threads::Mutex::Lock l (self->export_status->lock());
	self->start_timespan ();
	return 0;
}

void
ExportHandler::finish_timespan ()
{
	graph_builder->get_analysis_results (export_status->result_map);

	while (config_map.begin() != timespan_bounds.second) {

		// XXX single timespan+format may produce multiple files
		// e.g export selection == session
		// -> TagLib::FileRef is null

		ExportFormatSpecPtr fmt = config_map.begin()->second.format;
		std::string filename = config_map.begin()->second.filename->get_path(fmt);
		if (fmt->with_cue()) {
			export_cd_marker_file (current_timespan, fmt, filename, CDMarkerCUE);
		}

		if (fmt->with_toc()) {
			export_cd_marker_file (current_timespan, fmt, filename, CDMarkerTOC);
		}

		if (fmt->with_mp4chaps()) {
			export_cd_marker_file (current_timespan, fmt, filename, MP4Chaps);
		}

		Session::Exported (current_timespan->name(), filename); /* EMIT SIGNAL */

		/* close file first, otherwise TagLib enounters an ERROR_SHARING_VIOLATION
		 * The process cannot access the file because it is being used.
		 * ditto for post-export and upload.
		 */
		graph_builder->reset ();

		if (fmt->tag()) {
			/* TODO: check Umlauts and encoding in filename.
			 * TagLib eventually calls CreateFileA(),
			 */
			export_status->active_job = ExportStatus::Tagging;
			AudiofileTagger::tag_file(filename, *SessionMetadata::Metadata());
		}

		if (!fmt->command().empty()) {
			SessionMetadata const & metadata (*SessionMetadata::Metadata());

#if 0 // would be nicer with C++11 initialiser...
			std::map<char, std::string> subs {
				{ 'f', filename },
				{ 'd', Glib::path_get_dirname(filename)  + G_DIR_SEPARATOR },
				{ 'b', PBD::basename_nosuffix(filename) },
				...
			};
#endif
			export_status->active_job = ExportStatus::Command;
			PBD::ScopedConnection command_connection;
			std::map<char, std::string> subs;

			std::stringstream track_number;
			track_number << metadata.track_number ();
			std::stringstream total_tracks;
			total_tracks << metadata.total_tracks ();
			std::stringstream year;
			year << metadata.year ();

			subs.insert (std::pair<char, std::string> ('a', metadata.artist ()));
			subs.insert (std::pair<char, std::string> ('b', PBD::basename_nosuffix (filename)));
			subs.insert (std::pair<char, std::string> ('c', metadata.copyright ()));
			subs.insert (std::pair<char, std::string> ('d', Glib::path_get_dirname (filename) + G_DIR_SEPARATOR));
			subs.insert (std::pair<char, std::string> ('f', filename));
			subs.insert (std::pair<char, std::string> ('l', metadata.lyricist ()));
			subs.insert (std::pair<char, std::string> ('n', session.name ()));
			subs.insert (std::pair<char, std::string> ('s', session.path ()));
			subs.insert (std::pair<char, std::string> ('o', metadata.conductor ()));
			subs.insert (std::pair<char, std::string> ('t', metadata.title ()));
			subs.insert (std::pair<char, std::string> ('z', metadata.organization ()));
			subs.insert (std::pair<char, std::string> ('A', metadata.album ()));
			subs.insert (std::pair<char, std::string> ('C', metadata.comment ()));
			subs.insert (std::pair<char, std::string> ('E', metadata.engineer ()));
			subs.insert (std::pair<char, std::string> ('G', metadata.genre ()));
			subs.insert (std::pair<char, std::string> ('L', total_tracks.str ()));
			subs.insert (std::pair<char, std::string> ('M', metadata.mixer ()));
			subs.insert (std::pair<char, std::string> ('N', current_timespan->name())); // =?= config_map.begin()->first->name ()
			subs.insert (std::pair<char, std::string> ('O', metadata.composer ()));
			subs.insert (std::pair<char, std::string> ('P', metadata.producer ()));
			subs.insert (std::pair<char, std::string> ('S', metadata.disc_subtitle ()));
			subs.insert (std::pair<char, std::string> ('T', track_number.str ()));
			subs.insert (std::pair<char, std::string> ('Y', year.str ()));
			subs.insert (std::pair<char, std::string> ('Z', metadata.country ()));

			ARDOUR::SystemExec *se = new ARDOUR::SystemExec(fmt->command(), subs);
			info << "Post-export command line : {" << se->to_s () << "}" << endmsg;
			se->ReadStdout.connect_same_thread(command_connection, boost::bind(&ExportHandler::command_output, this, _1, _2));
			int ret = se->start (SystemExec::MergeWithStdin);
			if (ret == 0) {
				// successfully started
				while (se->is_running ()) {
					// wait for system exec to terminate
					Glib::usleep (1000);
				}
			} else {
				error << "Post-export command FAILED with Error: " << ret << endmsg;
			}
			delete (se);
		}

		// XXX THIS IS IN REALTIME CONTEXT, CALLED FROM
		// AudioEngine::process_callback()
		// freewheeling, yes, but still uploading here is NOT
		// a good idea.
		//
		// even less so, since SoundcloudProgress is using
		// connect_same_thread() - GUI updates from the RT thread
		// will cause crashes. http://pastebin.com/UJKYNGHR
		if (fmt->soundcloud_upload()) {
			SoundcloudUploader *soundcloud_uploader = new SoundcloudUploader;
			std::string token = soundcloud_uploader->Get_Auth_Token(soundcloud_username, soundcloud_password);
			DEBUG_TRACE (DEBUG::Soundcloud, string_compose(
						"uploading %1 - username=%2, password=%3, token=%4",
						filename, soundcloud_username, soundcloud_password, token) );
			std::string path = soundcloud_uploader->Upload (
					filename,
					PBD::basename_nosuffix(filename), // title
					token,
					soundcloud_make_public,
					soundcloud_downloadable,
					this);

			if (path.length() != 0) {
				info << string_compose ( _("File %1 uploaded to %2"), filename, path) << endmsg;
				if (soundcloud_open_page) {
					DEBUG_TRACE (DEBUG::Soundcloud, string_compose ("opening %1", path) );
					open_uri(path.c_str());  // open the soundcloud website to the new file
				}
			} else {
				error << _("upload to Soundcloud failed. Perhaps your email or password are incorrect?\n") << endmsg;
			}
			delete soundcloud_uploader;
		}
		config_map.erase (config_map.begin());
	}

	/* finish timespan is called in freewheeling rt-context,
	 * we cannot start a new export from here */
	assert (AudioEngine::instance()->freewheeling ());
	pthread_t tid;
	pthread_create (&tid, NULL, ExportHandler::start_timespan_bg, this);
	pthread_detach (tid);
}

void
ExportHandler::reset ()
{
	config_map.clear ();
	graph_builder->reset ();
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
		case MP4Chaps:
			header_func = &ExportHandler::write_mp4ch_header;
			track_func = &ExportHandler::write_track_info_mp4ch;
			index_func = &ExportHandler::write_index_info_mp4ch;
			break;
		default:
			return;
		}

		CDMarkerStatus status (filepath, timespan, file_format, filename);

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

		samplepos_t last_end_time = timespan->get_start();
		status.track_position = 0;

		for (i = temp.begin(); i != temp.end(); ++i) {

			status.marker = *i;

			if ((*i)->start() < last_end_time) {
				if ((*i)->is_mark()) {
					/* Index within track */

					status.index_position = (*i)->start_sample() - timespan->get_start();
					(this->*index_func) (status);
				}

				continue;
			}

			/* A track, defined by a cd range marker or a cd location marker outside of a cd range */

			status.track_position = last_end_time - timespan->get_start();
			status.track_start_sample = (*i)->start_sample() - timespan->get_start();  // everything before this is the pregap
			status.track_duration = 0;

			if ((*i)->is_mark()) {
				// a mark track location needs to look ahead to the next marker's start to determine length
				nexti = i;
				++nexti;

				if (nexti != temp.end()) {
					status.track_duration = (*nexti)->start_sample() - last_end_time;

					last_end_time = (*nexti)->start_sample();
				} else {
					// this was the last marker, use timespan end
					status.track_duration = timespan->get_end() - last_end_time;

					last_end_time = timespan->get_end();
				}
			} else {
				// range
				status.track_duration = (*i)->end_sample() - last_end_time;

				last_end_time = (*i)->end_sample();
			}

			(this->*track_func) (status);
		}

	} catch (std::exception& e) {
		error << string_compose (_("an error occurred while writing a TOC/CUE file: %1"), e.what()) << endmsg;
		::g_unlink (filepath.c_str());
	} catch (Glib::Exception& e) {
		error << string_compose (_("an error occurred while writing a TOC/CUE file: %1"), e.what()) << endmsg;
		::g_unlink (filepath.c_str());
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
	case MP4Chaps:
	{
		unsigned lastdot = filename.find_last_of('.');
		return filename.substr(0,lastdot) + ".chapters.txt";
	}
	default:
		return filename + ".marker"; // Should not be reached when actually creating a file
	}
}

void
ExportHandler::write_cue_header (CDMarkerStatus & status)
{
	string title = status.timespan->name().compare ("Session") ? status.timespan->name() : (string) session.name();

	// Album metadata
	string barcode      = SessionMetadata::Metadata()->barcode();
	string album_artist = SessionMetadata::Metadata()->album_artist();
	string album_title  = SessionMetadata::Metadata()->album();

	status.out << "REM Cue file generated by " << PROGRAM_NAME << endl;

	if (barcode != "")
		status.out << "CATALOG " << barcode << endl;

	if (album_artist != "")
		status.out << "PERFORMER " << cue_escape_cdtext (album_artist) << endl;

	if (album_title != "")
		title = album_title;

	status.out << "TITLE " << cue_escape_cdtext (title) << endl;

	/*  The original cue sheet spec mentions five file types
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

	// Album metadata
	string barcode      = SessionMetadata::Metadata()->barcode();
	string album_artist = SessionMetadata::Metadata()->album_artist();
	string album_title  = SessionMetadata::Metadata()->album();

	if (barcode != "")
		status.out << "CATALOG \"" << barcode << "\"" << endl;

	if (album_title != "")
		title = album_title;

	status.out << "CD_DA" << endl;
	status.out << "CD_TEXT {" << endl << "  LANGUAGE_MAP {" << endl << "    0 : EN" << endl << "  }" << endl;
	status.out << "  LANGUAGE 0 {" << endl << "    TITLE " << toc_escape_cdtext (title) << endl ;
	status.out << "    PERFORMER " << toc_escape_cdtext (album_artist) << endl;
	status.out << "  }" << endl << "}" << endl;
}

void
ExportHandler::write_mp4ch_header (CDMarkerStatus & status)
{
	status.out << "00:00:00.000 Intro" << endl;
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

	if (status.track_position != status.track_start_sample) {
		samples_to_cd_frame_string (buf, status.track_position);
		status.out << "    INDEX 00" << buf << endl;
	}

	samples_to_cd_frame_string (buf, status.track_start_sample);
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

	samples_to_cd_frame_string (buf, status.track_position);
	status.out << "FILE " << toc_escape_filename (status.filename) << ' ' << buf;

	samples_to_cd_frame_string (buf, status.track_duration);
	status.out << buf << endl;

	samples_to_cd_frame_string (buf, status.track_start_sample - status.track_position);
	status.out << "START" << buf << endl;
}

void ExportHandler::write_track_info_mp4ch (CDMarkerStatus & status)
{
	gchar buf[18];

	samples_to_chapter_marks_string(buf, status.track_start_sample);
	status.out << buf << " " << status.marker->name() << endl;
}

void
ExportHandler::write_index_info_cue (CDMarkerStatus & status)
{
	gchar buf[18];

	snprintf (buf, sizeof(buf), "    INDEX %02d", cue_indexnum);
	status.out << buf;
	samples_to_cd_frame_string (buf, status.index_position);
	status.out << buf << endl;

	cue_indexnum++;
}

void
ExportHandler::write_index_info_toc (CDMarkerStatus & status)
{
	gchar buf[18];

	samples_to_cd_frame_string (buf, status.index_position - status.track_start_sample);
	status.out << "INDEX" << buf << endl;
}

void
ExportHandler::write_index_info_mp4ch (CDMarkerStatus & status)
{
}

void
ExportHandler::samples_to_cd_frame_string (char* buf, samplepos_t when)
{
	samplecnt_t remainder;
	samplecnt_t fr = session.nominal_sample_rate();
	int mins, secs, samples;

	mins = when / (60 * fr);
	remainder = when - (mins * 60 * fr);
	secs = remainder / fr;
	remainder -= secs * fr;
	samples = remainder / (fr / 75);
	sprintf (buf, " %02d:%02d:%02d", mins, secs, samples);
}

void
ExportHandler::samples_to_chapter_marks_string (char* buf, samplepos_t when)
{
	samplecnt_t remainder;
	samplecnt_t fr = session.nominal_sample_rate();
	int hours, mins, secs, msecs;

	hours = when / (3600 * fr);
	remainder = when - (hours * 3600 * fr);
	mins = remainder / (60 * fr);
	remainder -= mins * 60 * fr;
	secs = remainder / fr;
	remainder -= secs * fr;
	msecs = (remainder * 1000) / fr;
	sprintf (buf, "%02d:%02d:%02d.%03d", hours, mins, secs, msecs);
}

std::string
ExportHandler::toc_escape_cdtext (const std::string& txt)
{
	Glib::ustring check (txt);
	std::string out;
	std::string latin1_txt;
	char buf[5];

	try {
		latin1_txt = Glib::convert_with_fallback (txt, "ISO-8859-1", "UTF-8", "_");
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

ExportHandler::CDMarkerStatus::~CDMarkerStatus () {
	if (!g_file_set_contents (path.c_str(), out.str().c_str(), -1, NULL)) {
		PBD::error << string_compose(("Editor: cannot open \"%1\" as export file for CD marker file"), path) << endmsg;
	}
}

} // namespace ARDOUR
