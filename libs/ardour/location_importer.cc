/*
 * Copyright (C) 2008-2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2008 Sakari Bergen <sakari.bergen@beatwaves.net>
 * Copyright (C) 2009-2010 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2017 Paul Davis <paul@linuxaudiosystems.com>
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

#include "ardour/location_importer.h"

#include <sstream>
#include <stdexcept>

#include "ardour/session.h"
#include "pbd/convert.h"
#include "pbd/failed_constructor.h"

#include "pbd/i18n.h"

using namespace std;
using namespace PBD;
using namespace ARDOUR;

/**** Handler ***/
LocationImportHandler::LocationImportHandler (XMLTree const & source, Session & session) :
  ElementImportHandler (source, session)
{
	XMLNode const *root = source.root();
	XMLNode const * location_node;

	if (!(location_node = root->child ("Locations"))) {
		throw failed_constructor();
	}

	// Construct importable locations
	XMLNodeList const & locations = location_node->children();
	for (XMLNodeList::const_iterator it = locations.begin(); it != locations.end(); ++it) {
		try {
			elements.push_back (ElementPtr ( new LocationImporter (source, session, *this, **it)));
		} catch (failed_constructor const&) {
			_dirty = true;
		}
	}
}

string
LocationImportHandler::get_info () const
{
	return _("Locations");
}

/*** LocationImporter ***/
LocationImporter::LocationImporter (XMLTree const & source, Session & session, LocationImportHandler & handler, XMLNode const & node) :
  ElementImporter (source, session),
  handler (handler),
  xml_location (node),
  location (0)
  {
	// Parse XML
	bool name_ok = false;
	XMLPropertyList props = xml_location.properties();

	for (XMLPropertyIterator it = props.begin(); it != props.end(); ++it) {
		string prop = (*it)->name();
		if (!prop.compare ("id") || !prop.compare ("flags") || !prop.compare ("locked")) {
			// All ok
		} else if (!prop.compare ("start") || !prop.compare ("end")) {
			// Sample rate conversion
			(*it)->set_value (rate_convert_samples ((*it)->value()));
		} else if (!prop.compare ("name")) {
			// rename region if necessary
			name = (*it)->value();
			name_ok = true;
		} else {
			std::cerr << string_compose (X_("LocationImporter did not recognise XML-property \"%1\""), prop) << endmsg;
		}
	}

	if (!name_ok) {
		error << X_("LocationImporter did not find necessary XML-property \"name\"") << endmsg;
		throw failed_constructor();
	}
}

LocationImporter::~LocationImporter ()
{
	if (!queued()) {
		delete location;
	}
}

string
LocationImporter::get_info () const
{
	samplepos_t start, end;
	Timecode::Time start_time, end_time;

	// Get sample positions
	std::istringstream iss_start (xml_location.property ("start")->value());
	iss_start >> start;
	std::istringstream iss_end (xml_location.property ("end")->value());
	iss_end >> end;

	// Convert to timecode
	session.sample_to_timecode (start, start_time, true, false);
	session.sample_to_timecode (end, end_time, true, false);

	// return info
	std::ostringstream oss;
	if (start == end) {
		oss << _("Location: ") << timecode_to_string (start_time);
	} else {
		oss << _("Range\nstart: ") << timecode_to_string (start_time) <<
		  _("\nend: ") << timecode_to_string (end_time);
	}

	return oss.str();
}

bool
LocationImporter::_prepare_move ()
{
	try {
		Location const original (session, xml_location);
		location = new Location (original); // Updates id
	} catch (failed_constructor& err) {
		throw std::runtime_error (X_("Error in session file!"));
		return false;
	}

	std::pair<bool, string> rename_pair;

	if (location->is_auto_punch()) {
		rename_pair = *Rename (_("The location is the Punch range. It will be imported as a normal range.\nYou may rename the imported location:"), name);
		if (!rename_pair.first) {
			return false;
		}

		name = rename_pair.second;
		location->set_auto_punch (false, this);
		location->set_is_range_marker (true, this);
	}

	if (location->is_auto_loop()) {
		rename_pair = *Rename (_("The location is a Loop range. It will be imported as a normal range.\nYou may rename the imported location:"), name);
		if (!rename_pair.first) { return false; }

		location->set_auto_loop (false, this);
		location->set_is_range_marker (true, this);
	}

	// duplicate name checking
	Locations::LocationList const & locations(session.locations()->list());
	for (Locations::LocationList::const_iterator it = locations.begin(); it != locations.end(); ++it) {
		if (!((*it)->name().compare (location->name())) || !handler.check_name (location->name())) {
			rename_pair = *Rename (_("A location with that name already exists.\nYou may rename the imported location:"), name);
			if (!rename_pair.first) { return false; }
			name = rename_pair.second;
		}
	}

	location->set_name (name);

	return true;
}

void
LocationImporter::_cancel_move ()
{
	delete location;
	location = 0;
}

void
LocationImporter::_move ()
{
	session.locations()->add (location);
}
