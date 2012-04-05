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

#include "ardour/audio_track_importer.h"

#include "ardour/audio_playlist_importer.h"
#include "ardour/audio_diskstream.h"
#include "ardour/session.h"

#include "pbd/controllable.h"
#include "pbd/convert.h"
#include "pbd/failed_constructor.h"

#include <sstream>
#include <algorithm>

#include "i18n.h"

using namespace std;
using namespace PBD;
using namespace ARDOUR;

/*** AudioTrackImportHandler ***/

AudioTrackImportHandler::AudioTrackImportHandler (XMLTree const & source, Session & session, AudioPlaylistImportHandler & pl_handler) :
  ElementImportHandler (source, session),
  pl_handler (pl_handler)
{
	XMLNode const * root = source.root();
	XMLNode const * routes;

	if (!(routes = root->child ("Routes"))) {
		throw failed_constructor();
	}

	XMLNodeList const & route_list = routes->children();
	for (XMLNodeList::const_iterator it = route_list.begin(); it != route_list.end(); ++it) {
		const XMLProperty* type = (*it)->property("default-type");
		if ( (!type || type->value() == "audio") &&  ((*it)->property ("diskstream") != 0 || (*it)->property ("diskstream-id") != 0)) {
			try {
				elements.push_back (ElementPtr ( new AudioTrackImporter (source, session, *this, **it, pl_handler)));
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

AudioTrackImporter::AudioTrackImporter (XMLTree const & source,
                                        Session & session,
                                        AudioTrackImportHandler & track_handler,
                                        XMLNode const & node,
                                        AudioPlaylistImportHandler & pl_handler) :
  ElementImporter (source, session),
  track_handler (track_handler),
  xml_track (node),
  pl_handler (pl_handler)
{
	XMLProperty * prop;

	if (!parse_route_xml ()) {
		throw failed_constructor();
	}

	if (!parse_io ()) {
		throw failed_constructor();
	}

	XMLNodeList const & controllables = node.children (Controllable::xml_node_name);
	for (XMLNodeList::const_iterator it = controllables.begin(); it != controllables.end(); ++it) {
		parse_controllable (**it);
	}

	XMLNode * remote_control = xml_track.child ("RemoteControl");
	if (remote_control && (prop = remote_control->property ("id"))) {
		uint32_t control_id = session.ntracks() + session.nbusses() + 1;
		prop->set_value (to_string (control_id, std::dec));
	}

	xml_track.remove_nodes_and_delete ("Extra");
}

AudioTrackImporter::~AudioTrackImporter ()
{
	playlists.clear ();
}

bool
AudioTrackImporter::parse_route_xml ()
{
	bool ds_ok = false;

	// Remove order keys, new ones will be generated
	xml_track.remove_property ("order-keys");

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
		} else if (!prop.compare("diskstream-id")) {
			old_ds_id = (*it)->value();
			(*it)->set_value (new_ds_id.to_s());
			ds_ok = true;
		} else {
			std::cerr << string_compose (X_("AudioTrackImporter: did not recognise XML-property \"%1\""), prop) << endmsg;
		}
	}

	if (!ds_ok) {
		error << X_("AudioTrackImporter: did not find necessary XML-property \"diskstream-id\"") << endmsg;
		return false;
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
			name = (*it)->value();
			name_ok = true;
		} else if (!prop.compare("id")) {
			PBD::ID id;
			(*it)->set_value (id.to_s());
			id_ok = true;
		} else if (!prop.compare("inputs")) {
			// TODO Handle this properly!
			/* Input and output ports are counted and added empty, so that no in/output connecting function fails. */
			uint32_t num_inputs = std::count ((*it)->value().begin(), (*it)->value().end(), '{');
			std::string value;
			for (uint32_t i = 0; i < num_inputs; i++) { value += "{}"; }
			(*it)->set_value (value);
		} else if (!prop.compare("outputs")) {
			// TODO See comments above
			uint32_t num_outputs = std::count ((*it)->value().begin(), (*it)->value().end(), '{');
			std::string value;
			for (uint32_t i = 0; i < num_outputs; i++) { value += "{}"; }
			(*it)->set_value (value);
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

	XMLNodeList const & controllables = io->children (Controllable::xml_node_name);
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
AudioTrackImporter::_prepare_move ()
{
	/* Copy dependent playlists */

	pl_handler.playlists_by_diskstream (old_ds_id, playlists);

	for (PlaylistList::iterator it = playlists.begin(); it != playlists.end(); ++it) {
		if (!(*it)->prepare_move ()) {
			playlists.clear ();
			return false;
		}
		(*it)->set_diskstream (new_ds_id);
	}

	/* Rename */

	while (session.route_by_name (name) || !track_handler.check_name (name)) {
		std::pair<bool, string> rename_pair = *Rename (_("A playlist with this name already exists, please rename it."), name);
		if (!rename_pair.first) {
			return false;
		}
		name = rename_pair.second;
	}
	xml_track.child ("IO")->property ("name")->set_value (name);
	track_handler.add_name (name);

	return true;
}

void
AudioTrackImporter::_cancel_move ()
{
	track_handler.remove_name (name);
	playlists.clear ();
}

void
AudioTrackImporter::_move ()
{
	/* Add diskstream */

	boost::shared_ptr<XMLSharedNodeList> ds_node_list;
	string xpath = "/Session/DiskStreams/AudioDiskstream[@id='" + old_ds_id.to_s() + "']";
	ds_node_list = source.find (xpath);

	if (ds_node_list->size() != 1) {
		error << string_compose (_("Error Importing Audio track %1"), name) << endmsg;
		return;
	}

	boost::shared_ptr<XMLNode> ds_node = ds_node_list->front();
	XMLProperty* p = ds_node->property (X_("id"));
	assert (p);
	p->set_value (new_ds_id.to_s());

	boost::shared_ptr<Diskstream> new_ds (new AudioDiskstream (session, *ds_node));
	new_ds->set_name (name);
	new_ds->do_refill_with_alloc ();
	new_ds->set_block_size (session.get_block_size ());

	/* Import playlists */

	for (PlaylistList::const_iterator it = playlists.begin(); it != playlists.end(); ++it) {
		(*it)->move ();
	}

	/* Import track */

	XMLNode routes ("Routes");
	routes.add_child_copy (xml_track);
	session.load_routes (routes, 3000);
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

		if (!(*it)->name().compare ("events")) {
			rate_convert_events (**it);
		}
	}

	return true;
}

bool
AudioTrackImporter::rate_convert_events (XMLNode & node)
{
	if (node.children().empty()) {
		return false;
	}

	XMLNode* content_node = node.children().front();

	if (content_node->content().empty()) {
		return false;
	}

	std::stringstream str (content_node->content());
	std::ostringstream new_content;

	framecnt_t x;
	double y;
	bool ok = true;

	while (str) {
		str >> x;
		if (!str) {
			break;
		}
		str >> y;
		if (!str) {
			ok = false;
			break;
		}

		new_content << rate_convert_samples (x) << ' ' << y;
	}

	if (!ok) {
		error << X_("AudioTrackImporter: error in rate converting automation events") << endmsg;
		return false;
	}

	content_node->set_content (new_content.str());

	return true;
}
