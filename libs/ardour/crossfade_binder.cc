/* 
    Copyright (C) 2011 Paul Davis
    Author: Carl Hetherington <cth@carlh.net>

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

#include "ardour/crossfade_binder.h"
#include "ardour/session_playlists.h"
#include "ardour/crossfade.h"

using namespace ARDOUR;

CrossfadeBinder::CrossfadeBinder (boost::shared_ptr<SessionPlaylists> playlists, PBD::ID id)
	: _playlists (playlists)
	, _id (id)
{
	
}


CrossfadeBinder::CrossfadeBinder (XMLNode* node, boost::shared_ptr<SessionPlaylists> playlists)
	: _playlists (playlists)
{
	XMLProperty* id = node->property ("crossfade-id");
	assert (id);

	_id = PBD::ID (id->value ());
}

ARDOUR::Crossfade *
CrossfadeBinder::get () const
{
	ARDOUR::Crossfade* c = _playlists->find_crossfade (_id).get ();
	assert (c);
	return c;
}

std::string
CrossfadeBinder::type_name () const
{
	return "ARDOUR::Crossfade";
}

void
CrossfadeBinder::add_state (XMLNode* node)
{
	node->add_property ("crossfade-id", _id.to_s ());
}

