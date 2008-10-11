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
#include <set>
#include <stdexcept>

#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <sigc++/signal.h>
#include <glibmm/ustring.h>

#include <pbd/uuid.h>
#include <pbd/file_utils.h>
#include <pbd/xml++.h>

#include <ardour/filesystem_paths.h>
#include <ardour/location.h>
#include <ardour/types.h>

using std::string;
using std::list;
using std::set;

namespace ARDOUR
{

class ExportHandler;
class ExportTimespan;
class ExportChannelConfiguration;
class ExportFormatSpecification;
class ExportFilename;
class ExportPreset;
class Location;
class Session;

/// Manages (de)serialization of export profiles and related classes
class ExportProfileManager
{
  public:

	ExportProfileManager (Session & s);
	~ExportProfileManager ();

	void load_profile ();
	void prepare_for_export ();
	
	typedef boost::shared_ptr<ExportPreset> PresetPtr;
	typedef std::list<PresetPtr> PresetList;
	
	PresetList const & get_presets () { return preset_list; }
	bool load_preset (PresetPtr preset);
	PresetPtr save_preset (string const & name);
	void remove_preset ();

  private:
	typedef boost::shared_ptr<ExportHandler> HandlerPtr;

	typedef std::pair<PBD::UUID, PBD::sys::path> FilePair;
	typedef std::map<PBD::UUID, PBD::sys::path> FileMap;
	
	HandlerPtr  handler;
	Session &   session;
	
	void load_presets ();
	void load_preset_from_disk (PBD::sys::path const & path);
	
	bool set_state (XMLNode const & root);
	bool set_global_state (XMLNode const & root);
	bool set_local_state (XMLNode const & root);
	
	void serialize_profile (XMLNode & root);
	void serialize_global_profile (XMLNode & root);
	void serialize_local_profile (XMLNode & root);
	
	PresetList preset_list;
	PresetPtr  current_preset;
	FileMap    preset_file_map;
	
	std::vector<PBD::sys::path> find_file (std::string const & pattern);
	
	PBD::sys::path  export_config_dir;
	PBD::SearchPath search_path;

/* Timespans */
  public:

	typedef boost::shared_ptr<ExportTimespan> TimespanPtr;
	typedef std::list<TimespanPtr> TimespanList;
	typedef boost::shared_ptr<TimespanList> TimespanListPtr;
	typedef std::list<Location *> LocationList;

	enum TimeFormat {
		SMPTE,
		BBT,
		MinSec,
		Frames,
		Off
	};

	struct TimespanState {
		TimespanListPtr timespans;
		TimeFormat      time_format;
		
		boost::shared_ptr<Location> session_range;
		boost::shared_ptr<Location> selection_range;
		boost::shared_ptr<LocationList> ranges;
		
		TimespanState (boost::shared_ptr<Location> session_range,
		               boost::shared_ptr<Location> selection_range,
		               boost::shared_ptr<LocationList> ranges) :
		  timespans (new TimespanList ()),
		  time_format (SMPTE),
		
		  session_range (session_range),
		  selection_range (selection_range),
		  ranges (ranges)
		{}
	};
	
	typedef boost::shared_ptr<TimespanState> TimespanStatePtr;
	typedef std::list<TimespanStatePtr> TimespanStateList;
	
	void set_selection_range (nframes_t start = 0, nframes_t end = 0);
	std::string set_single_range (nframes_t start, nframes_t end, Glib::ustring name);
	TimespanStateList const & get_timespans () { return check_list (timespans); }
	
  private:

	TimespanStateList timespans;

	bool init_timespans (XMLNodeList nodes);
	
	TimespanStatePtr deserialize_timespan (XMLNode & root);
	XMLNode & serialize_timespan (TimespanStatePtr state);
	
	/* Locations */
	
	void update_ranges ();
	
	boost::shared_ptr<Location>     session_range;
	boost::shared_ptr<Location>     selection_range;
	boost::shared_ptr<LocationList> ranges;
	
	bool                            single_range_mode;
	boost::shared_ptr<Location>     single_range;

/* Channel Configs */
  public:

	typedef boost::shared_ptr<ExportChannelConfiguration> ChannelConfigPtr;

	struct ChannelConfigState {
		ChannelConfigPtr config;
		
		ChannelConfigState (ChannelConfigPtr ptr) : config (ptr) {}
	};
	typedef boost::shared_ptr<ChannelConfigState> ChannelConfigStatePtr;
	typedef std::list<ChannelConfigStatePtr> ChannelConfigStateList;
	
	ChannelConfigStateList const & get_channel_configs () { return check_list (channel_configs); }

  private:

	ChannelConfigStateList channel_configs;

	bool init_channel_configs (XMLNodeList nodes);

/* Formats */
  public:

	typedef boost::shared_ptr<ExportFormatSpecification> FormatPtr;
	typedef std::list<FormatPtr> FormatList;

	struct FormatState {
		boost::shared_ptr<FormatList const> list;
		FormatPtr                           format;
		
		FormatState (boost::shared_ptr<FormatList const> list, FormatPtr format) :
		  list (list), format (format) {}
	};
	typedef boost::shared_ptr<FormatState> FormatStatePtr;
	typedef std::list<FormatStatePtr> FormatStateList;
	
	FormatStateList const & get_formats () { return check_list (formats); }
	FormatStatePtr duplicate_format_state (FormatStatePtr state);
	void remove_format_state (FormatStatePtr state);
	
	PBD::sys::path save_format_to_disk (FormatPtr format);
	void remove_format_profile (FormatPtr format);
	FormatPtr get_new_format (FormatPtr original);
	
	sigc::signal<void> FormatListChanged;

  private:

	FormatStateList formats;

	bool init_formats (XMLNodeList nodes);
	FormatStatePtr deserialize_format (XMLNode & root);
	XMLNode & serialize_format (FormatStatePtr state);

	void load_formats ();
	
	FormatPtr load_format (XMLNode & node);
	void load_format_from_disk (PBD::sys::path const & path);

	boost::shared_ptr<FormatList> format_list;
	FileMap                       format_file_map;
	
/* Filenames */
  public:
	
	typedef boost::shared_ptr<ExportFilename> FilenamePtr;
	
	struct FilenameState {
		FilenamePtr  filename;
		
		FilenameState (FilenamePtr ptr) : filename (ptr) {}
	};
	typedef boost::shared_ptr<FilenameState> FilenameStatePtr;
	typedef std::list<FilenameStatePtr> FilenameStateList;
	
	FilenameStateList const & get_filenames () { return check_list (filenames); }
	FilenameStatePtr duplicate_filename_state (FilenameStatePtr state);
	void remove_filename_state (FilenameStatePtr state);

  private:

	FilenameStateList filenames;
	
	bool init_filenames (XMLNodeList nodes);
	FilenamePtr load_filename (XMLNode & node);

/* Warnings */
  public:
	struct Warnings {
		std::list<Glib::ustring> errors;
		std::list<Glib::ustring> warnings;
		std::list<Glib::ustring> conflicting_filenames;
	};
	
	boost::shared_ptr<Warnings> get_warnings ();
	
  private:
	void check_config (boost::shared_ptr<Warnings> warnings,
	                   TimespanStatePtr timespan_state,
	                   ChannelConfigStatePtr channel_config_state,
	                   FormatStatePtr format_state,
	                   FilenameStatePtr filename_state);

 /* Utilities */

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
