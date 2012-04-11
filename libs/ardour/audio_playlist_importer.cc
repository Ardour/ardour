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

#include "ardour/audio_playlist_importer.h"

#include <sstream>

#include "pbd/failed_constructor.h"
#include "pbd/compose.h"
#include "pbd/error.h"

#include "ardour/audio_region_importer.h"
#include "ardour/session.h"
#include "ardour/playlist.h"
#include "ardour/playlist_factory.h"
#include "ardour/session_playlists.h"

#include "i18n.h"

using namespace std;
using namespace PBD;
using namespace ARDOUR;

/**** Handler ***/
AudioPlaylistImportHandler::AudioPlaylistImportHandler (XMLTree const & source, Session & session, AudioRegionImportHandler & region_handler, const char * nodename) :
  ElementImportHandler (source, session),
  region_handler (region_handler)
{
	XMLNode const * root = source.root();
	XMLNode const * playlists;

	if (!(playlists = root->child (nodename))) {
		throw failed_constructor();
	}

	XMLNodeList const & pl_children = playlists->children();
	for (XMLNodeList::const_iterator it = pl_children.begin(); it != pl_children.end(); ++it) {
		const XMLProperty* type = (*it)->property("type");
		if ( !type || type->value() == "audio" ) {
			try {
				elements.push_back (ElementPtr ( new AudioPlaylistImporter (source, session, *this, **it)));
			} catch (failed_constructor err) {
				set_dirty();
			}
		}
	}
}

string
AudioPlaylistImportHandler::get_info () const
{
	return _("Audio Playlists");
}

void
AudioPlaylistImportHandler::get_regions (XMLNode const & node, ElementList & list) const
{
	region_handler.create_regions_from_children (node, list);
}

void
AudioPlaylistImportHandler::update_region_id (XMLProperty* id_prop)
{
	PBD::ID old_id (id_prop->value());
	PBD::ID new_id (region_handler.get_new_id (old_id));
	id_prop->set_value (new_id.to_s());
}

void
AudioPlaylistImportHandler::playlists_by_diskstream (PBD::ID const & id, PlaylistList & list) const
{
	for (ElementList::const_iterator it = elements.begin(); it != elements.end(); ++it) {
		boost::shared_ptr<AudioPlaylistImporter> pl = boost::dynamic_pointer_cast<AudioPlaylistImporter> (*it);
		if (pl && pl->orig_diskstream() == id) {
			list.push_back (PlaylistPtr (new AudioPlaylistImporter (*pl)));
		}
	}
}

/*** AudioPlaylistImporter ***/
AudioPlaylistImporter::AudioPlaylistImporter (XMLTree const & source, Session & session, AudioPlaylistImportHandler & handler, XMLNode const & node) :
  ElementImporter (source, session),
  handler (handler),
  orig_node (node),
  xml_playlist (node),
  diskstream_id ("0")
{
	bool ds_ok = false;

	populate_region_list ();

	// Parse XML
	XMLPropertyList const & props = xml_playlist.properties();
	for (XMLPropertyList::const_iterator it = props.begin(); it != props.end(); ++it) {
		string prop = (*it)->name();
		if (!prop.compare("type") || !prop.compare("frozen")) {
			// All ok
		} else if (!prop.compare("name")) {
			name = (*it)->value();
		} else if (!prop.compare("orig-diskstream-id")) {
			orig_diskstream_id = (*it)->value();
			ds_ok = true;
		} else {
			std::cerr << string_compose (X_("AudioPlaylistImporter did not recognise XML-property \"%1\""), prop) << endmsg;
		}
	}

	if (!ds_ok) {
		error << string_compose (X_("AudioPlaylistImporter (%1): did not find XML-property \"orig_diskstream_id\" which is mandatory"), name) << endmsg;
		throw failed_constructor();
	}
}

AudioPlaylistImporter::AudioPlaylistImporter (AudioPlaylistImporter const & other) :
  ElementImporter (other.source, other.session),
  handler (other.handler),
  orig_node (other.orig_node),
  xml_playlist (other.xml_playlist),
  orig_diskstream_id (other.orig_diskstream_id)
{
	populate_region_list ();
}

AudioPlaylistImporter::~AudioPlaylistImporter ()
{

}

string
AudioPlaylistImporter::get_info () const
{
	XMLNodeList children = xml_playlist.children();
	unsigned int regions = 0;
	std::ostringstream oss;

	for (XMLNodeIterator it = children.begin(); it != children.end(); it++) {
		if ((*it)->name() == "Region") {
			++regions;
		}
	}

	oss << regions << " ";

	if (regions == 1) {
		oss << _("region");
	} else {
		oss << _("regions");
	}

	return oss.str();
}

bool
AudioPlaylistImporter::_prepare_move ()
{
	// Rename
	while (session.playlists->by_name (name) || !handler.check_name (name)) {
		std::pair<bool, string> rename_pair = *Rename (_("A playlist with this name already exists, please rename it."), name);
		if (!rename_pair.first) {
			return false;
		}
		name = rename_pair.second;
	}
	
	XMLProperty* p = xml_playlist.property ("name");
	if (!p) {
		error << _("badly-formed XML in imported playlist") << endmsg;
	}

	p->set_value (name);
	handler.add_name (name);

	return true;
}

void
AudioPlaylistImporter::_cancel_move ()
{
	handler.remove_name (name);
}

void
AudioPlaylistImporter::_move ()
{
	boost::shared_ptr<Playlist> playlist;

	// Update diskstream id
	xml_playlist.property ("orig-diskstream-id")->set_value (diskstream_id.to_s());

	// Update region XML in playlist and prepare sources
	xml_playlist.remove_nodes("Region");
	for (RegionList::iterator it = regions.begin(); it != regions.end(); ++it) {
		xml_playlist.add_child_copy ((*it)->get_xml());
		(*it)->add_sources_to_session();
		if ((*it)->broken()) {
			handler.set_dirty();
			set_broken();
			return; // TODO clean up?
		}
	}

	// Update region ids in crossfades
	XMLNodeList crossfades = xml_playlist.children("Crossfade");
	for (XMLNodeIterator it = crossfades.begin(); it != crossfades.end(); ++it) {
		XMLProperty* in = (*it)->property("in");
		XMLProperty* out = (*it)->property("out");
		if (!in || !out) {
			error << string_compose (X_("AudioPlaylistImporter (%1): did not find the \"in\" or \"out\" property from a crossfade"), name) << endmsg;
		}

		handler.update_region_id (in);
		handler.update_region_id (out);

		// rate convert length and position
		XMLProperty* length = (*it)->property("length");
		if (length) {
			length->set_value (rate_convert_samples (length->value()));
		}

		XMLProperty* position = (*it)->property("position");
		if (position) {
			position->set_value (rate_convert_samples (position->value()));
		}
	}

	// Create playlist
	playlist = PlaylistFactory::create (session, xml_playlist, false, true);
}

void
AudioPlaylistImporter::set_diskstream (PBD::ID const & id)
{
	diskstream_id = id;
}

void
AudioPlaylistImporter::populate_region_list ()
{
	ElementImportHandler::ElementList elements;
	handler.get_regions (orig_node, elements);
	for (ElementImportHandler::ElementList::iterator it = elements.begin(); it != elements.end(); ++it) {
		regions.push_back (boost::dynamic_pointer_cast<AudioRegionImporter> (*it));
	}
}

string
UnusedAudioPlaylistImportHandler::get_info () const
{
	return _("Audio Playlists (unused)");
}
