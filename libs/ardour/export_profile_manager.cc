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

#include <ardour/audioengine.h>
#include <ardour/export_failed.h>
#include <ardour/export_format_specification.h>
#include <ardour/export_timespan.h>
#include <ardour/export_channel_configuration.h>
#include <ardour/export_filename.h>
#include <ardour/export_handler.h>
#include <ardour/session.h>

#include "i18n.h"

using namespace PBD;

namespace ARDOUR
{

ExportProfileManager::Preset::Preset (string filename, Session & s) :
  _id (0), session (s), global (filename), local (0)
{
	XMLNode * root;
	if ((root = global.root())) {
		XMLProperty * prop;
		if ((prop = root->property ("id"))) {
			set_id ((uint32_t) atoi (prop->value()));
		}
		if ((prop = root->property ("name"))) {
			set_name (prop->value());
		}
		
		XMLNode * instant_xml = get_instant_xml ();
		if (instant_xml) {
			XMLNode * instant_copy = new XMLNode (*instant_xml);
			set_local_state (*instant_copy);
		}
	}
}

ExportProfileManager::Preset::~Preset ()
{
	if (local) {
		delete local;
	}
}

void
ExportProfileManager::Preset::set_name (string name)
{
	_name = name;

	XMLNode * node;	
	if ((node = global.root())) {
		node->add_property ("name", name);
	}
	if (local) {
		local->add_property ("name", name);
	}
}

void
ExportProfileManager::Preset::set_id (uint32_t id)
{
	_id = id;

	XMLNode * node;
	if ((node = global.root())) {
		node->add_property ("id", id);
	}
	if (local) {
		local->add_property ("id", id);
	}
}

void
ExportProfileManager::Preset::set_global_state (XMLNode & state)
{
	delete global.root ();
	global.set_root (&state);
	
	set_id (_id);
	set_name (_name);
}

void
ExportProfileManager::Preset::set_local_state (XMLNode & state)
{
	delete local;
	local = &state;
	
	set_id (_id);
	set_name (_name);
}

void
ExportProfileManager::Preset::save () const
{
	save_instant_xml ();
	if (global.root()) {
		global.write ();
	}
}

void
ExportProfileManager::Preset::remove_local () const
{
	remove_instant_xml ();
}

XMLNode *
ExportProfileManager::Preset::get_instant_xml () const
{
	XMLNode * instant_xml;
	
	if ((instant_xml = session.instant_xml ("ExportPresets"))) {
		XMLNodeList children = instant_xml->children ("ExportPreset");
		for (XMLNodeList::iterator it = children.begin(); it != children.end(); ++it) {
			XMLProperty * prop;
			if ((prop = (*it)->property ("id")) && _id == (uint32_t) atoi (prop->value())) {
				return *it;
			}
		}
	}
	
	return 0;
}

void
ExportProfileManager::Preset::save_instant_xml () const
{
	if (!local) { return; }

	/* First remove old, then add new */
	
	remove_instant_xml ();
	
	XMLNode * instant_xml;
	if ((instant_xml = session.instant_xml ("ExportPresets"))) {
		instant_xml->add_child_copy (*local);
	} else {
		instant_xml = new XMLNode ("ExportPresets");
		instant_xml->add_child_copy (*local);
		session.add_instant_xml (*instant_xml, false);
	}
}

void
ExportProfileManager::Preset::remove_instant_xml () const
{
	XMLNode * instant_xml;
	if ((instant_xml = session.instant_xml ("ExportPresets"))) {
		instant_xml->remove_nodes_and_delete ("id", to_string (_id, std::dec));
	}
}

ExportProfileManager::ExportProfileManager (Session & s) :
  handler (s.get_export_handler()),
  session (s),

  session_range (new Location ()),
  ranges (new LocationList ()),

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
}

