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

#include <ardour/session.h>

#include <pbd/id.h>
#include <pbd/failed_constructor.h>
#include <pbd/convert.h>

#include "i18n.h"

using namespace PBD;
using namespace ARDOUR;

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
  xml_track (node)
{
	XMLProperty * prop;

	if (!parse_route_xml ()) {
		throw failed_constructor();
	}
	
	if (!parse_io ()) {
		throw failed_constructor();
	}
	
	XMLNodeList const & controllables = node.children ("controllable");
	for (XMLNodeList::const_iterator it = controllables.begin(); it != controllables.end(); ++it) {
		parse_controllable (**it);
	}
	
	XMLNode * remote_control = xml_track.child ("remote_control");
	if (remote_control && (prop = remote_control->property ("id"))) {
		uint32_t control_id = session.ntracks() + session.nbusses() + 1;
		prop->set_value (to_string (control_id, std::dec));
	}
	
	xml_track.remove_nodes_and_delete ("extra");
}

bool
AudioTrackImporter::parse_route_xml ()
{
	XMLPropertyList const & props = xml_track.properties();

	for (XMLPropertyList::const_iterator it = props.begin(); it != props.end(); ++it) {
		string prop = (*it)->name();
		if (!prop.compare ("default-type") || !prop.compare ("flags") ||
		  !prop.compare ("active") || !prop.compare ("muted") ||
		  !prop.compare ("soloed") || !prop.compare ("phase-invert") ||
		  !prop.compare ("denormal-protection") || !prop.compare("mute-affects-pre-fader") ||
		  !prop.compare ("mute-affects-post-fader") || !prop.compare("mute-affects-control-outs") ||
		  !prop.compare ("mute-affects-main-outs") || !prop.compare("mode")) {
			// All ok
		} else if (!prop.compare("order-keys")) {
			// TODO
		} else if (!prop.compare("diskstream-id")) {
			// TODO
		} else {
			std::cerr << string_compose (X_("AudioTrackImporter: did not recognise XML-property \"%1\""), prop) << endmsg;
		}
	}
	
	return true;
}

bool
AudioTrackImporter::parse_io ()
{
	XMLNode * io;
	bool name_ok = false;
	bool id_ok = false;

	if (!(io = xml_track.child ("IO"))) {
		return false;
	}
	
	XMLPropertyList const & props = io->properties();

	for (XMLPropertyList::const_iterator it = props.begin(); it != props.end(); ++it) {
		string prop = (*it)->name();
		if (!prop.compare ("gain") || !prop.compare ("iolimits")) {
			// All ok
		} else if (!prop.compare("name")) {
			name = prop;
			name_ok = true;
		} else if (!prop.compare("id")) {
			PBD::ID id;
			(*it)->set_value (id.to_s());
			id_ok = true;
			// TODO
		} else if (!prop.compare("inputs")) {
			// TODO
		} else if (!prop.compare("outputs")) {
			// TODO
		} else {
			std::cerr << string_compose (X_("AudioTrackImporter: did not recognise XML-property \"%1\""), prop) << endmsg;
		}
	}
	
	if (!name_ok) {
		error << X_("AudioTrackImporter: did not find necessary XML-property \"name\"") << endmsg;
		return false;
	}
	
	if (!id_ok) {
		error << X_("AudioTrackImporter: did not find necessary XML-property \"id\"") << endmsg;
		return false;
	}
	
	XMLNodeList const & controllables = io->children ("controllable");
	for (XMLNodeList::const_iterator it = controllables.begin(); it != controllables.end(); ++it) {
		parse_controllable (**it);
	}
	
	XMLNodeList const & processors = io->children ("Processor");
	for (XMLNodeList::const_iterator it = processors.begin(); it != processors.end(); ++it) {
		parse_processor (**it);
	}
	
	XMLNodeList const & automations = io->children ("Automation");
	for (XMLNodeList::const_iterator it = automations.begin(); it != automations.end(); ++it) {
		parse_automation (**it);
	}
	
	return true;
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
AudioTrackImporter::parse_processor (XMLNode & node)
{
	XMLNode * automation = node.child ("Automation");
	if (automation) {
		parse_automation (*automation);
	}
	
	return true;
}

bool
AudioTrackImporter::parse_controllable (XMLNode & node)
{
	XMLProperty * prop;
	
	if ((prop = node.property ("id"))) {
		PBD::ID new_id;
		prop->set_value (new_id.to_s());
	} else {
		return false;
	}

	return true;
}

bool
AudioTrackImporter::parse_automation (XMLNode & node)
{

	XMLNodeList const & lists = node.children ("AutomationList");
	for (XMLNodeList::const_iterator it = lists.begin(); it != lists.end(); ++it) {
		XMLProperty * prop;
		
		if ((prop = (*it)->property ("id"))) {
			PBD::ID id;
			prop->set_value (id.to_s());
		}
		
		// TODO rate convert events
	}

	return true;
}
