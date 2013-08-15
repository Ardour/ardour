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

#ifndef __ardour_export_profile_manager_h__
#define __ardour_export_profile_manager_h__

#include <list>
#include <vector>
#include <map>
#include <stdexcept>
#include <string>

#include <boost/shared_ptr.hpp>

#include "pbd/uuid.h"
#include "pbd/file_utils.h"
#include "pbd/xml++.h"

#include "ardour/filesystem_paths.h"
#include "ardour/location.h"
#include "ardour/types.h"
#include "ardour/export_handler.h"

namespace ARDOUR
{

class ExportHandler;
class Location;
class Session;

/// Manages (de)serialization of export profiles and related classes
class ExportProfileManager
{
  public:

	enum ExportType {
		RegularExport,
		RangeExport,
		SelectionExport,
		RegionExport,
		StemExport
	};

	ExportProfileManager (Session & s, ExportType type);
	~ExportProfileManager ();

	void load_profile ();
	void prepare_for_export ();

	typedef std::list<ExportPresetPtr> PresetList;

	PresetList const & get_presets () { return preset_list; }
	bool load_preset (ExportPresetPtr preset);
	ExportPresetPtr new_preset (std::string const & name);
	ExportPresetPtr save_preset (std::string const & name);
	void remove_preset ();

  private:
	typedef boost::shared_ptr<ExportHandler> HandlerPtr;

	typedef std::pair<PBD::UUID, std::string> FilePair;
	typedef std::map<PBD::UUID, std::string> FileMap;

	ExportType type;
	std::string xml_node_name;
	HandlerPtr  handler;
	Session &   session;

	std::string preset_filename (std::string const & preset_name);
	void load_presets ();
	void load_preset_from_disk (std::string const & path);

	bool set_state (XMLNode const & root);
	bool set_global_state (XMLNode const & root);
	bool set_local_state (XMLNode const & root);

	void serialize_profile (XMLNode & root);
	void serialize_global_profile (XMLNode & root);
	void serialize_local_profile (XMLNode & root);

	PresetList      preset_list;
	ExportPresetPtr current_preset;
	FileMap         preset_file_map;

	std::vector<std::string> find_file (std::string const & pattern);

	std::string  export_config_dir;
	PBD::Searchpath search_path;

/* Timespans */
  public:

	typedef std::list<ExportTimespanPtr> TimespanList;
	typedef boost::shared_ptr<TimespanList> TimespanListPtr;
	typedef std::list<Location *> LocationList;

	enum TimeFormat {
		Timecode,
		BBT,
		MinSec,
		Frames,
	};

	struct TimespanState {
		TimespanListPtr timespans;
		TimeFormat      time_format;

		boost::shared_ptr<Location> selection_range;
		boost::shared_ptr<LocationList> ranges;

		TimespanState (boost::shared_ptr<Location> selection_range,
		               boost::shared_ptr<LocationList> ranges)
		  : timespans (new TimespanList ())
		  , time_format (Timecode)
		  , selection_range (selection_range)
		  , ranges (ranges)
		{}
	};

	typedef boost::shared_ptr<TimespanState> TimespanStatePtr;
	typedef std::list<TimespanStatePtr> TimespanStateList;

	void set_selection_range (framepos_t start = 0, framepos_t end = 0);
	std::string set_single_range (framepos_t start, framepos_t end, std::string name);
	TimespanStateList const & get_timespans () { return check_list (timespans); }

  private:

	TimespanStateList timespans;

	bool init_timespans (XMLNodeList nodes);

	TimespanStatePtr deserialize_timespan (XMLNode & root);
	XMLNode & serialize_timespan (TimespanStatePtr state);

	/* Locations */

	void update_ranges ();

	boost::shared_ptr<Location>     selection_range;
	boost::shared_ptr<LocationList> ranges;

	bool                            single_range_mode;
	boost::shared_ptr<Location>     single_range;

/* Channel Configs */
  public:

	struct ChannelConfigState {
		ExportChannelConfigPtr config;

