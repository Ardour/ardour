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
#include <list>
#include <fstream>

#include <boost/shared_ptr.hpp>

#include "ardour/ardour.h"
#include "ardour/types.h"
#include "ardour/session.h"

namespace ARDOUR
{

class ExportTimespan;
class ExportChannelConfiguration;
class ExportFormatSpecification;
class ExportFilename;
class ExportProcessor;

class ExportElementFactory
{
  protected:
	typedef boost::shared_ptr<ExportTimespan> TimespanPtr;
	typedef boost::shared_ptr<ExportChannelConfiguration> ChannelConfigPtr;
	typedef boost::shared_ptr<ExportFormatSpecification> FormatPtr;
	typedef boost::shared_ptr<ExportFilename> FilenamePtr;

  public:

	ExportElementFactory (Session & session);
	~ExportElementFactory ();

	TimespanPtr      add_timespan ();

	ChannelConfigPtr add_channel_config ();

	FormatPtr        add_format ();
	FormatPtr        add_format (XMLNode const & state);
	FormatPtr        add_format_copy (FormatPtr other);

	FilenamePtr      add_filename ();
	FilenamePtr      add_filename_copy (FilenamePtr other);

  private:
	Session & session;
};

class ExportHandler : public ExportElementFactory, public sigc::trackable
{
  private:
	
	/* Stuff for export configs
	 * The multimap maps timespans to file specifications
	 */
	
	struct FileSpec {
	
		FileSpec (ChannelConfigPtr channel_config, FormatPtr format, FilenamePtr filename) :
		  channel_config (channel_config),
		  format (format),
		  filename (filename)
		  {}
	
		ChannelConfigPtr channel_config;
		FormatPtr        format;
		FilenamePtr      filename;
	};
	
	typedef std::pair<TimespanPtr, FileSpec> ConfigPair;
	typedef std::multimap<TimespanPtr, FileSpec> ConfigMap;
	
	typedef boost::shared_ptr<ExportProcessor> ProcessorPtr;
	typedef boost::shared_ptr<ExportStatus> StatusPtr;

  private:
	/* Session::get_export_handler() should be used to obtain an export handler
	 * This ensures that it doesn't go out of scope before finalize_audio_export is called
	 */

	friend boost::shared_ptr<ExportHandler> Session::get_export_handler();
	ExportHandler (Session & session);
	
  public:
	~ExportHandler ();

	bool add_export_config (TimespanPtr timespan, ChannelConfigPtr channel_config, FormatPtr format, FilenamePtr filename);
	void do_export (bool rt = false);

  private:

	Session &          session;
	ProcessorPtr       processor;
	StatusPtr          export_status;
	ConfigMap          config_map;
	
	bool               realtime;
	
	sigc::connection          files_written_connection;
	std::list<Glib::ustring> files_written;
	
	/* CD Marker stuff */
	
	struct CDMarkerStatus {
		CDMarkerStatus (std::string out_file, TimespanPtr timespan, FormatPtr format, std::string filename) :
		  out (out_file.c_str()), timespan (timespan), format (format), filename (filename),
		  track_number (1), track_position (0), track_duration (0), track_start_frame (0),
		  index_number (1), index_position (0)
		  {}
		
		/* General info */
		std::ofstream  out;
		TimespanPtr    timespan;
		FormatPtr      format;
		std::string    filename;
		Location *     marker;
		
		/* Track info */
		uint32_t       track_number;
		nframes_t      track_position;
		nframes_t      track_duration;
		nframes_t      track_start_frame;
		
		/* Index info */
		uint32_t       index_number;
		nframes_t      index_position;
	};
	
	
	void export_cd_marker_file (TimespanPtr timespan, FormatPtr file_format, std::string filename, CDMarkerFormat format);
	
	void write_cue_header (CDMarkerStatus & status);
	void write_toc_header (CDMarkerStatus & status);
	
	void write_track_info_cue (CDMarkerStatus & status);
	void write_track_info_toc (CDMarkerStatus & status);

	void write_index_info_cue (CDMarkerStatus & status);
	void write_index_info_toc (CDMarkerStatus & status);
	
	void frames_to_cd_frames_string (char* buf, nframes_t when);
	
	int cue_tracknum;
	int cue_indexnum;
	
	/* Timespan management */
	
	void start_timespan ();
	void finish_timespan ();
	void timespan_thread_finished ();
	
	typedef std::pair<ConfigMap::iterator, ConfigMap::iterator> TimespanBounds;
	TimespanPtr          current_timespan;
	ConfigMap::iterator  current_map_it;
	TimespanBounds       timespan_bounds;
	sigc::connection     channel_config_connection;

};

} // namespace ARDOUR

#endif /* __ardour_export_handler_h__ */
