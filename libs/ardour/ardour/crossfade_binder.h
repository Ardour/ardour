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

#include "pbd/id.h"
#include "pbd/memento_command.h"

class XMLNode;

namespace ARDOUR {

class Crossfade;
class Playlist;
class SessionPlaylists;	

/** A MementoCommandBinder for Crossfades; required because the undo record
 *  may contain details of crossfades that have subsequently been deleted.
 *  This class allows recovery of a crossfade from an ID once it has been
 *  recreated by a previous undo step.
 */
class CrossfadeBinder : public MementoCommandBinder<ARDOUR::Crossfade>
{
public:
	CrossfadeBinder (boost::shared_ptr<ARDOUR::SessionPlaylists>, PBD::ID);
	CrossfadeBinder (XMLNode *, boost::shared_ptr<SessionPlaylists>);

	ARDOUR::Crossfade* get () const;
	std::string type_name () const;
	void add_state (XMLNode *);

private:
	boost::shared_ptr<ARDOUR::SessionPlaylists> _playlists;
	PBD::ID _id;
};

}
