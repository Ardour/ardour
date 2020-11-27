/*
 * Copyright (C) 2008 Sakari Bergen <sakari.bergen@beatwaves.net>
 * Copyright (C) 2009-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2016 Paul Davis <paul@linuxaudiosystems.com>
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

#include "ardour/tempo_map_importer.h"

#include <sstream>

#include "ardour/session.h"
#include "ardour/tempo.h"
#include "pbd/failed_constructor.h"

#include "pbd/i18n.h"

using namespace std;
using namespace PBD;
using namespace ARDOUR;
using namespace Temporal;

/**** Handler ***/
TempoMapImportHandler::TempoMapImportHandler (XMLTree const & source, Session & session) :
  ElementImportHandler (source, session)
{
	XMLNode const * root = source.root();
	XMLNode const * tempo_map;

	if (!(tempo_map = root->child (X_("TempoMap")))) {
		throw failed_constructor();
	}

	elements.push_back (ElementPtr ( new TempoMapImporter (source, session, *tempo_map)));
}

string
TempoMapImportHandler::get_info () const
{
	return _("Tempo map");
}

/*** TempoMapImporter ***/
TempoMapImporter::TempoMapImporter (XMLTree const & source, Session & session, XMLNode const & node) :
  ElementImporter (source, session),
  xml_tempo_map (node)
{
	name = _("Tempo Map");
}

string
TempoMapImporter::get_info () const
{
	std::ostringstream oss;
	unsigned int tempos = 0;
	unsigned int meters = 0;
	XMLNodeList children = xml_tempo_map.children();

	for (XMLNodeIterator it = children.begin(); it != children.end(); it++) {
		if ((*it)->name() == "Tempo") {
			tempos++;
		} else if ((*it)->name() == "Meters") {
			meters++;
		}
	}

	// return info
	oss << _("Tempo marks: ") << tempos << _("\nMeter marks: ") << meters;

	return oss.str();
}

bool
TempoMapImporter::_prepare_move ()
{
	// Prompt user for verification
	boost::optional<bool> replace = Prompt (_("This will replace the current tempo map!\nAre you sure you want to do this?"));
	return replace.value_or (false);
}

void
TempoMapImporter::_cancel_move ()
{
}

void
TempoMapImporter::_move ()
{
	TempoMap::SharedPtr tmap (TempoMap::write_copy());
	tmap->set_state (xml_tempo_map, Stateful::current_state_version);
	TempoMap::update (tmap);
}
