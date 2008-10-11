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

#include <ardour/export_profile_manager.h>

#include <cassert>
#include <stdexcept>

#include <glibmm/fileutils.h>

#include <pbd/enumwriter.h>
#include <pbd/xml++.h>
#include <pbd/convert.h>

#include <ardour/export_failed.h>
#include <ardour/export_file_io.h>
#include <ardour/export_format_specification.h>
#include <ardour/export_timespan.h>
#include <ardour/export_channel_configuration.h>
#include <ardour/export_filename.h>
#include <ardour/export_preset.h>
#include <ardour/export_handler.h>
#include <ardour/filename_extensions.h>
#include <ardour/session.h>

#include "i18n.h"

using namespace PBD;

namespace ARDOUR
{

ExportProfileManager::ExportProfileManager (Session & s) :
  handler (s.get_export_handler()),
  session (s),

  session_range (new Location ()),
  ranges (new LocationList ()),
  single_range_mode (false),

  format_list (new FormatList ())
{

	/* Initialize path variables */

	sys::path path;

	export_config_dir = user_config_directory();
	export_config_dir /= "export";
	search_path += export_config_dir;
	
	path = ardour_search_path().to_string();
	path /= "export";
	search_path += path;
	
	path = system_config_search_path().to_string();
	path /= "export";
	search_path += path;
	
	/* create export config directory if necessary */

	if (!sys::exists (export_config_dir)) {
		sys::create_directory (export_config_dir);
	}
	
	load_presets ();
	load_formats ();
	
	/* Initialize all lists with an empty config */
	
	XMLNodeList dummy;
	init_timespans (dummy);
	init_channel_configs (dummy);
	init_formats (dummy);
	init_filenames (dummy);
}

ExportProfileManager::~ExportProfileManager ()
{
	if (single_range_mode) { return; }
	
	XMLNode * instant_xml (new XMLNode ("ExportProfile"));
	serialize_profile (*instant_xml);
	session.add_instant_xml (*instant_xml, false);
}

void
ExportProfileManager::load_profile ()
{
	XMLNode * instant_node = session.instant_xml ("ExportProfile");
	if (instant_node) {
		set_state (*instant_node);
	} else {
		XMLNode empty_node ("ExportProfile");
		set_state (empty_node);
	}
}

void
ExportProfileManager::prepare_for_export ()
{
	ChannelConfigPtr channel_config = channel_configs.front()->config;
	TimespanListPtr ts_list = timespans.front()->timespans;
	
	FormatStateList::const_iterator format_it;
	FilenameStateList::const_iterator filename_it;
	
	for (TimespanList::iterator ts_it = ts_list->begin(); ts_it != ts_list->end(); ++ts_it) {
		for (format_it = formats.begin(), filename_it = filenames.begin();
		     format_it != formats.end() && filename_it != filenames.end();
		     ++format_it, ++filename_it) {
		
//			filename->include_timespan = (ts_list->size() > 1); Disabled for now...
			handler->add_export_config (*ts_it, channel_config, (*format_it)->format, (*filename_it)->filename);
		}
	}
}

bool
ExportProfileManager::load_preset (PresetPtr preset)
{
	bool ok = true;

	current_preset = preset;
	if (!preset) { return false; }

	XMLNode const * state;
	if ((state = preset->get_local_state())) {
		set_local_state (*state);
	} else { ok = false; }
	
	if ((state = preset->get_global_state())) {
		if (!set_global_state (*state)) {
			ok = false;
		}
	} else { ok = false; }
	
	return ok;
}

void
ExportProfileManager::load_presets ()
{
	vector<sys::path> found = find_file (string_compose (X_("*%1"),export_preset_suffix));

	for (vector<sys::path>::iterator it = found.begin(); it != found.end(); ++it) {
		load_preset_from_disk (*it);
	}
}

ExportProfileManager::PresetPtr
ExportProfileManager::save_preset (string const & name)
{
	if (!current_preset) {
		string filename = export_config_dir.to_string() + "/" + name + export_preset_suffix;
		current_preset.reset (new ExportPreset (filename, session));
		preset_list.push_back (current_preset);
	}
	
	XMLNode * global_preset = new XMLNode ("ExportPreset");
	XMLNode * local_preset = new XMLNode ("ExportPreset");

	serialize_global_profile (*global_preset);
	serialize_local_profile (*local_preset);

	current_preset->set_name (name);
	current_preset->set_global_state (*global_preset);
	current_preset->set_local_state (*local_preset);
	
	current_preset->save();
	
	return current_preset;
}

void
ExportProfileManager::remove_preset ()
{
	if (!current_preset) { return; }

	for (PresetList::iterator it = preset_list.begin(); it != preset_list.end(); ++it) {
		if (*it == current_preset) {
			preset_list.erase (it);
			break;
		}
	}

	FileMap::iterator it = preset_file_map.find (current_preset->id());
	if (it != preset_file_map.end()) {
		sys::remove (it->second);
		preset_file_map.erase (it);
	}
	
	current_preset->remove_local();
	current_preset.reset();
}

void
ExportProfileManager::load_preset_from_disk (PBD::sys::path const & path)
{
	PresetPtr preset (new ExportPreset (path.to_string(), session));
	preset_list.push_back (preset);
	
	/* Handle id to filename mapping */
	
	FilePair pair (preset->id(), path);
	preset_file_map.insert (pair);
}

bool
ExportProfileManager::set_state (XMLNode const & root)
{
	return set_global_state (root) && set_local_state (root);
}

bool
ExportProfileManager::set_global_state (XMLNode const & root)
{
	return init_filenames (root.children ("ExportFilename")) &&
	       init_formats (root.children ("ExportFormat"));
}

bool
ExportProfileManager::set_local_state (XMLNode const & root)
{
	return init_timespans (root.children ("ExportTimespan")) &&
	       init_channel_configs (root.children ("ExportChannelConfiguration"));
}

void
ExportProfileManager::serialize_profile (XMLNode & root)
{
	serialize_local_profile (root);
	serialize_global_profile (root);
}

void
ExportProfileManager::serialize_global_profile (XMLNode & root)
{
	for (FormatStateList::iterator it = formats.begin(); it != formats.end(); ++it) {
		root.add_child_nocopy (serialize_format (*it));
	}

	for (FilenameStateList::iterator it = filenames.begin(); it != filenames.end(); ++it) {
		root.add_child_nocopy ((*it)->filename->get_state());
	}
}

void
ExportProfileManager::serialize_local_profile (XMLNode & root)
{
	for (TimespanStateList::iterator it = timespans.begin(); it != timespans.end(); ++it) {
		root.add_child_nocopy (serialize_timespan (*it));
	}
	
	for (ChannelConfigStateList::iterator it = channel_configs.begin(); it != channel_configs.end(); ++it) {
		root.add_child_nocopy ((*it)->config->get_state());
	}
}

std::vector<sys::path>
ExportProfileManager::find_file (std::string const & pattern)
{
	vector<sys::path> found;

	Glib::PatternSpec pattern_spec (pattern);
	find_matching_files_in_search_path (search_path, pattern_spec, found);

	return found;
}

void
ExportProfileManager::set_selection_range (nframes_t start, nframes_t end)
{

	if (start || end) {
		selection_range.reset (new Location());
		selection_range->set_name (_("Selection"));
		selection_range->set (start, end);
	} else {
		selection_range.reset();
	}
	
	for (TimespanStateList::iterator it = timespans.begin(); it != timespans.end(); ++it) {
		(*it)->selection_range = selection_range;
	}
}

std::string
ExportProfileManager::set_single_range (nframes_t start, nframes_t end, Glib::ustring name)
{
	single_range_mode = true;
	
	single_range.reset (new Location());
	single_range->set_name (name);
	single_range->set (start, end);
	
	update_ranges ();
	
	return single_range->id().to_s();
}

bool
ExportProfileManager::init_timespans (XMLNodeList nodes)
{
	timespans.clear ();
	update_ranges ();
	
	bool ok = true;
	for (XMLNodeList::const_iterator it = nodes.begin(); it != nodes.end(); ++it) {
		TimespanStatePtr span = deserialize_timespan (**it);
		if (span) {
			timespans.push_back (span);
		} else { ok = false; }
	}
	
	if (timespans.empty()) {
		TimespanStatePtr timespan (new TimespanState (session_range, selection_range, ranges));
		timespans.push_back (timespan);
		return false;
	}
	
	return ok;
}

ExportProfileManager::TimespanStatePtr
ExportProfileManager::deserialize_timespan (XMLNode & root)
{
	TimespanStatePtr state (new TimespanState (session_range, selection_range, ranges));
	XMLProperty const * prop;
	
	XMLNodeList spans = root.children ("Range");
	for (XMLNodeList::iterator node_it = spans.begin(); node_it != spans.end(); ++node_it) {
		
		prop = (*node_it)->property ("id");
		if (!prop) { continue; }
		ustring id = prop->value();
	
		for (LocationList::iterator it = ranges->begin(); it != ranges->end(); ++it) {
			if ((!id.compare ("session") && *it == session_range.get()) ||
			    (!id.compare ("selection") && *it == selection_range.get()) ||
			    (!id.compare ((*it)->id().to_s()))) {
				TimespanPtr timespan = handler->add_timespan();
				timespan->set_name ((*it)->name());
				timespan->set_range_id (id);
				timespan->set_range ((*it)->start(), (*it)->end());
				state->timespans->push_back (timespan);
			}
		}
	}
	
	if ((prop = root.property ("format"))) {
		state->time_format = (TimeFormat) string_2_enum (prop->value(), TimeFormat);
	}
	
	return state;
}

XMLNode &
ExportProfileManager::serialize_timespan (TimespanStatePtr state)
{
	XMLNode & root = *(new XMLNode ("ExportTimespan"));
	XMLNode * span;
	
	update_ranges ();
	
	for (TimespanList::iterator it = state->timespans->begin(); it != state->timespans->end(); ++it) {
		if ((span = root.add_child ("Range"))) {
			span->add_property ("id", (*it)->range_id());
		}
	}
	
	root.add_property ("format", enum_2_string (state->time_format));
	
	return root;
}

void
ExportProfileManager::update_ranges () {
	ranges->clear();
	
	if (single_range_mode) {
		ranges->push_back (single_range.get());
		return;
	}

	/* Session */

	session_range->set_name (_("Session"));
	session_range->set (session.current_start_frame(), session.current_end_frame());
	ranges->push_back (session_range.get());
	
	/* Selection */
	
	if (selection_range) {
		ranges->push_back (selection_range.get());
	}
	
	/* ranges */
	
	LocationList const & list (session.locations()->list());
	for (LocationList::const_iterator it = list.begin(); it != list.end(); ++it) {
		if ((*it)->is_range_marker()) {
			ranges->push_back (*it);
		}
	}
}

bool
ExportProfileManager::init_channel_configs (XMLNodeList nodes)
{
	channel_configs.clear();

	if (nodes.empty()) {	
		ChannelConfigStatePtr config (new ChannelConfigState (handler->add_channel_config()));
		channel_configs.push_back (config);
		return false;
	}
	
	for (XMLNodeList::const_iterator it = nodes.begin(); it != nodes.end(); ++it) {
		ChannelConfigStatePtr config (new ChannelConfigState (handler->add_channel_config()));
		config->config->set_state (**it);
		channel_configs.push_back (config);
	}
	
	return true;
}

ExportProfileManager::FormatStatePtr
ExportProfileManager::duplicate_format_state (FormatStatePtr state)
{
	/* Note: The pointer in the new FormatState should point to the same format spec
	         as the original state's pointer. The spec itself should not be copied!   */

	FormatStatePtr format (new FormatState (format_list, state->format));
	formats.push_back (format);
	return format;
}

void
ExportProfileManager::remove_format_state (FormatStatePtr state)
{
	for (FormatStateList::iterator it = formats.begin(); it != formats.end(); ++it) {
		if (*it == state) {
			formats.erase (it);
			return;
		}
	}
}

sys::path
ExportProfileManager::save_format_to_disk (FormatPtr format)
{
	// TODO filename character stripping

	/* Get filename for file */

	Glib::ustring new_name = format->name();
	new_name += export_format_suffix;
	
	sys::path new_path (export_config_dir);
	new_path /= new_name;

	/* Check if format is on disk already */
	FileMap::iterator it;
	if ((it = format_file_map.find (format->id())) != format_file_map.end()) {
		/* Update data */
		{
			XMLTree tree (it->second.to_string());
			tree.set_root (&format->get_state());
			tree.write();
		}
		
		/* Rename if necessary */
		
		if (new_name.compare (it->second.leaf())) {
			sys::rename (it->second, new_path);
		}
		
	} else {
		/* Write new file */
		
		XMLTree tree (new_path.to_string());
		tree.set_root (&format->get_state());
		tree.write();
	}
	
	FormatListChanged ();
	return new_path;
}

void
ExportProfileManager::remove_format_profile (FormatPtr format)
{
	for (FormatList::iterator it = format_list->begin(); it != format_list->end(); ++it) {
		if (*it == format) {
			format_list->erase (it);
			break;
		}
	}

	FileMap::iterator it = format_file_map.find (format->id());
	if (it != format_file_map.end()) {
		sys::remove (it->second);
		format_file_map.erase (it);
	}
	
	FormatListChanged ();
}

ExportProfileManager::FormatPtr
ExportProfileManager::get_new_format (FormatPtr original)
{	
	FormatPtr format;
	if (original) {
		format.reset (new ExportFormatSpecification (*original));
	} else {
		format = handler->add_format();
		format->set_name ("empty format");
	}
	
	sys::path path = save_format_to_disk (format);
	FilePair pair (format->id(), path);
	format_file_map.insert (pair);
	
	format_list->push_back (format);
	FormatListChanged ();
	
	return format;
}

bool
ExportProfileManager::init_formats (XMLNodeList nodes)
{
	formats.clear();

	bool ok = true;
	for (XMLNodeList::const_iterator it = nodes.begin(); it != nodes.end(); ++it) {
		FormatStatePtr format = deserialize_format (**it);
		if (format) {
			formats.push_back (format);
		} else { ok = false; }
	}
	
	if (formats.empty ()) {
		FormatStatePtr format (new FormatState (format_list, FormatPtr ()));
		formats.push_back (format);
		return false;
	}
	
	return ok;
}

ExportProfileManager::FormatStatePtr
ExportProfileManager::deserialize_format (XMLNode & root)
{
	XMLProperty * prop;
	UUID id;
	
	if ((prop = root.property ("id"))) {
		id = prop->value();
	}
	
	for (FormatList::iterator it = format_list->begin(); it != format_list->end(); ++it) {
		if ((*it)->id() == id) {
			return FormatStatePtr (new FormatState (format_list, *it));
		}
	}

	return FormatStatePtr ();
}

XMLNode &
ExportProfileManager::serialize_format (FormatStatePtr state)
{
	XMLNode * root = new XMLNode ("ExportFormat");
	
	string id = state->format ? state->format->id().to_s() : "";
	root->add_property ("id", id);
	
	return *root;
}

void
ExportProfileManager::load_formats ()
{
	vector<sys::path> found = find_file (string_compose ("*%1", export_format_suffix));

	for (vector<sys::path>::iterator it = found.begin(); it != found.end(); ++it) {
		load_format_from_disk (*it);
	}
}

void
ExportProfileManager::load_format_from_disk (PBD::sys::path const & path)
{
	XMLTree const tree (path.to_string());
	FormatPtr format = handler->add_format (*tree.root());
	format_list->push_back (format);
	
	/* Handle id to filename mapping */
	
	FilePair pair (format->id(), path);
	format_file_map.insert (pair);
	
	FormatListChanged ();
}

ExportProfileManager::FilenameStatePtr
ExportProfileManager::duplicate_filename_state (FilenameStatePtr state)
{
	FilenameStatePtr filename (new FilenameState (handler->add_filename_copy (state->filename)));
	filenames.push_back (filename);
	return filename;
}

void
ExportProfileManager::remove_filename_state (FilenameStatePtr state)
{
	for (FilenameStateList::iterator it = filenames.begin(); it != filenames.end(); ++it) {
		if (*it == state) {
			filenames.erase (it);
			return;
		}
	}
}

bool
ExportProfileManager::init_filenames (XMLNodeList nodes)
{
	filenames.clear ();

	for (XMLNodeList::const_iterator it = nodes.begin(); it != nodes.end(); ++it) {
		FilenamePtr filename = handler->add_filename();
		filename->set_state (**it);
		filenames.push_back (FilenameStatePtr (new FilenameState (filename)));
	}
	
	if (filenames.empty()) {
		FilenameStatePtr filename (new FilenameState (handler->add_filename()));
		filenames.push_back (filename);
		return false;
	}
	
	return true;
}

boost::shared_ptr<ExportProfileManager::Warnings>
ExportProfileManager::get_warnings ()
{
	boost::shared_ptr<Warnings> warnings (new Warnings ());

	ChannelConfigStatePtr channel_config_state = channel_configs.front();
	TimespanStatePtr timespan_state = timespans.front();
	
	/*** Check "global" config ***/
	
	TimespanListPtr timespans = timespan_state->timespans;
	ChannelConfigPtr channel_config = channel_config_state->config;

	/* Check Timespans are not empty */
	
	if (timespans->empty()) {
		warnings->errors.push_back (_("No timespan has been selected!"));
	}

	/* Check channel config ports */
	
	if (!channel_config->all_channels_have_ports ()) {
		warnings->warnings.push_back (_("Some channels are empty"));
	}
	
	/*** Check files ***/
	
	FormatStateList::const_iterator format_it;
	FilenameStateList::const_iterator filename_it;
	for (format_it = formats.begin(), filename_it = filenames.begin();
	     format_it != formats.end() && filename_it != filenames.end();
	     ++format_it, ++filename_it) {
			check_config (warnings, timespan_state, channel_config_state, *format_it, *filename_it);
	}
	
	return warnings;
}

void
ExportProfileManager::check_config (boost::shared_ptr<Warnings> warnings,
	                            TimespanStatePtr timespan_state,
	                            ChannelConfigStatePtr channel_config_state,
	                            FormatStatePtr format_state,
	                            FilenameStatePtr filename_state)
{
	TimespanListPtr timespans = timespan_state->timespans;
	ChannelConfigPtr channel_config = channel_config_state->config;
	FormatPtr format = format_state->format;
	FilenamePtr filename = filename_state->filename;

	/* Check format and maximum channel count */
	if (!format || !format->type()) {
		warnings->errors.push_back (_("No format selected!"));
	} else if (!ExportFileFactory::check (format, channel_config->get_n_chans())) {
		warnings->errors.push_back (_("One or more of the selected formats is not compatible with this system!"));
	} else if (format->channel_limit() < channel_config->get_n_chans()) {
		warnings->errors.push_back
		  (string_compose (_("%1 supports only %2 channels, but you have %3 channels in your channel configuration"),
		                     format->format_name(),
		                     format->channel_limit(),
		                     channel_config->get_n_chans()));
	}
	
	if (!warnings->errors.empty()) { return; }
	
	/* Check filenames */
	
//	filename->include_timespan = (timespans->size() > 1); Disabled for now...
	
	for (std::list<TimespanPtr>::iterator timespan_it = timespans->begin(); timespan_it != timespans->end(); ++timespan_it) {
		filename->set_timespan (*timespan_it);
	
		if (channel_config->get_split()) {
			filename->include_channel = true;
			
			for (uint32_t chan = 1; chan <= channel_config->get_n_chans(); ++chan) {
				filename->set_channel (chan);
				
				Glib::ustring path = filename->get_path (format);
				
				if (sys::exists (sys::path (path))) {
					warnings->conflicting_filenames.push_back (path);
				}
			}
			
		} else {
			Glib::ustring path = filename->get_path (format);
			
			if (sys::exists (sys::path (path))) {
				warnings->conflicting_filenames.push_back (path);
			}
		}
	}
}

}; // namespace ARDOUR
