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

#include <ardour/audio_track_importer.h>

#include <pbd/id.h>
#include <pbd/failed_constructor.h>

#include "i18n.h"

namespace ARDOUR {

/*** AudioTrackImportHandler ***/

AudioTrackImportHandler::AudioTrackImportHandler (XMLTree const & source, Session & session) :
  ElementImportHandler (source, session)
{
	XMLNode const * root = source.root();
	XMLNode const * routes;
	
	if (!(routes = root->child ("Routes"))) {
		throw failed_constructor();
	}
	
	XMLNodeList const & route_list = routes->children();
	for (XMLNodeList::const_iterator it = route_list.begin(); it != route_list.end(); ++it) {
		const XMLProperty* type = (*it)->property("default-type");
		if ( !type || type->value() == "audio" ) {
			try {
				elements.push_back (ElementPtr ( new AudioTrackImporter (source, session, *this, **it)));
			} catch (failed_constructor err) {
				set_dirty();
			}
		}
	}
}

string
AudioTrackImportHandler::get_info () const
{
	return _("Audio Tracks");
}


/*** AudioTrackImporter ***/

AudioTrackImporter::AudioTrackImporter (XMLTree const & source, Session & session, AudioTrackImportHandler & handler, XMLNode const & node) :
  ElementImporter (source, session),
  xml_track ("Route")
{
	// TODO Parse top-level XML
	
	if (!parse_io (node)) {
		throw failed_constructor();
	}
	
	XMLNodeList const & controllables = node.children ("controllable");
	for (XMLNodeList::const_iterator it = controllables.begin(); it != controllables.end(); ++it) {
		parse_controllable (**it, xml_track);
	}
	
	// TODO parse remote-control and extra?
	
}

string
AudioTrackImporter::get_info () const
{
	// TODO
	return name;
}

bool
AudioTrackImporter::prepare_move ()
{
	// TODO
	return false;
}

void
AudioTrackImporter::cancel_move ()
{
	// TODO
}

void
AudioTrackImporter::move ()
{
	// TODO
}

bool
AudioTrackImporter::parse_io (XMLNode const & node)
{
	XMLNode * io;
	XMLProperty * prop;

	if (!(io = node.child ("IO"))) {
		return false;
	}
	
	if ((prop = io->property ("name"))) {
		name = prop->value();
	} else {
		return false;
	}
	
	// TODO parse rest of the XML

	return true;
}

bool
AudioTrackImporter::parse_controllable (XMLNode const & node, XMLNode & dest_parent)
{
	XMLProperty * prop;
	XMLNode new_node (node);
	
	if ((prop = new_node.property ("id"))) {
		PBD::ID old_id (prop->value());
		PBD::ID new_id;
		
		prop->set_value (new_id.to_s());
		// TODO do id mapping and everything else necessary...
		
	} else {
		return false;
	}
	
	dest_parent.add_child_copy (new_node);

	return true;
}

} // namespace ARDOUR
