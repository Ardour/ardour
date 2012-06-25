/*
    Copyright (C) 2003 Paul Davis 

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

#include <pbd/failed_constructor.h>
#include <pbd/error.h>

#include <ardour/session.h>
#include <ardour/utils.h>
#include <ardour/playlist.h>
#include <ardour/named_selection.h>

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;

sigc::signal<void,NamedSelection*> NamedSelection::NamedSelectionCreated;

typedef std::list<boost::shared_ptr<Playlist> > PlaylistList;

NamedSelection::NamedSelection (string n, PlaylistList& l) 
	: name (n)
{
	playlists = l;
	for (PlaylistList::iterator i = playlists.begin(); i != playlists.end(); ++i) {
		string new_name;

		/* rename playlists to reflect our ownership */
                
		new_name = name;
		new_name += '/';
		new_name += (*i)->name();

		(*i)->set_name (new_name);
		(*i)->use();
	}

	NamedSelectionCreated (this);
}

NamedSelection::NamedSelection (Session& session, const XMLNode& node)
{
	XMLNode* lists_node;
	const XMLProperty* property;

	if ((property = node.property ("name")) == 0) {
		throw failed_constructor();
	}

	name = property->value();
	
	if ((lists_node = find_named_node (node, "Playlists")) == 0) {
		return;
	}

	XMLNodeList nlist = lists_node->children();
	XMLNodeConstIterator niter;

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {

		const XMLNode* plnode;
		string playlist_name;
		boost::shared_ptr<Playlist> playlist;

		plnode = *niter;

		if ((property = plnode->property ("name")) != 0) {
			if ((playlist = session.playlist_by_name (property->value())) != 0) {
				playlist->use();
				playlists.push_back (playlist);
			} else {
				warning << string_compose (_("Chunk %1 uses an unknown playlist \"%2\""), name, property->value()) << endmsg;
			}
		} else {
			error << string_compose (_("Chunk %1 contains malformed playlist information"), name) << endmsg;
			throw failed_constructor();
		}
	}

	NamedSelectionCreated (this);
}

NamedSelection::~NamedSelection ()
{
	for (PlaylistList::iterator i = playlists.begin(); i != playlists.end(); ++i) {
		(*i)->release ();
		(*i)->GoingAway ();
	}
}

int
NamedSelection::set_state (const XMLNode& node)
{
	return 0;
}

XMLNode&
NamedSelection::get_state ()
{
	XMLNode* root = new XMLNode ("NamedSelection");
	XMLNode* child;

	root->add_property ("name", name);
	child = root->add_child ("Playlists");

	for (PlaylistList::iterator i = playlists.begin(); i != playlists.end(); ++i) {
		XMLNode* plnode = new XMLNode ("Playlist");

		plnode->add_property ("name", (*i)->name());
		child->add_child_nocopy (*plnode);
	}
	
	return *root;
}