		ChannelConfigState (ExportChannelConfigPtr ptr) : config (ptr) {}
	};
	typedef boost::shared_ptr<ChannelConfigState> ChannelConfigStatePtr;
	typedef std::list<ChannelConfigStatePtr> ChannelConfigStateList;

	ChannelConfigStateList const & get_channel_configs () { return check_list (channel_configs); }
	void clear_channel_configs () { channel_configs.clear(); }
	ChannelConfigStatePtr add_channel_config ();

  private:

	ChannelConfigStateList channel_configs;

	bool init_channel_configs (XMLNodeList nodes);

/* Formats */
  public:

	typedef std::list<ExportFormatSpecPtr> FormatList;

	struct FormatState {
		boost::shared_ptr<FormatList const> list;
		ExportFormatSpecPtr                     format;

		FormatState (boost::shared_ptr<FormatList const> list, ExportFormatSpecPtr format) :
		  list (list), format (format) {}
	};
	typedef boost::shared_ptr<FormatState> FormatStatePtr;
	typedef std::list<FormatStatePtr> FormatStateList;

	FormatStateList const & get_formats () { return check_list (formats); }
	FormatStatePtr duplicate_format_state (FormatStatePtr state);
	void remove_format_state (FormatStatePtr state);

	std::string save_format_to_disk (ExportFormatSpecPtr format);
	void remove_format_profile (ExportFormatSpecPtr format);
	ExportFormatSpecPtr get_new_format (ExportFormatSpecPtr original);

	PBD::Signal0<void> FormatListChanged;

  private:

	FormatStateList formats;

	bool init_formats (XMLNodeList nodes);
	FormatStatePtr deserialize_format (XMLNode & root);
	XMLNode & serialize_format (FormatStatePtr state);

	void load_formats ();

	ExportFormatSpecPtr load_format (XMLNode & node);
	void load_format_from_disk (std::string const & path);

	boost::shared_ptr<FormatList> format_list;
	FileMap                       format_file_map;

/* Filenames */
  public:

	struct FilenameState {
		ExportFilenamePtr  filename;

		FilenameState (ExportFilenamePtr ptr) : filename (ptr) {}
	};
	typedef boost::shared_ptr<FilenameState> FilenameStatePtr;
	typedef std::list<FilenameStatePtr> FilenameStateList;

	FilenameStateList const & get_filenames () { return check_list (filenames); }
	FilenameStatePtr duplicate_filename_state (FilenameStatePtr state);
	void remove_filename_state (FilenameStatePtr state);

	std::string get_sample_filename_for_format (ExportFilenamePtr filename, ExportFormatSpecPtr format);

  private:

	FilenameStateList filenames;

	bool init_filenames (XMLNodeList nodes);
	ExportFilenamePtr load_filename (XMLNode & node);

/* Warnings */
  public:
	struct Warnings {
		std::list<std::string> errors;
		std::list<std::string> warnings;
		std::list<std::string> conflicting_filenames;
	};

	boost::shared_ptr<Warnings> get_warnings ();

  private:
	void check_config (boost::shared_ptr<Warnings> warnings,
	                   TimespanStatePtr timespan_state,
	                   ChannelConfigStatePtr channel_config_state,
	                   FormatStatePtr format_state,
	                   FilenameStatePtr filename_state);

	bool check_format (ExportFormatSpecPtr format, uint32_t channels);
	bool check_sndfile_format (ExportFormatSpecPtr format, unsigned int channels);

 /* Utilities */

	void build_filenames(std::list<std::string> & result, ExportFilenamePtr filename,
	                     TimespanListPtr timespans, ExportChannelConfigPtr channel_config,
	                     ExportFormatSpecPtr format);

	/* Element state lists should never be empty, this is used to check them */
	template<typename T>
	std::list<T> const &
	check_list (std::list<T> const & list)
	{
		if (list.empty()) {
			throw std::runtime_error ("Programming error: Uninitialized list in ExportProfileManager");
		}
		return list;
	}

};


} // namespace ARDOUR

#endif /* __ardour_export_profile_manager_h__ */
