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

#ifndef __ardour_named_selection_h__
#define __ardour_named_selection_h__

#include <string>
#include <list>
#include <boost/shared_ptr.hpp>

#include "pbd/stateful.h"

class XMLNode;

namespace ARDOUR
{

class Session;
class Playlist;

class NamedSelection : public PBD::Stateful
{
public:
	NamedSelection (std::string, std::list<boost::shared_ptr<Playlist> >&);
	NamedSelection (Session&, const XMLNode&);
	virtual ~NamedSelection ();

	std::string name;
	std::list<boost::shared_ptr<Playlist> > playlists;

	XMLNode& get_state (void);

	int set_state (const XMLNode&, int version);

	static PBD::Signal1<void,NamedSelection*> NamedSelectionCreated;
};

}/* namespace ARDOUR */

#endif /* __ardour_named_selection_h__ */

