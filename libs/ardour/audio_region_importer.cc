/*
 * Copyright (C) 2008-2009 Sakari Bergen <sakari.bergen@beatwaves.net>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2012 Tim Mayberry <mojofunk@gmail.com>
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

#include "ardour/audio_region_importer.h"

#include <sstream>

#include <glibmm/miscutils.h>

#include "pbd/failed_constructor.h"
#include "pbd/compose.h"
#include "pbd/error.h"

#include "ardour/session.h"
#include "ardour/region.h"
#include "ardour/region_factory.h"
#include "ardour/session_directory.h"

#include "pbd/i18n.h"

using namespace std;
using namespace PBD;
using namespace ARDOUR;

/**** Handler ***/
AudioRegionImportHandler::AudioRegionImportHandler (XMLTree const & source, Session & session) :
  ElementImportHandler (source, session)
{
	XMLNode const * root = source.root();
	XMLNode const * regions;

	if (!(regions = root->child (X_("Regions")))) {
		throw failed_constructor();
	}

	create_regions_from_children (*regions, elements);
}

void
AudioRegionImportHandler::create_regions_from_children (XMLNode const & node, ElementList & list)
{
	XMLNodeList const & children = node.children();
	for (XMLNodeList::const_iterator it = children.begin(); it != children.end(); ++it) {
		XMLProperty const * type = (*it)->property("type");
		if (!(*it)->name().compare ("Region") && (!type || type->value() == "audio") ) {
			try {
				list.push_back (ElementPtr ( new AudioRegionImporter (source, session, *this, **it)));
			} catch (failed_constructor const&) {
				set_dirty();
			}
		}
	}
}

string
AudioRegionImportHandler::get_info () const
{
	return _("Audio Regions");
}

bool
AudioRegionImportHandler::check_source (string const & filename) const
{
	return (sources.find (filename) != sources.end());
}

void
AudioRegionImportHandler::add_source (string const & filename, boost::shared_ptr<Source> const & source)
{
	sources.insert (SourcePair (filename, source));
}

boost::shared_ptr<Source> const &
AudioRegionImportHandler::get_source (string const & filename) const
{
	return (sources.find (filename))->second;
}

void
AudioRegionImportHandler::register_id (PBD::ID & old_id, PBD::ID & new_id)
{
	id_map.insert (IdPair (old_id, new_id));
}

PBD::ID const &
AudioRegionImportHandler::get_new_id (PBD::ID & old_id) const
{
	return (id_map.find (old_id))->second;
}

/*** AudioRegionImporter ***/
AudioRegionImporter::AudioRegionImporter (XMLTree const & source, Session & session, AudioRegionImportHandler & handler, XMLNode const & node) :
  ElementImporter (source, session),
  xml_region (node),
  handler (handler),
  old_id ("0"),
  region_prepared (false),
  sources_prepared (false)
{
	if (!parse_xml_region () || !parse_source_xml ()) {
		throw failed_constructor();
	}
	handler.register_id (old_id, id);
}

AudioRegionImporter::~AudioRegionImporter ()
{
}

string
AudioRegionImporter::get_info () const
{
	samplecnt_t length, position;
	Timecode::Time length_time, position_time;
	std::ostringstream oss;

	// Get sample positions
	std::istringstream iss_length(xml_region.property ("length")->value());
	iss_length >> length;
	std::istringstream iss_position(xml_region.property ("position")->value());
	iss_position >> position;

	// Convert to timecode
	session.sample_to_timecode(length, length_time, true, false);
	session.sample_to_timecode(position, position_time, true, false);

	// return info
	oss << _("Length: ") <<
	  timecode_to_string(length_time) <<
	  _("\nPosition: ") <<
	  timecode_to_string(position_time) <<
	  _("\nChannels: ") <<
	  xml_region.property ("channels")->value();


	return oss.str();
}

bool
AudioRegionImporter::_prepare_move ()
{
	return true;
}

void
AudioRegionImporter::_cancel_move ()
{
}

void
AudioRegionImporter::_move ()
{
	if (!region_prepared) {
		prepare_region();
		if (!region_prepared) {
			return;
		}
	}

	if (broken()) {
		return;
	}
}

bool
AudioRegionImporter::parse_xml_region ()
{
	XMLPropertyList const & props = xml_region.properties();
	bool id_ok = false;
	bool name_ok = false;

	for (XMLPropertyList::const_iterator it = props.begin(); it != props.end(); ++it) {
		string prop = (*it)->name();
		if (!prop.compare ("type") || !prop.compare ("stretch") ||
		  !prop.compare ("shift") || !prop.compare ("first_edit") ||
		  !prop.compare ("layer") || !prop.compare ("flags") ||
		  !prop.compare ("scale-gain") || !prop.compare("channels") ||
		  !prop.compare ("first-edit") ||
		  prop.find ("master-source-") == 0 || prop.find ("source-") == 0) {
			// All ok
		} else if (!prop.compare ("start") || !prop.compare ("length") ||
		  !prop.compare ("position") || !prop.compare ("ancestral-start") ||
		  !prop.compare ("ancestral-length") || !prop.compare ("sync-position")) {
			// Sample rate conversion
			(*it)->set_value (rate_convert_samples ((*it)->value()));
		} else if (!prop.compare("id")) {
			// get old id and update id
			old_id = (*it)->value();
			(*it)->set_value (id.to_s());
			id_ok = true;
		} else if (!prop.compare("name")) {
			// rename region if necessary
			name = (*it)->value();
			name = RegionFactory::new_region_name (name);
			(*it)->set_value (name);
			name_ok = true;
		} else {
			std::cerr << string_compose (X_("AudioRegionImporter (%1): did not recognise XML-property \"%2\""), name, prop) << endmsg;
		}
	}

	if (!id_ok) {
		error << string_compose (X_("AudioRegionImporter (%1): did not find necessary XML-property \"id\""), name) << endmsg;
		return false;
	}

	if (!name_ok) {
		error << X_("AudioRegionImporter: did not find necessary XML-property \"name\"") << endmsg;
		return false;
	}

	return true;
}

