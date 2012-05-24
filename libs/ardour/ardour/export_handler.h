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

#ifndef __ardour_export_handler_h__
#define __ardour_export_handler_h__

#include <map>
#include <fstream>

#include <boost/operators.hpp>
#include <boost/shared_ptr.hpp>

#include "ardour/export_pointers.h"
#include "ardour/session.h"
#include "ardour/types.h"

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

class ExportElementFactory
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

class ExportHandler : public ExportElementFactory
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
		ExportFormatSpecPtr        format;
		ExportFilenamePtr      filename;
		BroadcastInfoPtr       broadcast_info;
	};

  private:

	/* Stuff for export configs
	 * The multimap maps timespans to file specifications
	 */

	typedef std::pair<ExportTimespanPtr, FileSpec> ConfigPair;
	typedef std::multimap<ExportTimespanPtr, FileSpec> ConfigMap;

	typedef boost::shared_ptr<ExportGraphBuilder> GraphBuilderPtr;

  private:
	/* Session::get_export_handler() should be used to obtain an export handler
	 * This ensures that it doesn't go out of scope before finalize_audio_export is called
	 */

	friend boost::shared_ptr<ExportHandler> Session::get_export_handler();
	ExportHandler (Session & session);

  public:
	~ExportHandler ();

	bool add_export_config (ExportTimespanPtr timespan, ExportChannelConfigPtr channel_config,
	                        ExportFormatSpecPtr format, ExportFilenamePtr filename,
	                        BroadcastInfoPtr broadcast_info);
	void do_export (bool rt = false);

	std::string get_cd_marker_filename(std::string filename, CDMarkerFormat format);

  private:

	int process (framecnt_t frames);

	Session &          session;
	GraphBuilderPtr    graph_builder;
	ExportStatusPtr    export_status;
	ConfigMap          config_map;

	bool               realtime;
	bool               normalizing;

	/* Timespan management */

	void start_timespan ();
	int  process_timespan (framecnt_t frames);
	int  process_normalize ();
	void finish_timespan ();

	typedef std::pair<ConfigMap::iterator, ConfigMap::iterator> TimespanBounds;
	ExportTimespanPtr     current_timespan;
	TimespanBounds        timespan_bounds;

	PBD::ScopedConnection process_connection;
	framepos_t             process_position;

	/* CD Marker stuff */

	struct CDMarkerStatus {
		CDMarkerStatus (std::string out_file, ExportTimespanPtr timespan,
		                ExportFormatSpecPtr format, std::string filename)
		  : out (out_file.c_str()), timespan (timespan), format (format), filename (filename), marker(0)
		  , track_number (1), track_position (0), track_duration (0), track_start_frame (0)
		  , index_number (1), index_position (0)
		  {}

		/* General info */
		std::ofstream  out;
		ExportTimespanPtr   timespan;
		ExportFormatSpecPtr format;
		std::string         filename;
		Location *          marker;

		/* Track info */
		uint32_t        track_number;
		framepos_t      track_position;
		framepos_t      track_duration;
		framepos_t      track_start_frame;

		/* Index info */
		uint32_t       index_number;
		framepos_t      index_position;
	};


	void export_cd_marker_file (ExportTimespanPtr timespan, ExportFormatSpecPtr file_format,
	                            std::string filename, CDMarkerFormat format);

	void write_cue_header (CDMarkerStatus & status);
	void write_toc_header (CDMarkerStatus & status);

	void write_track_info_cue (CDMarkerStatus & status);
	void write_track_info_toc (CDMarkerStatus & status);

	void write_index_info_cue (CDMarkerStatus & status);
	void write_index_info_toc (CDMarkerStatus & status);

	void frames_to_cd_frames_string (char* buf, framepos_t when);
	std::string toc_escape_cdtext (const std::string&);
	std::string toc_escape_filename (const std::string&);
	std::string cue_escape_cdtext (const std::string& txt);

	int cue_tracknum;
	int cue_indexnum;
};

} // namespace ARDOUR

#endif /* __ardour_export_handler_h__ */
