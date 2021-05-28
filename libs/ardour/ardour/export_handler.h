/*
 * Copyright (C) 2008-2013 Sakari Bergen <sakari.bergen@beatwaves.net>
 * Copyright (C) 2008-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2013-2014 Colin Fletcher <colin.m.fletcher@googlemail.com>
 * Copyright (C) 2015-2019 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_export_handler_h__
#define __ardour_export_handler_h__

#include <map>

#include <boost/operators.hpp>
#include <boost/shared_ptr.hpp>

#include "pbd/gstdio_compat.h"

#include "ardour/export_pointers.h"
#include "ardour/session.h"
#include "ardour/libardour_visibility.h"
#include "ardour/types.h"
#include "pbd/signals.h"

namespace AudioGrapher {
	class BroadcastInfo;
}

namespace ARDOUR
{

class ExportTimespan;
class ExportChannelConfiguration;
class ExportFormatSpecification;
class ExportFilename;
class ExportGraphBuilder;
class Location;

class LIBARDOUR_API ExportElementFactory
{
  public:

	ExportElementFactory (Session & session);
	~ExportElementFactory ();

	ExportTimespanPtr add_timespan ();

	ExportChannelConfigPtr add_channel_config ();

	ExportFormatSpecPtr    add_format ();
	ExportFormatSpecPtr    add_format (XMLNode const & state);
	ExportFormatSpecPtr    add_format_copy (ExportFormatSpecPtr other);

	ExportFilenamePtr      add_filename ();
	ExportFilenamePtr      add_filename_copy (ExportFilenamePtr other);

  private:
	Session & session;
};

/** Export Handler */
class LIBARDOUR_API ExportHandler : public ExportElementFactory, public sigc::trackable
{
  public:
	struct FileSpec {
		FileSpec() {}
		FileSpec (ExportChannelConfigPtr channel_config, ExportFormatSpecPtr format,
		          ExportFilenamePtr filename, BroadcastInfoPtr broadcast_info)
		  : channel_config (channel_config)
		  , format (format)
		  , filename (filename)
		  , broadcast_info (broadcast_info)
			{}

		ExportChannelConfigPtr channel_config;
		ExportFormatSpecPtr    format;
		ExportFilenamePtr      filename;
		BroadcastInfoPtr       broadcast_info;
	};

  private:
	/* Session::get_export_handler() should be used to obtain an export handler
	 * This ensures that it doesn't go out of scope before finalize_audio_export is called
	 */

	friend boost::shared_ptr<ExportHandler> Session::get_export_handler();
	ExportHandler (Session & session);

	void command_output(std::string output, size_t size);

  public:
	~ExportHandler ();

	bool add_export_config (ExportTimespanPtr timespan, ExportChannelConfigPtr channel_config,
	                        ExportFormatSpecPtr format, ExportFilenamePtr filename,
	                        BroadcastInfoPtr broadcast_info);
	int do_export ();

	std::string get_cd_marker_filename(std::string filename, CDMarkerFormat format);

	/** signal emitted when soundcloud export reports progress updates during upload.
	 * The parameters are total and current bytes downloaded, and the current filename
	 */
	PBD::Signal3<void, double, double, std::string> SoundcloudProgress;

	/* upload credentials & preferences */
	std::string soundcloud_username;
	std::string soundcloud_password;
	bool soundcloud_make_public;
	bool soundcloud_open_page;
	bool soundcloud_downloadable;

	void reset ();

  private:

	void handle_duplicate_format_extensions();
	int process (samplecnt_t samples);

	Session &          session;
	boost::shared_ptr<ExportGraphBuilder> graph_builder;
	ExportStatusPtr    export_status;

	/* The timespan and corresponding file specifications that we are exporting;
	   there can be multiple FileSpecs for each ExportTimespan.
	*/
	typedef std::multimap<ExportTimespanPtr, FileSpec> ConfigMap;
	ConfigMap          config_map;

	bool               post_processing;

	/* Timespan management */

	static void* start_timespan_bg (void*);

	int  start_timespan ();
	int  process_timespan (samplecnt_t samples);
	int  post_process ();
	void finish_timespan ();

	typedef std::pair<ConfigMap::iterator, ConfigMap::iterator> TimespanBounds;
	ExportTimespanPtr     current_timespan;
	TimespanBounds        timespan_bounds;

	PBD::ScopedConnection process_connection;
	samplepos_t           process_position;

	/* CD Marker stuff */

	struct CDMarkerStatus {
		CDMarkerStatus (std::string out_file, ExportTimespanPtr timespan,
		                ExportFormatSpecPtr format, std::string filename)
		  : path (out_file)
		  , timespan (timespan)
		  , format (format)
		  , filename (filename)
		  , marker(0)
		  , track_number (1)
		  , track_position (0)
		  , track_duration (0)
		  , track_start_sample (0)
		  , index_number (1)
		  , index_position (0)
		  {}

		~CDMarkerStatus ();

		/* I/O */
		std::string         path;
		std::stringstream   out;

		/* General info */
		ExportTimespanPtr   timespan;
		ExportFormatSpecPtr format;
		std::string         filename;
		Location *          marker;

		/* Track info */
		uint32_t        track_number;
		samplepos_t     track_position;
		samplepos_t     track_duration;
		samplepos_t     track_start_sample;

		/* Index info */
		uint32_t        index_number;
		samplepos_t     index_position;
	};


	void export_cd_marker_file (ExportTimespanPtr timespan, ExportFormatSpecPtr file_format,
	                            std::string filename, CDMarkerFormat format);

	void write_cue_header (CDMarkerStatus & status);
	void write_toc_header (CDMarkerStatus & status);
	void write_mp4ch_header (CDMarkerStatus & status);

	void write_track_info_cue (CDMarkerStatus & status);
	void write_track_info_toc (CDMarkerStatus & status);
	void write_track_info_mp4ch (CDMarkerStatus & status);

	void write_index_info_cue (CDMarkerStatus & status);
	void write_index_info_toc (CDMarkerStatus & status);
	void write_index_info_mp4ch (CDMarkerStatus & status);

	void samples_to_cd_frame_string (char* buf, samplepos_t when);
	void samples_to_chapter_marks_string (char* buf, samplepos_t when);

	std::string toc_escape_cdtext (const std::string&);
	std::string toc_escape_filename (const std::string&);
	std::string cue_escape_cdtext (const std::string& txt);

	int cue_tracknum;
	int cue_indexnum;
};

} // namespace ARDOUR

#endif /* __ardour_export_handler_h__ */