bool
AudioRegionImporter::parse_source_xml ()
{
	uint32_t channels;
	char buf[128];
	std::string source_dir(get_sound_dir (source));
	XMLNode * source_node;
	XMLProperty const * prop;

	// Get XML for sources
	if (!(source_node = source.root()->child (X_("Sources")))) {
		return false;
	}
	XMLNodeList const & sources = source_node->children();

	// Get source for each channel
	if (!(prop = xml_region.property ("channels"))) {
		error << string_compose (X_("AudioRegionImporter (%1): did not find necessary XML-property \"channels\""), name) << endmsg;
		return false;
	}

	channels = atoi (prop->value().c_str());
	for (uint32_t i = 0; i < channels; ++i) {
		bool source_found = false;

		// Get id for source-n
		snprintf (buf, sizeof(buf), X_("source-%d"), i);
		prop = xml_region.property (buf);
		if (!prop) {
			error << string_compose (X_("AudioRegionImporter (%1): did not find necessary XML-property \"%2\""), name, buf) << endmsg;
			return false;
		}
		string source_id = prop->value();

		// Get source
		for (XMLNodeList::const_iterator it = sources.begin(); it != sources.end(); ++it) {
			prop = (*it)->property ("id");
			if (prop && !source_id.compare (prop->value())) {
				prop = (*it)->property ("name");
				if (!prop) {
					error << string_compose (X_("AudioRegionImporter (%1): source %2 has no \"name\" property"), name, source_id) << endmsg;
					return false;
				}
				filenames.push_back (Glib::build_filename (source_dir, prop->value()));
				source_found = true;
				break;
			}
		}

		if (!source_found) {
			error << string_compose (X_("AudioRegionImporter (%1): could not find all necessary sources"), name) << endmsg;
			return false;
		}
	}

	return true;
}

std::string
AudioRegionImporter::get_sound_dir (XMLTree const & tree)
{
	SessionDirectory session_dir(Glib::path_get_dirname (tree.filename()));
	return session_dir.sound_path();
}

void
AudioRegionImporter::prepare_region ()
{
	if (region_prepared) {
		return;
	}

	SourceList source_list;
	prepare_sources();

	// Create source list
	for (std::list<string>::iterator it = filenames.begin(); it != filenames.end(); ++it) {
		source_list.push_back (handler.get_source (*it));
	}

	// create region and update XML
	boost::shared_ptr<Region> r = RegionFactory::create (source_list, xml_region);
	if (session.config.get_glue_new_regions_to_bars_and_beats ()) {
		r->set_position_time_domain (Temporal::BeatTime);
	}
	region.push_back (r);
	if (*region.begin()) {
		xml_region = (*region.begin())->get_state();
	} else {
		error << string_compose (X_("AudioRegionImporter (%1): could not construct Region"), name) << endmsg;
		handler.set_errors();
	}

	region_prepared = true;
}

void
AudioRegionImporter::prepare_sources ()
{
	if (sources_prepared) {
		return;
	}

	status.total = 0;
	status.replace_existing_source = false;
	status.done = false;
	status.cancel = false;
	status.freeze = false;
	status.progress = 0.0;
	status.quality = SrcBest; // TODO other qualities also

	// Get sources that still need to be imported
	for (std::list<string>::iterator it = filenames.begin(); it != filenames.end(); ++it) {
		if (!handler.check_source (*it)) {
			status.paths.push_back (*it);
			status.total++;
		}
	}

	// import files
	// TODO: threading & exception handling
	session.import_files (status);

	// Add imported sources to handlers map
	std::vector<string>::iterator file_it = status.paths.begin();
	for (SourceList::iterator source_it = status.sources.begin(); source_it != status.sources.end(); ++source_it) {
		if (*source_it) {
			handler.add_source(*file_it, *source_it);
		} else {
			error << string_compose (X_("AudioRegionImporter (%1): could not import all necessary sources"), name) << endmsg;
			handler.set_errors();
			set_broken();
		}

		++file_it;
	}

	sources_prepared = true;
}

void
AudioRegionImporter::add_sources_to_session ()
{
	if (!sources_prepared) {
		prepare_sources();
	}

	if (broken()) {
		return;
	}

	for (std::list<string>::iterator it = filenames.begin(); it != filenames.end(); ++it) {
		session.add_source (handler.get_source (*it));
	}
}

XMLNode const &
AudioRegionImporter::get_xml ()
{
	if(!region_prepared) {
		prepare_region();
	}

	return xml_region;
}