ExportProfileManager::~ExportProfileManager ()
{
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

void
ExportProfileManager::load_preset (PresetPtr preset)
{
	current_preset = preset;
	if (!preset) { return; }

	XMLNode const * state;
	if ((state = preset->get_local_state())) {
		set_local_state (*state);
	}
	
	if ((state = preset->get_global_state())) {
		set_global_state (*state);
	}
}

void
ExportProfileManager::load_presets ()
{
	preset_id_counter = 0;
	
	vector<sys::path> found = find_file ("*.preset");

	for (vector<sys::path>::iterator it = found.begin(); it != found.end(); ++it) {
		preset_id_counter = std::max (preset_id_counter, load_preset_from_disk (*it));
	}
}

ExportProfileManager::PresetPtr
ExportProfileManager::save_preset (string const & name)
{
	if (!current_preset) {
		++preset_id_counter;
		string filename = export_config_dir.to_string() + "/" + to_string (preset_id_counter, std::dec) + ".preset";
		current_preset.reset (new Preset (filename, session));
		preset_list.push_back (current_preset);
		current_preset->set_id (preset_id_counter);
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

uint32_t
ExportProfileManager::load_preset_from_disk (PBD::sys::path const & path)
{
	PresetPtr preset (new Preset (path.to_string(), session));
	preset_list.push_back (preset);
	
	/* Handle id to filename mapping */
	
	FilePair pair (preset->id(), path);
	preset_file_map.insert (pair);
	
	return preset->id();
}

void
ExportProfileManager::set_state (XMLNode const & root)
{
	set_global_state (root);
	set_local_state (root);
}

void
ExportProfileManager::set_global_state (XMLNode const & root)
{
	init_formats (root.children ("ExportFormat"));
	init_filenames (root.children ("ExportFilename"));
}

void
ExportProfileManager::set_local_state (XMLNode const & root)
{
	init_timespans (root.children ("ExportTimespan"));;
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
		root.add_child_nocopy (serialize_filename (*it));
	}
}

void
ExportProfileManager::serialize_local_profile (XMLNode & root)
{
	for (TimespanStateList::iterator it = timespans.begin(); it != timespans.end(); ++it) {
		root.add_child_nocopy (serialize_timespan (*it));
	}
	
	for (ChannelConfigStateList::iterator it = channel_configs.begin(); it != channel_configs.end(); ++it) {
		root.add_child_nocopy (serialize_channel_config (*it));
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

void
ExportProfileManager::init_timespans (XMLNodeList nodes)
{
	timespans.clear ();

	if (nodes.empty()) {
		update_ranges ();
	
		TimespanStatePtr timespan (new TimespanState (session_range, selection_range, ranges));
	
		timespans.push_back (timespan);
		return;
	}

	for (XMLNodeList::const_iterator it = nodes.begin(); it != nodes.end(); ++it) {
		timespans.push_back (deserialize_timespan (**it));
	}
}

ExportProfileManager::TimespanStatePtr
ExportProfileManager::deserialize_timespan (XMLNode & root)
{
	TimespanStatePtr state (new TimespanState (session_range, selection_range, ranges));
	XMLProperty const * prop;
	
	update_ranges ();
	
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

void
ExportProfileManager::init_channel_configs (XMLNodeList nodes)
{
	channel_configs.clear();

	if (nodes.empty()) {	
		ChannelConfigStatePtr config (new ChannelConfigState (handler->add_channel_config()));
		channel_configs.push_back (config);
		return;
	}
	
	for (XMLNodeList::const_iterator it = nodes.begin(); it != nodes.end(); ++it) {
		channel_configs.push_back (deserialize_channel_config (**it));
	}
}

ExportProfileManager::ChannelConfigStatePtr
ExportProfileManager::deserialize_channel_config (XMLNode & root)
{

	ChannelConfigStatePtr state (new ChannelConfigState (handler->add_channel_config()));
	XMLProperty const * prop;
	
	if ((prop = root.property ("split"))) {
		state->config->set_split(!prop->value().compare ("true"));
	}

	XMLNodeList channels = root.children ("Channel");
	for (XMLNodeList::iterator it = channels.begin(); it != channels.end(); ++it) {
		boost::shared_ptr<ExportChannel> channel (new ExportChannel ());
	
		XMLNodeList ports = (*it)->children ("Port");
		for (XMLNodeList::iterator p_it = ports.begin(); p_it != ports.end(); ++p_it) {
			if ((prop = (*p_it)->property ("name"))) {
				channel->add_port (dynamic_cast<AudioPort *> (session.engine().get_port_by_name (prop->value())));
			}
		}
	
		state->config->register_channel (channel);
	}

	return state;
}

XMLNode &
ExportProfileManager::serialize_channel_config (ChannelConfigStatePtr state)
{
	XMLNode * root = new XMLNode ("ExportChannelConfiguration");
	XMLNode * channel;
	XMLNode * port_node;
	
	root->add_property ("split", state->config->get_split() ? "true" : "false");
	root->add_property ("channels", to_string (state->config->get_n_chans(), std::dec));
	
	uint32_t i = 1;
	ExportChannelConfiguration::ChannelList const & chan_list = state->config->get_channels();
	for (ExportChannelConfiguration::ChannelList::const_iterator c_it = chan_list.begin(); c_it != chan_list.end(); ++c_it) {
		channel = root->add_child ("Channel");
		if (!channel) { continue; }
		
		channel->add_property ("number", to_string (i, std::dec));
		
		for (ExportChannel::const_iterator p_it = (*c_it)->begin(); p_it != (*c_it)->end(); ++p_it) {
			if ((port_node = channel->add_child ("Port"))) {
				port_node->add_property ("name", (*p_it)->name());
			}
		}
		
		++i;
	}
	
	return *root;
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
	new_name += ".format";
	
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

void
ExportProfileManager::init_formats (XMLNodeList nodes)
{
	formats.clear();

	if (nodes.empty()) {
		FormatStatePtr format (new FormatState (format_list, FormatPtr ()));
		formats.push_back (format);
		return;
	}
	
	for (XMLNodeList::const_iterator it = nodes.begin(); it != nodes.end(); ++it) {
		FormatStatePtr format;
		if ((format = deserialize_format (**it))) {
			formats.push_back (format);
		}
	}
	
	if (formats.empty ()) {
		FormatStatePtr format (new FormatState (format_list, FormatPtr ()));
		formats.push_back (format);
	}
}

ExportProfileManager::FormatStatePtr
ExportProfileManager::deserialize_format (XMLNode & root)
{
	XMLProperty * prop;
	uint32_t id = 0;
	
	if ((prop = root.property ("id"))) {
		id = atoi (prop->value());
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
	
	string id = state->format ? to_string (state->format->id(), std::dec) : "0";
	root->add_property ("id", id);
	
	return *root;
}

void
ExportProfileManager::load_formats ()
{
	vector<sys::path> found = find_file ("*.format");

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

void
ExportProfileManager::init_filenames (XMLNodeList nodes)
{
	filenames.clear ();

	if (nodes.empty()) {
		FilenameStatePtr filename (new FilenameState (handler->add_filename()));
		filenames.push_back (filename);
		return;
	}
	
	for (XMLNodeList::const_iterator it = nodes.begin(); it != nodes.end(); ++it) {
		filenames.push_back (deserialize_filename (**it));
	}
}

ExportProfileManager::FilenameStatePtr
ExportProfileManager::deserialize_filename (XMLNode & root)
{
	FilenamePtr filename = handler->add_filename();
	filename->set_state (root);
	return FilenameStatePtr (new FilenameState (filename));
}

XMLNode &
ExportProfileManager::serialize_filename (FilenameStatePtr state)
{
	return state->filename->get_state();
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
